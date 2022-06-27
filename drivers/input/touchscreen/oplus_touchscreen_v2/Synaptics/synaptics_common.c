// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "../touchpanel_common.h"
#include "synaptics_common.h"
#include <linux/crc32.h>
#include <linux/module.h>
#include <linux/version.h>
/*******Part0:LOG TAG Declear********************/
#ifdef TPD_DEVICE
#undef TPD_DEVICE
#define TPD_DEVICE "synaptics_common"
#else
#define TPD_DEVICE "synaptics_common"
#endif
/*******Part1:Call Back Function implement*******/

unsigned int extract_uint_le(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
	       (unsigned int)ptr[1] * 0x100 +
	       (unsigned int)ptr[2] * 0x10000 +
	       (unsigned int)ptr[3] * 0x1000000;
}
/*************************************auto test Funtion**************************************/
static int syna_auto_test_irq(struct touchpanel_data *ts,
			      struct syna_auto_test_operations *syna_test_ops,
			      struct auto_testdata *syna_testdata, bool false)
{
	int ret = 0;
	int eint_status, eint_count = 0, read_gpio_num = 0;

	if (syna_test_ops->syna_auto_test_enable_irq) {
		ret = syna_test_ops->syna_auto_test_enable_irq(ts->chip_data, false);

		if (ret) {
			TPD_INFO("%s: syna_auto_test_enable_irq failed !\n", __func__);
		}
	}

	eint_count = 0;
	read_gpio_num = 10;

	while (read_gpio_num--) {
		msleep(5);
		eint_status = gpio_get_value(syna_testdata->irq_gpio);

		if (eint_status == 1) {
			eint_count--;

		} else {
			eint_count++;
		}

		TPD_INFO("%s: eint_count = %d  eint_status = %d\n", __func__, eint_count,
			 eint_status);
	}

	return eint_count;
}

int synaptics_auto_test(struct seq_file *s,  struct touchpanel_data *ts)
{
	int ret = 0;
	int error_count = 0;

	/*for save result file buffer*/
	uint8_t  data_buf[64];

	/*for limit fw*/
	struct auto_test_header *test_head = NULL;
	/*for item limit data*/
	uint32_t *p_data32 = NULL;
	uint32_t item_cnt = 0;
	uint32_t i = 0;

	struct test_item_info *p_test_item_info = NULL;
	struct syna_auto_test_operations *syna_test_ops = NULL;
	struct com_test_data *com_test_data_p = NULL;

	struct auto_testdata syna_testdata = {
		.tx_num = 0,
		.rx_num = 0,
		.fp = NULL,
		.pos = NULL,
		.irq_gpio = -1,
		.key_tx = 0,
		.key_rx = 0,
		.tp_fw = 0,
		.fw = NULL,
		.test_item = 0,
	};
	TPD_INFO("%s  is called\n", __func__);

	com_test_data_p = &ts->com_test_data;

	if (!com_test_data_p->limit_fw || !ts || !com_test_data_p->chip_test_ops) {
		TPD_INFO("%s: data is null\n", __func__);
		return -1;
	}

	syna_test_ops = (struct syna_auto_test_operations *)
			com_test_data_p->chip_test_ops;

	/*decode the limit image*/
	test_head = (struct auto_test_header *)com_test_data_p->limit_fw->data;
	p_data32 = (uint32_t *)(com_test_data_p->limit_fw->data + 16);

	if ((test_head->magic1 != Limit_MagicNum1)
			|| (test_head->magic2 != Limit_MagicNum2)) {
		TPD_INFO("limit image is not generated by oplus\n");
		seq_printf(s, "limit image is not generated by oplus\n");
		return -1;
	}

	TPD_INFO("current test item: %llx\n", test_head->test_item);

	for (i = 0; i < 8 * sizeof(test_head->test_item); i++) {
		if ((test_head->test_item >> i) & 0x01) {
			item_cnt++;
		}
	}

	/*check limit support any item or not*/
	if (!item_cnt) {
		TPD_INFO("no any test item\n");
		error_count++;
		seq_printf(s, "no any test item\n");
		return -1;
	}

	/*init syna_testdata*/
	syna_testdata.fp        = ts->com_test_data.result_data;
	syna_testdata.length    = ts->com_test_data.result_max_len;
	syna_testdata.pos       = &ts->com_test_data.result_cur_len;
	syna_testdata.tx_num    = ts->hw_res.tx_num;
	syna_testdata.rx_num    = ts->hw_res.rx_num;
	syna_testdata.irq_gpio  = ts->hw_res.irq_gpio;
	syna_testdata.key_tx    = ts->hw_res.key_tx;
	syna_testdata.key_rx    = ts->hw_res.key_rx;
	syna_testdata.tp_fw     = ts->panel_data.tp_fw;
	syna_testdata.fw        = ts->com_test_data.limit_fw;
	syna_testdata.test_item = test_head->test_item;

	TPD_INFO("%s, step 0: begin to check INT-GND short item\n", __func__);
	ret = syna_auto_test_irq(ts, syna_test_ops, &syna_testdata, false);

	TPD_INFO("TP EINT PIN direct short! eint_count ret = %d\n", ret);

	if (ret == 10) {
		TPD_INFO("error :  TP EINT PIN direct short!\n");
		error_count++;
		seq_printf(s, "eint_status is low, TP EINT direct stort\n");
		sprintf(data_buf, "eint_status is low, TP EINT direct stort, \n");
		tp_test_write(syna_testdata.fp, syna_testdata.length, data_buf,
			      strlen(data_buf), syna_testdata.pos);
		ret = 0;
		goto END;
	}

	if (!syna_test_ops->syna_auto_test_preoperation) {
		TPD_INFO("not support syna_test_ops->syna_auto_test_preoperation callback\n");

	} else {
		syna_test_ops->syna_auto_test_preoperation(s, ts->chip_data, &syna_testdata,
				p_test_item_info);
	}

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST1);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST1);

	} else {
		ret = syna_test_ops->test1(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST2);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST2);

	} else {
		ret = syna_test_ops->test2(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST3);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST3);

	} else {
		ret = syna_test_ops->test3(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST4);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST4);

	} else {
		ret = syna_test_ops->test4(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST5);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST5);

	} else {
		ret = syna_test_ops->test5(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST6);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST6);

	} else {
		ret = syna_test_ops->test6(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST7);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST7);

	} else {
		ret = syna_test_ops->test7(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST8);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST8);

	} else {
		ret = syna_test_ops->test8(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST9);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST9);

	} else {
		ret = syna_test_ops->test9(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST10);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST10);

	} else {
		ret = syna_test_ops->test10(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(syna_testdata.fw, TYPE_TEST11);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST11);

	} else {
		ret = syna_test_ops->test11(s, ts->chip_data, &syna_testdata, p_test_item_info);

		if (ret > 0) {
			TPD_INFO("synaptics_capacity_test failed! ret is %d\n", ret);
			error_count++;
			goto END_TEST;
		}
	}

	tp_kfree((void **)&p_test_item_info);

END_TEST:

	if (!syna_test_ops->syna_auto_test_endoperation) {
		TPD_INFO("not support syna_test_ops->syna_auto_test_endoperation callback\n");

	} else {
		syna_test_ops->syna_auto_test_endoperation(s, ts->chip_data, &syna_testdata,
				p_test_item_info);
	}

END:

	seq_printf(s, "imageid = 0x%llx, deviceid = 0x%llx\n", syna_testdata.tp_fw,
		   syna_testdata.tp_fw);
	seq_printf(s, "%d error(s). %s\n", error_count,
		   error_count ? "" : "All test passed.");
	TPD_INFO(" TP auto test %d error(s). %s\n", error_count,
		 error_count ? "" : "All test passed.");
	TPD_INFO("\n\nstep5 reset and open irq complete\n");

	return error_count;
}
EXPORT_SYMBOL(synaptics_auto_test);
/*************************************auto test Funtion**************************************/

/*************************************TCM Firmware Parse Funtion**************************************/
int synaptics_parse_header_v2(struct image_info *image_info,
			      const unsigned char *fw_image)
{
	struct image_header_v2 *header;
	unsigned int magic_value;
	unsigned int number_of_areas;
	unsigned int i = 0;
	unsigned int addr;
	unsigned int length;
	unsigned int checksum;
	unsigned int flash_addr;
	const unsigned char *content;
	struct area_descriptor *descriptor;
	int offset = sizeof(struct image_header_v2);

	header = (struct image_header_v2 *)fw_image;
	magic_value = le4_to_uint(header->magic_value);

	if (magic_value != IMAGE_FILE_MAGIC_VALUE) {
		pr_err("invalid magic number %d\n", magic_value);
		return -EINVAL;
	}

	number_of_areas = le4_to_uint(header->num_of_areas);

	for (i = 0; i < number_of_areas; i++) {
		addr = le4_to_uint(fw_image + offset);
		descriptor = (struct area_descriptor *)(fw_image + addr);
		offset += 4;

		magic_value =  le4_to_uint(descriptor->magic_value);

		if (magic_value != FLASH_AREA_MAGIC_VALUE) {
			continue;
		}

		length = le4_to_uint(descriptor->length);
		content = (unsigned char *)descriptor + sizeof(*descriptor);
		flash_addr = le4_to_uint(descriptor->flash_addr_words) * 2;
		checksum = le4_to_uint(descriptor->checksum);

		if (0 == strncmp((char *)descriptor->id_string,
				 BOOT_CONFIG_ID,
				 strlen(BOOT_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				pr_err("Boot config checksum error\n");
				return -EINVAL;
			}

			image_info->boot_config.size = length;
			image_info->boot_config.data = content;
			image_info->boot_config.flash_addr = flash_addr;
			pr_info("Boot config size = %d, address = 0x%08x\n", length, flash_addr);

		} else if (0 == strncmp((char *)descriptor->id_string,
					APP_CODE_ID,
					strlen(APP_CODE_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				pr_err("Application firmware checksum error\n");
				return -EINVAL;
			}

			image_info->app_firmware.size = length;
			image_info->app_firmware.data = content;
			image_info->app_firmware.flash_addr = flash_addr;
			pr_info("Application firmware size = %d address = 0x%08x\n", length,
				flash_addr);

		} else if (0 == strncmp((char *)descriptor->id_string,
					APP_CONFIG_ID,
					strlen(APP_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				pr_err("Application config checksum error\n");
				return -EINVAL;
			}

			image_info->app_config.size = length;
			image_info->app_config.data = content;
			image_info->app_config.flash_addr = flash_addr;
			pr_info("Application config size = %d address = 0x%08x\n", length, flash_addr);

		} else if (0 == strncmp((char *)descriptor->id_string,
					DISP_CONFIG_ID,
					strlen(DISP_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				pr_err("Display config checksum error\n");
				return -EINVAL;
			}

			image_info->disp_config.size = length;
			image_info->disp_config.data = content;
			image_info->disp_config.flash_addr = flash_addr;
			pr_info("Display config size = %d address = 0x%08x\n", length, flash_addr);
		}
	}

	return 0;
}
EXPORT_SYMBOL(synaptics_parse_header_v2);
/**********************************RMI Firmware Parse Funtion*****************************************/
void synaptics_parse_header(struct image_header_data *header,
			    const unsigned char *fw_image)
{
	struct image_header *data = (struct image_header *)fw_image;

	header->checksum = extract_uint_le(data->checksum);
	TPD_DEBUG(" checksume is %x", header->checksum);

	header->bootloader_version = data->bootloader_version;
	TPD_DEBUG(" bootloader_version is %d\n", header->bootloader_version);

	header->firmware_size = extract_uint_le(data->firmware_size);
	TPD_DEBUG(" firmware_size is %x\n", header->firmware_size);

	header->config_size = extract_uint_le(data->config_size);
	TPD_DEBUG(" header->config_size is %x\n", header->config_size);

	/* only available in s4322 , reserved in other, begin*/
	header->bootloader_offset = extract_uint_le(data->bootloader_addr);
	header->bootloader_size = extract_uint_le(data->bootloader_size);
	TPD_DEBUG(" header->bootloader_offset is %x\n", header->bootloader_offset);
	TPD_DEBUG(" header->bootloader_size is %x\n", header->bootloader_size);

	header->disp_config_offset = extract_uint_le(data->dsp_cfg_addr);
	header->disp_config_size = extract_uint_le(data->dsp_cfg_size);
	TPD_DEBUG(" header->disp_config_offset is %x\n", header->disp_config_offset);
	TPD_DEBUG(" header->disp_config_size is %x\n", header->disp_config_size);
	/* only available in s4322 , reserved in other ,  end*/

	memcpy(header->product_id, data->product_id, sizeof(data->product_id));
	header->product_id[sizeof(data->product_id)] = 0;

	memcpy(header->product_info, data->product_info, sizeof(data->product_info));

	header->contains_firmware_id = data->options_firmware_id;
	TPD_DEBUG(" header->contains_firmware_id is %x\n",
		  header->contains_firmware_id);

	if (header->contains_firmware_id) {
		header->firmware_id = extract_uint_le(data->firmware_id);
	}

	return;
}

static int tp_RT251_read_func(struct seq_file *s, void *v)
{
	struct touchpanel_data *ts = s->private;
	struct debug_info_proc_operations *debug_info_ops;

	if (!ts) {
		return 0;
	}

	debug_info_ops = (struct debug_info_proc_operations *)ts->debug_info_ops;

	if (!debug_info_ops) {
		return 0;
	}

	if (!debug_info_ops->reserve1) {
		seq_printf(s, "Not support RT251 proc node\n");
		return 0;
	}

	disable_irq_nosync(ts->client->irq);
	mutex_lock(&ts->mutex);
	debug_info_ops->reserve1(s, ts->chip_data);
	mutex_unlock(&ts->mutex);
	enable_irq(ts->client->irq);

	return 0;
}

static int RT251_open(struct inode *inode, struct file *file)
{
	return single_open(file, tp_RT251_read_func, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
static const struct proc_ops tp_RT251_proc_fops = {
	.proc_open  = RT251_open,
	.proc_read  = seq_read,
	.proc_release = single_release,
};
#else
static const struct file_operations tp_RT251_proc_fops = {
	.owner = THIS_MODULE,
	.open  = RT251_open,
	.read  = seq_read,
	.release = single_release,
};
#endif

static int tp_RT76_read_func(struct seq_file *s, void *v)
{
	struct touchpanel_data *ts = s->private;
	struct debug_info_proc_operations *debug_info_ops;

	if (!ts) {
		return 0;
	}

	debug_info_ops = (struct debug_info_proc_operations *)ts->debug_info_ops;

	if (!debug_info_ops) {
		return 0;
	}

	if (!debug_info_ops->reserve2) {
		seq_printf(s, "Not support RT76 proc node\n");
		return 0;
	}

	disable_irq_nosync(ts->client->irq);
	mutex_lock(&ts->mutex);
	debug_info_ops->reserve2(s, ts->chip_data);
	mutex_unlock(&ts->mutex);
	enable_irq(ts->client->irq);

	return 0;
}

static int RT76_open(struct inode *inode, struct file *file)
{
	return single_open(file, tp_RT76_read_func, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
static const struct proc_ops tp_RT76_proc_fops = {
	.proc_open  = RT76_open,
	.proc_read  = seq_read,
	.proc_release = single_release,
};
#else
static const struct file_operations tp_RT76_proc_fops = {
	.owner = THIS_MODULE,
	.open  = RT76_open,
	.read  = seq_read,
	.release = single_release,
};
#endif

static int tp_DRT_read_func(struct seq_file *s, void *v)
{
	struct touchpanel_data *ts = s->private;
	struct debug_info_proc_operations *debug_info_ops;

	if (!ts) {
		return 0;
	}

	debug_info_ops = (struct debug_info_proc_operations *)ts->debug_info_ops;

	if (!debug_info_ops) {
		return 0;
	}

	if (!debug_info_ops->reserve4) {
		seq_printf(s, "Not support RT76 proc node\n");
		return 0;
	}

	if (ts->is_suspended && (ts->gesture_enable != 1)) {
		seq_printf(s, "In suspend state, and gesture not enable\n");
		return 0;
	}

	if (ts->int_mode == BANNABLE) {
		disable_irq_nosync(ts->irq);
	}

	mutex_lock(&ts->mutex);
	debug_info_ops->reserve4(s, ts->chip_data);
	mutex_unlock(&ts->mutex);

	if (ts->int_mode == BANNABLE) {
		enable_irq(ts->client->irq);
	}

	return 0;
}

static int DRT_open(struct inode *inode, struct file *file)
{
	return single_open(file, tp_DRT_read_func, PDE_DATA(inode));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
static const struct proc_ops tp_DRT_proc_fops = {
	.proc_open  = DRT_open,
	.proc_read  = seq_read,
	.proc_release = single_release,
};
#else
static const struct file_operations tp_DRT_proc_fops = {
	.owner = THIS_MODULE,
	.open  = DRT_open,
	.read  = seq_read,
	.release = single_release,
};
#endif

static ssize_t proc_touchfilter_control_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char page[PAGESIZE] = {0};
	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct synaptics_proc_operations *syn_ops;

	if (!ts) {
		return 0;
	}

	syn_ops = (struct synaptics_proc_operations *)ts->private_data;

	if (!syn_ops->get_touchfilter_state) {
		return 0;
	}

	snprintf(page, PAGESIZE - 1, "%d.\n",
		 syn_ops->get_touchfilter_state(ts->chip_data));
	ret = simple_read_from_buffer(user_buf, count, ppos, page, strlen(page));

	return ret;
}

static ssize_t proc_touchfilter_control_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[8] = {0};
	int temp = 0;
	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct synaptics_proc_operations *syn_ops;

	if (!ts) {
		return count;
	}

	syn_ops = (struct synaptics_proc_operations *)ts->private_data;

	if (!syn_ops->set_touchfilter_state) {
		return count;
	}

	if (count > 2) {
		return count;
	}

	if (copy_from_user(buf, buffer, count)) {
		TPD_DEBUG("%s: read proc input error.\n", __func__);
		return count;
	}

	sscanf(buf, "%d", &temp);
	mutex_lock(&ts->mutex);
	TPD_INFO("%s: value = %d\n", __func__, temp);
	syn_ops->set_touchfilter_state(ts->chip_data, temp);
	mutex_unlock(&ts->mutex);

	return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
static const struct proc_ops touch_filter_proc_fops = {
	.proc_read  = proc_touchfilter_control_read,
	.proc_write = proc_touchfilter_control_write,
	.proc_open  = simple_open,
};
#else
static const struct file_operations touch_filter_proc_fops = {
	.read  = proc_touchfilter_control_read,
	.write = proc_touchfilter_control_write,
	.open  = simple_open,
	.owner = THIS_MODULE,
};
#endif

int synaptics_create_proc(struct touchpanel_data *ts,
			  struct synaptics_proc_operations *syna_ops)
{
	int ret = 0;

	/* touchpanel_auto_test interface*/
	struct proc_dir_entry *prEntry_tmp = NULL;
	ts->private_data = syna_ops;

	/* show RT251 interface*/
	prEntry_tmp = proc_create_data("RT251", 0666, ts->prEntry_debug_tp,
				       &tp_RT251_proc_fops, ts);

	if (prEntry_tmp == NULL) {
		ret = -ENOMEM;
		TPD_INFO("%s: Couldn't create proc entry, %d\n", __func__, __LINE__);
	}

	/* show RT76 interface*/
	prEntry_tmp = proc_create_data("RT76", 0666, ts->prEntry_debug_tp,
				       &tp_RT76_proc_fops, ts);

	if (prEntry_tmp == NULL) {
		ret = -ENOMEM;
		TPD_INFO("%s: Couldn't create proc entry, %d\n", __func__, __LINE__);
	}

	prEntry_tmp = proc_create_data("DRT", 0666, ts->prEntry_debug_tp,
				       &tp_DRT_proc_fops, ts);

	if (prEntry_tmp == NULL) {
		ret = -ENOMEM;
		TPD_INFO("%s: Couldn't create proc entry, %d\n", __func__, __LINE__);
	}

	if (ts->face_detect_support) {
		prEntry_tmp = proc_create_data("touch_filter", 0666, ts->prEntry_tp,
					       &touch_filter_proc_fops, ts);

		if (prEntry_tmp == NULL) {
			ret = -ENOMEM;
			TPD_INFO("%s: Couldn't create proc entry, %d\n", __func__, __LINE__);
		}
	}

	return ret;
}
EXPORT_SYMBOL(synaptics_create_proc);

int synaptics_remove_proc(struct touchpanel_data *ts,
			  struct synaptics_proc_operations *syna_ops)
{
	if (!ts) {
		return -EINVAL;
	}

	remove_proc_entry("RT251", ts->prEntry_debug_tp);
	remove_proc_entry("RT76", ts->prEntry_debug_tp);
	remove_proc_entry("DRT", ts->prEntry_debug_tp);

	if (ts->face_detect_support) {
		remove_proc_entry("touch_filter", ts->prEntry_tp);
	}

	return 0;
}
EXPORT_SYMBOL(synaptics_remove_proc);

MODULE_DESCRIPTION("Touchscreen Synaptics Common Interface");
MODULE_LICENSE("GPL");
