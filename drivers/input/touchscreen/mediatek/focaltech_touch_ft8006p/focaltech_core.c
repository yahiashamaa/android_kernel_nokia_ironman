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
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#include "focaltech_core.h"
#include "tpd.h"
#if FTS_MTK_OLD_TYPE
#include "mt_spi.h"
#endif

#include <linux/sched/types.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts_ft8006p"
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      2800000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

#define FTS_I2C_SLAVE_ADDR                  0x38

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag;

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
static int tpd_def_calmat_local_normal[8]  = TPD_CALIBRATION_MATRIX_ROTATION_NORMAL;
static int tpd_def_calmat_local_factory[8] = TPD_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif

#ifndef RTPM_PRIO_TPD
#define RTPM_PRIO_TPD                       0x04
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_ft8006p_data;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
*  Name: fts_ft8006p_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_ft8006p_wait_tp_to_valid(void)
{
    int ret = 0;
    int cnt = 0;
    u8 reg_value = 0;
    u8 chip_id = fts_ft8006p_data->ic_info.ids.chip_idh;

    do {
        ret = fts_ft8006p_read_reg(FTS_REG_CHIP_ID, &reg_value);
        if ((ret < 0) || (reg_value != chip_id)) {
            FTS_DEBUG("TP Not Ready, ReadData = 0x%x", reg_value);
        } else if (reg_value == chip_id) {
            FTS_INFO("TP Ready, Device ID = 0x%x", reg_value);
            return 0;
        }
        cnt++;
        msleep(INTERVAL_READ_REG);
    } while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

    return -EIO;
}

/*****************************************************************************
*  Name: fts_ft8006p_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
 void fts_ft8006p_tp_state_recovery(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    /* wait tp stable */
    fts_ft8006p_wait_tp_to_valid();
    /* recover TP charger state 0x8B */
    /* recover TP glove state 0xC0 */
    /* recover TP cover state 0xC1 */
    fts_ft8006p_ex_mode_recovery(ts_data);
#if FTS_PSENSOR_EN
    fts_proximity_recovery(ts_data);
#endif

    /* recover TP gesture state 0xD0 */
#if FTS_GESTURE_EN
    fts_gesture_recovery(ts_data);
#endif
    FTS_FUNC_EXIT();
}

int fts_ft8006p_reset_proc(int hdelayms)
{
    FTS_DEBUG("tp reset");
    tpd_gpio_output(fts_ft8006p_data->pdata->reset_gpio, 0);
    msleep(5);
    tpd_gpio_output(fts_ft8006p_data->pdata->reset_gpio, 1);
    if (hdelayms) {
        msleep(hdelayms);
    }

    return 0;
}

void fts_ft8006p_irq_disable(void)
{
    unsigned long irqflags;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_ft8006p_data->irq_lock, irqflags);

    if (!fts_ft8006p_data->irq_disabled) {
        disable_irq_nosync(fts_ft8006p_data->irq);
        fts_ft8006p_data->irq_disabled = true;
    }

    spin_unlock_irqrestore(&fts_ft8006p_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

void fts_ft8006p_irq_enable(void)
{
    unsigned long irqflags = 0;

    FTS_FUNC_ENTER();
    spin_lock_irqsave(&fts_ft8006p_data->irq_lock, irqflags);

    if (fts_ft8006p_data->irq_disabled) {
        enable_irq(fts_ft8006p_data->irq);
        fts_ft8006p_data->irq_disabled = false;
    }

    spin_unlock_irqrestore(&fts_ft8006p_data->irq_lock, irqflags);
    FTS_FUNC_EXIT();
}

void fts_ft8006p_hid2std(void)
{
    int ret = 0;
    u8 buf[3] = {0xEB, 0xAA, 0x09};

    ret = fts_ft8006p_write(buf, 3);
    if (ret < 0) {
        FTS_ERROR("hid2std cmd write fail");
    } else {
        msleep(10);
        buf[0] = buf[1] = buf[2] = 0;
        ret = fts_ft8006p_read(NULL, 0, buf, 3);
        if (ret < 0) {
            FTS_ERROR("hid2std cmd read fail");
        } else if ((0xEB == buf[0]) && (0xAA == buf[1]) && (0x08 == buf[2])) {
            FTS_DEBUG("hidi2c change to stdi2c successful");
        } else {
            FTS_DEBUG("hidi2c change to stdi2c not support or fail");
        }
    }
}

static int fts_get_chip_types(
    struct fts_ts_data *ts_data,
    u8 id_h, u8 id_l, bool fw_valid)
{
    int i = 0;
    struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
    u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

    if ((0x0 == id_h) || (0x0 == id_l)) {
        FTS_ERROR("id_h/id_l is 0");
        return -EINVAL;
    }

    FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
    for (i = 0; i < ctype_entries; i++) {
        if (VALID == fw_valid) {
            if ((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
                break;
        } else {
            if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
                || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
                || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
                break;
        }
    }

    if (i >= ctype_entries) {
        return -ENODATA;
    }

    ts_data->ic_info.ids = ctype[i];
    return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
    int ret = 0;
    u8 chip_id[2] = { 0 };
    u8 id_cmd[4] = { 0 };
    u32 id_cmd_len = 0;

    id_cmd[0] = FTS_CMD_START1;
    id_cmd[1] = FTS_CMD_START2;
    ret = fts_ft8006p_write(id_cmd, 2);
    if (ret < 0) {
        FTS_ERROR("start cmd write fail");
        return ret;
    }

    msleep(FTS_CMD_START_DELAY);
    id_cmd[0] = FTS_CMD_READ_ID;
    id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
    if (ts_data->ic_info.is_incell)
        id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
    else
        id_cmd_len = FTS_CMD_READ_ID_LEN;
    ret = fts_ft8006p_read(id_cmd, id_cmd_len, chip_id, 2);
    if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
        FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
        return -EIO;
    }

    id[0] = chip_id[0];
    id[1] = chip_id[1];
    return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int cnt = 0;
    u8 chip_id[2] = { 0 };

    ts_data->ic_info.is_incell = FTS_CHIP_IDC;
    ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;

    for (cnt = 0; cnt < 3; cnt++) {
        fts_ft8006p_reset_proc(0);
        mdelay(8);

        ret = fts_read_bootid(ts_data, &chip_id[0]);
        if (ret <  0) {
            FTS_DEBUG("read boot id fail,retry:%d", cnt);
            continue;
        }

        ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
        if (ret < 0) {
            FTS_DEBUG("can't get ic informaton,retry:%d", cnt);
            continue;
        }

        break;
    }

    if (cnt >= 3) {
        FTS_ERROR("get ic informaton fail");
        return -EIO;
    }


    FTS_INFO("get ic information, chip id = 0x%02x%02x",
             ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

    return 0;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, int datalen)
{
    int i = 0;
    int count = 0;
    char *tmpbuf = NULL;

    tmpbuf = kzalloc(1024, GFP_KERNEL);
    if (!tmpbuf) {
        FTS_ERROR("tmpbuf zalloc fail");
        return;
    }

    for (i = 0; i < datalen; i++) {
        count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
        if (count >= 1024)
            break;
    }
    FTS_DEBUG("point buffer:%s", tmpbuf);

    if (tmpbuf) {
        kfree(tmpbuf);
        tmpbuf = NULL;
    }
}

void fts_ft8006p_release_all_finger(void)
{
    struct input_dev *input_dev = fts_ft8006p_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
    u32 finger_count = 0;
    u32 max_touches = fts_ft8006p_data->pdata->max_touch_number;
#endif

    FTS_FUNC_ENTER();
    mutex_lock(&fts_ft8006p_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
    for (finger_count = 0; finger_count < max_touches; finger_count++) {
        input_mt_slot(input_dev, finger_count);
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
#else
    input_mt_sync(input_dev);
#endif
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_sync(input_dev);

    fts_ft8006p_data->touchs = 0;
    fts_ft8006p_data->key_state = 0;
    mutex_unlock(&fts_ft8006p_data->report_mutex);
    FTS_FUNC_EXIT();
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
    int i = 0;
    int x = data->events[index].x;
    int y = data->events[index].y;
    int *x_dim = &data->pdata->key_x_coords[0];
    int *y_dim = &data->pdata->key_y_coords[0];

    if (!data->pdata->have_key) {
        return -EINVAL;
    }
    for (i = 0; i < data->pdata->key_number; i++) {
        if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
            (y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
            if (EVENT_DOWN(data->events[index].flag)
                && !(data->key_state & (1 << i))) {
                input_report_key(data->input_dev, data->pdata->keys[i], 1);
                data->key_state |= (1 << i);
                FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
            } else if (EVENT_UP(data->events[index].flag)
                       && (data->key_state & (1 << i))) {
                input_report_key(data->input_dev, data->pdata->keys[i], 0);
                data->key_state &= ~(1 << i);
                FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
            }
            return 0;
        }
    }
    return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
    int i = 0;
    int uppoint = 0;
    int touchs = 0;
    bool va_reported = false;
    u32 max_touch_num = data->pdata->max_touch_number;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (fts_input_report_key(data, i) == 0) {
            continue;
        }

        va_reported = true;
        input_mt_slot(data->input_dev, events[i].id);

        if (EVENT_DOWN(events[i].flag)) {
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            touchs |= BIT(events[i].id);
            data->touchs |= BIT(events[i].id);

            if ((data->log_level >= 2) ||
                ((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
                /*FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
                          events[i].id,
                          events[i].x, events[i].y,
                          events[i].p, events[i].area);*/
            }
        } else {
            uppoint++;
            input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            data->touchs &= ~BIT(events[i].id);
            /*if (data->log_level >= 1) {
                FTS_DEBUG("[B]P%d UP!", events[i].id);
            }*/
        }
    }

    if (unlikely(data->touchs ^ touchs)) {
        for (i = 0; i < max_touch_num; i++)  {
            if (BIT(i) & (data->touchs ^ touchs)) {
             /*   if (data->log_level >= 1) {
                    FTS_DEBUG("[B]P%d UP!", i);
                }*/
                va_reported = true;
                input_mt_slot(data->input_dev, i);
                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
            }
        }
    }
    data->touchs = touchs;

    if (va_reported) {
        /* touchs==0, there's no point but key */
        if (EVENT_NO_DOWN(data) || (!touchs)) {
           /* if (data->log_level >= 1) {
                FTS_DEBUG("[B]Points All Up!");
            }*/
            input_report_key(data->input_dev, BTN_TOUCH, 0);
        } else {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
    int i = 0;
    int touchs = 0;
    bool va_reported = false;
    struct ts_event *events = data->events;

    for (i = 0; i < data->touch_point; i++) {
        if (fts_input_report_key(data, i) == 0) {
            continue;
        }

        va_reported = true;
        if (EVENT_DOWN(events[i].flag)) {
            input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
            if (events[i].p <= 0) {
                events[i].p = 0x3f;
            }
            input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
            if (events[i].area <= 0) {
                events[i].area = 0x09;
            }
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

            input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

            input_mt_sync(data->input_dev);

            if ((data->log_level >= 2) ||
                ((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
                FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
                          events[i].id,
                          events[i].x, events[i].y,
                          events[i].p, events[i].area);
            }
            touchs++;
        }
    }

    /* last point down, current no point but key */
    if (data->touchs && !touchs) {
        va_reported = true;
    }
    data->touchs = touchs;

    if (va_reported) {
        if (EVENT_NO_DOWN(data)) {
            if (data->log_level >= 1) {
                FTS_DEBUG("[A]Points All Up!");
            }
            input_report_key(data->input_dev, BTN_TOUCH, 0);
            input_mt_sync(data->input_dev);
        } else {
            input_report_key(data->input_dev, BTN_TOUCH, 1);
        }
    }

    input_sync(data->input_dev);
    return 0;
}
#endif

static int fts_read_touchdata(struct fts_ts_data *data)
{
    int ret = 0;
    u8 *buf = data->point_buf;

    memset(buf, 0xFF, data->pnt_buf_size);


    buf[0] = 0x01;
    ret = fts_ft8006p_read(buf, 1, buf + 1, data->pnt_buf_size - 1);
    if ((ret < 0) && ((buf[1] & 0xF0) != 0x90)) {
        /* check if need recovery fw */
        fts_ft8006p_fw_recovery();
	FTS_ERROR("fts_fw_recovery end");
	data->fw_is_running = true;//add by songbinbo.wt for tp small probability abnormal  20191025
        return ret;
    }
    if ((ret < 0) || ((buf[1] & 0xF0) != 0x90)) {
        FTS_ERROR("touch data(%x) abnormal,ret:%d", buf[1], ret);
        return -EIO;
    }

#if FTS_GESTURE_EN
    ret = fts_gesture_readdata(data, buf + FTS_TOUCH_DATA_LEN);
    if (0 == ret) {
        FTS_INFO("succuss to get gesture data in irq handler");
        return 1;
    }
#endif


    if (data->log_level >= 3) {
        fts_show_touch_buffer(buf, data->pnt_buf_size);
    }

    return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *data)
{
    int ret = 0;
    int i = 0;
    u8 pointid = 0;
    int base = 0;
    struct ts_event *events = data->events;
    int max_touch_num = data->pdata->max_touch_number;
    u8 *buf = data->point_buf;

    ret = fts_read_touchdata(data);
    if (ret) {
        return ret;
    }

    data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
    data->touch_point = 0;

    if (data->ic_info.is_incell) {
        if ((data->point_num == 0x0F) && (buf[2] == 0xFF) && (buf[3] == 0xFF)
            && (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
            FTS_DEBUG("touch buff is 0xff, need recovery state");
            fts_ft8006p_release_all_finger();
            fts_ft8006p_tp_state_recovery(data);
            return -EIO;
        }
    }

    if (data->point_num > max_touch_num) {
        FTS_INFO("invalid point_num(%d)", data->point_num);
        return -EIO;
    }

    for (i = 0; i < max_touch_num; i++) {
        base = FTS_ONE_TCH_LEN * i;
        pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
        if (pointid >= FTS_MAX_ID)
            break;
        else if (pointid >= max_touch_num) {
            FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
            return -EINVAL;
        }

        data->touch_point++;
        events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
                      (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
        events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
                      (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
        events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
        events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
        events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
        events[i].p =  buf[FTS_TOUCH_PRE_POS + base];

        if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
            FTS_INFO("abnormal touch data from fw");
            return -EIO;
        }
    }

    if (data->touch_point == 0) {
        FTS_INFO("no touch point information");
        return -EIO;
    }

    return 0;
}

static void fts_irq_read_report(void)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_ft8006p_data;

#if FTS_ESDCHECK_EN
    fts_ft8006p_esdcheck_set_intr(1);
#endif

#if FTS_POINT_REPORT_CHECK_EN
    fts_prc_queue_work(ts_data);
#endif

    ret = fts_read_parse_touchdata(ts_data);
    if (ret == 0) {
        mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
        fts_input_report_b(ts_data);
#else
        fts_input_report_a(ts_data);
#endif
        mutex_unlock(&ts_data->report_mutex);
    }

#if FTS_ESDCHECK_EN
    fts_ft8006p_esdcheck_set_intr(0);
#endif
}

static int touch_event_handler(void *unused)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

    sched_setscheduler(current, SCHED_RR, &param);
    do {
        set_current_state(TASK_INTERRUPTIBLE);
        wait_event_interruptible(waiter, tpd_flag != 0);

        tpd_flag = 0;
        set_current_state(TASK_RUNNING);

#if FTS_PSENSOR_EN
        if (fts_proximity_readdata(fts_ft8006p_data) == 0)
            continue;
#endif

        fts_irq_read_report();
    } while (!kthread_should_stop());

    return 0;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
    tpd_flag = 1;
    wake_up_interruptible(&waiter);
    return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
    int ret = 0;
    struct device_node *node = NULL;

    node = of_find_matching_node(node, touch_of_match);
    if (NULL == node) {
        FTS_ERROR("Can not find touch eint device node!");
        return -ENODATA;
    }

    ts_data->thread_tpd = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR_OR_NULL(ts_data->thread_tpd)) {
        ret = PTR_ERR(ts_data->thread_tpd);
        FTS_ERROR("create kernel thread_tpd fail,ret:%d", ret);
        ts_data->thread_tpd = NULL;
        return ret;
    }

    tpd_gpio_as_int(ts_data->pdata->irq_gpio);

    ts_data->irq = irq_of_parse_and_map(node, 0);
    ts_data->pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING;
    FTS_INFO("irq:%d, flag:%x", ts_data->irq, ts_data->pdata->irq_gpio_flags);
    ret = request_irq(ts_data->irq, fts_irq_handler,
                      ts_data->pdata->irq_gpio_flags,
                      FTS_DRIVER_NAME, ts_data);

    return ret;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int key_num = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;
    struct input_dev *input_dev;

    FTS_FUNC_ENTER();
    input_dev = input_allocate_device();
    if (!input_dev) {
        FTS_ERROR("Failed to allocate memory for input device");
        return -ENOMEM;
    }

    /* Init and register Input device */
    input_dev->name = FTS_DRIVER_NAME;
    input_dev->id.bustype = BUS_SPI;

    input_dev->dev.parent = ts_data->dev;

    input_set_drvdata(input_dev, ts_data);

    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

    if (pdata->have_key) {
        FTS_INFO("set key capabilities");
        for (key_num = 0; key_num < pdata->key_number; key_num++)
            input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
    }

#if FTS_MT_PROTOCOL_B_EN
    input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
    input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

    ret = input_register_device(input_dev);
    if (ret) {
        FTS_ERROR("Input device registration failed");
        input_set_drvdata(input_dev, NULL);
        input_free_device(input_dev);
        input_dev = NULL;
        return ret;
    }

    ts_data->input_dev = input_dev;

    FTS_FUNC_EXIT();
    return 0;
}

static int fts_report_buffer_init(struct fts_ts_data *ts_data)
{
    int point_num = 0;
    int events_num = 0;

    point_num = FTS_MAX_POINTS_SUPPORT;
    ts_data->pnt_buf_size = FTS_TOUCH_DATA_LEN + FTS_GESTURE_DATA_LEN;

    ts_data->point_buf = (u8 *)kzalloc(ts_data->pnt_buf_size + 1, GFP_KERNEL);
    if (!ts_data->point_buf) {
        FTS_ERROR("failed to alloc memory for point buf");
        return -ENOMEM;
    }

    events_num = point_num * sizeof(struct ts_event);
    ts_data->events = (struct ts_event *)kzalloc(events_num, GFP_KERNEL);
    if (!ts_data->events) {
        FTS_ERROR("failed to alloc memory for point events");
        kfree_safe(ts_data->point_buf);
        return -ENOMEM;
    }

    return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
    int ret = 0;

    if (IS_ERR_OR_NULL(ts_data->vdd)) {
        FTS_ERROR("vdd is invalid");
        return -EINVAL;
    }

    FTS_FUNC_ENTER();
    if (enable) {
        if (ts_data->power_disabled) {
            FTS_DEBUG("regulator enable !");
            tpd_gpio_output(ts_data->pdata->reset_gpio, 0);
            msleep(1);
            ret = regulator_enable(ts_data->vdd);
            if (ret) {
                FTS_ERROR("enable vdd regulator failed,ret=%d", ret);
            }
            ts_data->power_disabled = false;
        }
    } else {
        if (!ts_data->power_disabled) {
            FTS_DEBUG("regulator disable !");
            tpd_gpio_output(ts_data->pdata->reset_gpio, 0);
            msleep(1);
            ret = regulator_disable(ts_data->vdd);
            if (ret) {
                FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
            }
            ts_data->power_disabled = true;
        }
    }

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_power_source_init(struct fts_ts_data *ts_data)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    ts_data->vdd = regulator_get(tpd->tpd_dev, "vtouch");
    if (IS_ERR_OR_NULL(ts_data->vdd)) {
        ret = PTR_ERR(ts_data->vdd);
        FTS_ERROR("get vdd regulator failed,ret=%d", ret);
        return ret;
    }

    if (regulator_count_voltages(ts_data->vdd) > 0) {
        ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV,
                                    FTS_VTG_MAX_UV);
        if (ret) {
            FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
            regulator_put(ts_data->vdd);
            return ret;
        }
    }

    ts_data->power_disabled = true;
    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret) {
        FTS_ERROR("fail to enable power(regulator)");
    }

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
    fts_power_source_ctrl(ts_data, DISABLE);

    if (!IS_ERR_OR_NULL(ts_data->vdd)) {
        if (regulator_count_voltages(ts_data->vdd) > 0)
            regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
        regulator_put(ts_data->vdd);
    }

    return 0;
}

static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
    int ret = 0;

    ret = fts_power_source_ctrl(ts_data, DISABLE);
    if (ret < 0) {
        FTS_ERROR("power off fail, ret=%d", ret);
    }

    return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
    int ret = 0;

    ret = fts_power_source_ctrl(ts_data, ENABLE);
    if (ret < 0) {
        FTS_ERROR("power on fail, ret=%d", ret);
    }

    return ret;
}
#endif /* FTS_POWER_SOURCE_CUST_EN */

static int fts_gpio_configure(struct fts_ts_data *ts_data)
{
    tpd_gpio_output(ts_data->pdata->reset_gpio, 1);
    return 0;
}

static void fts_platform_data_init(struct fts_ts_data *ts_data)
{
    int i = 0;
    struct fts_ts_platform_data *pdata = ts_data->pdata;

    if (tpd_dts_data.use_tpd_button) {
        pdata->have_key = tpd_dts_data.use_tpd_button;
        pdata->key_number = tpd_dts_data.tpd_key_num;
        for (i = 0; i < pdata->key_number; i++) {
            pdata->key_x_coords[i] = tpd_dts_data.tpd_key_dim_local[i].key_x;
            pdata->key_y_coords[i] = tpd_dts_data.tpd_key_dim_local[i].key_y;
        }
        memcpy(pdata->keys, tpd_dts_data.tpd_key_local,
               pdata->key_number * sizeof(int));

        FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
                 "coords:(%d,%d),(%d,%d),(%d,%d)",
                 pdata->key_number,
                 pdata->keys[0], pdata->keys[1], pdata->keys[2],
                 pdata->key_x_coords[0], pdata->key_y_coords[0],
                 pdata->key_x_coords[1], pdata->key_y_coords[1],
                 pdata->key_x_coords[2], pdata->key_y_coords[2]);
    }
    pdata->max_touch_number = tpd_dts_data.touch_max_num;
    pdata->irq_gpio = 1;
    pdata->reset_gpio = 0;
    pdata->x_min = 0;
    pdata->x_max = TPD_RES_X;
    pdata->y_min = 0;
    pdata->y_max = TPD_RES_Y;

    FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d"
             "resolution:(%d,%d)~(%d,%d)",
             pdata->max_touch_number,
             pdata->irq_gpio, pdata->reset_gpio,
             pdata->x_min, pdata->y_min,
             pdata->x_max, pdata->y_max);
}

#ifdef FTS_FT8006P_CONFIG_TOUCHSCREEN_LOCKDOWN_INFO
static struct proc_dir_entry *fts_proc_lock_down_info_wt;
static u8 lockdown_info[8];
static unsigned char g_tp_color = 0x00;

static int get_tp_color(void)
{
	int ret = 0;
	char *tp_color_start=NULL;
        char *temp;
	char tp_color[8]={'\0'};

	/*parse tp color from command line*/
	tp_color_start = strstr(saved_command_line, "tp_color=");
	 if(tp_color_start == NULL){
		FTS_ERROR("command_line have non tp_color info.\n");
		return -1;
	 }

	temp = tp_color_start + strlen("tp_color=");
	memcpy(tp_color, temp, 4);
        FTS_INFO("Read tp_color from command_line is %s.\n", tp_color);

	/* convert tp color string to unsigned int*/
	ret =  kstrtou8(tp_color, 0, &g_tp_color);
	if (ret != 0){
		FTS_ERROR("Convert tp color string to unsigned int error.\n");
		return -EINVAL;
	}
	FTS_INFO("Tp color is : 0x%02x.\n", g_tp_color);
       return ret;
}

static ssize_t ctp_lockdown_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
        char *ptr;
        int ret;

        if(*ppos){
            FTS_ERROR("tp test again return\n");
            return 0;
        }

        *ppos += count;
        ptr = kzalloc(count, GFP_KERNEL);

        /*
       * get lock down info
       */
        memset(lockdown_info, 0, sizeof(lockdown_info));
	get_tp_color(); //get tp color
	lockdown_info[2] = g_tp_color;

       FTS_INFO("%s:Get tp color is:0x%02x.\n", __func__, g_tp_color);

	ret=  sprintf(ptr, "%02X%02X%02X%02X%02X%02X%02X%02X\n",
				   lockdown_info[0], lockdown_info[1], lockdown_info[2], lockdown_info[3],
				   lockdown_info[4], lockdown_info[5], lockdown_info[6], lockdown_info[7]);

	if (copy_to_user(buf, ptr, ret+1)) {
                FTS_ERROR("%s: fail to copy default config\n", __func__);
                ret = -EFAULT;
        }

	kfree(ptr);

        return  ret;
}

static ssize_t ctp_lockdown_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
{
    return -1;
}

static const struct file_operations fts_proc_lockdown_info_ops = {
    .write = ctp_lockdown_proc_write,
    .read = ctp_lockdown_proc_read,
    .owner = THIS_MODULE,
};

#endif

//+ add by songbinbo.wt for tp open short  20190404
#if OPEN_SHORT_PROC
#define CTP_PARENT_PROC_NAME  "touchscreen"
#define CTP_OPEN_PROC_NAME    "ctp_openshort_test"
#define  CTP_OPEN_SHORT_TEST_INI_NAME	"T99621_Focal_truly.ini"
extern int fts_ft8006p_open_short_test(char *ini_file_name);

static void fts_set_ini_name(char *cfgname)
{
	//int ret;

        sprintf(cfgname, "%s", CTP_OPEN_SHORT_TEST_INI_NAME);
        //return ret;
}

static ssize_t ctp_open_proc_write(struct file *filp, const char __user *userbuf,size_t count, loff_t *ppos)
{
	return -1;
}

static ssize_t ctp_open_proc_read(struct file *file, char __user *buf,size_t count, loff_t *ppos)
{
	char fwname[128] = {0};
	struct fts_ts_data *ts_data = fts_ft8006p_data;
	struct spi_device *slave_dev;
	struct input_dev *input_dev;
	int result = 0;
	int ret;
	int len = count;

	slave_dev = ts_data->spi;
	input_dev = ts_data->input_dev;

	if(*ppos){
		FTS_ERROR("tp test again return\n");
		return 0;
	}
	*ppos += count;

	memset(fwname, 0, sizeof(fwname));
	fts_set_ini_name(fwname);
	fwname[strlen(fwname)] = '\0';
	printk("fwname:%s.", fwname);

	mutex_lock(&input_dev->mutex);
	disable_irq(slave_dev->irq);

#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_ft8006p_esdcheck_switch(DISABLE);
#endif

	//+ add by songbinbo.wt for tp open short  20190418
	ret = fts_ft8006p_enter_test_environment(1);
	  if (ret < 0) {
		  FTS_ERROR("enter test environment fail");
	  } else {
		ret = fts_ft8006p_open_short_test(fwname);
		if(ret == 1){
			result = 1;
			printk("fts open short test success\n");
		}else{
			result = 0;
			printk("fts open short test fail\n");
		}
	  }
	  ret = fts_ft8006p_enter_test_environment(0);
	  if (ret < 0) {
		  FTS_ERROR("enter normal environment fail");
	  }
	//- add by songbinbo.wt for tp open short  20190404
#if defined(FTS_ESDCHECK_EN) && (FTS_ESDCHECK_EN)
	fts_ft8006p_esdcheck_switch(ENABLE);
#endif

	enable_irq(slave_dev->irq);
	mutex_unlock(&input_dev->mutex);

	if (count > 9)
		len = 9;
	printk("fts result = %d\n",result);
	if (result == 1){
		if (copy_to_user(buf, "result=1\n", len)) {
			FTS_ERROR("copy_to_user fail\n");
			return -1;
		}
 	}else{
 		if (copy_to_user(buf, "result=0\n", len)) {
			FTS_ERROR("copy_to_user fail\n");
			return -1;
 		}
	}
 return len;
}

static  struct file_operations ctp_open_procs_fops =
{
    .write = ctp_open_proc_write,
    .read = ctp_open_proc_read,
    .owner = THIS_MODULE,
};

static struct proc_dir_entry *ctp_device_proc = NULL;

void fts_ft8006p_create_ctp_proc(void)
{
   //----------------------------------------
    //create read/write interface for tp information
    //the path is :proc/touchscreen
    //child node is :version
    //----------------------------------------
    struct proc_dir_entry *ctp_open_proc = NULL;
    printk("openshort fts_ft8006p_create_ctp_proc \n");

    if( ctp_device_proc == NULL)
    {
         ctp_device_proc = proc_mkdir(CTP_PARENT_PROC_NAME, NULL);
        if(ctp_device_proc == NULL)
        {
            FTS_ERROR("create parent_proc fail\n");
            return;
        }
    }
    ctp_open_proc = proc_create(CTP_OPEN_PROC_NAME, 0777, ctp_device_proc, &ctp_open_procs_fops);
    if (ctp_open_proc == NULL)
    {
        FTS_ERROR("create open_proc fail\n");
	return;
    }

#ifdef FTS_FT8006P_CONFIG_TOUCHSCREEN_LOCKDOWN_INFO
	fts_proc_lock_down_info_wt = proc_create(FTS_PROC_LOCK_DOWN_INFO_FILE_WT, 0777,ctp_device_proc, &fts_proc_lockdown_info_ops);
	if (fts_proc_lock_down_info_wt == NULL)
	{
			FTS_ERROR(" %s: proc lock_down info file create failed!\n", __func__);
			return;
	}
#endif

	return;
}

#endif
//- add by songbinbo.wt for tp open short  20190404
static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
    int ret = 0;
    int pdata_size = sizeof(struct fts_ts_platform_data);

    FTS_FUNC_ENTER();
    FTS_INFO("%s", FTS_DRIVER_VERSION);
    ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
    if (!ts_data->pdata) {
        FTS_ERROR("allocate memory for platform_data fail");
        return -ENOMEM;
    }
    fts_platform_data_init(ts_data);

    ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
    if (!ts_data->ts_workqueue) {
        FTS_ERROR("create fts workqueue fail");
    }

    spin_lock_init(&ts_data->irq_lock);
    mutex_init(&ts_data->report_mutex);
    mutex_init(&ts_data->bus_lock);

    /* Init communication interface */
    ret = fts_ft8006p_bus_init(ts_data);
    if (ret) {
        FTS_ERROR("bus initialize fail");
        goto err_bus_init;
    }

    ret = fts_input_init(ts_data);
    if (ret) {
        FTS_ERROR("input initialize fail");
        goto err_input_init;
    }

    ret = fts_report_buffer_init(ts_data);
    if (ret) {
        FTS_ERROR("report buffer init fail");
        goto err_report_buffer;
    }

    ret = fts_gpio_configure(ts_data);
    if (ret) {
        FTS_ERROR("configure the gpios fail");
        goto err_gpio_config;
    }

#if FTS_POWER_SOURCE_CUST_EN
    ret = fts_power_source_init(ts_data);
    if (ret) {
        FTS_ERROR("fail to get power(regulator)");
        goto err_power_init;
    }
#endif

#if (!FTS_CHIP_IDC)
    fts_ft8006p_reset_proc(200);
#endif

    ret = fts_get_ic_information(ts_data);
    if (ret) {
        FTS_ERROR("not focal IC, unregister driver");
        goto err_irq_req;
    }

#if FTS_APK_NODE_EN
    ret = fts_ft8006p_create_apk_debug_channel(ts_data);
    if (ret) {
        FTS_ERROR("create apk debug node fail");
    }
#endif

#if FTS_SYSFS_NODE_EN
    ret = fts_ft8006p_create_sysfs(ts_data);
    if (ret) {
        FTS_ERROR("create sysfs node fail");
    }
#endif

#if FTS_POINT_REPORT_CHECK_EN
    ret = fts_point_report_check_init(ts_data);
    if (ret) {
        FTS_ERROR("init point report check fail");
    }
#endif

    ret = fts_ft8006p_ex_mode_init(ts_data);
    if (ret) {
        FTS_ERROR("init glove/cover/charger fail");
    }

#if FTS_GESTURE_EN
    ret = fts_gesture_init(ts_data);
    if (ret) {
        FTS_ERROR("init gesture fail");
    }
#endif

#if FTS_TEST_EN
    ret = fts_ft8006p_test_init(ts_data);
    if (ret) {
        FTS_ERROR("init production test fail");
    }
#endif

#if FTS_ESDCHECK_EN
    ret = fts_ft8006p_esdcheck_init(ts_data);
    if (ret) {
        FTS_ERROR("init esd check fail");
    }
#endif

//+ add by songbinbo.wt for tp open short  20190404
#if OPEN_SHORT_PROC
	 fts_ft8006p_create_ctp_proc();
#endif
//- add by songbinbo.wt for tp open short	20190404

    ret = fts_irq_registration(ts_data);
    if (ret) {
        FTS_ERROR("request irq failed");
        goto err_irq_req;
    }

    ret = fts_ft8006p_fwupg_init(ts_data);
    if (ret) {
        FTS_ERROR("init fw upgrade fail");
    }

    tpd_load_status = 1;
    FTS_FUNC_EXIT();
    return 0;

err_irq_req:
    if (!IS_ERR_OR_NULL(ts_data->thread_tpd)) {
        kthread_stop(ts_data->thread_tpd);
        ts_data->thread_tpd = NULL;
    }
err_gpio_config:
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
    fts_power_source_exit(ts_data);
#endif
    kfree_safe(ts_data->point_buf);
    kfree_safe(ts_data->events);
err_report_buffer:
    input_unregister_device(ts_data->input_dev);
err_input_init:
    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);
err_bus_init:
    kfree_safe(ts_data->bus_tx_buf);
    kfree_safe(ts_data->bus_rx_buf);
    kfree_safe(ts_data->pdata);

    FTS_FUNC_EXIT();
    return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();

#if FTS_POINT_REPORT_CHECK_EN
    fts_point_report_check_exit(ts_data);
#endif

#if FTS_APK_NODE_EN
    fts_ft8006p_release_apk_debug_channel(ts_data);
#endif

#if FTS_SYSFS_NODE_EN
    fts_ft8006p_remove_sysfs(ts_data);
#endif

    fts_ft8006p_ex_mode_exit(ts_data);

    fts_ft8006p_fwupg_exit(ts_data);

#if FTS_TEST_EN
    fts_ft8006p_test_exit(ts_data);
#endif

#if FTS_ESDCHECK_EN
    fts_ft8006p_esdcheck_exit(ts_data);
#endif

#if FTS_GESTURE_EN
    fts_gesture_exit(ts_data);
#endif

    fts_ft8006p_bus_exit(ts_data);

    free_irq(ts_data->irq, ts_data);
    input_unregister_device(ts_data->input_dev);

    if (ts_data->ts_workqueue)
        destroy_workqueue(ts_data->ts_workqueue);


    if (!IS_ERR_OR_NULL(ts_data->thread_tpd)) {
        kthread_stop(ts_data->thread_tpd);
        ts_data->thread_tpd = NULL;
    }

#if FTS_PSENSOR_EN
    fts_proximity_exit();
#endif

#if FTS_POWER_SOURCE_CUST_EN
    fts_power_source_exit(ts_data);
#endif

    kfree_safe(ts_data->point_buf);
    kfree_safe(ts_data->events);

    kfree_safe(ts_data->pdata);
    kfree_safe(ts_data);

    FTS_FUNC_EXIT();

    return 0;
}

/*****************************************************************************
* TP Driver
*****************************************************************************/
#if FTS_MTK_OLD_TYPE
static struct mt_chip_conf fts_mt_chip_conf = {
    .setuptime = 120,
    .holdtime = 120,
    .high_time = 25,
    .low_time = 25,
    .cs_idletime = 2,
    .ulthgh_thrsh = 0,
    .cpol = 0,
    .cpha = 1,
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .tx_endian = 0,
    .rx_endian = 0,
    .com_mod = DMA_TRANSFER,/*FIFO_TRANSFER,*/
    .pause = 1, /*0 : twice cs   1: one cs*/
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};
#endif
static int fts_spi_parse_dt(struct device *dev, struct fts_ts_data *data)
{
    int ret;

    FTS_FUNC_ENTER();

    data->pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR(data->pinctrl)) {
        ret = PTR_ERR(data->pinctrl);
        FTS_ERROR("Cannot find touch pc!\n");
        goto err_out;
    }

    data->spi_default = pinctrl_lookup_state(data->pinctrl, "default");
    if (IS_ERR(data->spi_default)) {
        ret = PTR_ERR(data->spi_default);
        FTS_ERROR("Cannot find touch pinctrl default %d!", ret);
    }

    data->spi_active = pinctrl_lookup_state(data->pinctrl, "state_spi");
    if (IS_ERR(data->spi_active)) {
        ret = PTR_ERR(data->spi_active);
        FTS_ERROR("Cannot find touch pinctrl state_spi!");
        goto err_out;
    }

    FTS_FUNC_EXIT();
    return 0;

err_out:
    FTS_ERROR("error out: %d", ret);
    FTS_FUNC_EXIT();
    return ret;
}

void fts_ft8006p_spi_select_active(struct fts_ts_data *data)
{
    FTS_FUNC_ENTER();
    pinctrl_select_state(data->pinctrl, data->spi_active);
    FTS_FUNC_EXIT();
}

static int fts_ts_probe(struct spi_device *spi)
{
    int ret = 0;
    struct fts_ts_data *ts_data = NULL;

    FTS_INFO("Touch Screen(SPI BUS) driver prboe...");

    spi->mode = SPI_MODE_1;
    spi->bits_per_word = 8;
    if (spi->max_speed_hz > FTS_SPI_CLK_MAX){
        spi->max_speed_hz = FTS_SPI_CLK_MAX;
    }
    spi->max_speed_hz = SPI_CLK_HZ;
	FTS_ERROR(" spi->max_speed_hz == %d.\n",  spi->max_speed_hz);
#if FTS_MTK_OLD_TYPE
    spi->controller_data = (void *)&fts_mt_chip_conf;
#endif

    ret = spi_setup(spi);
    if (ret) {
        FTS_ERROR("spi setup fail");
        return ret;
    }

    /* malloc memory for global struct variable */
    ts_data = (struct fts_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("allocate memory for fts_ft8006p_data fail");
        return -ENOMEM;
    }

    /* configure spi pins */
    ret = fts_spi_parse_dt(&spi->dev, ts_data);
    if (ret == 0) {
        fts_ft8006p_spi_select_active(ts_data);
    }

    fts_ft8006p_data = ts_data;
    ts_data->spi = spi;
    ts_data->dev = &spi->dev;
    ts_data->log_level = 1;
    spi_set_drvdata(spi, ts_data);

    ret = fts_ts_probe_entry(ts_data);
    if (ret) {
        FTS_ERROR("Touch Screen(SPI BUS) driver probe fail");
        kfree_safe(ts_data);
        return ret;
    }

    FTS_INFO("Touch Screen(SPI BUS) driver prboe successfully");
    return 0;
}

static int fts_ts_remove(struct spi_device *spi)
{
    return fts_ts_remove_entry(spi_get_drvdata(spi));
}

static const struct spi_device_id fts_ts_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};
static const struct of_device_id fts_dt_match[] = {
    {.compatible = "focaltech_ft8006p,fts", },
    {},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct spi_driver fts_ts_driver = {
    .probe = fts_ts_probe,
    .remove = fts_ts_remove,
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(fts_dt_match),
    },
    .id_table = fts_ts_id,
};

static int fts_ts_driver_init(void)
{
    return spi_register_driver(&fts_ts_driver);
}


static int tpd_local_init(void)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    ret = fts_ts_driver_init();
    if (ret) {
        FTS_ERROR("Focaltech touch screen driver init failed!");
        return ret;
    }

    if (tpd_dts_data.use_tpd_button) {
        tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
                           tpd_dts_data.tpd_key_dim_local);
    }

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local_factory, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local_factory, 8 * 4);

    memcpy(tpd_calmat, tpd_def_calmat_local_normal, 8 * 4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local_normal, 8 * 4);
#endif

    tpd_type_cap = 1;

    FTS_FUNC_EXIT();
    return 0;
}

static void tpd_suspend(struct device *dev)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_ft8006p_data;

    FTS_FUNC_ENTER();
    if (ts_data->suspended) {
        FTS_INFO("Already in suspend state");
        return;
    }

    if (ts_data->fw_loading) {
        FTS_INFO("fw upgrade in process, can't suspend");
        return;
    }

#if FTS_PSENSOR_EN
    if (fts_proximity_suspend() == 0) {
        fts_ft8006p_release_all_finger();
        ts_data->suspended = true;
        return;
    }
#endif

#if FTS_ESDCHECK_EN
    fts_ft8006p_esdcheck_suspend();
#endif

#if FTS_GESTURE_EN
    if (fts_gesture_suspend(ts_data) == 0) {
        /* Enter into gesture mode(suspend) */
        ts_data->suspended = true;
        return;
    }
#endif


    /* TP enter sleep mode */
    ret = fts_ft8006p_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP_VALUE);
    if (ret < 0)
        FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);

    if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
        ret = fts_power_source_suspend(ts_data);
        if (ret < 0) {
            FTS_ERROR("power enter suspend fail");
        }
#endif
    }

    ts_data->suspended = true;
    FTS_FUNC_EXIT();
}

static void tpd_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = fts_ft8006p_data;

    FTS_FUNC_ENTER();
    if (!ts_data->suspended) {
        FTS_DEBUG("Already in awake state");
        return;
    }

#if FTS_PSENSOR_EN
    if (fts_proximity_resume() == 0) {
        ts_data->suspended = false;
        return;
    }
#endif

    fts_ft8006p_release_all_finger();

    if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
        fts_power_source_resume(ts_data);
#endif
        fts_ft8006p_reset_proc(200);
    }

    fts_ft8006p_tp_state_recovery(ts_data);

#if FTS_ESDCHECK_EN
    fts_ft8006p_esdcheck_resume();
#endif

#if FTS_GESTURE_EN
    if (fts_gesture_resume(ts_data) == 0) {
        ts_data->suspended = false;
        return;
    }
#endif


    ts_data->suspended = false;
    FTS_FUNC_EXIT();
}

/*****************************************************************************
*  TPD Device Driver
*****************************************************************************/
static struct tpd_driver_t tpd_device_driver = {
    .tpd_device_name = FTS_DRIVER_NAME,
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
};

/*****************************************************************************
*  Name: tpd_driver_init
*  Brief: 1. Get dts information
*         2. call tpd_driver_add to add tpd_device_driver
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int __init tpd_driver_init(void)
{
    FTS_FUNC_ENTER();
    FTS_INFO("Driver version: %s", FTS_DRIVER_VERSION);
    tpd_get_dts_info();
    if (tpd_dts_data.touch_max_num < 2)
        tpd_dts_data.touch_max_num = 2;
    else if (tpd_dts_data.touch_max_num > FTS_MAX_POINTS_SUPPORT)
        tpd_dts_data.touch_max_num = FTS_MAX_POINTS_SUPPORT;
    FTS_INFO("tpd max touch num:%d", tpd_dts_data.touch_max_num);

#if FTS_PSENSOR_EN
    fts_proximity_init();
#endif

    if (tpd_driver_add(&tpd_device_driver) < 0) {
        FTS_ERROR("[TPD]: Add FTS Touch driver failed!!");
    }

    FTS_FUNC_EXIT();
    return 0;
}

static void __exit tpd_driver_exit(void)
{
    FTS_FUNC_ENTER();
    tpd_driver_remove(&tpd_device_driver);
    FTS_FUNC_EXIT();
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
