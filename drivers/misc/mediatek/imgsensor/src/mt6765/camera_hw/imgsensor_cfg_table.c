// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "kd_imgsensor.h"

#include "regulator/regulator.h"
#include "gpio/gpio.h"
/*#include "mt6306/mt6306.h"*/
#include "mclk/mclk.h"



#include "imgsensor_cfg_table.h"

enum IMGSENSOR_RETURN
	(*hw_open[IMGSENSOR_HW_ID_MAX_NUM])(struct IMGSENSOR_HW_DEVICE **) = {
	imgsensor_hw_regulator_open,
	imgsensor_hw_gpio_open,
	/*imgsensor_hw_mt6306_open,*/
	imgsensor_hw_mclk_open
};

struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
                        //bug584789,zhouyikuan.wt,MODIFY,2020/10/12,rogue_dongci_s5k3l6 sensor bringup
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD},
                        //+bug584789,zhouyikuan.wt,ADD,2020/20/22,CN3927E af bring up
                        {IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AFVDD},
                        //-bug584789,zhouyikuan.wt,ADD,2020/20/22,CN3927E af bring up
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
                        //+bug584789,zhouyikuan.wt,MODIFY,2020/10/12,rogue_st_hi846 sensor bringup
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
                        //-bug584789,zhouyikuan.wt,MODIFY,2020/10/12,rogue_st_hi846 sensor bringup
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef MIPI_SWITCH
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN3,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
#ifdef MIPI_SWITCH
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_EN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL},
#endif
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};

struct IMGSENSOR_HW_POWER_SEQ platform_power_sequence[] = {
#ifdef MIPI_SWITCH
	{
		PLATFORM_POWER_SEQ_NAME,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		},
		IMGSENSOR_SENSOR_IDX_MAIN2,
	},
	{
		PLATFORM_POWER_SEQ_NAME,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
		},
		IMGSENSOR_SENSOR_IDX_MAIN3,
	},
#endif

	{NULL}
};


/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {

//+bug584789,chenbocheng.wt,MODIFY,2020/10/13,main s5kgm1 and depth gc02m1b sensor bringup
#if defined(RONIN_S5KGM1_REAR_TXD_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_S5KGM1_REAR_TXD_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{RST, Vol_High, 15},
			{SensorMCLK, Vol_High, 0},
		},
	},
#endif

#if defined(RONIN_HI4821Q_REAR_ST_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_HI4821Q_REAR_ST_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			//+bug584789,lintaicheng.wt,MODIFY,2020/12/15,update ronin HI4821Q rear sensor power sequence
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			//-bug584789,lintaicheng.wt,MODIFY,2020/12/15,update ronin HI4821Q rear sensor power sequence
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High, 10},
			{RST, Vol_High, 10},
		},
	},
#endif

#if defined(RONIN_HI846_FRONT_ST_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_HI846_FRONT_ST_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 10},
			{RST, Vol_Low, 10},
			{RST, Vol_High, 1},
		},
	},
#endif

#if defined(RONIN_GC8034_FRONT_LH_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_GC8034_FRONT_LH_MIPI_RAW,
		{
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			//+bug584789,lintaicheng.wt,MODIFY,2020/12/12,update ronin GC8034 front sensor power sequence
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 0},
			//-bug584789,lintaicheng.wt,MODIFY,2020/12/12,update ronin GC8034 front sensor power sequence
		},
	},
#endif

#if defined(RONIN_GC02M1B_DEP_CXT_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_GC02M1B_DEP_CXT_MIPI_RAW,
		{
			{PDN, Vol_Low, 15},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
		},
	},
#endif

#if defined(RONIN_BF2253_DEP_LH_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_BF2253_DEP_LH_MIPI_RAW,
		{
			{PDN, Vol_High, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			//+bug584789,lintaicheng.wt,ADD,2020/12/15,update ronin BF2253 dep sensor power sequence
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 1, Vol_High, 1},
			{SensorMCLK, Vol_High, 0},
			//-bug584789,lintaicheng.wt,ADD,2020/12/15,update ronin BF2253 dep sensor power sequence
		},
	},
#endif
//-bug584789,chenbocheng.wt,MODIFY,2020/10/13,main s5kgm1 and depth gc02m1b sensor bringup

//+bug584789,zhouyikuan.wt,MODIFY,2020/10/12,rogue_dongci_s5k3l6 sensor bringup
#if defined(ROGUE_S5K3L6_REAR_DC_MIPI_RAW)
        {
                SENSOR_DRVNAME_ROGUE_S5K3L6_REAR_DC_MIPI_RAW,
                {
                        //+bug584789,zhouyikuan.wt,MODIFY,2020/12/10,update rogue_dongci_s5k3l6 power sequence
                        {RST, Vol_Low, 0},
                        {DOVDD, Vol_1800, 1},
                        {AVDD, Vol_High, 1},
                        {DVDD, Vol_High, 1},
                        {AFVDD, Vol_2800, 5},
                        {RST, Vol_High, 1},
                        {SensorMCLK, Vol_High, 0},
                        //-bug584789,zhouyikuan.wt,MODIFY,2020/12/10,update rogue_dongci_s5k3l6 power sequence
                },
        },
#endif
//-bug584789,zhouyikuan.wt,MODIFY,2020/10/12,rogue_dongci_s5k3l6 sensor bringup
//+bug584789,zhouyikuan.wt,MODIFY,2020/10/12,rogue_st_hi846 sensor bringup
#if defined(ROGUE_HI846_FRONT_ST_MIPI_RAW)
        {
                SENSOR_DRVNAME_ROGUE_HI846_FRONT_ST_MIPI_RAW,
                {
                        {DOVDD, Vol_1800, 1},
                        {DVDD, Vol_1200, 1},
                        {AVDD, Vol_2800, 1},
                        {SensorMCLK, Vol_High, 10},
                        {RST, Vol_Low, 10},
                        {RST, Vol_High, 1},
                },
        },
#endif
//+bug584789,zhouyikuan.wt,MODIFY,2020/10/12,rogue_st_hi846 sensor bringup
//+bug584789,zhanghengyuan.wt,MODIFY,2020/10/14,rogue depth gc02m1b sensor bringup
#if defined(ROGUE_GC02M1B_DEP_CXT_MIPI_RAW)
	{
		SENSOR_DRVNAME_ROGUE_GC02M1B_DEP_CXT_MIPI_RAW,
		{
			{PDN, Vol_Low, 15},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
		},
	},
#endif
//-bug584789,zhanghengyuan.wt,MODIFY,2020/10/14,rogue depth gc02m1b sensor bringup
//+bug584789,zhanghengyuan.wt,MODIFY,2020/10/15,rogue micro gc02m1 sensor bringup
#if defined(ROGUE_GC02M1_MICRO_LH_MIPI_RAW)
	{
		SENSOR_DRVNAME_ROGUE_GC02M1_MICRO_LH_MIPI_RAW,
			{
			//{PDN, Vol_Low, 0},
			{RST, Vol_Low, 5},
			{SensorMCLK, Vol_High, 2},
			//{DVDD, Vol_1100, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			//{PDN, Vol_High, 1},
			{RST, Vol_High, 2},
			},

	},
#endif
#if defined(ROGUE_GC02M1_MICRO_CXT_MIPI_RAW)
	{
		SENSOR_DRVNAME_ROGUE_GC02M1_MICRO_CXT_MIPI_RAW,
			{
			//{PDN, Vol_Low, 0},
			{RST, Vol_Low, 5},
			{SensorMCLK, Vol_High, 2},
			//{DVDD, Vol_1100, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			//{PDN, Vol_High, 1},
			{RST, Vol_High, 2},
			},

	},
#endif
//-bug584789,zhanghengyuan.wt,MODIFY,2020/10/15,rogue micro gc02m1 sensor bringup
//+bug584789,zhanghengyuan.wt,MODIFY,2020/10/21,front gc8034 and depth bf2253 sensor bringup
#if defined(ROGUE_GC8034_FRONT_LH_MIPI_RAW)
	{
		SENSOR_DRVNAME_ROGUE_GC8034_FRONT_LH_MIPI_RAW,
		{
                        //+bug584789,zhouyikuan.wt,MODIFY,2020/12/10,update rogue_lh_gc8034 power sequence
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
                        {SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 1},
                        //+bug584789,zhouyikuan.wt,MODIFY,2020/12/10,update rogue_lh_gc8034 power sequence
		},
	},
#endif
#if defined(ROGUE_BF2253_DEP_LH_MIPI_RAW)
	{
		SENSOR_DRVNAME_ROGUE_BF2253_DEP_LH_MIPI_RAW,
		{
			//+bug596770,liuxiangyin.wt,MODIFY,2020/12/17,update rogue bf2253 depth sensor power sequence
			{PDN, Vol_High, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{PDN, Vol_Low, 1, Vol_High, 1},
			{SensorMCLK, Vol_High, 0},
			//-bug596770,liuxiangyin.wt,MODIFY,2020/12/17,update rogue bf2253 depth sensor power sequence
		},
	},
#endif
//-bug584789,zhanghengyuan.wt,MODIFY,2020/10/21,front gc8034 and depth bf2253 sensor bringup
//+bug584789,zhanghengyuan.wt,MODIFY,2020/10/21,rear hi1336 sensor bringup
#if defined(ROGUE_HI1336_REAR_TXD_MIPI_RAW)
	{
		SENSOR_DRVNAME_ROGUE_HI1336_REAR_TXD_MIPI_RAW,
		{
                        //+bug584789,zhouyikuan.wt,MODIFY,2020/12/10,update rogue_txd_hi1336 power sequence
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_High, 1},
			{DVDD, Vol_High, 1},
			{AFVDD, Vol_2800, 5},
                        {SensorMCLK, Vol_High, 0},
			{RST, Vol_High, 1},
                        //+bug584789,zhouyikuan.wt,MODIFY,2020/12/10,update rogue_txd_hi1336 power sequence
		},
	},
#endif
//-bug584789,zhanghengyuan.wt,MODIFY,2020/10/21,rear hi1336 sensor bringup


//+bug584789,liuxingyu.wt,MODIFY,2020/10/13,rogue hi556 gc02m1 gc5035 sensor bringup
#if defined(RONIN_HI556_WIDE_DC_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_HI556_WIDE_DC_MIPI_RAW,
		{
			//{PDN, Vol_Low, 0},
			{RST, Vol_Low, 5},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 5},
			//{PDN, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(RONIN_GC02M1_MICRO_LH_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_GC02M1_MICRO_LH_MIPI_RAW,
		{
			{PDN, Vol_Low, 15},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			//{RST, Vol_Low, 0},
			//{RST, Vol_High, 0}
		},
	},
#endif
#if defined(RONIN_GC02M1_MICRO_CXT_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_GC02M1_MICRO_CXT_MIPI_RAW,
		{
			{PDN, Vol_Low, 15},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			//{RST, Vol_Low, 0},
			//{RST, Vol_High, 0}
		},

	},
#endif
#if defined(RONIN_GC5035_WIDE_ST_MIPI_RAW)
	{
		SENSOR_DRVNAME_RONIN_GC5035_WIDE_ST_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_Low, 10},
			{RST, Vol_High, 1},
			//{PDN, Vol_Low, 0},
			//{PDN, Vol_High, 0},
			{SensorMCLK, Vol_High, 1},
		},

	},
#endif
//-bug584789,liuxingyu.wt,MODIFY,2020/10/13,rogue hi556 gc02m1 gc5035 sensor bringup
//+ bug591033,cdzk,20201022,modify,main,main2,sub camera bring up
#if defined(WR_S5K3L6_REAR_DC_MIPI_RAW)
	{
		SENSOR_DRVNAME_WR_S5K3L6_REAR_DC_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 5},
			{AVDD, Vol_2800, 1},
			//{AFVDD, Vol_2800, 1},
			//{AFVDD_EN, Vol_Low, 0},
			//{AFVDD_EN, Vol_High, 1},
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 10},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(WR_HI1336_REAR_QH_MIPI_RAW)
	{
		SENSOR_DRVNAME_WR_HI1336_REAR_QH_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			//{AFVDD, Vol_2800, 1},
			//{PDN, Vol_Low, 0},
			//{PDN, Vol_High, 0},
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 10},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(WR_HI556_FRONT_LY_MIPI_RAW)
	{
		SENSOR_DRVNAME_WR_HI556_FRONT_LY_MIPI_RAW,
		{
			//{PDN, Vol_Low, 0},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{SensorMCLK, Vol_High, 1},
			//{PDN, Vol_High, 1},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(WR_GC5035_FRONT_QH_MIPI_RAW)
	{
		SENSOR_DRVNAME_WR_GC5035_FRONT_QH_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_Low, 10},
			{RST, Vol_High, 1},
			//{PDN, Vol_Low, 0},
			//{PDN, Vol_High, 0},
			{SensorMCLK, Vol_High, 1},
		},
	},
#endif
#if defined(WR_GC2375H_DEPTH_CXT_MIPI_RAW)
	{
		SENSOR_DRVNAME_WR_GC2375H_DEPTH_CXT_MIPI_RAW,
		{
			{PDN, Vol_High, 1},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			//{DVDD, Vol_1200, 10},
			{PDN, Vol_Low, 1, Vol_High, 1},
			//{PDN, Vol_Low, 0},
			//{PDN, Vol_High, 0},
		},
	},
#endif
#if defined(WR_BF2253_DEPTH_QH_MIPI_RAW)
	{
		SENSOR_DRVNAME_WR_BF2253_DEPTH_QH_MIPI_RAW,
		{
			{PDN, Vol_High, 1},
			{DOVDD, Vol_1800, 1},
			//{DVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 1, Vol_High, 1},
			//{RST, Vol_Low, 5},
			//{RST, Vol_High, 0}
		},
	},
#endif
//- bug591033,cdzk,20201022,modify,main,main2,sub camera bring up
#if 0
#if defined(IMX398_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX398_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(OV23850_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV23850_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX386_MIPI_RAW,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX386_MIPI_MONO,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif

#if defined(IMX338_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX338_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(S5K4E6_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4E6_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2900, 0},
			{DVDD, Vol_1200, 2},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P8SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P8SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2T7SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2T7SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(IMX230_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX230_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(S5K3M2_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M2_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P3SX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P3SX_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K5E2YA_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K4ECGX_MIPI_YUV)
	{
		SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV,
		{
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV16880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5K2P7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2P8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX258_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX258_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX377_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX377_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8858_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8858_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV8856_MIPI_RAW)
	{SENSOR_DRVNAME_OV8856_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5K2X8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2X8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX214_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX214_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX214_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX214_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(S5K3L8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3L8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX362_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX362_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K2L7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2L7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 3},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX318_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX318_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8865_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8865_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX219_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX219_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1000, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3M3_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M3_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW_2)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW_2,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV20880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV20880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#endif
	/* add new sensor before this line */
	{NULL,},
};

