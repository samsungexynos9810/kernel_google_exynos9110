/*
 * Driver for MultiSensors
 *
 */

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>	/* For handling misc devices */
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/unistd.h>
#include <linux/power_supply.h>
#include <linux/mfd/samsung/rtc-s2mp.h>
#include <linux/wakelock.h>
#include <linux/proc_fs.h>

#include "subcpu_battery.h"
#include "MSensorsDrv.h"

#define DRV_NAME	"MultiSensors"
#define DRV_NAME_CD 	"MultiSensors_CD"
#define DRV_NAME_CD_01 	"MultiSensors_CD_01"

#define MINOR_COUNT 	(1)
/* #define CFG_SUBCPU_LOG */

#define INC_INDEX(a, r)	\
do {	\
	if ((a) < ((r)-1))	\
		(a)++;	\
	else		\
		(a)=0;	\
} while(0)

struct WriteDataBuf {
	unsigned char	m_data[WRITE_DATA_SIZE];
	unsigned char	m_used;
	struct list_head bqueue;
};

static spinlock_t slock;
struct mutex mlock;
static struct WriteDataBuf WriteDataBuf[WRITE_DATABUFF_SIZE];
static struct list_head wd_queue;

static struct Msensors_state 	*g_st=0;
static dev_t 			dev_id;
static struct class		*Msensors_class;
static struct device		*class_dev;

static struct Msensors_data	Msensors_data_buff[MSENSORS_DATA_MAX];
static unsigned int		dataBuffReadIndex;
static unsigned int		dataBuffWriteIndex;
static unsigned int		PacketDataNum=0;
static unsigned char		HeaderData[HEADER_DATA_SIZE] = {
	0x00, 0x00, 0x00, 0x1e, 0x21, 0x80, 0x00, 0x05 };	/* 2015/1/1 0:0:0 */
static unsigned char		Flg_driver_ready = 0;
static unsigned char		Flg_driver_probed = 0;
static unsigned char		Flg_driver_shutdown = 0;

static unsigned char SubReadData[SUB_COM_DATA_SIZE_GETDATA];

static wait_queue_head_t wait_ioctl;
static volatile int ioctl_complete;

static wait_queue_head_t wait_rd;
static wait_queue_head_t wait_subint;
static volatile int sub_main_int_occur;
static volatile int data_pushed;
static int64_t soc_time;

struct Msensors_state *get_msensors_state(void)
{
	return g_st;
}

static struct WriteDataBuf *allocWbBuf(void)
{
	int i;
	unsigned long flags;

	for (i = 0; i < WRITE_DATABUFF_SIZE; i++) {
		spin_lock_irqsave(&slock, flags);
		if (WriteDataBuf[i].m_used == 0) {
			WriteDataBuf[i].m_used = 1;
			spin_unlock_irqrestore(&slock, flags);
			return &WriteDataBuf[i];
		}
		spin_unlock_irqrestore(&slock, flags);
	}
	return NULL;
}

static void freeWbBuf(struct WriteDataBuf *wb)
{
	wb->m_used = 0;
}

ssize_t Msensors_Spi_Send( struct Msensors_state *st, char* send_buf, char* recv_buf, size_t count )
{
	struct spi_message msg;
	struct spi_transfer xfer;
	unsigned char tx_type;
	int ret;

	memset(&xfer, 0, sizeof(xfer));
	xfer.len = count;
	xfer.speed_hz = 1000 * 1000;
	xfer.tx_buf = send_buf;
	xfer.rx_buf = recv_buf;
	xfer.bits_per_word = 8;

	tx_type = (unsigned char)send_buf[0];

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(st->sdev, &msg);
	if (ret < 0)
		pr_info("%s: ret = %d\n", __func__, ret);

	return ret;
}

static int Msensors_Open(struct inode *inode, struct file *file)
{
	struct Msensors_state *st;

	st = container_of(inode->i_cdev, struct Msensors_state, cdev);
	dev_dbg(&st->sdev->dev, "Msensors_Open\n");

	file->private_data = st;

	return 0;
}

static int Msensors_Close(struct inode *inode, struct file *file)
{
	struct Msensors_state *st;
	st = file->private_data;
	dev_dbg(&st->sdev->dev, "Msensors_Close\n");
	return 0;
}

static int64_t getTimestamp(void)
{
	struct timespec ts;
	get_monotonic_boottime(&ts);
	return timespec_to_ns(&ts);
}

void Msensors_SetTimestamp(void)
{
	soc_time = getTimestamp();
}

int Msensors_PushData(unsigned char* write_buff)
{
	struct WriteDataBuf *wb;
	unsigned long flags;
	int counter = 0;

	if (g_st->fw.status == MSENSORS_FW_UP_UPDATING)
		return -EPROBE_DEFER;

	if (Flg_driver_probed == 0 || Flg_driver_shutdown) {
		dev_info(&g_st->sdev->dev, "write command type:%02x id:%02x ignored.\n"
				,write_buff[1] , write_buff[2]);
		return -EPROBE_DEFER;
	}

retry:
	wb = allocWbBuf();
	if (wb) {
		memcpy(wb->m_data, &write_buff[0], WRITE_DATA_SIZE);
		spin_lock_irqsave(&slock, flags);
		list_add_tail(&wb->bqueue, &wd_queue);
		spin_unlock_irqrestore(&slock, flags);
		data_pushed = 1;
		wake_up(&wait_subint);
	} else if (Flg_driver_ready) {
		msleep(10);
		counter++;
		if (counter < 10)
			goto retry;
	}
	return 0;
}

static void sub_HeaderInfoProc(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	/* charger or battery property changed */
	if (HeaderData[2] & (1 << SUB_ALERT_BIT_POWER_CHG1)) {
		write_buff[0] = SUB_COM_TYPE_READ;
		write_buff[1] = SUB_COM_GETID_POWER_PROP1;
		write_buff[2] = 0xff;
		Msensors_PushData(&write_buff[0]);
	}

	if (HeaderData[2] & (1 << SUB_ALERT_BIT_POWER_CHG2)) {
		write_buff[0] = SUB_COM_TYPE_READ;
		write_buff[1] = SUB_COM_GETID_POWER_PROP2;
		write_buff[2] = 0xff;
		Msensors_PushData(&write_buff[0]);
	}
}

static uint32_t cnt_num[NORMAL_SENSOR_NUM];	// number of samples
static uint32_t cnt_idx[NORMAL_SENSOR_NUM];
static int64_t meas_tri;

static void timestamp_estimate_ts(int64_t measTri)
{
	meas_tri = measTri & ~(0x3FF);
}

static void timestamp_count(uint8_t *recv, int packetnum)
{
	int i, type;

	for (i = 0; i < NORMAL_SENSOR_NUM; i++) {
		cnt_num[i] = 0;
		cnt_idx[i] = 0;
	}

	for (i = 0; i < packetnum; i++) {
		type = *recv;
		recv += (SUB_COM_DATA_SIZE_PACKET + SUB_COM_ID_SIZE);
		if (type == 0)
			continue;
		type = (type & 0x1f) - 1;
		if (type < NORMAL_SENSOR_NUM)
			++cnt_num[type];
	}
}


static int64_t get_timestamp(int type)
{
	int64_t t;
	int u;
	++cnt_idx[type];
	/* the lower 10 bit is used for the index of decrease */
	u = cnt_num[type] - cnt_idx[type];
	if ( u < 0 ) u = 0;  /* not occure but safe*/
	t = meas_tri + u;
	return t;
}

static struct wake_lock wlock;

static int SensorReadThread(void *p)
{
	struct Msensors_state *st = g_st;
	unsigned char type;
	unsigned char send_type;
	int sensor_num;
	int sensor_type;
	int cnt;
	int recv_index;
	int next_recv_size;
	unsigned char* recv_buf = &st->spi.recv_buf[0];
	int64_t event_time;
	uint32_t elapsed_time = 0;
	unsigned long flags;
	struct WriteDataBuf *wb;

	while (!kthread_should_stop()) {

		next_recv_size = SUB_COM_TYPE_SIZE;
		send_type = SUB_COM_TYPE_RES_NOMAL;
		memset(&st->spi.send_buf[0], SUB_COM_SEND_DUMMY, SPI_DATA_MAX);

		/* Get Type */
		type = recv_buf[SUB_COM_HEAD_INDEX_TYPE];

		/* HEADER Information */
#ifdef CFG_SUBCPU_LOG
		if (type == SUB_COM_TYPE_BIT_HEAD || type == SUB_COM_TYPE_SUBCPU_LOG) {
#else
		if (type == SUB_COM_TYPE_BIT_HEAD) {
#endif
			/* Get Header Data */
			memcpy(&HeaderData[0], &recv_buf[0], HEADER_DATA_SIZE);

			/* Sub Header Infomation Proc */
			sub_HeaderInfoProc();

			/* Get sensors data num */
			sensor_num = recv_buf[SUB_COM_HEAD_INDEX_PACKET];

			/* Calc sensors data size */
#ifdef CFG_SUBCPU_LOG
			if (type == SUB_COM_TYPE_SUBCPU_LOG)
			next_recv_size += sensor_num * 65;
			else
#endif
			next_recv_size += sensor_num * ( SUB_COM_DATA_SIZE_PACKET + SUB_COM_ID_SIZE );
			PacketDataNum = sensor_num;
			if (st->spi.pre_send_type==SUB_COM_TYPE_READ) {
				next_recv_size += SUB_COM_DATA_SIZE_GETDATA + SUB_COM_ID_SIZE;
				if(PacketDataNum == 0)
					send_type = SUB_COM_TYPE_GETDATA;
				else
					send_type = SUB_COM_TYPE_SENSOR_GETDATA;
			} else if(PacketDataNum) {
				send_type = SUB_COM_TYPE_SENSOR;
			}
		} else if (type == SUB_COM_TYPE_FWUP_READY) {	/* 0x81 */
			pr_info("msensors fw update\n");
			st->fw.status = MSENSORS_FW_UP_READY;
			wake_up(&st->fw.wait);
			wait_event(st->fw.wait,
					st->fw.status == MSENSORS_FW_UP_WAIT_RESET);
			st->spi.pre_send_type = SUB_COM_TYPE_RES_NOMAL;
		} else {
		/* Data */
			recv_index = SUB_COM_DATA_INDEX_ID;
			if ((type == SUB_COM_TYPE_GETDATA ) ||		/* Get Data Only */
				(type == SUB_COM_TYPE_SENSOR_GETDATA )) {	/* Get Data and Sensor Data */
				if (recv_buf[recv_index] == SUB_COM_GETID_SUB_CPU_VER) {
					Msensors_set_fw_version(st, &recv_buf[recv_index + 1]);
#ifdef CONFIG_SUBCPU_BATTERY
				} else if (recv_buf[recv_index] == SUB_COM_GETID_POWER_PROP1) {
					subcpu_battery_update_status1(recv_buf);
				} else if (recv_buf[recv_index] == SUB_COM_GETID_POWER_PROP2) {
					subcpu_battery_update_status2(recv_buf);
#endif
				} else if (recv_buf[recv_index] < SUB_COM_GETID_NUM) {
					memcpy(SubReadData, &recv_buf[recv_index + 1], SUB_COM_DATA_SIZE_GETDATA);
					ioctl_complete = 1;
					wake_up_interruptible(&wait_ioctl);
				}
				/* Get Data Proc */
				recv_index += SUB_COM_DATA_SIZE_GETDATA + SUB_COM_ID_SIZE;
			}

			if ((type == SUB_COM_TYPE_SENSOR) ||		/* Sensor Data Only */
				(type == SUB_COM_TYPE_SENSOR_GETDATA)) {	/* Get Data and Sensor Data */
				event_time = soc_time - elapsed_time * 1000000LL;
				/* Sensor Data Proc */
				if (recv_buf[recv_index] == MSENSORS_TYPE_TIMESTAMP)
					wake_lock_timeout(&wlock, HZ/5);
#ifdef CFG_SUBCPU_LOG
			  if (recv_buf[recv_index] == MSENSORS_TYPE_SUBCPU_LOG) {	// subcpu log
				for (cnt= 0; cnt < PacketDataNum; cnt++) {
					if (recv_buf[recv_index] != MSENSORS_TYPE_SUBCPU_LOG) {
						pr_info("subcpu type invalid\n");
						break;
					}
					recv_index++;
					printk(KERN_DEBUG "[subcpu] %s", &recv_buf[recv_index]);
					recv_index += 64;
				}
			  } else {
#endif
				timestamp_count(&recv_buf[recv_index], PacketDataNum);
				timestamp_estimate_ts(getTimestamp());
				for (cnt= 0; cnt < PacketDataNum; cnt++) {
					sensor_type = recv_buf[recv_index++];

					if (sensor_type == MSENSORS_TYPE_TIMESTAMP) {
						elapsed_time = recv_buf[recv_index+3]<<24 | recv_buf[recv_index+2]<<16 |
										recv_buf[recv_index+1]<<8 | recv_buf[recv_index];
						event_time = soc_time - elapsed_time * 1000000LL;
					} else {
						if ((sensor_type & 0x1f) > NORMAL_SENSOR_NUM) {
							Msensors_data_buff[dataBuffWriteIndex].timestamp = event_time;
						} else {
							Msensors_data_buff[dataBuffWriteIndex].timestamp =
								get_timestamp((sensor_type & 0x1f) - 1);
						}
						Msensors_data_buff[dataBuffWriteIndex].sensor_type = sensor_type;
						memcpy(&Msensors_data_buff[dataBuffWriteIndex].sensor_value[0],
											&recv_buf[recv_index], 6);
						spin_lock_irqsave(&slock, flags);
						INC_INDEX(dataBuffWriteIndex, MSENSORS_DATA_MAX);
						if (dataBuffWriteIndex == dataBuffReadIndex)
							INC_INDEX(dataBuffReadIndex, MSENSORS_DATA_MAX);
						spin_unlock_irqrestore(&slock, flags);
					}
					recv_index += SUB_COM_DATA_SIZE_PACKET;
				}
				wake_up_interruptible_sync(&wait_rd);
#ifdef CFG_SUBCPU_LOG
			  }
#endif
			}
		}
		wb = NULL;
		if (send_type == SUB_COM_TYPE_RES_NOMAL) {
			next_recv_size += SUB_COM_HEAD_SIZE_SETDATA + SUB_COM_ID_SIZE;
retry_check_wq:
			if (!list_empty(&wd_queue)) {
				wb = list_first_entry(&wd_queue, struct WriteDataBuf, bqueue);
				send_type = wb->m_data[0];
				memcpy(&st->spi.send_buf[0], wb->m_data, WRITE_DATA_SIZE);
			}
		}
		/* Next SPI Send Proc */
		st->spi.send_buf[SUB_COM_HEAD_INDEX_TYPE] = send_type;
		st->spi.pre_send_type =  send_type;
		if (send_type == SUB_COM_TYPE_RES_NOMAL) {
			wait_event(wait_subint, sub_main_int_occur | Flg_driver_shutdown | data_pushed);
			if (data_pushed) {
				data_pushed = 0;
				goto retry_check_wq;
			}
		} else {
			if (sub_main_int_occur == 0)
				gpio_set_value(g_st->main_sub_int, 1);
			wait_event(wait_subint, sub_main_int_occur | Flg_driver_shutdown);
		}
		sub_main_int_occur = 0;
		data_pushed = 0;
		gpio_set_value(g_st->main_sub_int, 0);
		if (Flg_driver_shutdown)
			break;

		while (!Flg_driver_ready)
			msleep(10);

		if (st->spi.send_buf[0] == SUB_COM_TYPE_WRITE &&
			st->spi.send_buf[1] == SUB_COM_SETID_MAIN_STATUS &&
			st->spi.send_buf[2] == 0x2)	/* shutdown */
			Flg_driver_shutdown = 1;
		Msensors_SetTimestamp();
		Msensors_Spi_Send(st, &st->spi.send_buf[0], &st->spi.recv_buf[0], next_recv_size);

		if (wb) {
			spin_lock_irqsave(&slock, flags);
			list_del(&wb->bqueue);
			spin_unlock_irqrestore(&slock, flags);
			freeWbBuf(wb);
		}
	}
	return 0;
}

void spi_send_wrapper_for_fwup(uint8_t *sendbuf, uint8_t *recvbuf, size_t count)
{
	if (sub_main_int_occur == 0)
		gpio_set_value(g_st->main_sub_int, 1);
	wait_event(wait_subint, sub_main_int_occur);
	sub_main_int_occur = 0;
	gpio_set_value(g_st->main_sub_int, 0);
	Msensors_Spi_Send(g_st, sendbuf, recvbuf, count);
}

int cyttsp5_get_palm_on(void);
#define MSENSORS_HANDLE_WRIST_TILT		(0x09)		/* Sensor Type Wrist Tilt  */

static ssize_t Msensors_Read( struct file* file, char* buf, size_t count, loff_t* offset )
{
	int cnt=0;
	int buf_index=0;
	int size = sizeof(struct Msensors_data);
	int ret;
	int data_num = count / sizeof(struct Msensors_data);
	struct Msensors_state *st;

	st = file->private_data;
retrywait:
	/* Wait until dataBuffWriteIndex moved */
	ret = wait_event_interruptible(wait_rd, dataBuffWriteIndex != dataBuffReadIndex);
	if (ret < 0) {
		return ret;
	}
	while (cnt < data_num) {
		if (dataBuffWriteIndex == dataBuffReadIndex) {
			if (cnt == 0)
				goto retrywait;
			else
				break;	/* no data */
		}
		if (cyttsp5_get_palm_on()) {
			if (Msensors_data_buff[dataBuffReadIndex].sensor_type == MSENSORS_HANDLE_WRIST_TILT) {
				INC_INDEX(dataBuffReadIndex, MSENSORS_DATA_MAX);
				continue;
			}
		}
		if (copy_to_user(buf+buf_index, &Msensors_data_buff[dataBuffReadIndex], size))
			return -EFAULT;
		buf_index += size;
		cnt++;
		INC_INDEX(dataBuffReadIndex, MSENSORS_DATA_MAX);
	}

	return buf_index;
}

void sharp_lcd_notify_seglcd(int seglcd_on);
void sharp_lcd_notify_always_segment(int mode);
static int mode_always_segment_wf;
static int mode_always_segment_activity;

static ssize_t Msensors_Write(struct file* file, const char* buf, size_t count,
						loff_t* offset)
{
	struct Msensors_state *st;
	int ret;
	unsigned char write_buff[WRITE_DATA_SIZE];

	st = file->private_data;
	dev_dbg(&st->sdev->dev, "Msensors_Write Start count=%zu\n", count);

	ret = copy_from_user(&write_buff[1], buf, count);

	if (!ret) {
#ifdef CONFIG_BACKLIGHT_SUBCPU
		if (write_buff[1] == SUB_COM_SETID_SEG_CMD) {
			if (write_buff[2] == 0) {			// SegLCD On/Off Control
				sharp_lcd_notify_seglcd((write_buff[3] & 0x80) ? 1 : 0);
			} else if (write_buff[2] == 9) {	// Notify On/Off AlwaysSegment
				if ((write_buff[3] & 0x7F) == 0) {
					mode_always_segment_wf = (write_buff[3] & 0x80) ? 1 : 0;
				} else {
					mode_always_segment_activity = (write_buff[3] & 0x80) ? 1 : 0;
				}
				sharp_lcd_notify_always_segment((mode_always_segment_wf || mode_always_segment_activity));
			}
		}
#endif
		write_buff[0] = SUB_COM_TYPE_WRITE;	//0xA1
		Msensors_PushData(&write_buff[0]);
	}

	return ret;
}

static int sub_read_command(uint8_t command)
{
	int ret;
	unsigned char write_buff[WRITE_DATA_SIZE];

	write_buff[0] = SUB_COM_TYPE_READ;
	write_buff[1] = command;
	write_buff[2] = 0xff;
	mutex_lock(&mlock);
	ioctl_complete = 0;
	Msensors_PushData(&write_buff[0]);
	ret = wait_event_interruptible(wait_ioctl, ioctl_complete == 1);
	mutex_unlock(&mlock);
	return ret;
}

static void get_subcpu_version(uint8_t *buf)
{
	buf[0] = g_st->fw.maj_ver;
	buf[1] = g_st->fw.min_ver;
	buf[2] = g_st->fw.revision;
}

static long Msensors_Ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	uint8_t buf[3];

	/* this function must be reentrant */
	pr_info("%s: cmd %x\n", __func__, cmd);
	if (cmd == IOC_GET_VERSION) {
		get_subcpu_version(buf);
		ret = copy_to_user((void __user *)arg, buf, 3);
	} else if (cmd == IOC_ACCEL_ADJ) {
		/* accelerometer factory adjustment */
		ret = sub_read_command(SUB_COM_GETID_ACC_ADJ);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 3);
	} else if (cmd == IOC_POWER_WARN) {
		ret = sub_read_command(SUB_COM_GETID_POWER_WARN);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 2);
	} else if (cmd == IOC_HEIGHT_CORR) {
		ret = sub_read_command(SUB_COM_GETID_HEIGHT_CORR);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 4);
	} else if (cmd == IOC_PRESSURE_CORR) {
		ret = sub_read_command(SUB_COM_GETID_PRESSURE_CORR);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 4);
	} else if (cmd == IOC_STEP_TODAY) {
		ret = sub_read_command(SUB_COM_GETID_STEP_TODAY);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 4);
	} else if (cmd == IOC_STEP_YESTERDAY) {
		ret = sub_read_command(SUB_COM_GETID_STEP_YESTERDAY);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 4);
	} else if (cmd == IOC_BATTERY_LOG_START) {
		ret = sub_read_command(SUB_COM_GETID_BATTERY_LOG_START);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 5);
	} else if (cmd == IOC_BATTERY_LOG_GET_DATA) {
		ret = sub_read_command(SUB_COM_GETID_BATTERY_LOG_GET_DATA);
		if (ret >= 0)
			ret = copy_to_user((void __user *)arg, SubReadData, 6);
	} else {
		ret = -1;
		pr_info("%s: unknown command %x\n", __func__, cmd);
	}
	return ret;
}

#ifdef CFG_SUBCPU_LOG
static void get_subcpu_log(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_REQUEST_SUBCPU_LOG;
	write_buff[2] = 0xff;
	Msensors_PushData(&write_buff[0]);
}
#endif

static ssize_t read_i2c_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	sub_read_command(SUB_COM_GETID_I2C_STATUS);
	return sprintf(buf, "i2c_status: %d\n", SubReadData[0]);

}


static ssize_t read_acc_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int16_t *offset;

	sub_read_command(SUB_COM_GETID_ACC_READ_OFFSET);
	offset = (int16_t *)SubReadData;
	return sprintf(buf, "acc_offset: %d,%d,%d\n",
		offset[0], offset[1], offset[2]);
}

static ssize_t read_acc_gain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	uint16_t *gain;

	sub_read_command(SUB_COM_GETID_ACC_READ_GAIN);
	gain = (int16_t *)SubReadData;
	return sprintf(buf, "acc_gain: %d,%d,%d\n",
		gain[0], gain[1], gain[2]);
}

static struct device_attribute attributes[] = {
	__ATTR(i2c_status, S_IRUGO, read_i2c_status_show, NULL),
	__ATTR(acc_offset, S_IRUGO, read_acc_offset_show, NULL),
	__ATTR(acc_gain, S_IRUGO, read_acc_gain_show, NULL),
};

static int add_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto undo;
	return 0;
undo:
	for (i--; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static struct file_operations Msensors_fops = {
		.owner = THIS_MODULE,
		.open = Msensors_Open,
		.release = Msensors_Close,
		.read = Msensors_Read,
		.write = Msensors_Write,
		.unlocked_ioctl = Msensors_Ioctl,
		.compat_ioctl = Msensors_Ioctl,
};

static void Msensors_init(struct Msensors_state *st)
{
	memset(&st->spi.send_buf[0], SUB_COM_SEND_DUMMY, SPI_DATA_MAX);
	st->spi.send_buf[0] = SUB_COM_TYPE_WRITE;
	st->spi.send_buf[1] = SUB_COM_SETID_MAIN_STATUS;
	st->spi.send_buf[2] = 0x0;	/* Nomal */
	st->spi.pre_send_type =  st->spi.send_buf[0];

	sub_main_int_occur = 0;
	gpio_set_value(g_st->main_sub_int, 1);
	wait_event(wait_subint, sub_main_int_occur);
	sub_main_int_occur = 0;
	gpio_set_value(g_st->main_sub_int, 0);
	Msensors_Spi_Send(st, &st->spi.send_buf[0], &st->spi.recv_buf[0], WRITE_DATA_SIZE);
}

static irqreturn_t sub_main_int_wake_isr(int irq, void *dev)
{
	sub_main_int_occur = 1;
	wake_up(&wait_subint);
	return IRQ_HANDLED;
}

static int subcpu_proc_show(struct seq_file *m, void *v)
{
	uint16_t *p = (uint16_t *)SubReadData;

	sub_read_command(SUB_COM_GETID_FG_VER);
	seq_printf(m, "subcpu %02x.%02x.%02x\n",
		g_st->fw.maj_ver, g_st->fw.min_ver,  g_st->fw.revision);
	seq_printf(m, "subcpu reset cause %02x\n", g_st->fw.subcpu_reset_cause);
	seq_printf(m, "fg %04x, par %04x\n", p[0], p[1]);
	return 0;
}

static int subcpu_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, subcpu_proc_show, PDE_DATA(inode));
}

static const struct file_operations subcpu_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= subcpu_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int Msensors_probe(struct spi_device *spi)
{
	int ret, err;
	int rc, irq;
	struct Msensors_state *st;

	dev_dbg(&spi->dev, "Msensors_probe Start\n");

	init_waitqueue_head(&wait_rd);
	init_waitqueue_head(&wait_subint);
	init_waitqueue_head(&wait_ioctl);
	spin_lock_init(&slock);
	mutex_init(&mlock);
	INIT_LIST_HEAD(&wd_queue);
	wake_lock_init(&wlock, WAKE_LOCK_SUSPEND, "multisensors");
	dataBuffReadIndex = 0;
	dataBuffWriteIndex = 0;
	memset(&HeaderData[0], 0x00, HEADER_DATA_SIZE);

	st = kzalloc(sizeof *st, GFP_KERNEL);
	if (st == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	err = alloc_chrdev_region(&dev_id,0,MINOR_COUNT,DRV_NAME_CD);
	if (err < 0) {
		dev_err(&spi->dev, "Msensors_probe alloc_chrdev_region fail\n");
		ret = err;
		goto error_free_dev;
	}

	cdev_init(&st->cdev,&Msensors_fops);
	st->cdev.owner = THIS_MODULE;
	st->cdev.ops = &Msensors_fops;
	err = cdev_add(&st->cdev, dev_id, MINOR_COUNT);
	if (err < 0) {
		dev_err(&spi->dev, "Msensors_probe cdev_add fail\n");
		ret = err;
		goto error_free_dev;
	}

	Msensors_class = class_create(THIS_MODULE, DRV_NAME_CD);
	class_dev = device_create(Msensors_class, &spi->dev, dev_id, NULL, DRV_NAME_CD_01);

	st->sdev = spi_dev_get(spi);
	spi_set_drvdata(spi, st);

	spi->max_speed_hz = 1000 * 1000;
	spi->mode = SPI_MODE_1;	/* CPHA=1 CPOL=0 */
	spi->bits_per_word = 8;
	spi_setup(spi);

	st->sub_main_int = of_get_named_gpio(spi->dev.of_node,	"nsw,sub_main_int", 0);
	pr_info("%s: sub_main_int = %d\n", __func__, st->sub_main_int);
	st->main_sub_int = of_get_named_gpio(spi->dev.of_node,	"nsw,main_sub_int", 0);
	pr_info("%s: main_sub_int = %d\n", __func__, st->main_sub_int);

	g_st = st;

	rc = gpio_request(st->sub_main_int, "sub_main_int_gpio");
	if (unlikely(rc)) {
		return rc;
	}
	gpio_direction_input(st->sub_main_int);
	irq = gpio_to_irq(st->sub_main_int);
	ret = request_irq(irq, sub_main_int_wake_isr, IRQF_TRIGGER_RISING, "sub_main_int_wake", NULL);
	if (ret) {
		gpio_free(st->sub_main_int);
		return ret;
	}

	rc = gpio_request_one(st->main_sub_int, GPIOF_OUT_INIT_LOW, "main_sub_int_gpio");
	if (unlikely(rc)) {
		return rc;
	}

	add_sysfs_interfaces(&spi->dev);

	enable_irq_wake(irq);

	Flg_driver_ready = 1;
	Flg_driver_probed = 1;
	Msensors_init(st);

	st->spi.pre_send_type = 0;
	st->spi.sensor_read_thread = kthread_run(SensorReadThread, st,DRV_NAME);
	if (IS_ERR(st->spi.sensor_read_thread)) {
		ret = PTR_ERR(st->spi.sensor_read_thread);
		goto error_free_dev;
	}
	msensors_fw_up_init(st);
#ifdef CFG_SUBCPU_LOG
	get_subcpu_log();
#endif

	proc_create_data("subcpu", S_IRUGO, NULL, &subcpu_proc_fops, NULL);

	dev_dbg(&spi->dev, "Msensors_probe End\n");
	return 0;

error_free_dev:
	kfree(st);

error_ret:
	return ret;
}

static void Msensors_shutdown(struct spi_device *spi)
{
	unsigned char write_buff[WRITE_DATA_SIZE];
	int count = 0;

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;

	write_buff[2] = 0x3;	/* prepare shutdown */
	Msensors_PushData(&write_buff[0]);
	while (!list_empty(&wd_queue) && count < 30) {
		msleep(10);
		count++;
	}
	count = 0;

	write_buff[2] = 0x2;	/* shutdown */
	Msensors_PushData(&write_buff[0]);
	while (!list_empty(&wd_queue) && count < 30) {
		msleep(10);
		count++;
	}

	Flg_driver_shutdown = 1;
}

static int Msensors_suspend(struct device *dev)
{
	unsigned char write_buff[WRITE_DATA_SIZE];
	int count = 0;

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x1;	/* suspend */

	Msensors_PushData(&write_buff[0]);
	while (!list_empty(&wd_queue) && count < 30) {
		msleep(10);
		count++;
	}
	Flg_driver_ready = 0;

	return 0;
}

static void Msensors_complete(struct device *dev)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	Flg_driver_ready = 1;

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x0;	/* Nomal */

	Msensors_PushData(&write_buff[0]);
}

static int Msensors_runtime_suspend(struct device *dev)
{
	return 0;
}

static int Msensors_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops Msensors_pm = {
	.suspend = Msensors_suspend,
	.complete = Msensors_complete,
	SET_RUNTIME_PM_OPS(Msensors_runtime_suspend,
			   Msensors_runtime_resume, NULL)
};

static struct of_device_id Msensors_match_table[] = {
	{ .compatible = "MultiSensors,24",},
	{ },
};

static struct spi_driver Msensors_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &Msensors_pm,
		.of_match_table	= Msensors_match_table,
	},
	.probe = Msensors_probe,
	.shutdown = Msensors_shutdown,
};
module_spi_driver(Msensors_driver);

void SUB_VibratorSet(int timeout)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_VIB_SET;
	write_buff[2] = (timeout >> 0) & 0xFF;	/* ms */
	write_buff[3] = (timeout >> 8) & 0xFF;	/* ms */

	Msensors_PushData(&write_buff[0]);
}

int SUBCPU_rtc_read_time(uint8_t *data)
{
	uint8_t *p;
	uint32_t version;

	version = g_st->fw.maj_ver << 16 | g_st->fw.min_ver << 8 | g_st->fw.revision;
	if (version >= 0x150a00) {
		sub_read_command(SUB_COM_GETID_RTC_DATE_TIME);
		p = SubReadData;
	} else {
		p = &HeaderData[3];
	}
	data[RTC_YEAR]  = (uint8_t)(p[0] >> 1);
	data[RTC_MONTH] = (uint8_t)(((p[0] & 0x01) << 3) |
			                    ((p[1] & 0xE0) >> 5));
	data[RTC_DATE]  = (uint8_t)(p[1] & 0x1F);
	data[RTC_WEEKDAY] = BIT((uint8_t)(p[2] >> 5));
	data[RTC_HOUR]  = (uint8_t)(p[2] & 0x1F);
	data[RTC_MIN]   = p[3];
	data[RTC_SEC]   = p[4];

	return 0;
}

void SUBCPU_send_lowtemp_burnoff_enable(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_LOWTEMP_BURNOFF;
	write_buff[2] = 1;

	Msensors_PushData(&write_buff[0]);
}

#ifdef CONFIG_BACKLIGHT_SUBCPU
static int last_brightness = -1;
int SUB_LCDBrightnessSet(unsigned char LCDBrightness)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	if (last_brightness == LCDBrightness)
		return 0;
	last_brightness = LCDBrightness;

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_LCD_SET;
	write_buff[2] = LCDBrightness;	/* LCD brightness */

	return Msensors_PushData(&write_buff[0]);
}

int SUB_LCDForceOnOffSet(unsigned char force_on)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_SEG_CMD;
	write_buff[2] = 1;
	write_buff[3] = 18;
	if (force_on)
		write_buff[4] = 1;
	else
		write_buff[4] = 0;

	return Msensors_PushData(&write_buff[0]);
}
#endif

MODULE_AUTHOR("Casio");
MODULE_DESCRIPTION("MultiSensors driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:" DRV_NAME);

