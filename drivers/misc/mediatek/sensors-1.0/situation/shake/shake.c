/* shake gesture sensor driver
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
//+Extr WVR2458 tangsha.wt, ADD,20200624, add a shaking gesture function
#define pr_fmt(fmt) "[shake] " fmt

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/init.h>

#include <hwmsensor.h>
#include <sensors_io.h>
#include "situation.h"
#include "shake.h"
#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
//#include "scp_helper.h"
//+Extb WVR-3688 tangsha.wt, MOD,20200713, modify shaking lock screen algorithm and  solve vts test fail problem
static int i=0;
static int count=0;
static int open1 = -1;
static int flag = 0;
static struct workqueue_struct *shake_queue=NULL;
struct delayed_work shake_work;
EXPORT_SYMBOL(shake_work);
struct my_struct_t{
int x;
int y;
int z;
};
struct my_struct_t my_data;
//#define WORK_DELAY_VALUE 1000
static struct situation_init_info shake_init_info;
static int shake_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
      return 0;
        pr_err("sensor_shake_get_data\n");
	err = sensor_get_data_from_hub(ID_ACCELEROMETER, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp		= data.time_stamp;
        *status = data.accelerometer_t.status;
	return 0;
}

static int shake_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{     
#if 0
        pr_err("sensor_shake_batch\n");
	return sensor_batch_to_hub(ID_SHAKE,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
#endif
    return 0 ;
}
static int shake_flush(void)
{
#if 0
        pr_err("sensor_shake_batch\n");
	return sensor_flush_to_hub(ID_SHAKE);
 #endif 
     flag++;
     return 0;
}
 
//+Extb WVR-3611 tangsha.wt, MOD,20200713, modify shaking lock screen algorithm
/*********************************************
* FUNCTION 
* static int stk_sqrt(int in)
* DESCRIPTION
* rough approximation to sqrt
* PARAMETERS
* in integer of which to calculate the sqrt
* RETURNS
* int
*********************************************/
 #if 1
static int stk_sqrt(int in)
{
    long root, bit;
    root = 0;
    for (bit = 0x40000000; bit > 0; bit >>= 2)
    {
        long trial = root + bit;
        root >>= 1;
        if (trial <= in)
        {
            root += bit;
            in -= trial;
        }
    }
    return (int)root;
}
#endif 
#if 1
int shake_recv_data(struct data_unit_t *event,
	void *reserved)
{
	int err = 0;
       if (event->flush_action == DATA_ACTION)
           {
              my_data.x = event->accelerometer_t.x;
              my_data.y = event->accelerometer_t.y;
              my_data.z = event->accelerometer_t.z;
              //pr_err("sensor_shake_recv_data:ID=%d,acc.x=%d,my.x=%d\n",ID_SHAKE,event->accelerometer_t.x,my_data.x);
            }
	return err;
}
#endif
//-Extb WVR-3611 tangsha.wt, MOD,20200713, modify shaking lock screen algorithm
EXPORT_SYMBOL(shake_recv_data);

static void shake_work_handler(struct work_struct *data)
{
  
//+Extb WVR-3908 tangsha.wt, MOD,20200723, solve vts test fail issue due to shake sensor 
        static int old_status=-1;
        int status=0;
        int deltaX,deltaY,deltaZ;
        static int lastX=0,lastY=0,lastZ=0;
        int speed = 0;
        
	//pr_err("sensor_shake_report_data111\n");
              count++;
              deltaX = my_data.x - lastX;
              deltaY = my_data.y - lastY;
              deltaZ = my_data.z - lastZ;
              speed = (stk_sqrt(deltaX*deltaX+deltaY*deltaY+deltaZ*deltaZ))/1000;
	      //pr_err("sensor_shake_report_data107:deltaX =%d,deltaY=%d,deltaZ=%d\n",deltaX,deltaY,deltaZ);
              lastX = my_data.x;
              lastY = my_data.y;
              lastZ = my_data.z;
              if (speed >= 16)
              {
                if(count >30)
                  {
                     i=0;
                     count=0;
                  }
                else
                  {
                     i = i+1 ;
                     count = 0;
                  }
                
              }
              status = (i==5 )?1:0;
              //pr_err("sensor_shake_report_data107:old_status =%d,status=%d,speed=%d,i=%d,count=%d,open1=%d\n",old_status,status,speed,i,count,open1);
              if (status != old_status)
                 {
                     situation_data_report(ID_SHAKE,status);
                     i=0;
                     //pr_err("sensor_shake_report_data111:old_status =%d,status=%d\n",old_status,status);
                     old_status = status ;
                     
                 }

              if (flag)
                 {
		  situation_flush_report(ID_SHAKE);
                  flag--;
                 }
             schedule_delayed_work(&shake_work,msecs_to_jiffies(100));
//-Extb WVR-3908 tangsha.wt, MOD,20200723, solve vts test fail issue due to shake sensor 
}
static int shake_open_report_data(int open)
{
	int ret = 0;
        pr_err("sensor_shake_open_report_data\n");
        pr_err("sensor_shake_open_report_data,open=%d\n",open);
        open1=open;
        if (open == 1)
          {
           schedule_delayed_work(&shake_work,msecs_to_jiffies(100));
          }
        else
          {
           cancel_delayed_work(&shake_work);
          }
#if 0
        pr_err("sensor_shake_open_report_data\n");
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_SHAKE, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	pr_debug("%s : type=%d, open=%d\n",
		__func__, ID_SHAKE, open);
	ret = sensor_enable_to_hub(ID_SHAKE, open);
#endif
	return ret;
}
static int shake_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;
        pr_err("sensor_shake_local_init\n");
        shake_queue=create_singlethread_workqueue("shaking");
        INIT_DELAYED_WORK(&shake_work,shake_work_handler);
	ctl.open_report_data = shake_open_report_data;
	ctl.batch = shake_batch;
	ctl.flush = shake_flush,
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_SHAKE);
        pr_err("sensor_shake_control\n");
	if (err) {
		pr_err("sensor_shake_controlregister stationary control path err\n");
		goto exit;
	}
        
	data.get_data = shake_get_data;
         pr_err("sensor_shake_get_data_init\n");
	 err = situation_register_data_path(&data, ID_SHAKE);
	if (err) {
		pr_err("register stationary data path err\n");
		goto exit;
	}
        pr_err("sensor_shake_init\n");
	err = scp_sensorHub_data_registration(ID_SHAKE,
		shake_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
        

	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int shake_local_uninit(void)
{   
	return 0;
}
//-Extb WVR-3688 tangsha.wt, MOD,20200713, modify shaking lock screen algorithm and  solve vts test fail problem
static struct situation_init_info shake_init_info = {
	.name = "shake_hub",
	.init = shake_local_init,
	.uninit = shake_local_uninit,
};

static int __init shake_init(void)
{
	situation_driver_add(&shake_init_info,
		ID_SHAKE);
	return 0;
}

static void __exit shake_exit(void)
{ 
	pr_debug("%s\n", __func__);
}
//-Extr WVR2458 tangsha.wt, ADD,20200624, add a shaking gesture function
module_init(shake_init);
module_exit(shake_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Stationary Gesture driver");
MODULE_AUTHOR("qiangming.xia@mediatek.com");
