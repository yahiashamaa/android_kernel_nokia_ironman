/*
 * file abov_sar.c
 * brief abov Driver for two channel SAP using
 *
 * Driver for the ABOV
 * Copyright (c) 2015-2016 ABOV Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG
#define DRIVER_NAME "abov_sar"
#define USE_SENSORS_CLASS
//Bug615050 hehao.wt, MOD,20201231, recalibrate sar when USB is plugged in
#define SAR_USB_CHANGE
//bug524972,linliwei.wt,MODIFY,200103,in factoy app add sar info 
#define WT_ADD_SAR_HARDWARE_INFO 1

#define MAX_WRITE_ARRAY_SIZE 32
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/sensors.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/power_supply.h>
#if WT_ADD_SAR_HARDWARE_INFO
#include <linux/hardware_info.h>
#endif
#include <linux/input/abov_sar.h> /* main struct, interrupt,init,pointers */

#define BOOT_UPDATE_ABOV_FIRMWARE 1

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/async.h>
#include <linux/firmware.h>
#define SLEEP(x) mdelay(x)

#define C_I2C_FIFO_SIZE 8

#define I2C_MASK_FLAG (0x00ff)

static u8 checksum_h;
static u8 checksum_h_bin;
static u8 checksum_l;
static u8 checksum_l_bin;
static char _Buffer[128];

#define IDLE 0
#define ACTIVE 1
#define S_PROX   1
#define S_BODY   2

#define ABOV_DEBUG 1
#define LOG_TAG "ABOV"

//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
#define TEMPORARY_HOLD_TIME 500
struct wakeup_source wakesrc;
//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

#if ABOV_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define LOG_DBG(fmt, args...) pr_info(LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)   pr_err(LOG_TAG fmt, ##args)

static int last_val;
static int mEnabled;
static int programming_done;
struct abovXX * abov_sar_ptr;

static int abov_tk_fw_mode_enter(struct i2c_client *client);

/**
 * struct abov
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct abov {
 pbuttonInformation_t pbuttonInformation;
 struct abov_platform_data * hw; /* specific platform data settings */
} abov_t, *pabov_t;

//extern char Sar_name[HARDWARE_MAX_ITEM_LONGTH];

static void ForcetoTouched(struct abovXX * this)
{
 pabov_t pDevice = NULL;
 struct input_dev *input_top = NULL;
 struct input_dev *input_bottom = NULL;
 struct _buttonInfo *pCurrentButton  = NULL;
 
 if (this != NULL)
  pDevice = this->pDevice;
 if (pDevice != NULL) {
  LOG_INFO("ForcetoTouched()\n");

  pCurrentButton = pDevice->pbuttonInformation->buttons;
  input_top = pDevice->pbuttonInformation->input_top;
  input_bottom = pDevice->pbuttonInformation->input_bottom;
  pCurrentButton->state = ACTIVE;
  last_val = 1;
  if (mEnabled) {
   input_report_abs(input_top, ABS_DISTANCE, 1);
   input_sync(input_top);
   input_report_abs(input_bottom, ABS_DISTANCE, 1);
   input_sync(input_bottom);
  }
  LOG_INFO("Leaving ForcetoTouched()\n");
 }
}

/**
 * fn static int write_register(pabovXX_t this, u8 address, u8 value)
 * brief Sends a write register to the device
 * param this Pointer to main parent struct
 * param address 8-bit register address
 * param value   8-bit register value to write to address
 * return Value from i2c_master_send
 */
static int write_register(struct abovXX * this, u8 address, u8 value)
{
 struct i2c_client *i2c = 0;
 char buffer[2];
 int returnValue = 0;

 buffer[0] = address;
 buffer[1] = value;
 returnValue = -ENOMEM;
 if (this && this->bus) {
  i2c = this->bus;

  returnValue = i2c_master_send(i2c, buffer, 2);
  LOG_INFO("write_register Addr: 0x%x Val: 0x%x Return: %d\n",
    address, value, returnValue);
 }
 if (returnValue < 0) {
  ForcetoTouched(this);
  LOG_DBG("Write_register-ForcetoTouched()\n");
 }
 return returnValue;
}

/**
 * fn static int read_register(pabovXX_t this, u8 address, u8 *value)
 * brief Reads a register's value from the device
 * param this Pointer to main parent struct
 * param address 8-Bit address to read from
 * param value Pointer to 8-bit value to save register value to
 * return Value from i2c_smbus_read_byte_data if < 0. else 0
 */
static int read_register(struct abovXX * this, u8 address, u8 *value)
{
 struct i2c_client *i2c = 0;
 s32 returnValue = 0;

 if (this && value && this->bus) {
  i2c = this->bus;
  returnValue = i2c_smbus_read_byte_data(i2c, address);
  LOG_INFO("read_register Addr: 0x%x Return: 0x%x\n",
    address, returnValue);
  if (returnValue >= 0) {
   *value = returnValue;
   return 0;
  } else {
   return returnValue;
  }
 }
 ForcetoTouched(this);
 LOG_INFO("read_register-ForcetoTouched()\n");
 return -ENOMEM;
}

/**
 * detect if abov exist or not
 * return 1 if chip exist else return 0
 */
 #if 1
static int abov_detect(struct i2c_client *client)
{
 s32 returnValue = 0, i;
 u8 address = ABOV_ABOV_WHOAMI_REG;
 u8 value = 0xAB;

 if (client) {
  for (i = 0; i < 3; i++) {
   returnValue = i2c_smbus_read_byte_data(client, address);
   LOG_INFO("abov read_register for %d time Addr: 0x%x Return: 0x%x\n",
     i, address, returnValue);
   if (returnValue >= 0) {
    if (value == returnValue) {
     LOG_INFO("abov detect success!\n");
     return 1;
    }
   }
  }
 
  for (i = 0; i < 3; i++) {
    if(abov_tk_fw_mode_enter(client) == 0) {
     LOG_INFO("abov boot detect success!\n");
     return 2;
    }
  }
 }
 LOG_INFO("abov detect failed!!!\n");
 return 0;
}
#endif
/**
 * brief Perform a manual offset calibration
 * param this Pointer to main parent struct
 * return Value return value from the write register
 */
static int manual_offset_calibration(struct abovXX * this)
{
 s32 returnValue = 0;

 returnValue = write_register(this, ABOV_RECALI_REG, 0x01);
 return returnValue;
}

/**
 * brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t manual_offset_calibration_show(struct device *dev,
  struct device_attribute *attr, char *buf)
{
 u8 reg_value = 0;
 struct abovXX * this = dev_get_drvdata(dev);

 LOG_INFO("Reading IRQSTAT_REG\n");
 read_register(this, ABOV_IRQSTAT_REG, &reg_value);
 return scnprintf(buf, PAGE_SIZE, "%d\n", reg_value);
}

/* brief sysfs store function for manual calibration */
static ssize_t manual_offset_calibration_store(struct device *dev,
  struct device_attribute *attr,
  const char *buf, size_t count)
{
 struct abovXX * this = dev_get_drvdata(dev);
 unsigned long val;

 if (kstrtoul(buf, 0, &val))
  return -EINVAL;
 if (val) {
  LOG_INFO("Performing manual_offset_calibration()\n");
  manual_offset_calibration(this);
 }
 return count;
}

static DEVICE_ATTR(calibrate, 0644, manual_offset_calibration_show,
  manual_offset_calibration_store);
static struct attribute *abov_attributes[] = {
 &dev_attr_calibrate.attr,
 NULL,
};
static struct attribute_group abov_attr_group = {
 .attrs = abov_attributes,
};

/**
 * fn static int read_regStat(pabovXX_t this)
 * brief Shortcut to read what caused interrupt.
 * details This is to keep the drivers a unified
 * function that will read whatever register(s)
 * provide information on why the interrupt was caused.
 * param this Pointer to main parent struct
 * return If successful, Value of bit(s) that cause interrupt, else 0
 */
static int read_regStat(struct abovXX * this)
{
 u8 data = 0;

 if (this) {
  if (read_register(this, ABOV_IRQSTAT_REG, &data) == 0)
   return (data & 0x00FF);
 }
 return 0;
}

/**
 * brief  Initialize I2C config from platform data
 * param this Pointer to main parent struct
 *
 */
static void hw_init(struct abovXX * this)
{
 pabov_t pDevice = 0;
 struct abov_platform_data * pdata = 0;
 int i = 0;

 /* configure device */
 LOG_INFO("Going to Setup I2C Registers\n");
    if(this) {
     pDevice = this->pDevice;
     pdata = pDevice->hw;
    }
    if (this && pDevice && pdata) {
     for (i = 0;i < ARRAY_SIZE(abov_i2c_reg_setup);i++) {
      /* Write all registers/values contained in i2c_reg */
      LOG_INFO("Going to Write Reg: 0x%x Value: 0x%x\n",
        pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
      write_register(this, pdata->pi2c_reg[i].reg,
        pdata->pi2c_reg[i].val);
     }
    } else {
     LOG_DBG("ERROR! platform data error \n");
     /* Force to touched if error */
     ForcetoTouched(this);
     LOG_INFO("Hardware_init-ForcetoTouched()\n");
    }
}

/**
 * fn static int initialize(pabovXX_t this)
 * brief Performs all initialization needed to configure the device
 * param this Pointer to main parent struct
 * return Last used command's return value (negative if error)
 */
static int initialize(struct abovXX * this)
{
 int ret;

 if (this) {
  /* prepare reset by disabling any irq handling */
  /* perform a reset */

  ret = write_register(this, ABOV_SOFTRESET_REG,
    0x10);
  if (ret < 0)
   goto error_exit;
  /* wait until the reset has finished by monitoring NIRQ */
  LOG_INFO("Software Reset. Waiting device back to continue.\n");
  /* just sleep for awhile instead of using a loop with reading irq status */
  msleep(300);
  /*
   *    while(this->get_nirq_low && this->get_nirq_low()) { read_regStat(this); }
   */
  LOG_INFO("Device back from the reset, continuing. NIRQ = %d\n",
    this->get_nirq_low(this->board->irq_gpio));
  hw_init(this);
  msleep(100); /* make sure everything is running */
  ret = manual_offset_calibration(this);
  if (ret < 0)
   goto error_exit;
  /* re-enable interrupt handling */

  /* make sure no interrupts are pending since enabling irq will only
   * work on next falling edge */
  read_regStat(this);
  LOG_INFO("Exiting initialize(). NIRQ = %d\n",
    this->get_nirq_low(this->board->irq_gpio));
  programming_done = ACTIVE;
  return 0;
 }
 return -ENOMEM;

error_exit:
 programming_done = IDLE;
 return ret;
}

/**
 * brief Handle what to do when a touch occurs
 * param this Pointer to main parent struct
 */
static void touchProcess(struct abovXX * this)
{
 int counter = 0;
 u8 i = 0;
 int numberOfButtons = 0;
 pabov_t pDevice = NULL;
 struct _buttonInfo *buttons = NULL;
 struct input_dev *input_top = NULL;
 struct input_dev *input_bottom = NULL;
 struct _buttonInfo *pCurrentButton  = NULL;
 struct abov_platform_data *board;

    if(this) {
     pDevice = this->pDevice;
     board = this->board;
    }

 if (this && pDevice) {
  LOG_INFO("Inside touchProcess()\n");
  read_register(this, ABOV_IRQSTAT_REG, &i);

  buttons = pDevice->pbuttonInformation->buttons;
  input_top = pDevice->pbuttonInformation->input_top;
  input_bottom = pDevice->pbuttonInformation->input_bottom;
  numberOfButtons = pDevice->pbuttonInformation->buttonSize;

  if (unlikely((buttons == NULL) || (input_top == NULL) || (input_bottom == NULL))) {
   LOG_DBG("ERROR!! buttons or input NULL!!!\n");
   return;
  }

  for (counter = 0; counter < numberOfButtons; counter++) {
   pCurrentButton = &buttons[counter];
   if (pCurrentButton == NULL) {
    LOG_DBG("ERR!current button index: %d NULL!\n",
      counter);
    return; /* ERRORR!!!! */
   }
   switch (pCurrentButton->state) {
   case IDLE: /* Button is being in far state! */
    if ((i & pCurrentButton->mask) == pCurrentButton->mask) {
     LOG_INFO("CS %d State=BODY.\n",
       counter);
     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 2);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 2);
      input_sync(input_bottom);
     }
     pCurrentButton->state = S_BODY;
     last_val = 2;
    } else if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
     LOG_INFO("CS %d State=PROX.\n",
       counter);
     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 1);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 1);
      input_sync(input_bottom);
     }
     pCurrentButton->state = S_PROX;
     last_val = 0;
    } else {
     LOG_INFO("CS %d still in IDLE State.\n",counter);
     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 0);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 0);
      input_sync(input_bottom);
     }
    }
    break;
   case S_PROX: /* Button is being in proximity! */
    if ((i & pCurrentButton->mask) == pCurrentButton->mask) {
     LOG_INFO("CS %d State=BODY.\n",  
       counter);
     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 2);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 2);
      input_sync(input_bottom);
     }
     pCurrentButton->state = S_BODY;
     last_val = 2;
    } else if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
     LOG_INFO("CS %d still in PROX State.\n",counter);

     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 1);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 1);
      input_sync(input_bottom);
     }
    } else{
     LOG_INFO("CS %d State=IDLE.\n",
       counter);
     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 0);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 0);
      input_sync(input_bottom);
     }
     pCurrentButton->state = IDLE;
     last_val = 0;
    }
    break;
   case S_BODY: /* Button is being in 0mm! */
    if ((i & pCurrentButton->mask) == pCurrentButton->mask) {
     LOG_INFO("CS %d still in BODY State.\n",
       counter);
    } else if ((i & pCurrentButton->mask) == (pCurrentButton->mask & 0x05)) {
     LOG_INFO("CS %d State=PROX.\n",
       counter);
     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 1);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 1);
      input_sync(input_bottom);
     }
     pCurrentButton->state = S_PROX;
     last_val = 1;
    } else{
     LOG_INFO("CS %d State=IDLE.\n",
       counter);
     if (board->cap_channel_top == counter) {
      input_report_abs(input_top, ABS_DISTANCE, 0);
      input_sync(input_top);
     } else if (board->cap_channel_bottom == counter) {
      input_report_abs(input_bottom, ABS_DISTANCE, 0);
      input_sync(input_bottom);
     }
     pCurrentButton->state = IDLE;
     last_val = 0;
    }
    break;
   default: /* Shouldn't be here, device only allowed ACTIVE or IDLE */
    break;
   };
  }
  LOG_INFO("Leaving touchProcess()\n");
 }
}

static int abov_get_nirq_state(unsigned irq_gpio)
{
 if (irq_gpio) {
  return !gpio_get_value(irq_gpio);
 } else {
  LOG_INFO("abov irq_gpio is not set.");
  return -EINVAL;
 }
}

static struct _totalButtonInformation smtcButtonInformation = {
 .buttons = psmtcButtons,
 .buttonSize = ARRAY_SIZE(psmtcButtons),
};

/**
 *fn static void abov_reg_setup_init(struct i2c_client *client)
 *brief read reg val form dts
 *      reg_array_len for regs needed change num
 *      data_array_val's format <reg val ...>
 */
static void abov_reg_setup_init(struct i2c_client *client)
{
 u32 data_array_len = 0;
 u32 *data_array;
 int ret, i, j;
 struct device_node *np = client->dev.of_node;

 ret = of_property_read_u32(np, "reg_array_len", &data_array_len);
 if (ret < 0) {
  LOG_DBG("data_array_len read error");
  return;
 }
 data_array = kzalloc(data_array_len * 2 * sizeof(u32), GFP_KERNEL);
 if (!data_array)
 	return ;
 
 ret = of_property_read_u32_array(np, "reg_array_val",
   data_array,
   data_array_len*2);
 if (ret < 0) {
  LOG_DBG("data_array_val read error");
  goto error_exit;//Bug 471205,pengbin2_wt,MODIFY,20190822,RESOURCE_LEAK
 }
 for (i = 0; i < ARRAY_SIZE(abov_i2c_reg_setup); i++) {
  for (j = 0; j < data_array_len*2; j += 2) {
   if (data_array[j] == abov_i2c_reg_setup[i].reg) {
    abov_i2c_reg_setup[i].val = data_array[j+1];
    LOG_DBG("read dtsi 0x%02x:0x%02x set reg\n",
     data_array[j], data_array[j+1]);
   }
  }
 }
 error_exit://Bug 471205,pengbin2_wt,ADD,20190822,RESOURCE_LEAK
 kfree(data_array);
}

static void abov_platform_data_of_init(struct i2c_client *client,
  struct abov_platform_data * pplatData)
{
 struct device_node *np = client->dev.of_node;
 u32 cap_channel_top, cap_channel_bottom;
 int ret;
 //static const char *sar_info;

 client->irq = of_get_named_gpio_flags(np, "abov,irq-gpio",
                      0, &pplatData->irq_gpio_flags);

 pplatData->irq_gpio = client->irq;

 ret = of_property_read_u32(np, "cap,use_channel_top", &cap_channel_top);

 ret = of_property_read_u32(np, "cap,use_channel_bottom", &cap_channel_bottom);

 //sar_info = of_get_property(np, "label", NULL);
 //strlcpy(Sar_name, "A96T316EW", HARDWARE_MAX_ITEM_LONGTH);

 pplatData->cap_channel_top = (int)cap_channel_top;
 pplatData->cap_channel_bottom = (int)cap_channel_bottom;

 pplatData->get_is_nirq_low = abov_get_nirq_state;
 pplatData->init_platform_hw = NULL;
 /*  pointer to an exit function. Here in case needed in the future */
 /*
  *.exit_platform_hw = abov_exit_ts,
  */
 pplatData->exit_platform_hw = NULL;
 abov_reg_setup_init(client);
 pplatData->pi2c_reg = abov_i2c_reg_setup;
 pplatData->i2c_reg_num = ARRAY_SIZE(abov_i2c_reg_setup);

 pplatData->pbuttonInformation = &smtcButtonInformation;

 ret = of_property_read_string(np, "label", &pplatData->fw_name);
 if (ret < 0) {
  LOG_DBG("firmware name read error!\n");
  return;
 }
}

static ssize_t capsense_reset_show(struct class *class,
  struct class_attribute *attr,
  char *buf)
{
 return snprintf(buf, 8, "%d\n", programming_done);
}

static ssize_t capsense_reset_store(struct class *class,
  struct class_attribute *attr,
  const char *buf, size_t count)
{
 struct abovXX * this = abov_sar_ptr;
 pabov_t pDevice = NULL;
 struct input_dev *input_top = NULL;
 struct input_dev *input_bottom = NULL;

    if (!count || (this == NULL))
     return -EINVAL;

    if(this) {
     pDevice = this->pDevice;
    
     input_top = pDevice->pbuttonInformation->input_top;
     input_bottom = pDevice->pbuttonInformation->input_bottom;

     if (!strncmp(buf, "reset", 5) || !strncmp(buf, "1", 1))
      write_register(this, ABOV_SOFTRESET_REG, 0x10);

     input_report_abs(input_top, ABS_DISTANCE, 0);
     input_sync(input_top);
     input_report_abs(input_bottom, ABS_DISTANCE, 0);
     input_sync(input_bottom);
    }

 return count;
}

static CLASS_ATTR_RW(capsense_reset);

static ssize_t capsense_enable_show(struct class *class,
  struct class_attribute *attr,
  char *buf)
{
 return snprintf(buf, 8, "%d\n", mEnabled);
}

static ssize_t capsense_enable_store(struct class *class,
  struct class_attribute *attr,
  const char *buf, size_t count)
{
 struct abovXX * this = abov_sar_ptr;
 pabov_t pDevice = NULL;
 struct input_dev *input_top = NULL;
 struct input_dev *input_bottom = NULL;
    char tmp[5] = {"\0"};

 if (!count || (this == NULL))
  return -EINVAL;
 pDevice = this->pDevice;
 input_top = pDevice->pbuttonInformation->input_top;
 input_bottom = pDevice->pbuttonInformation->input_bottom;

 if(copy_from_user(tmp,buf,4)){
   return -1;
  }

 if (!strncmp(tmp, "1", 1)) {
  LOG_DBG("enable cap sensor\n");
  initialize(this);

        //write_register(this, ABOV_CTRL_MODE_RET, 0x02);
  //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
  if(this->irq_disabled == 1){
   enable_irq(this->irq);
   this->irq_disabled = 0;
   LOG_DBG("abov enable_irq\n");
  }
  //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
  input_report_abs(input_top, ABS_DISTANCE, 0);
  input_sync(input_top);
  input_report_abs(input_bottom, ABS_DISTANCE, 0);
  input_sync(input_bottom);
  mEnabled = 1;
 } else if (!strncmp(tmp, "0", 1)) {
  LOG_DBG("disable cap sensor\n");
  //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
  if(this->irq_disabled == 0){
   disable_irq(this->irq);
   this->irq_disabled = 1;
   LOG_DBG("abov disable_irq\n");
  }

  //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
  write_register(this, ABOV_CTRL_MODE_RET, 0x02);
  input_report_abs(input_top, ABS_DISTANCE, -1);
  input_sync(input_top);
  input_report_abs(input_bottom, ABS_DISTANCE, -1);
  input_sync(input_bottom);
  mEnabled = 0;
 } else {
  LOG_DBG("unknown enable symbol\n");
 }

 return count;
}

#ifdef USE_SENSORS_CLASS
static int capsensor_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
 struct abovXX * this = abov_sar_ptr;
 pabov_t pDevice = NULL;
 struct input_dev *input_top = NULL;
 struct input_dev *input_bottom = NULL;
    //char tmp[5] = {"\0"};

 pDevice = this->pDevice;
 input_top = pDevice->pbuttonInformation->input_top;
 input_bottom = pDevice->pbuttonInformation->input_bottom;


 if (enable == 1) {
  LOG_DBG("enable cap sensor\n");

  //initialize(this);
        write_register(this, ABOV_CTRL_MODE_RET, 0x00);
        manual_offset_calibration(this);
        msleep(500);
  //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
  if(this->irq_disabled == 1){
   enable_irq(this->irq);
   this->irq_disabled = 0;
   LOG_DBG("abov enable_irq\n");
  }
  //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
#ifdef FACTORY_VERSION
  input_report_abs(input_top, ABS_DISTANCE, 0);
  input_sync(input_top);
  input_report_abs(input_bottom, ABS_DISTANCE, 0);
  input_sync(input_bottom);
#else
  touchProcess(this);
#endif
  mEnabled = 1;
 } else if (enable == 0) {
  LOG_DBG("disable cap sensor\n");
  //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
  if(this->irq_disabled == 0){
   disable_irq(this->irq);
   this->irq_disabled = 1;
   LOG_DBG("abov disable_irq\n");
  }
  //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
#ifdef FACTORY_VERSION
  write_register(this, ABOV_CTRL_MODE_RET, 0x02);
#else
  write_register(this, ABOV_CTRL_MODE_RET, 0x01);
#endif
  input_report_abs(input_top, ABS_DISTANCE, -1);
  input_sync(input_top);
  input_report_abs(input_bottom, ABS_DISTANCE, -1);
  input_sync(input_bottom);
  mEnabled = 0;
 } else {
  LOG_DBG("unknown enable symbol\n");
 }

 return 0;
}
#endif

static CLASS_ATTR_RW(capsense_enable);

static ssize_t reg_dump_show(struct class *class,
  struct class_attribute *attr,
  char *buf)
{
 u8 reg_value = 0, i;
 u16 offset_value = 0, diff_value = 0;
 struct abovXX * this = abov_sar_ptr;
 char *p = buf;

 if (this->read_flag) {
  this->read_flag = 0;
  read_register(this, this->read_reg, &reg_value);
  p += snprintf(p, PAGE_SIZE, "%02x\n", reg_value);
  return (p-buf);
 }

 for (i = 0; i < 0x26; i++) {
  read_register(this, i, &reg_value);
  p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
    i, reg_value);
 }

 for (i = 0x80; i < 0x8C; i++) {
  read_register(this, i, &reg_value);
  p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n",
    i, reg_value);
 }
 if (this->get_ch0_flag) {
  /* diff */
  read_register(this, 0x1C, &reg_value);
  diff_value = reg_value;
  diff_value <<= 8;
  read_register(this, 0x1D, &reg_value);
  diff_value += reg_value;

  /* offset */
  read_register(this, 0x20, &reg_value);
  offset_value = reg_value;
  offset_value <<= 8;
  read_register(this, 0x21, &reg_value);
  offset_value += reg_value;
 } else {

  /* diff */
  read_register(this, 0x1E, &reg_value);
  diff_value = reg_value;
  diff_value <<= 8;
  read_register(this, 0x1F, &reg_value);
  diff_value += reg_value;

  /* offset */
  read_register(this, 0x22, &reg_value);
  offset_value = reg_value;
  offset_value <<= 8;
  read_register(this, 0x23, &reg_value);
  offset_value += reg_value;
 }

 reg_value = gpio_get_value(this->board->irq_gpio);
 p += snprintf(p, PAGE_SIZE, "NIRQ=%d\n", reg_value);

 p += snprintf(p, PAGE_SIZE, "diff_value=0x%04x\n",
   diff_value);
 p += snprintf(p, PAGE_SIZE, "offset_value=0x%04x\n",
   offset_value);

 return (p-buf);
}

static ssize_t reg_dump_store(struct class *class,
  struct class_attribute *attr,
  const char *buf, size_t count)
{
 struct abovXX * this = abov_sar_ptr;
 unsigned int val, reg, opt;
 if (strcmp("select_ch0\n", buf) == 0) {
  this->get_ch0_flag = 1;
 } else if (strcmp("select_ch1\n", buf) == 0) {
  this->get_ch0_flag = 0;
 } else if (strcmp("select_ch2\n", buf) == 0) {
  this->get_ch0_flag = 0;
 } else if (strcmp("calibrate\n", buf) == 0) {
  write_register(this, ABOV_RECALI_REG, 0x01);
 } else if (sscanf(buf, "%x,%x,%x", &reg, &val, &opt) == 3) {
  LOG_INFO("%s, read reg = 0x%02x\n", __func__, *(u8 *)&reg);
  this->read_reg = *((u8 *)&reg);
  this->read_flag = 1;
 } else if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
  LOG_INFO("%s,reg = 0x%02x, val = 0x%02x\n",
    __func__, *(u8 *)&reg, *(u8 *)&val);
  write_register(this, *((u8 *)&reg), *((u8 *)&val));
 }

 return count;
}

static CLASS_ATTR_RW(reg_dump);

static struct class capsense_class = {
 .name   = "capsense",
 .owner   = THIS_MODULE,
};

#ifdef SAR_USB_CHANGE
static void ps_notify_callback_work(struct work_struct *work)
{
 struct abovXX * this = container_of(work, abovXX_t, ps_notify_work);
 pabov_t pDevice = NULL;
 struct input_dev *input_top = NULL;
 struct input_dev *input_bottom = NULL;
 int ret = 0;

 pDevice = this->pDevice;
 input_top = pDevice->pbuttonInformation->input_top;
 input_bottom = pDevice->pbuttonInformation->input_bottom;

 LOG_INFO("Usb insert,going to force calibrate\n");
 ret = write_register(this, ABOV_RECALI_REG, 0x01);
 if (ret < 0)
  LOG_DBG(" Usb insert,calibrate cap sensor failed\n");

 input_report_abs(input_top, ABS_DISTANCE, 0);
 input_sync(input_top);
 input_report_abs(input_bottom, ABS_DISTANCE, 0);
 input_sync(input_bottom);
}

//+Bug615050 hehao.wt, MOD,20201231, recalibrate sar when USB is plugged in
static int ps_get_state(struct power_supply *psy, bool *present)
{
 union power_supply_propval pval = { 0 };
 int retval;

 retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
   &pval);
 if (retval) {
  LOG_DBG("%s psy get property failed\n", psy->desc->name);
  return retval;
 }
 *present = (pval.intval) ? true : false;
 LOG_INFO("%s is %s\n", psy->desc->name,
   (*present) ? "present" : "not present");
 return 0;
}

static int ps_notify_callback(struct notifier_block *self,
  unsigned long event, void *p)
{
 struct abovXX * this = container_of(self, abovXX_t, ps_notif);
 struct power_supply *psy = p;
 bool present;
 int retval;

 if ((event == PSY_EVENT_PROP_CHANGED)
   && psy && psy->desc->name &&
   !strncmp(psy->desc->name, "mtk_charger_type", sizeof("mtk_charger_type"))) {
  LOG_INFO("ps notification: event = %lu\n", event);
  retval = ps_get_state(psy, &present);
  if (retval) {
   LOG_DBG("psy get property failed\n");
   return retval;
  }

  if (event == PSY_EVENT_PROP_CHANGED) {
   if (this->ps_is_present == present) {
    LOG_INFO("ps present state not change\n");
    return 0;
   }
  }
  this->ps_is_present = present;
  schedule_work(&this->ps_notify_work);
 }

 return 0;
}
//+Bug615050 hehao.wt, MOD,20201231, recalibrate sar when USB is plugged in
#endif
static char *toString(u8 *buffer, int len)
{
 int i = 0;
 int h = 0;
 int l = 0;

 if (buffer == 0 || len < 1)
  _Buffer[0] = 0x00;
 else
  for (i = 0; i < len && i < 40; i++) {
   h = (buffer[i] & 0xf0) >> 4;
   l = buffer[i] & 0x0f;
   _Buffer[i * 3 + 0] = h < 10 ? '0' + h : 'A' + (h - 10);
   _Buffer[i * 3 + 1] = l < 10 ? '0' + l : 'A' + (l - 10);
   _Buffer[i * 3 + 2] = ' ';
   _Buffer[i * 3 + 3] = 0x00;
  }

 return _Buffer;
}

static int _i2c_adapter_block_write(struct i2c_client *client, u8 *data, u8 len, int outData)
{
 u8 buffer[C_I2C_FIFO_SIZE];
 u8 left = len;
 u8 offset = 0;
 u8 retry = 0;

 struct i2c_msg msg = {
  .addr = client->addr & I2C_MASK_FLAG,
  .flags = 0,
  .buf = buffer,
 };

 if (data == NULL || len < 1) {
  LOG_ERR("Invalid : data is null or len=%d\n", len);
  return -EINVAL;
 }

 while (left > 0) {
  retry = 0;
  if (left >= C_I2C_FIFO_SIZE) {
   msg.buf = &data[offset];
   msg.len = C_I2C_FIFO_SIZE;
   left -= C_I2C_FIFO_SIZE;
   offset += C_I2C_FIFO_SIZE;
  } else {
   msg.buf = &data[offset];
   msg.len = left;
   left = 0;
  }

  while (i2c_transfer(client->adapter, &msg, 1) != 1) {
   retry++;
   if (retry > 10) {
    if (outData)
     LOG_ERR("OUT : fail - addr:%#x len:%d data:%s\n", client->addr, msg.len, toString(msg.buf, msg.len));
    else
     LOG_ERR("OUT : fail - addr:%#x len:%d \n", client->addr, msg.len);
    return -EIO;
   }
  }
 }
 return 0;
}

static int i2c_adapter_block_write_nodatalog(struct i2c_client *client, u8 *data, u8 len)
{
 return _i2c_adapter_block_write(client, data, len, 0);
}

static int abov_tk_check_busy(struct i2c_client *client)
{
 int ret, count = 0;
 unsigned char val = 0x00;

 do {
  ret = i2c_master_recv(client, &val, sizeof(val));
  if (val & 0x01) {
   count++;
   if (count > 1000) {
    LOG_INFO("%s: val = 0x%x\r\n", __func__, val);
    return ret;
   }
  } else {
   break;
  }
 } while (1);

 return ret;
}

static int abov_tk_fw_write(struct i2c_client *client, unsigned char *addrH,
      unsigned char *addrL, unsigned char *val)
{
 int length = 36, ret = 0;
 unsigned char buf[40] = {0, };

 buf[0] = 0xAC;
 buf[1] = 0x7A;
 memcpy(&buf[2], addrH, 1);
 memcpy(&buf[3], addrL, 1);
 memcpy(&buf[4], val, 32);
 ret = i2c_adapter_block_write_nodatalog(client, buf, length);
 if (ret < 0) {
  LOG_ERR("Firmware write fail ...\n");
  return ret;
 }

 SLEEP(3);
 abov_tk_check_busy(client);

 return 0;
}

static int abov_tk_reset_for_bootmode(struct i2c_client *client)
{
 int ret, retry_count = 10;
 unsigned char buf[16] = {0, };

retry:
 buf[0] = 0xF0;
 buf[1] = 0xAA;
 ret = i2c_master_send(client, buf, 2);
 if (ret < 0) {
  LOG_INFO("write fail(retry:%d)\n", retry_count);
  if (retry_count-- > 0) {
   goto retry;
  }
  return -EIO;
 } else {
  LOG_INFO("success reset & boot mode\n");
  return 0;
 }
}

static int i2c_adapter_read_raw(struct i2c_client *client, u8 *data, u8 len)
{
 struct i2c_msg msg;
 int ret;
 int retry = 3;

 msg.addr = client->addr;
 msg.flags = 1;
 msg.len = len;
 msg.buf = data;
 while (retry--) {
  ret = i2c_transfer(client->adapter, &msg, 1);
  if (ret == 1) {
   break;
  }

  if (ret < 0) {
   LOG_ERR("%s fail(data read)(%d)\n", __func__, retry);
   SLEEP(10);
  }
 }

 if (retry) {
  LOG_INFO("TRAN : succ - addr:%#x ... len:%d data:%s\n", client->addr, msg.len, toString(msg.buf, msg.len));
  return 0;
 } else {
  LOG_INFO("TRAN : fail - addr:%#x len:%d \n", client->addr, msg.len);
  return -EIO;
 }
}

static int abov_tk_fw_mode_enter(struct i2c_client *client)
{
 int ret = 0;
 unsigned char buf[40] = {0, };

 buf[0] = 0xAC;
 buf[1] = 0x5B;
 ret = i2c_master_send(client, buf, 2);
 if (ret != 2) {
  LOG_ERR("SEND : fail - addr:%#x data:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
  return -EIO;
 }
 LOG_INFO("SEND : succ - addr:%#x data:%#x %#x... ret:%d\n", client->addr, buf[0], buf[1], ret);
 SLEEP(5);

 ret = i2c_adapter_read_raw(client, buf, 1);
 if (ret < 0) {
  LOG_ERR("Enter fw mode fail ...\n");
  return -EIO;
 }

 LOG_INFO("succ ... data:%#x\n", buf[0]);

 return 0;
}

static int abov_tk_fw_mode_exit(struct i2c_client *client)
{
 int ret = 0;
 unsigned char buf[40] = {0, };

 buf[0] = 0xAC;
 buf[1] = 0xE1;
 ret = i2c_master_send(client, buf, 2);
 if (ret != 2) {
  LOG_ERR("SEND : fail - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
  return -EIO;
 }
 LOG_INFO("SEND : succ - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

 return 0;
}

static int abov_tk_flash_erase(struct i2c_client *client)
{
 int ret = 0;
 unsigned char buf[16] = {0, };

 buf[0] = 0xAC;
#ifdef ABOV_POWER_CONFIG
 buf[1] = 0x2D;
#else
 buf[1] = 0x2E;
#endif

 ret = i2c_master_send(client, buf, 2);
 if (ret != 2) {
  LOG_ERR("SEND : fail - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);
  return -EIO;
 }

 LOG_INFO("SEND : succ - addr:%#x data:%#x %#x ... ret:%d\n", client->addr, buf[0], buf[1], ret);

 return 0;
}

static int abov_tk_i2c_read_checksum(struct i2c_client *client)
{
 unsigned char checksum[6] = {0, };
 unsigned char buf[16] = {0, };
 int ret;

 checksum_h = 0;
 checksum_l = 0;

#ifdef ABOV_POWER_CONFIG
 buf[0] = 0xAC;
 buf[1] = 0x9E;
 buf[2] = 0x10;
 buf[3] = 0x00;
 buf[4] = 0x3F;
 buf[5] = 0xFF;
 ret = i2c_master_send(client, buf, 6);
#else
 buf[0] = 0xAC;
 buf[1] = 0x9E;
 buf[2] = 0x00;
 buf[3] = 0x00;
 buf[4] = checksum_h_bin;
 buf[5] = checksum_l_bin;
 ret = i2c_master_send(client, buf, 6);
#endif
 if (ret != 6) {
  LOG_ERR("SEND : fail - addr:%#x len:%d data:%s ... ret:%d\n", client->addr, 6, toString(buf, 6), ret);
  return -EIO;
 }
 SLEEP(5);

 buf[0] = 0x00;
 ret = i2c_master_send(client, buf, 1);
 if (ret != 1) {
  LOG_ERR("SEND : fail - addr:%#x data:%#x ... ret:%d\n", client->addr, buf[0], ret);
  return -EIO;
 }
 SLEEP(5);

 ret = i2c_adapter_read_raw(client, checksum, 6);
 if (ret < 0) {
  LOG_ERR("Read raw fail ... \n");
  return -EIO;
 }

 checksum_h = checksum[4];
 checksum_l = checksum[5];

 return 0;
}

static int _abov_fw_update(struct i2c_client *client, const u8 *image, u32 size)
{
 int ret, ii = 0;
 int count;
 unsigned short address;
 unsigned char addrH, addrL;
 unsigned char data[32] = {0, };

 LOG_INFO("%s: call in\r\n", __func__);

 if (abov_tk_reset_for_bootmode(client) < 0) {
  LOG_ERR("don't reset(enter boot mode)!");
  return -EIO;
 }

 SLEEP(45);

 for (ii = 0; ii < 10; ii++) {
  if (abov_tk_fw_mode_enter(client) < 0) {
   LOG_ERR("don't enter the download mode! %d", ii);
   SLEEP(40);
   continue;
  }
  break;
 }

 if (10 <= ii) {
  return -EAGAIN;
 }

 if (abov_tk_flash_erase(client) < 0) {
  LOG_ERR("don't erase flash data!");
  return -EIO;
 }

 SLEEP(1400);

 address = 0x800;
 count = size / 32;

 for (ii = 0; ii < count; ii++) {
  /* first 32byte is header */
  addrH = (unsigned char)((address >> 8) & 0xFF);
  addrL = (unsigned char)(address & 0xFF);
  memcpy(data, &image[ii * 32], 32);
  ret = abov_tk_fw_write(client, &addrH, &addrL, data);
  if (ret < 0) {
   LOG_INFO("fw_write.. ii = 0x%x err\r\n", ii);
   return ret;
  }

  address += 0x20;
  memset(data, 0, 32);
 }

 ret = abov_tk_i2c_read_checksum(client);
 ret = abov_tk_fw_mode_exit(client);
 if ((checksum_h == checksum_h_bin) && (checksum_l == checksum_l_bin)) {
  LOG_INFO("checksum successful. checksum_h:%x=%x && checksum_l:%x=%x\n",
   checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
 } else {
  LOG_INFO("checksum error. checksum_h:%x=%x && checksum_l:%x=%x\n",
   checksum_h, checksum_h_bin, checksum_l, checksum_l_bin);
  ret = -1;
 }
 SLEEP(100);

 return ret;
}

static int abov_fw_update(bool force)
{
 int update_loop;
 struct abovXX * this = abov_sar_ptr;
 struct i2c_client *client = this->bus;
 int rc;
 bool fw_upgrade = false;
 u8 fw_version = 0, fw_file_version = 0;
 u8 fw_modelno = 0, fw_file_modeno = 0;
 const struct firmware *fw = NULL;
 char fw_name[32] = {0};

 strlcpy(fw_name, this->board->fw_name, sizeof(fw_name));//Bug 471236,pengbin2_wt,MODIFY,20190822,overrun
 strlcat(fw_name, ".BIN", sizeof(fw_name));//Bug 471236,pengbin2_wt,MODIFY,20190822,overrun
 rc = request_firmware(&fw, fw_name, this->pdev);
 if (rc < 0) {
  LOG_INFO("Request firmware failed - %s (%d)\n",
    this->board->fw_name, rc);
  return rc;
 }
 read_register(this, ABOV_VERSION_REG, &fw_version);
 read_register(this, ABOV_MODELNO_REG, &fw_modelno);

 fw_file_modeno = fw->data[1];
 fw_file_version = fw->data[5];
 checksum_h_bin = fw->data[8];
 checksum_l_bin = fw->data[9];

 if ((force) || (fw_version < fw_file_version) || (fw_modelno != fw_file_modeno))
  fw_upgrade = true;
 else {
  LOG_INFO("Exiting fw upgrade...\n");
  fw_upgrade = false;
  rc = -EIO;
  goto rel_fw;
 }

 if (fw_upgrade) {
  for (update_loop = 0; update_loop < 10; update_loop++) {
   rc = _abov_fw_update(client, &fw->data[32], fw->size-32);
   if (rc < 0)
    LOG_INFO("retry : %d times!\n", update_loop);
   /*+Bug 471233,pengbin2_wt,MODIFY,20190822,DEADCODE*/
   else{
    initialize(this);
    break;
    }

  }
  if (update_loop >= 10)
   rc = -EIO;
 }

rel_fw:
 release_firmware(fw);
 return rc;
}

static ssize_t capsense_fw_ver_show(struct class *class,
  struct class_attribute *attr,
  char *buf)
{
 u8 fw_version = 0;
 struct abovXX * this = abov_sar_ptr;

 read_register(this, ABOV_VERSION_REG, &fw_version);
 return snprintf(buf, 16, "0x%x\n", fw_version);
}

static ssize_t capsense_fw_ver_store(struct class *class,
  struct class_attribute *attr,
  const char *buf, size_t count)
{
 struct abovXX * this = abov_sar_ptr;
 unsigned long val;
 bool irq_status_keeper = 0;
 int rc;

 if (count > 2)
  return -EINVAL;

 rc = kstrtoul(buf, 10, &val);
 if (rc != 0)
  return rc;

 if(this->irq_disabled == 0){
  irq_status_keeper = 1;
  this->irq_disabled = 1;
  disable_irq(this->irq);
 }

 mutex_lock(&this->mutex);
 if (!this->loading_fw  && val) {
  this->loading_fw = true;
  abov_fw_update(false);
  this->loading_fw = false;
 }
 mutex_unlock(&this->mutex);

 if(irq_status_keeper == 1){
  enable_irq(this->irq);
  this->irq_disabled = 0;
 }

 return count;
}
static CLASS_ATTR_RW(capsense_fw_ver);

static ssize_t capsense_force_update_fw_show(struct class *class,
  struct class_attribute *attr,
  char *buf)
{
 u8 fw_version = 0;
 struct abovXX * this = abov_sar_ptr;

 read_register(this, ABOV_VERSION_REG, &fw_version);
 return snprintf(buf, 16, "0x%x\n", fw_version);
}

static ssize_t capsense_force_update_fw_store(struct class *class,
  struct class_attribute *attr,
  const char *buf, size_t count)
{
 struct abovXX * this = abov_sar_ptr;
 unsigned long val;
 bool irq_status_keeper = 0;
 int rc;

 if (count > 2)
  return -EINVAL;

 rc = kstrtoul(buf, 10, &val);
 if (rc != 0)
  return rc;

 if(this->irq_disabled == 0){
  irq_status_keeper = 1;
  this->irq_disabled = 1;
  disable_irq(this->irq);
 }

 mutex_lock(&this->mutex);
 if (!this->loading_fw  && val) {
  this->loading_fw = true;
  abov_fw_update(true);
  this->loading_fw = false;
 }
 mutex_unlock(&this->mutex);

 if(irq_status_keeper == 1){
  enable_irq(this->irq);
  this->irq_disabled = 0;
 }

 return count;
}
static CLASS_ATTR_RW(capsense_force_update_fw);

static void capsense_update_work(struct work_struct *work)
{
 struct abovXX * this = container_of(work, abovXX_t, fw_update_work);

 LOG_INFO("%s: start update firmware\n", __func__);
 mutex_lock(&this->mutex);
 this->loading_fw = true;
 abov_fw_update(false);
 this->loading_fw = false;
 mutex_unlock(&this->mutex);
 LOG_INFO("%s: update firmware end\n", __func__);
}

static void capsense_fore_update_work(struct work_struct *work)
{
 pabovXX_t this = container_of(work, abovXX_t, fw_update_work);

 LOG_INFO("%s: start force update firmware\n", __func__);
 mutex_lock(&this->mutex);
 this->loading_fw = true;
 abov_fw_update(true);
 this->loading_fw = false;
 mutex_unlock(&this->mutex);
 LOG_INFO("%s: force update firmware end\n", __func__);
}


/**
 * fn static int abov_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * brief Probe function
 * param client pointer to i2c_client
 * param id pointer to i2c_device_id
 * return Whether probe was successful
 */
static int abov_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
 struct abovXX * this = NULL;
 pabov_t pDevice = 0;
 struct abov_platform_data * pplatData = NULL;
 bool isForceUpdate = false;
 int ret=0;
 //int err =0;
 struct input_dev *input_top = NULL;
 struct input_dev *input_bottom = NULL;
#ifdef SAR_USB_CHANGE
 struct power_supply *psy = NULL;
#endif
#if WT_ADD_SAR_HARDWARE_INFO
    u8 fw_version = 0;
    char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
#endif
 printk("abov_probe()\n");
/*+Bug 440975 lishuai_wt,20190501,add,open sar sensor for sda450*/

 ret = abov_detect(client);//check device
 if (ret == 0) {
  return -ENODEV;
 }
 if (ret == 2) {
  isForceUpdate = true;
 }
 pplatData = kzalloc(sizeof(struct abov_platform_data), GFP_KERNEL);
 if (!pplatData) {
  LOG_DBG("platform data is required!\n");
  return -EINVAL;
 }
 abov_platform_data_of_init(client, pplatData);
 client->dev.platform_data = pplatData;

 pplatData->cap_vdd = regulator_get(&client->dev, "cap_vdd");
 if (IS_ERR(pplatData->cap_vdd)) {
  if (PTR_ERR(pplatData->cap_vdd) == -EPROBE_DEFER) {
   ret = PTR_ERR(pplatData->cap_vdd);
   goto err_vdd_defer;
  }
  LOG_INFO("%s: Failed to get regulator\n",
    __func__);
 } else {
  int error = regulator_enable(pplatData->cap_vdd);
  if (error) {
   regulator_put(pplatData->cap_vdd);
   LOG_DBG("%s: Error %d enable regulator\n",
     __func__, error);
   goto err_vdd_defer;
  }
  pplatData->cap_vdd_en = true;
  LOG_INFO("cap_vdd regulator is %s\n",
    regulator_is_enabled(pplatData->cap_vdd) ?
    "on" : "off");
 }
/*
 pplatData->cap_svdd = regulator_get(&client->dev, "cap_svdd");
 if (!IS_ERR(pplatData->cap_svdd)) {
  ret = regulator_enable(pplatData->cap_svdd);
  if (ret) {
   regulator_put(pplatData->cap_svdd);
   LOG_INFO("Failed to enable cap_svdd\n");
   goto err_svdd_error;
  }
  pplatData->cap_svdd_en = true;
  LOG_INFO("cap_svdd regulator is %s\n",
    regulator_is_enabled(pplatData->cap_svdd) ?
    "on" : "off");
 } else {
  ret = PTR_ERR(pplatData->cap_vdd);
  if (ret == -EPROBE_DEFER)
   goto err_svdd_error;
 }
 SLEEP(100);
*/
 if (!i2c_check_functionality(client->adapter,
    I2C_FUNC_SMBUS_READ_WORD_DATA))
  //goto err_platform_init;
 /* detect if abov exist or not */
 if (abov_detect(client) == 0) {
  //goto err_platform_init;
 }
 /*-Bug 440975 lishuai_wt,20190501,add,open sar sensor for sda450*/

    this = kzalloc(sizeof(abovXX_t), GFP_KERNEL);
 /*this = kzalloc(sizeof(struct abovXX), GFP_KERNEL); create memory for main struct */
 if (!this) {
  LOG_DBG("failed to alloc for main struct\n");
  ret = -EINVAL;
  goto err_platform_init;
 }
 //LOG_INFO("\t Initialized Main Memory: 0x%p\n", this);

 //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
 this->sar_pinctl = devm_pinctrl_get(&client->dev);
 if (IS_ERR_OR_NULL(this->sar_pinctl)) {
  ret = PTR_ERR(this->sar_pinctl);
  printk("abov Target does not use pinctrl %d\n", ret);
 }

 this->pinctrl_state_default
  = pinctrl_lookup_state(this->sar_pinctl, "abov_int_default");
 if (IS_ERR_OR_NULL(this->pinctrl_state_default)) {
  ret = PTR_ERR(this->pinctrl_state_default);
  printk("abov Can not lookup %s pinstate %d\n","abov_int_default", ret);
 }
 wakesrc.name = "abov";
 wakeup_source_add(&wakesrc);
 //wakeup_source_init(&wakesrc, "abov");
 //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
 if (this) {
  /* In case we need to reinitialize data
   * (e.q. if suspend reset device) */
  this->init = initialize;
  /* shortcut to read status of interrupt */
  this->refreshStatus = read_regStat;
  /* pointer to function from platform data to get pendown
   * (1->NIRQ=0, 0->NIRQ=1) */
  this->get_nirq_low = pplatData->get_is_nirq_low;
  /* save irq in case we need to reference it */
  this->irq = gpio_to_irq(client->irq);
  /* do we need to create an irq timer after interrupt ? */
  this->useIrqTimer = 0;
  this->board = pplatData;
  /* Setup function to call on corresponding reg irq source bit */
  if (MAX_NUM_STATUS_BITS >= 8) {
   this->statusFunc[0] = 0; /* TXEN_STAT */
   this->statusFunc[1] = 0; /* UNUSED */
   this->statusFunc[2] = 0; /* UNUSED */
   this->statusFunc[3] = 0;/*read_rawData;  CONV_STAT */
   this->statusFunc[4] = 0; /* COMP_STAT */
   this->statusFunc[5] = touchProcess; /* RELEASE_STAT */
   this->statusFunc[6] = touchProcess; /* TOUCH_STAT  */
   this->statusFunc[7] = 0; /* RESET_STAT */
  }
  /* setup i2c communication */
  this->bus = client;
  i2c_set_clientdata(client, this);

  /* record device struct */
  this->pdev = &client->dev;

  /* create memory for device specific struct */
  this->pDevice = pDevice = kzalloc(sizeof(abov_t), GFP_KERNEL);
  LOG_INFO("\t Initialized Device Specific Memory: 0x%p\n",
    pDevice);
  abov_sar_ptr = this;
  if (pDevice) {
   /* for accessing items in user data (e.g. calibrate) */
   ret = sysfs_create_group(&client->dev.kobj, &abov_attr_group);

   /* Check if we hava a platform initialization function to call*/
   if (pplatData->init_platform_hw)
    pplatData->init_platform_hw();

   /* Add Pointer to main platform data struct */
   pDevice->hw = pplatData;

   /* Initialize the button information initialized with keycodes */
   pDevice->pbuttonInformation = pplatData->pbuttonInformation;

   /* Create the input device */
   input_top = input_allocate_device();
   if (!input_top)
    return -ENOMEM;

   /* Set all the keycodes */
   __set_bit(EV_ABS, input_top->evbit);
   input_set_abs_params(input_top, ABS_DISTANCE, -1, 100, 0, 0);
   /* save the input pointer and finish initialization */
   pDevice->pbuttonInformation->input_top = input_top;
   input_top->name = "ABOV Cap Touch top";
   if (input_register_device(input_top)) {
    LOG_INFO("add top cap touch unsuccess\n");
    return -ENOMEM;
   }
   /* Create the input device */
   input_bottom = input_allocate_device();
   if (!input_bottom)
    return -ENOMEM;
   /* Set all the keycodes */
   __set_bit(EV_ABS, input_bottom->evbit);
   input_set_abs_params(input_bottom, ABS_DISTANCE, -1, 100, 0, 0);
   /* save the input pointer and finish initialization */
   pDevice->pbuttonInformation->input_bottom = input_bottom;
   /* save the input pointer and finish initialization */
   input_bottom->name = "ABOV Cap Touch bottom";
   if (input_register_device(input_bottom)) {
    LOG_INFO("add bottom cap touch unsuccess\n");
    return -ENOMEM;
   }
      input_report_abs(input_top, ABS_DISTANCE, -1);
      input_sync(input_top);
      input_report_abs(input_bottom, ABS_DISTANCE, -1);
      input_sync(input_bottom);
  }
  ret = class_register(&capsense_class);
  if (ret < 0) {
   LOG_DBG("Create fsys class failed (%d)\n", ret);
   return ret;
  }
  ret = class_create_file(&capsense_class, &class_attr_capsense_reset);
  if (ret < 0) {
   LOG_DBG("Create reset file failed (%d)\n", ret);
   return ret;
  }

  ret = class_create_file(&capsense_class, &class_attr_capsense_enable);
  if (ret < 0) {
   LOG_DBG("Create enable file failed (%d)\n", ret);
   return ret;
  }

  ret = class_create_file(&capsense_class, &class_attr_reg_dump);
  if (ret < 0) {
   LOG_DBG("Create reg file failed (%d)\n", ret);
   return ret;
  }

  ret = class_create_file(&capsense_class, &class_attr_capsense_fw_ver);
  if (ret < 0) {
   LOG_DBG("Create update_fw file failed (%d)\n", ret);
   return ret;
  }

  ret = class_create_file(&capsense_class, &class_attr_capsense_force_update_fw);
  if (ret < 0) {
   LOG_DBG("Create update_fw file failed (%d)\n", ret);
   return ret;
  }
#ifdef USE_SENSORS_CLASS
//  sensors_capsensor_top_cdev.sensors_enable = capsensor_set_enable;
//  sensors_capsensor_top_cdev.sensors_poll_delay = NULL;
//  ret = sensors_classdev_register(&input_top->dev, &sensors_capsensor_top_cdev);
//  if (ret < 0)
//   LOG_DBG("create top cap sensor_class  file failed (%d)\n", ret);
  sensors_capsensor_bottom_cdev.sensors_enable = capsensor_set_enable;
  sensors_capsensor_bottom_cdev.sensors_poll_delay = NULL;
  ret = sensors_classdev_register(&input_bottom->dev, &sensors_capsensor_bottom_cdev);
  if (ret < 0)
   LOG_DBG("create bottom cap sensor_class file failed (%d)\n", ret);
#endif



  //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
  //if (this->sar_pinctl ) {
  // err = pinctrl_select_state(this->sar_pinctl,
  //  this->pinctrl_state_default);
  // if (err)
  //  pr_err("%s:%d cannot set pin to default state",
  //   __func__, __LINE__);
  //}
  //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

  abovXX_sar_init(this);
#ifdef FACTORY_VERSION
  write_register(this, ABOV_CTRL_MODE_RET, 0x02);
#else
  write_register(this, ABOV_CTRL_MODE_RET, 0x01);
#endif
  mEnabled = 0;

#ifdef SAR_USB_CHANGE
  INIT_WORK(&this->ps_notify_work, ps_notify_callback_work);
  this->ps_notif.notifier_call = ps_notify_callback;
  ret = power_supply_reg_notifier(&this->ps_notif);
  if (ret) {
   LOG_DBG(
    "Unable to register ps_notifier: %d\n", ret);
   goto free_ps_notifier;
  }

  psy = power_supply_get_by_name("mtk_charger_type");
  if (psy) {
   ret = ps_get_state(psy, &this->ps_is_present);
   if (ret) {
    LOG_DBG(
     "psy get property failed rc=%d\n",
     ret);
    goto free_ps_notifier;
   }
  }
#endif

#if WT_ADD_SAR_HARDWARE_INFO
   read_register(this, ABOV_VERSION_REG, &fw_version);
//Bug584788 hehao.wt, MOD,20201020, modify SAR sensor hardware info
   snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"A96T349HW,FW:0x%x",fw_version);
   hardwareinfo_set_prop(HARDWARE_SAR,firmware_ver);   
#endif

  this->loading_fw = false;
  if (isForceUpdate == true) {   
      INIT_WORK(&this->fw_update_work, capsense_fore_update_work);
  } else {
   INIT_WORK(&this->fw_update_work, capsense_update_work);
  }
  schedule_work(&this->fw_update_work);

        printk("abov_probe end()\n");
  return  0;
 }
 //return -ENOMEM;
#ifdef SAR_USB_CHANGE
free_ps_notifier:
 power_supply_unreg_notifier(&this->ps_notif);

 LOG_INFO("%s input free device.\n", __func__);
 input_free_device(input_top);
 input_free_device(input_bottom);
#endif
err_platform_init:

//err_svdd_error:
// LOG_INFO("%s svdd defer.\n", __func__);
// regulator_disable(pplatData->cap_vdd);
// regulator_put(pplatData->cap_vdd);

err_vdd_defer:
 kfree(pplatData);

 return ret;
}

/**
 * fn static int abov_remove(struct i2c_client *client)
 * brief Called when device is to be removed
 * param client Pointer to i2c_client struct
 * return Value from abovXX_sar_remove()
 */
static int abov_remove(struct i2c_client *client)
{
 struct abov_platform_data * pplatData = 0;
 pabov_t pDevice = 0;
 struct abovXX * this = i2c_get_clientdata(client);

    if(this) {
     pDevice = this->pDevice;
    }

 if (this && pDevice) {
#ifdef USE_SENSORS_CLASS
  sensors_classdev_unregister(&sensors_capsensor_top_cdev);
  sensors_classdev_unregister(&sensors_capsensor_bottom_cdev);
#endif
  input_unregister_device(pDevice->pbuttonInformation->input_top);
  input_unregister_device(pDevice->pbuttonInformation->input_bottom);

  if (this->board->cap_svdd_en) {
   regulator_disable(this->board->cap_svdd);
   regulator_put(this->board->cap_svdd);
  }

  if (this->board->cap_vdd_en) {
   regulator_disable(this->board->cap_vdd);
   regulator_put(this->board->cap_vdd);
  }
#ifdef USE_SENSORS_CLASS
  sensors_classdev_unregister(&sensors_capsensor_top_cdev);
  sensors_classdev_unregister(&sensors_capsensor_bottom_cdev);
#endif
  sysfs_remove_group(&client->dev.kobj, &abov_attr_group);
  pplatData = client->dev.platform_data;
  if (pplatData && pplatData->exit_platform_hw)
   pplatData->exit_platform_hw();
  kfree(this->pDevice);
 }
 return abovXX_sar_remove(this);
}

//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
/***** Kernel Suspend *****/
static int abov_suspend(struct device *dev)
{
  struct i2c_client *client = to_i2c_client(dev);
  struct abovXX * this = i2c_get_clientdata(client);

  LOG_INFO("%s start.\n", __func__);

  if(this->irq_disabled == 0){
      enable_irq_wake(this->irq);
      write_register(this, ABOV_CTRL_MODE_RET, 0x01);
  } else {
      //Bug 440421 huangcunzhu_wt,20190502,add, the bottom current has a vertical line
      write_register(this, ABOV_CTRL_MODE_RET, 0x02);
  }


  LOG_INFO("%s end.\n", __func__);

  return 0;
}

/***** Kernel Resume *****/
static int abov_resume(struct device *dev)
{
   struct i2c_client *client = to_i2c_client(dev);
   struct abovXX * this = i2c_get_clientdata(client);

   LOG_INFO("%s start.\n", __func__);

   if(this->irq_disabled == 0){
  disable_irq_wake(this->irq);
   }
#ifdef FACTORY_VERSION
   write_register(this, ABOV_CTRL_MODE_RET, 0x00);
#else
 write_register(this, ABOV_CTRL_MODE_RET, 0x01);
#endif
   LOG_INFO("%s end.\n", __func__);

   return 0;
}
//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

#ifdef CONFIG_OF
static const struct of_device_id synaptics_rmi4_match_tbl[] = {
 { .compatible = "abov," DRIVER_NAME },
 { },
};
MODULE_DEVICE_TABLE(of, synaptics_rmi4_match_tbl);
#endif

static struct i2c_device_id abov_idtable[] = {
 { DRIVER_NAME, 0 },
 { }
};
MODULE_DEVICE_TABLE(i2c, abov_idtable);

//+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
static SIMPLE_DEV_PM_OPS(abov_pm_ops, abov_suspend, abov_resume);
//-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping

static struct i2c_driver abov_driver = {
 .driver = {
  .owner  = THIS_MODULE,
  .name   = DRIVER_NAME,
  .pm = &abov_pm_ops,
 },
 .id_table = abov_idtable,
 .probe   = abov_probe,
 .remove   = abov_remove,
};
static int __init abov_init(void)
{
 return i2c_add_driver(&abov_driver);
}
static void __exit abov_exit(void)
{
 i2c_del_driver(&abov_driver);
}

//late_initcall(abov_init);
module_init(abov_init);
module_exit(abov_exit);

MODULE_AUTHOR("ABOV Corp.");
MODULE_DESCRIPTION("ABOV Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#ifdef USE_THREADED_IRQ
static void abovXX_process_interrupt(pabovXX_t this, u8 nirqlow)
{
 int status = 0;

 if (!this) {
  pr_err("abovXX_worker_func, NULL abovXX_t\n");
  return;
 }
 /* since we are not in an interrupt don't need to disable irq. */
 status = this->refreshStatus(this);
 LOG_INFO("Worker - Refresh Status %d\n", status);
 this->statusFunc[6](this);
 if (unlikely(this->useIrqTimer && nirqlow)) {
  /* In case we need to send a timer for example on a touchscreen
   * checking penup, perform this here
   */
  cancel_delayed_work(&this->dworker);
  schedule_delayed_work(&this->dworker, msecs_to_jiffies(this->irqTimeout));
  LOG_INFO("Schedule Irq timer");
 }
}


static void abovXX_worker_func(struct work_struct *work)
{
 pabovXX_t this = 0;

 if (work) {
  this = container_of(work, abovXX_t, dworker.work);
  if (!this) {
   LOG_DBG("abovXX_worker_func, NULL abovXX_t\n");
   return;
  }
  if ((!this->get_nirq_low) || (!this->get_nirq_low(this->board->irq_gpio))) {
   /* only run if nirq is high */
   abovXX_process_interrupt(this, 0);
  }
 } else {
  LOG_INFO("abovXX_worker_func, NULL work_struct\n");
 }
}
static irqreturn_t abovXX_interrupt_thread(int irq, void *data)
{
 pabovXX_t this = 0;
 this = data;

 LOG_INFO("abovXX_irq begin\n");

 mutex_lock(&this->mutex);
 LOG_INFO("abovXX_irq\n");

 if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio))
  abovXX_process_interrupt(this, 1);
 else
  LOG_DBG("abovXX_irq - nirq read high\n");
 mutex_unlock(&this->mutex);
 //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
 __pm_wakeup_event(&wakesrc, TEMPORARY_HOLD_TIME);
 //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when sleeping
 return IRQ_HANDLED;
}
#else
static void abovXX_schedule_work(struct abovXX * this, unsigned long delay)
{
 unsigned long flags;

 if (this) {
  LOG_INFO("abovXX_schedule_work()\n");
  spin_lock_irqsave(&this->lock, flags);
  /* Stop any pending penup queues */
  cancel_delayed_work(&this->dworker);
  /*
   *after waiting for a delay, this put the job in the kernel-global
   workqueue. so no need to create new thread in work queue.
   */
  schedule_delayed_work(&this->dworker, delay);
  spin_unlock_irqrestore(&this->lock, flags);
 } else
  LOG_DBG("abovXX_schedule_work, NULL pabovXX_t\n");
}

static irqreturn_t abovXX_irq(int irq, void *pvoid)
{
 struct abovXX * this = 0;
return IRQ_HANDLED;
 if (pvoid) {
  this = (struct abovXX *)pvoid;
  LOG_INFO("abovXX_irq\n");
  if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio)) {
   LOG_INFO("abovXX_irq - Schedule Work\n");
   abovXX_schedule_work(this, 0);
  } else
   LOG_INFO("abovXX_irq - nirq read high\n");
 } else
  LOG_INFO("abovXX_irq, NULL pvoid\n");
 return IRQ_HANDLED;
}

static void abovXX_worker_func(struct work_struct *work)
{
 struct abovXX * this = 0;
 int status = 0;
 int counter = 0;
 u8 nirqLow = 0;

 if (work) {
  this = container_of(work, abovXX_t, dworker.work);

  if (!this) {
   LOG_INFO("abovXX_worker_func, NULL abovXX_t\n");
   return;
  }
  if (unlikely(this->useIrqTimer)) {
   if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio))
    nirqLow = 1;
  }
  /* since we are not in an interrupt don't need to disable irq. */
  status = this->refreshStatus(this);
  counter = -1;
  LOG_INFO("Worker - Refresh Status %d\n", status);
  while ((++counter) < MAX_NUM_STATUS_BITS) { /* counter start from MSB */
   LOG_INFO("Looping Counter %d\n", counter);
   if (((status >> counter) & 0x01) && (this->statusFunc[counter])) {
    LOG_INFO("Function Pointer Found. Calling\n");
    this->statusFunc[counter](this);
   }
  }
  if (unlikely(this->useIrqTimer && nirqLow)) {
   /* Early models and if RATE=0 for newer models require a penup timer */
   /* Queue up the function again for checking on penup */
   abovXX_schedule_work(this, msecs_to_jiffies(this->irqTimeout));
  }
 } else {
  LOG_INFO("abovXX_worker_func, NULL work_struct\n");
 }
}
#endif

void abovXX_sar_suspend(struct abovXX * this)
{
 if (this) {
  //disable_irq(this->irq);
  //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when system is sleeping
  write_register(this, ABOV_CTRL_MODE_RET, 0x01);
  //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when system is sleeping
 }
}
void abovXX_sar_resume(struct abovXX * this)
{
 if (this) {
#ifdef USE_THREADED_IRQ
  mutex_lock(&this->mutex);
  /* Just in case need to reset any uncaught interrupts */
  abovXX_process_interrupt(this, 0);
  mutex_unlock(&this->mutex);
#else
  abovXX_schedule_work(this, 0);
#endif
  if (this->init)
   this->init(this);
  //enable_irq(this->irq);
 }
}

int abovXX_sar_init(struct abovXX * this)
{
 int err = 0;

 if (this && this->pDevice) {
#ifdef USE_THREADED_IRQ

  /* initialize worker function */
  INIT_DELAYED_WORK(&this->dworker, abovXX_worker_func);


  /* initialize mutex */
  mutex_init(&this->mutex);
  /* initailize interrupt reporting */

        //+Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when system is sleeping
        this->board->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
        err = request_threaded_irq(this->irq, NULL, abovXX_interrupt_thread,
              this->board->irq_gpio_flags, this->pdev->driver->name,
              this);
  disable_irq(this->irq);
  this->irq_disabled = 1;
                //-Bug 436909 huangcunzhu_wt,20190416,MODIFY sar work when system is sleeping
#else
  /* initialize spin lock */
  spin_lock_init(&this->lock);

  /* initialize worker function */
  INIT_DELAYED_WORK(&this->dworker, abovXX_worker_func);

  /* initailize interrupt reporting */
  this->irq_disabled = 1;
  err = request_irq(this->irq, abovXX_irq, IRQF_TRIGGER_FALLING,
    this->pdev->driver->name, this);
#endif
  if (err) {
   LOG_DBG("irq %d busy?\n", this->irq);
   return err;
  }
#ifdef USE_THREADED_IRQ
  LOG_DBG("registered with threaded irq (%d)\n", this->irq);
#else
  LOG_DBG("registered with irq (%d)\n", this->irq);
#endif
  /* call init function pointer (this should initialize all registers */
  if (this->init)
   return this->init(this);
  LOG_DBG("No init function!!!!\n");
 }
 return -ENOMEM;
}

int abovXX_sar_remove(struct abovXX * this)
{
 if (this) {
  cancel_delayed_work_sync(&this->dworker); /* Cancel the Worker Func */
  free_irq(this->irq, this);
  kfree(this);
  return 0;
 }
 return -ENOMEM;
}
