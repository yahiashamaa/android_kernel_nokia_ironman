// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	//+bug584789 chenbocheng.wt, add, 2020/10/30, add main camera s5kgm1 and wide camera hi556 otp code
	{RONIN_S5KGM1_REAR_TXD_SENSOR_ID, 0xA0, Common_read_region},
	//+bug584789 chenbocheng.wt, add, 2020/10/30, add main camera hi4821q otp code
	{RONIN_HI4821Q_REAR_ST_SENSOR_ID, 0xA0, Common_read_region},
        //+bug584789,zhouyikuan.wt,ADD,2020/20/27,S5K3L6 eeprom bring up
	{ROGUE_S5K3L6_REAR_DC_SENSOR_ID, 0xA0, Common_read_region},
        //-bug584789,zhouyikuan.wt,ADD,2020/20/27,S5K3L6 eeprom bring up
        //+bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up
        {ROGUE_HI846_FRONT_ST_SENSOR_ID, 0x42, Common_read_region},
        //-bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up
        //+bug584789,zhouyikuan.wt,ADD,2020/11/03,HI1336 eeprom bring up
        {ROGUE_HI1336_REAR_TXD_SENSOR_ID, 0xA0, Common_read_region},
        //-bug584789,zhouyikuan.wt,ADD,2020/11/03,HI1336 eeprom bring up
        //+bug584789,zhouyikuan.wt,ADD,2020/11/04,GC8034 eeprom bring up
        {ROGUE_GC8034_FRONT_LH_SENSOR_ID, 0xA0, Common_read_region},
        //-bug584789,zhouyikuan.wt,ADD,2020/11/04,GC8034 eeprom bring up
	//+bug596770,houzhipeng.wt,ADD,2020/11/03, add macro cam eeprom
	{ROGUE_GC02M1_MICRO_LH_SENSOR_ID, 0xA4, Common_read_region},
	{RONIN_GC02M1_MICRO_LH_SENSOR_ID, 0xA4, Common_read_region},
	//-bug596770,houzhipeng.wt,ADD,2020/11/03, add macro cam eeprom
	//+bug596770,zhanghengyuan.wt,ADD,2020/11/05, add macro cxt gc02m1 cam eeprom
	{ROGUE_GC02M1_MICRO_CXT_SENSOR_ID, 0xA4, Common_read_region},
	{RONIN_GC02M1_MICRO_CXT_SENSOR_ID, 0xA4, Common_read_region},
	//-bug596770,zhanghengyuan.wt,ADD,2020/11/05, add macro cxt gc02m1 cam eeprom
	//+bug596770,liuxingyu.wt,ADD,2020/11/10, add gc5035 gc8035 cam eeprom
	{RONIN_GC5035_WIDE_ST_SENSOR_ID, 0xA2, Common_read_region},
	{RONIN_GC8034_FRONT_LH_SENSOR_ID, 0xA0, Common_read_region},
	//-bug596770,liuxingyu.wt,ADD,2020/11/10, add gc5035 gc8035 cam eeprom
	//+bug591033 cdzk, modify, 2020/10/27, the otp porting
	{WR_S5K3L6_REAR_DC_SENSOR_ID, 0xA0, Common_read_region},
	{WR_HI1336_REAR_QH_SENSOR_ID, 0xA0, Common_read_region},
	{WR_HI556_FRONT_LY_SENSOR_ID, 0x40, Common_read_region},
	{WR_GC5035_FRONT_QH_SENSOR_ID, 0x7E, Common_read_region},
	//-bug591033 cdzk, modify, 2020/10/27, the otp porting
	{IMX230_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{OV8858_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*B+B*/
	{S5K2P7_SENSOR_ID, 0xA0, Common_read_region},
	{OV8856_SENSOR_ID, 0xA0, Common_read_region},
	/*61*/
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3L8_SENSOR_ID, 0xA0, Common_read_region},
	{S5K5E8YX_SENSOR_ID, 0xA2, Common_read_region},
	/*99*/
	{IMX258_SENSOR_ID, 0xA0, Common_read_region},
	{IMX258_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*97*/
	{OV23850_SENSOR_ID, 0xA0, Common_read_region},
	{OV23850_SENSOR_ID, 0xA8, Common_read_region},
	{S5K3M2_SENSOR_ID, 0xA0, Common_read_region},
	/*55*/
	{S5K2P8_SENSOR_ID, 0xA2, Common_read_region},
	{S5K2P8_SENSOR_ID, 0xA0, Common_read_region},
	{OV8858_SENSOR_ID, 0xA2, Common_read_region},
	/* Others */
	{S5K2X8_SENSOR_ID, 0xA0, Common_read_region},
	{IMX377_SENSOR_ID, 0xA0, Common_read_region},
	{IMX214_SENSOR_ID, 0xA0, Common_read_region},
	{IMX214_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX486_SENSOR_ID, 0xA8, Common_read_region},
	{OV12A10_SENSOR_ID, 0xA8, Common_read_region},
	{OV13855_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3L8_SENSOR_ID, 0xA0, Common_read_region},
	{HI556_SENSOR_ID, 0x51, Common_read_region},
	{S5K5E8YX_SENSOR_ID, 0x5a, Common_read_region},
	{S5K5E8YXREAR2_SENSOR_ID, 0x5a, Common_read_region},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


