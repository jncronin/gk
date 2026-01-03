#if 0

#include <linux/earlysuspend.h>

#include <mach/kernel.h>
#include <mach/module.h>
#include <mach/cdev.h>
#include <mach/diag.h>
#include <mach/typedef.h>
#include <mach/gp_ti2c_bus.h>
#include <mach/gp_gpio.h>
#include <mach/hal/hal_gpio.h>
#include <linux/jiffies.h>
#include <mach/clock_mgr/gp_clock.h>
#include <mach/gp_board.h>

#include "sysconfig.h"
#include "gslX680_311_5_E.h"


#define TI2C_ADDR 0x80
#define TI2C_CLK 300
#define MAX_CONTACTS 10
#define MULTI_TP_POINTS 10

#define ADJUST_CPU_FREQ	0// 1
#define DMA_TRANS_LEN		0x20
//#define VIRTUAL_KEYS
//#define GSL_DEBUG

#ifdef GSL_DEBUG 
#define print_info(fmt, args...)   \
        do{                              \
                printk(fmt, ##args);     \
        }while(0)
#else
#define print_info(fmt, args...)   //
#endif

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/
typedef struct gp_tp_s {
	struct input_dev *dev;
	ti2c_set_value_t *i2c_handle;
	int client;
	int touch_reset;
	int prev_touched;
	struct work_struct mt_work;
	struct work_struct mt_set_nice_work;
	struct workqueue_struct *touch_wq;
	int intIoIndex;
}gp_tp_t;


/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static void gp_touch_early_suspend(struct early_suspend *h);
static void gp_touch_late_resume(struct early_suspend *h);

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static gp_tp_t ts;

static u32 id_sign[MAX_CONTACTS+1] = {0};
static u8 id_state_flag[MAX_CONTACTS+1] = {0};
static u8 id_state_old_flag[MAX_CONTACTS+1] = {0};
static u16 x_old[MAX_CONTACTS+1] = {0};
static u16 y_old[MAX_CONTACTS+1] = {0};
static u16 x_new = 0;
static u16 y_new = 0;


/**************************************************************************
 *             F U N C T I O N    I M P L E M E N T A T I O N S           *
 **************************************************************************/
#ifdef VIRTUAL_KEYS

static ssize_t
virtual_keys_show(
	struct kobject *kobj,
	struct kobj_attribute *attr, char *buf
)
{
/*	gp_board_touch_t *touch_config = NULL;
	
	touch_config = gp_board_get_config("touch", gp_board_touch_t);
	if ( touch_config != NULL && touch_config->get_touch_virtualkeyshow != NULL) {
		return touch_config->get_touch_virtualkeyshow((char*) buf);
	}
	else{
		printk("[%s][%d]ERROR!!!No Virtual Key is Defined in board_config.c\n", __FUNCTION__, __LINE__);
		return 0;
	}*/
	
	/*return sprintf(buf,
		__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":360:860:40:83"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":240:860:40:83"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":120:860:40:83"
		"\n");*/
		
			return sprintf(buf,
		__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":829:79:1:1"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":829:47:1:1"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":829:0:1:1"
		"\n");
		
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.gp_ts",
		.mode = S_IRUGO,
	},
	.show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};

static struct attribute_group properties_attr_group = {
	.attrs = properties_attrs,
};

static void
virtual_keys_init(
	void
)
{
	int ret = 0;
	struct kobject *properties_kobj;
	
	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj, &properties_attr_group);
	if (!properties_kobj || ret)
		printk("failed to create board_properties\n");    
}

#endif

/**
 * @brief touch panel request fucntion
 * @return : handle of the requested touch panel(<0 invalid handle)
 */
int gp_tp_request(void)
{
	return (int)(&ts);
}

/**
 * @brief touch panel free fucntion
 * @param handle[in] : handle of touch panel to release
 * @return : SP_OK(0)/ERROR_ID
 */
int gp_tp_release(int handle)
{
	return 0;
}


static int gsl_ts_write(int addr, char *pdata, unsigned int datalen)
{
	int ret = 0, j;

	ts.i2c_handle->transmitMode = TI2C_NORMAL_WRITE_MODE;
	ts.i2c_handle->subAddrMode = TI2C_NORMAL_SUBADDR_8BITS;  
	ts.i2c_handle->pSubAddr = &addr;
	ts.i2c_handle->pBuf = pdata;
	ts.i2c_handle->dataCnt = datalen;
	
	ret = gp_ti2c_bus_xfer(ts.i2c_handle);
	if (ret < 0 ) {
			ret = gp_ti2c_bus_xfer(ts.i2c_handle);
		if(ret<0) {
			printk("gsl_ts_write error!\n");
		}
	}
	return ret;
}

static int gsl_ts_read(int addr, char *pdata, unsigned int datalen)
{
	int ret = 0, j;

	ts.i2c_handle->transmitMode = TI2C_BURST_READ_NOSTOPBIT_MODE;
	ts.i2c_handle->subAddrMode = TI2C_NORMAL_SUBADDR_8BITS;
	ts.i2c_handle->pSubAddr = &addr;	
	ts.i2c_handle->pBuf = pdata;
	ts.i2c_handle->dataCnt = datalen;
	
	ret = gp_ti2c_bus_xfer(ts.i2c_handle);
	if (ret < 0) {
		printk("gsl_ts_read error!\n");				
	}
	return ret;
}

static void test_i2c(void)
{
	char read_buf[4] = {0xaa,0xaa,0xaa,0xaa};
	char write_buf[4] = {0x12,0x00,0x00,0x00};

	gsl_ts_read(0xf0, read_buf, sizeof(read_buf));
	printk("gslX680 test_i2c read 0xf0: %x_%x_%x_%x\n",read_buf[3],read_buf[2],read_buf[1],read_buf[0]);
	gsl_ts_write(0xf0, write_buf, sizeof(write_buf));
	printk("gslX680 test_i2c write 0xf0: %x_%x_%x_%x\n",write_buf[3],write_buf[2],write_buf[1],write_buf[0]);
	gsl_ts_read(0xf0, read_buf, sizeof(read_buf));
	printk("gslX680 test_i2c read 0xf0: %x_%x_%x_%x\n",read_buf[3],read_buf[2],read_buf[1],read_buf[0]);

}


static void startup_chip(void)
{
	char buf[4] = {0x00};
#if 1
	char buf[4] = {0x00};
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

static void clr_reg(struct i2c_client *client)
{
	u8 write_buf[4]	= {0};

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

static inline unsigned int  join_bytes(char a, char b)
{
	unsigned int ab = 0;
	ab = ab | a;
	ab = ab << 8 | b;
	return ab;
}

static void gsl_load_fw(void)
{
	char buf[4] = {0};
	char reg = 0;
	unsigned int source_line = 0;
	unsigned int source_len;
	char read_buf[4] = {0};
	struct fw_data *ptr_fw;

	printk("=============gsl_load_fw start==============\n");


	ptr_fw = GSLX680_FW;
	source_len = ARRAY_SIZE(GSLX680_FW);

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

static void check_mem_data(void)
{
	char write_buf;
	char read_buf[4]  = {0};
	
	msleep(30);

	gsl_ts_read(0xb0, read_buf, sizeof(read_buf));
	if (read_buf[3] != 0x5a || read_buf[2] != 0x5a || read_buf[1] != 0x5a || read_buf[0] != 0x5a)
	{
		printk("#########check mem read 0xb0 = %x %x %x %x #########\n", read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
		gp_gpio_set_output(io_wake,0,0);
		msleep(20);		
		gp_gpio_set_output(io_wake,1,0);
		msleep(20);	
		//test_i2c();
		clr_reg();
		reset_chip();
		gsl_load_fw();
		startup_chip();
		reset_chip();
		startup_chip();
	}
}

static void 
record_point(u16 x, u16 y , char id)
{
	u16 x_err =0;
	u16 y_err =0;

	id_sign[id]=id_sign[id]+1;
	
	if(id_sign[id]==1){
		x_old[id]=x;
		y_old[id]=y;
	}

	x = (x_old[id] + x)/2;
	y = (y_old[id] + y)/2;
		
	if(x>x_old[id]){
		x_err=x -x_old[id];
	}
	else{
		x_err=x_old[id]-x;
	}

	if(y>y_old[id]){
		y_err=y -y_old[id];
	}
	else{
		y_err=y_old[id]-y;
	}

	if( (x_err > 3 && y_err > 1) || (x_err > 1 && y_err > 3) ){
		x_new = x;     x_old[id] = x;
		y_new = y;     y_old[id] = y;
	}
	else{
		if(x_err > 3){
			x_new = x;     x_old[id] = x;
		}
		else
			x_new = x_old[id];
		if(y_err> 3){
			y_new = y;     y_old[id] = y;
		}
		else
			y_new = y_old[id];
	}

}

static void
report_data(
	unsigned short x,
	unsigned short y,
	unsigned char pressure,
	unsigned char id
)
{
	unsigned short temp;
	temp = x;
	x = y;
	y = temp;	
	if(x>=SCREEN_MAX_X||y>=SCREEN_MAX_Y)
		return;
	print_info("x=%d, y=%d, id=%d\n", x, y, id);

	input_report_abs(ts.dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(ts.dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts.dev, ABS_MT_POSITION_Y, y);
	input_report_abs(ts.dev, ABS_MT_TOUCH_MAJOR, pressure);
	input_mt_sync(ts.dev);
}

/**
 * interrupt callback
 */
void gp_ts_callback(void* client)
{
	gp_gpio_enable_irq(ts.client, 0);
	queue_work(ts.touch_wq, &ts.mt_work);
}

static void
gp_mt_set_nice_work(
	struct work_struct *work
)
{
	print_info("[%s:%d]\n", __FUNCTION__, __LINE__);
	set_user_nice(current, -20);
}

static void
gp_multi_touch_work(
	struct work_struct *work
)
{
	int i,ret;
	char touched, id;
	unsigned short x, y;
	unsigned int pending;
	int irq_state;
 	char tp_data[(MULTI_TP_POINTS + 1)*4 ];
 	
	print_info("WQ  gp_multi_touch_work.\n");

#if ADJUST_CPU_FREQ
	clockstatus_configure(CLOCK_STATUS_TOUCH,1);
#endif

	ret = gsl_ts_read(0x80, tp_data, sizeof(tp_data));
	if( ret < 0) {
		print_info("gp_tp_get_data fail,return %d\n",ret);
		gp_gpio_enable_irq(ts.client, 1);
		return;
	}

	touched = (tp_data[0]< MULTI_TP_POINTS ? tp_data[0] : MULTI_TP_POINTS);
#ifdef GSL_NOID_VERSION
	cinfo.finger_num = touched;
	print_info("tp-gsl  finger_num = %d\n",cinfo.finger_num);
	for(i = 0; i < (touches < MAX_CONTACTS ? touches : MAX_CONTACTS); i ++)
	{
		cinfo.x[i] = join_bytes( ( ts->touch_data[ts->dd->x_index  + 4 * i + 1] & 0xf),
				ts->touch_data[ts->dd->x_index + 4 * i]);
		cinfo.x[i] = join_bytes(ts->touch_data[ts->dd->y_index + 4 * i + 1],
				ts->touch_data[ts->dd->y_index + 4 * i ]);
		print_info("tp-gsl  x = %d y = %d \n",cinfo.x[i],cinfo.y[i]);
	}
	gsl_alg_id_main(&cinfo);
	touched = cinfo.finger_num;
#endif
	for(i=1;i<=MAX_CONTACTS;i++)
	{
		id_state_flag[i] = 0;		
	}	
	for (i = 0; i < touched; i++) {
	#ifdef GSL_NOID_VERSION
		id = cinfo.id[i];
		x =  cinfo.x[i];
		y =  cinfo.y[i];	
	#else		
		id = tp_data[4 *( i + 1) + 3] >> 4;
		x = join_bytes(tp_data[4 *( i + 1) + 3] & 0xf,tp_data[4 *( i + 1) + 2]);
		y = join_bytes(tp_data[4 *( i + 1) + 1],tp_data[4 *( i + 1) + 0]);		
	#endif
		if(1 <= id && id <= MAX_CONTACTS){
			record_point(x, y, id);
			report_data(x_new, y_new, 10, id);
			id_state_flag[id] = 1;
		}
	}
	if (touched == 0) {
		input_mt_sync(ts.dev);
	}
	for(i=1;i<=MAX_CONTACTS;i++)
	{	
		if( (0 == touched) || ((0 != id_state_old_flag[i]) && (0 == id_state_flag[i])) )
		{
			id_sign[i]=0;
		}
		id_state_old_flag[i] = id_state_flag[i];		
	}

#if ADJUST_CPU_FREQ
	if(touched == 0){
		clockstatus_configure(CLOCK_STATUS_TOUCH,0);
	}
#endif
	ts.prev_touched = touched;
	input_sync(ts.dev);


__error_check_touch_int:
__error_get_mt_data:
	/* Clear interrupt flag */
	pending = (1 << GPIO_PIN_NUMBER(ts.intIoIndex));
	gpHalGpioSetIntPending(ts.intIoIndex, pending);

	gp_gpio_enable_irq(ts.client, 1);
}

/** device driver probe*/
static int __init gp_tp_probe(struct platform_device *pdev)
{
	int rc;
	int ret = 0;
	int intidx, slaveAddr;
	int io_wake= -1;
	gpio_content_t ctx;	
	unsigned int debounce = 1;//27000; /*1ms*/
	gp_board_touch_t *touch_config = NULL;
	
	print_info("Entering gp_tp_probe\n");

#ifdef VIRTUAL_KEYS
	virtual_keys_init();
#endif

	memset(&ts, 0, sizeof(gp_tp_t));

	/* Create single thread work queue */
	ts.touch_wq = create_singlethread_workqueue("touch_wq");
	if (!ts.touch_wq)
	{
		print_info("%s unable to create single thread work queue\n", __func__);
		ret = -ENOMEM;
		goto __err_work_queue;
	}
	INIT_WORK(&ts.mt_set_nice_work, gp_mt_set_nice_work);
	queue_work(ts.touch_wq, &ts.mt_set_nice_work);
	touch_config = gp_board_get_config("touch", gp_board_touch_t);
	if ( touch_config != NULL && touch_config->get_touch_intpin != NULL) {
		touch_config->get_touch_intpin(&intidx);
	} else {
		intidx = MK_GPIO_INDEX(0,0,0,0);
	}
	print_info("intidx = %d \n",intidx);
		
	if ( touch_config != NULL && touch_config->get_i2c_slaveaddr != NULL) {
		touch_config->get_i2c_slaveaddr(&slaveAddr);
	} else {
		slaveAddr = TI2C_ADDR;
	}
	print_info("slaveAddr = %x \n",slaveAddr);
		
	ts.dev = input_allocate_device();
	if ( NULL==ts.dev ){
		print_info("Unable to alloc input device\n");
		ret = -ENOMEM;
		goto __err_alloc;
	}

	ts.i2c_handle = (ti2c_set_value_t *)kmalloc(sizeof(ti2c_set_value_t), GFP_KERNEL);
	if (NULL == ts.i2c_handle) {
		ret = -ENOMEM;
		goto __err_i2c_allocate;
	}

	ts.i2c_handle->pDeviceString = "gslX680";
	ts.i2c_handle->slaveAddrMode = TI2C_NORMAL_SLAVEADDR_8BITS;
	ts.i2c_handle->slaveAddr = (unsigned short)slaveAddr;
	ts.i2c_handle->clockRate = TI2C_CLK;
	ts.i2c_handle->apbdmaEn = true;
	/* open ti2c */
	ret = gp_ti2c_bus_request(ts.i2c_handle);
	if(ret != 0) {
		print_info("[%s], Open TI2C device fail.\n", __FUNCTION__);
		ret = -EIO;
		goto __err_i2c;
	}

	__set_bit(EV_ABS, ts.dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_device->propbit);
	input_device->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(ts.dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X - 1, 0, 0);
	input_set_abs_params(ts.dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y - 1, 0, 0);
	input_set_abs_params(ts.dev, ABS_MT_TRACKING_ID, 0, (MAX_CONTACTS + 1), 0, 0);
	input_set_abs_params(ts.dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	input_set_capability(ts.dev, EV_KEY, KEY_BACK);
	input_set_capability(ts.dev, EV_KEY, KEY_HOME);
	input_set_capability(ts.dev, EV_KEY, KEY_MENU);

	ts.dev->name = "gp_ts";
	ts.dev->phys = "gp_ts";
	ts.dev->id.bustype = BUS_I2C;

	/* All went ok, so register to the input system */
	rc = input_register_device(ts.dev);
	if (rc) {
		ret = -EIO;
		goto __err_reg_input;
	}


	ctx.pin_index = MK_GPIO_INDEX( 1, 0, 17, 11 );  
	io_wake = gp_gpio_request( ctx.pin_index, NULL );
	gp_gpio_set_function( io_wake, 0 );
	gp_gpio_set_direction( io_wake, GPIO_DIR_OUTPUT );
	gp_gpio_set_output(io_wake,0,0);
	msleep(20);		
	gp_gpio_set_output(io_wake,1,0);
	msleep(20);
	//test_i2c();
	clr_reg();
	reset_chip();
	gsl_load_fw();
	startup_chip();
	reset_chip();			
	startup_chip();
	
	INIT_WORK(&ts.mt_work, gp_multi_touch_work);
	
	ts.client = gp_gpio_request(intidx, "touch_int"); /* GPIO1[7] ---- */
	if(IS_ERR((void*)ts.client)) {
		print_info("%s unable to register client\n", __func__);
		ret = -ENOMEM;
		goto __err_register;
	}

	gp_gpio_set_input(ts.client, GPIO_PULL_LOW);
	gp_gpio_irq_property(ts.client, GPIO_IRQ_EDGE_TRIGGER|GPIO_IRQ_ACTIVE_RISING, &debounce);
	gp_gpio_register_isr(ts.client, gp_ts_callback, (void *)ts.client);

#ifdef GSL_NOID_VERSION
	gsl_DataInit(gsl_config_data_id);
#endif


	print_info("End gp_tp_probe\n");
	return 0;


__err_register:
	input_unregister_device(ts.dev);
__err_reg_input:
	gp_ti2c_bus_release(ts.i2c_handle);
__err_i2c:
	kfree(ts.i2c_handle);	
__err_i2c_allocate:
	//gp_gpio_release(ts.touch_reset);
__err_pin_request:
	input_free_device(ts.dev);
__err_alloc:
	destroy_workqueue(ts.touch_wq);
__err_work_queue:
	return ret;
}

/** device driver remove*/
static int gp_tp_remove(struct platform_device *pdev)
{
	//gp_gpio_release(ts.touch_reset);
	gp_gpio_unregister_isr(ts.client);
	gp_gpio_release(ts.client);
	gp_ti2c_bus_release(ts.i2c_handle);
	kfree(ts.i2c_handle);
	input_unregister_device(ts.dev);
	input_free_device(ts.dev);	
	destroy_workqueue(ts.touch_wq);
	return 0;
}

static int gp_tp_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("Enter gp_tp_suspend.\n");

	gp_gpio_set_output(io_wake,0,0);
	msleep(10);	
	
	return 0;
}

static int gp_tp_resume(struct platform_device *pdev)
{
	int io_wake= -1;
	gpio_content_t ctx;	
		
	printk("Enter gp_tp_resume.\n");

	gp_gpio_set_output(io_wake,1,0);
	msleep(20);
	reset_chip();			
	startup_chip();
	check_mem_data();

	gp_gpio_enable_irq(ts.client, 1);
		
	return 0;
}

static void gp_tp_device_release(struct device *dev)
{
	DIAG_INFO("remove touch pad device ok\n");
}

static struct platform_device gp_tp_device = {
	.name = "gp_tp",
	.id   = -1,
	.dev	= {
		.release = gp_tp_device_release,
	}
};

static struct platform_driver gp_tp_driver = {
       .driver         = {
	       .name   = "gp_tp",
	       .owner  = THIS_MODULE,
       },
       .probe          = gp_tp_probe,
       .remove         = gp_tp_remove,
       .suspend        = gp_tp_suspend,
       .resume         = gp_tp_resume,

};

static int __init gp_tp_module_init(void)
{
	int rc;

	platform_device_register(&gp_tp_device);
	rc = platform_driver_register(&gp_tp_driver);
	return rc;
}

static void __exit gp_tp_module_exit(void)
{
	platform_device_unregister(&gp_tp_device);
	platform_driver_unregister(&gp_tp_driver);
}

module_init(gp_tp_module_init);
module_exit(gp_tp_module_exit);
MODULE_LICENSE_GP;

#endif
