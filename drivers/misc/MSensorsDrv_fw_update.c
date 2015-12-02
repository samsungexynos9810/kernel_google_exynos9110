#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
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

/* get the number of rows of the data */
static int get_total_line(uint8_t *data, size_t size)
{
	int i, count=0;

	for (i=0; i<size; i++)
		if (data[i] == '\n')
			count++;

	return count;
}

/* get one line from *data
 * return a size of the line
 * set the index to the next line
 */
static int get_one_line(uint8_t *buf, uint8_t *data, size_t size, int *ind)
{
	int i = 0;

	while(!(data[*ind] == '\r' || data[*ind] == '\n')) {
		buf[i] = data[*ind];
		*ind = *ind+1;
		i++;
		if (*ind >= size)
			return -1;
	}

	while(data[*ind] == '\r' || data[*ind] == '\n') {
		*ind = *ind+1;
		if (*ind >= size)
			break;
	}

	return i;
}

static int msensor_do_update(struct Msensors_state *st, uint8_t *data, size_t size)
{
	int line_len, line_now, line_total;
	uint8_t buf[256];
	int ind = 0;

	line_total = get_total_line(data, size);
	line_now = 1;
	line_len = get_one_line(buf, data, size, &ind);

	while (!kthread_should_stop()) {
		switch (st->spi.recv_buf[SUB_COM_HEAD_INDEX_TYPE]) {
		case SUB_COM_TYPE_FWUP_READY:
		case SUB_COM_TYPE_FW_REQ_HEAD:
			if (line_len < 0) {
			dev_err(&st->sdev->dev,
					"FWUp Failed incorrect file format (%04d/%04d)line \n",
					line_now, line_total);
			goto update_finish;
			}
			memset(&st->spi.send_buf[0], SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
			st->spi.send_buf[0] = SUB_COM_TYPE_SET_FW_SIZE;
			st->spi.send_buf[1] = (uint8_t)(line_len & 0x00FF);
			st->spi.send_buf[2] = (uint8_t)((line_len & 0xFF00) >> 8);
			st->spi.send_buf[3] = (uint8_t)(line_now & 0x00FF);
			st->spi.send_buf[4] = (uint8_t)((line_now & 0xFF00) >> 8);
			st->spi.send_buf[5] = (uint8_t)(line_total & 0x00FF);
			st->spi.send_buf[6] = (uint8_t)((line_total & 0xFF00) >> 8);
			st->spi.pre_send_type =  st->spi.send_buf[0];
			Msensors_Spi_Send(st, st->spi.send_buf, st->spi.recv_buf, WRITE_DATA_SIZE);
			break;
		case SUB_COM_TYPE_FW_REQ_DATA:
			memset(&st->spi.send_buf[0], SUB_COM_SEND_DUMMY, 1 + line_len);
			st->spi.send_buf[0] = SUB_COM_TYPE_SET_FW_DATA;
			memcpy(&st->spi.send_buf[1], buf, line_len);
			st->spi.pre_send_type = st->spi.send_buf[0];
			Msensors_Spi_Send(st, st->spi.send_buf, st->spi.recv_buf, 1 + line_len);
			if (line_now == line_total) {
				dev_info(&st->sdev->dev, "subcpu FW update Complete!\n");
				goto update_finish;
			}
			line_now++;
			line_len = get_one_line(buf, data, size, &ind);
			break;
		default:
			dev_err(&st->sdev->dev,
					"FWUp RecvDeta Error (%04d/%04d)line \n", line_now, line_total);
			goto update_finish;
		}
	}
update_finish:
	return 0;
}

void Msensors_set_fw_version(struct Msensors_state *st, uint8_t *data)
{
	st->fw.maj_ver  = data[0];
	st->fw.min_ver  = data[1];
	st->fw.revision = data[2];
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
	Set_WriteDataBuff(&write_buff[0]);
}

static uint8_t atoi(uint8_t num)
{
	uint8_t ret = 0;

	if (num >= '0' && num <= '9')
		ret = num - '0';
	else if (num >= 'a' && num <= 'f')
		ret = num - 'a' + 10;
	else if (num >= 'A' && num <= 'F')
		ret = num - 'A' + 10;
	else
		printk("[msensors_fw_up,atoi] unexpected value\n");

	return ret;
}

static int need_update(struct Msensors_state *st, uint8_t *data, size_t size)
{
	uint8_t buf[256], ver[6];
	int ind = 0;
	uint8_t maj, min, rev;

	/* version info is embedded in 26th of the 2nd line */
	get_one_line(buf, data, size, &ind);
	get_one_line(buf, data, size, &ind);
	memcpy(ver, &buf[26], 6);
	maj = atoi(ver[0]) << 4 | atoi(ver[1]);
	min = atoi(ver[2]) << 4 | atoi(ver[3]);
	rev = atoi(ver[4]) << 4 | atoi(ver[5]);

	if (st->fw.maj_ver == maj && st->fw.min_ver == min
			&& st->fw.revision == rev) {
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
	write_buff[1] = SUB_COM_SETID_SUB_FIRM_UPDATE;
	Set_WriteDataBuff(&write_buff[0]);
}

static void _msensors_firmware_cont(const struct firmware *fw, void *context)
{
	struct device *dev = context;
	struct Msensors_state *st = get_msensors_state();

	if (!fw) {
		st->fw.status = MSENSORS_FW_UP_IDLE;
		show_fw_status(st);
		return;
	}

	if (!fw->data || !fw->size) {
		dev_err(dev, "%s: No firmware received\n", __func__);
		release_firmware(fw);
		st->fw.status = MSENSORS_FW_UP_IDLE;
		show_fw_status(st);
		return;
	}

	if (st->fw.ver_loaded == 0)
		wait_event(st->fw.wait, st->fw.ver_loaded == 1);

	if (!(need_update(st, (uint8_t *)fw->data, fw->size))) {
		release_firmware(fw);
		st->fw.status = MSENSORS_FW_UP_IDLE;
		show_fw_status(st);
		return;
	}

	start_msensors_update();
	wait_event(st->fw.wait, st->fw.status == MSENSORS_FW_UP_READY);
	st->fw.status = MSENSORS_FW_UP_UPDATING;
	msensor_do_update(st, (uint8_t *)fw->data, fw->size);
	release_firmware(fw);
	st->fw.status = MSENSORS_FW_UP_WAIT_RESET;
	show_fw_status(st);
	wake_up(&st->fw.wait);
}

static int update_firmware_from_class(struct device *dev)
{
	int retval;

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG,
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
