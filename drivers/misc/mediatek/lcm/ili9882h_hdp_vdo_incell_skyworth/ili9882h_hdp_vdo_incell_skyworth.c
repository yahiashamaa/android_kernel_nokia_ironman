/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID (0x80)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE								0
#define FRAME_WIDTH											(720)
#define FRAME_HEIGHT										(1600)

#define LCM_PHYSICAL_WIDTH									(67608)
#define LCM_PHYSICAL_HEIGHT									(142728)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },//bug568912, sheqihao.wt, MODIFY, 20200703, fix LCD panel Flicker
	{0x10, 0, {} },
	{REGFLAG_DELAY, 65, {} },
	//{0xB1, 1, {0x01} }
};

//+bug568878, sheqihao.wt, MODIFY, 20200709, fix LCD Gamma test fail
static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF,0x03,{0x98,0x82,0x01}},
	{0x00,0x01,{0x42}},  //STV A rise  41
	{0x01,0x01,{0x33}},  //STV A overlap  32
	{0x02,0x01,{0x01}},
	{0x03,0x01,{0x00}},
	{0x04,0x01,{0x02}}, //STV B rise  03
	{0x05,0x01,{0x31}},  //STV B overlap  30
	{0x07,0x01,{0x00}},
	{0x08,0x01,{0x80}},  //CLK A rise   01
	{0x09,0x01,{0x81}},  //CLK A fall   80
	{0x0a,0x01,{0x71}},  //CLK A overlap  70
	{0x0b,0x01,{0x00}},
	{0x0c,0x01,{0x70}},
	{0x0d,0x01,{0x70}},

	{0x0e,0x01,{0x05}},  //DESENSE_0827    //Panda 00
	{0x0f,0x01,{0x05}},  //DESENSE_0827	 //Panda 00
	{0x10,0x01,{0x1F}},  //DESENSE_0827
	{0x11,0x01,{0x1F}},  //DESENSE_0827
	{0x12,0x01,{0x05}},  //DESENSE_0827
	{0x1E,0x01,{0x1F}},  //DESENSE_0827
	{0x1F,0x01,{0x1F}},  //DESENSE_0827
	{0x20,0x01,{0x05}},  //DESENSE_0827



	{0x28,0x01,{0x88}},
	{0x29,0x01,{0x88}},
	{0x2a,0x01,{0x00}},
	{0x2b,0x01,{0x00}},
	{0x31,0x01,{0x07}},
	{0x32,0x01,{0x0c}},
	{0x33,0x01,{0x22}},
	{0x34,0x01,{0x23}},
	{0x35,0x01,{0x07}},
	{0x36,0x01,{0x08}},
	{0x37,0x01,{0x16}},
	{0x38,0x01,{0x14}},
	{0x39,0x01,{0x12}},
	{0x3a,0x01,{0x10}},
	{0x3b,0x01,{0x21}},
	{0x3c,0x01,{0x07}},
	{0x3d,0x01,{0x07}},
	{0x3e,0x01,{0x07}},
	{0x3f,0x01,{0x07}},
	{0x40,0x01,{0x07}},
	{0x41,0x01,{0x07}},
	{0x42,0x01,{0x07}},
	{0x43,0x01,{0x07}},
	{0x44,0x01,{0x07}},
	{0x45,0x01,{0x07}},
	{0x46,0x01,{0x07}},                                
	{0x47,0x01,{0x07}},
	{0x48,0x01,{0x0d}},
	{0x49,0x01,{0x22}},
	{0x4a,0x01,{0x23}},
	{0x4b,0x01,{0x07}},
	{0x4c,0x01,{0x09}},
	{0x4d,0x01,{0x17}},
	{0x4e,0x01,{0x15}},
	{0x4f,0x01,{0x13}},
	{0x50,0x01,{0x11}},
	{0x51,0x01,{0x21}},
	{0x52,0x01,{0x07}},
	{0x53,0x01,{0x07}},
	{0x54,0x01,{0x07}},
	{0x55,0x01,{0x07}},
	{0x56,0x01,{0x07}},
	{0x57,0x01,{0x07}},
	{0x58,0x01,{0x07}},
	{0x59,0x01,{0x07}},
	{0x5a,0x01,{0x07}},
	{0x5b,0x01,{0x07}},
	{0x5c,0x01,{0x07}},                                   
	{0x61,0x01,{0x07}},
	{0x62,0x01,{0x0d}},
	{0x63,0x01,{0x22}},
	{0x64,0x01,{0x23}},
	{0x65,0x01,{0x07}},
	{0x66,0x01,{0x09}},
	{0x67,0x01,{0x11}},
	{0x68,0x01,{0x13}},
	{0x69,0x01,{0x15}},
	{0x6a,0x01,{0x17}},
	{0x6b,0x01,{0x21}},
	{0x6c,0x01,{0x07}},
	{0x6d,0x01,{0x07}},
	{0x6e,0x01,{0x07}},
	{0x6f,0x01,{0x07}},
	{0x70,0x01,{0x07}},
	{0x71,0x01,{0x07}},
	{0x72,0x01,{0x07}},
	{0x73,0x01,{0x07}},
	{0x74,0x01,{0x07}},
	{0x75,0x01,{0x07}},
	{0x76,0x01,{0x07}},
                                       
	{0x77,0x01,{0x07}},
	{0x78,0x01,{0x0c}},
	{0x79,0x01,{0x22}},
	{0x7a,0x01,{0x23}},
	{0x7b,0x01,{0x07}},
	{0x7c,0x01,{0x08}},
	{0x7d,0x01,{0x10}},
	{0x7e,0x01,{0x12}},
	{0x7f,0x01,{0x14}},
	{0x80,0x01,{0x16}},
	{0x81,0x01,{0x21}},
	{0x82,0x01,{0x07}},
	{0x83,0x01,{0x07}},
	{0x84,0x01,{0x07}},
	{0x85,0x01,{0x07}},
	{0x86,0x01,{0x07}},
	{0x87,0x01,{0x07}},
	{0x88,0x01,{0x07}},
	{0x89,0x01,{0x07}},
	{0x8a,0x01,{0x07}},
	{0x8b,0x01,{0x07}},
	{0x8c,0x01,{0x07}},
	{0xb0,0x01,{0x33}},
	{0xba,0x01,{0x06}},
	{0xc0,0x01,{0x07}},
	{0xc1,0x01,{0x00}},
	{0xc2,0x01,{0x00}},
	{0xc3,0x01,{0x00}},
	{0xc4,0x01,{0x00}},
	{0xca,0x01,{0x44}},                            
	{0xd0,0x01,{0x01}},
	{0xd1,0x01,{0x22}},
	{0xd3,0x01,{0x40}},
	{0xd5,0x01,{0x51}},
	{0xd6,0x01,{0x20}},
	{0xd7,0x01,{0x01}},
	{0xd8,0x01,{0x00}},
	{0xdc,0x01,{0xc2}},
	{0xdd,0x01,{0x10}},
	{0xdf,0x01,{0xb6}},
	{0xe0,0x01,{0x3E}},
	{0xe2,0x01,{0x47}},
	{0xe7,0x01,{0x54}},                                    
	{0xe6,0x01,{0x22}},
	{0xEA,0x01,{0x05}},   //DESENSE_0827
	{0xee,0x01,{0x15}},
	{0xf1,0x01,{0x00}},
	{0xf2,0x01,{0x00}},
	{0xfa,0x01,{0xdf}},

// RTN. Internal VBP, Internal VFP
	{0xFF,0x03,{0x98,0x82,0x02}},
	{0xF1,0x01,{0x1C}},     // Tcon ESD option
	{0x40,0x01,{0x4B}},     //Data_in=7us
	{0x4B,0x01,{0x5A}},     // line_chopper
	{0x50,0x01,{0xCA}},     // line_chopper
	{0x51,0x01,{0x50}},     // line_chopper BG offset
	{0x06,0x01,{0x8E}},    // Internal Line Time (RTN)
	{0x0B,0x01,{0xA0}},     // Internal VFP[9]
	{0x0C,0x01,{0x00}},     // Internal VFP[8]
	{0x0D,0x01,{0x12}},     // Internal VBP
	{0x0E,0x01,{0xF0}},     // Internal VFP
	{0x4D,0x01,{0xCE}},     // Power Saving Off
	{0x70,0x01,{0x32}},     // for notch tuning  
	{0x73,0x01,{0x04}},    	// for notch tuning
	{0x83,0x01,{0x20}},      
	{0x84,0x01,{0x00}},

// Power Setting
	{0xFF,0x03,{0x98,0x82,0x05}},
	{0xA8,0x01,{0x62}},
	{0x03,0x01,{0x00}},     // VCOM
	{0x04,0x01,{0xAF}},     // VCOM
	{0x63,0x01,{0x73}},     // GVDDN = -4.8V
	{0x64,0x01,{0x73}},     // GVDDP = 4.8V
	{0x68,0x01,{0x65}},     // VGHO = 12V
	{0x69,0x01,{0x6B}},     // VGH = 13V
	{0x6A,0x01,{0xA1}},     // VGLO = -12V
	{0x6B,0x01,{0x93}},    // VGL = -13V
	{0x00,0x01,{0x01}},     // Panda Enable
	{0x46,0x01,{0x00}},     // LVD HVREG option 
	{0x45,0x01,{0x01}},     // VGHO/DCHG1/DCHG2 DELAY OPTION Default
	{0x17,0x01,{0x50}},     // LVD rise debounce
	{0x18,0x01,{0x01}},     // Keep LVD state
	{0x85,0x01,{0x37}},     // HW RESET option
	{0x86,0x01,{0x0F}},     // FOR PANDA GIP DCHG1/2ON EN

// Resolution
	{0xFF,0x03,{0x98,0x82,0x06}},
	{0xD9,0x01,{0x1F}},     // 3Lane 4Lane  10
	{0xC0,0x01,{0x40}},    
	{0xC1,0x01,{0x16}},     
	{0x07,0x01,{0xF7}},     
	{0x06,0x01,{0xA4}}, 

	{0xFF,0x01,{0x98,82,07}},
	{0x00,0x01,{0x0C}},

//Gamma Register
	{0xFF,0x03,{0x98,0x82,0x08}},
	{0xE0,0x1b,{0x40,0x24,0x97,0xD1,0x10,0x55,0x40,0x64,0x8D,0xAD,0xA9,0xDE,0x03,0x24,0x43,0xAA,0x63,0x8C,0xA8,0xCC,0xFE,0xEC,0x18,0x50,0x7D,0x03,0xEC}},
	{0xE1,0x1b,{0x40,0x24,0x97,0xD1,0x10,0x55,0x40,0x64,0x8D,0xAD,0xA9,0xDE,0x03,0x24,0x43,0xAA,0x63,0x8C,0xA8,0xCC,0xFE,0xEC,0x18,0x50,0x7D,0x03,0xEC}},

//Gamma Register   _V02_20200806								
//	{0xFF,0x01,{0x98,82,08								
//	{0xE0,0x1b,{0x40,24,98,D2,13,55,44,68,91,B2,A9,E3,09,2A,49,AA,69,92,AE,D3,FE,F4,21,5B,8E,0x01,{0xEC	}},							
//	{0xE1,0x1b,{0x40,24,98,D2,13,55,44,68,91,B2,A9,E3,09,2A,49,AA,69,92,AE,D3,FE,F4,21,5B,8E,0x01,{0xEC}},								


// OSC Auto Trim Setting
	{0xFF,0x03,{0x98,0x82,0x0B}},
	{0x9A,0x01,{0x44}},     //internal porch_0827
	{0x9B,0x01,{0x7C}},	 //internal porch_0827
	{0x9C,0x01,{0x03}},	 //internal porch_0827
	{0x9D,0x01,{0x03}},	 //internal porch_0827
	{0x9E,0x01,{0x70}},	 //internal porch_0827
	{0x9F,0x01,{0x70}},	 //internal porch_0827
	{0xAB,0x01,{0xE0}},     

	{0xFF,0x03,{0x98,0x82,0x0E}},
	{0x02,0x01,{0x09}},
	{0x11,0x01,{0x50}},      // TSVD Rise Poisition
	{0x13,0x01,{0x10}},     // TSHD Rise Poisition
	{0x00,0x01,{0xA0}},     // LV mode

	{0xFF,0x03,{0x98,0x82,0x00}},
	{0x68,0x02,{0x04,0x00}},
	{0x51,0x02,{0x00,0x00}},
	{0x53,0x01,{0x2C}},
	{0x11,0x01,{0x00}},
//DELAY,80
	{REGFLAG_DELAY, 80,{}},
	{0x29,0x01,{0x00}},
//DELAY,20
	{REGFLAG_DELAY, 20,{}},
	{0x35,0x01,{0x00}},

};
//-bug568878, sheqihao.wt, MODIFY, 20200709, fix LCD Gamma test fail

//extern void himax_lcd_resume_func(void);
static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	//params->physical_width_um = LCM_PHYSICAL_WIDTH;
	//params->physical_height_um = LCM_PHYSICAL_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	//lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	//params->dsi.switch_mode = CMD_MODE;
	//lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	//params->dsi.mode   = SYNC_PULSE_VDO_MODE;	//SYNC_EVENT_VDO_MODE
#endif
	//LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
//+bug576293, sheqihao.wt, MODIFY, 20200722, fix LCD panel  MIPI CLK&LCD Power On/Off Sequence
	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 16;
	params->dsi.vertical_frontporch = 240;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;
//584789 , liuchunyang.wt, modify, 20201023, modify prich by FAE ask
	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 20;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/*params->dsi.ssc_disable = 1;*/
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 270;	/* this value must be in MTK suggested table */
#else
	//bug601701, liuchunyang.wt, modify, 20201116, modify mipi clock for HW ask
	params->dsi.PLL_CLOCK = 284;	/* this value must be in MTK suggested table */
#endif
//-bug576293, sheqihao.wt, MODIFY, 20200722, fix LCD panel  MIPI CLK&LCD Power On/Off Sequence
	//params->dsi.PLL_CK_CMD = 220;
	//params->dsi.PLL_CK_VDO = 255;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
//+bug600800, liuchunyang.wt, modify, 20201113, open lcd esd check for T99652/T99653
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
//-bug600800, liuchunyang.wt, modify, 20201113, open lcd esd check for T99652/T99653
/*
	params->dsi.lcm_esd_check_table[1].cmd = 0x09;
	params->dsi.lcm_esd_check_table[1].count = 3;
	params->dsi.lcm_esd_check_table[1].para_list[0] = 0x80;
	params->dsi.lcm_esd_check_table[1].para_list[1] = 0x73;
	params->dsi.lcm_esd_check_table[1].para_list[2] = 0x04;
*/
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
#endif
//	params->use_gpioID = 1;
//	params->pioID_value = 8;
}

static void lcm_init_power(void)
{

	//pr_debug("[LCM][GPIO]lcm_init_power !\n");
	lcm_power_enable();
}

static void lcm_suspend_power(void)
{
	//pr_debug("[LCM][GPIO]lcm_suspend_power !\n");
	lcm_reset_pin(1);//bug584789 , liuchunyang.wt, modify, 20201028, modify lcm_reset hight when suspend
	MDELAY(3);//bug568912, sheqihao.wt, MODIFY, 20200703, fix LCD panel Flicker
	lcm_power_disable();
}

static void lcm_resume_power(void)
{

	//pr_debug("[LCM][GPIO]lcm_resume_power !\n");
	lcm_power_enable();
}
#ifdef BUILD_LK
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
   mt_set_gpio_mode(GPIO, GPIO_MODE_00);
   mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
   mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
}
#endif
static void lcm_init(void)
{
	MDELAY(5);
	lcm_reset_pin(0);
	MDELAY(5);
	lcm_reset_pin(1);
	MDELAY(15);//bug568912, sheqihao.wt, MODIFY, 20200703, fix LCD panel Flicker
	//pr_debug("befor lcd resume func.\n");
	//himax_lcd_resume_func();
	//pr_debug("after lcd resume func.\n");
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{

	//pr_debug("[LCM]lcm_suspend\n");

	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);

}

static void lcm_resume(void)
{
	//pr_debug("[LCM]lcm_resume\n");

	lcm_init();
}

#if 1
static unsigned int lcm_compare_id(void)
{

	return 1;
}
#endif

/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9c) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}

	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}

#if (LCM_DSI_CMD_MODE)

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty. In this case we need to use minimu roi */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		LCM_LOGD("%s calc error,assign full roi:y=%d,h=%d\n", __func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*LCM_LOGD("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",*/
	/*	*x, *y, *width, *height, x1, y1, w, h);*/

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, begin*/
struct boe_panel_white_point{
	unsigned short int white_x;
	unsigned short int white_y;
};

//#define WHITE_POINT_BASE_X 167
//#define WHITE_POINT_BASE_Y 192
#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER ili9882h_hdp_vdo_cmd_ctc_lcm_drv = {
	.name = "ili9882h_hd_plus_dsi_cmd_ctc_lcm_drv",
#else

struct LCM_DRIVER ili9882h_hdp_vdo_incell_skyworth_lcm_drv = {
	.name = "ili9882h_hdp_vdo_incell_skyworth",
#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
//	.set_backlight_cmdq = lcm_setbacklight_cmdq,
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, begin*/
//	.get_white_point = lcm_white_x_y,
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, end*/
	.ata_check = lcm_ata_check,
	.switch_mode = lcm_switch_mode,
//	.set_cabc_cmdq = lcm_set_cabc_cmdq,
//	.get_cabc_status = lcm_get_cabc_status,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
