/*
 * param.c
 *
 * Parameter read & save driver on param partition.
 *
 * COPYRIGHT(C) Samsung Electronics Co.Ltd. 2006-2010 All Right Reserved.
 *
 * Author: Jeonghwan Min <jeonghwan.min@samsung.com>
 *
 * 20080226. Supprot on BML layer.
 *
 */
/*
 * On every write, recalculate checksum and update it
 * Check checksum to decide if a param block is broken
 * On write:
 * if (MB is broken)
 *   write new data to MB
 * else {
 *   copy MB to BB
 *   write new data to MB
 * }
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/blkpg.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>

#include <mach/hardware.h>
#include <mach/param.h>

#include <XsrTypes.h>
#include <BML.h>

#define SECTOR_BITS		9

#define SPP(volume)		(vs[volume].nSctsPerPg)
#define SPB(volume)		(vs[volume].nSctsPerPg * vs[volume].nPgsPerBlk)

extern BMLVolSpec		*xsr_get_vol_spec(UINT32 volume);
extern XSRPartI			*xsr_get_part_spec(UINT32 volume);

#define PARAM_LEN		(32 * 2048)

#define BLOCK_OFFSET_MAIN	1
#define BLOCK_OFFSET_BACKUP	2

int is_valid_param(status_t *status)
{
	return (status->param_magic == PARAM_MAGIC) &&
		(status->param_version == PARAM_VERSION);
}

int write_param_block(unsigned int dev_id, unsigned char *addr, unsigned block_offset)
{
	unsigned int i, err;
	unsigned int first;
	unsigned int nBytesReturned = 0;
	unsigned int nPart[2] = {PARAM_PART_ID, BML_PI_ATTR_RW};
	unsigned char *pbuf, *buf;
	BMLVolSpec *vs;
	XSRPartI *pi;

	if (addr == NULL) {
		printk(KERN_ERR "%s: wrong address\n", __FUNCTION__);
		return -EINVAL;
	}

	buf = vmalloc(PARAM_LEN);
	if (!buf)
		return -ENOMEM;

	/* get vol info */
	vs = xsr_get_vol_spec(0);

	/* get part info */
	pi = xsr_get_part_spec(0);

	for (i = 0 ; i < pi->nNumOfPartEntry ; i++) {
		if (pi->stPEntry[i].nID == PARAM_PART_ID) {
			break;
		}
	}

	/* start sector */
	first = (pi->stPEntry[i].n1stVbn + block_offset) * SPB(0);

	err = BML_IOCtl(0, BML_IOCTL_CHANGE_PART_ATTR,
			(unsigned char *)&nPart, sizeof(unsigned int) * 2, NULL, 0, &nBytesReturned);
	if (err!= BML_SUCCESS) {
		printk(KERN_ERR "%s: ioctl error\n", __FUNCTION__);
		return -EIO;
	}

	pbuf = buf;

	for (i = 0 ; i < (SPB(0)/2) ; i += SPP(0))	{
		err = BML_Read(0, first + i, SPP(0), pbuf, NULL, BML_FLAG_SYNC_OP | BML_FLAG_ECC_ON);
		if (err != BML_SUCCESS)	{
			printk(KERN_ERR "%s: read page error\n", __FUNCTION__);
			return -EIO;
		}
		pbuf += (SPP(0) << SECTOR_BITS);
	}

	/* erase block before write */
	err = BML_EraseBlk(0, first/SPB(0), BML_FLAG_SYNC_OP);
	if(err != BML_SUCCESS){
		printk(KERN_ERR "%s: erase block error\n", __FUNCTION__);
		err = -EIO;
		goto fail2;
	}

	pbuf = buf;

	for (i = 0 ; i < (SPB(0)/2) ; i += SPP(0))	{
		err = BML_Write(0, first + i, SPP(0), pbuf, NULL, BML_FLAG_SYNC_OP | BML_FLAG_ECC_ON);
		if (err != BML_SUCCESS)	{
			printk(KERN_ERR "%s: write page error\n", __FUNCTION__);
			err = -EIO;
			goto fail2;
		}
		pbuf += (SPP(0) << SECTOR_BITS);
	}

	for (i = (SPB(0)/2) ; i < SPB(0) ; i += SPP(0))	{
		err = BML_Write(0, first + i, SPP(0), addr, NULL, BML_FLAG_SYNC_OP | BML_FLAG_ECC_ON);
		if (err != BML_SUCCESS)	{
			printk(KERN_ERR "%s: write page error\n", __FUNCTION__);
			err = -EIO;
			goto fail2;
		}
		addr += (SPP(0) << SECTOR_BITS);
	}

fail2:
	nPart[1] = BML_PI_ATTR_RO;
	err = BML_IOCtl(0, BML_IOCTL_CHANGE_PART_ATTR,
			(unsigned char *)&nPart, sizeof(unsigned int) * 2, NULL, 0, &nBytesReturned);
	if (err != BML_SUCCESS) {
		printk(KERN_ERR "%s: ioctl error\n", __FUNCTION__);
		err = -EIO;
		goto fail1;
	}

	vfree(buf);
	return 0;

fail1:
	vfree(buf);
	return err;
}

int read_param_block(unsigned int dev_id, unsigned char *addr, unsigned block_offset)
{
	unsigned int i, err;
	unsigned int first;
	BMLVolSpec *vs;
	XSRPartI *pi;

	if (addr == NULL){
		printk(KERN_ERR "%s: wrong address\n", __FUNCTION__);
		return -EINVAL;
	}

	/* Get vol info */
	vs = xsr_get_vol_spec(0);

	/* get part info */
	pi = xsr_get_part_spec(0);

	for (i = 0 ; i < pi->nNumOfPartEntry ; i++) {
		if (pi->stPEntry[i].nID == PARAM_PART_ID) {
			break;
		}
	}

	/* Start sector */
	first = (pi->stPEntry[i].n1stVbn + block_offset) * SPB(0);

	for (i = (SPB(0)/2) ; i < SPB(0) ; i += SPP(0))	{
		err = BML_Read(0, first + i, SPP(0), addr, NULL, BML_FLAG_SYNC_OP | BML_FLAG_ECC_ON);
		if (err != BML_SUCCESS)	{
			printk(KERN_ERR "%s: read page error\n", __FUNCTION__);
			return -EIO;
		}
		addr += (SPP(0) << SECTOR_BITS);
	}

	return 0;
}

static status_t param_status;

static int load_param_value(void)
{
	unsigned char *addr = NULL;
	unsigned int err = 0, dev_id = 0;

	addr = vmalloc(PARAM_LEN);
	if (!addr)
		return -ENOMEM;

	err = BML_Open(dev_id);
	if (err) {
		printk(KERN_ERR "%s: open error\n", __FUNCTION__);
		goto fail;
	}

	err = read_param_block(dev_id, addr, BLOCK_OFFSET_MAIN);
	if (err) {
		printk(KERN_ERR "%s: read param error\n", __FUNCTION__);
		goto fail;
	}

	if (is_valid_param((status_t *)addr)) {
		memcpy(&param_status, addr, sizeof(status_t));
	}
	else {

		printk(KERN_ERR "%s: no param info in first param block\n", __FUNCTION__);

		err = read_param_block(dev_id, addr, BLOCK_OFFSET_BACKUP);
		if (err) {
			printk(KERN_ERR "%s: read backup param error\n", __FUNCTION__);
			goto fail;
		}

		if (is_valid_param((status_t *)addr)) {
			memcpy(&param_status, addr, sizeof(status_t));
		}
		else {
			printk(KERN_ERR "%s: no param info in backup param block\n", __FUNCTION__);
			err = -1;
		}
	}

fail:
	vfree(addr);
	BML_Close(dev_id);

	return err;
}

int save_param_value(void)
{
	unsigned int err = 0, dev_id = 0;
	unsigned char *addr = NULL;

	addr = vmalloc(PARAM_LEN);
	if (!addr)
		return -ENOMEM;

	err = BML_Open(dev_id);
	if (err) {
		printk(KERN_ERR "%s: open error\n", __FUNCTION__);
		goto fail;
	}

	// if MAIN is not broken, copy MAIN to BACKUP
	err = read_param_block(dev_id, addr, BLOCK_OFFSET_MAIN);
	if (err) {
		printk(KERN_ERR "%s: read param error\n", __FUNCTION__);
		goto fail;
	}

	if (is_valid_param((status_t *)addr)) {
		err = write_param_block(dev_id, addr, BLOCK_OFFSET_BACKUP);
		if (err) {
			printk(KERN_ERR "%s: write backup param error\n", __FUNCTION__);
			goto fail;
		}
	} else
		printk(KERN_ERR "%s: main block is invalid, not backing up.\n", __FUNCTION__);


	// update MAIN
	memset(addr, 0, PARAM_LEN);
	memcpy(addr, &param_status, sizeof(status_t));

	err = write_param_block(dev_id, addr, BLOCK_OFFSET_MAIN);
	if (err) {
		printk(KERN_ERR "%s: write param error\n", __FUNCTION__);
		goto fail;
	}

fail:
	vfree(addr);
	BML_Close(dev_id);

	return err;
}
EXPORT_SYMBOL(save_param_value);

void set_param_value(int idx, void *value)
{
	int i, str_i;

	for (i = 0; i < MAX_PARAM; i++) {
		if (i < (MAX_PARAM - MAX_STRING_PARAM)) {
			if(param_status.param_list[i].ident == idx) {
				param_status.param_list[i].value = *(int *)value;
			}
		}
		else {
			str_i = (i - (MAX_PARAM - MAX_STRING_PARAM));
			if(param_status.param_str_list[str_i].ident == idx) {
				strlcpy(param_status.param_str_list[str_i].value,
					(char *)value, PARAM_STRING_SIZE);
			}
		}
	}

	save_param_value();
}
EXPORT_SYMBOL(set_param_value);

void get_param_value(int idx, void *value)
{
	int i, str_i;

	for (i = 0 ; i < MAX_PARAM; i++) {
		if (i < (MAX_PARAM - MAX_STRING_PARAM)) {
			if(param_status.param_list[i].ident == idx) {
				*(int *)value = param_status.param_list[i].value;
			}
		}
		else {
			str_i = (i - (MAX_PARAM - MAX_STRING_PARAM));
			if(param_status.param_str_list[str_i].ident == idx) {
				strlcpy((char *)value,
					param_status.param_str_list[str_i].value, PARAM_STRING_SIZE);
			}
		}
	}
}
EXPORT_SYMBOL(get_param_value);

static int param_init(void)
{
	int ret;

	ret = load_param_value();
	if (ret < 0) {
		printk(KERN_ERR "%s -> relocated to default value!\n", __FUNCTION__);

		memset(&param_status, 0, sizeof(status_t));

		param_status.param_magic = PARAM_MAGIC;
		param_status.param_version = PARAM_VERSION;
		param_status.param_list[0].ident = __SERIAL_SPEED;
		param_status.param_list[0].value = SERIAL_SPEED;
		param_status.param_list[1].ident = __LOAD_RAMDISK;
		param_status.param_list[1].value = LOAD_RAMDISK;
		param_status.param_list[2].ident = __BOOT_DELAY;
		param_status.param_list[2].value = BOOT_DELAY;
		param_status.param_list[3].ident = __LCD_LEVEL;
		param_status.param_list[3].value = LCD_LEVEL;
		param_status.param_list[4].ident = __SWITCH_SEL;
		param_status.param_list[4].value = SWITCH_SEL;
		param_status.param_list[5].ident = __PHONE_DEBUG_ON;
		param_status.param_list[5].value = PHONE_DEBUG_ON;
		param_status.param_list[6].ident = __LCD_DIM_LEVEL;
		param_status.param_list[6].value = LCD_DIM_LEVEL;
		param_status.param_list[7].ident = __MELODY_MODE;
		param_status.param_list[7].value = MELODY_MODE;
		param_status.param_list[8].ident = __REBOOT_MODE;
		param_status.param_list[8].value = REBOOT_MODE;
		param_status.param_list[9].ident = __NATION_SEL;
		param_status.param_list[9].value = NATION_SEL;
		param_status.param_list[10].ident = __SET_DEFAULT_PARAM;
		param_status.param_list[10].value = SET_DEFAULT_PARAM;
#ifdef CONFIG_MACH_CYGNUS
		param_status.param_list[11].ident = __TSP_FACTORY_CAL_DONE;
		param_status.param_list[11].value = SET_TSP_FACTORY_CAL;
#endif
#ifdef CONFIG_MACH_SATURN
		param_status.param_list[12].ident = __AUTO_RAMDUMP_MODE;
		param_status.param_list[12].value = AUTO_RAMDUMP_MODE;
#endif
		param_status.param_str_list[0].ident = __VERSION;
		strlcpy(param_status.param_str_list[0].value,
			VERSION_LINE, PARAM_STRING_SIZE);
		param_status.param_str_list[1].ident = __CMDLINE;
		strlcpy(param_status.param_str_list[1].value,
			COMMAND_LINE, PARAM_STRING_SIZE);

#ifdef CONFIG_MACH_CYGNUS
		// For Store Cygnus TSP Factory Cal Data
		param_status.param_str_list[2].ident = __TSP_FACTORY_CAL;
		strlcpy(param_status.param_str_list[2].value,
			FACTORY_TSP_CAL, PARAM_STRING_SIZE);
#endif
	}

	sec_set_param_value = set_param_value;
	sec_get_param_value = get_param_value;

	return 0;
}

static void param_exit(void)
{
}

module_init(param_init);
module_exit(param_exit);
