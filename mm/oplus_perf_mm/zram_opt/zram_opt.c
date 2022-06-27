// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "zram_opt: " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <trace/hooks/vmscan.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>

static int g_direct_swappiness = 60;
static int g_swappiness = 160;

#define PARA_BUF_LEN 128
static int g_hybridswapd_swappiness = 200;
static struct proc_dir_entry *para_entry;

#ifdef CONFIG_HYBRIDSWAP_SWAPD
typedef bool (*free_swap_is_low_func)(void);
free_swap_is_low_func free_swap_is_low_fp = NULL;
EXPORT_SYMBOL(free_swap_is_low_fp);
#endif

static void zo_set_swappiness(void *data, int *swappiness)
{
	if (current_is_kswapd()) {
		*swappiness = g_swappiness;
#ifdef CONFIG_HYBRIDSWAP_SWAPD
	} else if (strncmp(current->comm, "hybridswapd:", sizeof("hybridswapd:") - 1) == 0) {
		*swappiness = g_hybridswapd_swappiness;
		if (free_swap_is_low_fp && free_swap_is_low_fp())
			*swappiness = 0;
#endif
	} else
		*swappiness = g_direct_swappiness;

	return;
}

static void zo_set_inactive_ratio(void *data, unsigned long *inactive_ratio, int file)
{
	if (file)
		*inactive_ratio = min(2UL, *inactive_ratio);
	else
		*inactive_ratio = 1;

	return;
}

static int register_zram_opt_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_tune_swappiness(zo_set_swappiness, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_set_swappiness failed! ret=%d\n", ret);
		goto out;
	}

	ret = register_trace_android_vh_tune_inactive_ratio(zo_set_inactive_ratio, NULL);
	if (ret != 0) {
		pr_err("register_trace_android_vh_tune_inactive_ratio failed! ret=%d\n", ret);
		goto out;
	}

out:
	return ret;
}

static void unregister_zram_opt_vendor_hooks(void)
{
	unregister_trace_android_vh_tune_swappiness(zo_set_swappiness, NULL);
	unregister_trace_android_vh_tune_inactive_ratio(zo_set_inactive_ratio, NULL);

	return;
}

static inline bool debug_get_val(char *buf, char *token, unsigned long *val)
{
	int ret = -EINVAL;
	char *str = strstr(buf, token);

	if (!str)
		return ret;

	ret = kstrtoul(str + strlen(token), 0, val);
	if (ret)
		return -EINVAL;

	if (*val > 200) {
		pr_err("%lu is invalid\n", *val);
		return -EINVAL;
	}

	return 0;
}

static ssize_t swappiness_para_write(struct file *file,
		const char __user *buff, size_t len, loff_t *ppos)
{
	char kbuf[PARA_BUF_LEN] = {'0'};
	char *str;
	long val;

	if (len > PARA_BUF_LEN - 1) {
		pr_err("len %d is too long\n", len);
		return -EINVAL;
	}

	if (copy_from_user(&kbuf, buff, len))
		return -EFAULT;
	kbuf[len] = '\0';

	str = strstrip(kbuf);
	if (!str) {
		pr_err("buff %s is invalid\n", kbuf);
		return -EINVAL;
	}

	if (!debug_get_val(str, "vm_swappiness=", &val)) {
		g_swappiness = val;
		return len;
	}

	if (!debug_get_val(str, "direct_swappiness=", &val)) {
		g_direct_swappiness = val;
		return len;
	}

	if (!debug_get_val(str, "swapd_swappiness=", &val)) {
		g_hybridswapd_swappiness = val;
		return len;
	}

	return -EINVAL;
}

static ssize_t swappiness_para_read(struct file *file,
		char __user *buffer, size_t count, loff_t *off)
{
	char kbuf[PARA_BUF_LEN] = {'0'};
	int len;

	len = snprintf(kbuf, PARA_BUF_LEN, "vm_swappiness: %d\n", g_swappiness);
	len += snprintf(kbuf + len, PARA_BUF_LEN - len,
			"direct_swappiness: %d\n", g_direct_swappiness);
	len += snprintf(kbuf + len, PARA_BUF_LEN - len,
			"swapd_swappiness: %d\n", g_hybridswapd_swappiness);

	if (len == PARA_BUF_LEN)
		kbuf[len - 1] = '\0';

	if (len > *off)
		len -= *off;
	else
		len = 0;

	if (copy_to_user(buffer, kbuf + *off, (len < count ? len : count)))
		return -EFAULT;

	*off += (len < count ? len : count);
	return (len < count ? len : count);
}

static const struct proc_ops proc_swappiness_para_ops = {
	.proc_write          = swappiness_para_write,
	.proc_read		= swappiness_para_read,
};

static int __init create_swappiness_para_proc(void)
{
	struct proc_dir_entry *root_dir_entry = proc_mkdir("oplus_mem", NULL);

	para_entry = proc_create((root_dir_entry ?
				"swappiness_para" : "oplus_mem/swappiness_para"),
			S_IRUSR|S_IWUSR, NULL, &proc_swappiness_para_ops);

	if (para_entry) {
		printk("Register swappiness_para interface passed.\n");
		return 0;
	}

	pr_err("Register swappiness_para interface failed.\n");
	return -ENOMEM;
}

static void __exit destroy_swappiness_para_proc(void)
{
	proc_remove(para_entry);
	para_entry = NULL;
}

static int __init zram_opt_init(void)
{
	int ret = 0;

	ret = create_swappiness_para_proc();
	if (ret)
		return ret;

	ret = register_zram_opt_vendor_hooks();
	if (ret != 0) {
		destroy_swappiness_para_proc();
		return ret;
	}

	pr_info("zram_opt_init succeed!\n");
	return 0;
}

static void __exit zram_opt_exit(void)
{
	unregister_zram_opt_vendor_hooks();
	destroy_swappiness_para_proc();

	pr_info("zram_opt_exit succeed!\n");

	return;
}

module_init(zram_opt_init);
module_exit(zram_opt_exit);

module_param_named(vm_swappiness, g_swappiness, int, S_IRUGO | S_IWUSR);
module_param_named(direct_vm_swappiness, g_direct_swappiness, int, S_IRUGO | S_IWUSR);
module_param_named(hybridswapd_swappiness, g_hybridswapd_swappiness, int, S_IRUGO | S_IWUSR);
MODULE_LICENSE("GPL v2");
