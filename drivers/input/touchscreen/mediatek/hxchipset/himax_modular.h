/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for modularize functions
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __HIMAX_MODULAR_H__
#define __HIMAX_MODULAR_H__

#include "himax_modular_table.h"

static bool (*this_detect_fp)(void);

#ifndef HX_NO_KSYM_LOOKUP
static void himax_add_chip_dt(bool (*detect_fp)(void))
{
	int32_t idx;
	struct himax_chip_entry *entry;

	this_detect_fp = detect_fp;
	idx = himax_get_ksym_idx();
	if (idx < 0) {
		/*TODO: No entry, handle this error*/
		E("%s: no entry exist, please insert ic module first!", __func__);
	} else {
		entry = (void *)kallsyms_lookup_name(himax_ksym_lookup[idx]);
		if (/*!(entry->core_chip_dt)*/isEmpty(idx) == 1) {
			entry->core_chip_dt = kcalloc(HX_DRIVER_MAX_IC_NUM,
				sizeof(struct himax_chip_detect), GFP_KERNEL);
			entry->hx_ic_dt_num = 0;
		}
		entry->core_chip_dt[entry->hx_ic_dt_num++].fp_chip_detect = detect_fp;
	}
}

#else

static void himax_add_chip_dt(bool (*detect_fp)(void))
{
	this_detect_fp = detect_fp;
	if (himax_ksym_lookup.core_chip_dt == NULL) {
		himax_ksym_lookup.core_chip_dt = kcalloc(HX_DRIVER_MAX_IC_NUM,
			sizeof(struct himax_chip_detect), GFP_KERNEL);
		himax_ksym_lookup.hx_ic_dt_num = 0;
	}

	himax_ksym_lookup.core_chip_dt[himax_ksym_lookup.hx_ic_dt_num].fp_chip_detect = detect_fp;
	himax_ksym_lookup.hx_ic_dt_num++;
}

#endif

static void free_chip_dt_table(void)
{
	int i, j, idx;
	struct himax_chip_entry *entry;

	idx = himax_get_ksym_idx();
	if (idx >= 0) {
		if (isEmpty(idx) != 0) {
			I("%s: no chip registered or entry has been clean up, leave safetly.\n",
				__func__);
			return;
		}
		entry = get_chip_entry_by_index(idx);

		for (i = 0; i < entry->hx_ic_dt_num; i++) {
			if (entry->core_chip_dt[i].fp_chip_detect == this_detect_fp) {
				if (i == (entry->hx_ic_dt_num - 1)) {
					entry->core_chip_dt[i].fp_chip_detect = NULL;
					entry->hx_ic_dt_num = 0;
				} else {
					for (j = i; j < entry->hx_ic_dt_num; j++)
						entry->core_chip_dt[i].fp_chip_detect = entry->core_chip_dt[j].fp_chip_detect;

					entry->core_chip_dt[j].fp_chip_detect = NULL;
					entry->hx_ic_dt_num--;
				}
			}
		}
		if (entry->hx_ic_dt_num == 0) {
			kfree(entry->core_chip_dt);
			entry->core_chip_dt = NULL;
		}
	}
}

#ifdef HX_NO_KSYM_LOOKUP
#define setup_symbol(sym)	({kp_##sym = &(sym); kp_##sym; })
#define setup_symbol_func(sym)	({kp_##sym = (sym); kp_##sym; })
#else
#define setup_symbol(sym)	({kp_##sym = (void *)kallsyms_lookup_name(#sym); kp_##sym; })
#define setup_symbol_func(sym)	setup_symbol(sym)
#endif

#define assert_on_symbol(sym)	do { \
					if (!setup_symbol(sym)) { \
						E("%s: setup %s failed!\n", __func__, #sym); \
						ret = -1; \
					} \
				} while (0)

#define assert_on_symbol_func(sym)	do { \
					if (!setup_symbol_func(sym)) { \
						E("%s: setup %s failed!\n", __func__, #sym); \
						ret = -1; \
					} \
				} while (0)
#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
static struct fw_operation **kp_pfw_op;
static struct ic_operation **kp_pic_op;
static struct driver_operation **kp_pdriver_op;
#endif
static struct himax_chip_detect **kp_g_core_chip_dt;

#if defined(HX_ZERO_FLASH)
static struct zf_operation **kp_pzf_op;
static int *kp_G_POWERONOF;
#endif

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
	static char **kp_i_CTPM_firmware_name;
#endif
#endif

static unsigned char *kp_IC_CHECKSUM;

#ifdef HX_ESD_RECOVERY
static u8 *kp_HX_ESD_RESET_ACTIVATE;
#endif

static unsigned long *kp_FW_VER_MAJ_FLASH_ADDR;
static unsigned long *kp_FW_VER_MIN_FLASH_ADDR;
static unsigned long *kp_CFG_VER_MAJ_FLASH_ADDR;
static unsigned long *kp_CFG_VER_MIN_FLASH_ADDR;
static unsigned long *kp_CID_VER_MAJ_FLASH_ADDR;
static unsigned long *kp_CID_VER_MIN_FLASH_ADDR;

static unsigned long *kp_FW_VER_MAJ_FLASH_LENG;
static unsigned long *kp_FW_VER_MIN_FLASH_LENG;
static unsigned long *kp_CFG_VER_MAJ_FLASH_LENG;
static unsigned long *kp_CFG_VER_MIN_FLASH_LENG;
static unsigned long *kp_CID_VER_MAJ_FLASH_LENG;
static unsigned long *kp_CID_VER_MIN_FLASH_LENG;

#ifdef HX_AUTO_UPDATE_FW
	static int *kp_g_i_FW_VER;
	static int *kp_g_i_CFG_VER;
	static int *kp_g_i_CID_MAJ;
	static int *kp_g_i_CID_MIN;
	static unsigned char **kp_i_CTPM_FW;
#endif

#ifdef HX_TP_PROC_2T2R
	static bool *kp_Is_2T2R;
#endif

#ifdef HX_USB_DETECT_GLOBAL
	static void (*kp_himax_cable_detect_func)(bool force_renew);
#endif

#ifdef HX_RST_PIN_FUNC
	static void (*kp_himax_rst_gpio_set)(int pinnum, uint8_t value);
#endif

static struct himax_ts_data **kp_private_ts;
static struct himax_core_fp *kp_g_core_fp;
static struct himax_ic_data **kp_ic_data;

#if defined(__HIMAX_HX852xH_MOD__) || defined(__HIMAX_HX852xG_MOD__)
#if defined(__HIMAX_HX852xG_MOD__)
static struct on_driver_operation **kp_on_pdriver_op;
#endif
static void (*kp_himax_mcu_on_cmd_init)(void);
static int (*kp_himax_mcu_on_cmd_struct_init)(void);
static void (*kp_himax_on_parse_assign_cmd)(uint32_t addr, uint8_t *cmd, int len);
#else
static void (*kp_himax_mcu_in_cmd_init)(void);
static int (*kp_himax_mcu_in_cmd_struct_init)(void);
static void (*kp_himax_in_parse_assign_cmd)(uint32_t addr, uint8_t *cmd, int len);
#endif

static int *kp_irq_enable_count;
static int (*kp_himax_bus_read)(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry);
static int (*kp_himax_bus_write)(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry);
static int (*kp_himax_bus_write_command)(uint8_t command, uint8_t toRetry);
static int (*kp_himax_bus_master_write)(uint8_t *data, uint32_t length, uint8_t toRetry);
static void (*kp_himax_int_enable)(int enable);
static int (*kp_himax_ts_register_interrupt)(void);
static uint8_t (*kp_himax_int_gpio_read)(int pinnum);
static int (*kp_himax_gpio_power_config)(struct himax_i2c_platform_data *pdata);

#ifdef HX_NO_KSYM_LOOKUP
#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
extern struct fw_operation *pfw_op;
extern struct ic_operation *pic_op;
extern struct driver_operation *pdriver_op;
#endif
extern struct himax_chip_detect *g_core_chip_dt;

#if defined(HX_ZERO_FLASH)
extern struct zf_operation *pzf_op;
extern int G_POWERONOF;
#endif

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
	extern char *i_CTPM_firmware_name;
#endif
#endif

extern unsigned char IC_CHECKSUM;

#ifdef HX_ESD_RECOVERY
extern u8 HX_ESD_RESET_ACTIVATE;
#endif

extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long CFG_VER_MIN_FLASH_ADDR;
extern unsigned long CID_VER_MAJ_FLASH_ADDR;
extern unsigned long CID_VER_MIN_FLASH_ADDR;

extern unsigned long FW_VER_MAJ_FLASH_LENG;
extern unsigned long FW_VER_MIN_FLASH_LENG;
extern unsigned long CFG_VER_MAJ_FLASH_LENG;
extern unsigned long CFG_VER_MIN_FLASH_LENG;
extern unsigned long CID_VER_MAJ_FLASH_LENG;
extern unsigned long CID_VER_MIN_FLASH_LENG;

#ifdef HX_AUTO_UPDATE_FW
	extern int g_i_FW_VER;
	extern int g_i_CFG_VER;
	extern int g_i_CID_MAJ;
	extern int g_i_CID_MIN;
	extern unsigned char *i_CTPM_FW;
#endif

#ifdef HX_TP_PROC_2T2R
	extern bool Is_2T2R;
#endif

#ifdef HX_USB_DETECT_GLOBAL
	extern void (himax_cable_detect_func)(bool force_renew);
#endif

#ifdef HX_RST_PIN_FUNC
	extern void (himax_rst_gpio_set)(int pinnum, uint8_t value);
#endif

extern struct himax_ts_data *private_ts;
extern struct himax_core_fp g_core_fp;
extern struct himax_ic_data *ic_data;

#if defined(__HIMAX_HX852xH_MOD__) || defined(__HIMAX_HX852xG_MOD__)
#if defined(__HIMAX_HX852xG_MOD__)
extern struct on_driver_operation *on_pdriver_op;
#endif
extern void (himax_mcu_on_cmd_init)(void);
extern int (himax_mcu_on_cmd_struct_init)(void);
extern void (himax_on_parse_assign_cmd)(uint32_t addr, uint8_t *cmd, int len);
#else
extern void (himax_mcu_in_cmd_init)(void);
extern int (himax_mcu_in_cmd_struct_init)(void);
extern void (himax_in_parse_assign_cmd)(uint32_t addr, uint8_t *cmd, int len);
#endif

extern int irq_enable_count;
extern int (himax_bus_read)(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry);
extern int (himax_bus_write)(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry);
extern int (himax_bus_write_command)(uint8_t command, uint8_t toRetry);
extern int (himax_bus_master_write)(uint8_t *data, uint32_t length, uint8_t toRetry);
extern void (himax_int_enable)(int enable);
extern int (himax_ts_register_interrupt)(void);
extern uint8_t (himax_int_gpio_read)(int pinnum);
extern int (himax_gpio_power_config)(struct himax_i2c_platform_data *pdata);
#endif

static int32_t himax_ic_setup_external_symbols(void)
{
	int32_t ret = 0;

#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
	assert_on_symbol(pfw_op);
	assert_on_symbol(pic_op);
	assert_on_symbol(pdriver_op);
#endif
	assert_on_symbol(g_core_chip_dt);

	assert_on_symbol(private_ts);
	assert_on_symbol(g_core_fp);
	assert_on_symbol(ic_data);

#if defined(__HIMAX_HX852xH_MOD__) || defined(__HIMAX_HX852xG_MOD__)
#if defined(__HIMAX_HX852xG_MOD__)
	assert_on_symbol(on_pdriver_op);
#endif
	assert_on_symbol_func(himax_mcu_on_cmd_init);
	assert_on_symbol_func(himax_mcu_on_cmd_struct_init);
	assert_on_symbol_func(himax_on_parse_assign_cmd);
#else
	assert_on_symbol_func(himax_mcu_in_cmd_init);
	assert_on_symbol_func(himax_mcu_in_cmd_struct_init);
	assert_on_symbol_func(himax_in_parse_assign_cmd);
#endif

	assert_on_symbol(irq_enable_count);
	assert_on_symbol_func(himax_bus_read);
	assert_on_symbol_func(himax_bus_write);
	assert_on_symbol_func(himax_bus_write_command);
	assert_on_symbol_func(himax_bus_master_write);
	assert_on_symbol_func(himax_int_enable);
	assert_on_symbol_func(himax_ts_register_interrupt);
	assert_on_symbol_func(himax_int_gpio_read);
	assert_on_symbol_func(himax_gpio_power_config);

#if defined(HX_ZERO_FLASH)
	assert_on_symbol(pzf_op);
	assert_on_symbol(G_POWERONOF);
#endif

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
	assert_on_symbol(i_CTPM_firmware_name);
#endif
#endif
	assert_on_symbol(IC_CHECKSUM);

	assert_on_symbol(FW_VER_MAJ_FLASH_ADDR);
	assert_on_symbol(FW_VER_MIN_FLASH_ADDR);
	assert_on_symbol(CFG_VER_MAJ_FLASH_ADDR);
	assert_on_symbol(CFG_VER_MIN_FLASH_ADDR);
	assert_on_symbol(CID_VER_MAJ_FLASH_ADDR);
	assert_on_symbol(CID_VER_MIN_FLASH_ADDR);

	assert_on_symbol(FW_VER_MAJ_FLASH_LENG);
	assert_on_symbol(FW_VER_MIN_FLASH_LENG);
	assert_on_symbol(CFG_VER_MAJ_FLASH_LENG);
	assert_on_symbol(CFG_VER_MIN_FLASH_LENG);
	assert_on_symbol(CID_VER_MAJ_FLASH_LENG);
	assert_on_symbol(CID_VER_MIN_FLASH_LENG);

#ifdef HX_AUTO_UPDATE_FW
	assert_on_symbol(g_i_FW_VER);
	assert_on_symbol(g_i_CFG_VER);
	assert_on_symbol(g_i_CID_MAJ);
	assert_on_symbol(g_i_CID_MIN);
	assert_on_symbol(i_CTPM_FW);
#endif

#ifdef HX_TP_PROC_2T2R
	assert_on_symbol(Is_2T2R);
#endif

#ifdef HX_USB_DETECT_GLOBAL
	assert_on_symbol_func(himax_cable_detect_func);
#endif

#ifdef HX_RST_PIN_FUNC
	assert_on_symbol_func(himax_rst_gpio_set);
#endif

#ifdef HX_ESD_RECOVERY
	assert_on_symbol(HX_ESD_RESET_ACTIVATE);
#endif
	return ret;
}


#endif
