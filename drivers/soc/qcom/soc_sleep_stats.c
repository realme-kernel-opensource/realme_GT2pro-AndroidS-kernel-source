// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/string.h>

#include <linux/soc/qcom/smem.h>
#include <soc/qcom/soc_sleep_stats.h>
#include <clocksource/arm_arch_timer.h>

#define STAT_TYPE_ADDR		0x0
#define COUNT_ADDR		0x4
#define LAST_ENTERED_AT_ADDR	0x8
#define LAST_EXITED_AT_ADDR	0x10
#define ACCUMULATED_ADDR	0x18
#define CLIENT_VOTES_ADDR	0x1c

#define DDR_STATS_MAGIC_KEY	0xA1157A75
#define DDR_STATS_MAX_NUM_MODES	0x14
#define MAX_DRV			18
#define MAX_MSG_LEN		35
#define DRV_ABSENT		0xdeaddead
#define DRV_INVALID		0xffffdead
#define VOTE_MASK		0x3fff
#define VOTE_X_SHIFT		14

#define DDR_STATS_MAGIC_KEY_ADDR	0x0
#define DDR_STATS_NUM_MODES_ADDR	0x4
#define DDR_STATS_NAME_ADDR		0x0
#define DDR_STATS_COUNT_ADDR		0x4
#define DDR_STATS_DURATION_ADDR		0x8

#if IS_ENABLED(CONFIG_QCOM_SMEM)
struct subsystem_data {
	const char *name;
	u32 smem_item;
	u32 pid;
};

static struct subsystem_data subsystems[] = {
	{ "modem", 605, 1 },
	{ "wpss", 605, 13 },
	{ "adsp", 606, 2 },
	{ "adsp_island", 613, 2 },
	{ "cdsp", 607, 5 },
	{ "slpi", 608, 3 },
	{ "slpi_island", 613, 3 },
	{ "gpu", 609, 0 },
	{ "display", 610, 0 },
	{ "apss", 631, QCOM_SMEM_HOST_ANY },
};
#endif

struct stats_config {
	unsigned int offset_addr;
	unsigned int ddr_offset_addr;
	unsigned int num_records;
	bool appended_stats_avail;
};

struct stats_entry {
	uint32_t name;
	uint32_t count;
	uint64_t duration;
};

struct stats_prv_data {
	const struct stats_config *config;
	void __iomem *reg;
};

struct ddr_stats_g_data {
	bool read_vote_info;
	void __iomem *ddr_reg;
	u32 entry_count;
	struct mutex ddr_stats_lock;
	struct mbox_chan *stats_mbox_ch;
	struct mbox_client stats_mbox_cl;
};

struct sleep_stats {
	u32 stat_type;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
};

struct appended_stats {
	u32 client_votes;
	u32 reserved[3];
};

#ifdef CONFIG_OPLUS_POWERINFO_RPMH
struct soc_sleep_stats_data {
	struct device_node *node;
	const struct stats_config *config;
	struct kobject *stat_kobj;
	struct kobject *master_kobj;
	struct kobj_attribute ka_stat_oplus;
	struct kobj_attribute ka_master_oplus;
	void __iomem *reg;
};
static struct soc_sleep_stats_data *drv_backup;
#endif

struct ddr_stats_g_data *ddr_gdata;

static void print_sleep_stats(struct seq_file *s, struct sleep_stats *stat)
{
	u64 accumulated = stat->accumulated;
	/*
	 * If a subsystem is in sleep when reading the sleep stats adjust
	 * the accumulated sleep duration to show actual sleep time.
	 */
	if (stat->last_entered_at > stat->last_exited_at)
		accumulated += arch_timer_read_counter()
			       - stat->last_entered_at;

	seq_printf(s, "Count = %u\n", stat->count);
	seq_printf(s, "Last Entered At = %llu\n", stat->last_entered_at);
	seq_printf(s, "Last Exited At = %llu\n", stat->last_exited_at);
	seq_printf(s, "Accumulated Duration = %llu\n", accumulated);
}

static int subsystem_sleep_stats_show(struct seq_file *s, void *d)
{
#if IS_ENABLED(CONFIG_QCOM_SMEM)
	struct subsystem_data *subsystem = s->private;
	struct sleep_stats *stat;

	stat = qcom_smem_get(subsystem->pid, subsystem->smem_item, NULL);
	if (IS_ERR(stat))
		return PTR_ERR(stat);

	print_sleep_stats(s, stat);

#endif
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(subsystem_sleep_stats);

static int soc_sleep_stats_show(struct seq_file *s, void *d)
{
	struct stats_prv_data *prv_data = s->private;
	void __iomem *reg = prv_data->reg;
	struct sleep_stats stat;

	stat.count = readl_relaxed(reg + COUNT_ADDR);
	stat.last_entered_at = readq(reg + LAST_ENTERED_AT_ADDR);
	stat.last_exited_at = readq(reg + LAST_EXITED_AT_ADDR);
	stat.accumulated = readq(reg + ACCUMULATED_ADDR);

	print_sleep_stats(s, &stat);

	if (prv_data->config->appended_stats_avail) {
		struct appended_stats app_stat;

		app_stat.client_votes = readl_relaxed(reg + CLIENT_VOTES_ADDR);
		seq_printf(s, "Client_votes = %#x\n", app_stat.client_votes);
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(soc_sleep_stats);

static void  print_ddr_stats(struct seq_file *s, int *count,
			     struct stats_entry *data, u64 accumulated_duration)
{

	u32 cp_idx = 0;
	u32 name, duration = 0;

	if (accumulated_duration)
		duration = (data->duration * 100) / accumulated_duration;

	name = (data->name >> 8) & 0xFF;
	if (name == 0x0) {
		name = (data->name) & 0xFF;
		*count = *count + 1;
		seq_printf(s,
		"LPM %d:\tName:0x%x\tcount:%u\tDuration (ticks):%ld (~%d%%)\n",
			*count, name, data->count, data->duration, duration);
	} else if (name == 0x1) {
		cp_idx = data->name & 0x1F;
		name = data->name >> 16;

		if (!name || !data->count)
			return;

		seq_printf(s,
		"Freq %dMhz:\tCP IDX:%u\tcount:%u\tDuration (ticks):%ld (~%d%%)\n",
			name, cp_idx, data->count, data->duration, duration);
	}
}

static int ddr_stats_show(struct seq_file *s, void *d)
{
	struct stats_entry data[DDR_STATS_MAX_NUM_MODES];
	void __iomem *reg = s->private;
	u32 entry_count;
	u64 accumulated_duration = 0;
	int i, lpm_count = 0;

	entry_count = readl_relaxed(reg + DDR_STATS_NUM_MODES_ADDR);
	if (entry_count > DDR_STATS_MAX_NUM_MODES) {
		pr_err("Invalid entry count\n");
		return 0;
	}

	reg += DDR_STATS_NUM_MODES_ADDR + 0x4;

	for (i = 0; i < entry_count; i++) {
		data[i].count = readl_relaxed(reg + DDR_STATS_COUNT_ADDR);

		data[i].name = readl_relaxed(reg + DDR_STATS_NAME_ADDR);

		data[i].duration = readq_relaxed(reg + DDR_STATS_DURATION_ADDR);

		accumulated_duration += data[i].duration;
		reg += sizeof(struct stats_entry);
	}

	for (i = 0; i < entry_count; i++)
		print_ddr_stats(s, &lpm_count, &data[i], accumulated_duration);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(ddr_stats);

int ddr_stats_get_ss_count(void)
{
	return ddr_gdata->read_vote_info ? MAX_DRV : -EOPNOTSUPP;
}
EXPORT_SYMBOL(ddr_stats_get_ss_count);

int ddr_stats_get_ss_vote_info(int ss_count,
				struct ddr_stats_ss_vote_info *vote_info)
{
	char buf[MAX_MSG_LEN] = {};
	struct qmp_pkt pkt;
	void __iomem *reg;
	u32 vote_offset, val[MAX_DRV];
	int ret, i;

	if (!vote_info || !(ss_count == MAX_DRV) || !ddr_gdata)
		return -ENODEV;

	if (!ddr_gdata->read_vote_info)
		return -EOPNOTSUPP;

	mutex_lock(&ddr_gdata->ddr_stats_lock);
	ret = scnprintf(buf, MAX_MSG_LEN, "{class: ddr, res: drvs_ddr_votes}");
	pkt.size = (ret + 0x3) & ~0x3;
	pkt.data = buf;

	ret = mbox_send_message(ddr_gdata->stats_mbox_ch, &pkt);
	if (ret < 0) {
		pr_err("Error sending mbox message: %d\n", ret);
		mutex_unlock(&ddr_gdata->ddr_stats_lock);
		return ret;
	}

	vote_offset = sizeof(u32) + sizeof(u32) +
			(ddr_gdata->entry_count * sizeof(struct stats_entry));
	reg = ddr_gdata->ddr_reg;

	for (i = 0; i < ss_count; i++, reg += sizeof(u32)) {
		val[i] = readl_relaxed(reg + vote_offset);
		if (val[i] == DRV_ABSENT) {
			vote_info[i].ab = DRV_ABSENT;
			vote_info[i].ib = DRV_ABSENT;
			continue;
		} else if (val[i] == DRV_INVALID) {
			vote_info[i].ab = DRV_INVALID;
			vote_info[i].ib = DRV_INVALID;
			continue;
		}

		vote_info[i].ab = (val[i] >> VOTE_X_SHIFT) & VOTE_MASK;
		vote_info[i].ib = val[i] & VOTE_MASK;
	}

	mutex_unlock(&ddr_gdata->ddr_stats_lock);
	return 0;

}
EXPORT_SYMBOL(ddr_stats_get_ss_vote_info);

static struct dentry *create_debugfs_entries(void __iomem *reg,
					     void __iomem *ddr_reg,
					     struct stats_prv_data *prv_data,
					     struct device_node *node)
{
	struct dentry *root;
	char stat_type[sizeof(u32) + 1] = {0};
	u32 offset, type, key;
	int i, j, n_subsystems;
	const char *name;

	root = debugfs_create_dir("qcom_sleep_stats", NULL);

	for (i = 0; i < prv_data[0].config->num_records; i++) {
		offset = STAT_TYPE_ADDR + (i * sizeof(struct sleep_stats));

		if (prv_data[0].config->appended_stats_avail)
			offset += i * sizeof(struct appended_stats);

		prv_data[i].reg = reg + offset;

		type = readl_relaxed(prv_data[i].reg);
		memcpy(stat_type, &type, sizeof(u32));
		strim(stat_type);

		debugfs_create_file(stat_type, 0444, root,
				    &prv_data[i],
				    &soc_sleep_stats_fops);
	}

	n_subsystems = of_property_count_strings(node, "ss-name");
	if (n_subsystems < 0)
		goto exit;

	for (i = 0; i < n_subsystems; i++) {
		of_property_read_string_index(node, "ss-name", i, &name);

		for (j = 0; j < ARRAY_SIZE(subsystems); j++) {
			if (!strcmp(subsystems[j].name, name)) {
				debugfs_create_file(subsystems[j].name, 0444,
						    root, &subsystems[j],
						    &subsystem_sleep_stats_fops);
				break;
			}
		}

	}

	if (!ddr_reg)
		goto exit;

	key = readl_relaxed(ddr_reg + DDR_STATS_MAGIC_KEY_ADDR);
	if (key == DDR_STATS_MAGIC_KEY)
		debugfs_create_file("ddr_stats", 0444,
				     root, ddr_reg, &ddr_stats_fops);

exit:
	return root;
}

#ifdef CONFIG_OPLUS_POWERINFO_RPMH
#define MSM_ARCH_TIMER_FREQ 19200000
static inline u64 get_time_in_msec(u64 counter)
{
	do_div(counter, (MSM_ARCH_TIMER_FREQ/MSEC_PER_SEC));

	return counter;
}

static inline ssize_t oplus_append_data_to_buf(int index, char *buf, int length,
					 struct sleep_stats *stat)
{
	if (index == 0) {
		//vddlow: aosd: AOSS deep sleep
		return scnprintf(buf, length,
			"vlow:%x:%llx\n",
			stat->count, stat->accumulated);
	} else if (index == 1) {
	  //vddmin: cxsd: cx collapse
		return scnprintf(buf, length,
			"vmin:%x:%llx\r\n",
			stat->count, stat->accumulated);
	} else {
		return 0;
	}
}

static ssize_t oplus_rpmh_stats_show(struct kobject *obj, struct kobj_attribute *attr,
			  char *buf)
{
	int i;
	ssize_t length = 0, op_length;
	struct soc_sleep_stats_data *drv = container_of(attr,
					   struct soc_sleep_stats_data, ka_stat_oplus);
	void __iomem *reg = drv->reg;
	struct sleep_stats stat;
	struct appended_stats app_stat;

	for (i = 0; i < drv->config->num_records; i++) {
		stat.stat_type = le32_to_cpu(readl_relaxed(reg + STAT_TYPE_ADDR));
		stat.count = le32_to_cpu(readl_relaxed(reg + COUNT_ADDR));
		stat.last_entered_at = le64_to_cpu(readq(reg + LAST_ENTERED_AT_ADDR));
		stat.last_exited_at = le64_to_cpu(readq(reg + LAST_EXITED_AT_ADDR));
		stat.accumulated = le64_to_cpu(readq(reg + ACCUMULATED_ADDR));

		stat.last_entered_at = get_time_in_msec(stat.last_entered_at);
		stat.last_exited_at = get_time_in_msec(stat.last_exited_at);
		stat.accumulated = get_time_in_msec(stat.accumulated);

		reg += sizeof(struct sleep_stats);

		if (drv->config->appended_stats_avail) {
			app_stat.client_votes = le32_to_cpu(readl_relaxed(reg +
								     CLIENT_VOTES_ADDR));

			reg += sizeof(struct appended_stats);
		} else {
			app_stat.client_votes = 0;
		}

		op_length = oplus_append_data_to_buf(i, buf + length, PAGE_SIZE - length,
					       &stat);
		if (op_length >= PAGE_SIZE - length)
			goto exit;

		length += op_length;
	}
exit:
	return length;
}

static ssize_t oplus_msm_rpmh_master_stats_print_data(char *prvbuf, ssize_t length,
				struct sleep_stats *stat,
				const char *name)
{
	uint64_t accumulated_duration = stat->accumulated;
	/*
	 * If a master is in sleep when reading the sleep stats from SMEM
	 * adjust the accumulated sleep duration to show actual sleep time.
	 * This ensures that the displayed stats are real when used for
	 * the purpose of computing battery utilization.
	 */
	if (stat->last_entered_at > stat->last_exited_at)
		accumulated_duration +=
				(__arch_counter_get_cntvct()
				- stat->last_entered_at);

	return scnprintf(prvbuf, length, "%s:%x:%llx\n",
			name, stat->count,
			get_time_in_msec(accumulated_duration));
}

static ssize_t oplus_rpmh_master_stats_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	ssize_t length = 0;
	int i = 0, j, n_subsystems;
	struct device_node *node;
	struct sleep_stats *stat;
	const char *name;
	struct soc_sleep_stats_data *drv = container_of(attr,
					   struct soc_sleep_stats_data, ka_master_oplus);

	node = drv->node;
	n_subsystems = of_property_count_strings(node, "ss-name");
	if (n_subsystems < 0)
		goto exit;

	for (i = 0; i < n_subsystems; i++) {
		of_property_read_string_index(node, "ss-name", i, &name);


		for (j = 0; j < ARRAY_SIZE(subsystems); j++) {
			if (!strcmp(subsystems[j].name, name)) {
				stat = qcom_smem_get(subsystems[j].pid,
						subsystems[j].smem_item, NULL);
				if (IS_ERR(stat))
					return PTR_ERR(stat);

				length += oplus_msm_rpmh_master_stats_print_data(
						buf + length, PAGE_SIZE - length,
						stat,
						subsystems[j].name);
				break;
			}
		}
	}

exit:
	return length;
}

static struct kobject *get_module_kobj(struct device *dev)
{
	if (!dev)
		return NULL;
	return &dev->driver->owner->mkobj.kobj;
}

static struct kobject *oplus_power_kobj;
static int soc_sleep_stats_create_sysfs(struct platform_device *pdev,
					struct soc_sleep_stats_data *drv)
{
	int ret = 0;

	oplus_power_kobj = get_module_kobj(&pdev->dev);
	if (!oplus_power_kobj)
		return -EINVAL;

	drv->stat_kobj = kobject_create_and_add("soc_sleep", oplus_power_kobj);
	if (!drv->stat_kobj)
		return -ENOMEM;

	sysfs_attr_init(&drv->ka_stat_oplus.attr);
	drv->ka_stat_oplus.attr.mode = 0444;
	drv->ka_stat_oplus.attr.name = "oplus_rpmh_stats";
	drv->ka_stat_oplus.show = oplus_rpmh_stats_show;

	ret = sysfs_create_file(drv->stat_kobj, &drv->ka_stat_oplus.attr);

	drv->master_kobj = kobject_create_and_add("rpmh_stats", oplus_power_kobj);
	if (!drv->master_kobj)
		return -ENOMEM;

	sysfs_attr_init(&drv->ka_master_oplus.attr);
	drv->ka_master_oplus.attr.mode = 0444;
	drv->ka_master_oplus.attr.name = "oplus_rpmh_master_stats";
	drv->ka_master_oplus.show = oplus_rpmh_master_stats_show;

	ret |= sysfs_create_file(drv->master_kobj, &drv->ka_master_oplus.attr);

	return ret;
}
#endif

static int soc_sleep_stats_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *reg;
	void __iomem *offset_addr;
	phys_addr_t stats_base;
	resource_size_t stats_size;
	struct dentry *root;
	const struct stats_config *config;
	struct stats_prv_data *prv_data;
	int i;

#ifdef CONFIG_OPLUS_POWERINFO_RPMH
	struct soc_sleep_stats_data *drv;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
#endif

	config = device_get_match_data(&pdev->dev);
	if (!config)
		return -ENODEV;

#ifdef CONFIG_OPLUS_POWERINFO_RPMH
	drv->config = config;
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return PTR_ERR(res);

	offset_addr = ioremap(res->start + config->offset_addr, sizeof(u32));
	if (IS_ERR(offset_addr))
		return PTR_ERR(offset_addr);

	stats_base = res->start | readl_relaxed(offset_addr);
	stats_size = resource_size(res);
	iounmap(offset_addr);

	reg = devm_ioremap(&pdev->dev, stats_base, stats_size);
	if (!reg)
		return -ENOMEM;

#ifdef CONFIG_OPLUS_POWERINFO_RPMH
	drv->reg = reg;
	drv->node = pdev->dev.of_node;
#endif

	prv_data = devm_kzalloc(&pdev->dev, config->num_records *
				sizeof(struct stats_prv_data), GFP_KERNEL);
	if (!prv_data)
		return -ENOMEM;

	for (i = 0; i < config->num_records; i++)
		prv_data[i].config = config;

	ddr_gdata = devm_kzalloc(&pdev->dev, sizeof(*ddr_gdata), GFP_KERNEL);
	if (!ddr_gdata)
		return -ENOMEM;

	ddr_gdata->read_vote_info = false;
	if (!config->ddr_offset_addr)
		goto skip_ddr_stats;

	offset_addr = ioremap(res->start + config->ddr_offset_addr,
								sizeof(u32));
	if (IS_ERR(offset_addr))
		return PTR_ERR(offset_addr);

	stats_base = res->start | readl_relaxed(offset_addr);
	iounmap(offset_addr);

	ddr_gdata->ddr_reg = devm_ioremap(&pdev->dev, stats_base, stats_size);
	if (!ddr_gdata->ddr_reg)
		return -ENOMEM;

	mutex_init(&ddr_gdata->ddr_stats_lock);

	ddr_gdata->entry_count = readl_relaxed(ddr_gdata->ddr_reg + DDR_STATS_NUM_MODES_ADDR);
	if (ddr_gdata->entry_count > DDR_STATS_MAX_NUM_MODES) {
		pr_err("Invalid entry count\n");
		goto skip_ddr_stats;
	}

	ddr_gdata->stats_mbox_cl.dev = &pdev->dev;
	ddr_gdata->stats_mbox_cl.tx_block = true;
	ddr_gdata->stats_mbox_cl.tx_tout = 1000;
	ddr_gdata->stats_mbox_cl.knows_txdone = false;

	ddr_gdata->stats_mbox_ch = mbox_request_channel(&ddr_gdata->stats_mbox_cl, 0);
	if (IS_ERR(ddr_gdata->stats_mbox_ch))
		goto skip_ddr_stats;

	ddr_gdata->read_vote_info = true;

skip_ddr_stats:
	root = create_debugfs_entries(reg, ddr_gdata->ddr_reg,  prv_data,
				      pdev->dev.of_node);
	platform_set_drvdata(pdev, root);

#ifdef CONFIG_OPLUS_POWERINFO_RPMH
	soc_sleep_stats_create_sysfs(pdev, drv);
	drv_backup = drv;
#endif

	return 0;
}

static int soc_sleep_stats_remove(struct platform_device *pdev)
{
	struct dentry *root = platform_get_drvdata(pdev);

	debugfs_remove_recursive(root);

#ifdef CONFIG_OPLUS_POWERINFO_RPMH
	sysfs_remove_file(drv_backup->stat_kobj, &drv_backup->ka_stat_oplus.attr);
	kobject_put(drv_backup->stat_kobj);

	sysfs_remove_file(drv_backup->master_kobj, &drv_backup->ka_master_oplus.attr);
	kobject_put(drv_backup->master_kobj);
#endif

	return 0;
}

static const struct stats_config rpm_data = {
	.offset_addr = 0x14,
	.num_records = 2,
	.appended_stats_avail = true,
};

static const struct stats_config rpmh_data = {
	.offset_addr = 0x4,
	.ddr_offset_addr = 0x1c,
	.num_records = 3,
	.appended_stats_avail = false,
};

static const struct of_device_id soc_sleep_stats_table[] = {
	{ .compatible = "qcom,rpm-sleep-stats", .data = &rpm_data },
	{ .compatible = "qcom,rpmh-sleep-stats", .data = &rpmh_data },
	{ }
};

static struct platform_driver soc_sleep_stats_driver = {
	.probe = soc_sleep_stats_probe,
	.remove = soc_sleep_stats_remove,
	.driver = {
		.name = "soc_sleep_stats",
		.of_match_table = soc_sleep_stats_table,
	},
};
module_platform_driver(soc_sleep_stats_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. (QTI) SoC Sleep Stats driver");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: smem");
