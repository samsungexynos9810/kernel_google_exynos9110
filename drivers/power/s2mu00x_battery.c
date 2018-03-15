/*
 * s2mu00x_battery.c - Example battery driver
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/version.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/power/s2mu00x_charger_common.h>
#include <linux/power/s2mu00x_battery.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#define FAKE_BAT_LEVEL          50
#define DEFAULT_HEALTH_CHECK_COUNT  5
#define DEFAULT_POLLING_PERIOD	10

static char *charging_status_str[] = {
	"None",
	"Progress",
};

static char *bat_status_str[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not-charging",
	"Full"
};

static char *health_str[] = {
	"Unknown",
	"Good",
	"Overheat",
	"Dead",
	"OverVoltage",
	"UnspecFailure",
	"Cold",
	"WatchdogTimerExpire",
	"SafetyTimerExpire",
	"Warm",
	"Cool",
	"UnderVoltage",
	"OverheatLimit"
};

static enum power_supply_property s2mu00x_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property s2mu00x_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

typedef struct s2mu00x_battery_platform_data {
	sec_charging_current_t *charging_current;
	char *charger_name;
	char *fuelgauge_name;

	unsigned int temp_check_count;
	int temp_highlimit_threshold_normal;
	int temp_highlimit_recovery_normal;
	int temp_high_threshold_normal;
	int temp_high_recovery_normal;
	int temp_low_threshold_normal;
	int temp_low_recovery_normal;

	/* full check */
	unsigned int full_check_count;
	unsigned int chg_recharge_vcell;
	unsigned int chg_full_vcell;

	/* battery */
	char *vendor;
	int technology;
	int battery_type;
	void *battery_data;
} s2mu00x_battery_platform_data_t;

struct s2mu00x_battery_info {
	struct device *dev;
	s2mu00x_battery_platform_data_t *pdata;

	struct power_supply *psy_battery;
	struct power_supply_desc psy_battery_desc;
	struct power_supply *psy_usb;
	struct power_supply_desc psy_usb_desc;
	struct power_supply *psy_ac;
	struct power_supply_desc psy_ac_desc;

	struct mutex iolock;

	struct delayed_work polling_work;
	struct wake_lock monitor_wake_lock;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
	struct wake_lock vbus_wake_lock;

	int input_current;
	int charging_current;
	int topoff_current;
	int cable_type;
	bool is_charging;

	struct notifier_block batt_nb;

	int full_check_cnt;

	/* charging */
	unsigned int charging_status;
	unsigned int charging_mode;
	bool is_recharging;

	int unhealth_cnt;
	bool battery_valid;
	int status;
	int health;
	bool present;

	int voltage_now;
	int voltage_avg;
	int voltage_ocv;

	unsigned int capacity;
	unsigned int input_voltage;     /* CHGIN/WCIN input voltage (V) */
	unsigned int charge_power;      /* charge power (mW) */
	int current_now;        /* current (mA) */
	int current_avg;        /* average current (mA) */
	int current_max;        /* input current limit (mA) */

#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cable_check;
#endif

	/* temperature check */
	int temperature;    /* battery temperature */

	int temp_highlimit_threshold;
	int temp_highlimit_recovery;
	int temp_high_threshold;
	int temp_high_recovery;
	int temp_low_threshold;
	int temp_low_recovery;

	unsigned int temp_highlimit_cnt;
	unsigned int temp_high_cnt;
	unsigned int temp_low_cnt;
	unsigned int temp_recover_cnt;

	unsigned int loop_cnt; /* for checking booting time roughly */
};

static char *s2mu00x_supplied_to[] = {
	"s2mu00x-battery",
};

unsigned int batt_booting_chk;

static int set_charge(
		struct s2mu00x_battery_info *battery,
		bool enable)
{
	union power_supply_propval val;

	pr_info("%s: battery status[%d]\n", __func__, battery->status);
	if (battery->cable_type == POWER_SUPPLY_TYPE_OTG)
		return 0;

	if (enable)
		val.intval = battery->cable_type;
	else
		val.intval = POWER_SUPPLY_TYPE_BATTERY;

	battery->temp_highlimit_cnt = 0;
	battery->temp_high_cnt = 0;
	battery->temp_low_cnt = 0;
	battery->temp_recover_cnt = 0;

	psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_ONLINE, val);

	psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_ONLINE, val);

	return 0;
}

static void set_battery_status(struct s2mu00x_battery_info *battery,
		int status)
{
	union power_supply_propval value;

	switch (status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		/* charger on */
		set_charge(battery, true);
		battery->charging_status = SEC_BATTERY_CHARGING_PROGRESS;
		break;

	case POWER_SUPPLY_STATUS_DISCHARGING:
		/* charger off */
		set_charge(battery, false);
		battery->charging_status = SEC_BATTERY_CHARGING_NONE;
		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		/* charger off */
		set_charge(battery, false);
		battery->charging_status = SEC_BATTERY_CHARGING_NONE;
		break;

	case POWER_SUPPLY_STATUS_FULL:
		/* charger off (charger mode update) */
		set_charge(battery, false);
		battery->charging_status = SEC_BATTERY_CHARGING_NONE;
		break;
	}

	/* battery status update */
	battery->status = status;
	value.intval = battery->status;
	psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_STATUS, value);
}

static int s2mu00x_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct s2mu00x_battery_info *battery =  power_supply_get_drvdata(psy);
	int ret = 0;

	dev_dbg(battery->dev, "prop: %d\n", psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = battery->status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = battery->health;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = battery->cable_type;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battery->battery_valid;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (!battery->battery_valid)
			val->intval = FAKE_BAT_LEVEL;
		else
			val->intval = battery->voltage_now * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = battery->voltage_avg * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->temperature;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = battery->charging_mode;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!battery->battery_valid)
			val->intval = FAKE_BAT_LEVEL;
		else {
			if (battery->status == POWER_SUPPLY_STATUS_FULL)
				val->intval = 100;
			else
				val->intval = battery->capacity;
		}
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static int s2mu00x_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu00x_battery_info *battery =  power_supply_get_drvdata(psy);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	switch (battery->cable_type) {
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_ACA:
		val->intval = 1;
		break;
	default:
		val->intval = 0;
		break;
	}

	return 0;
}

/*
 * AC charger operations
 */
static int s2mu00x_ac_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu00x_battery_info *battery =  power_supply_get_drvdata(psy);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	switch (battery->cable_type) {
	case POWER_SUPPLY_TYPE_MAINS:
	case POWER_SUPPLY_TYPE_UARTOFF:
	case POWER_SUPPLY_TYPE_LAN_HUB:
	case POWER_SUPPLY_TYPE_UNKNOWN:
	case POWER_SUPPLY_TYPE_HV_PREPARE_MAINS:
	case POWER_SUPPLY_TYPE_HV_ERR:
	case POWER_SUPPLY_TYPE_HV_UNKNOWN:
	case POWER_SUPPLY_TYPE_HV_MAINS:
		val->intval = 1;
		break;
	default:
		val->intval = 0;
		break;
	}

	return 0;
}

static int s2mu00x_bat_cable_check(struct s2mu00x_battery_info *battery,
		muic_attached_dev_t attached_dev)
{
	int current_cable_type = -1;

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_LAN_HUB;
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_POWER_SHARING;
		break;
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_PREPARE_MAINS;
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_MAINS;
		break;
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_ERR;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_UNKNOWN;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
				__func__, attached_dev);
	}

	return current_cable_type;
}

static int s2mu00x_battery_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	const char *cmd;
	int cable_type;
	union power_supply_propval value;
	struct s2mu00x_battery_info *battery =
		container_of(nb, struct s2mu00x_battery_info, batt_nb);

	if (attached_dev == ATTACHED_DEV_MHL_MUIC)
		return 0;

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		cmd = "DETACH";
		cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		cmd = "ATTACH";
		cable_type = s2mu00x_bat_cable_check(battery, attached_dev);
		break;
	default:
		cmd = "ERROR";
		cable_type = -1;
		break;
	}

	pr_info("%s: current_cable(%d) former cable_type(%d) battery_valid(%d)\n",
			__func__, cable_type, battery->cable_type,
			battery->battery_valid);
	if (battery->battery_valid == false)
		pr_info("%s: Battery is disconnected\n", __func__);

	battery->cable_type = cable_type;
	pr_info("%s: CMD=%s, attached_dev=%d battery_cable=%d\n",
			__func__, cmd, attached_dev, battery->cable_type);


	if (attached_dev == ATTACHED_DEV_OTG_MUIC) {
		if (!strcmp(cmd, "ATTACH")) {
			value.intval = true;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
					value);
			pr_info("%s: OTG cable attached\n", __func__);
		} else {
			value.intval = false;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
					value);
			pr_info("%s: OTG cable detached\n", __func__);
		}
	}

	if (battery->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
			battery->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		battery->is_recharging = false;
		set_battery_status(battery, POWER_SUPPLY_STATUS_DISCHARGING);
	} else {
		if (battery->cable_type == POWER_SUPPLY_TYPE_OTG ||
				battery->cable_type == POWER_SUPPLY_TYPE_POWER_SHARING) {
			set_battery_status(battery, POWER_SUPPLY_STATUS_DISCHARGING);
		} else {
			if (battery->status != POWER_SUPPLY_STATUS_FULL)
				set_battery_status(battery, POWER_SUPPLY_STATUS_CHARGING);
		}
	}

	pr_info(
			"%s: Status(%s), charging(%s), Health(%s), Cable(%d), Recharging(%d))\n",
			__func__,
			bat_status_str[battery->status],
			charging_status_str[battery->charging_status],
			health_str[battery->health],
			battery->cable_type,
			battery->is_recharging
		  );

	cancel_delayed_work(&battery->polling_work);
	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	return 0;
}

static void get_battery_info(struct s2mu00x_battery_info *battery)
{
	union power_supply_propval value;

	/* Get voltage and current value */
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	battery->voltage_now = value.intval;

	value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	battery->voltage_avg = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);
	battery->current_now = value.intval;

	/* Get SOC value (NOT raw SOC) */
	value.intval = 0;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
	battery->capacity = value.intval;

	/* Get temperature info */
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_TEMP, value);
	battery->temperature = value.intval;

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_STATUS, value);
	if (battery->status != value.intval)
		pr_err("%s: battery status = %d, charger status = %d\n",
				__func__, battery->status, value.intval);

	pr_info(
			"%s:Vnow(%dmV),Inow(%dmA),SOC(%d%%),Tbat(%d),Vavg(%dmV)\n",
			__func__,
			battery->voltage_now, battery->current_now,
			battery->capacity, battery->temperature,
			battery->voltage_avg
			);
}

static bool bat_health_check(struct s2mu00x_battery_info *battery)
{
	union power_supply_propval value;
	int health;

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		dev_dbg(battery->dev, "%s: Charging Disabled\n", __func__);
		return true;
	} else if ((battery->status == POWER_SUPPLY_STATUS_FULL) &&
			(battery->charging_status == SEC_BATTERY_CHARGING_NONE)) {
		dev_dbg(battery->dev, "%s: Charging Finished\n", __func__);
		return true;
	}

	/* Get health status from charger */
	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_HEALTH, value);
	health = value.intval;

	/* Check OVP, UVLO, and Vchgin-Vbat
	 * if happened, stop charging and re-enable when voltage is good
	 */
	if (battery->health != health) {
		battery->health = health;
		switch (health) {
		case POWER_SUPPLY_HEALTH_GOOD:
			dev_info(battery->dev, "%s: Safe voltage\n", __func__);
			dev_info(battery->dev, "%s: is_recharging : %d\n", __func__, battery->is_recharging);
			set_battery_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			break;
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			dev_info(battery->dev,
					"%s: Unsafe voltage (%d)\n",
					__func__, health);
			set_battery_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
			battery->is_recharging = false;
			/* Take the wakelock during 10 seconds
			   when over-voltage status is detected  */
			wake_lock_timeout(&battery->vbus_wake_lock, HZ * 10);
			break;
		}
	}
	return true;
}

static bool bat_temperature_check(
		struct s2mu00x_battery_info *battery)
{
	int temp_value;
	int pre_health;

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
		dev_dbg(battery->dev, "%s: Charging Disabled\n", __func__);
		return true;
	}

	if (battery->health != POWER_SUPPLY_HEALTH_GOOD &&
			battery->health != POWER_SUPPLY_HEALTH_OVERHEAT &&
			battery->health != POWER_SUPPLY_HEALTH_COLD &&
			battery->health != POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
		dev_dbg(battery->dev, "%s: No need to check\n", __func__);
		return false;
	}

	temp_value = battery->temperature;
	pre_health = battery->health;

	/* Check temperature range */
	if (temp_value >= battery->temp_highlimit_threshold) {
		if (battery->health != POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
			if (battery->temp_highlimit_cnt < battery->pdata->temp_check_count) {
				battery->temp_highlimit_cnt++;
				battery->temp_high_cnt = 0;
				battery->temp_low_cnt = 0;
				battery->temp_recover_cnt = 0;
			}
			dev_dbg(battery->dev, "%s: highlimit count = %d\n",
					__func__, battery->temp_highlimit_cnt);
		}
	} else if (temp_value >= battery->temp_high_threshold) {
		if (battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT) {
			if (temp_value <= battery->temp_highlimit_recovery) {
				if (battery->temp_recover_cnt < battery->pdata->temp_check_count) {
					battery->temp_recover_cnt++;
					battery->temp_highlimit_cnt = 0;
					battery->temp_high_cnt = 0;
					battery->temp_low_cnt = 0;
				}
				dev_dbg(battery->dev, "%s: recovery count = %d\n",
						__func__, battery->temp_recover_cnt);
			}
		} else if (battery->health != POWER_SUPPLY_HEALTH_OVERHEAT) {
			if (battery->temp_high_cnt < battery->pdata->temp_check_count) {
				battery->temp_high_cnt++;
				battery->temp_highlimit_cnt = 0;
				battery->temp_low_cnt = 0;
				battery->temp_recover_cnt = 0;
			}
			dev_dbg(battery->dev, "%s: high count = %d\n",
					__func__, battery->temp_high_cnt);
		}
	} else if ((temp_value <= battery->temp_high_recovery) &&
			(temp_value >= battery->temp_low_recovery)) {
		if (battery->health == POWER_SUPPLY_HEALTH_OVERHEAT ||
				battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT ||
				battery->health == POWER_SUPPLY_HEALTH_COLD) {
			if (battery->temp_recover_cnt < battery->pdata->temp_check_count) {
				battery->temp_recover_cnt++;
				battery->temp_highlimit_cnt = 0;
				battery->temp_high_cnt = 0;
				battery->temp_low_cnt = 0;
			}
			dev_dbg(battery->dev, "%s: recovery count = %d\n",
					__func__, battery->temp_recover_cnt);
		}
	} else if (temp_value <= battery->temp_low_threshold) {
		if (battery->health != POWER_SUPPLY_HEALTH_COLD) {
			if (battery->temp_low_cnt < battery->pdata->temp_check_count) {
				battery->temp_low_cnt++;
				battery->temp_highlimit_cnt = 0;
				battery->temp_high_cnt = 0;
				battery->temp_recover_cnt = 0;
			}
			dev_dbg(battery->dev, "%s: low count = %d\n",
					__func__, battery->temp_low_cnt);
		}
	} else {
		battery->temp_highlimit_cnt = 0;
		battery->temp_high_cnt = 0;
		battery->temp_low_cnt = 0;
		battery->temp_recover_cnt = 0;
	}

	/* Judge if status needs to be changed or not */
	if (battery->temp_highlimit_cnt >= battery->pdata->temp_check_count) {
		battery->health = POWER_SUPPLY_HEALTH_OVERHEATLIMIT;
		battery->temp_highlimit_cnt = 0;
	} else if (battery->temp_high_cnt >= battery->pdata->temp_check_count) {
		battery->health = POWER_SUPPLY_HEALTH_OVERHEAT;
		battery->temp_high_cnt = 0;
	} else if (battery->temp_low_cnt >= battery->pdata->temp_check_count) {
		battery->health = POWER_SUPPLY_HEALTH_COLD;
		battery->temp_low_cnt = 0;
	} else if (battery->temp_recover_cnt >= battery->pdata->temp_check_count) {
		if (battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT)
			battery->health = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			battery->health = POWER_SUPPLY_HEALTH_GOOD;

		battery->temp_recover_cnt = 0;
	}

	/* Update battery status */
	if (battery->health != pre_health) {
		if ((battery->health == POWER_SUPPLY_HEALTH_OVERHEAT) ||
				(battery->health == POWER_SUPPLY_HEALTH_COLD) ||
				(battery->health == POWER_SUPPLY_HEALTH_OVERHEATLIMIT)) {
			if (battery->status != POWER_SUPPLY_STATUS_NOT_CHARGING) {
				dev_info(battery->dev, "%s: Unsafe Temperature(%d)\n",
						__func__, battery->temperature);
				set_battery_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
				return false;
			}
		} else {
			/* if recovered from not charging */
			if ((battery->health == POWER_SUPPLY_HEALTH_GOOD) &&
					(battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
				dev_info(battery->dev, "%s: Safe Temperature\n", __func__);
				set_battery_status(battery, POWER_SUPPLY_STATUS_CHARGING);
				return false;
			}
		}
	}
	return true;
}

static void check_charging_full(
		struct s2mu00x_battery_info *battery)
{
	pr_info("%s Start\n", __func__);

	if ((battery->status == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
		dev_dbg(battery->dev,
				"%s: No Need to Check Full-Charged\n", __func__);
		return;
	}

	/* 1. Recharging check */
	if (battery->status == POWER_SUPPLY_STATUS_FULL &&
			battery->voltage_now < battery->pdata->chg_recharge_vcell &&
			!battery->is_recharging) {
		pr_info("%s: Recharging start\n", __func__);
		set_battery_status(battery, POWER_SUPPLY_STATUS_CHARGING);
		battery->is_recharging = true;
	}

	/* 2. Full charged check */
	if ((battery->current_now > 0 && battery->current_now <
				battery->pdata->charging_current[
				battery->cable_type].full_check_current) &&
			(battery->voltage_avg > battery->pdata->chg_full_vcell)) {
		battery->full_check_cnt++;
		pr_info("%s: Full Check Cnt (%d)\n", __func__, battery->full_check_cnt);
	}

	/* 3. If full charged, turn off charging. */
	if (battery->full_check_cnt >= battery->pdata->full_check_count) {
		battery->full_check_cnt = 0;
		battery->is_recharging = false;
		set_battery_status(battery, POWER_SUPPLY_STATUS_FULL);
		pr_info("%s: Full charged, charger off\n", __func__);
	}
}

static void bat_polling_work(struct work_struct *work)
{
	struct s2mu00x_battery_info *battery =
		container_of(work, struct s2mu00x_battery_info, polling_work.work);

	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
}

static void set_monitor_polling(struct s2mu00x_battery_info *battery)
{
	unsigned int polling_time;

	polling_time = DEFAULT_POLLING_PERIOD;
	schedule_delayed_work(&battery->polling_work, polling_time * HZ);
}

static void bat_monitor_work(struct work_struct *work)
{
	struct s2mu00x_battery_info *battery =
		container_of(work, struct s2mu00x_battery_info, monitor_work.work);
	union power_supply_propval value;

	pr_info("%s: start monitoring\n", __func__);

	/* Let charger know booting period to control additional charging */
	if (batt_booting_chk == 0) {
		battery->loop_cnt++;
		if (battery->loop_cnt > 6)  /* 6x10sec = 60sec */
			batt_booting_chk = 1;
	}

	/* Battery existence check */
	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_PRESENT, value);
	if (!value.intval) {
		battery->battery_valid = false;
		pr_info("%s: There is no battery, skip monitoring.\n", __func__);
		goto continue_monitor;
	} else
		battery->battery_valid = true;

	/* Update battery status information */
	get_battery_info(battery);

	/* Battery health check */
	if (!bat_health_check(battery))
		goto continue_monitor;

	/* Temperature check */
	if (!bat_temperature_check(battery))
		goto continue_monitor;

	/* Battery level check for full check or recharge */
	check_charging_full(battery);

	/* Battery status information logging */
	power_supply_changed(battery->psy_battery);

continue_monitor:
	pr_err(
		 "%s: Status(%s), charging(%s), Health(%s), Cable(%d), Recharging(%d))"
		 "\n", __func__,
		 bat_status_str[battery->status],
		 charging_status_str[battery->charging_status],
		 health_str[battery->health],
		 battery->cable_type,
		 battery->is_recharging
		 );

	set_monitor_polling(battery);
	wake_unlock(&battery->monitor_wake_lock);
}

#ifdef CONFIG_OF
static int s2mu00x_battery_parse_dt(struct device *dev,
		struct s2mu00x_battery_info *battery)
{
	struct device_node *np = of_find_node_by_name(NULL, "battery");
	s2mu00x_battery_platform_data_t *pdata = battery->pdata;
	int ret = 0, len;
	unsigned int i;
	const u32 *p;
	u32 temp;

	if (!np) {
		pr_err("%s np NULL(battery)\n", __func__);
		return -1;
	}
	ret = of_property_read_string(np,
			"battery,vendor", (char const **)&pdata->vendor);
	if (ret)
		pr_info("%s: Vendor is empty\n", __func__);

	ret = of_property_read_string(np,
			"battery,charger_name", (char const **)&pdata->charger_name);
	if (ret)
		pr_info("%s: Charger name is empty\n", __func__);

	ret = of_property_read_string(np,
			"battery,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
	if (ret)
		pr_info("%s: Fuelgauge name is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,technology",
			&pdata->technology);
	if (ret)
		pr_info("%s : technology is empty\n", __func__);


	p = of_get_property(np, "battery,input_current_limit", &len);
	if (!p)
		return 1;

	len = len / sizeof(u32);

	pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

	for (i = 0; i < len; i++) {
		ret = of_property_read_u32_index(np,
				"battery,input_current_limit", i,
				&pdata->charging_current[i].input_current_limit);
		if (ret)
			pr_info("%s : Input_current_limit is empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"battery,fast_charging_current", i,
				&pdata->charging_current[i].fast_charging_current);
		if (ret)
			pr_info("%s : Fast charging current is empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"battery,full_check_current", i,
				&pdata->charging_current[i].full_check_current);
		if (ret)
			pr_info("%s : Full check current is empty\n",
					__func__);

	}
	ret = of_property_read_u32(np, "battery,temp_check_count",
			&pdata->temp_check_count);
	if (ret)
		pr_info("%s : Temp check count is Empty\n", __func__);


	ret = of_property_read_u32(np, "battery,temp_highlimit_threshold_normal",
			&temp);
	pdata->temp_highlimit_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp highlimit threshold normal is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_highlimit_recovery_normal",
			&temp);
	pdata->temp_highlimit_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp highlimit recovery normal is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_high_threshold_normal",
			&temp);
	pdata->temp_high_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp high threshold normal is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_high_recovery_normal",
			&temp);
	pdata->temp_high_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp high recovery normal is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_low_threshold_normal",
			&temp);
	pdata->temp_low_threshold_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp low threshold normal is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,temp_low_recovery_normal",
			&temp);
	pdata->temp_low_recovery_normal =  (int)temp;
	if (ret)
		pr_info("%s : Temp low recovery normal is empty\n", __func__);

	pr_info("%s : HIGHLIMIT_THRESHOLD_NOLMAL(%d), HIGHLIMIT_RECOVERY_NORMAL(%d)\n"
			"HIGH_THRESHOLD_NORMAL(%d), HIGH_RECOVERY_NORMAL(%d)"
			"LOW_THRESHOLD_NORMAL(%d), LOW_RECOVERY_NORMAL(%d)\n",
			__func__,
			pdata->temp_highlimit_threshold_normal, pdata->temp_highlimit_recovery_normal,
			pdata->temp_high_threshold_normal, pdata->temp_high_recovery_normal,
			pdata->temp_low_threshold_normal, pdata->temp_low_recovery_normal);

	ret = of_property_read_u32(np, "battery,full_check_count",
			&pdata->full_check_count);
	if (ret)
		pr_info("%s : full_check_count is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_full_vcell",
			&pdata->chg_full_vcell);
	if (ret)
		pr_info("%s : chg_full_vcell is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_recharge_vcell",
			&pdata->chg_recharge_vcell);
	if (ret)
		pr_info("%s : chg_recharge_vcell is empty\n", __func__);

	pr_info("%s:DT parsing is done, vendor : %s, technology : %d\n",
			__func__, pdata->vendor, pdata->technology);
	return ret;
}
#else
static int s2mu00x_battery_parse_dt(struct device *dev,
		struct s2mu00x_battery_platform_data *pdata)
{
	return pdev->dev.platform_data;
}
#endif

static const struct of_device_id s2mu00x_battery_match_table[] = {
	{ .compatible = "samsung,s2mu00x-battery",},
	{},
};

static int s2mu00x_battery_probe(struct platform_device *pdev)
{
	struct s2mu00x_battery_info *battery;
	struct power_supply_config psy_cfg = {};
	union power_supply_propval value;
	int ret = 0;
#ifndef CONFIG_OF
	int i;
#endif

	pr_info("%s: S2MU00x battery driver loading\n", __func__);

	/* Allocate necessary device data structures */
	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;

	battery->pdata = devm_kzalloc(&pdev->dev, sizeof(*(battery->pdata)),
			GFP_KERNEL);
	if (!battery->pdata) {
		ret = -ENOMEM;
		goto err_bat_free;
	}

	/* Get device/board dependent configuration data from DT */
	if (s2mu00x_battery_parse_dt(&pdev->dev, battery)) {
		dev_err(&pdev->dev, "%s: Failed to get battery dt\n", __func__);
		ret = -EINVAL;
		goto err_parse_dt_nomem;
	}

	/* Set driver data */
	platform_set_drvdata(pdev, battery);
	battery->dev = &pdev->dev;

	mutex_init(&battery->iolock);

	wake_lock_init(&battery->monitor_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-monitor");
	wake_lock_init(&battery->vbus_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-vbus");

	/* Inintialization of battery information */
	battery->status = POWER_SUPPLY_STATUS_DISCHARGING;
	battery->health = POWER_SUPPLY_HEALTH_GOOD;
	battery->present = true;

	battery->input_current = 0;
	battery->charging_current = 0;
	battery->topoff_current = 0;

	battery->temp_highlimit_threshold =
		battery->pdata->temp_highlimit_threshold_normal;
	battery->temp_highlimit_recovery =
		battery->pdata->temp_highlimit_recovery_normal;
	battery->temp_high_threshold =
		battery->pdata->temp_high_threshold_normal;
	battery->temp_high_recovery =
		battery->pdata->temp_high_recovery_normal;
	battery->temp_low_recovery =
		battery->pdata->temp_low_recovery_normal;
	battery->temp_low_threshold =
		battery->pdata->temp_low_threshold_normal;

	battery->is_recharging = false;
	battery->cable_type = POWER_SUPPLY_TYPE_BATTERY;

	psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_PRESENT, value);
	if (!value.intval)
		battery->battery_valid = false;
	else
		battery->battery_valid = true;

	/* Register battery as "POWER_SUPPLY_TYPE_BATTERY" */
	battery->psy_battery_desc.name = "s2mu00x-battery";
	battery->psy_battery_desc.type = POWER_SUPPLY_TYPE_BATTERY;
	battery->psy_battery_desc.get_property =  s2mu00x_battery_get_property;
	battery->psy_battery_desc.properties = s2mu00x_battery_props;
	battery->psy_battery_desc.num_properties =  ARRAY_SIZE(s2mu00x_battery_props);

	battery->psy_usb_desc.name = "s2mu00x-usb";
	battery->psy_usb_desc.type = POWER_SUPPLY_TYPE_USB;
	battery->psy_usb_desc.get_property = s2mu00x_usb_get_property;
	battery->psy_usb_desc.properties = s2mu00x_power_props;
	battery->psy_usb_desc.num_properties = ARRAY_SIZE(s2mu00x_power_props);

	battery->psy_ac_desc.name = "s2mu00x-ac";
	battery->psy_ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	battery->psy_ac_desc.properties = s2mu00x_power_props;
	battery->psy_ac_desc.num_properties = ARRAY_SIZE(s2mu00x_power_props);
	battery->psy_ac_desc.get_property = s2mu00x_ac_get_property;

	/* Initialize work queue for periodic polling thread */
	battery->monitor_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!battery->monitor_wqueue) {
		dev_err(battery->dev,
				"%s: Fail to Create Workqueue\n", __func__);
		goto err_irr;
	}

	INIT_DELAYED_WORK(&battery->monitor_work, bat_monitor_work);
	INIT_DELAYED_WORK(&battery->polling_work, bat_polling_work);

	/* Register power supply to framework */
	psy_cfg.drv_data = battery;
	psy_cfg.supplied_to = s2mu00x_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mu00x_supplied_to);

	battery->psy_battery = power_supply_register(&pdev->dev, &battery->psy_battery_desc, &psy_cfg);
	if (IS_ERR(battery->psy_battery)) {
		pr_err("%s: Failed to Register psy_battery\n", __func__);
		ret = PTR_ERR(battery->psy_battery);
		goto err_workqueue;
	}
	pr_info("%s: Registered battery as power supply\n", __func__);

	battery->psy_usb = power_supply_register(&pdev->dev, &battery->psy_usb_desc, &psy_cfg);
	if (IS_ERR(battery->psy_usb)) {
		pr_err("%s: Failed to Register psy_usb\n", __func__);
		ret = PTR_ERR(battery->psy_usb);
		goto err_unreg_battery;
	}
	pr_info("%s: Registered USB as power supply\n", __func__);

	battery->psy_ac = power_supply_register(&pdev->dev, &battery->psy_ac_desc, &psy_cfg);
	if (IS_ERR(battery->psy_ac)) {
		pr_err("%s: Failed to Register psy_ac\n", __func__);
		ret = PTR_ERR(battery->psy_ac);
		goto err_unreg_usb;
	}
	pr_info("%s: Registered AC as power supply\n", __func__);

	/* To get SOC value (NOT raw SOC), need to reset value */
	value.intval = 0;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
	battery->capacity = value.intval;

#if defined(CONFIG_MUIC_NOTIFIER)
	pr_info("%s: Register MUIC notifier\n", __func__);
	muic_notifier_register(&battery->batt_nb, s2mu00x_battery_handle_notification,
			MUIC_NOTIFY_DEV_CHARGER);
#endif

	/* Kick off monitoring thread */
	pr_info("%s: start battery monitoring work\n", __func__);
	battery->loop_cnt = 0;
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 5*HZ);

	dev_info(battery->dev, "%s: Battery driver is loaded\n", __func__);
	return 0;

err_unreg_usb:
	power_supply_unregister(battery->psy_usb);
err_unreg_battery:
	power_supply_unregister(battery->psy_battery);
err_workqueue:
	destroy_workqueue(battery->monitor_wqueue);
err_irr:
	wake_lock_destroy(&battery->monitor_wake_lock);
	mutex_destroy(&battery->iolock);
err_parse_dt_nomem:
	kfree(battery->pdata);
err_bat_free:
	kfree(battery);

	return ret;
}

static int s2mu00x_battery_remove(struct platform_device *pdev)
{
	return 0;
}

#if defined CONFIG_PM
static int s2mu00x_battery_prepare(struct device *dev)
{
	struct s2mu00x_battery_info *battery = dev_get_drvdata(dev);

	dev_dbg(battery->dev, "%s: Stop battery monitoring\n", __func__);
	cancel_delayed_work(&battery->polling_work);
	return 0;
}

static int s2mu00x_battery_suspend(struct device *dev)
{
	return 0;
}

static int s2mu00x_battery_resume(struct device *dev)
{
	return 0;
}

static void s2mu00x_battery_complete(struct device *dev)
{
	struct s2mu00x_battery_info *battery = dev_get_drvdata(dev);

	dev_dbg(battery->dev, "%s: Restart battery monitoring\n", __func__);
	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
}

#else
#define s2mu00x_battery_prepare NULL
#define s2mu00x_battery_suspend NULL
#define s2mu00x_battery_resume NULL
#define s2mu00x_battery_complete NULL
#endif

static const struct dev_pm_ops s2mu00x_battery_pm_ops = {
	.prepare = s2mu00x_battery_prepare,
	.suspend = s2mu00x_battery_suspend,
	.resume = s2mu00x_battery_resume,
	.complete = s2mu00x_battery_complete,
};

static struct platform_driver s2mu00x_battery_driver = {
	.driver         = {
		.name   = "s2mu00x-battery",
		.owner  = THIS_MODULE,
		.pm     = &s2mu00x_battery_pm_ops,
		.of_match_table = s2mu00x_battery_match_table,
	},
	.probe          = s2mu00x_battery_probe,
	.remove     = s2mu00x_battery_remove,
};

static int __init s2mu00x_battery_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mu00x_battery_driver);
	return ret;
}
late_initcall(s2mu00x_battery_init);

static void __exit s2mu00x_battery_exit(void)
{
	platform_driver_unregister(&s2mu00x_battery_driver);
}
module_exit(s2mu00x_battery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Battery driver for S2MU00x");
