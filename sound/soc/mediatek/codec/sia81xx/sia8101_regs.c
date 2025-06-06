/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#define LOG_FLAG	"sia8101_regs"

#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/device.h>
#include "sia81xx_common.h"
#include "sia81xx_regmap.h"
#include "sia8101_regs.h"

#define SIA8101_WRITEABLE_REG_NUM			(6)

static const char sia8101_palyback_defaults[][SIA8101_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x48,		//SIA8101_REG_MOD_CFG
				0xE0,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x00,		//SIA8101_REG_X_DATA_C
				0x18		//SIA8101_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x48,		//SIA8101_REG_MOD_CFG
				0xE0,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x00,		//SIA8101_REG_X_DATA_C
				0x18		//SIA8101_REG_TEST_CFG
	}
};

static const char sia8101_voice_defaults[][SIA8101_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x0D,		//SIA8101_REG_MOD_CFG
				0xE0,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x02,		//SIA8101_REG_X_DATA_C
				0x00		//SIA8101_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x0D,		//SIA8101_REG_MOD_CFG
				0xE0,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x02,		//SIA8101_REG_X_DATA_C
				0x00		//SIA8101_REG_TEST_CFG
	}
};

static const char sia8101_receiver_defaults[][SIA8101_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x10,		//SIA8101_REG_MOD_CFG
				0xA0,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x00,		//SIA8101_REG_X_DATA_C
				0x18		//SIA8101_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x10,		//SIA8101_REG_MOD_CFG
				0xA8,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x00,		//SIA8101_REG_X_DATA_C
				0x18		//SIA8101_REG_TEST_CFG
	}
};

static const char sia8101_factory_defaults[][SIA8101_WRITEABLE_REG_NUM] = {
	[SIA81XX_CHANNEL_L] = {
				0x0D,		//SIA8101_REG_MOD_CFG
				0xE0,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x02,		//SIA8101_REG_X_DATA_C
				0x00		//SIA8101_REG_TEST_CFG
	},
	[SIA81XX_CHANNEL_R] = {
				0x0D,		//SIA8101_REG_MOD_CFG
				0xE0,		//SIA8101_REG_SYS_EN
				0x00,		//SIA8101_REG_X_DATA_A
				0x00,		//SIA8101_REG_X_DATA_B
				0x02,		//SIA8101_REG_X_DATA_C
				0x00		//SIA8101_REG_TEST_CFG
	}
};

const struct sia81xx_reg_default_val sia8101_reg_default_val = {
	.chip_type = CHIP_TYPE_SIA8101, 
	.offset = SIA8101_REG_MOD_CFG,
	.reg_defaults[AUDIO_SCENE_PLAYBACK] = {
		.vals = (char *)sia8101_palyback_defaults,
		.num = ARRAY_SIZE(sia8101_palyback_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_VOICE] = {
		.vals = (char *)sia8101_voice_defaults,
		.num = ARRAY_SIZE(sia8101_voice_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_RECEIVER] = {
		.vals = (char *)sia8101_receiver_defaults,
		.num = ARRAY_SIZE(sia8101_receiver_defaults[0])
	},
	.reg_defaults[AUDIO_SCENE_FACTORY] = {
		.vals = (char *)sia8101_factory_defaults,
		.num = ARRAY_SIZE(sia8101_factory_defaults[0])
	}
};

static bool sia8101_writeable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8101_REG_MOD_CFG ... SIA8101_REG_TEST_CFG :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8101_readable_register(
	struct device *dev, 
	unsigned int reg)
{
	switch (reg) {
		case SIA8101_REG_CHIP_ID ... SIA8101_REG_STATE_FLAG2 :
			return true;
		default : 
			break;
	}

	return false;
}

static bool sia8101_volatile_register(
	struct device *dev, 
	unsigned int reg)
{
	return true;
}

const struct regmap_config sia8101_regmap_config = {
	.name = "sia8101",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 0,
	.pad_bits = 0,
	.cache_type = REGCACHE_NONE,
	.reg_defaults = NULL,
	.num_reg_defaults = 0,
	.writeable_reg = sia8101_writeable_register,
	.readable_reg = sia8101_readable_register,
	.volatile_reg = sia8101_volatile_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};


static int sia8101_check_chip_id(
	struct regmap *regmap) 
{
	char val = 0;

	if(0 != sia81xx_regmap_read(regmap, SIA8101_REG_CHIP_ID, 1, &val))
		return -1;

	if((SIA8101_CHIP_ID_VAL & SIA8101_CHIP_ID_MASK) != 
		(val & SIA8101_CHIP_ID_MASK))
		return -1;

	return 0;
}

static void sia8101_set_xfilter(
	struct regmap *regmap, 
	unsigned int val)
{
	pr_debug("[debug][%s] %s: val = %u \r\n", LOG_FLAG, __func__, val);

	return ;
}

static void sia8101_chip_on(
	struct regmap *regmap)
{
	char val = 0;

	if(0 != sia81xx_regmap_read(regmap, SIA8101_REG_SYS_EN, 1, &val))
		return;

	val |= 0x01 << 6;	// enable classd
	if(0 != sia81xx_regmap_write(regmap, SIA8101_REG_SYS_EN, 1, &val))
		return;
	
	mdelay(40); /* wait classd up */
	
	val = 0x00;	// disable test mode
	if (0 != sia81xx_regmap_write(regmap, SIA8101_REG_TEST_CFG, 1, &val)){
		pr_err("[  err][%s] %s: write test SIA8101_REG_TEST_CFG err !!! \r\n", 
			LOG_FLAG, __func__);
		return;
	}
}

static void sia8101_chip_off(
	struct regmap *regmap)
{
	const char val = 0x18;

	// enable test mode
	if (0 != sia81xx_regmap_write(regmap, SIA8101_REG_TEST_CFG, 1, &val)) {
		pr_err("[  err][%s] %s: write test reg err !!! \r\n", 
			LOG_FLAG, __func__);
		return;
	}
	
	mdelay(10); /* wait  */
}

const struct sia81xx_opt_if sia8101_opt_if = {
	.check_chip_id = sia8101_check_chip_id,
	.set_xfilter = sia8101_set_xfilter,
	.chip_on = sia8101_chip_on,
	.chip_off = sia8101_chip_off,
	.get_chip_en = NULL,
};


