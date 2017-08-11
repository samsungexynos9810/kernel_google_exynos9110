/* drivers/battery/s2mpw01_charger.c
 * S2MPW01 Charger Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/wakelock.h>
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/mfd/samsung/s2mpw01-private.h>
#include <linux/power/s2mpw01_charger.h>
#include <linux/version.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>

#define ENABLE_MIVR 1
#define MINVAL(a, b) ((a <= b) ? a : b)

#define EOC_DEBOUNCE_CNT 2
#define HEALTH_DEBOUNCE_CNT 3
#define DEFAULT_CHARGING_CURRENT 500

#define EOC_SLEEP 200
#define EOC_TIMEOUT (EOC_SLEEP * 6)
#ifndef EN_TEST_READ
#define EN_TEST_READ 1
#endif

struct s2mpw01_charger_data {
	struct s2mpw01_dev	*iodev;
	struct i2c_client       *client;
	struct device *dev;
	struct s2mpw01_platform_data *s2mpw01_pdata;
	struct delayed_work init_work;
	struct power_supply	*psy_chg;
	struct power_supply_desc psy_chg_desc;
	s2mpw01_charger_platform_data_t *pdata;
	int dev_id;
	int charging_current;
	int cable_type;
	bool is_charging;
	bool is_usb_ready;
	struct mutex io_lock;

	/* register programming */
	int reg_addr;
	int reg_data;

	bool full_charged;
	bool ovp;
	bool factory_mode;

	int unhealth_cnt;
	int status;
	int onoff;

	/* charger enable, disable data */
	u8 chg_en_data;

	/* s2mpw01 */
	int irq_det_bat;
	int irq_chg;
	int irq_tmrout;
};

static enum power_supply_property s2mpw01_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_USB_OTG,
};

extern unsigned int batt_booting_chk;

static int s2mpw01_get_charging_health(struct s2mpw01_charger_data *charger);

static char *s2mpw01_supplied_to[] = {
	"s2mu00x-battery",
};

static void s2mpw01_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x0; i <= 0x17; i++) {
		s2mpw01_read_reg(i2c, i, &data);

		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	pr_err("[DEBUG]%s: %s\n", __func__, str);
}

static void s2mpw01_enable_charger_switch(struct s2mpw01_charger_data *charger,
		int onoff)
{
	unsigned int data = 0;
	u8 acok_stat = 0;

	data = charger->chg_en_data;

	/* ACOK status */
	if (charger->dev_id < EVT_3) {
		s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &acok_stat);
		pr_err("[DEBUG]%s: onoff[%d], chg_en_data[0x%x], acok[0x%x]\n",
			__func__, onoff, charger->chg_en_data, acok_stat);
	}

	if (onoff > 0) {
		pr_err("[DEBUG]%s: turn on charger\n", __func__);
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);
	} else {
		charger->full_charged = false;
		pr_err("[DEBUG] %s: turn off charger\n", __func__);

		if (charger->dev_id < EVT_3) {
			if (!charger->factory_mode) {
				data |= 0x40;
				s2mpw01_write_reg(charger->client, 0x2E, data);
				s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, 0, EN_CHG_MASK);
			}
		} else {
			s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, 0, EN_CHG_MASK);
		}

		/* ACOK status : high > keep charger off, low > charger on */
		if (!(acok_stat & ACOK_STATUS_MASK) && (charger->dev_id < EVT_3)) {
			data = charger->chg_en_data;
			s2mpw01_write_reg(charger->client, 0x2E, data);
			s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);
			pr_err("[DEBUG] %s: turn on charger for RID detection\n", __func__);
		}
	}
}

static void s2mpw01_topoff_interrupt_onoff(struct s2mpw01_charger_data *charger, int onoff)
{

	if (onoff > 0) {
		/* Use top-off interrupt. Masking off */
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_INT1M, 0x00, 0x04);
		pr_err("[DEBUG]%s: Use top-off interrupt: 0x%x, 0x%x\n", __func__,
			charger->iodev->irq_masks_cur[3], charger->iodev->irq_masks_cache[3]);
		charger->iodev->irq_masks_cur[3] &= ~0x04;
		charger->iodev->irq_masks_cache[3] &= ~0x04;
		charger->iodev->topoff_mask_status = 1;
		s2mpw01_enable_charger_switch(charger, 0);
		msleep(100);
		s2mpw01_enable_charger_switch(charger, 1);

	} else {
		/* Not use top-off interrupt. Masking */
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_INT1M, 0x04, 0x04);
		pr_err("[DEBUG]%s: Top-off interrupt Masking: 0x%x, 0x%x\n", __func__,
			charger->iodev->irq_masks_cur[3], charger->iodev->irq_masks_cache[3]);
		charger->iodev->irq_masks_cur[3] |= 0x04;
		charger->iodev->irq_masks_cache[3] |= 0x04;
		charger->iodev->topoff_mask_status = 0;
	}

}

static void s2mpw01_set_regulation_voltage(struct s2mpw01_charger_data *charger,
		int float_voltage)
{
	unsigned int data;

	pr_err("[DEBUG]%s: float_voltage %d\n", __func__, float_voltage);

	if (float_voltage <= 4200)
		data = 0;
	else if (float_voltage > 4200 && float_voltage <= 4550)
		data = (float_voltage - 4200) / 50;
	else
		data = 0x7;

	s2mpw01_update_reg(charger->client,
			S2MPW01_CHG_REG_CTRL5, data << SET_VF_VBAT_SHIFT, SET_VF_VBAT_MASK);
}

static void s2mpw01_set_fast_charging_current(struct i2c_client *i2c,
		int charging_current)
{
	int data;

	pr_err("[DEBUG]%s: fast charge current  %d\n", __func__, charging_current);

	if (charging_current <= 75)
		data = 0x6;
	else if (charging_current <= 150)
		data = 0;
	else if (charging_current <= 175)
		data = 0x7;
	else if (charging_current > 175 && charging_current <= 400)
		data = (charging_current - 150) / 50;
	else
		data = 0x5;

	s2mpw01_update_reg(i2c, S2MPW01_CHG_REG_CTRL2, data << FAST_CHARGING_CURRENT_SHIFT,
			FAST_CHARGING_CURRENT_MASK);
	s2mpw01_test_read(i2c);
}

static int s2mpw01_get_fast_charging_current(struct i2c_client *i2c)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(i2c, S2MPW01_CHG_REG_CTRL2, &data);
	if (ret < 0)
		return ret;

	data = data & FAST_CHARGING_CURRENT_MASK;

	if (data > 0x5)
		data = 0x5;
	return data * 50 + 150;
}

int eoc_current[16] = {
	5, 10, 12, 15, 20, 17, 25, 30, 35, 40, 50, 60, 70, 80, 90, 100,};

static int s2mpw01_get_current_eoc_setting(struct s2mpw01_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_CTRL4, &data);
	if (ret < 0)
		return ret;

	data = (data & FIRST_TOPOFF_CURRENT_MASK) >> 4;

	if (data > 0x0f)
		data = 0x0f;

	pr_err("[DEBUG]%s: data(0x%x), top-off current	%d\n", __func__, data, eoc_current[data]);

	return eoc_current[data];
}

static void s2mpw01_set_topoff_current(struct i2c_client *i2c, int current_limit)
{
	int data;

	if (current_limit <= 5)
		data = 0;
	else if (current_limit > 5 && current_limit <= 10)
		data = (current_limit - 5) / 5;
	else if (current_limit > 10 && current_limit < 18)
		data = (current_limit - 10) / 5 * 2 + 1;
	else if (current_limit >= 18 && current_limit < 20)
		data = 5;	  /* 17.5 mA */
	else if (current_limit >= 20 && current_limit < 25)
		data = 4;
	else if (current_limit >= 25 && current_limit <= 40)
		data = (current_limit - 25) / 5 + 6;
	else if (current_limit > 40 && current_limit <= 100)
		data = (current_limit - 40) / 10 + 9;
	else
		data = 0x0F;

	pr_err("[DEBUG]%s: top-off current	%d, data=0x%x\n", __func__, current_limit, data);

	s2mpw01_update_reg(i2c, S2MPW01_CHG_REG_CTRL4, data << FIRST_TOPOFF_CURRENT_SHIFT,
			FIRST_TOPOFF_CURRENT_MASK);
}

static void s2mpw01_set_additional(struct s2mpw01_charger_data *charger, int n, int onoff)
{
	int val = 0xff;

	pr_err("[DEBUG]%s: n (%d), onoff (%d)\n", __func__, n, onoff);

	if (onoff == 1) {

		switch (n) {
		case 1:
			val = 0x1;
			break;
		case 2:
			val = 0x3;
			break;
		case 3:
			val = 0x7;
			break;
		case 4:
			val = 0xf;
			break;
		case 5:
			val = 0x1f;
			break;
		case 6:
			val = 0x3f;
			break;
		case 7:
			val = 0x7f;
			break;
		case 8:
			val = 0xff;
			break;
		}
		/* Apply additional charging current */
		s2mpw01_update_reg(charger->client,
				S2MPW01_CHG_REG_CTRL7, val << SET_ADD_PATH_SHIFT, SET_ADD_PATH_MASK);

		/* Additional charging path On */
		s2mpw01_update_reg(charger->client,
				S2MPW01_CHG_REG_CTRL5, 1 << SET_ADD_ON_SHIFT, SET_ADD_ON_MASK);
	} else if (onoff == 0) {
		/* Additional charging path Off */
		s2mpw01_update_reg(charger->client,
				S2MPW01_CHG_REG_CTRL5, 0 << SET_ADD_ON_SHIFT, SET_ADD_ON_MASK);

		/* Restore addition charging current */
		s2mpw01_update_reg(charger->client,
		S2MPW01_CHG_REG_CTRL7, val << SET_ADD_PATH_SHIFT, SET_ADD_PATH_MASK);
	}
}


/* eoc reset */
static void s2mpw01_set_charging_current(struct s2mpw01_charger_data *charger)
{
	s2mpw01_set_fast_charging_current(charger->client, charger->charging_current);

	if (batt_booting_chk == 0)
		s2mpw01_set_additional(charger, 7, 1);
	else
		s2mpw01_set_additional(charger, 0, 0);
}

enum {
	S2MPW01_MIVR_4200MV = 0,
	S2MPW01_MIVR_4300MV,
	S2MPW01_MIVR_4400MV,
	S2MPW01_MIVR_4500MV,
	S2MPW01_MIVR_4600MV,
	S2MPW01_MIVR_4700MV,
	S2MPW01_MIVR_4800MV,
	S2MPW01_MIVR_4900MV,
};

#if ENABLE_MIVR
/* charger input regulation voltage setting */
static void s2mpw01_set_mivr_level(struct s2mpw01_charger_data *charger)
{
	int mivr = S2MPW01_MIVR_4600MV;

	s2mpw01_update_reg(charger->client,
			S2MPW01_CHG_REG_CTRL4, mivr << SET_VIN_DROP_SHIFT, SET_VIN_DROP_MASK);
}
#endif /*ENABLE_MIVR*/

static void s2mpw01_configure_charger(struct s2mpw01_charger_data *charger)
{
	struct device *dev = charger->dev;
	int eoc = 0;
	union power_supply_propval chg_mode;

	dev_err(dev, "%s() set configure charger\n", __func__);

	if (charger->charging_current < 0) {
		dev_info(dev, "%s() OTG is activated. Ignore command!\n",
				__func__);
		return;
	}

	if (!charger->pdata->charging_current_table) {
		dev_err(dev, "%s() table is not exist\n", __func__);
		return;
	}

#if ENABLE_MIVR
	s2mpw01_set_mivr_level(charger);
#endif /*DISABLE_MIVR*/

	/* msleep(200); */

	s2mpw01_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);

	charger->charging_current = charger->pdata->charging_current_table
		[charger->cable_type].fast_charging_current;

	dev_err(dev, "%s() fast charging current (%dmA)\n",
			__func__, charger->charging_current);

	s2mpw01_set_charging_current(charger);

	if (charger->pdata->full_check_type == SEC_BATTERY_FULLCHARGED_CHGPSY) {
		if (charger->pdata->full_check_type_2nd == SEC_BATTERY_FULLCHARGED_CHGPSY) {
				psy_do_property("s2mu00x-battery", get,
						POWER_SUPPLY_PROP_CHARGE_NOW,
						chg_mode);

				if (chg_mode.intval == SEC_BATTERY_CHARGING_2ND) {
					/* s2mpw01_enable_charger_switch(charger, 0); */
					charger->full_charged = false;
					eoc = charger->pdata->charging_current_table
						[charger->cable_type].full_check_current_2nd;
				} else {
					eoc = charger->pdata->charging_current_table
						[charger->cable_type].full_check_current_1st;
				}
			} else {
				eoc = charger->pdata->charging_current_table
					[charger->cable_type].full_check_current_1st;
			}
		s2mpw01_set_topoff_current(charger->client, eoc);
	}
	s2mpw01_enable_charger_switch(charger, 1);
}

/* here is set init charger data */
static bool s2mpw01_chg_init(struct s2mpw01_charger_data *charger)
{
	dev_info(&charger->client->dev, "%s : DEV ID : 0x%x\n", __func__,
			charger->dev_id);
	/* Buck switching mode frequency setting */

	/* Disable Timer function (Charging timeout fault) */
	/* to be */

	/* change Top-off detection debounce time (0x56 to 0x76) */
	s2mpw01_write_reg(charger->client, 0x2C, 0x76);

#if !(ENABLE_MIVR)
	/* voltage regulatio disable does not exist mu005 */
#endif
	/* Top-off Timer Disable */
	s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL8, 0x04, 0x04);

	/* Factory_mode initialization */
	charger->factory_mode = false;

	return true;
}

static int s2mpw01_get_charging_status(struct s2mpw01_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	u8 chg_sts;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &chg_sts);
	if (ret < 0)
		return status;
	dev_info(charger->dev, "%s : charger status : 0x%x\n", __func__, chg_sts);

	if (charger->full_charged) {
			dev_info(charger->dev, "%s : POWER_SUPPLY_STATUS_FULL : 0x%x\n", __func__, chg_sts);
			return POWER_SUPPLY_STATUS_FULL;
	}

	switch (chg_sts & 0x12) {
	case 0x00:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 0x10:	/*charge state */
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x12:	/* Input is invalid */
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	default:
		break;
	}

	s2mpw01_test_read(charger->client);
	return status;
}

static int s2mpw01_get_charge_type(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(iic, S2MPW01_CHG_REG_STATUS1, &data);
	if (ret < 0) {
		dev_err(&iic->dev, "%s fail\n", __func__);
		return ret;
	}

	switch (data & (1 << CHG_STATUS1_CHG_STS)) {
	case 0x10:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}

	return status;
}

static bool s2mpw01_get_batt_present(struct i2c_client *iic)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(iic, S2MPW01_CHG_REG_STATUS2, &data);
	if (ret < 0)
		return false;

	return (data & DET_BAT_STATUS_MASK) ? true : false;
}

static int s2mpw01_get_charging_health(struct s2mpw01_charger_data *charger)
{
	int ret;
	u8 data, data1;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &data);
	s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &data1);

	pr_info("[%s] chg_status1: 0x%x, pm_status1: 0x%x\n ", __func__, data, data1);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (data & (1 << CHG_STATUS1_CHGVIN)) {
		charger->ovp = false;
		charger->unhealth_cnt = 0;
		pr_info("[%s] POWER_SUPPLY_HEALTH_GOOD\n ", __func__);
		return POWER_SUPPLY_HEALTH_GOOD;
	}

	if ((data1 & ACOK_STATUS_MASK) && (data & CHGVINOVP_STATUS_MASK) &&
		(data & CIN2BAT_STATUS_MASK)) {
		pr_info("[%s] POWER_SUPPLY_HEALTH_OVERVOLTAGE, unhealth_cnt %d\n ",
			__func__, charger->unhealth_cnt);
		if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT)
			return POWER_SUPPLY_HEALTH_GOOD;
		charger->unhealth_cnt++;
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int s2mpw01_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	/* int chg_curr, aicr; */
	struct s2mpw01_charger_data *charger =
		power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mpw01_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mpw01_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 2000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current)
			val->intval = s2mpw01_get_fast_charging_current(charger->client);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mpw01_get_charge_type(charger->client);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = charger->pdata->chg_float_voltage;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mpw01_get_batt_present(charger->client);
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_USB_OTG:
		val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mpw01_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mpw01_charger_data *charger =
		power_supply_get_drvdata(psy);

	struct device *dev = charger->dev;
	int eoc;
/*	int previous_cable_type = charger->cable_type; */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
				charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			dev_info(dev, "%s() [BATT] Type Battery\n", __func__);
			if (!charger->pdata->charging_current_table)
				return -EINVAL;

			charger->charging_current = charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].fast_charging_current;

			s2mpw01_set_charging_current(charger);
			s2mpw01_set_topoff_current(charger->client,
					charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].full_check_current_1st);
			charger->is_charging = false;
			charger->full_charged = false;
			s2mpw01_enable_charger_switch(charger, 0);
			s2mpw01_topoff_interrupt_onoff(charger, 0);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			dev_info(dev, "%s() OTG mode not supported\n", __func__);
		} else {
			dev_info(dev, "%s()  Set charging, Cable type = %d\n",
				 __func__, charger->cable_type);
			charger->is_charging = true;
			/* Enable charger */
			s2mpw01_configure_charger(charger);
		}
#if EN_TEST_READ
		msleep(100);
		s2mpw01_test_read(charger->client);
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* set charging current */
		pr_info("[DEBUG] %s: is_charging %d\n", __func__, charger->is_charging);
		if (charger->is_charging) {
			charger->charging_current = val->intval;
			s2mpw01_set_charging_current(charger);
		}
#if EN_TEST_READ
		s2mpw01_test_read(charger->client);
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		dev_info(dev, "%s() set current[%d]\n", __func__, val->intval);
		charger->charging_current = val->intval;
		s2mpw01_set_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		dev_info(dev, "%s() float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mpw01_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		eoc = s2mpw01_get_current_eoc_setting(charger);
		dev_info(dev, "%s() Set Power Now -> chg current = %d mA, eoc = %d mA\n",
				__func__, val->intval, eoc);
		s2mpw01_set_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_USB_OTG:
		dev_err(dev, "%s() OTG mode not supported\n", __func__);
		/* s2mpw01_charger_otg_control(charger, val->intval); */
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		dev_info(dev, "%s() CHARGING_ENABLE\n", __func__);
		/* charger->is_charging = val->intval; */
		s2mpw01_enable_charger_switch(charger, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mpw01_factory_mode_setting(struct s2mpw01_charger_data *charger)
{
	unsigned int stat_val;

	/* ACOK status */
	if (charger->dev_id < EVT_3) {
		stat_val = charger->chg_en_data;
		s2mpw01_write_reg(charger->client, 0x2E, stat_val);
		s2mpw01_enable_charger_switch(charger, 1);
	}

	charger->factory_mode = true;
	pr_err("%s, factory mode\n", __func__);
}

static irqreturn_t s2mpw01_chg_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	u8 val, valm;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &val);
	pr_info("[DEBUG]%s , %02x\n", __func__, val);
	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_INT1M, &valm);
	pr_info("%s : CHG_INT1 ---> 0x%x\n", __func__, valm);

	if ((val & TOP_OFFSTATUS_MASK) && (val & CHGSTS_STATUS_MASK)) {
		pr_info("%s : top_off status~!\n", __func__);
		charger->full_charged = true;
		/* TOP-OFF interrupt masking */
		s2mpw01_topoff_interrupt_onoff(charger, 0);
	}

	return IRQ_HANDLED;
}

static irqreturn_t s2mpw01_tmrout_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	u8 val;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &val);
	if (val & 0x10) {
		/* Timer out status */
		pr_err("%s, fast-charging timeout, timer clear\n", __func__);
		s2mpw01_enable_charger_switch(charger, 0);
		msleep(100);
		s2mpw01_enable_charger_switch(charger, 1);
	}
	return IRQ_HANDLED;
}


#ifdef CONFIG_OF
static int s2mpw01_charger_parse_dt(struct device *dev,
		struct s2mpw01_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mpw01-charger");
	const u32 *p;
	int ret, i, len;

	/* SC_CTRL8 , SET_VF_VBAT , Battery regulation voltage setting */
	ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
		"battery,charger_name", (char const **)&pdata->charger_name);

		ret = of_property_read_u32(np, "battery,full_check_type",
				&pdata->full_check_type);

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
				&pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n",
						__func__);

		pdata->chg_eoc_dualpath = of_property_read_bool(np,
				"battery,chg_eoc_dualpath");

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->charging_current_table =
				kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				"battery,input_current_limit", i,
				&pdata->charging_current_table[i].input_current_limit);
			ret = of_property_read_u32_index(np,
				"battery,fast_charging_current", i,
				&pdata->charging_current_table[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
				"battery,full_check_current_1st", i,
				&pdata->charging_current_table[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
				"battery,full_check_current_2nd", i,
				&pdata->charging_current_table[i].full_check_current_2nd);
		}
	}
	dev_info(dev, "s2mpw01 charger parse dt retval = %d\n", ret);
	return ret;
}
#else
static int s2mpw01_charger_parse_dt(struct device *dev,
		struct s2mpw01_charger_platform_data *pdata)
{
	return -ENOSYS;
}
#endif
/* if need to set s2mpw01 pdata */
static struct of_device_id s2mpw01_charger_match_table[] = {
	{ .compatible = "samsung,s2mpw01-charger",},
	{},
};

static int s2mpw01_charger_probe(struct platform_device *pdev)
{
	struct s2mpw01_dev *s2mpw01 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw01_platform_data *pdata = dev_get_platdata(s2mpw01->dev);
	struct s2mpw01_charger_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;
	u8 acok_stat = 0, data = 0;
	u8 chg_sts2, chg_sts3;

	pr_info("%s:[BATT] S2MPW01 Charger driver probe\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->io_lock);

	charger->iodev = s2mpw01;
	charger->dev = &pdev->dev;
	charger->client = s2mpw01->charger;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mpw01_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "s2mpw01-charger";

	charger->psy_chg_desc.name           = charger->pdata->charger_name;
	charger->psy_chg_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg_desc.get_property   = s2mpw01_chg_get_property;
	charger->psy_chg_desc.set_property   = s2mpw01_chg_set_property;
	charger->psy_chg_desc.properties     = s2mpw01_charger_props;
	charger->psy_chg_desc.num_properties = ARRAY_SIZE(s2mpw01_charger_props);

	charger->dev_id = s2mpw01->pmic_rev;
	charger->onoff = 0;

	s2mpw01_chg_init(charger);

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = s2mpw01_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mpw01_supplied_to);

	charger->psy_chg = power_supply_register(&pdev->dev, &charger->psy_chg_desc, &psy_cfg);
	if (IS_ERR(charger->psy_chg)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(charger->psy_chg);
		goto err_power_supply_register;
	}

	charger->irq_chg = pdata->irq_base + S2MPW01_CHG_IRQ_TOPOFF_INT1;
	ret = request_threaded_irq(charger->irq_chg, NULL,
			s2mpw01_chg_isr, 0, "chg-irq", charger);
	if (ret < 0) {
		dev_err(s2mpw01->dev, "%s: Fail to request charger irq in IRQ: %d: %d\n",
					__func__, charger->irq_chg, ret);
		goto err_power_supply_register;
	}

	charger->irq_tmrout = charger->iodev->irq_base + S2MPW01_CHG_IRQ_TMROUT_INT3;
	ret = request_threaded_irq(charger->irq_tmrout, NULL,
			s2mpw01_tmrout_isr, 0, "tmrout-irq", charger);
	if (ret < 0) {
		dev_err(s2mpw01->dev, "%s: Fail to request charger irq in IRQ: %d: %d\n",
					__func__, charger->irq_tmrout, ret);
		goto err_power_supply_register;
	}

	s2mpw01_test_read(charger->client);

	/* initially TOP-OFF interrupt masking */
	s2mpw01_topoff_interrupt_onoff(charger, 0);

	ret = s2mpw01_read_reg(charger->iodev->pmic, 0x41, &charger->chg_en_data);
	if (ret < 0) {
		pr_err("%s: failed to read PM addr 0x41(%d)\n", __func__, ret);
		goto err_power_supply_register;
	}

	/* factory_mode setting */
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &chg_sts2);
	if (ret < 0)
		pr_err("{DEBUG] %s : chg status2 read fail\n", __func__);

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &chg_sts3);
	if (ret < 0)
		pr_err("{DEBUG] %s : chg status3 read fail\n", __func__);

	if ((chg_sts2 & USB_BOOT_ON_STATUS_MASK) || (chg_sts2 & USB_BOOT_OFF_STATUS_MASK) ||
		(chg_sts3 & UART_BOOT_ON_STATUS_MASK) || (chg_sts2 & UART_BOOT_OFF_STATUS_MASK))
		s2mpw01_factory_mode_setting(charger);

	/* make bit[6] to 0 (if 0x41 is 0, old rev.) */
	if (charger->dev_id < EVT_3) {
		if (charger->chg_en_data == 0)
			charger->chg_en_data = 0x21;
		else
			charger->chg_en_data &= 0xBF;
	}

	ret = s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &acok_stat);
	if (ret < 0) {
		pr_err("%s: failed to read S2MPW01_PMIC_REG_STATUS1(%d)\n", __func__, ret);
		goto err_power_supply_register;
	}

	data = charger->chg_en_data;
	/* if acok is high, set 1 to bit[6]. if acok is low, set 0 to bit[6] */
	if ((!charger->factory_mode) && (charger->dev_id < EVT_3)) {
		if (acok_stat & ACOK_STATUS_MASK)
			data |= 0x40;
		s2mpw01_write_reg(charger->client, 0x2E, data);
	}
	pr_info("%s:[BATT] S2MPW01 charger driver loaded OK\n", __func__);

	return 0;

err_power_supply_register:
	power_supply_unregister(charger->psy_chg);
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return ret;
}

static int s2mpw01_charger_remove(struct platform_device *pdev)
{
	struct s2mpw01_charger_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mpw01_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mpw01_charger_resume(struct device *dev)
{
	struct s2mpw01_charger_data *charger = dev_get_drvdata(dev);
	u8 val;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_INT1M, &val);
	pr_info("%s : CHG_INT1 ---> 0x%x\n", __func__, val);
	pr_err("[DEBUG]%s: Top-off interrupt Masking : 0x%x, topoff status %d\n", __func__,
		charger->iodev->irq_masks_cur[3], charger->iodev->topoff_mask_status);

	return 0;
}
#else
#define s2mpw01_charger_suspend NULL
#define s2mpw01_charger_resume NULL
#endif

static void s2mpw01_charger_shutdown(struct device *dev)
{
	struct s2mpw01_charger_data *charger = dev_get_drvdata(dev);
	unsigned int stat_val = 0;

	/* ACOK status */
	if (charger->dev_id < EVT_3) {
		stat_val = charger->chg_en_data;
		s2mpw01_write_reg(charger->client, 0x2E, stat_val);
	}
	s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);
	s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL2, 0x1 << FAST_CHARGING_CURRENT_SHIFT,
			FAST_CHARGING_CURRENT_MASK);

	s2mpw01_set_additional(charger, 0, 0);

	pr_info("%s: S2MPW01 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mpw01_charger_pm_ops, s2mpw01_charger_suspend,
		s2mpw01_charger_resume);

static struct platform_driver s2mpw01_charger_driver = {
	.driver         = {
		.name   = "s2mpw01-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mpw01_charger_match_table,
		.pm     = &s2mpw01_charger_pm_ops,
		.shutdown = s2mpw01_charger_shutdown,
	},
	.probe          = s2mpw01_charger_probe,
	.remove		= s2mpw01_charger_remove,
};

static int __init s2mpw01_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mpw01_charger_driver);

	return ret;
}
device_initcall(s2mpw01_charger_init);

static void __exit s2mpw01_charger_exit(void)
{
	platform_driver_unregister(&s2mpw01_charger_driver);
}
module_exit(s2mpw01_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MPW01");
