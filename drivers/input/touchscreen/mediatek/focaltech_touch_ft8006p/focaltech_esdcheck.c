/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
*
* File Name: focaltech_esdcheck.c
*
*    Author: Focaltech Driver Team
*
*   Created: 2016-08-03
*
*  Abstract: ESD check function
*
*   Version: v1.0
*
* Revision History:
*        v1.0:
*            First release. By luougojin 2016-08-03
*        v1.1: By luougojin 2017-02-15
*            1. Add LCD_ESD_PATCH to control idc_esdcheck_lcderror
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"

#if FTS_ESDCHECK_EN
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define ESDCHECK_WAIT_TIME              1000    /* ms */
#define LCD_ESD_PATCH                  1// 0

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct fts_esdcheck_st {
    u8      mode                : 1;    /* 1- need check esd 0- no esd check */
    u8      suspend             : 1;
    u8      proc_debug          : 1;    /* apk or adb use */
    u8      intr                : 1;    /* 1- Interrupt trigger */
    u8      unused              : 4;
    u8      flow_work_hold_cnt;         /* Flow Work Cnt(reg0x91) keep a same value for x times. >=5 times is ESD, need reset */
    u8      flow_work_cnt_last;         /* Save Flow Work Cnt(reg0x91) value */
    u32     hardware_reset_cnt;
    u32     nack_cnt;
    u32     dataerror_cnt;
};

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct fts_esdcheck_st fts_esdcheck_data;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern int lcm_power_enable(void);
extern int lcm_power_disable(void);

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/
extern int lcd_need_reset;
#if LCD_ESD_PATCH
static int tp_need_recovery; /* LCD reset cause Tp reset */
int idc_esdcheck_lcderror(struct fts_ts_data *ts_data)
{
    int ret = 0;
    u8 val = 0;

    //FTS_DEBUG("[ESD]Check LCD ESD");
    if ( (tp_need_recovery == 1) && (lcd_need_reset == 0) ) {
        tp_need_recovery = 0;
        /* LCD reset, need recover TP state */
        fts_ft8006p_release_all_finger();
        fts_ft8006p_tp_state_recovery(ts_data);
    }

    ret = fts_ft8006p_read_reg(FTS_REG_ESD_SATURATE, &val);
    if ( ret < 0) {
        FTS_ERROR("[ESD]: Read ESD_SATURATE(0xED) failed ret=%d!", ret);
        return -EIO;
    }

    if (val == 0xAA) {
        /*
        * 1. Set flag lcd_need_reset = 1;
        * 2. LCD driver need reset(recovery) LCD and set lcd_need_reset to 0
        * 3. recover TP state
        */
        FTS_INFO("LCD ESD, Execute LCD reset!");
        lcd_need_reset = 1;
        tp_need_recovery = 1;
    }

    return 0;
}
#endif

static int fts_esdcheck_tp_reset(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();

    fts_esdcheck_data.flow_work_hold_cnt = 0;
    fts_esdcheck_data.hardware_reset_cnt++;

    fts_ft8006p_reset_proc(200);
    fts_ft8006p_release_all_finger();
    fts_ft8006p_tp_state_recovery(ts_data);

    FTS_FUNC_EXIT();
    return 0;
}

/*****************************************************************************
*  Name: get_chip_id
*  Brief: Read Chip Id 3 times
*  Input:
*  Output:
*  Return:  1(ture) - Read Chip Id 3 times failed
*           0(false) - Read Chip Id pass
*****************************************************************************/
static bool get_chip_id(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int i = 0;
    u8 reg_value = 0;
    u8 reg_addr = 0;
    u8 chip_id = ts_data->ic_info.ids.chip_idh;

    for (i = 0; i < 3; i++) {
        reg_addr = FTS_REG_CHIP_ID;
        ret = fts_ft8006p_read(&reg_addr, 1, &reg_value, 1);
        if (ret < 0) {
            FTS_ERROR("[ESD]: Read Reg 0xA3 failed ret = %d!!", ret);
            fts_esdcheck_data.nack_cnt++;
        } else {
            if (reg_value == chip_id) {
                break;
            } else {
                FTS_DEBUG("read chip_id:%x,retry:%d", reg_value, i);
                fts_esdcheck_data.dataerror_cnt++;
            }
        }
        msleep(10);
    }

    /* if can't get correct data in 3 times, then need hardware reset */
    if (i >= 3) {
        FTS_ERROR("[ESD]: Read Chip id 3 times failed, need execute TP reset!!");
        return true;
    }

    return false;
}

/*****************************************************************************
*  Name: get_flow_cnt
*  Brief: Read flow cnt(0x91)
*  Input:
*  Output:
*  Return:  1(true) - Reg 0x91(flow cnt) abnormal: hold a value for 5 times
*           0(false) - Reg 0x91(flow cnt) normal
*****************************************************************************/
static bool get_flow_cnt(struct fts_ts_data *ts_data)
{
    int     ret = 0;
    u8      reg_value = 0;
    u8      reg_addr = 0;

    reg_addr = FTS_REG_FLOW_WORK_CNT;
    ret = fts_ft8006p_read(&reg_addr, 1, &reg_value, 1);
    if (ret < 0) {
        FTS_ERROR("[ESD]: Read Reg 0x91 failed ret = %d!!", ret);
        fts_esdcheck_data.nack_cnt++;
    } else {
        if ( reg_value == fts_esdcheck_data.flow_work_cnt_last ) {
            fts_esdcheck_data.flow_work_hold_cnt++;
        } else {
            fts_esdcheck_data.flow_work_hold_cnt = 0;
        }

        fts_esdcheck_data.flow_work_cnt_last = reg_value;
    }

    /* if read flow work cnt 5 times and the value are all the same, then need hardware_reset */
    if (fts_esdcheck_data.flow_work_hold_cnt >= 5) {
        FTS_DEBUG("[ESD]: Flow Work Cnt(reg0x91) keep a value for 5 times, need execute TP reset!!");
        return true;
    }

    return false;
}

static int esdcheck_algorithm(struct fts_ts_data *ts_data)
{
    int     ret = 0;
    u8      reg_value = 0;
    u8      reg_addr = 0;
    bool    hardware_reset = 0;

    /* 1. esdcheck is interrupt, then return */
    if (fts_esdcheck_data.intr == 1) {
        FTS_DEBUG("[ESD]: In interrupt state, not check esd, return immediately!!");
        return 0;
    }

    /* 2. check power state, if suspend, no need check esd */
    if (fts_esdcheck_data.suspend == 1) {
        FTS_DEBUG("[ESD]: In suspend, not check esd, return immediately!!");
        /* because in suspend state, adb can be used, when upgrade FW, will active ESD check(active = 1)
        *  But in suspend, then will don't queue_delayed_work, when resume, don't check ESD again
        */
        return 0;
    }

    /* 3. check fts_esdcheck_data.proc_debug state, if 1-proc busy, no need check esd*/
    if (fts_esdcheck_data.proc_debug == 1) {
        FTS_INFO("[ESD]: In apk or adb command mode, not check esd, return immediately!!");
        return 0;
    }

    /* 4. In factory mode, can't check esd */
    reg_addr = FTS_REG_WORKMODE;
    ret = fts_ft8006p_read_reg(reg_addr, &reg_value);
    if ( ret < 0 ) {
        fts_esdcheck_data.nack_cnt++;
    } else if ( (reg_value & 0x70) ==  FTS_REG_WORKMODE_FACTORY_VALUE) {
        FTS_DEBUG("[ESD]:  in FACTORY mode, no check esd, return immediately!!");
        return 0;
    }

    /* 5. IDC esd check lcd  default:close */
#if LCD_ESD_PATCH
    idc_esdcheck_lcderror(ts_data);
#endif

    /* 6. Get Chip ID */
    hardware_reset = get_chip_id(ts_data);

    /* 7. get Flow work cnt: 0x91 If no change for 5 times, then ESD and reset */
    if (!hardware_reset) {
        hardware_reset = get_flow_cnt(ts_data);
    }

    /* 8. If need hardware reset, then handle it here */
    if ( hardware_reset == 1) {
        FTS_DEBUG("[ESD]: NoACK=%d, Error Data=%d, Hardware Reset=%d", \
                  fts_esdcheck_data.nack_cnt, fts_esdcheck_data.dataerror_cnt, \
                  fts_esdcheck_data.hardware_reset_cnt);

		lcm_power_disable();
		msleep(6);
		lcm_power_enable();

        msleep(35);
        fts_esdcheck_tp_reset(ts_data);
    }

    return 0;
}

static void esdcheck_func(struct work_struct *work)
{
    u8 val = 0;
	u8 val_1 = 0;
    struct fts_ts_data *ts_data = container_of(work,
                                  struct fts_ts_data, esdcheck_work.work);

    if (ENABLE == fts_esdcheck_data.mode) {
        if (ts_data->ic_info.is_incell) {
            fts_ft8006p_read_reg(FTS_REG_ESDCHECK_DISABLE, &val);
            if (0xA5 == val) {
                fts_esdcheck_data.mode = DISABLE;
                return;
            }
        }
        esdcheck_algorithm(ts_data);

//+add by songbinbo.wt for focal tp  open ESD check function 20190617
	fts_ft8006p_read_reg(0xc8,&val_1);
	 if (0x1 == val_1)
	 {
		lcm_power_disable();
		msleep(6);
		lcm_power_enable();
		fts_ft8006p_write_reg(0xc9,0x1);
	 }
//-add by songbinbo.wt for focal tp  open ESD check function 20190617

        queue_delayed_work(ts_data->ts_workqueue, &ts_data->esdcheck_work,
                           msecs_to_jiffies(ESDCHECK_WAIT_TIME));
    }

}

int fts_ft8006p_esdcheck_set_intr(bool intr)
{
    /* interrupt don't add debug message */
    fts_esdcheck_data.intr = intr;
    return 0;
}

int fts_ft8006p_esdcheck_get_status(void)
{
    /* interrupt don't add debug message */
    return fts_esdcheck_data.mode;
}

/*****************************************************************************
*  Name: fts_ft8006p_esdcheck_proc_busy
*  Brief: When APK or ADB command access TP via driver, then need set proc_debug,
*         then will not check ESD.
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_ft8006p_esdcheck_proc_busy(bool proc_debug)
{
    fts_esdcheck_data.proc_debug = proc_debug;
    return 0;
}

/*****************************************************************************
*  Name: fts_ft8006p_esdcheck_switch
*  Brief: FTS esd check function switch.
*  Input:   enable:  1 - Enable esd check
*                    0 - Disable esd check
*  Output:
*  Return:
*****************************************************************************/
int fts_ft8006p_esdcheck_switch(bool enable)
{
    struct fts_ts_data *ts_data = fts_ft8006p_data;
    FTS_FUNC_ENTER();
    if (fts_esdcheck_data.mode == ENABLE) {
        if (enable) {
            FTS_DEBUG("[ESD]: ESD check start!!");
            fts_esdcheck_data.flow_work_hold_cnt = 0;
            fts_esdcheck_data.flow_work_cnt_last = 0;
            queue_delayed_work(ts_data->ts_workqueue, &ts_data->esdcheck_work,
                               msecs_to_jiffies(ESDCHECK_WAIT_TIME));
        } else {
            FTS_DEBUG("[ESD]: ESD check stop!!");
            cancel_delayed_work_sync(&ts_data->esdcheck_work);
        }
    } else {
        FTS_DEBUG("[ESD]: ESD should disable!!");
        cancel_delayed_work_sync(&ts_data->esdcheck_work);
    }

    FTS_FUNC_EXIT();
    return 0;
}

int fts_ft8006p_esdcheck_suspend(void)
{
    FTS_FUNC_ENTER();
    fts_ft8006p_esdcheck_switch(DISABLE);
    fts_esdcheck_data.suspend = 1;
    FTS_FUNC_EXIT();
    return 0;
}

int fts_ft8006p_esdcheck_resume( void )
{
    FTS_FUNC_ENTER();
    fts_ft8006p_esdcheck_switch(ENABLE);
    fts_esdcheck_data.suspend = 0;
    FTS_FUNC_EXIT();
    return 0;
}

static ssize_t fts_esdcheck_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input_dev = fts_ft8006p_data->input_dev;

    mutex_lock(&input_dev->mutex);
    if (FTS_SYSFS_ECHO_ON(buf)) {
        FTS_DEBUG("enable esdcheck");
        fts_esdcheck_data.mode = ENABLE;
        fts_ft8006p_esdcheck_switch(ENABLE);
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        FTS_DEBUG("disable esdcheck");
        fts_esdcheck_data.mode = DISABLE;
        fts_ft8006p_esdcheck_switch(DISABLE);
    }
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_esdcheck_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count;
    struct input_dev *input_dev = fts_ft8006p_data->input_dev;

    mutex_lock(&input_dev->mutex);
    count = snprintf(buf, PAGE_SIZE, "Esd check: %s\n", \
                     fts_ft8006p_esdcheck_get_status() ? "On" : "Off");
    mutex_unlock(&input_dev->mutex);

    return count;
}

/* sysfs esd node
 *   read example: cat  fts_esd_mode        ---read esd mode
 *   write example:echo 01 > fts_esd_mode   ---make esdcheck enable
 *
 */
static DEVICE_ATTR (fts_esd_mode, S_IRUGO | S_IWUSR, fts_esdcheck_show, fts_esdcheck_store);

static struct attribute *fts_esd_mode_attrs[] = {

    &dev_attr_fts_esd_mode.attr,
    NULL,
};

static struct attribute_group fts_esd_group = {
    .attrs = fts_esd_mode_attrs,
};

int fts_ft8006p_create_esd_sysfs(struct device *dev)
{
    int ret = 0;

    ret = sysfs_create_group(&dev->kobj, &fts_esd_group);
    if ( ret != 0) {
        FTS_ERROR("fts_ft8006p_create_esd_sysfs(sysfs) create failed!");
        sysfs_remove_group(&dev->kobj, &fts_esd_group);
        return ret;
    }
    return 0;
}

int fts_ft8006p_esdcheck_init(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();

    if (ts_data->ts_workqueue) {
        INIT_DELAYED_WORK(&ts_data->esdcheck_work, esdcheck_func);
    } else {
        FTS_ERROR("fts workqueue is NULL, can't run esd check function");
        return -EINVAL;
    }

    memset((u8 *)&fts_esdcheck_data, 0, sizeof(struct fts_esdcheck_st));

    fts_esdcheck_data.mode = ENABLE;
    fts_ft8006p_esdcheck_switch(ENABLE);
    fts_ft8006p_create_esd_sysfs(ts_data->dev);
    FTS_FUNC_EXIT();
    return 0;
}

int fts_ft8006p_esdcheck_exit(struct fts_ts_data *ts_data)
{
    sysfs_remove_group(&ts_data->dev->kobj, &fts_esd_group);
    return 0;
}
#endif /* FTS_ESDCHECK_EN */

