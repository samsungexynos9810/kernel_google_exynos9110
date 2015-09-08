/*
* get proprty of fuel gauge from subCPU
*
* Author: Itsuki Yamashita
*
*/

#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define SUB_BAT_NAME		"subcpu_battery"

#define FAKE_BATT_LEVEL		80

#define CHG_STAT_MASK		0x07
#define BAT_PRESENT_MASK	0x08

#define SB_REMOVED	0
#define SB_PROP_SET 1
#define SB_PROBED	2

enum subcpu_power_data1 {
	PWR_PROP_CURR_LOW = 0x02,
	PWR_PROP_CURR_HIGH,
	PWR_PROP_HEALTH,
};

enum subcpu_power_data2 {
	PWR_PROP_CHGSTATUS = 0x02,
	PWR_PROP_SOC,
	PWR_PROP_TEMP_LOW,
	PWR_PROP_TEMP_HIGH,
	PWR_PROP_VOLT_LOW,
	PWR_PROP_VOLT_HIGH,
};

enum subcpu_health {
	PWR_HEALTH_NORMAL = 0x00,
	PWR_HEALTH_LOW_TEMP_BURN,
};

struct sub_battery {
	struct device *dev;
	struct power_supply battery;
	struct power_supply ac;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;

	int soc;
	int temperature;
	int voltage;
	int current_now;
	int status;
	int health;
	int bat_present;
	int is_ac_online;

	uint8_t sb_status; // 0: not probed 1: parameter is set 2: probed
};
static struct sub_battery g_sb;

static enum power_supply_property sub_battery_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
};

static enum power_supply_property sub_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	SUB_BAT_NAME,
};

static int sub_bat_voltage(uint8_t volt_low, uint8_t volt_high)
{
	int volt = (uint16_t)(volt_low | volt_high << 8);
	volt *= 1000;

	return volt;
}

static int sub_bat_current(uint8_t curr_low, uint8_t curr_high)
{
	int curr = (int16_t)(curr_low | curr_high << 8);
	curr *= 1000;

	return curr;
}

static int sub_bat_temperature(uint8_t temp_low, uint8_t temp_high)
{
	return (int16_t)(temp_low | temp_high << 8);
}

static int sub_bat_soc(uint8_t soc)
{
	return (int)soc;
}

static int sub_bat_status(uint8_t chg_status)
{
	return chg_status & CHG_STAT_MASK;
}

static int sub_bat_health(uint8_t health)
{
	int ret;

	switch (health) {
	case PWR_HEALTH_NORMAL:
		ret = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case PWR_HEALTH_LOW_TEMP_BURN:
		ret = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	default:
		ret = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	return ret;
}

static int sub_bat_present(uint8_t chg_status)
{
	if (chg_status & BAT_PRESENT_MASK)
		return 1;
	else
		return 0;
}

static int sub_is_ac_online(uint8_t chg_status)
{
	uint8_t status = chg_status & CHG_STAT_MASK;

	if (status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
		status == POWER_SUPPLY_STATUS_CHARGING ||
		status == POWER_SUPPLY_STATUS_FULL)
		return 1;
	else
		return 0;
}

void subcpu_battery_update_status1(uint8_t* data)
{
	struct sub_battery *sb = &g_sb;

	sb->current_now =
		sub_bat_current(data[PWR_PROP_CURR_LOW], data[PWR_PROP_CURR_HIGH]);
	sb->health = sub_bat_health(data[PWR_PROP_HEALTH]);
}

void subcpu_battery_update_status2(uint8_t* data)
{
	struct sub_battery *sb = &g_sb;

	sb->status =
		sub_bat_status(data[PWR_PROP_CHGSTATUS]);
	sb->bat_present =
		sub_bat_present(data[PWR_PROP_CHGSTATUS]);
	if (sb->bat_present == 0)
		sb->soc = FAKE_BATT_LEVEL;
	else
		sb->soc = sub_bat_soc(data[PWR_PROP_SOC]);
	sb->temperature =
		sub_bat_temperature(data[PWR_PROP_TEMP_LOW], data[PWR_PROP_TEMP_HIGH]);
	sb->voltage =
		sub_bat_voltage(data[PWR_PROP_VOLT_LOW], data[PWR_PROP_VOLT_HIGH]);
	sb->is_ac_online = sub_is_ac_online(data[PWR_PROP_CHGSTATUS]);

	if (sb->sb_status == SB_PROBED)
		power_supply_changed(&sb->battery);
	else
		sb->sb_status = SB_PROP_SET;
}

static int sub_bat_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sub_battery *sb = &g_sb;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = sb->soc;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = sb->voltage;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = sb->current_now;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = sb->temperature;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = sb->status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = sb->health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sb->bat_present;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sub_ac_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sub_battery *sb = &g_sb;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = sb->is_ac_online;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sub_bat_probe(struct platform_device *pdev)
{
	struct sub_battery *sb = &g_sb;
	int ret;

	platform_set_drvdata(pdev, sb);

	sb->battery.name			= SUB_BAT_NAME;
	sb->battery.type			= POWER_SUPPLY_TYPE_BATTERY;
	sb->battery.get_property	= sub_bat_get_property;
	sb->battery.properties		= sub_battery_props;
	sb->battery.num_properties	= ARRAY_SIZE(sub_battery_props);

	if (sb->sb_status != SB_PROP_SET) {
		sb->soc			= FAKE_BATT_LEVEL;
		sb->temperature	= 0;
		sb->voltage		= 0;
		sb->current_now		= 0;
		sb->status 		= POWER_SUPPLY_STATUS_CHARGING;
		sb->health		= POWER_SUPPLY_HEALTH_GOOD;
		sb->bat_present = 1;
	}

	ret = power_supply_register(&pdev->dev, &sb->battery);

	if (ret) {
		dev_err(&pdev->dev, "failed: power supply bat register\n");
		platform_set_drvdata(pdev,NULL);
		return ret;
	}

	sb->ac.name			= "ac";
	sb->ac.type			= POWER_SUPPLY_TYPE_MAINS;
	sb->ac.supplied_to	= supply_list;
	sb->ac.num_supplicants = ARRAY_SIZE(supply_list);
	sb->ac.properties	= sub_power_props;
	sb->ac.get_property	= sub_ac_get_property;
	sb->ac.num_properties	= ARRAY_SIZE(sub_power_props);

	ret = power_supply_register(&pdev->dev, &sb->ac);

	if (ret) {
		dev_err(&pdev->dev, "failed: power supply ac register\n");
		platform_set_drvdata(pdev,NULL);
		return ret;
	}
	sb->sb_status = SB_PROBED;

	return ret;
}

static int sub_bat_remove(struct platform_device *pdev)
{
	struct sub_battery *sb = &g_sb;

	sb->sb_status = SB_REMOVED;
	power_supply_unregister(&sb->battery);
	platform_set_drvdata(pdev,NULL);

	return 0;
}

static const struct of_device_id sub_battery_of_match[] = {
	{ .compatible = "casio,sub-battery", },
	{},
};
MODULE_DEVICE_TABLE(of, sub_battery_of_match);

static struct platform_driver subcpu_battery_driver = {
	.driver = {
		.name = "subcpu-battery",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sub_battery_of_match),
	},
	.probe = sub_bat_probe,
	.remove = sub_bat_remove
};
module_platform_driver(subcpu_battery_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("subCPU fuel gauge");
MODULE_AUTHOR("Itsuki Yamashita");
