// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"
#include "eeprom_i2c_dev.h"
#include "eeprom_i2c_common_driver.h"
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#if defined(CONFIG_WT_PROJECT_T99651AA2) || defined(CONFIG_WT_PROJECT_T99653AA1)  || defined(CONFIG_WT_PROJECT_T99652AA1)
//+bug591033 cdzk, add, 2020/10/27, the otp porting
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_DBG(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
//-bug591033 cdzk, add, 2020/10/27, the otp porting
#endif

#define CAM_CAL_DRV_NAME "CAM_CAL_DRV"
#define CAM_CAL_DEV_MAJOR_NUMBER 226

#define CAM_CAL_MAX_BUF_SIZE 65536  /*For Safety, Can Be Adjested */

#define CAM_CAL_I2C_DEV1_NAME CAM_CAL_DRV_NAME
#define CAM_CAL_I2C_DEV2_NAME "CAM_CAL_DEV2"
#define CAM_CAL_I2C_DEV3_NAME "CAM_CAL_DEV3"
#define CAM_CAL_I2C_DEV4_NAME "CAM_CAL_DEV4"

static dev_t g_devNum = MKDEV(CAM_CAL_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static unsigned int g_drvOpened;
static struct i2c_client *g_pstI2Cclients[I2C_DEV_IDX_MAX] = { NULL };


static DEFINE_SPINLOCK(g_spinLock); /*for SMP */


static unsigned int g_lastDevID;

#if defined(CONFIG_WT_PROJECT_T99651AA2)
//+bug591033 cdzk, modify, 2020/10/27, otp bring up
#define HI556_LSC_DATA_SIZE 1868
#define HI556_AWB_DATA_SIZE 16
#define GC5035_OTP_WB_DATA_SIZE            16
#define TOTAL_CHECKSUM_ADDR 0xCF8

extern unsigned char hi556_data_lsc[HI556_LSC_DATA_SIZE + 1];
extern unsigned char hi556_data_awb[HI556_AWB_DATA_SIZE + 1];
extern unsigned char hi556_module_id;
extern unsigned char hi556_lsc_valid;
extern unsigned char hi556_awb_valid;

extern unsigned char gc5035_data_awb[GC5035_OTP_WB_DATA_SIZE + 1];
extern unsigned char gc5035_module_id;
extern unsigned char gc5035_awb_valid;
u32 hi556_vendor_id = 0x18020100;
u32 gc5035_vendor_id = 0x0a020000;
//-bug591033 cdzk, modify, 2020/10/27, otp bring up
//+bug584789 chenbocheng.wt, add, 2020/10/30, add main camera s5kgm1 and wide camera hi556 otp code
#elif defined(CONFIG_WT_PROJECT_T99653AA1)
//one supply wide camera hi566
#define HI556_LSC_DATA_SIZE 1868
#define HI556_AWB_DATA_SIZE 16

extern unsigned char hi556_data_lsc[HI556_LSC_DATA_SIZE + 1];
extern unsigned char hi556_data_awb[HI556_AWB_DATA_SIZE + 1];
extern unsigned char hi556_module_id;
extern unsigned char hi556_lsc_valid;
extern unsigned char hi556_awb_valid;

u32 hi556_vendor_id = 0x13030000;

//one supply front camera hi846
#define HI846_LSC_DATA_SIZE 1868
#define HI846_AWB_DATA_SIZE 16

extern unsigned char hi846_data_lsc[HI846_LSC_DATA_SIZE + 1];
extern unsigned char hi846_data_awb[HI846_AWB_DATA_SIZE + 1];
extern unsigned char hi846_module_id;
extern unsigned char hi846_lsc_valid;
extern unsigned char hi846_awb_valid;

u32 hi846_vendor_id = 0x17050100;
//-bug584789 chenbocheng.wt, add, 2020/10/30, add main camera s5kgm1 and wide camera hi556 otp code
#endif

//+bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up
#if defined(CONFIG_WT_PROJECT_T99652AA1)

#define HI846_LSC_DATA_SIZE 1868
#define HI846_AWB_DATA_SIZE 16
//#define HI846_AF_DATA_SIZE 5
//#define VENDOR_ID_DATA_SIZE 1

extern unsigned char hi846_data_lsc[HI846_LSC_DATA_SIZE + 1];
extern unsigned char hi846_data_awb[HI846_AWB_DATA_SIZE + 1];
extern unsigned char hi846_module_id;
extern unsigned char hi846_lsc_valid;
extern unsigned char hi846_awb_valid;

u32 hi846_vendor_id = 0x17050100;

#endif
//-bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up


/***********************************************************
 *
 ***********************************************************/
struct stCAM_CAL_CMD_INFO_STRUCT {
    unsigned int sensorID;
    unsigned int deviceID;
    unsigned int i2cAddr;
    struct i2c_client *client;
    cam_cal_cmd_func readCMDFunc;
    cam_cal_cmd_func writeCMDFunc;
    unsigned int maxEepromSize;
};

static struct stCAM_CAL_CMD_INFO_STRUCT
    g_camCalDrvInfo[IMGSENSOR_SENSOR_IDX_MAX_NUM];

/********************************************************************
 * EEPROM_set_i2c_bus()
 * To i2c client and slave id
 ********************************************************************/

static int EEPROM_set_i2c_bus(unsigned int deviceID,
                  struct stCAM_CAL_CMD_INFO_STRUCT *cmdInfo)
{
    enum IMGSENSOR_SENSOR_IDX idx;
    enum EEPROM_I2C_DEV_IDX i2c_idx;
    struct i2c_client *client;

    idx = IMGSENSOR_SENSOR_IDX_MAP(deviceID);
    i2c_idx = get_i2c_dev_sel(idx);

    if (idx == IMGSENSOR_SENSOR_IDX_NONE)
        return -EFAULT;

    if (i2c_idx < I2C_DEV_IDX_1 || i2c_idx >= I2C_DEV_IDX_MAX)
        return -EFAULT;

    client = g_pstI2Cclients[i2c_idx];
    pr_debug("%s end! deviceID=%d index=%u client=%p\n",
         __func__, deviceID, idx, client);

    if (client == NULL) {
        pr_err("i2c client is NULL");
        return -EFAULT;
    }

    if (cmdInfo != NULL) {
        client->addr = cmdInfo->i2cAddr;
        cmdInfo->client = client;
    }

    return 0;
}



/*************************************************
 * EEPROM_get_cmd_info function
 *************************************************/

static int EEPROM_get_cmd_info(unsigned int sensorID,
    struct stCAM_CAL_CMD_INFO_STRUCT *cmdInfo)
{
    struct stCAM_CAL_LIST_STRUCT *pCamCalList = NULL;
    int i = 0;

    cam_cal_get_sensor_list(&pCamCalList);
    if (pCamCalList != NULL) {
        pr_debug("pCamCalList!=NULL && pCamCalFunc!= NULL\n");
        for (i = 0; pCamCalList[i].sensorID != 0; i++) {
            if (pCamCalList[i].sensorID == sensorID) {
                pr_debug("pCamCalList[%d].sensorID==%x\n", i,
                       pCamCalList[i].sensorID);

                cmdInfo->i2cAddr = pCamCalList[i].slaveID >> 1;
                cmdInfo->readCMDFunc =
                    pCamCalList[i].readCamCalData;
                cmdInfo->maxEepromSize =
                    pCamCalList[i].maxEepromSize;

                /*
                 * Default 8K for Common_read_region driver
                 * 0 for others
                 */
                if (cmdInfo->readCMDFunc == Common_read_region
                    && cmdInfo->maxEepromSize == 0) {
                    cmdInfo->maxEepromSize =
                        DEFAULT_MAX_EEPROM_SIZE_8K;
                }

                return 1;
            }
        }
    }
    return 0;

}

static struct stCAM_CAL_CMD_INFO_STRUCT *EEPROM_get_cmd_info_ex
    (unsigned int sensorID, unsigned int deviceID)
{
    int i = 0;

    /* To check device ID */
    for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
        if (g_camCalDrvInfo[i].deviceID == deviceID)
            break;
    }
    /* To check cmd from Sensor ID */

    if (i == IMGSENSOR_SENSOR_IDX_MAX_NUM) {
        for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
            /* To Set Client */
            if (g_camCalDrvInfo[i].sensorID == 0) {
                pr_debug("Start get_cmd_info!\n");
                EEPROM_get_cmd_info(sensorID,
                    &g_camCalDrvInfo[i]);

                if (g_camCalDrvInfo[i].readCMDFunc != NULL) {
                    g_camCalDrvInfo[i].sensorID = sensorID;
                    g_camCalDrvInfo[i].deviceID = deviceID;
                    pr_debug("deviceID=%d, SensorID=%x\n",
                        deviceID, sensorID);
                }
                break;
            }
        }
    }

    if (i == IMGSENSOR_SENSOR_IDX_MAX_NUM) {/*g_camCalDrvInfo is full */
        return NULL;
    } else {
        return &g_camCalDrvInfo[i];
    }
}

/**************************************************
 * EEPROM_HW_i2c_probe
 **************************************************/
static int EEPROM_HW_i2c_probe
    (struct i2c_client *client, const struct i2c_device_id *id)
{
    /* get sensor i2c client */
    spin_lock(&g_spinLock);
    g_pstI2Cclients[I2C_DEV_IDX_1] = client;

    /* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
    g_pstI2Cclients[I2C_DEV_IDX_1]->timing = gi2c_dev_timing[I2C_DEV_IDX_1];
    g_pstI2Cclients[I2C_DEV_IDX_1]->ext_flag &= ~I2C_POLLING_FLAG;
#endif

    /* Default EEPROM Slave Address Main= 0xa0 */
    g_pstI2Cclients[I2C_DEV_IDX_1]->addr = 0x50;
    spin_unlock(&g_spinLock);

    return 0;
}



/**********************************************
 * CAMERA_HW_i2c_remove
 **********************************************/
static int EEPROM_HW_i2c_remove(struct i2c_client *client)
{
    return 0;
}

/***********************************************
 * EEPROM_HW_i2c_probe2
 ***********************************************/
static int EEPROM_HW_i2c_probe2
    (struct i2c_client *client, const struct i2c_device_id *id)
{
    /* get sensor i2c client */
    spin_lock(&g_spinLock);
    g_pstI2Cclients[I2C_DEV_IDX_2] = client;

    /* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
    g_pstI2Cclients[I2C_DEV_IDX_2]->timing = gi2c_dev_timing[I2C_DEV_IDX_2];
    g_pstI2Cclients[I2C_DEV_IDX_2]->ext_flag &= ~I2C_POLLING_FLAG;
#endif

    /* Default EEPROM Slave Address sub = 0xa8 */
    g_pstI2Cclients[I2C_DEV_IDX_2]->addr = 0x54;
    spin_unlock(&g_spinLock);

    return 0;
}

/********************************************************
 * CAMERA_HW_i2c_remove2
 ********************************************************/
static int EEPROM_HW_i2c_remove2(struct i2c_client *client)
{
    return 0;
}

/********************************************************
 * EEPROM_HW_i2c_probe3
 ********************************************************/
static int EEPROM_HW_i2c_probe3
    (struct i2c_client *client, const struct i2c_device_id *id)
{
    /* get sensor i2c client */
    spin_lock(&g_spinLock);
    g_pstI2Cclients[I2C_DEV_IDX_3] = client;

    /* set I2C clock rate */
#ifdef CONFIG_MTK_I2C_EXTENSION
    g_pstI2Cclients[I2C_DEV_IDX_3]->timing = gi2c_dev_timing[I2C_DEV_IDX_3];
    g_pstI2Cclients[I2C_DEV_IDX_3]->ext_flag &= ~I2C_POLLING_FLAG;
#endif

    /* Default EEPROM Slave Address Main2 = 0xa4 */
    g_pstI2Cclients[I2C_DEV_IDX_3]->addr = 0x52;
    spin_unlock(&g_spinLock);

    return 0;
}

/*************************************************************
 * CAMERA_HW_i2c_remove3
 *************************************************************/
static int EEPROM_HW_i2c_remove3(struct i2c_client *client)
{
    return 0;
}

/*************************************************************
 * I2C related variable
 *************************************************************/


static const struct i2c_device_id
    EEPROM_HW_i2c_id[] = { {CAM_CAL_DRV_NAME, 0}, {} };
static const struct i2c_device_id
    EEPROM_HW_i2c_id2[] = { {CAM_CAL_I2C_DEV2_NAME, 0}, {} };
static const struct i2c_device_id
    EEPROM_HW_i2c_id3[] = { {CAM_CAL_I2C_DEV3_NAME, 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id EEPROM_HW_i2c_of_ids[] = {
    {.compatible = "mediatek,camera_main_eeprom",},
    {}
};
#endif

struct i2c_driver EEPROM_HW_i2c_driver = {
    .probe = EEPROM_HW_i2c_probe,
    .remove = EEPROM_HW_i2c_remove,
    .driver = {
           .name = CAM_CAL_DRV_NAME,
           .owner = THIS_MODULE,

#ifdef CONFIG_OF
           .of_match_table = EEPROM_HW_i2c_of_ids,
#endif
           },
    .id_table = EEPROM_HW_i2c_id,
};

/*********************************************************
 * I2C Driver structure for Sub
 *********************************************************/
#ifdef CONFIG_OF
static const struct of_device_id EEPROM_HW2_i2c_driver_of_ids[] = {
    {.compatible = "mediatek,camera_sub_eeprom",},
    {}
};
#endif

struct i2c_driver EEPROM_HW_i2c_driver2 = {
    .probe = EEPROM_HW_i2c_probe2,
    .remove = EEPROM_HW_i2c_remove2,
    .driver = {
           .name = CAM_CAL_I2C_DEV2_NAME,
           .owner = THIS_MODULE,
#ifdef CONFIG_OF
           .of_match_table = EEPROM_HW2_i2c_driver_of_ids,
#endif
           },
    .id_table = EEPROM_HW_i2c_id2,
};

/**********************************************************
 * I2C Driver structure for Main2
 **********************************************************/
#ifdef CONFIG_OF
static const struct of_device_id EEPROM_HW3_i2c_driver_of_ids[] = {
    {.compatible = "mediatek,camera_main_two_eeprom",},
    {}
};
#endif

struct i2c_driver EEPROM_HW_i2c_driver3 = {
    .probe = EEPROM_HW_i2c_probe3,
    .remove = EEPROM_HW_i2c_remove3,
    .driver = {
           .name = CAM_CAL_I2C_DEV3_NAME,
           .owner = THIS_MODULE,
#ifdef CONFIG_OF
           .of_match_table = EEPROM_HW3_i2c_driver_of_ids,
#endif
           },
    .id_table = EEPROM_HW_i2c_id3,
};

#if defined(CONFIG_WT_PROJECT_T99651AA2)
//+bug591033 cdzk, modify, 2020/10/27, the otp porting
#define MAIN_OTP_DUMP 1


struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainData = NULL;
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromSubData = NULL;

static  void EEPROM_reset_cmd_info_ex
    (unsigned int sensorID, unsigned int deviceID){
    int i = 0;
    for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
        if ((g_camCalDrvInfo[i].deviceID == deviceID)&&(sensorID == g_camCalDrvInfo[i].sensorID)){
            g_camCalDrvInfo[i].deviceID = 0;
            g_camCalDrvInfo[i].sensorID = 0;
        }
    }
}

#if MAIN_OTP_DUMP
void dumpEEPROMData(int u4Length,u8* pu1Params)
{
    int i = 0;
    for(i = 0; i < u4Length; i += 16){
        if(u4Length - i  >= 16){
            pr_info("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
            i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
            ,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
            ,pu1Params[i+15]);
        }else{
            int j = i;
            for(;j < u4Length;j++)
            pr_info("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
        }
    }
    LOG_INF("\n");
}
#endif

int imgSensorCheckEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* cData){
    int i = 0;
    int length = 0;
    int count;
    u32 sum = 0;

    if((pData != NULL)&&(pData->dataBuffer != NULL)&&(cData != NULL)){
        u8* buffer = pData->dataBuffer;
        //verity validflag and checksum
        for((count = 0);count < MAX_ITEM;count++){
            if(cData[count].item < MAX_ITEM) {
                if(buffer[cData[count].flagAdrees]!= cData[count].validFlag){
                    LOG_INF("invalid otp data cItem=%d,flag=%d failed\n", cData[count].item,buffer[cData[count].flagAdrees]);
                    return -ENODEV;
                } else {
                    LOG_INF("check cTtem=%d,flag=%d otp flag data successful!\n", cData[count].item,buffer[cData[count].flagAdrees]);
                }
                sum = 0;
                length = cData[count].endAdress - cData[count].startAdress;
                for(i = 0;i <= length;i++){
                    sum += buffer[cData[count].startAdress+i];
                }
                if(((sum%0xff)+1)!= buffer[cData[count].checksumAdress]){
                    //+ExtB WVR-3119 chenbocheng.wt, modify, 2020/7/6, the total checksum of s5k3l6 sensor module was write an incorrect value
                    if(TOTAL_CHECKSUM_ADDR == cData[count].checksumAdress){
                        LOG_INF("bypass! total checksum cItem=%d,0x%x,length = 0x%x failed\n",cData[count].item,sum,length);
                    }else{
                        LOG_INF("non total checksum cItem=%d,0x%x,length = 0x%x failed\n",cData[count].item,sum,length);
                        return -ENODEV;
                    }
                    //-ExtB WVR-3119 chenbocheng.wt, modify, 2020/7/6, the total checksum of s5k3l6 sensor module was write an incorrect value
                } else {
                    LOG_INF("checksum cItem=%d,0x%x,length = 0x%x successful!\n",cData[count].item,sum,length);
                }
            } else {
                break;
            }
        }
    } else {
        LOG_INF("some data not inited!\n");
        return -ENODEV;
    }

    LOG_INF("sensor[0x%x][0x%x] eeprom checksum success\n", pData->sensorID, pData->deviceID);

    return 0;
}

int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checkData){
    struct stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;
    int i4RetValue = -1;
    u32 vendorID = 0;
    u8 tmpBuf[4] = {0};

    if((pData == NULL)||(checkData == NULL)){
        LOG_INF("pData or checkData not inited!\n");
        return -EFAULT;
    }

    LOG_INF("SensorID=%x DeviceID=%x\n",pData->sensorID, pData->deviceID);

    pcmdInf = EEPROM_get_cmd_info_ex(pData->sensorID,pData->deviceID);
    if (pcmdInf != NULL) {
        LOG_INF("i2cAddr=0x%x sensorID=0x%x\n",pcmdInf->i2cAddr, pcmdInf->sensorID);
        if(EEPROM_set_i2c_bus(pData->deviceID, pcmdInf) != 0) {
            LOG_INF("deviceID Error!\n");
            return -EFAULT;
        }
    }

    if (pcmdInf != NULL) {
        if (pcmdInf->readCMDFunc != NULL){
            if (pData->dataBuffer == NULL){
                pData->dataBuffer = kmalloc(pData->dataLength, GFP_KERNEL);
                if (pData->dataBuffer == NULL) {
                    LOG_INF("pData->dataBuffer is malloc fail\n");
                    return -EFAULT;
                }
            }
            //verity vendorID
            i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, pData->vendorByte[0], &tmpBuf[0], 1);
            if(i4RetValue != 1){
                LOG_INF("vendorID read falied 0x%x != 0x%x\n",tmpBuf[0], pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }
            vendorID = tmpBuf[0];
            if(vendorID != pData->sensorVendorid >> 24){
                LOG_INF("vendorID cmp falied 0x%x != 0x%x\n",vendorID, pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }
            //get eeprom data
            i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, 0, pData->dataBuffer, pData->dataLength);
            if(i4RetValue != pData->dataLength){
                kfree(pData->dataBuffer);
                pData->dataBuffer = NULL;
                LOG_INF("readCMDFunc failed\n");
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }else{
                if(imgSensorCheckEepromData(pData,checkData) != 0){
                    kfree(pData->dataBuffer);
                    pData->dataBuffer = NULL;
                    LOG_INF("checksum failed\n");
                    EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                    return -EFAULT;
                }
                LOG_INF("SensorID=%x DeviceID=%x read otp data success\n",pData->sensorID, pData->deviceID);
            }
        } else {
            LOG_INF("pcmdInf->readCMDFunc == NULL\n");
        }
    } else {
        LOG_INF("pcmdInf == NULL\n");
    }

    return i4RetValue;
}

int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData){
    int i4RetValue = 0;
    LOG_INF("pData->deviceID = %d\n",pData->deviceID);
    if(pData->deviceID == 0x01){
        if(g_eepromMainData != NULL){
            return -ETXTBSY;
        }
        g_eepromMainData = pData;
    }else{
        LOG_INF("we don't have this devices\n");
        return -ENODEV;
    }
#if MAIN_OTP_DUMP
    if(pData->dataBuffer)
        dumpEEPROMData(pData->dataLength,pData->dataBuffer);
#endif
    return i4RetValue;
}
//-bug591033 cdzk, modify, 2020/10/27, the otp porting
//+bug584789 chenbocheng.wt, add, 2020/10/30, add main camera s5kgm1 and wide camera hi556 otp code
//+bug584789,zhouyikuan.wt,ADD,2020/11/02,S5K3L6 pdaf bring up
#elif defined(CONFIG_WT_PROJECT_T99652AA1)
//-bug584789,zhouyikuan.wt,ADD,2020/11/02,S5K3L6 pdaf bring up

#define MAIN_OTP_DUMP 1

struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainData = NULL;
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromSubData = NULL;
//+bug584789,zhouyikuan.wt,ADD,2020/11/04,GC8034 eeprom bring up
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainMicroData = NULL;

static  void EEPROM_reset_cmd_info_ex
    (unsigned int sensorID, unsigned int deviceID){
    int i = 0;
    for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
        if ((g_camCalDrvInfo[i].deviceID == deviceID)&&(sensorID == g_camCalDrvInfo[i].sensorID)){
            g_camCalDrvInfo[i].deviceID = 0;
            g_camCalDrvInfo[i].sensorID = 0;
        }
    }
}

#if MAIN_OTP_DUMP
void dumpEEPROMData(int u4Length,u8* pu1Params)
{
    int i = 0;
    for(i = 0; i < u4Length; i += 16){
        if(u4Length - i  >= 16){
            pr_info("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
            i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
            ,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
            ,pu1Params[i+15]);
        }else{
            int j = i;
            for(;j < u4Length;j++)
            pr_info("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
        }
    }
    LOG_INF("\n");
}
#endif

int imgSensorCheckEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* cData){
    int i = 0;
    int length = 0;
    int count;
    u32 sum = 0;

    if((pData != NULL)&&(pData->dataBuffer != NULL)&&(cData != NULL)){
        u8* buffer = pData->dataBuffer;
        //verity validflag and checksum
        for((count = 0);count < MAX_ITEM;count++){
            if(cData[count].item < MAX_ITEM) {
                if(buffer[cData[count].flagAdrees]!= cData[count].validFlag){
                    LOG_INF("invalid otp data cItem=%d,flag=%d failed\n", cData[count].item,buffer[cData[count].flagAdrees]);
                    return -ENODEV;
                } else {
                    LOG_INF("check cTtem=%d,flag=%d otp flag data successful!\n", cData[count].item,buffer[cData[count].flagAdrees]);
                }
                sum = 0;
                length = cData[count].endAdress - cData[count].startAdress;
                for(i = 0;i <= length;i++){
                    sum += buffer[cData[count].startAdress+i];
                }
                if(((sum%0xff)+1)!= buffer[cData[count].checksumAdress]){
                    LOG_INF("checksum cItem=%d,0x%x,length = 0x%x failed\n",cData[count].item,sum,length);
                    return -ENODEV;
                } else {
                    LOG_INF("checksum cItem=%d,0x%x,length = 0x%x successful!\n",cData[count].item,sum,length);
                }
            } else {
                break;
            }
        }
    } else {
        LOG_INF("some data not inited!\n");
        return -ENODEV;
    }

    LOG_INF("sensor[0x%x][0x%x] eeprom checksum success\n", pData->sensorID, pData->deviceID);

    return 0;
}

int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checkData){
    struct stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;
    int i4RetValue = -1;
    u32 vendorID = 0;
    u8 tmpBuf[4] = {0};

    if((pData == NULL)||(checkData == NULL)){
        LOG_INF("pData or checkData not inited!\n");
        return -EFAULT;
    }

    LOG_INF("SensorID=%x DeviceID=%x\n",pData->sensorID, pData->deviceID);

    pcmdInf = EEPROM_get_cmd_info_ex(pData->sensorID,pData->deviceID);
    if (pcmdInf != NULL) {
        LOG_INF("i2cAddr=0x%x sensorID=0x%x\n",pcmdInf->i2cAddr, pcmdInf->sensorID);
        if(EEPROM_set_i2c_bus(pData->deviceID, pcmdInf) != 0) {
            LOG_INF("deviceID Error!\n");
            return -EFAULT;
        }
    }

    if (pcmdInf != NULL) {
        if (pcmdInf->readCMDFunc != NULL){
            if (pData->dataBuffer == NULL){
                pData->dataBuffer = kmalloc(pData->dataLength, GFP_KERNEL);
                if (pData->dataBuffer == NULL) {
                    LOG_INF("pData->dataBuffer is malloc fail\n");
                    return -EFAULT;
                }
            }
            //verity vendorID
            i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, pData->vendorByte[0], &tmpBuf[0], 1);
            if(i4RetValue != 1){
                LOG_INF("vendorID read falied 0x%x != 0x%x\n",tmpBuf[0], pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }
            vendorID = tmpBuf[0];
            if(vendorID != pData->sensorVendorid >> 24){
                LOG_INF("vendorID cmp falied 0x%x != 0x%x\n",vendorID, pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }
            //get eeprom data
            i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, 0, pData->dataBuffer, pData->dataLength);
            if(i4RetValue != pData->dataLength){
                kfree(pData->dataBuffer);
                pData->dataBuffer = NULL;
                LOG_INF("readCMDFunc failed\n");
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }else{
                if(imgSensorCheckEepromData(pData,checkData) != 0){
                    kfree(pData->dataBuffer);
                    pData->dataBuffer = NULL;
                    LOG_INF("checksum failed\n");
                    EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                    return -EFAULT;
                }
                LOG_INF("SensorID=%x DeviceID=%x read otp data success\n",pData->sensorID, pData->deviceID);
            }
        } else {
            LOG_INF("pcmdInf->readCMDFunc == NULL\n");
        }
    } else {
        LOG_INF("pcmdInf == NULL\n");
    }

    return i4RetValue;
}

int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData){
    int i4RetValue = 0;
    LOG_INF("pData->deviceID = %d\n",pData->deviceID);
    if(pData->deviceID == 0x01){
        if(g_eepromMainData != NULL){
            return -ETXTBSY;
        }
        g_eepromMainData = pData;
         //+bug596770,zhouyikuan.wt,ADD,2020/11/05, add sub cam eeprom
    }else if(pData->deviceID == 0x02){
        if(g_eepromSubData != NULL){
            return -ETXTBSY;
        }
        g_eepromSubData= pData;
         //+bug596770,zhouyikuan.wt,ADD,2020/11/05, add sub cam eeprom
    }else if(pData->deviceID == 0x08){
        if(g_eepromMainMicroData != NULL){
            return -ETXTBSY;
        }
        g_eepromMainMicroData= pData;
    //-bug596770,houzhipeng.wt,ADD,2020/11/03, add macro cam eeprom
    }else{
        LOG_INF("we don't have this devices\n");
        return -ENODEV;
    }
#if MAIN_OTP_DUMP
    if(pData->dataBuffer)
        dumpEEPROMData(pData->dataLength,pData->dataBuffer);
#endif
    return i4RetValue;
}
#elif defined(CONFIG_WT_PROJECT_T99653AA1)
//-bug584789,zhouyikuan.wt,ADD,2020/11/02,S5K3L6 pdaf bring up

#define MAIN_OTP_DUMP 0

struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainData = NULL;
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromSubData = NULL;
//+bug584789,zhouyikuan.wt,ADD,2020/11/04,GC8034 eeprom bring up
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainMicroData = NULL;
struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainWideData = NULL;

static  void EEPROM_reset_cmd_info_ex
    (unsigned int sensorID, unsigned int deviceID){
    int i = 0;
    for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
        if ((g_camCalDrvInfo[i].deviceID == deviceID)&&(sensorID == g_camCalDrvInfo[i].sensorID)){
            g_camCalDrvInfo[i].deviceID = 0;
            g_camCalDrvInfo[i].sensorID = 0;
        }
    }
}

#if MAIN_OTP_DUMP
void dumpEEPROMData(int u4Length,u8* pu1Params)
{
    int i = 0;
    for(i = 0; i < u4Length; i += 16){
        if(u4Length - i  >= 16){
            pr_info("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
            i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
            ,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
            ,pu1Params[i+15]);
        }else{
            int j = i;
            for(;j < u4Length;j++)
            pr_info("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
        }
    }
    LOG_INF("\n");
}
#endif

int imgSensorCheckEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* cData){
    int i = 0;
    int length = 0;
    int count;
    u32 sum = 0;

    if((pData != NULL)&&(pData->dataBuffer != NULL)&&(cData != NULL)){
        u8* buffer = pData->dataBuffer;
        //verity validflag and checksum
        for((count = 0);count < MAX_ITEM;count++){
            if(cData[count].item < MAX_ITEM) {
                if(buffer[cData[count].flagAdrees]!= cData[count].validFlag){
                    LOG_INF("invalid otp data cItem=%d,flag=%d failed\n", cData[count].item,buffer[cData[count].flagAdrees]);
                    return -ENODEV;
                } else {
                    LOG_INF("check cTtem=%d,flag=%d otp flag data successful!\n", cData[count].item,buffer[cData[count].flagAdrees]);
                }
                sum = 0;
                length = cData[count].endAdress - cData[count].startAdress;
                for(i = 0;i <= length;i++){
                    sum += buffer[cData[count].startAdress+i];
                }
                if(((sum%0xff)+1)!= buffer[cData[count].checksumAdress]){
                    LOG_INF("checksum cItem=%d,0x%x,length = 0x%x failed\n",cData[count].item,sum,length);
                    return -ENODEV;
                } else {
                    LOG_INF("checksum cItem=%d,0x%x,length = 0x%x successful!\n",cData[count].item,sum,length);
                }
            } else {
                break;
            }
        }
    } else {
        LOG_INF("some data not inited!\n");
        return -ENODEV;
    }

    LOG_INF("sensor[0x%x][0x%x] eeprom checksum success\n", pData->sensorID, pData->deviceID);

    return 0;
}

int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checkData){
    struct stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;
    int i4RetValue = -1;
    u32 vendorID = 0;
    u8 tmpBuf[4] = {0};

    if((pData == NULL)||(checkData == NULL)){
        LOG_INF("pData or checkData not inited!\n");
        return -EFAULT;
    }

    LOG_INF("SensorID=%x DeviceID=%x\n",pData->sensorID, pData->deviceID);

    pcmdInf = EEPROM_get_cmd_info_ex(pData->sensorID,pData->deviceID);
    if (pcmdInf != NULL) {
        LOG_INF("i2cAddr=0x%x sensorID=0x%x\n",pcmdInf->i2cAddr, pcmdInf->sensorID);
        if(EEPROM_set_i2c_bus(pData->deviceID, pcmdInf) != 0) {
            LOG_INF("deviceID Error!\n");
            return -EFAULT;
        }
    }

    if (pcmdInf != NULL) {
        if (pcmdInf->readCMDFunc != NULL){
            if (pData->dataBuffer == NULL){
                pData->dataBuffer = kmalloc(pData->dataLength, GFP_KERNEL);
                if (pData->dataBuffer == NULL) {
                    LOG_INF("pData->dataBuffer is malloc fail\n");
                    return -EFAULT;
                }
            }
            //verity vendorID
            i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, pData->vendorByte[0], &tmpBuf[0], 1);
            if(i4RetValue != 1){
                LOG_INF("vendorID read falied 0x%x != 0x%x\n",tmpBuf[0], pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }
            vendorID = tmpBuf[0];
            if(vendorID != pData->sensorVendorid >> 24){
                LOG_INF("vendorID cmp falied 0x%x != 0x%x\n",vendorID, pData->sensorVendorid >> 24);
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }
            //get eeprom data
            i4RetValue = pcmdInf->readCMDFunc(pcmdInf->client, 0, pData->dataBuffer, pData->dataLength);
            if(i4RetValue != pData->dataLength){
                kfree(pData->dataBuffer);
                pData->dataBuffer = NULL;
                LOG_INF("readCMDFunc failed\n");
                EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                return -EFAULT;
            }else{
                if(imgSensorCheckEepromData(pData,checkData) != 0){
                    kfree(pData->dataBuffer);
                    pData->dataBuffer = NULL;
                    LOG_INF("checksum failed\n");
                    EEPROM_reset_cmd_info_ex(pData->sensorID,pData->deviceID);
                    return -EFAULT;
                }
                LOG_INF("SensorID=%x DeviceID=%x read otp data success\n",pData->sensorID, pData->deviceID);
            }
        } else {
            LOG_INF("pcmdInf->readCMDFunc == NULL\n");
        }
    } else {
        LOG_INF("pcmdInf == NULL\n");
    }

    return i4RetValue;
}

int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData){
    int i4RetValue = 0;
    LOG_INF("pData->deviceID = %d\n",pData->deviceID);
    if(pData->deviceID == 0x01){
        if(g_eepromMainData != NULL){
            return -ETXTBSY;
        }
        g_eepromMainData = pData;
         //+bug596770,zhouyikuan.wt,ADD,2020/11/05, add sub cam eeprom
    }else if(pData->deviceID == 0x02){
        if(g_eepromSubData != NULL){
            return -ETXTBSY;
        }
        g_eepromSubData= pData;
         //+bug596770,zhouyikuan.wt,ADD,2020/11/05, add sub cam eeprom
    }else if(pData->deviceID == 0x10){
        if(g_eepromMainMicroData != NULL){
            return -ETXTBSY;
        }
        g_eepromMainMicroData= pData;
    //-bug596770,houzhipeng.wt,ADD,2020/11/03, add macro cam eeprom
    }else if(pData->deviceID == 0x08){
        if(g_eepromMainWideData != NULL){
            return -ETXTBSY;
        }
        g_eepromMainWideData= pData;
    //-bug596770,liuxingyu.wt,ADD,2020/11/10, add gc5035 cam eeprom
    }else{
        LOG_INF("we don't have this devices\n");
        return -ENODEV;
    }
#if MAIN_OTP_DUMP
    if(pData->dataBuffer)
        dumpEEPROMData(pData->dataLength,pData->dataBuffer);
#endif
    return i4RetValue;
}
#endif
//-bug584789 chenbocheng.wt, add, 2020/10/30, add main camera s5kgm1 and wide camera hi556 otp code


#if 0
//+bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up
#if defined(CONFIG_WT_PROJECT_T99652AA1)
#define MAIN_OTP_DUMP 1

struct stCAM_CAL_DATAINFO_STRUCT *g_eepromMainData = NULL;
//struct stCAM_CAL_DATAINFO_STRUCT *g_eepromSubData = NULL;

#if 0
static  void EEPROM_reset_cmd_info_ex
    (unsigned int sensorID, unsigned int deviceID){
    int i = 0;
    for (i = 0; i < IMGSENSOR_SENSOR_IDX_MAX_NUM; i++) {
        if ((g_camCalDrvInfo[i].deviceID == deviceID)&&(sensorID == g_camCalDrvInfo[i].sensorID)){
            g_camCalDrvInfo[i].deviceID = 0;
            g_camCalDrvInfo[i].sensorID = 0;
        }
    }
}
#endif

#if MAIN_OTP_DUMP
void dumpEEPROMData(int u4Length,u8* pu1Params)
{
    int i = 0;
    for(i = 0; i < u4Length; i += 16){
        if(u4Length - i  >= 16){
            pr_info("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
            i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
            ,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
            ,pu1Params[i+15]);
        }else{
            int j = i;
            for(;j < u4Length;j++)
            pr_info("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
        }
    }
    LOG_INF("\n");
}
#endif
#endif
//-bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up
#endif

/*******************************************************
 * EEPROM_HW_probe
 *******************************************************/
static int EEPROM_HW_probe(struct platform_device *pdev)
{
    i2c_add_driver(&EEPROM_HW_i2c_driver2);
    i2c_add_driver(&EEPROM_HW_i2c_driver3);
    return i2c_add_driver(&EEPROM_HW_i2c_driver);
}

/*******************************************************
 * EEPROM_HW_remove()
 *******************************************************/
static int EEPROM_HW_remove(struct platform_device *pdev)
{
    i2c_del_driver(&EEPROM_HW_i2c_driver);
    i2c_del_driver(&EEPROM_HW_i2c_driver2);
    i2c_del_driver(&EEPROM_HW_i2c_driver3);
    return 0;
}

/******************************************************
 *
 ******************************************************/
static struct platform_device g_platDev = {
    .name = CAM_CAL_DRV_NAME,
    .id = 0,
    .dev = {
        }
};


static struct platform_driver g_stEEPROM_HW_Driver = {
    .probe = EEPROM_HW_probe,
    .remove = EEPROM_HW_remove,
    .driver = {
           .name = CAM_CAL_DRV_NAME,
           .owner = THIS_MODULE,
        }
};


/************************************************/

#ifdef CONFIG_COMPAT
static int compat_put_cal_info_struct
    (struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
        struct stCAM_CAL_INFO_STRUCT __user *data)
{
    compat_uptr_t p;
    compat_uint_t i;
    int err;

    err = get_user(i, &data->u4Offset);
    err |= put_user(i, &data32->u4Offset);
    err |= get_user(i, &data->u4Length);
    err |= put_user(i, &data32->u4Length);
    err |= get_user(i, &data->sensorID);
    err |= put_user(i, &data32->sensorID);
    err |= get_user(i, &data->deviceID);
    err |= put_user(i, &data32->deviceID);

    /* Assume pointer is not change */

    err |= get_user(p, (compat_uptr_t *) &data->pu1Params);
    err |= put_user(p, &data32->pu1Params);

    return err;
}

static int EEPROM_compat_get_info
    (struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
        struct stCAM_CAL_INFO_STRUCT __user *data)
{
    compat_uptr_t p;
    compat_uint_t i;
    int err;

    err = get_user(i, &data32->u4Offset);
    err |= put_user(i, &data->u4Offset);
    err |= get_user(i, &data32->u4Length);
    err |= put_user(i, &data->u4Length);
    err |= get_user(i, &data32->sensorID);
    err |= put_user(i, &data->sensorID);
    err |= get_user(i, &data32->deviceID);
    err |= put_user(i, &data->deviceID);

    err |= get_user(p, &data32->pu1Params);
    err |= put_user(compat_ptr(p), &data->pu1Params);

    return err;
}

/*************************************************
 * ioctl
 *************************************************/

static long EEPROM_drv_compat_ioctl
    (struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret;

    struct COMPAT_stCAM_CAL_INFO_STRUCT __user *data32;
    struct stCAM_CAL_INFO_STRUCT __user *data;
    int err;

    if (!filp->f_op || !filp->f_op->unlocked_ioctl)
        return -ENOTTY;

    switch (cmd) {

    case COMPAT_CAM_CALIOC_G_READ:{
            data32 = compat_ptr(arg);
            data = compat_alloc_user_space(sizeof(*data));
            if (data == NULL || data32 == NULL)
                return -EFAULT;

            err = EEPROM_compat_get_info(data32, data);
            if (err)
                return err;

            ret = filp->f_op->unlocked_ioctl(filp,
                CAM_CALIOC_G_READ, (unsigned long)data);
            err = compat_put_cal_info_struct(data32, data);

            if (err != 0)
                pr_debug("getinfo_struct failed\n");

            return ret;
        }

    case COMPAT_CAM_CALIOC_S_WRITE:{
                /*Note: Write Command is Unverified! */
            data32 = compat_ptr(arg);
            data = compat_alloc_user_space(sizeof(*data));
            if (data == NULL || data32 == NULL)
                return -EFAULT;

            err = EEPROM_compat_get_info(data32, data);
            if (err)
                return err;

            ret = filp->f_op->unlocked_ioctl(filp,
                CAM_CALIOC_S_WRITE, (unsigned long)data);
            if (err != 0)
                pr_debug("getinfo_struct failed\n");

            return ret;
        }
    default:
        return -ENOIOCTLCMD;
    }

}

#endif

#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int EEPROM_drv_ioctl(struct inode *a_pstInode,
                struct file *a_pstFile,
                unsigned int a_u4Command, unsigned long a_u4Param)
#else
static long EEPROM_drv_ioctl(struct file *file,
    unsigned int a_u4Command, unsigned long a_u4Param)
#endif
{

    int i4RetValue = 0;
    u8 *pBuff = NULL;
    u8 *pu1Params = NULL;
    /*u8 *tempP = NULL; */
    struct stCAM_CAL_INFO_STRUCT *ptempbuf = NULL;
    struct stCAM_CAL_CMD_INFO_STRUCT *pcmdInf = NULL;

#ifdef CAM_CALGETDLT_DEBUG
    struct timeval ktv1, ktv2;
    unsigned long TimeIntervalUS;
#endif
    if (_IOC_DIR(a_u4Command) != _IOC_NONE) {
        pBuff = kmalloc(sizeof(struct stCAM_CAL_INFO_STRUCT),
                    GFP_KERNEL);
        if (pBuff == NULL) {

            pr_debug("ioctl allocate pBuff mem failed\n");
            return -ENOMEM;
        }

        if (copy_from_user
            ((u8 *) pBuff, (u8 *) a_u4Param,
                sizeof(struct stCAM_CAL_INFO_STRUCT))) {
            /*get input structure address */
            kfree(pBuff);
            pr_debug("ioctl copy from user failed\n");
            return -EFAULT;
        }

        ptempbuf = (struct stCAM_CAL_INFO_STRUCT *)pBuff;

        if ((ptempbuf->u4Length <= 0) ||
            (ptempbuf->u4Length > CAM_CAL_MAX_BUF_SIZE)) {
            kfree(pBuff);
            pr_debug("Buffer Length Error!\n");
            return -EFAULT;
        }

        pu1Params = kmalloc(ptempbuf->u4Length, GFP_KERNEL);

        if (pu1Params == NULL) {
            kfree(pBuff);
            pr_debug("ioctl allocate pu1Params mem failed\n");
            return -ENOMEM;
        }

        if (copy_from_user
            ((u8 *) pu1Params, (u8 *) ptempbuf->pu1Params,
            ptempbuf->u4Length)) {
            kfree(pBuff);
            kfree(pu1Params);
            pr_debug("ioctl copy from user failed\n");
            return -EFAULT;
        }
    }
    if (ptempbuf == NULL) { /*It have to add */
        pr_debug("ptempbuf is Null !!!");
        return -EFAULT;
    }
    switch (a_u4Command) {

    case CAM_CALIOC_S_WRITE:    /*Note: Write Command is Unverified! */
        pr_debug("CAM_CALIOC_S_WRITE start!\n");
#ifdef CAM_CALGETDLT_DEBUG
        do_gettimeofday(&ktv1);
#endif

        pcmdInf = EEPROM_get_cmd_info_ex(ptempbuf->sensorID,
            ptempbuf->deviceID);

        /* Check the max size if specified */
        if (pcmdInf != NULL &&
            (pcmdInf->maxEepromSize != 0) &&
            (pcmdInf->maxEepromSize <
             (ptempbuf->u4Offset + ptempbuf->u4Length))) {
            pr_debug("Error!! not support address >= 0x%x!!\n",
                 pcmdInf->maxEepromSize);
            kfree(pBuff);
            kfree(pu1Params);
            return -EFAULT;
        }

        if (pcmdInf != NULL && g_lastDevID != ptempbuf->deviceID) {
            if (EEPROM_set_i2c_bus(ptempbuf->deviceID,
                           pcmdInf) != 0) {
                pr_debug("deviceID Error!\n");
                kfree(pBuff);
                kfree(pu1Params);
                return -EFAULT;
            }
            g_lastDevID = ptempbuf->deviceID;
        }

        if (pcmdInf != NULL) {
            if (pcmdInf->writeCMDFunc != NULL) {
                i4RetValue = pcmdInf->writeCMDFunc(
                    pcmdInf->client,
                    ptempbuf->u4Offset, pu1Params,
                    ptempbuf->u4Length);
            } else
                pr_debug("pcmdInf->writeCMDFunc == NULL\n");
        } else
            pr_debug("pcmdInf == NULL\n");

#ifdef CAM_CALGETDLT_DEBUG
        do_gettimeofday(&ktv2);
        if (ktv2.tv_sec > ktv1.tv_sec)
            TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
        else
            TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

        pr_debug("Write data %d bytes take %lu us\n",
            ptempbuf->u4Length, TimeIntervalUS);
#endif
        pr_debug("CAM_CALIOC_S_WRITE End!\n");
        break;

    case CAM_CALIOC_G_READ:
        pr_debug("CAM_CALIOC_G_READ start! offset=%d, length=%d\n",
            ptempbuf->u4Offset, ptempbuf->u4Length);

#ifdef CAM_CALGETDLT_DEBUG
        do_gettimeofday(&ktv1);
#endif
#if defined(CONFIG_WT_PROJECT_T99651AA2)
        //+bug591033 cdzk, modify, 2020/10/27, the otp porting
        LOG_INF("SensorID=%x DeviceID=%x\n", ptempbuf->sensorID, ptempbuf->deviceID);
        pr_debug("%d,1CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
        if((ptempbuf->deviceID == 0x01)&&(g_eepromMainData != NULL)){
            u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
                //LOG_INF("read in main\n");
                if((g_eepromMainData->dataBuffer)&&(totalLength <= g_eepromMainData->dataLength)){
                    if(ptempbuf->u4Offset == 1){
                        memcpy(pu1Params,(u8*)&g_eepromMainData->sensorVendorid,4);
                        pr_debug("%d,ifCAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                    } else {
                        memcpy(pu1Params,g_eepromMainData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                        pr_debug("%d,ifCAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                }
                i4RetValue = ptempbuf->u4Length;
            } else {
                LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMainData->dataBuffer,totalLength,g_eepromMainData->dataLength);
            }
        } else if((ptempbuf->deviceID == 0x02) && (ptempbuf->sensorID == 0x556)) {
            if(ptempbuf->u4Offset == 1){
                memcpy(pu1Params,(u8 *)&hi556_vendor_id,4);
            }else{
                pr_debug("%d,hi556,ifCAM_CALIOC_G_READ start! offset=0x%x, length=0x%x,pu1Params:0x%x,sensorid:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params,ptempbuf->sensorID);
                if (ptempbuf->u4Length == 0x07 && ptempbuf->u4Offset == 0x40B){//HI556 MID data
                    memcpy(pu1Params,(u8 *)&hi556_module_id,ptempbuf->u4Length);
                }
                if (ptempbuf->u4Length == 0x10 && ptempbuf->u4Offset == 0x41b && hi556_awb_valid){//HI556 AWB data
                    memcpy(pu1Params,(u8 *) &hi556_data_awb[0],ptempbuf->u4Length);
                }
                if (ptempbuf->u4Length == 0x74C && ptempbuf->u4Offset == 0x44f && hi556_lsc_valid){//HI556 LSC data
                    memcpy(pu1Params,(u8 *) &hi556_data_lsc[0],ptempbuf->u4Length);
                }
            }
            i4RetValue = ptempbuf->u4Length;
        } else if((ptempbuf->deviceID == 0x02) && (ptempbuf->sensorID == 0x5035)) {
            if(ptempbuf->u4Offset == 1){
                memcpy(pu1Params,(u8 *)&gc5035_vendor_id,4);
            }else{
                pr_debug("%d,gc5035,ifCAM_CALIOC_G_READ start! offset=0x%x, length=0x%x,pu1Params:0x%x,sensorid:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params,ptempbuf->sensorID);
                if (ptempbuf->u4Length == 0x07 && ptempbuf->u4Offset == 0xa48){//GC5035 MID data
                    memcpy(pu1Params,(u8 *)&gc5035_module_id,ptempbuf->u4Length);
                }
                if (ptempbuf->u4Length == 0x10 && ptempbuf->u4Offset == 0xb08 && gc5035_awb_valid){//GC5035 AWB data
                    memcpy(pu1Params,(u8 *) &gc5035_data_awb[0],ptempbuf->u4Length);
                }
            }
            i4RetValue = ptempbuf->u4Length;
        }else {

            pr_debug("SensorID=%x DeviceID=%x\n",
                ptempbuf->sensorID, ptempbuf->deviceID);
            pcmdInf = EEPROM_get_cmd_info_ex(
                ptempbuf->sensorID,
                ptempbuf->deviceID);

            /* Check the max size if specified */
            if (pcmdInf != NULL &&
                (pcmdInf->maxEepromSize != 0) &&
                (pcmdInf->maxEepromSize <
                 (ptempbuf->u4Offset + ptempbuf->u4Length))) {
                pr_debug("Error!! not support address >= 0x%x!!\n",
                     pcmdInf->maxEepromSize);
                kfree(pBuff);
                kfree(pu1Params);
                return -EFAULT;
            }

            if (pcmdInf != NULL && g_lastDevID != ptempbuf->deviceID) {
                if (EEPROM_set_i2c_bus(ptempbuf->deviceID,
                               pcmdInf) != 0) {
                    pr_debug("deviceID Error!\n");
                    kfree(pBuff);
                    kfree(pu1Params);
                    return -EFAULT;
                }
                g_lastDevID = ptempbuf->deviceID;
            }

            if (pcmdInf != NULL) {
                if (pcmdInf->readCMDFunc != NULL)
                    i4RetValue =
                        pcmdInf->readCMDFunc(pcmdInf->client,
                                  ptempbuf->u4Offset,
                                  pu1Params,
                                  ptempbuf->u4Length);
                else {
                    pr_debug("pcmdInf->readCMDFunc == NULL\n");
                    kfree(pBuff);
                    kfree(pu1Params);
                    return -EFAULT;
                }
            }
        }
        //-bug591033 cdzk, modify, 2020/10/27, the otp porting
//+bug584789 chenbocheng.wt, add, 2020/10/30, add main camera s5kgm1 and wide camera hi556 otp code
#elif defined(CONFIG_WT_PROJECT_T99653AA1)
        LOG_INF("SensorID=%x DeviceID=%x\n", ptempbuf->sensorID, ptempbuf->deviceID);
        pr_debug("%d,1CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
        if((ptempbuf->deviceID == 0x01)&&(g_eepromMainData != NULL)){
            u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
                //LOG_INF("read in main\n");
                if((g_eepromMainData->dataBuffer)&&(totalLength <= g_eepromMainData->dataLength)){
                    if(ptempbuf->u4Offset == 1){
                        memcpy(pu1Params,(u8*)&g_eepromMainData->sensorVendorid,4);
                        pr_debug("%d,fCAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                    } else {
                        memcpy(pu1Params,g_eepromMainData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                        pr_debug("%d,ifCAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                }
                i4RetValue = ptempbuf->u4Length;
            } else {
                LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMainData->dataBuffer,totalLength,g_eepromMainData->dataLength);
            }
        }else if(ptempbuf->deviceID == 0x02 &&ptempbuf->sensorID == 0x846) {
            if(ptempbuf->u4Offset == 0x201 && ptempbuf->u4Length == 0x04){ //check vendor id
                memcpy(pu1Params,(u8 *)&hi846_vendor_id,4);
            }else{
                pr_debug("%d,hi846,ifCAM_CALIOC_G_READ start! offset=0x%x, length=0x%x,pu1Params:0x%x,sensorid:0x%x\n",__LINE__,
                    ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params,ptempbuf->sensorID);
                if (ptempbuf->sensorID == 0x846 && ptempbuf->u4Length == 0x07 && ptempbuf->u4Offset == 0x201){//HI846 MID data
                    memcpy(pu1Params,(u8 *)&hi846_module_id,ptempbuf->u4Length);
                }
                if (ptempbuf->sensorID == 0x846 && ptempbuf->u4Length == 0x10 && ptempbuf->u4Offset == 0x21A && hi846_awb_valid){//HI846 AWB data
                    memcpy(pu1Params,(u8 *) &hi846_data_awb[0],ptempbuf->u4Length);
                }
                if (ptempbuf->sensorID == 0x846 && ptempbuf->u4Length == 0x74C && ptempbuf->u4Offset == 0x24E && hi846_lsc_valid){//HI846 LSC data
                    memcpy(pu1Params,(u8 *) &hi846_data_lsc[0],ptempbuf->u4Length);
                }
            }
            i4RetValue = ptempbuf->u4Length;
        } else if((ptempbuf->deviceID == 0x08) && (ptempbuf->sensorID == 0x556)) {
            if(ptempbuf->u4Offset == 1){
                memcpy(pu1Params,(u8 *)&hi556_vendor_id,4);
            }else{
                pr_debug("%d,hi556,ifCAM_CALIOC_G_READ start! offset=0x%x, length=0x%x,pu1Params:0x%x,sensorid:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params,ptempbuf->sensorID);
                if (ptempbuf->sensorID == 0x556 && ptempbuf->u4Length == 0x07 && ptempbuf->u4Offset == 0x40B){//HI846 MID data
                    memcpy(pu1Params,(u8 *)&hi556_module_id,ptempbuf->u4Length);
                }
                if (ptempbuf->sensorID == 0x556 && ptempbuf->u4Length == 0x10 && ptempbuf->u4Offset == 0x41b && hi556_awb_valid){//HI556 AWB data
                    memcpy(pu1Params,(u8 *) &hi556_data_awb[0],ptempbuf->u4Length);
                }
                if (ptempbuf->sensorID == 0x556 && ptempbuf->u4Length == 0x74C && ptempbuf->u4Offset == 0x44f && hi556_lsc_valid){//HI556 LSC data
                    memcpy(pu1Params,(u8 *) &hi556_data_lsc[0],ptempbuf->u4Length);
                }
            }
            i4RetValue = ptempbuf->u4Length;
        } else if((ptempbuf->deviceID == 0x08) && (ptempbuf->sensorID == 0x5035) && (g_eepromMainWideData != NULL)) {  //+bug596770,liuxingyu.wt,ADD,2020/11/10, add gc5035 cam eeprom
            u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
            LOG_INF("gc5035 read otp in wide totalLength:%d, dataLength:%d\n",totalLength,g_eepromMainWideData->dataLength);
            if((g_eepromMainWideData->dataBuffer)&&(totalLength <= g_eepromMainWideData->dataLength)){
                if(ptempbuf->u4Offset == 1){
                    memcpy(pu1Params,(u8*)&g_eepromMainWideData->sensorVendorid,4);
                    LOG_INF("%d, wide CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                } else {
                    memcpy(pu1Params,g_eepromMainWideData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                    LOG_INF("%d, wide CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                }
                i4RetValue = ptempbuf->u4Length;
            } else {
                LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMainMicroData->dataBuffer,totalLength,g_eepromMainMicroData->dataLength);
            }
        } else if((ptempbuf->deviceID == 0x02) && (ptempbuf->sensorID == 0x8044) && (g_eepromSubData != NULL)){   //+bug596770,liuxingyu.wt,ADD,2020/11/10, add gc8034 cam eeprom
            u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
            LOG_INF("gc8034 read otp in front totalLength:%d, dataLength:%d\n",totalLength,g_eepromSubData->dataLength);
            if((g_eepromSubData->dataBuffer)&&(totalLength <= g_eepromSubData->dataLength)){
                if(ptempbuf->u4Offset == 1){
                    memcpy(pu1Params,(u8*)&g_eepromSubData->sensorVendorid,4);
                    LOG_INF("%d, gc8034 front CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                } else {
                    memcpy(pu1Params,g_eepromSubData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                    LOG_INF("%d, gc8034 front CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                }
                i4RetValue = ptempbuf->u4Length;
            } else {
                LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromSubData->dataBuffer,totalLength,g_eepromSubData->dataLength);
            }
        } else if((ptempbuf->deviceID == 0x10)&&(g_eepromMainMicroData != NULL)){
            u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
            LOG_INF("read otp in macro totalLength:%d, dataLength:%d\n",totalLength,g_eepromMainMicroData->dataLength);
            if((g_eepromMainMicroData->dataBuffer)&&(totalLength <= g_eepromMainMicroData->dataLength)){
                if(ptempbuf->u4Offset == 1){
                    memcpy(pu1Params,(u8*)&g_eepromMainMicroData->sensorVendorid,4);
                    LOG_INF("%d, macro CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                } else {
                    memcpy(pu1Params,g_eepromMainMicroData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                    LOG_INF("%d, macro CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                            ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                }
                i4RetValue = ptempbuf->u4Length;
            } else {
                LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMainMicroData->dataBuffer,totalLength,g_eepromMainMicroData->dataLength);
            }
            //-bug584789,zhouyikuan.wt,ADD,2020/11/04,change macro cam eeprom eepromdata
            //-bug596770,houzhipeng.wt,ADD,2020/11/03, add macro cam eeprom
          } else {
            pr_debug("SensorID=%x DeviceID=%x\n",
                    ptempbuf->sensorID, ptempbuf->deviceID);
            pcmdInf = EEPROM_get_cmd_info_ex(
                ptempbuf->sensorID,
                ptempbuf->deviceID);

            /* Check the max size if specified */
            if (pcmdInf != NULL &&
                (pcmdInf->maxEepromSize != 0) &&
                (pcmdInf->maxEepromSize <
                 (ptempbuf->u4Offset + ptempbuf->u4Length))) {
                pr_debug("Error!! not support address >= 0x%x!!\n",
                     pcmdInf->maxEepromSize);
                kfree(pBuff);
                kfree(pu1Params);
                return -EFAULT;
            }

            if (pcmdInf != NULL && g_lastDevID != ptempbuf->deviceID) {
                if (EEPROM_set_i2c_bus(ptempbuf->deviceID,
                               pcmdInf) != 0) {
                    pr_debug("deviceID Error!\n");
                    kfree(pBuff);
                    kfree(pu1Params);
                    return -EFAULT;
                }
                g_lastDevID = ptempbuf->deviceID;
            }

            if (pcmdInf != NULL) {
                if (pcmdInf->readCMDFunc != NULL)
                    i4RetValue =
                        pcmdInf->readCMDFunc(pcmdInf->client,
                                  ptempbuf->u4Offset,
                                  pu1Params,
                                  ptempbuf->u4Length);
                else {
                    pr_debug("pcmdInf->readCMDFunc == NULL\n");
                    kfree(pBuff);
                    kfree(pu1Params);
                    return -EFAULT;
                }
            }
        }
//+bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up
#elif defined(CONFIG_WT_PROJECT_T99652AA1)
                 LOG_INF("SensorID=%x DeviceID=%x\n", ptempbuf->sensorID, ptempbuf->deviceID);
                 pr_debug("%d,zhou:1CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                    ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                 if((ptempbuf->deviceID == 0x01)&&(g_eepromMainData != NULL)){
                    u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
                         //LOG_INF("read in main\n");
                         if((g_eepromMainData->dataBuffer)&&(totalLength <= g_eepromMainData->dataLength)){
                                 if(ptempbuf->u4Offset == 1){
                                         memcpy(pu1Params,(u8*)&g_eepromMainData->sensorVendorid,4);
                                         pr_debug("%d,zhou:ifCAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                                                 ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                                 } else {
                                         memcpy(pu1Params,g_eepromMainData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                                         pr_debug("%d,zhou:ifCAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                                                 ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                                 }
                                 i4RetValue = ptempbuf->u4Length;
                         } else {
                                 LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMainData->dataBuffer,totalLength,g_eepromMainData->dataLength);
                         }
                 //+bug596770,houzhipeng.wt,ADD,2020/11/03, add macro cam eeprom
                 //+bug584789,zhouyikuan.wt,ADD,2020/11/04,change macro cam eeprom eepromdata
                 } else if((ptempbuf->deviceID == 0x08)&&(g_eepromMainMicroData != NULL)){
                    u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
                         LOG_INF("read otp in macro totalLength:%d, dataLength:%d\n",totalLength,g_eepromMainMicroData->dataLength);
                         if((g_eepromMainMicroData->dataBuffer)&&(totalLength <= g_eepromMainMicroData->dataLength)){
                                 if(ptempbuf->u4Offset == 1){
                                         memcpy(pu1Params,(u8*)&g_eepromMainMicroData->sensorVendorid,4);
                                         LOG_INF("%d, macro CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                                                 ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                                 } else {
                                         memcpy(pu1Params,g_eepromMainMicroData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                                         LOG_INF("%d, macro CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                                                 ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                                 }
                                 i4RetValue = ptempbuf->u4Length;
                         } else {
                                 LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromMainMicroData->dataBuffer,totalLength,g_eepromMainMicroData->dataLength);
                         }
                 //-bug584789,zhouyikuan.wt,ADD,2020/11/04,change macro cam eeprom eepromdata
                 //-bug596770,houzhipeng.wt,ADD,2020/11/03, add macro cam eeprom
                 } else if(ptempbuf->deviceID == 0x02) {
                         //+bug584789,zhouyikuan.wt,ADD,2020/11/04,GC8034 eeprom bring up
                         if(ptempbuf->sensorID == 0x846){
                             if(ptempbuf->u4Offset == 0x201 && ptempbuf->u4Length == 0x04){ //check vendor id
                                     memcpy(pu1Params,(u8 *)&hi846_vendor_id,4);
                             }else{
                                     pr_debug("%d,zyk666:hi846,ifCAM_CALIOC_G_READ start! offset=0x%x, length=0x%x,pu1Params:0x%x,sensorid:0x%x\n",__LINE__,
                                             ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params,ptempbuf->sensorID);
                                     if (ptempbuf->u4Length == 0x07 && ptempbuf->u4Offset == 0x201){//HI846 MID data
                                             memcpy(pu1Params,(u8 *)&hi846_module_id,ptempbuf->u4Length);
                                     }
                                     if (ptempbuf->u4Length == 0x10 && ptempbuf->u4Offset == 0x21A && hi846_awb_valid){//HI846 AWB data
                                             memcpy(pu1Params,(u8 *) &hi846_data_awb[0],ptempbuf->u4Length);
                                     }
                                     if (ptempbuf->u4Length == 0x74C && ptempbuf->u4Offset == 0x24E && hi846_lsc_valid){//HI846 LSC data
                                             memcpy(pu1Params,(u8 *) &hi846_data_lsc[0],ptempbuf->u4Length);
                                     }
                             }
                           i4RetValue = ptempbuf->u4Length;
                        }else if(ptempbuf->sensorID == 0x8044){
                u32 totalLength = ptempbuf->u4Offset+ ptempbuf->u4Length;
                LOG_INF("read otp in macro totalLength:%d, dataLength:%d\n",totalLength,g_eepromSubData->dataLength);
                if((g_eepromSubData->dataBuffer)&&(totalLength <= g_eepromSubData->dataLength)){
                    if(ptempbuf->u4Offset == 1){
                        memcpy(pu1Params,(u8*)&g_eepromSubData->sensorVendorid,4);
                        LOG_INF("%d, macro CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                                    ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                                    } else {
                        memcpy(pu1Params,g_eepromSubData->dataBuffer+ptempbuf->u4Offset,ptempbuf->u4Length);
                        LOG_INF("%d, macro CAM_CALIOC_G_READ start! offset=%d, length=%d,pu1Params:0x%x\n",__LINE__,
                                    ptempbuf->u4Offset, ptempbuf->u4Length,*pu1Params);
                    }
                    i4RetValue = ptempbuf->u4Length;
                } else {
                    LOG_INF("maybe some error buf(%p)read(%d)have(%d) \n",g_eepromSubData->dataBuffer,totalLength,g_eepromSubData->dataLength);
                }
                       }
                       //-bug584789,zhouyikuan.wt,ADD,2020/11/04,GC8034 eeprom bring up
                 } else {
                    pr_debug("SensorID=%x DeviceID=%x\n",
                                    ptempbuf->sensorID, ptempbuf->deviceID);
                                    pcmdInf = EEPROM_get_cmd_info_ex(
                                    ptempbuf->sensorID,
                                    ptempbuf->deviceID);

                        /* Check the max size if specified */
                         if (pcmdInf != NULL &&
                             (pcmdInf->maxEepromSize != 0) &&
                             (pcmdInf->maxEepromSize <
                             (ptempbuf->u4Offset + ptempbuf->u4Length))) {
                                 pr_debug("Error!! not support address >= 0x%x!!\n",
                                                  pcmdInf->maxEepromSize);
                                 kfree(pBuff);
                                 kfree(pu1Params);
                                 return -EFAULT;
                         }

                         if (pcmdInf != NULL && g_lastDevID != ptempbuf->deviceID) {
                       if (EEPROM_set_i2c_bus(ptempbuf->deviceID,
                                     pcmdInf) != 0) {
                                         pr_debug("deviceID Error!\n");
                                         kfree(pBuff);
                                         kfree(pu1Params);
                                         return -EFAULT;
                                 }
                                 g_lastDevID = ptempbuf->deviceID;
                         }

                         if (pcmdInf != NULL) {
                                 if (pcmdInf->readCMDFunc != NULL)
                                         i4RetValue =
                                             pcmdInf->readCMDFunc(pcmdInf->client,
                                             ptempbuf->u4Offset,
                                             pu1Params,
                                             ptempbuf->u4Length);
                                 else {
                                         pr_debug("pcmdInf->readCMDFunc == NULL\n");
                                         kfree(pBuff);
                                         kfree(pu1Params);
                                         return -EFAULT;
                                 }
                         }
                }
//-bug584789,zhouyikuan.wt,ADD,2020/20/28,HI846 eeprom bring up
#else
        pr_debug("SensorID=%x DeviceID=%x\n",
                ptempbuf->sensorID, ptempbuf->deviceID);
            pcmdInf = EEPROM_get_cmd_info_ex(
                ptempbuf->sensorID,
                ptempbuf->deviceID);

            /* Check the max size if specified */
            if (pcmdInf != NULL &&
                (pcmdInf->maxEepromSize != 0) &&
                (pcmdInf->maxEepromSize <
                 (ptempbuf->u4Offset + ptempbuf->u4Length))) {
                pr_debug("Error!! not support address >= 0x%x!!\n",
                     pcmdInf->maxEepromSize);
                kfree(pBuff);
                kfree(pu1Params);
                return -EFAULT;
            }

            if (pcmdInf != NULL && g_lastDevID != ptempbuf->deviceID) {
                if (EEPROM_set_i2c_bus(ptempbuf->deviceID,
                               pcmdInf) != 0) {
                    pr_debug("deviceID Error!\n");
                    kfree(pBuff);
                    kfree(pu1Params);
                    return -EFAULT;
                }
                g_lastDevID = ptempbuf->deviceID;
            }

            if (pcmdInf != NULL) {
                if (pcmdInf->readCMDFunc != NULL)
                    i4RetValue =
                        pcmdInf->readCMDFunc(pcmdInf->client,
                                  ptempbuf->u4Offset,
                                  pu1Params,
                                  ptempbuf->u4Length);
                else {
                    pr_debug("pcmdInf->readCMDFunc == NULL\n");
                    kfree(pBuff);
                    kfree(pu1Params);
                    return -EFAULT;
                }
            }
#endif
//-bug584789 chenbocheng.wt, add, 2020/10/30, add main camera s5kgm1 and wide camera hi556 otp code
#ifdef CAM_CALGETDLT_DEBUG
        do_gettimeofday(&ktv2);
        if (ktv2.tv_sec > ktv1.tv_sec)
            TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
        else
            TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

        pr_debug("Read data %d bytes take %lu us\n",
            ptempbuf->u4Length, TimeIntervalUS);
#endif
        break;

    default:
        pr_debug("No CMD\n");
        i4RetValue = -EPERM;
        break;
    }

    if (_IOC_READ & _IOC_DIR(a_u4Command)) {
        if (copy_to_user
            ((u8 __user *) ptempbuf->pu1Params, (u8 *) pu1Params,
                ptempbuf->u4Length)) {
            kfree(pBuff);
            kfree(pu1Params);
            pr_debug("ioctl copy to user failed\n");
            return -EFAULT;
        }
    }

    kfree(pBuff);
    kfree(pu1Params);
    return i4RetValue;
}

static int EEPROM_drv_open(struct inode *a_pstInode, struct file *a_pstFile)
{
    int ret = 0;

    pr_debug("%s start\n", __func__);
    spin_lock(&g_spinLock);
    if (g_drvOpened) {
        spin_unlock(&g_spinLock);
        pr_debug("Opened, return -EBUSY\n");
        ret = -EBUSY;
    } else {
        g_drvOpened = 1;
        spin_unlock(&g_spinLock);
    }
    mdelay(2);

    return ret;
}

static int EEPROM_drv_release(struct inode *a_pstInode, struct file *a_pstFile)
{
    spin_lock(&g_spinLock);
    g_drvOpened = 0;
    spin_unlock(&g_spinLock);

    return 0;
}

static const struct file_operations g_stCAM_CAL_fops1 = {
    .owner = THIS_MODULE,
    .open = EEPROM_drv_open,
    .release = EEPROM_drv_release,
    /*.ioctl = CAM_CAL_Ioctl */
#ifdef CONFIG_COMPAT
    .compat_ioctl = EEPROM_drv_compat_ioctl,
#endif
    .unlocked_ioctl = EEPROM_drv_ioctl
};

/***********************************************
 *
 ***********************************************/

#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1
static inline int EEPROM_chrdev_register(void)
{
    struct device *device = NULL;

    pr_debug("%s Start\n", __func__);

#if CAM_CAL_DYNAMIC_ALLOCATE_DEVNO
    if (alloc_chrdev_region(&g_devNum, 0, 1, CAM_CAL_DRV_NAME)) {
        pr_debug("Allocate device no failed\n");
        return -EAGAIN;
    }
#else
    if (register_chrdev_region(g_devNum, 1, CAM_CAL_DRV_NAME)) {
        pr_debug("Register device no failed\n");
        return -EAGAIN;
    }
#endif

    g_charDrv = cdev_alloc();

    if (g_charDrv == NULL) {
        unregister_chrdev_region(g_devNum, 1);
        pr_debug("Allocate mem for kobject failed\n");
        return -ENOMEM;
    }

    cdev_init(g_charDrv, &g_stCAM_CAL_fops1);
    g_charDrv->owner = THIS_MODULE;

    if (cdev_add(g_charDrv, g_devNum, 1)) {
        pr_debug("Attatch file operation failed\n");
        unregister_chrdev_region(g_devNum, 1);
        return -EAGAIN;
    }

    g_drvClass = class_create(THIS_MODULE, "CAM_CALdrv1");
    if (IS_ERR(g_drvClass)) {
        int ret = PTR_ERR(g_drvClass);

        pr_debug("Unable to create class, err = %d\n", ret);
        return ret;
    }
    device = device_create(g_drvClass, NULL, g_devNum, NULL,
        CAM_CAL_DRV_NAME);
    pr_debug("%s End\n", __func__);

    return 0;
}

static void EEPROM_chrdev_unregister(void)
{
    /*Release char driver */

    class_destroy(g_drvClass);

    device_destroy(g_drvClass, g_devNum);

    cdev_del(g_charDrv);

    unregister_chrdev_region(g_devNum, 1);
}

/***********************************************
 *
 ***********************************************/

static int __init EEPROM_drv_init(void)
{
    pr_debug("%s Start!\n", __func__);

    if (platform_driver_register(&g_stEEPROM_HW_Driver)) {
        pr_debug("failed to register EEPROM driver i2C main\n");
        return -ENODEV;
    }

    if (platform_device_register(&g_platDev)) {
        pr_debug("failed to register EEPROM device");
        return -ENODEV;
    }

    EEPROM_chrdev_register();

    pr_debug("%s End!\n", __func__);
    return 0;
}

static void __exit EEPROM_drv_exit(void)
{

    platform_device_unregister(&g_platDev);
    platform_driver_unregister(&g_stEEPROM_HW_Driver);

    EEPROM_chrdev_unregister();
}
module_init(EEPROM_drv_init);
module_exit(EEPROM_drv_exit);

MODULE_DESCRIPTION("EEPROM Driver");
MODULE_AUTHOR("MM3_SW2");
MODULE_LICENSE("GPL");
