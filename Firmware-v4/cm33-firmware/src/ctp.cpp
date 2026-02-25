#include <stm32mp2xx.h>
#include "pins.h"
#include "i2c.h"
#include "clocks.h"
#include "gslX680_311_5_F.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "gk_conf.h"
#include "guard.h"
#include "logger.h"
#include "interface/cm33_data.h"
#include "message.h"
#include <atomic>

extern cm33_data_kernel dk;

/* Command queue */
enum class ctp_command
{
	CTP_INT, CTP_SLEEP, CTP_RESUME
};
static QueueHandle_t ctp_queue;

/* Adapted from ER-provided driver */
#define MAX_CONTACTS 10
#define MULTI_TP_POINTS 10

static const constexpr pin CTP_WAKE { GPIOA, 2 };
static const constexpr pin CTP_INT { GPIOA, 5 };
static const constexpr uint8_t ctp_addr = 0x40;
#define printk klog
#define print_info(...) 
static bool ctp_is_init = false;

static std::atomic<bool> ctp_enabled = false;

static void msleep(unsigned int ms)
{
	vTaskDelay(pdMS_TO_TICKS(ms));
}

static int gsl_ts_write(int addr, char *pdata, unsigned int datalen)
{
    auto &i2c1 = i2c(4);
    return i2c1.RegisterWrite(ctp_addr, (uint8_t)addr, pdata, datalen);
}

static int gsl_ts_read(int addr, char *pdata, unsigned int datalen)
{
    auto &i2c1 = i2c(4);
    return i2c1.RegisterRead(ctp_addr, (uint8_t)addr, pdata, datalen);
}

static void clr_reg()
{
	char write_buf[4]	= {0};

	write_buf[0] = 0x88;
	gsl_ts_write(0xe0, &write_buf[0], 1); 	
	msleep(20);
	write_buf[0] = 0x01;
	gsl_ts_write(0x80, &write_buf[0], 1); 	
	msleep(5);
	write_buf[0] = 0x04;
	gsl_ts_write(0xe4, &write_buf[0], 1); 	
	msleep(5);
	write_buf[0] = 0x00;
	gsl_ts_write(0xe0, &write_buf[0], 1); 	
	msleep(20);
}

static void reset_chip(void)
{
	char buf[4] = {0x00};
	
	buf[0] = 0x88;	
	gsl_ts_write(0xe0, buf, 1);
	msleep(10);
	
	buf[0] = 0x04;
	gsl_ts_write(0xe4, buf, 1);
	
	msleep(10);
	buf[0] = 0x00;
	buf[1] = 0x00;	
	buf[2] = 0x00;	
	buf[3] = 0x00;		
	gsl_ts_write(0xbc, buf, 4);
	msleep(10);
}

static void gsl_load_fw(void)
{
	char buf[4] = {0};
	char reg = 0;
	unsigned int source_line = 0;
	unsigned int source_len;
	const struct fw_data *ptr_fw;

	printk("=============gsl_load_fw start==============\n");


	ptr_fw = GSLX680_FW;
	source_len = sizeof(GSLX680_FW) / sizeof(GSLX680_FW[0]);

	for (source_line = 0; source_line < source_len; source_line++) 
	{
		/* init page trans, set the page val */
		if (0xf0 == ptr_fw[source_line].offset)
		{
			buf[0] = (char)(ptr_fw[source_line].val & 0x000000ff);
			gsl_ts_write(0xf0, buf, 1);
		}
		else 
		{
			reg = ptr_fw[source_line].offset;
			buf[0] = (char)(ptr_fw[source_line].val & 0x000000ff);
			buf[1] = (char)((ptr_fw[source_line].val & 0x0000ff00) >> 8);
			buf[2] = (char)((ptr_fw[source_line].val & 0x00ff0000) >> 16);
			buf[3] = (char)((ptr_fw[source_line].val & 0xff000000) >> 24);

			gsl_ts_write(reg, buf, 4);
		}
	}

	printk("=============gsl_load_fw end==============\n");

}

static void startup_chip(void)
{
	char buf[4] = {0x00};

#if 0
    buf[3] = 0x01;
	buf[2] = 0xfe;
	buf[1] = 0x10;
	buf[0] = 0x00;	
	gsl_ts_write(0xf0, buf, sizeof(buf));
	buf[3] = 0x00;
	buf[2] = 0x00;
	buf[1] = 0x00;
	buf[0] = 0x0f;	
	gsl_ts_write(0x04, buf, sizeof(buf));
	msleep(20);	
#endif

    gsl_ts_write(0xe0, buf, 4);
	msleep(10);
}

static void gsl_reset()
{
    CTP_WAKE.set_as_output();
    CTP_WAKE.clear();
	msleep(20);
    CTP_WAKE.set();
	msleep(20);

	clr_reg();
	reset_chip();
	gsl_load_fw();
	startup_chip();
	reset_chip();			
	startup_chip();
}

struct ctp_point
{
	int x, y;
	bool pressed = false;
};

static ctp_point curpts[MAX_CONTACTS] = { 0 };

static uint32_t touch_encode(unsigned int id, unsigned int x, unsigned int y)
{
	return (id << 20) | (y << 10) | (x);
}

static void handle_press(unsigned int id, unsigned int x, unsigned int y)
{
	return send_message(CM33_DK_MSG_TOUCHPRESS | touch_encode(id, x, y));
}

static void handle_move(unsigned int id, unsigned int x, unsigned int y)
{
	return send_message(CM33_DK_MSG_TOUCHMOVE | touch_encode(id, x, y));
}

static void handle_release(unsigned int id, unsigned int x, unsigned int y)
{
	return send_message(CM33_DK_MSG_TOUCHRELEASE | touch_encode(id, x, y));
}

static void read_ctp()
{
	uint32_t tp_data[MULTI_TP_POINTS + 1];

	auto ret = gsl_ts_read(0x80, (char *)tp_data, sizeof(tp_data));
	if( ret < 0) {
		print_info("gp_tp_get_data fail,return %d\n",ret);
		//gp_gpio_enable_irq(ts.client, 1);
		return;
	}

	auto ntouched = (size_t)tp_data[0];
	if(ntouched > MULTI_TP_POINTS)
		ntouched = MULTI_TP_POINTS;

	ctp_point newpts[MAX_CONTACTS] = { 0 };

	for(auto i = 0U; i < ntouched; i++)
	{
		auto d = tp_data[i + 1];
		auto id = d >> 28;
		auto x = (d & 0xffffU);
		auto y = (d >> 16) & 0xfffU;

		newpts[id - 1].pressed = true;
		newpts[id - 1].x = x;
		newpts[id - 1].y = y;
	}

	// now look for any changed on pressed or x,y >= 4 pixel movements
	for(auto i = 0U; i < MAX_CONTACTS; i++)
	{
		if(newpts[i].pressed && !curpts[i].pressed)
		{
			handle_press(i, newpts[i].x, newpts[i].y);
			//klog("press\n");
			curpts[i] = newpts[i];
		}
		else if(!newpts[i].pressed && curpts[i].pressed)
		{
			handle_release(i, curpts[i].x, curpts[i].y);
			//klog("release\n");
			curpts[i] = newpts[i];
		}
		else if(newpts[i].pressed && curpts[i].pressed)
		{
			// check for movement
			if(std::abs(newpts[i].x - curpts[i].x) >= 4 ||
				std::abs(newpts[i].y - curpts[i].y) >= 4)
			{
				handle_move(i, newpts[i].x, newpts[i].y);
				//klog("move\n");
			}
			curpts[i] = newpts[i];
		}
	}

	//klog("ctp: ntouched: %u\n", ntouched);

}

static void check_mem_data(void)
{
	char read_buf[4]  = {0};
	
	msleep(30);

	gsl_ts_read(0xb0, read_buf, sizeof(read_buf));
	if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
	{
		//klog("ctp: mem data incorrect - resetting\n");
		ctp_is_init = false;
	}
}

[[maybe_unused]] static void ctp_thread(void *)
{
	while(true)
	{
		if(!ctp_is_init)
		{
			gsl_reset();
			ctp_is_init = true;
		}

		ctp_command cmd;
		if(xQueueReceive(ctp_queue, &cmd, portMAX_DELAY) == pdTRUE)
		{
			switch(cmd)
			{
				case ctp_command::CTP_INT:
					read_ctp();
					NVIC_EnableIRQ(EXTI1_5_IRQn);
					//gic_set_enable(305);
					break;

				case ctp_command::CTP_RESUME:
					// clear current state
					for(auto i = 0U; i < MAX_CONTACTS; i++)
					{
						curpts[i].pressed = false;
						curpts[i].x = 0;
						curpts[i].y = 0;
					}

					CTP_WAKE.set();
					msleep(20);
					reset_chip();			
					startup_chip();
					check_mem_data();

					if(ctp_is_init)
					{
						UninterruptibleGuard ug;
						dk.sr |= CM33_DK_SR_TOUCH_ENABLE;
						ctp_enabled = true;
						NVIC_EnableIRQ(EXTI1_5_IRQn);
					}
					else
					{
						// try again
						auto resume = ctp_command::CTP_RESUME;
						xQueueSend(ctp_queue, &resume, 0);
					}
					break;

				case ctp_command::CTP_SLEEP:
					{
						UninterruptibleGuard ug;
						dk.sr &= ~CM33_DK_SR_TOUCH_ENABLE;
						ctp_enabled = false;
						CTP_WAKE.clear();
						NVIC_DisableIRQ(EXTI1_5_IRQn);
					}
					msleep(10);
					break;
			}
		}
	}
}

extern "C" void EXTI1_5_IRQHandler()
{
	//klog("ctp: irq\n");
	//gic_clear_enable(305);
	NVIC_DisableIRQ(EXTI1_5_IRQn);
	EXTI1->RPR1 = (1U << 5);
	auto ctpint = ctp_command::CTP_INT;
	BaseType_t hpt;
	xQueueSendFromISR(ctp_queue, &ctpint, &hpt);
	portYIELD_FROM_ISR(hpt);
	__DMB();
}

void init_ctp()
{
	ctp_enabled = false;
	CTP_INT.set_as_input();
    NVIC_SetPriority(EXTI1_5_IRQn, 8);     // check this - we have 4 priority bits

	ctp_queue = xQueueCreate(8, sizeof(ctp_command));

	EXTI1->EXTICR[1] = (EXTI1->EXTICR[1] & ~EXTI_EXTICR2_EXTI5_Msk) |
		(0U << EXTI_EXTICR2_EXTI5_Pos);
	EXTI1->RTSR1 |= (1U << 5);
	EXTI1->FTSR1 &= ~(1U << 5);
	EXTI1->C2IMR1 |= (1U << 5);

#if GK_ENABLE_TOUCH
	xTaskCreate(ctp_thread, "ctp", 2048, nullptr, configMAX_PRIORITIES - 1, nullptr);
#endif
}

bool ctp_get_status()
{
	return ctp_enabled;
}

void ctp_enable()
{
	ctp_command ctpen = ctp_command::CTP_RESUME;
	xQueueSend(ctp_queue, &ctpen, 0);
}

void ctp_disable()
{
	ctp_command ctpdis = ctp_command::CTP_SLEEP;
	xQueueSend(ctp_queue, &ctpdis, 0);
}
