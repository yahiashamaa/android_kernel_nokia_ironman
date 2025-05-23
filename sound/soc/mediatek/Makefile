# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -Werror

# dsp related code
obj-$(CONFIG_MTK_AUDIODSP_SUPPORT) += audio_dsp/
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_dsp/
#obj-$(CONFIG_MTK_VOW_BARGE_IN_SUPPORT) += scp_vow/
# btcvsd
ifeq ($(CONFIG_SND_SOC_MT6779_MT6359),m)
subdir-ccflags-y += -DASOC_TEMP_BYPASS
endif
obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += common/
obj-$(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT) += common_int/
obj-$(CONFIG_SND_SOC_MEDIATEK) += common/
obj-$(CONFIG_SND_SOC_MT2701) += mt2701/
obj-$(CONFIG_SND_SOC_MT6797) += mt6797/
obj-$(CONFIG_SND_SOC_MT6779) += mt6779/
obj-$(CONFIG_SND_SOC_MT8173) += mt8173/
obj-$(CONFIG_SND_SOC_AW87519) += aw87519/

# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

# adsp
ifeq ($(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/tasks/call/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp
endif

ifeq ($(CONFIG_SND_SOC_MTK_SMART_PHONE),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
endif

# New architecture
ifeq ($(CONFIG_MTK_SND_SOC_NEW_ARCH),y)

# MTK Platform Support
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

# MTK Codec Chip Support
MTK_CODEC := $(subst ",,$(CONFIG_MTK_SND_SOC_CODEC))

# flags for not ready feature, uncomment after ready
subdir-ccflags-y += -DAUDIO_USING_WRAP_DRIVER
subdir-ccflags-y += -D_MT_IDLE_HEADER

ifeq ($(CONFIG_SND_SOC_AW87519),y)
subdir-ccflags-y += -DSND_SOC_AW87519
endif

# include path
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/accdet/mt6358
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/

ifeq ($(CONFIG_MACH_MT6761),y)
AUDIO_PLATFORM := mt6765
else
AUDIO_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/scp/$(AUDIO_PLATFORM)/
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/audio_ipi/$(AUDIO_PLATFORM)/


# Common include path
subdir-ccflags-y += -Werror -I$(src)/common_int

# PMIC include path
ifneq  ($(MTK_CODEC),)
subdir-ccflags-y += -Werror -I$(src)/codec/$(MTK_CODEC)
endif
#+Bug584786 liujin.wt, modify, 20201013, audio bringup for sia81xx 
subdir-ccflags-$(CONFIG_SND_SOC_SIA81XX) += -Werror -I$(src)/codec/sia81xx/
subdir-ccflags-$(CONFIG_WT_PROJECT_T99652AA1) += -DCONFIG_WT_PROJECT_T99652AA1
#-Bug584786 liujin.wt, modify, 20201013, audio bringup for sia81xx 

#Bugbug589753 liujin.wt, modify, 20201014, audio bringup for sia81xx 
subdir-ccflags-$(CONFIG_WT_PROJECT_T99653AA1) += -DCONFIG_WT_PROJECT_T99653AA1
subdir-ccflags-y += -Werror -Wno-incompatible-pointer-types

# Update pmic built flow to avoid codec init later than codec probe
ifneq  ($(MTK_CODEC),)
obj-y += codec/$(MTK_CODEC)/
endif
obj-y += common_int/

#Bug584786 liujin.wt, modify, 20201013, audio bringup for sia81xx 
obj-$(CONFIG_SND_SOC_SIA81XX) += codec/sia81xx/

ifeq ($(MTK_PLATFORM),mt6758)
  obj-y += mt6758_int/
  subdir-ccflags-y += -Werror -I$(src)/mt6758_int
else
  obj-y += $(AUDIO_PLATFORM)/

# AP include path
  subdir-ccflags-y += -Werror -I$(src)/$(AUDIO_PLATFORM)
endif
endif
