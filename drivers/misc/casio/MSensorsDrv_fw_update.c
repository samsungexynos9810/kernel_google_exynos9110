#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/reboot.h>
#include "MSensorsDrv.h"

#define MSENSORS_FW_UPDATE_FILE_NAME "msensors_fw_update"

static ssize_t msensors_fw_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct Msensors_state *st = get_msensors_state();

	return sprintf(buf, "%d\n", st->fw.status);
}

static struct device_attribute dev_attr_fw_status =
	__ATTR(msensors_fw_status, S_IRUSR,
			msensors_fw_status_show, NULL);

static void show_fw_status(struct Msensors_state *st)
{
	int rc;

	rc = device_create_file(&st->sdev->dev, &dev_attr_fw_status);
	if (rc) {
		dev_err(&st->sdev->dev, "%s: Error, could not create msensors_fw_status\n",
				__func__);
	}
}

static int msensor_do_update(struct Msensors_state *st, uint8_t *data, size_t size)
{
	uint16_t pktnum;
	unsigned char *recv_buf = &st->spi.send_buf[0];

	/* send packet num */
	pktnum = (size + 511) / 512;
	memset(&st->spi.send_buf[0], SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
	st->spi.send_buf[0] = SUB_COM_TYPE_SET_FW_SIZE;	/* 0xAF */
	st->spi.send_buf[1] = (uint8_t)(pktnum & 0xff);
	st->spi.send_buf[2] = (uint8_t)(pktnum >> 8);
	spi_send_wrapper_for_fwup(st->spi.send_buf, recv_buf, WRITE_DATA_SIZE);
	if (recv_buf[0] != SUB_COM_TYPE_FW_RECV_SIZE)
		return 1;

	while (!kthread_should_stop()) {
		st->spi.send_buf[0] = SUB_COM_TYPE_SET_FW_DATA;
		if (size < 512) {
			memcpy(&st->spi.send_buf[4], data, size);
			size = 0;
		} else {
			memcpy(&st->spi.send_buf[4], data, 512);
			size -= 512;
			data += 512;
		}
		spi_send_wrapper_for_fwup(st->spi.send_buf, recv_buf, 516);
		if (recv_buf[0] != SUB_COM_TYPE_FW_RECV_PKT)
			return 2;
		if (size == 0)
			break;
	}
	return 0;
}

void Msensors_set_fw_version(struct Msensors_state *st, uint8_t *data)
{
	st->fw.maj_ver  = data[0];
	st->fw.min_ver  = data[1];
	st->fw.revision = data[2];
	st->fw.subcpu_reset_cause = data[3];
	dev_info(&st->sdev->dev, "subcpu FW version:%02x:%02x:%02x\n",
		st->fw.maj_ver, st->fw.min_ver, st->fw.revision);

	st->fw.ver_loaded = 1;
	wake_up(&st->fw.wait);
}

static void get_msensors_version(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	write_buff[0] = SUB_COM_TYPE_READ;
	write_buff[1] = SUB_COM_GETID_SUB_CPU_VER;
	write_buff[2] = 0xff;
	Msensors_PushData(&write_buff[0]);
}

static int need_update(struct Msensors_state *st, uint8_t *data, size_t size)
{
	/*
	 * From subcpu-fw ver 15.03.00, endian of version number in
	 * sub-psoc.bin was changed from big to little. For compatibility,
	 * check both endian.
	 */
	if (((st->fw.maj_ver == data[4] && st->fw.revision == data[6]) ||
		(st->fw.maj_ver == data[6] && st->fw.revision == data[4])) &&
		st->fw.min_ver == data[5]) {
		dev_info(&st->sdev->dev, "subcpu FW is up-to-date\n");
		return 0;
	} else {
		dev_info(&st->sdev->dev, "subcpu FW version not match, start update\n");
		return 1;
	}
}

static void start_msensors_update(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_SUB_FIRM_UPDATE;	/* 0x99 */
	write_buff[2] = 0xff;
	Msensors_PushData(&write_buff[0]);
}

static void _msensors_firmware_cont(const struct firmware *fw, void *context)
{
	struct device *dev = context;
	struct Msensors_state *st = get_msensors_state();

	if (!fw)
		goto idle;

	if (!fw->data || !fw->size) {
		dev_err(dev, "%s: No firmware received\n", __func__);
		release_firmware(fw);
		goto idle;
	}

	if (st->fw.ver_loaded == 0)
		wait_event(st->fw.wait, st->fw.ver_loaded == 1);

	if (!(need_update(st, (uint8_t *)fw->data, fw->size))) {
		release_firmware(fw);
		goto idle;
	}

	start_msensors_update();
	wait_event(st->fw.wait, st->fw.status == MSENSORS_FW_UP_READY);
	st->fw.status = MSENSORS_FW_UP_UPDATING;
	msensor_do_update(st, (uint8_t *)fw->data, fw->size);
	release_firmware(fw);
	st->fw.status = MSENSORS_FW_UP_WAIT_RESET;
	wake_up(&st->fw.wait);
	kernel_power_off();
idle:
	st->fw.status = MSENSORS_FW_UP_IDLE;
	show_fw_status(st);
	SUBCPU_send_lowtemp_burnoff_enable();
}

static int update_firmware_from_class(struct device *dev)
{
	int retval;

	retval = request_firmware_nowait(THIS_MODULE, false,
			MSENSORS_FW_UPDATE_FILE_NAME, dev, GFP_KERNEL, dev,
			_msensors_firmware_cont);
	if (retval < 0) {
		dev_err(dev, "%s: Fail request firmware class file load\n",
			__func__);
		return retval;
	}

	return 0;
}

static ssize_t msensors_fw_update_store (struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct Msensors_state *st = get_msensors_state();
	int rc;

	if (st->fw.status != MSENSORS_FW_UP_IDLE)
		return -EBUSY;
	st->fw.status = MSENSORS_FW_UP_WAIT;

	rc = update_firmware_from_class(dev);

	if (rc < 0) {
		st->fw.status = MSENSORS_FW_UP_IDLE;
		show_fw_status(st);
	}

	return size;
}

static struct device_attribute dev_attr_fw_update =
	__ATTR(msensors_fw, S_IWUSR,
			NULL, msensors_fw_update_store);

void msensors_fw_up_init(struct Msensors_state *st)
{
	int rc;

	init_waitqueue_head(&st->fw.wait);
	st->fw.status = MSENSORS_FW_UP_IDLE;
	st->fw.ver_loaded = 0;
	get_msensors_version();

	rc = device_create_file(&st->sdev->dev, &dev_attr_fw_update);
	if (rc) {
		dev_err(&st->sdev->dev, "%s: Error, could not create msensors_fw\n",
				__func__);
	}
}

MODULE_AUTHOR("CASIO");
MODULE_DESCRIPTION("MultiSensors FW update");
MODULE_LICENSE("GPL v2");
