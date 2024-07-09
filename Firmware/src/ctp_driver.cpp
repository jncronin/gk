#include "i2c.h"
#include "osmutex.h"
#include "thread.h"
#include "pins.h"
#include "scheduler.h"
#include "SEGGER_RTT.h"

// The CST340 chip is complex - see https://git.zx2c4.com/linux/plain/drivers/input/touchscreen/hynitron_cstxxx.c
constexpr const unsigned int addr = 0x1a;

constexpr const pin CTP_NRESET { GPIOC, 13 };
constexpr const pin CTP_INT { GPIOC, 14 };

SRAM4_DATA static bool ctp_init = false;
SRAM4_DATA static BinarySemaphore sem_ctp;

extern Spinlock s_rtt;

#define CST3XX_FIRMWARE_INFO_START_CMD		0x01d1
#define CST3XX_FIRMWARE_INFO_END_CMD		0x09d1
#define CST3XX_FIRMWARE_CHK_CODE_REG		0xfcd1
#define CST3XX_FIRMWARE_VERSION_REG		0x08d2
#define CST3XX_FIRMWARE_VER_INVALID_VAL		0xa5a5a5a5

#define CST3XX_BOOTLDR_PROG_CMD			0xaa01a0
#define CST3XX_BOOTLDR_PROG_CHK_REG		0x02a0
#define CST3XX_BOOTLDR_CHK_VAL			0xac

#define CST3XX_TOUCH_DATA_PART_REG		0x00d0
#define CST3XX_TOUCH_DATA_FULL_REG		0x07d0
#define CST3XX_TOUCH_DATA_CHK_VAL		0xab
#define CST3XX_TOUCH_DATA_TOUCH_VAL		0x03
#define CST3XX_TOUCH_DATA_STOP_CMD		0xab00d0
#define CST3XX_TOUCH_COUNT_MASK			GENMASK(6, 0)

static bool init_ctp();
static void cst_read();
static void ctp_reset(kernel_time delay);

//char cst_regs[65536];

void *cst_thread(void *param)
{
    CTP_INT.set_as_input();
    CTP_NRESET.set_as_output();

    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;
    SYSCFG->EXTICR[3] &= SYSCFG_EXTICR4_EXTI14_Msk;
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI14_PC;

    //EXTI->RTSR1 |= EXTI_RTSR1_TR14;
    EXTI->FTSR1 |= EXTI_FTSR1_TR14;
    //EXTI->IMR1 |= EXTI_IMR1_IM14;

    NVIC_EnableIRQ(EXTI15_10_IRQn);

    while(true)
    {
        while(!ctp_init)
        {
            ctp_init = init_ctp();
        }

        //if(sem_ctp.Wait(clock_cur() + kernel_time::from_ms(2000)))
        Block(clock_cur() + kernel_time::from_ms(1000));
            cst_read();
    }
}

void ctp_reset(kernel_time delay)
{
    CTP_NRESET.clear();
    Block(clock_cur() + kernel_time::from_ms(20));
    CTP_NRESET.set();
    if(delay.is_valid())
        Block(clock_cur() + delay);
}

static int ctp_bootloader_enter()
{
    uint32_t buf;

	for (int retry = 0; retry < 5; retry++) {
		ctp_reset(kernel_time::from_ms(7 + retry));
		/* set cmd to enter program mode */
        buf = CST3XX_BOOTLDR_PROG_CMD;
        i2c_send(addr, &buf, 3);
        Block(clock_cur() + kernel_time::from_ms(2));

		/* check whether in program mode */
        buf = 0;
		i2c_register_read(addr,
					       (uint16_t)CST3XX_BOOTLDR_PROG_CHK_REG,
					       &buf, 1);

		if (buf == CST3XX_BOOTLDR_CHK_VAL)
			break;
	}

	if (buf != CST3XX_BOOTLDR_CHK_VAL) {
		CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ctp: unable to enter bootloader mode\n");
		return -1;
	}

    ctp_reset(kernel_time::from_ms(40));

    return 0;
}

static int ctp_firmware_info()
{
    int buf = CST3XX_FIRMWARE_INFO_START_CMD;
    i2c_send(addr, &buf, 2);

    Block(clock_cur() + kernel_time::from_us(10000));

    int chkcode;
    i2c_register_read(addr, (uint16_t)CST3XX_FIRMWARE_CHK_CODE_REG, &chkcode, 4);

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ctp: check %x\n", (uint32_t)chkcode);
    }

    Block(clock_cur() + kernel_time::from_ms(10));

    unsigned int fver;
    i2c_register_read(addr, (uint16_t)CST3XX_FIRMWARE_VERSION_REG, &fver, 4);
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ctp: firmware version %x\n", (uint32_t)fver);
    }

    Block(clock_cur() + kernel_time::from_ms(10));

    for(int i = 0; i < 3; i++)
    {
        buf = CST3XX_FIRMWARE_INFO_END_CMD;
        i2c_send(addr, &buf, 2);

        Block(clock_cur() + kernel_time::from_us(5000));
    }

    if((chkcode & 0xffff0000) == 0xcaca0000 && fver != CST3XX_FIRMWARE_VER_INVALID_VAL)
        return 0;
    else
        return -1;
}

bool init_ctp()
{
    ctp_reset(kernel_time::from_ms(60));
    if(ctp_bootloader_enter() < 0)
        return false;
    if(ctp_firmware_info() < 0)
        return false;
    ctp_reset(kernel_time::from_ms(40));

    EXTI->IMR1 |= EXTI_IMR1_IM14;
    return true;
}

void cst_read()
{
    uint8_t buf[28] = { 0 };

/*
    for(int i = 0x1000; i < 0x2000; i++)
    {
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "ctp %d\n", i);
        }
        i2c_register_read(addr, (uint16_t)i, buf, 1);
        if(buf[0] == CST3XX_TOUCH_DATA_CHK_VAL)
        {
            __asm__ volatile("bkpt \n" ::: "memory");
        } // 4360
    } */

/*  for(int taddr = 0; taddr < 65536; taddr += 32)
    {
        i2c_register_read(addr, (uint16_t)taddr, &cst_regs[taddr], 32);
    }
    for(int i = 0; i < 65536; i++)
    {
        if(cst_regs[i] == CST3XX_TOUCH_DATA_CHK_VAL)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "cst: touch check at %d\n", i);
        }
    } */

    // checked with scope - appropriate request however result still 00 a5 a5 a5 ...
    //  there is some suggestion this is due to buggy firmware:
    //  https://blog-csdn-net.translate.goog/professionalmcu/article/details/124559206?_x_tr_sl=zh-CN&_x_tr_tl=en&_x_tr_hl=en&_x_tr_pto=sc
    i2c_register_read(addr, (uint16_t)CST3XX_TOUCH_DATA_PART_REG, buf, 28);

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ctp@%u: %x, %x\n",
            (uint32_t)clock_cur_us(), buf[6], buf[0]);
    }

    int stop = CST3XX_TOUCH_DATA_STOP_CMD;
    i2c_send(addr, &stop, 3);
}

extern "C" void EXTI15_10_IRQHandler()
{
    sem_ctp.Signal();
    EXTI->PR1 = EXTI_PR1_PR14;
    __DMB();
}
