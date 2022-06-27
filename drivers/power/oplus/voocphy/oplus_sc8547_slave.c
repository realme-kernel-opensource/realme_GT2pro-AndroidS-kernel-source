/*
 * SC8547 battery charging driver
*/

#define pr_fmt(fmt)	"[sc8547] %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/proc_fs.h>

#include <trace/events/sched.h>
#include<linux/ktime.h>
#include "oplus_voocphy.h"
#include "../oplus_vooc.h"
#include "../oplus_gauge.h"
#include "../oplus_charger.h"
#include "oplus_sc8547.h"

static struct mutex i2c_rw_lock;
static int sc8547_slave_get_chg_enable(struct oplus_voocphy_manager *chip, u8 *data);

/************************************************************************/
static int __sc8547_slave_read_byte(struct i2c_client *client, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8547_slave_write_byte(struct i2c_client *client, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}

	return 0;
}

static int sc8547_slave_read_byte(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&i2c_rw_lock);
	ret = __sc8547_slave_read_byte(client, reg, data);
	mutex_unlock(&i2c_rw_lock);

	return ret;
}

static int sc8547_slave_write_byte(struct i2c_client *client, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&i2c_rw_lock);
	ret = __sc8547_slave_write_byte(client, reg, data);
	mutex_unlock(&i2c_rw_lock);

	return ret;
}


static int sc8547_slave_update_bits(struct i2c_client *client, u8 reg,
                                    u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&i2c_rw_lock);
	ret = __sc8547_slave_read_byte(client, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8547_slave_write_byte(client, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
out:
	mutex_unlock(&i2c_rw_lock);
	return ret;
}

static void sc8547_slave_update_data(struct oplus_voocphy_manager *chip)
{
	u8 data_block[2] = {0};
	int i = 0;
	u8 data = 0;
	u8 int_flag = 0;

	sc8547_slave_read_byte(chip->slave_client, SC8547_REG_0F, &int_flag);

	/*parse data_block for improving time of interrupt*/
	i2c_smbus_read_i2c_block_data(chip->slave_client, SC8547_REG_13, 2, data_block);
	for (i=0; i<2; i++) {
		pr_info("data_block[%d] = %u\n", i, data_block[i]);
	}
	chip->slave_cp_ichg = ((data_block[0] << 8) | data_block[1])*1875 / 1000;
	pr_info("slave cp_ichg = %d int_flag = %d", chip->slave_cp_ichg, int_flag);
}
/*********************************************************************/
int sc8547_slave_get_ichg(struct oplus_voocphy_manager *chip)
{
	u8 slave_cp_enable;
	sc8547_slave_update_data(chip);

	sc8547_slave_get_chg_enable(chip, &slave_cp_enable);
	if(chip->slave_ops) {
		if(slave_cp_enable == 1)
			return chip->slave_cp_ichg;
		else
			return 0;
	} else {
		return 0;
	}
}

static int sc8547_slave_get_cp_status(struct oplus_voocphy_manager *chip)
{
	u8 data_reg06, data_reg07;
	int ret_reg06, ret_reg07;

	if (!chip) {
		pr_err("Failed\n");
		return 0;
	}

	ret_reg06 = sc8547_slave_read_byte(chip->slave_client, SC8547_REG_06, &data_reg06);
	ret_reg07 = sc8547_slave_read_byte(chip->slave_client, SC8547_REG_07, &data_reg07);

	if (ret_reg06 < 0 || ret_reg07 < 0) {
		pr_err("SC8547_REG_06 or SC8547_REG_07 err\n");
		return 0;
	}
	data_reg06 = data_reg06 & SC8547_CP_SWITCHING_STAT_MASK;
	data_reg06 = data_reg06 >> 2;
	data_reg07 = data_reg07 >> 7;

	pr_err("reg06 = %d reg07 = %d\n", data_reg06, data_reg07);

	if (data_reg06 == 1 && data_reg07 == 1) {
		return 1;
	} else {
		return 0;
	}

}

static int sc8547_slave_reg_reset(struct oplus_voocphy_manager *chip, bool enable)
{
	int ret;
	u8 val;
	if (enable)
		val = SC8547_RESET_REG;
	else
		val = SC8547_NO_REG_RESET;

	val <<= SC8547_REG_RESET_SHIFT;

	ret = sc8547_slave_update_bits(chip->slave_client, SC8547_REG_07,
	                               SC8547_REG_RESET_MASK, val);

	return ret;
}


static int sc8547_slave_get_chg_enable(struct oplus_voocphy_manager *chip, u8 *data)
{
	int ret = 0;

	if (!chip) {
		pr_err("Failed\n");
		return -1;
	}

	ret = sc8547_slave_read_byte(chip->slave_client, SC8547_REG_07, data);
	if (ret < 0) {
		pr_err("SC8547_REG_07\n");
		return -1;
	}
	*data = *data >> 7;

	return ret;
}

static int sc8547_slave_set_chg_enable(struct oplus_voocphy_manager *chip, bool enable)
{
	u8 data = 0;

	if (!chip) {
		pr_err("Failed\n");
		return -1;
	}

	if (enable) {
		pr_err("SC8547 slave open charge 0x7 reg will be 0x85!\n");
		return sc8547_slave_write_byte(chip->slave_client, SC8547_REG_07, 0x85);
	} else {
		pr_err("SC8547 slave close charge 0x7 reg will be 0x05!\n");
		return sc8547_slave_write_byte(chip->slave_client, SC8547_REG_07, 0x05);
	}
}

static int sc8547_slave_init_device(struct oplus_voocphy_manager *chip)
{
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_11, 0x00);	//ADC_CTRL:disable
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_02, 0x07);	//
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_04, 0x00);	//VBUS_OVP:10 2:1 or 1:1V
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_00, 0x2E);	//VBAT_OVP:4.65V
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_05, 0x28);	//IBUS_OCP_UCP:3.6A
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_2B, 0x00);	//VOOC_CTRL:disable
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_30, 0x7F);
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_3C, 0x40);
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_07, 0x05);  //adjust slave freq

	return 0;
}

static int sc8547_slave_init_vooc(struct oplus_voocphy_manager *chip)
{
	u8 value;
	pr_err("sc8547_slave_init_vooc\n");

	sc8547_slave_reg_reset(chip, true);
	sc8547_slave_init_device(chip);

	return 0;
}

static int sc8547_slave_svooc_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_02, 0x01);	//VAC_OVP:12v
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_04, 0x50);	//VBUS_OVP:10v
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_05, 0x28);	//IBUS_OCP_UCP:3.6A
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_09, 0x03);	//WD:1000ms
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_3C, 0x40);	//VOOC_CTRL:disable
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_11, 0x80);	//ADC_CTRL:ADC_EN
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_07, 0x05);  //adjust slave freq

	return 0;
}

static int sc8547_slave_vooc_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_02, 0x07);	//VAC_OVP:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_04, 0x50);	//ADC_CTRL:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_05, 0x2c);	//IBUS_OCP_UCP:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_07, 0x05);	//CTRL1:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_09, 0x83);	//WD:5000ms
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_11, 0x80);	//ADC_CTRL:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_2B, 0x00);	//VOOC_CTRL
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_3C, 0x40);	//VOOC_CTRL:disable

	return 0;
}

static int sc8547_slave_5v2a_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_02, 0x07);	//VAC_OVP:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_04, 0x00);	//VBUS_OVP:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_07, 0x05);
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_09, 0x00);	//WD:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_11, 0x00);	//ADC_CTRL:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_2B, 0x00);	//VOOC_CTRL

	return 0;
}

static int sc8547_slave_pdqc_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_02, 0x01);	//VAC_OVP:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_04, 0x50);	//VBUS_OVP:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_07, 0x05);
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_09, 0x00);	//WD:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_11, 0x00);	//ADC_CTRL:
	sc8547_slave_write_byte(chip->slave_client, SC8547_REG_2B, 0x00);	//VOOC_CTRL
	return 0;
}

static int sc8547_slave_hw_setting(struct oplus_voocphy_manager *chip, int reason)
{
	if (!chip) {
		pr_err("chip is null exit\n");
		return -1;
	}
	switch (reason) {
	case SETTING_REASON_PROBE:
	case SETTING_REASON_RESET:
		sc8547_slave_init_device(chip);
		pr_info("SETTING_REASON_RESET OR PROBE\n");
		break;
	case SETTING_REASON_SVOOC:
		sc8547_slave_svooc_hw_setting(chip);
		pr_info("SETTING_REASON_SVOOC\n");
		break;
	case SETTING_REASON_VOOC:
		sc8547_slave_vooc_hw_setting(chip);
		pr_info("SETTING_REASON_VOOC\n");
		break;
	case SETTING_REASON_5V2A:
		sc8547_slave_5v2a_hw_setting(chip);
		pr_info("SETTING_REASON_5V2A\n");
		break;
	case SETTING_REASON_PDQC:
		sc8547_slave_pdqc_hw_setting(chip);
		pr_info("SETTING_REASON_PDQC\n");
		break;
	default:
		pr_err("do nothing\n");
		break;
	}
	return 0;
}

static int sc8547_slave_reset_voocphy(struct oplus_voocphy_manager *chip)
{
	sc8547_slave_set_chg_enable(chip, false);
	sc8547_slave_hw_setting(chip, SETTING_REASON_RESET);

	return VOOCPHY_SUCCESS;
}

static ssize_t sc8547_slave_show_registers(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct oplus_voocphy_manager *chip = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8547");
	for (addr = 0x0; addr <= 0x3C; addr++) {
		if((addr < 0x24) || (addr > 0x2B && addr < 0x33)
		   || addr == 0x36 || addr == 0x3C) {
			ret = sc8547_slave_read_byte(chip->slave_client, addr, &val);
			if (ret == 0) {
				len = snprintf(tmpbuf, PAGE_SIZE - idx,
				               "Reg[%.2X] = 0x%.2x\n", addr, val);
				memcpy(&buf[idx], tmpbuf, len);
				idx += len;
			}
		}
	}

	return idx;
}

static ssize_t sc8547_slave_store_register(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	struct oplus_voocphy_manager *chip = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x3C)
		sc8547_slave_write_byte(chip->slave_client, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, 0660, sc8547_slave_show_registers, sc8547_slave_store_register);

static void sc8547_slave_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}


static struct of_device_id sc8547_slave_charger_match_table[] = {
	{
		.compatible = "sc,sc8547-slave",
	},
	{},
};

static struct oplus_voocphy_operations oplus_sc8547_slave_ops = {
	.hw_setting		= sc8547_slave_hw_setting,
	.init_vooc		= sc8547_slave_init_vooc,
	.update_data		= sc8547_slave_update_data,
	.get_chg_enable		= sc8547_slave_get_chg_enable,
	.set_chg_enable		= sc8547_slave_set_chg_enable,
	.get_ichg			= sc8547_slave_get_ichg,
	.reset_voocphy      = sc8547_slave_reset_voocphy,
	.get_cp_status 		= sc8547_slave_get_cp_status,
};

static int sc8547_slave_charger_probe(struct i2c_client *client,
                                      const struct i2c_device_id *id)
{
	struct oplus_voocphy_manager *chip;
	int ret;

	pr_err("sc8547_slave_slave_charger_probe enter!\n");

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}

	chip->slave_client = client;
	chip->slave_dev = &client->dev;
	mutex_init(&i2c_rw_lock);
	i2c_set_clientdata(client, chip);

	if (oplus_voocphy_chip_is_null()) {
		pr_err("oplus_voocphy_chip null, will do after master cp init.\n");
		return -EPROBE_DEFER;
	}

	sc8547_slave_create_device_node(&(client->dev));

	sc8547_slave_reg_reset(chip, true);

	sc8547_slave_init_device(chip);

#if 0
	ret = sc8547_slave_irq_register(chip);
	pr_err("sc8547_slave_slave_charger_probe enter 7!\n");
	if (ret < 0)
		goto err_1;
#endif
	chip->slave_ops = &oplus_sc8547_slave_ops;

	oplus_voocphy_slave_init(chip);

	pr_err("sc8547_slave_parse_dt successfully!\n");

	return 0;
#if 0
err_1:
	pr_err("sc8547 probe err_1\n");
	return ret;
#endif
}

static void sc8547_slave_charger_shutdown(struct i2c_client *client)
{
	sc8547_slave_write_byte(client, SC8547_REG_11, 0x00);
	sc8547_slave_write_byte(client, SC8547_REG_21, 0x00);

	return;
}

static const struct i2c_device_id sc8547_slave_charger_id[] = {
	{"sc8547-slave", 0},
	{},
};

static struct i2c_driver sc8547_slave_charger_driver = {
	.driver		= {
		.name	= "sc8547-charger-slave",
		.owner	= THIS_MODULE,
		.of_match_table = sc8547_slave_charger_match_table,
	},
	.id_table	= sc8547_slave_charger_id,

	.probe		= sc8547_slave_charger_probe,
	.shutdown	= sc8547_slave_charger_shutdown,
};

static int __init sc8547_slave_subsys_init(void)
{
	int ret = 0;
	chg_debug(" init start\n");

	if (i2c_add_driver(&sc8547_slave_charger_driver) != 0) {
		chg_err(" failed to register sc8547 i2c driver.\n");
	} else {
		chg_debug(" Success to register sc8547 i2c driver.\n");
	}

	return ret;
}

subsys_initcall(sc8547_slave_subsys_init);
//module_i2c_driver(sc8547_slave_charger_driver);

MODULE_DESCRIPTION("SC SC8547 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Aiden-yu@southchip.com");
