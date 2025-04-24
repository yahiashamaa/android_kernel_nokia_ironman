#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of_fdt.h>
#include <asm/setup.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/kdev_t.h>

#define READ_SYNC		0
#define WRITE_SYNC		REQ_SYNC

extern struct gendisk *emmc_disk;
static struct platform_device *proinfo_pdev;
static struct device *proinfo_root_dev;

#define UNDISTRIBUTED 99

//define type
typedef enum{
    WT_PROINFO_oem_clear = 1,
    WT_PROINFO_psn = 5,
    WT_PROINFO_sn = 4,
    WT_PROINFO_IMEI0 = 11,
    WT_PROINFO_IMEI1 = 12,
    WT_PROINFO_MEID = 13,
    WT_PROINFO_color_id = 6,
    WT_PROINFO_sku_id = 7,
    WT_PROINFO_factoryreset_date = 8,
    WT_PROINFO_build_type = 9,
    WT_PROINFO_bt = 14,
    WT_PROINFO_wlan = 15,
    WT_PROINFO_ta_code = 16,
    WT_PROINFO_block_fastboot_mode = 17,
    WT_PROINFO_block_factory_reset = 18,
    WT_PROINFO_remote_lock_value = 22,

}wt_proinfo_type;

#define WT_PROINFO_STRING_LEN 30
#define WT_PROINFO_IMEI_LEN 8
#define WT_PROINFO_REMOTE_LOCK_VLAUE_LEN 256
#define SIZE_4K 4096
#define WT_PROINFO_1_LEN 1
#define WT_PROINFO_8_LEN 8

#define PHONE_INFO_PATH "/dev/block/platform/bootdevice/by-name/proinfo"
#define OEM_INFO_PATH "/dev/block/platform/bootdevice/by-name/oem"

static int wt_proinfo_read(wt_proinfo_type type, char* buf);
static int wt_proinfo_write(wt_proinfo_type type, const char* buf, int len);

//define macor ATTR
#define WT_PROINFO_CREATE_ATTR(name) \
static ssize_t proinfo_##name##_store(struct device * dev, struct device_attribute *attr, const char * buf,size_t count); \
static ssize_t proinfo_##name##_show(struct device *dev, struct device_attribute *attr, char *buf); \
\
\
static ssize_t proinfo_##name##_store(struct device * dev, struct device_attribute *attr, const char * buf,\
                  size_t count)\
{\
    int ret = -1;\
    printk("entry  %s\n",__FUNCTION__);\
\
    ret = wt_proinfo_write(WT_PROINFO_##name, buf, count);\
    \
    return count;\
}\
\
static ssize_t proinfo_##name##_show(struct device *dev, struct device_attribute *attr, char *buf)\
{\
    int ret = -1;\
    char pbuf[257];\
    printk("entry  %s\n",__FUNCTION__);\
\
    ret = wt_proinfo_read(WT_PROINFO_##name,pbuf);\
\
    return sprintf(buf, "%s\n",pbuf);\
}\
\
static DEVICE_ATTR(name, S_IWUSR|S_IRUSR|S_IROTH|S_IRGRP, proinfo_##name##_show, proinfo_##name##_store);

//define function
WT_PROINFO_CREATE_ATTR(oem_clear)
WT_PROINFO_CREATE_ATTR(psn)
WT_PROINFO_CREATE_ATTR(sn)
WT_PROINFO_CREATE_ATTR(color_id)
WT_PROINFO_CREATE_ATTR(sku_id)
WT_PROINFO_CREATE_ATTR(factoryreset_date)
WT_PROINFO_CREATE_ATTR(build_type)
WT_PROINFO_CREATE_ATTR(IMEI0)

WT_PROINFO_CREATE_ATTR(IMEI1)
WT_PROINFO_CREATE_ATTR(MEID)
WT_PROINFO_CREATE_ATTR(bt)
WT_PROINFO_CREATE_ATTR(wlan)
WT_PROINFO_CREATE_ATTR(ta_code)
WT_PROINFO_CREATE_ATTR(block_fastboot_mode)
WT_PROINFO_CREATE_ATTR(block_factory_reset)
WT_PROINFO_CREATE_ATTR(remote_lock_value)

static int wt_create_device_files(void)
{
    int rc = 0;

    rc = device_create_file(proinfo_root_dev, &dev_attr_oem_clear);
    if (rc)
        return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_psn);
    if (rc)
         return rc;

        rc = device_create_file(proinfo_root_dev, &dev_attr_sn);
        if (rc)
               return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_color_id);
    if (rc)
        return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_sku_id);
    if (rc)
        return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_factoryreset_date);
    if (rc)
        return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_build_type);
    if (rc)
        return rc;
       rc = device_create_file(proinfo_root_dev, &dev_attr_bt);
        if (rc)
                return rc;
        rc = device_create_file(proinfo_root_dev, &dev_attr_wlan);
        if (rc)
                return rc;

        rc = device_create_file(proinfo_root_dev, &dev_attr_ta_code);
        if (rc)
                return rc;

    rc = device_create_file(proinfo_root_dev, &dev_attr_IMEI0);
    if (rc)
        return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_IMEI1);
    if (rc)
        return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_MEID);
    if (rc)
        return rc;

    rc = device_create_file(proinfo_root_dev, &dev_attr_block_fastboot_mode);
    if (rc)
        return rc;
    rc = device_create_file(proinfo_root_dev, &dev_attr_block_factory_reset);
    if (rc)
        return rc;

    rc = device_create_file(proinfo_root_dev, &dev_attr_remote_lock_value);
    if (rc)
        return rc;

    return 0;
}

#define MMC_BLOCK_SIZE (512)
static dev_t emmc_lookup_partition(const char *part_name, sector_t *start, sector_t *nr_sect)
{
	struct disk_part_iter piter;
	struct hd_struct *part;

	dev_t devt = MKDEV(0, 0);
	if (!emmc_disk) {
		printk("[mzss] emmc disk = null\n");

		return devt;
	}

	disk_part_iter_init(&piter, emmc_disk, DISK_PITER_INCL_EMPTY);
	while ((part = disk_part_iter_next(&piter))) {
		if (part->info && !strcmp(part->info->volname, part_name)) {
			devt = part->__dev.devt;
			*start = part->start_sect;
			*nr_sect = part->nr_sects;
			break;
		}
	}
	disk_part_iter_exit(&piter);

	return devt;
}

static int emmc_block_rw(int write, sector_t index, void *buffer, size_t len)
{
	struct block_device *bdev;
	struct buffer_head *bh = NULL;
	fmode_t mode = FMODE_READ;
	int err = -EIO;

	if (len > MMC_BLOCK_SIZE)
		return -EINVAL;

	bdev = bdget(MKDEV(MMC_BLOCK_MAJOR, 0));
	if (!bdev)
		return -EIO;

	mode = write ? FMODE_WRITE : FMODE_READ;
	if (blkdev_get(bdev, mode, NULL)) {
		bdput(bdev);
		goto out;
	}

	set_blocksize(bdev, MMC_BLOCK_SIZE);

	bh = __getblk(bdev, index, MMC_BLOCK_SIZE);

	if (bh) {
		clear_buffer_uptodate(bh);
		get_bh(bh);
		lock_buffer(bh);
		bh->b_end_io = end_buffer_read_sync;
		submit_bh(REQ_OP_READ, READ_SYNC, bh);
		wait_on_buffer(bh);
		pr_err("emmc read sucess!!\n");
		if (unlikely(!buffer_uptodate(bh))) {
			pr_err("emmc read error!!\n");
			goto out;
		}
		if (write) {
			lock_buffer(bh);
			memcpy(bh->b_data, buffer, len);
			bh->b_end_io = end_buffer_write_sync;
			get_bh(bh);
			submit_bh(REQ_OP_WRITE, WRITE_SYNC, bh);
			wait_on_buffer(bh);
			pr_err("emmc go to write sucess!!\n");
			if (unlikely(!buffer_uptodate(bh))) {
				pr_err("emmc write error!!\n");
				goto out;
			}
		} else {
			memcpy(buffer, bh->b_data, len);
			pr_err("emmc write sucess!!\n");
		}
		err = 0;
	} else {
		pr_info("%s error\n", __func__);
	}

out:
	brelse(bh);
	blkdev_put(bdev, mode);

	return err;
}

int emmc_partition_rw(const char *part_name, int write, loff_t offset,void *buffer, size_t len)
{
	int ret = 0;
	sector_t index;
	unsigned char *p = buffer;

	dev_t devt;
	sector_t start=0, nr_sect=0;

	if (buffer == NULL)
		return -EINVAL;
	printk("[mzss123]%s: offset(%lld) unalign to 512Byte!\n", __func__, offset);
	if (offset % MMC_BLOCK_SIZE) {
		printk("[mzss]%s: offset(%lld) unalign to 512Byte!\n", __func__, offset);
		return -EINVAL;
	}

	devt = emmc_lookup_partition(part_name, &start, &nr_sect);
	if (!devt) {
		printk("[mzss]%s: can't find eMMC partition(%s)\n", __func__, part_name);
		return -ENODEV;
	}

	if (offset < 0 || (offset + len) >= nr_sect * MMC_BLOCK_SIZE) {
		printk("[mzss]%s: access area exceed parition(%s) range.\n", __func__, part_name);
		return -EINVAL;
	}

	index = start + offset / MMC_BLOCK_SIZE;

	while (len > 0) {
		size_t size = len;

		if (size > MMC_BLOCK_SIZE)
			size = MMC_BLOCK_SIZE;

		ret = emmc_block_rw(write, index, p, size);
		if (ret) {
			printk("[mzss]%s (%lu) error %d\n", __func__, (unsigned long)len, ret);
			break;
		}

		len -= size;
		index++;
		p += MMC_BLOCK_SIZE;
	}

	return ret;
}
EXPORT_SYMBOL(emmc_partition_rw);

static char temp_buf[SIZE_4K] = { };

static int wt_proinfo_read(wt_proinfo_type type, char* buf)
{
    //struct file *fp = NULL;

    char fbuf[WT_PROINFO_STRING_LEN]={0};
    char fl_buf[257]={0};
    int ret= 0;
    int len =0;

    printk("%s\n",__func__);

    /*
    if((type != WT_PROINFO_block_fastboot_mode) && (type != WT_PROINFO_block_factory_reset)
        && (type != WT_PROINFO_factoryreset_date) && (type != WT_PROINFO_remote_lock_value)) {
        fp = filp_open(PHONE_INFO_PATH, O_RDWR | O_CREAT, 0);
        if (IS_ERR(fp)){
            printk("[TYPE] %s: open proinfo path error\n", __func__);
               return -1;
        }
    }else{
        if(type != WT_PROINFO_factoryreset_date){
            fp = filp_open(OEM_INFO_PATH, O_RDWR | O_CREAT, 0);
            if (IS_ERR(fp)){
                printk("[RTX] %s: open phone info path error\n", __func__);
                return -1;
            }
        }
    }
    msleep(50);
    */

    switch(type) {
        case WT_PROINFO_color_id:
            printk("%s read color_id \n", __func__);

            ret = emmc_partition_rw("proinfo", 0, 0, temp_buf, SIZE_4K / 2);
            if (ret < 0) {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                return -1;
            }
            memcpy(fbuf, temp_buf + 341 + 238, WT_PROINFO_1_LEN);

            /*
            fp->f_pos = fp->f_pos + 341 + 238;
            ret = kernel_read(fp, (loff_t) fp->f_pos, (char *) fbuf, (unsigned long) WT_PROINFO_1_LEN);
            if (ret < 0)
            {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                filp_close(fp, NULL);
                return -1;
            }
            */

            fbuf[WT_PROINFO_1_LEN-1] += '0';
            break;
        case WT_PROINFO_sku_id:
            printk("%s read sku_id \n", __func__);

            ret = emmc_partition_rw("proinfo", 0, 0, temp_buf, SIZE_4K / 2);
            if (ret < 0) {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                return -1;
            }
            memcpy(fbuf, temp_buf + 341 + 934, WT_PROINFO_8_LEN);

            /*
            fp->f_pos = fp->f_pos + 341 + 934;
            ret = kernel_read(fp, (loff_t) fp->f_pos, (char *) fbuf, (unsigned long) WT_PROINFO_8_LEN);
            if (ret < 0)
            {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                filp_close(fp, NULL);
                return -1;
            }
            */

            break;
        case WT_PROINFO_ta_code:
            printk("%s read ta_code \n", __func__);

            ret = emmc_partition_rw("proinfo", 0, 0, temp_buf, SIZE_4K / 2);
            if (ret < 0) {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                return -1;
            }
            memcpy(fbuf, temp_buf + 341 + 942, WT_PROINFO_8_LEN);

            /*
            fp->f_pos = fp->f_pos + 341 + 942;
            ret = kernel_read(fp, (loff_t) fp->f_pos, (char *) fbuf, (unsigned long) WT_PROINFO_8_LEN);
            if (ret < 0)
            {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                filp_close(fp, NULL);
                return -1;
            }
            */

            break;

        case WT_PROINFO_block_fastboot_mode:
            printk("%s read block_fastboot_mode \n", __func__);

            ret = emmc_partition_rw("oem", 0, type * SIZE_4K, temp_buf, SIZE_4K / 8);
            if (ret < 0) {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                return -1;
            }
            memcpy(fbuf, temp_buf, WT_PROINFO_STRING_LEN);

            /*
            fp->f_pos = fp->f_pos + type * SIZE_4K;
            ret = kernel_read(fp, (loff_t) fp->f_pos, (char *) fbuf, (unsigned long) WT_PROINFO_STRING_LEN);
            if (ret < 0)
            {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                filp_close(fp, NULL);
                return -1;
            }
            */

            break;
        case WT_PROINFO_block_factory_reset:
            printk("%s read block_factory_reset \n", __func__);

            ret = emmc_partition_rw("oem", 0, type * SIZE_4K, temp_buf, SIZE_4K / 8);
            if (ret < 0) {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                return -1;
            }
            memcpy(fbuf, temp_buf, WT_PROINFO_STRING_LEN);

            /*
            fp->f_pos = fp->f_pos + type * SIZE_4K;
            ret = kernel_read(fp, (loff_t) fp->f_pos, (char *) fbuf, (unsigned long) WT_PROINFO_STRING_LEN);
            if (ret < 0)
            {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                filp_close(fp, NULL);
                return -1;
            }
            */

            break;
         case WT_PROINFO_remote_lock_value:
            printk("%s read remote_lock_value \n", __func__);

            ret = emmc_partition_rw("oem", 0, type * SIZE_4K, temp_buf, SIZE_4K / 8);
            if (ret < 0) {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                return -1;
            }
            memcpy(fl_buf, temp_buf, WT_PROINFO_REMOTE_LOCK_VLAUE_LEN);

            /*
            fp->f_pos = fp->f_pos + type * SIZE_4K;
            ret = kernel_read(fp, (loff_t) fp->f_pos, (char *) fl_buf, (unsigned long) WT_PROINFO_REMOTE_LOCK_VLAUE_LEN);
            if (ret < 0)
            {
                printk("%s: Read bytes from proinfo failed! %d\n", __func__, ret);
                filp_close(fp, NULL);
                return -1;
            }
            */

            break;
        case WT_PROINFO_factoryreset_date:
            printk("%s read factory[%d] \n", __func__, type);
            ret = emmc_partition_rw("oem",  0, type * SIZE_4K,(char *) fbuf, (unsigned long) WT_PROINFO_STRING_LEN);
            break;
        default:
            break;
    }

    /*
    if(type != WT_PROINFO_factoryreset_date){
        filp_close(fp, NULL);
    }
    */

    if(type == WT_PROINFO_remote_lock_value){
        len = strlen(fl_buf);
        if(len <= WT_PROINFO_REMOTE_LOCK_VLAUE_LEN){
            strcpy(buf,fl_buf);
            printk("%s,read fl_buf:%s, len = %d\n",__func__,buf,len);
        }
        return len;
    }
    len = strlen(fbuf);

    if(len < WT_PROINFO_STRING_LEN){
        strcpy(buf,fbuf);
        printk("%s,read fbuf:%s\n",__func__,fbuf);
        return len;
    }
    else{
        return 0;
    }
}

//store in buf, return len. buf least size is 1024
static int wt_proinfo_write(wt_proinfo_type type, const char* buf, int len)
{
    //struct file *fp = NULL;

    int ret = 0;
    char buf_tmp[257] = {0};

    printk("%s\n",__func__);
    printk("%s,%s len %d\n",__func__,buf,len);

    //max len is WT_PROINFO_STRING_LEN
    if(type != WT_PROINFO_remote_lock_value)
    {
       if(len > WT_PROINFO_STRING_LEN)
       return -1;
    }else{
     if(len > WT_PROINFO_REMOTE_LOCK_VLAUE_LEN)
       len = WT_PROINFO_REMOTE_LOCK_VLAUE_LEN;
    }

    memcpy(buf_tmp,buf,len);
    if(buf_tmp[len-1] == '\n')
        buf_tmp[len-1] = 0x00;

    ///Don't allow to echo value to protect prinfo
    if((type != WT_PROINFO_block_fastboot_mode) && (type != WT_PROINFO_block_factory_reset)
        && (type != WT_PROINFO_factoryreset_date) && (type != WT_PROINFO_remote_lock_value)
        && (type != WT_PROINFO_sku_id)) {
        printk("%s proinfo not support write this type value \n", __func__);
        return -1;
    }

    /*
    if(type != WT_PROINFO_factoryreset_date){
        fp = filp_open(OEM_INFO_PATH, O_RDWR | O_CREAT, 0);
        if (IS_ERR(fp)){
            printk("[RTX] %s: open phone info path error\n", __func__);
            return -1;
        }
    }
    */

    switch(type) {
        case WT_PROINFO_IMEI0:
        case WT_PROINFO_IMEI1:
        case WT_PROINFO_MEID:
            printk("%s proinfo not support write imei \n", __func__);
            break;

        /* just for testing fingerprint. */
        case WT_PROINFO_sku_id:
            printk("%s write sku_id[%d] \n", __func__, type);

            memcpy(temp_buf + 341 + 934, buf_tmp, len);
            ret = emmc_partition_rw("proinfo", 1, 0, temp_buf, SIZE_4K / 2);
            if (ret < 0) {
                printk("%s: write bytes to proinfo failed! %d\n", __func__, ret);
                return -1;
            }

            break;
       case WT_PROINFO_factoryreset_date:
           printk("%s write factory[%d] \n", __func__, type);
           ret = emmc_partition_rw("oem", 1, type * SIZE_4K, (char *) buf_tmp, (unsigned long) len);
           break;

        default:
            printk("%s write type[%d] \n", __func__, type);

            memcpy(temp_buf, buf_tmp, len);
            ret = emmc_partition_rw("oem", 1, type * SIZE_4K, temp_buf, SIZE_4K / 8);
            if (ret < 0) {
                printk("%s: write bytes to oem failed! %d\n", __func__, ret);
                return -1;
            }

            /*
            fp->f_pos = fp->f_pos + type * SIZE_4K;
            ret = kernel_write(fp, (char *) buf_tmp, (unsigned long) len, (loff_t) fp->f_pos);
            if (ret < 0) {
                printk("%s: write bytes to oem failed! %d\n", __func__, ret);
                filp_close(fp, NULL);
                return -1;
            }
            */

            break;
    }

    /*
    if(type != WT_PROINFO_factoryreset_date){
        filp_close(fp, NULL);
    }
    */

    printk("%s,write buf len:%d\n",__func__,len);

    return ret;
}


static struct platform_driver proinfo_pdrv = {
    .driver = {
        .name    = "proinfo",
        .owner    = THIS_MODULE,
    //    .pm    = &proinfo_ops,
    },
};

static int __init
proinfo_init(void)
{
    int rc;

    rc = platform_driver_register(&proinfo_pdrv);
    if (rc)
        return rc;

    proinfo_pdev = platform_device_register_simple("proinfo", -1, NULL,
                            0);
    if (IS_ERR(proinfo_pdev)) {
        rc = PTR_ERR(proinfo_pdev);
        goto out_pdrv;
    }



    proinfo_root_dev = root_device_register("proinfo");
    if (IS_ERR(proinfo_root_dev)) {
        rc = PTR_ERR(proinfo_root_dev);
        goto out_pdev;
    }
    rc = wt_create_device_files();
    if (rc)
        goto out_root;

    return 0;

out_root:
    root_device_unregister(proinfo_root_dev);
out_pdev:
    platform_device_unregister(proinfo_pdev);
out_pdrv:
    platform_driver_unregister(&proinfo_pdrv);
    return rc;
}

/*
 * The init/exit functions.
 */
static void __exit
proinfo_exit(void)
{
    platform_device_unregister(proinfo_pdev);
    platform_driver_unregister(&proinfo_pdrv);
    root_device_unregister(proinfo_root_dev);
}

module_init(proinfo_init);
module_exit(proinfo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wt_modem");

