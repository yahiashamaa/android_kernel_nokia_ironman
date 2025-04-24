#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/kthread.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/spi/spidev.h>
#include <linux/semaphore.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/input.h>
#include <linux/signal.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/cdev.h>

#include <linux/platform_device.h>
#include <linux/of_platform.h>

#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#endif

#if defined(CONFIG_MTK_HARDWAREINFO_SUPPORT)
extern char *fingerprint_name_info;
#endif

#include <linux/platform_data/spi-mt65xx.h>
struct mtk_spi {
	void __iomem *base;
	void __iomem *peri_regs;
	u32 state;
	int pad_num;
	u32 *pad_sel;
	struct clk *parent_clk, *sel_clk, *spi_clk;
	struct spi_transfer *cur_transfer;
	u32 xfer_len;
	struct scatterlist *tx_sgl, *rx_sgl;
	u32 tx_sgl_len, rx_sgl_len;
	const struct mtk_spi_compatible *dev_comp;
	u32 dram_8gb_offset;
};

void mt_spi_enable_master_clk(struct spi_device *spi);
void mt_spi_disable_master_clk(struct spi_device *spi);
//#ifdef CONFIG_SPI_MT65XX
//extern void mt_spi_enable_master_clk(struct spi_device *spidev);
//extern void mt_spi_disable_master_clk(struct spi_device *spidev);
//#endif

//#define COMPAT_VENDOR

#ifdef COMPAT_VENDOR
//#include "tee_client_api.h"
//#include <../../../misc/mediatek/tkcore/include/linux/tee_fp.h>
#include "fp_func.h"
#include "mtk_spi.h"
struct TEEC_UUID cdfinger_uuid = {0x7778c03f, 0xc30c, 0x4dd0,
								{0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x0c}};
#endif

//#define PINBO_TEE_COMPATIBLE

#ifdef PINBO_TEE_COMPATIBLE
//#include <tee_fp.h>
#include <../../../misc/mediatek/tkcore/include/linux/tee_fp.h>
#endif

static struct mtk_chip_config spi_conf={
	.cs_setuptime = 10,
	.cs_holdtime = 10,
	.cs_idletime = 1,
	.deassert_mode = 0,
	.sample_sel = 0,
	.tick_delay = 0,
};


static u8 cdfinger_debug = 0x00;
static int isInKeyMode = 0; // key mode
static int screen_status = 1; // screen on
static int sign_sync = 0; // for poll
typedef struct key_report{
	int key;
	int value;
}key_report_t;

#define CDFINGER_DBG(fmt, args...) \
	do{ \
		if(cdfinger_debug & 0x01) \
			printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
	}while(0)
#define CDFINGER_FUNCTION(fmt, args...) \
	do{ \
		if(cdfinger_debug & 0x02) \
			printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
	}while(0)
#define CDFINGER_REG(fmt, args...) \
	do{ \
		if(cdfinger_debug & 0x04) \
			printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
	}while(0)
#define CDFINGER_ERR(fmt, args...) \
    do{ \
		printk( "[DBG][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
    }while(0)
#define CDFINGER_PROBE(fmt, args...) \
    do{ \
		printk( "[PROBE][cdfinger]:%5d: <%s>" fmt, __LINE__,__func__,##args ); \
    }while(0)


    
#define DTS_PROBE    
#define HAS_RESET_PIN

#define VERSION                         "cdfinger version 3.2"
#define DEVICE_NAME                     "fpsdev0"
#define SPI_DRV_NAME                    "cdfinger"

/**************** Custom device : platfotm  or spi **************/
//#define  USE_PLATFORM_BUS     1
#define  USE_SPI_BUS  1
 
struct spi_device *spi_fingerprint;
int cdfinger_fp_exist = -1;

/**************************************************************/

#define CDFINGER_IOCTL_MAGIC_NO          0xFB
#define CDFINGER_INIT                    _IOW(CDFINGER_IOCTL_MAGIC_NO, 0, uint8_t)
#define CDFINGER_GETIMAGE                _IOW(CDFINGER_IOCTL_MAGIC_NO, 1, uint8_t)
#define CDFINGER_INITERRUPT_MODE	     _IOW(CDFINGER_IOCTL_MAGIC_NO, 2, uint8_t)
#define CDFINGER_INITERRUPT_KEYMODE      _IOW(CDFINGER_IOCTL_MAGIC_NO, 3, uint8_t)
#define CDFINGER_INITERRUPT_FINGERUPMODE _IOW(CDFINGER_IOCTL_MAGIC_NO, 4, uint8_t)
#define CDFINGER_RELEASE_WAKELOCK        _IO(CDFINGER_IOCTL_MAGIC_NO, 5)
#define CDFINGER_CHECK_INTERRUPT         _IO(CDFINGER_IOCTL_MAGIC_NO, 6)
#define CDFINGER_SET_SPI_SPEED           _IOW(CDFINGER_IOCTL_MAGIC_NO, 7, uint8_t)
#define	CDFINGER_GETID			 		 _IO(CDFINGER_IOCTL_MAGIC_NO,9)
#define	CDFINGER_SETID						_IOW(CDFINGER_IOCTL_MAGIC_NO,8,uint8_t)
#define CDFINGER_REPORT_KEY_LEGACY              _IOW(CDFINGER_IOCTL_MAGIC_NO, 10, uint8_t)
#define CDFINGER_REPORT_KEY              _IOW(CDFINGER_IOCTL_MAGIC_NO, 19, key_report_t)
#define CDFINGER_POWERDOWN               _IO(CDFINGER_IOCTL_MAGIC_NO, 11)
#define CDFINGER_ENABLE_IRQ               _IO(CDFINGER_IOCTL_MAGIC_NO, 12)
#define CDFINGER_DISABLE_IRQ               _IO(CDFINGER_IOCTL_MAGIC_NO, 13)
#define CDFINGER_HW_RESET               _IOW(CDFINGER_IOCTL_MAGIC_NO, 14, uint8_t)
#define CDFINGER_GET_STATUS               _IO(CDFINGER_IOCTL_MAGIC_NO, 15)
#define CDFINGER_SPI_CLK               _IOW(CDFINGER_IOCTL_MAGIC_NO, 16, uint8_t)
#define CDFINGER_WAKE_LOCK	           _IOW(CDFINGER_IOCTL_MAGIC_NO,26,uint8_t)
#define CDFINGER_ENABLE_CLK				  _IOW(CDFINGER_IOCTL_MAGIC_NO, 30, uint8_t)
#define CDFINGER_POLL_TRIGGER			 _IO(CDFINGER_IOCTL_MAGIC_NO,31)
#define CDFINGER_NEW_KEYMODE		_IOW(CDFINGER_IOCTL_MAGIC_NO, 37, uint8_t)
#define KEY_INTERRUPT                   KEY_F11

enum work_mode {
	CDFINGER_MODE_NONE       = 1<<0,
	CDFINGER_INTERRUPT_MODE  = 1<<1,
	CDFINGER_KEY_MODE        = 1<<2,
	CDFINGER_FINGER_UP_MODE  = 1<<3,
	CDFINGER_READ_IMAGE_MODE = 1<<4,
	CDFINGER_MODE_MAX
};

static struct cdfinger_data {
	struct spi_device *spi;
#if defined(USE_PLATFORM_BUS)	
	struct platform_device *pd;
#endif

	struct mutex buf_lock;
	unsigned int irq;
	int irq_enabled;
	u32 chip_id;

	u32 vdd_ldo_enable;
	u32 vio_ldo_enable;
	u32 config_spi_pin;

	struct pinctrl *fps_pinctrl;
	struct pinctrl_state *fps_reset_high;
	struct pinctrl_state *fps_reset_low;
	struct pinctrl_state *fps_power_on;
	struct pinctrl_state *fps_power_off;
	struct pinctrl_state *fps_vio_on;
	struct pinctrl_state *fps_vio_off;
	struct pinctrl_state *cdfinger_spi_miso;
	struct pinctrl_state *cdfinger_spi_mosi;
	struct pinctrl_state *cdfinger_spi_sck;
	struct pinctrl_state *cdfinger_spi_cs;
	struct pinctrl_state *cdfinger_irq;

	int thread_wakeup;
	int process_interrupt;
	int key_report;
	enum work_mode device_mode;
	//struct timer_list int_timer;
	struct input_dev *cdfinger_inputdev;
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source cdfinger_lock;
#else
	struct wake_lock cdfinger_lock;
#endif
	struct task_struct *cdfinger_thread;
	struct fasync_struct *async_queue;
	uint8_t cdfinger_interrupt;
	struct notifier_block notifier;
	struct mt_spi_t * cdfinger_ms;
	struct mtk_chip_config mtk_spi_config;

}*g_cdfinger;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DECLARE_WAIT_QUEUE_HEAD(cdfinger_waitqueue);



#ifdef CONFIG_PM_WAKELOCKS
static inline void wakeup_source_prepare(struct wakeup_source *ws, const char *name)
{
    if (ws) {
        memset(ws, 0, sizeof(*ws));
        ws->name = name;
    }
}

static inline void wakeup_source_drop(struct wakeup_source *ws)
{
    if (!ws)
        return;
    __pm_relax(ws);
}

static inline void wakeup_source_init(struct wakeup_source *ws,
                      const char *name)
{
    wakeup_source_prepare(ws, name);
    wakeup_source_add(ws);
}

static inline void wakeup_source_trash(struct wakeup_source *ws)
{
    wakeup_source_remove(ws);
    wakeup_source_drop(ws);
}
#endif



static void cdfinger_disable_irq(struct cdfinger_data *cdfinger)
{
	if(cdfinger->irq_enabled == 1)
	{
		disable_irq_nosync(cdfinger->irq);
		cdfinger->irq_enabled = 0;
		CDFINGER_DBG("irq disable\n");
	}
}

static void cdfinger_enable_irq(struct cdfinger_data *cdfinger)
{
	if(cdfinger->irq_enabled == 0)
	{
		enable_irq(cdfinger->irq);
		cdfinger->irq_enabled =1;
		CDFINGER_DBG("irq enable\n");
	}
}
static int cdfinger_getirq_from_platform(struct cdfinger_data *cdfinger)
{
	if(!(cdfinger->spi->dev.of_node)){
		CDFINGER_ERR("of node not exist!\n");
		return -1;
	}

	cdfinger->irq = irq_of_parse_and_map(cdfinger->spi->dev.of_node, 0);
	#if 0
	if(cdfinger->irq < 0)
	{
		CDFINGER_ERR("parse irq failed! irq[%d]\n",cdfinger->irq);
		return -1;
	}
	#endif
	CDFINGER_PROBE("get irq success! irq[%d]\n",cdfinger->irq);
	pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_irq);
	return 0;
}

static int cdfinger_parse_dts(struct cdfinger_data *cdfinger)
{
	int ret = -1;	
	if(cdfinger->spi == NULL)
	{
		CDFINGER_ERR("spi is NULL !\n");
		goto parse_err;
	}
#ifdef DTS_PROBE
	cdfinger->spi->dev.of_node = of_find_compatible_node(NULL,NULL,"mediatek,mt6765-fingerprint");
	if (!cdfinger->spi->dev.of_node) {
			CDFINGER_ERR("of_find_compatible_node(..) failed.\n");
		goto parse_err;
	}
	printk("cdfinger DTS_PROBE open!\n");
#endif
	
	of_property_read_u32(cdfinger->spi->dev.of_node,"vdd_ldo_enable",&cdfinger->vdd_ldo_enable);
	of_property_read_u32(cdfinger->spi->dev.of_node,"vio_ldo_enable",&cdfinger->vio_ldo_enable);
	of_property_read_u32(cdfinger->spi->dev.of_node,"config_spi_pin",&cdfinger->config_spi_pin);


	CDFINGER_PROBE("vdd_ldo_enable[%d], vio_ldo_enable[%d], config_spi_pin[%d]\n",
		cdfinger->vdd_ldo_enable, cdfinger->vio_ldo_enable, cdfinger->config_spi_pin);

	cdfinger->fps_pinctrl = devm_pinctrl_get(&cdfinger->spi->dev);
	if (IS_ERR(cdfinger->fps_pinctrl)) {
		ret = PTR_ERR(cdfinger->fps_pinctrl);
		CDFINGER_ERR("Cannot find fingerprint cdfinger->fps_pinctrl! ret=%d\n", ret);
		goto parse_err;
	}
	cdfinger->cdfinger_irq = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_irq");
	if (IS_ERR(cdfinger->cdfinger_irq))
	{
		ret = PTR_ERR(cdfinger->cdfinger_irq);
		CDFINGER_ERR("cdfinger->cdfinger_irq ret = %d\n",ret);
		goto parse_err;
	}
	cdfinger->fps_reset_low = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_reset_low");
	if (IS_ERR(cdfinger->fps_reset_low))
	{
		ret = PTR_ERR(cdfinger->fps_reset_low);
		CDFINGER_ERR("cdfinger->fps_reset_low ret = %d\n",ret);
		goto parse_err;
	}
	cdfinger->fps_reset_high = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_reset_high");
	if (IS_ERR(cdfinger->fps_reset_high))
	{
		ret = PTR_ERR(cdfinger->fps_reset_high);
		CDFINGER_ERR("cdfinger->fps_reset_high ret = %d\n",ret);
		goto parse_err;
	}

	if(cdfinger->config_spi_pin == 1)
	{
		cdfinger->cdfinger_spi_miso = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_miso");
		if (IS_ERR(cdfinger->cdfinger_spi_miso))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_miso);
			CDFINGER_ERR("cdfinger->cdfinger_spi_miso ret = %d\n",ret);
			goto parse_err;
		}
		cdfinger->cdfinger_spi_mosi = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_mosi");
		if (IS_ERR(cdfinger->cdfinger_spi_mosi))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_mosi);
			CDFINGER_ERR("cdfinger->cdfinger_spi_mosi ret = %d\n",ret);
			goto parse_err;
		}
		cdfinger->cdfinger_spi_sck = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_sck");
		if (IS_ERR(cdfinger->cdfinger_spi_sck))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_sck);
			CDFINGER_ERR("cdfinger->cdfinger_spi_sck ret = %d\n",ret);
			goto parse_err;
		}
		cdfinger->cdfinger_spi_cs = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_spi_cs");
		if (IS_ERR(cdfinger->cdfinger_spi_cs))
		{
			ret = PTR_ERR(cdfinger->cdfinger_spi_cs);
			CDFINGER_ERR("cdfinger->cdfinger_spi_cs ret = %d\n",ret);
			goto parse_err;
		}
	}

	if(cdfinger->vdd_ldo_enable == 1)
	{
		cdfinger->fps_power_on = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fp_pwr_high");
		if (IS_ERR(cdfinger->fps_power_on))
		{
			ret = PTR_ERR(cdfinger->fps_power_on);
			CDFINGER_ERR("cdfinger->fps_power_on ret = %d\n",ret);
			goto parse_err;
		}

		cdfinger->fps_power_off = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fp_pwr_low");
		if (IS_ERR(cdfinger->fps_power_off))
		{
			ret = PTR_ERR(cdfinger->fps_power_off);
			CDFINGER_ERR("cdfinger->fps_power_off ret = %d\n",ret);
			goto parse_err;
		}
	}

	if(cdfinger->vio_ldo_enable == 1)
	{
		cdfinger->fps_vio_on = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_vio_high");
		if (IS_ERR(cdfinger->fps_vio_on))
		{
			ret = PTR_ERR(cdfinger->fps_vio_on);
			CDFINGER_ERR("cdfinger->fps_vio_on ret = %d\n",ret);
			goto parse_err;
		}

		cdfinger->fps_vio_off = pinctrl_lookup_state(cdfinger->fps_pinctrl,"fingerprint_vio_low");
		if (IS_ERR(cdfinger->fps_vio_off))
		{
			ret = PTR_ERR(cdfinger->fps_vio_off);
			CDFINGER_ERR("cdfinger->fps_vio_off ret = %d\n",ret);
			goto parse_err;
		}
	}

	return 0;
parse_err:
	CDFINGER_ERR("parse dts failed!\n");

	return ret;
}

#ifdef COMPAT_VENDOR
static int spi_send_cmd(struct cdfinger_data *cdfinger,  u8 *tx, u8 *rx, u16 spilen)
{
	struct spi_message m;
	struct spi_transfer t = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = spilen,
	};

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return spi_sync(cdfinger->spi, &m);
}
#endif

static void cdfinger_power_on(struct cdfinger_data *cdfinger)
{
	if(cdfinger->config_spi_pin == 0)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_miso);
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_mosi);
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_sck);
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->cdfinger_spi_cs);
	}

	if(cdfinger->vdd_ldo_enable == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_power_on);
	}

	if(cdfinger->vio_ldo_enable == 1)
	{
		pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_vio_on);
	}
}


static void cdfinger_reset(int count)
{
#ifdef HAS_RESET_PIN
	struct cdfinger_data *cdfinger = g_cdfinger;
	CDFINGER_DBG("cdfinger_reset count = %d", count);
	pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_low);
	mdelay(count);
	pinctrl_select_state(cdfinger->fps_pinctrl, cdfinger->fps_reset_high);
	mdelay(count);
#endif
}


static void cdfinger_release_wakelock(struct cdfinger_data *cdfinger)
{
	CDFINGER_FUNCTION("enter\n");
#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(&cdfinger->cdfinger_lock);
#else
	wake_unlock(&cdfinger->cdfinger_lock);
#endif
	CDFINGER_FUNCTION("exit\n");
}

static int cdfinger_mode_init(struct cdfinger_data *cdfinger, uint8_t arg, enum work_mode mode)
{
	CDFINGER_DBG("mode=0x%x\n", mode);
	cdfinger->process_interrupt = 1;
	cdfinger->device_mode = mode;
	cdfinger->key_report = 0;

	return 0;
}

static void cdfinger_wake_lock(struct cdfinger_data *cdfinger,int arg)
{
	CDFINGER_DBG("cdfinger_wake_lock enter----------\n");
	if(arg)
	{
#ifdef CONFIG_PM_WAKELOCKS
		__pm_stay_awake(&cdfinger->cdfinger_lock);
#else
		wake_lock(&cdfinger->cdfinger_lock);
#endif
	}
	else
	{
#ifdef CONFIG_PM_WAKELOCKS
		__pm_relax(&cdfinger->cdfinger_lock);
		__pm_wakeup_event(&cdfinger->cdfinger_lock, jiffies_to_msecs(3*HZ));
#else
		wake_unlock(&cdfinger->cdfinger_lock);
		wake_lock_timeout(&cdfinger->cdfinger_lock,3*HZ);
#endif
	}
}

int cdfinger_report_key(struct cdfinger_data *cdfinger, unsigned long arg)
{
	key_report_t report;
	int ret = 0;

	if (copy_from_user(&report, (key_report_t *)arg, sizeof(key_report_t)))
	{
		CDFINGER_ERR("%s err\n", __func__);
		return -1;
	}
	CDFINGER_ERR("enter report key is %d \n", report.key);
	switch(report.key)
	{
	case KEY_UP:
		report.key=KEY_UP_NOTIF;
		ret = 1;
		break;
	case KEY_DOWN:
		report.key=KEY_DOWN_NOTIF;
		ret = 1;
		break;
	case KEY_RIGHT:
		//report.key=KEY_DOWN_NOTIF;
		break;
	case KEY_LEFT:
		//report.key=KEY_DOWN_NOTIF;
		break;
	case KEY_F12:
		report.key=KEY_LONG_PRESS;
		ret = 1;
	default:
		break;
	}

	if(ret){
		input_report_key(cdfinger->cdfinger_inputdev, report.key, !!report.value);
		input_sync(cdfinger->cdfinger_inputdev);
		CDFINGER_ERR("exit \n");
	}
	
	return 0;
}

static int cdfinger_report_key_legacy(struct cdfinger_data *cdfinger, uint8_t arg)
{
	CDFINGER_FUNCTION("enter\n");
	input_report_key(cdfinger->cdfinger_inputdev, KEY_INTERRUPT, !!arg);
	input_sync(cdfinger->cdfinger_inputdev);
	CDFINGER_FUNCTION("exit\n");

	return 0;
}
static unsigned int cdfinger_poll(struct file *filp, struct poll_table_struct *wait)
{
	int mask = 0;
	poll_wait(filp, &cdfinger_waitqueue, wait);
	if (sign_sync == 1)
	{
		mask |= POLLIN|POLLPRI;
	} else if (sign_sync == 2)
	{
		mask |= POLLOUT;
	}
	sign_sync = 0;
	CDFINGER_DBG("mask %u\n",mask);
	return mask;
}
static long cdfinger_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct cdfinger_data *cdfinger = filp->private_data;
	int ret = 0;

	CDFINGER_FUNCTION("enter\n");
	if(cdfinger == NULL)
	{
		CDFINGER_ERR("%s: fingerprint please open device first!\n", __func__);
		return -EIO;
	}

	mutex_lock(&cdfinger->buf_lock);
	switch (cmd) {
		case CDFINGER_INIT:
			break;
		case CDFINGER_GETIMAGE:
			break;
		case CDFINGER_INITERRUPT_MODE:
			sign_sync = 0;
			isInKeyMode = 1;  // not key mode
			cdfinger_reset(2);
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_INTERRUPT_MODE);
			break;
		case CDFINGER_NEW_KEYMODE:
			isInKeyMode = 0;
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_INTERRUPT_MODE);
			break;
		case CDFINGER_INITERRUPT_FINGERUPMODE:
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_FINGER_UP_MODE);
			break;
		case CDFINGER_RELEASE_WAKELOCK:
			cdfinger_release_wakelock(cdfinger);
			break;
		case CDFINGER_INITERRUPT_KEYMODE:
			ret = cdfinger_mode_init(cdfinger,arg,CDFINGER_KEY_MODE);
			break;
		case CDFINGER_CHECK_INTERRUPT:
			break;
		case CDFINGER_SET_SPI_SPEED:
			break;
		case CDFINGER_WAKE_LOCK:
			cdfinger_wake_lock(cdfinger,arg);
			break;
		case CDFINGER_REPORT_KEY:
			ret = cdfinger_report_key(cdfinger, arg);
			break;
		case CDFINGER_REPORT_KEY_LEGACY:
			ret = cdfinger_report_key_legacy(cdfinger, arg);
			break;
		case CDFINGER_POWERDOWN:
			break;
		case CDFINGER_ENABLE_IRQ:
			cdfinger_enable_irq(cdfinger);
			break;
		case CDFINGER_DISABLE_IRQ:
			cdfinger_disable_irq(cdfinger);
			break;
		case CDFINGER_ENABLE_CLK:
		case CDFINGER_SPI_CLK:
			if (arg == 1)
				mt_spi_enable_master_clk(cdfinger->spi);

			else if (arg == 0)
				mt_spi_disable_master_clk(cdfinger->spi);

			break;
		case CDFINGER_HW_RESET:
			cdfinger_reset(arg);
			break;
		case CDFINGER_SETID:
			cdfinger->chip_id = arg;
			CDFINGER_DBG("set cdfinger chip id 0x%x\n",cdfinger->chip_id);
			break;
		case CDFINGER_GETID:
			ret = cdfinger->chip_id;
			CDFINGER_DBG("get cdfinger chip id 0x%x\n",ret);
			break;
		case CDFINGER_GET_STATUS:
			ret = screen_status;
			break;
		case CDFINGER_POLL_TRIGGER:
			sign_sync = 2;
			wake_up_interruptible(&cdfinger_waitqueue);
			ret = 0;
			break;
		default:
			ret = -ENOTTY;
			break;
	}
	mutex_unlock(&cdfinger->buf_lock);
	CDFINGER_FUNCTION("exit\n");

	return ret;
}

static int cdfinger_open(struct inode *inode, struct file *file)
{
	CDFINGER_FUNCTION("enter\n");
	file->private_data = g_cdfinger;
	CDFINGER_FUNCTION("exit\n");

	return 0;
}

static ssize_t cdfinger_write(struct file *file, const char *buff, size_t count, loff_t * ppos)
{
	return 0;
}

static int cdfinger_async_fasync(int fd, struct file *filp, int mode)
{
	struct cdfinger_data *cdfinger = g_cdfinger;

	CDFINGER_FUNCTION("enter\n");
	return fasync_helper(fd, filp, mode, &cdfinger->async_queue);
}

static ssize_t cdfinger_read(struct file *file, char *buff, size_t count, loff_t * ppos)
{
	return 0;
}

static int cdfinger_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations cdfinger_fops = {
	.owner = THIS_MODULE,
	.open = cdfinger_open,
	.write = cdfinger_write,
	.read = cdfinger_read,
	.release = cdfinger_release,
	.fasync = cdfinger_async_fasync,
	.unlocked_ioctl = cdfinger_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cdfinger_ioctl,
#endif
	.poll = cdfinger_poll,
};

static struct miscdevice cdfinger_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &cdfinger_fops,
};

static void cdfinger_async_Report(void)
{
	struct cdfinger_data *cdfinger = g_cdfinger;

	CDFINGER_FUNCTION("enter\n");
	kill_fasync(&cdfinger->async_queue, SIGIO, POLL_IN);
	CDFINGER_FUNCTION("exit\n");
}
/*
static void int_timer_handle(unsigned long arg)
{
	struct cdfinger_data *cdfinger = g_cdfinger;

	CDFINGER_DBG("enter\n");
	if ((cdfinger->device_mode == CDFINGER_KEY_MODE) && (cdfinger->key_report == 1)) {
		input_report_key(cdfinger->cdfinger_inputdev, KEY_INTERRUPT, 0);
		input_sync(cdfinger->cdfinger_inputdev);
		cdfinger->key_report = 0;
	}

	if (cdfinger->device_mode == CDFINGER_FINGER_UP_MODE){
		cdfinger->process_interrupt = 0;
		cdfinger_async_Report();
	}
	CDFINGER_DBG("exit\n");
}
*/
static int cdfinger_thread_func(void *arg)
{
	struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;

	do {
		wait_event_interruptible(waiter, cdfinger->thread_wakeup != 0);
		CDFINGER_DBG("cdfinger:%s,thread wakeup\n",__func__);
		cdfinger->thread_wakeup = 0;
#ifdef CONFIG_PM_WAKELOCKS
		__pm_wakeup_event(&cdfinger->cdfinger_lock, jiffies_to_msecs(3*HZ));
#else
		wake_lock_timeout(&cdfinger->cdfinger_lock,3*HZ);
#endif

		if (cdfinger->device_mode == CDFINGER_INTERRUPT_MODE) {
			cdfinger->process_interrupt = 0;
			sign_sync = 1;
			wake_up_interruptible(&cdfinger_waitqueue);
			cdfinger_async_Report();
			//del_timer_sync(&cdfinger->int_timer);
			continue;
		} else if ((cdfinger->device_mode == CDFINGER_KEY_MODE) && (cdfinger->key_report == 0)) {
			input_report_key(cdfinger->cdfinger_inputdev, KEY_INTERRUPT, 1);
			input_sync(cdfinger->cdfinger_inputdev);
			cdfinger->key_report = 1;
		}

	}while(!kthread_should_stop());

	CDFINGER_ERR("thread exit\n");
	return -1;
}

static irqreturn_t cdfinger_interrupt_handler(unsigned irq, void *arg)
{
	struct cdfinger_data *cdfinger = (struct cdfinger_data *)arg;

	cdfinger->cdfinger_interrupt = 1;
	if (cdfinger->process_interrupt == 1)
	{
		//mod_timer(&cdfinger->int_timer, jiffies + HZ / 10);
		cdfinger->thread_wakeup = 1;
		wake_up_interruptible(&waiter);
	}

	return IRQ_HANDLED;
}

static int cdfinger_create_inputdev(struct cdfinger_data *cdfinger)
{
	cdfinger->cdfinger_inputdev = input_allocate_device();
	if (!cdfinger->cdfinger_inputdev) {
		CDFINGER_ERR("cdfinger->cdfinger_inputdev create faile!\n");
		return -ENOMEM;
	}
	__set_bit(EV_KEY, cdfinger->cdfinger_inputdev->evbit);
	__set_bit(KEY_INTERRUPT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F1, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F2, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F3, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_F4, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_VOLUMEUP, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_VOLUMEDOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_PAGEUP, cdfinger->cdfinger_inputdev->keybit);
    __set_bit(KEY_PAGEDOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_UP, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_LEFT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_RIGHT, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_DOWN, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_ENTER, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_DOWN_NOTIF, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_UP_NOTIF, cdfinger->cdfinger_inputdev->keybit);
	__set_bit(KEY_LONG_PRESS, cdfinger->cdfinger_inputdev->keybit);

	cdfinger->cdfinger_inputdev->id.bustype = BUS_HOST;
	cdfinger->cdfinger_inputdev->name = "cdfinger_inputdev";
	if (input_register_device(cdfinger->cdfinger_inputdev)) {
		CDFINGER_ERR("register inputdev failed\n");
		input_free_device(cdfinger->cdfinger_inputdev);
		return -1;
	}

	return 0;
}

static int cdfinger_fb_notifier_callback(struct notifier_block* self,
                                        unsigned long event, void* data)
{
    struct fb_event* evdata = data;
    unsigned int blank;
    int retval = 0;
	
    if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }
    blank = *(int*)evdata->data;
    switch (blank) {
        case FB_BLANK_UNBLANK:
			CDFINGER_DBG("sunlin==FB_BLANK_UNBLANK==\n");
			mutex_lock(&g_cdfinger->buf_lock);
			screen_status = 1;
			if (isInKeyMode == 0) {
				sign_sync = 1;
				wake_up_interruptible(&cdfinger_waitqueue);
				cdfinger_async_Report();
			}
			mutex_unlock(&g_cdfinger->buf_lock);
            break;

        case FB_BLANK_POWERDOWN:
			CDFINGER_DBG("sunlin==FB_BLANK_POWERDOWN==\n");
			mutex_lock(&g_cdfinger->buf_lock);
			screen_status = 0;
			if (isInKeyMode == 0) {
				sign_sync = 1;
				wake_up_interruptible(&cdfinger_waitqueue);
				cdfinger_async_Report();
			}
			mutex_unlock(&g_cdfinger->buf_lock);
            break;
        default:
            break;
    }

    return retval;
}
#ifdef USE_SPI_BUS
static int tee_spi_transfer(const char *txbuf, char *rxbuf, int len)
{
    struct spi_transfer t;
    struct spi_message m;
    memset(&t, 0, sizeof(t));
    spi_message_init(&m);
    t.tx_buf = txbuf;
    t.rx_buf = rxbuf;
    t.bits_per_word = 8;
    t.len = len;
    t.speed_hz = 1*1000000;
    spi_message_add_tail(&t, &m);
    return spi_sync(g_cdfinger->spi, &m);
}
#endif

#if defined(USE_SPI_BUS)
static int cdfinger_probe(struct spi_device *spi)
#elif defined(USE_PLATFORM_BUS)
static int cdfinger_probe(struct platform_device *spi)
#endif
{
	struct cdfinger_data *cdfinger = NULL;
	int status = -ENODEV;
	
#if defined(COMPAT_VENDOR) || defined(PINBO_TEE_COMPATIBLE) || defined(USE_SPI_BUS)
	int ret = -1;
	int TestCnt;
	unsigned int hwid = 0;
	static uint8_t start = 0x18;
	uint8_t read = 0;
	uint8_t chipid_rx[4] = {0};
	uint8_t chipid_cdfinger_tx[4] = {0x74, 0x66, 0x66, 0x66};
	uint8_t chipid_chipone_tx[4] = {0x00, 0x55, 0x00, 0x00};
#endif
	CDFINGER_PROBE("enter...\n");

	cdfinger = kzalloc(sizeof(struct cdfinger_data), GFP_KERNEL);
	if (!cdfinger) {
		CDFINGER_ERR("alloc cdfinger failed!\n");
		return -ENOMEM;
	}

	g_cdfinger = cdfinger;
	
	cdfinger->spi = spi;
	/* to chipone_fp fp spi */
	spi_fingerprint = spi;

#if defined(USE_PLATFORM_BUS)
	cdfinger->pd = spi;
#endif	
	//cdfinger->cdfinger_ms = spi_master_get_devdata(spi->master);
	if(cdfinger_parse_dts(cdfinger))
	{
		CDFINGER_ERR("%s: parse dts failed!\n", __func__);
		goto free_cdfinger;
	}
	cdfinger->spi->bits_per_word = 8;
	cdfinger->spi->mode = SPI_MODE_0;
	cdfinger->spi->max_speed_hz    = 10 * 1000 * 1000;
	memcpy (&cdfinger->mtk_spi_config, &spi_conf, sizeof (struct mtk_chip_config));
	cdfinger->spi->controller_data = (void *)&cdfinger->mtk_spi_config;
	if(spi_setup(cdfinger->spi) != 0)
	{
		CDFINGER_ERR("%s: spi setup failed!\n", __func__);
		goto free_cdfinger;
	}
	cdfinger_power_on(cdfinger);
#ifdef HAS_RESET_PIN
	cdfinger_reset(50);
#endif
	mt_spi_enable_master_clk(cdfinger->spi);
#ifdef USE_SPI_BUS
	for(TestCnt = 0; TestCnt < 5; TestCnt++) {
		cdfinger_reset(10);
		ret = tee_spi_transfer(&start, &read, 1);
		mdelay(1);
		if (ret == 0) {
			CDFINGER_PROBE("chipid_rx : %x  %x  %x  %x \n",chipid_rx[0],chipid_rx[1],chipid_rx[2],chipid_rx[3]);
			ret = tee_spi_transfer(chipid_cdfinger_tx, chipid_rx, 4);
			if (chipid_rx[3] == 0x35) {
				CDFINGER_PROBE("ret = %d ,cdfinger chipid_rx : %x  %x  %x  %x \n",ret,chipid_rx[0],chipid_rx[1],chipid_rx[2],chipid_rx[3]);
				CDFINGER_PROBE("get id = (0x%x)\n",chipid_rx[3]);
				CDFINGER_PROBE("get cdfinger id success\n");
				cdfinger_fp_exist = 1;
				break;
			} else {
				ret = tee_spi_transfer(chipid_chipone_tx, chipid_rx, 4);
				CDFINGER_PROBE("ret = %d ,chipone chipid_rx : %x  %x  %x  %x \n",ret,chipid_rx[0],chipid_rx[1],chipid_rx[2],chipid_rx[3]);
				hwid = (chipid_rx[1] << 8) | (chipid_rx[2]);
				if ((hwid == 0x7332) || (hwid == 0x7153) || (hwid == 0x7230) || (hwid == 0x7222) || (hwid == 0x7312)) {
					CDFINGER_PROBE("get chipone_fp id success cnt = %d \n", TestCnt);
					goto free_cdfinger;
				}
				if(TestCnt == 4){
					CDFINGER_ERR("get cdfinger id failed \n");
					goto free_cdfinger;
				}
			}
		} else {
			CDFINGER_ERR("tee_spi_transfer invoke err()\n");
			ret = -1;
			if(TestCnt == 0){
				goto free_cdfinger;
			}
		}
	}
#endif
	mutex_init(&cdfinger->buf_lock);
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&cdfinger->cdfinger_lock, "cdfinger wakelock");
#else
	wake_lock_init(&cdfinger->cdfinger_lock, WAKE_LOCK_SUSPEND, "cdfinger wakelock");
#endif
	spi_set_drvdata(spi, cdfinger);
	
	status = misc_register(&cdfinger_dev);
	if (status < 0) {
		CDFINGER_ERR("%s: cdev register failed!\n", __func__);
		goto free_lock;
	}

	if(cdfinger_create_inputdev(cdfinger) < 0)
	{
		CDFINGER_ERR("%s: inputdev register failed!\n", __func__);
		goto free_device;
	}

	//init_timer(&cdfinger->int_timer);
	//cdfinger->int_timer.function = int_timer_handle;
	//add_timer(&cdfinger->int_timer);
	if(cdfinger_getirq_from_platform(cdfinger)!=0)
		goto free_work;
	status = request_threaded_irq(cdfinger->irq, (irq_handler_t)cdfinger_interrupt_handler, NULL,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, "cdfinger-irq", cdfinger);
	if(status){
		CDFINGER_ERR("request_irq error\n");
		goto free_work;
	}

	enable_irq_wake(cdfinger->irq);
	//cdfinger_enable_irq(cdfinger);
	cdfinger->irq_enabled = 1;
	
	cdfinger->cdfinger_thread = kthread_run(cdfinger_thread_func, cdfinger, "cdfinger_thread");
	if (IS_ERR(cdfinger->cdfinger_thread)) {
		CDFINGER_ERR("kthread_run is failed\n");
		goto free_irq;
	}
	cdfinger->notifier.notifier_call = cdfinger_fb_notifier_callback;
    fb_register_client(&cdfinger->notifier);
	
#if defined(CONFIG_MTK_HARDWAREINFO_SUPPORT)
    fingerprint_name_info = "fps998";
#endif

	CDFINGER_PROBE("exit\n");

	return 0;

free_irq:
	free_irq(cdfinger->irq, cdfinger);
free_work:
	//del_timer(&cdfinger->int_timer);
	input_unregister_device(cdfinger->cdfinger_inputdev);
	cdfinger->cdfinger_inputdev = NULL;
	input_free_device(cdfinger->cdfinger_inputdev);
free_device:
	misc_deregister(&cdfinger_dev);
free_lock:
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_trash(&cdfinger->cdfinger_lock);
#else
	wake_lock_destroy(&cdfinger->cdfinger_lock);
#endif
	mutex_destroy(&cdfinger->buf_lock);
free_cdfinger:
	
	kfree(cdfinger);
	cdfinger = NULL;

	return -1;
}



static int cdfinger_suspend (struct device *dev)
{
	return 0;
}

static int cdfinger_resume (struct device *dev)
{
	return 0;
}
static const struct dev_pm_ops cdfinger_pm = {
	.suspend = cdfinger_suspend,
	.resume = cdfinger_resume
};


#if defined(USE_PLATFORM_BUS)
struct of_device_id cdfinger_of_match[] = {
	{ .compatible = "mediatek,cdfinger",},
	{},
};

static struct platform_driver cdfinger_plat_driver = {
	.driver = {
		.name		= SPI_DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = cdfinger_of_match,
	},
    .probe =    cdfinger_probe,
    .remove =   NULL,
};


#elif defined(USE_SPI_BUS)
struct of_device_id cdfinger_of_match[] = {
	{ .compatible = "mediatek,mt6765-fingerprint",},
	{},
};
MODULE_DEVICE_TABLE(of, cdfinger_of_match);

static const struct spi_device_id cdfinger_id[] = {
	{SPI_DRV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(spi, cdfinger_id);

static struct spi_driver cdfinger_spi_driver = {
	.driver = {
		.name = SPI_DRV_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.pm = &cdfinger_pm,
		.of_match_table = of_match_ptr(cdfinger_of_match),
	},
	.id_table = cdfinger_id,
	.probe = cdfinger_probe,
	.remove = NULL,
};

#endif 

#ifndef DTS_PROBE 
static struct spi_board_info spi_board_cdfinger[] __initdata = {
	[0] = {
		.modalias = "cdfinger",
		.bus_num = 0,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.max_speed_hz = 6000000,
		.controller_data = &spi_conf,
	},
};
#endif


static int __init cdfinger_spi_init(void)
{
    int status = 0;
	CDFINGER_ERR("cdfinger_spi_init\n");
#ifndef DTS_PROBE 
	spi_register_board_info(spi_board_cdfinger, ARRAY_SIZE(spi_board_cdfinger));
#endif

#if defined(USE_PLATFORM_BUS)
	if(spi_fingerprint == NULL){
		CDFINGER_ERR("spi device is NULL, cannot spi transfer \n ");
		return -EINVAL;
   } else {
    	status = platform_driver_register(&cdfinger_plat_driver);
	}
#elif defined(USE_SPI_BUS)
    status = spi_register_driver(&cdfinger_spi_driver);
#endif

    if (status < 0) {
        CDFINGER_ERR("cdfinger_spi_init faile \n");
    }

    return status;
}

static void __exit cdfinger_spi_exit(void)
{
#if defined(USE_PLATFORM_BUS)
	platform_driver_unregister(&cdfinger_plat_driver);
#elif defined(USE_SPI_BUS)
	spi_unregister_driver(&cdfinger_spi_driver);
#endif
}

//late_initcall_sync(cdfinger_spi_init);
//late_initcall(cdfinger_spi_init);
module_init(cdfinger_spi_init);
module_exit(cdfinger_spi_exit);

MODULE_DESCRIPTION("cdfinger tee Driver");
MODULE_AUTHOR("shuaitao@cdfinger.com");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cdfinger");
