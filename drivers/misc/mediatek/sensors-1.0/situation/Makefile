# In case the platform does NOT support this type of sensors

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include

obj-y	+= situation.o
obj-$(CONFIG_MTK_INPKHUB)	+=	inpocket/
obj-$(CONFIG_MTK_STATHUB) 	+= 	stationary/
obj-$(CONFIG_MTK_WAKEHUB)	+=	wake_gesture/
obj-$(CONFIG_MTK_GLGHUB)	+=	glance_gesture/
obj-$(CONFIG_MTK_PICKUPHUB)	+=	pickup_gesture/
obj-$(CONFIG_MTK_ANSWER_CALL_HUB)		+=	answercall/
obj-$(CONFIG_MTK_DEVICE_ORIENTATION_HUB)	+=	device_orientation/
obj-$(CONFIG_MTK_MOTION_DETECT_HUB)			+=	motion_detect/
obj-$(CONFIG_MTK_TILTDETECTHUB)				+=	tilt_detector/
obj-$(CONFIG_MTK_FLAT_HUB)				+=	flat/
obj-$(CONFIG_MTK_SAR_HUB)				+=	sar/
obj-$(CONFIG_WT_PROJECT_T99653AA1)				+=	shake/
obj-$(CONFIG_WT_PROJECT_T99651AA2)				+=	shake/
