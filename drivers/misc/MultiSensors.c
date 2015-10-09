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
#include <linux/mfd/samsung/rtc.h>
#include <linux/wakelock.h>

#include "../power/subcpu_battery.h"
#include "../video/backlight/bd82103.h"
#include "MSensorsDrv.h"
#include "MultiSensors_FW.h"

#define DRV_NAME	"MultiSensors"
#define DRV_NAME_CD 	"MultiSensors_CD"
#define DRV_NAME_CD_01 	"MultiSensors_CD_01"

#define MINOR_COUNT 	(1)

#define SPI_DATA_MAX (SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE + SUB_COM_DATA_SIZE_GETDATA + (SUB_COM_MAX_PACKET * ( SUB_COM_DATA_SIZE_PACKET + SUB_COM_ID_SIZE )))
#define HEADER_DATA_SIZE ( SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE +SUB_COM_HEAD_SIZE_SETDATA )
#define WRITE_DATA_SIZE ( SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE + SUB_COM_HEAD_SIZE_SETDATA)
#define WRITE_DATABUFF_SIZE 32

#define INC_INDEX(a, r)	\
do {	\
	if ((a) < ((r)-1))	\
		(a)++;	\
	else		\
		(a)=0;	\
} while(0)

struct Msensors_state {
	struct spi_device *sdev;
	struct cdev cdev;
	unsigned int sub_main_int;
};

struct WriteDataBuf {
	unsigned char	m_data[WRITE_DATA_SIZE];
	unsigned char	m_used;
	struct list_head bqueue;
};

static spinlock_t slock;
static struct WriteDataBuf WriteDataBuf[WRITE_DATABUFF_SIZE];
static struct list_head wd_queue;

static struct Msensors_state 	*g_st=0;
static dev_t 			dev_id;
static struct class		*Msensors_class;
static struct device		*class_dev;
static struct task_struct	*pSensorReadThread;

static struct Msensors_data	Msensors_data_buff[MSENSORS_DATA_MAX];
static unsigned int		dataBuffReadIndex;
static unsigned int		dataBuffWriteIndex;
static unsigned int		PacketDataNum=0;
static unsigned int		PreSendType=0;
static unsigned int		SpiDataLen=0;
static unsigned int		SpiErrorCount=0;	/*** pet yasui ADD ***/
static unsigned char		HeaderData[HEADER_DATA_SIZE];
static unsigned char 		SpiSendBuff[SPI_DATA_MAX];
static unsigned char 		SpiRecvBuff[SPI_DATA_MAX];
static unsigned char		HeaderRcvFlg=0;
static unsigned char		Flg_driver_ready = 0;
static unsigned char		Flg_driver_probed = 0;
static unsigned char		Flg_driver_shutdown = 0;

static unsigned char SubCpuVersion[SUB_COM_DATA_SIZE_GETDATA];
static unsigned char SubReadData[SUB_COM_DATA_SIZE_GETDATA];

static const char path[] = "/data/local/tmp/SUB_CPU_TZ.srec";
static	int Totallen = 0;
static	int Now_len = 1;
static uint8_t gFlg_AutoUpdate = 0;
static wait_queue_head_t wait_ioctl;
static int ioctl_complete;
static struct sub_FW_data *gSUBCPU_Firmware;

static wait_queue_head_t wait_rd;
static wait_queue_head_t wait_subint;
static int sub_main_int_occur;
static int64_t soc_time;

static int SensorFWUpdateLoop(void);


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

static ssize_t Msensors_Spi_Send( struct Msensors_state *st, char* send_buf, char* recv_buf, size_t count )
{
	struct spi_message msg;
	struct spi_transfer xfer;
	unsigned char tx_type;
	int ret, ret_spi;

	xfer.len = count;
	xfer.speed_hz = 1024 * 1024;
	xfer.tx_buf = send_buf;
	xfer.rx_buf = recv_buf;
	xfer.bits_per_word = 8;
	xfer.cs_change = 0;
	xfer.delay_usecs = 0;

	tx_type = (unsigned char)send_buf[0];

	SpiDataLen = (unsigned int)count;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret_spi = spi_sync(st->sdev, &msg);
	if (ret_spi)
		ret = -EIO;

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
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    ktime_get_ts(&t);
    return (int64_t)t.tv_sec * 1000000000LL + t.tv_nsec;
}

void Msensors_SetTimestamp(void)
{
	soc_time = getTimestamp();
}

static uint8_t is_subcpu_need_update(uint8_t maj, uint8_t min, uint8_t rev)
{
	uint8_t ret = 0;
	uint32_t version = (uint32_t)(maj << 16 | min << 8 | rev);

	if (version !=
		(SUB_CPU_VER_MAJ << 16 | SUB_CPU_VER_MIN << 8 | SUB_CPU_REVISION)) {
		if (rev < 100) {
			gFlg_AutoUpdate = 1;
			ret = 1;
		}
	}

	return ret;
}

static int Set_WriteDataBuff(unsigned char* write_buff)
{
	struct WriteDataBuf *wb;
	unsigned long flags;

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
		sub_main_int_occur = 1;
		wake_up_interruptible(&wait_subint);
	} else if (Flg_driver_ready) {
		msleep(10);
		goto retry;
	}
	return 0;
}

static void start_subcpu_update(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_SUB_FIRM_UPDATE;
	Set_WriteDataBuff(&write_buff[0]);
}

static int SensorFWUpdateLoop_auto(void)
{
	struct Msensors_state *st = g_st;
	int line_len, line_now, line_total;

	line_total = SUB_FW_TOTAL_LEN;
	line_now = 1;

	while (!kthread_should_stop()) {
		line_len = gSUBCPU_Firmware[line_now-1].len;

		switch (SpiRecvBuff[SUB_COM_HEAD_INDEX_TYPE]) {
		case SUB_COM_TYPE_FWUP_READY:
		case SUB_COM_TYPE_FW_REQ_HEAD:
			memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
			SpiSendBuff[0] = SUB_COM_TYPE_SET_FW_SIZE;
			SpiSendBuff[1] = (uint8_t)(line_len & 0x00FF);
			SpiSendBuff[2] = (uint8_t)((line_len & 0xFF00) >> 8);
			SpiSendBuff[3] = (uint8_t)(line_now & 0x00FF);
			SpiSendBuff[4] = (uint8_t)((line_now & 0xFF00) >> 8);
			SpiSendBuff[5] = (uint8_t)(line_total & 0x00FF);
			SpiSendBuff[6] = (uint8_t)((line_total & 0xFF00) >> 8);
			PreSendType =  SpiSendBuff[0];
			Msensors_Spi_Send(st, SpiSendBuff, SpiRecvBuff, WRITE_DATA_SIZE);
			break;
		case SUB_COM_TYPE_FW_REQ_DATA:
			memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, 1 + line_len);
			SpiSendBuff[0] = SUB_COM_TYPE_SET_FW_DATA;
			memcpy(&SpiSendBuff[1], gSUBCPU_Firmware[line_now-1].data, line_len);
			PreSendType = SpiSendBuff[0];
			Msensors_Spi_Send(st, SpiSendBuff, SpiRecvBuff, 1 + line_len);
			if (line_now == line_total) {
				dev_info(&st->sdev->dev, "subcpu FW update Complete!\n");
				goto update_finish;
			}
			line_now++;
			break;
		default:
			dev_info(&st->sdev->dev,
					"FWUp RecvDeta Error (%04d/%04d)line \n", line_now, line_total);
			goto update_finish;
		}
	}
update_finish:
	kfree(gSUBCPU_Firmware);
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
		Set_WriteDataBuff(&write_buff[0]);
	}

	if (HeaderData[2] & (1 << SUB_ALERT_BIT_POWER_CHG2)) {
		write_buff[0] = SUB_COM_TYPE_READ;
		write_buff[1] = SUB_COM_GETID_POWER_PROP2;
		Set_WriteDataBuff(&write_buff[0]);
	}
}

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
	unsigned char* recv_buf = &SpiRecvBuff[0];
	int64_t event_time, last_event_time = 0;
	uint32_t elapsed_time = 0;
	unsigned long flags;
	static uint8_t flg_first = 1;
	struct WriteDataBuf *wb;

	HeaderRcvFlg = 0;

	while (!kthread_should_stop()) {

		next_recv_size = SUB_COM_TYPE_SIZE;
		send_type = SUB_COM_TYPE_RES_NOMAL;
		memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, SPI_DATA_MAX);

		/* Get Type */
		type = recv_buf[SUB_COM_HEAD_INDEX_TYPE];

		/* HEADER Information */
		if (type == SUB_COM_TYPE_BIT_HEAD) {
			/* Get Header Data */
			memcpy(&HeaderData[0], &recv_buf[0], HEADER_DATA_SIZE);
			HeaderRcvFlg = 1;

			/* Sub Header Infomation Proc */
			sub_HeaderInfoProc();

			/* Get sensors data num */
			sensor_num = recv_buf[SUB_COM_HEAD_INDEX_PACKET];

			/* Calc sensors data size */
			next_recv_size += sensor_num * ( SUB_COM_DATA_SIZE_PACKET + SUB_COM_ID_SIZE );
			PacketDataNum = sensor_num;
			if (PreSendType==SUB_COM_TYPE_READ) {
				next_recv_size += SUB_COM_DATA_SIZE_GETDATA + SUB_COM_ID_SIZE;
				if(PacketDataNum == 0)
					send_type = SUB_COM_TYPE_GETDATA;
				else
					send_type = SUB_COM_TYPE_SENSOR_GETDATA;
			} else {
				if(PacketDataNum == 0)
					send_type = SUB_COM_TYPE_RES_NOMAL;
				else
					send_type = SUB_COM_TYPE_SENSOR;
			}
		} else if (type == SUB_COM_TYPE_FWUP_READY) {
		/* FW Update Response */
			/* Thread Start */
			if (gFlg_AutoUpdate) {
				gFlg_AutoUpdate = 0;
				SensorFWUpdateLoop_auto();
			} else {
				SensorFWUpdateLoop();
			}
		} else {
		/* Data */
			recv_index = SUB_COM_DATA_INDEX_ID;
			if ((type == SUB_COM_TYPE_GETDATA ) ||		/* Get Data Only */
				(type == SUB_COM_TYPE_SENSOR_GETDATA )) {	/* Get Data and Sensor Data */
				if (recv_buf[recv_index] == SUB_COM_GETID_SUB_CPU_VER) {
					memcpy(SubCpuVersion, &recv_buf[recv_index + 1], SUB_COM_DATA_SIZE_GETDATA);
					dev_info(&st->sdev->dev, "subcpu FW version:%02x:%02x:%02x\n",
							SubCpuVersion[0], SubCpuVersion[1], SubCpuVersion[2]);
					if (flg_first) {
						if (is_subcpu_need_update(SubCpuVersion[0],
								SubCpuVersion[1], SubCpuVersion[2])) {
							dev_info(&st->sdev->dev, "subcpu FW update Start!\n");
							start_subcpu_update();
						} else {
							kfree(gSUBCPU_Firmware);
						}
						flg_first = 0;
					}
				} else if (recv_buf[recv_index] == SUB_COM_GETID_POWER_PROP1) {
					subcpu_battery_update_status1(recv_buf);
				} else if (recv_buf[recv_index] == SUB_COM_GETID_POWER_PROP2) {
					subcpu_battery_update_status2(recv_buf);
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
				for (cnt= 0; cnt < PacketDataNum; cnt++) {
					sensor_type = recv_buf[recv_index++];

					if (sensor_type == 0) {
						elapsed_time = recv_buf[recv_index+3]<<24 | recv_buf[recv_index+2]<<16 |
										recv_buf[recv_index+1]<<8 | recv_buf[recv_index];
						event_time = soc_time - elapsed_time * 1000000LL;
					} else {
						if (last_event_time > event_time)
							event_time = last_event_time;
						last_event_time = event_time;
						Msensors_data_buff[dataBuffWriteIndex].timestamp = event_time;
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
			}
		}

		wb = NULL;
		if (send_type == SUB_COM_TYPE_RES_NOMAL) {
			next_recv_size += SUB_COM_HEAD_SIZE_SETDATA + SUB_COM_ID_SIZE;
			if (!list_empty(&wd_queue)) {
				wb = list_first_entry(&wd_queue, struct WriteDataBuf, bqueue);
				send_type = wb->m_data[0];
				memcpy(&SpiSendBuff[0], wb->m_data, WRITE_DATA_SIZE);
			}
		}
		/* Next SPI Send Proc */
		SpiSendBuff[SUB_COM_HEAD_INDEX_TYPE] = send_type;
		PreSendType =  send_type;
		if (send_type == SUB_COM_TYPE_RES_NOMAL) {
			wait_event_interruptible(wait_subint, sub_main_int_occur | Flg_driver_shutdown);
			sub_main_int_occur = 0;
		}
		if (Flg_driver_shutdown)
			break;

		while (!Flg_driver_ready)
			msleep(10);

		Msensors_Spi_Send(st, &SpiSendBuff[0], &SpiRecvBuff[0], next_recv_size);
		if (wb) {
			spin_lock_irqsave(&slock, flags);
			list_del(&wb->bqueue);
			spin_unlock_irqrestore(&slock, flags);
			freeWbBuf(wb);
		}
	}

	return 0;
}

static int SensorFWUpdateLoop(void)
{
	struct Msensors_state *st = g_st;
	unsigned char type;
	unsigned char send_type;
	int next_recv_size;
	unsigned char* recv_buf = &SpiRecvBuff[0];

	struct file* filp=NULL;
	int readsize=1;
	char buf[256] = {0};
	char buf2[256] = {0};
	mm_segment_t oldfs;
	char* t;
	int len=0;

	/* Open and get line 1 */
	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, O_RDONLY, 0);
	set_fs(oldfs);

	if (IS_ERR(filp)) {
		dev_dbg(&st->sdev->dev, "File Open Error\n");
		return 0;
	} else {
		oldfs = get_fs();
		set_fs(get_ds());
		readsize = filp->f_op->read(filp, buf, (sizeof(buf)-1), &filp->f_pos);
		set_fs(oldfs);

		t = memchr(buf, '\n', readsize);
		if (t != NULL) {
			len = ++t - buf;
			memset(buf2, 0x00, sizeof(buf2));
			memcpy(buf2, buf, len);
			filp->f_pos = (filp->f_pos - (readsize - len));
		}
	}

	Now_len = 1; //init

	if(buf2[0] != 'S')
		dev_dbg(&st->sdev->dev, "Error Not S-Record %d \n", buf2[0]);

	memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
	memset(&SpiRecvBuff[0], 0x00, WRITE_DATA_SIZE);
	SpiSendBuff[0] = SUB_COM_TYPE_SET_FW_SIZE;
	SpiSendBuff[1] = (unsigned char)(len & 0x00FF);
	SpiSendBuff[2] = (unsigned char)((len & 0xFF00) >> 8);
	SpiSendBuff[3] = (unsigned char)(Now_len & 0x00FF);
	SpiSendBuff[4] = (unsigned char)((Now_len & 0xFF00) >> 8);
	SpiSendBuff[5] = (unsigned char)(Totallen & 0x00FF);
	SpiSendBuff[6] = (unsigned char)((Totallen & 0xFF00) >> 8);

	PreSendType =  SpiSendBuff[0];
	Msensors_Spi_Send(st, &SpiSendBuff[0], &SpiRecvBuff[0], WRITE_DATA_SIZE);

	while (!kthread_should_stop()) {
		next_recv_size = SUB_COM_TYPE_SIZE;
		send_type = SUB_COM_TYPE_RES_NOMAL;
		memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, SPI_DATA_MAX);

		/* Get Type */
		type = recv_buf[SUB_COM_HEAD_INDEX_TYPE];

		/* HEADER Information */
		if (type == SUB_COM_TYPE_FW_REQ_DATA) {
			SpiSendBuff[0] = SUB_COM_TYPE_SET_FW_DATA;
			memcpy(&SpiSendBuff[1], buf2, len);
			PreSendType = SpiSendBuff[0];
			Msensors_Spi_Send(st, &SpiSendBuff[0], &SpiRecvBuff[0], 1 + len);
			dev_dbg(&st->sdev->dev, "Snd Data = %s \n",buf2);
		} else if(type == SUB_COM_TYPE_FW_REQ_HEAD) {
			oldfs = get_fs();
			set_fs(get_ds());
			readsize = filp->f_op->read(filp, buf, (sizeof(buf)-1), &filp->f_pos);
			set_fs(oldfs);

			if (readsize == 0) {
				dev_dbg(&st->sdev->dev, "FWUpdate Complete!!!!\n");
				filp_close(filp, NULL);
				ioctl_complete = 1;
				wake_up_interruptible(&wait_ioctl);
				return 0;
			}

			t = memchr(buf, '\n', readsize);
			if (t != NULL) {
				len = ++t - buf;
				memset(buf2, 0x00, sizeof(buf2));
				memcpy(buf2, buf, len);
				filp->f_pos = (filp->f_pos - (readsize - len));
			}

			Now_len++;

			memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
			SpiSendBuff[0] = SUB_COM_TYPE_SET_FW_SIZE;
			SpiSendBuff[1] = (unsigned char)(len & 0x00FF);
			SpiSendBuff[2] = (unsigned char)((len & 0xFF00) >> 8);
			SpiSendBuff[3] = (unsigned char)(Now_len & 0x00FF);
			SpiSendBuff[4] = (unsigned char)((Now_len & 0xFF00) >> 8);
			SpiSendBuff[5] = (unsigned char)(Totallen & 0x00FF);
			SpiSendBuff[6] = (unsigned char)((Totallen & 0xFF00) >> 8);

			PreSendType =  SpiSendBuff[0];
			Msensors_Spi_Send(st, &SpiSendBuff[0], &SpiRecvBuff[0], WRITE_DATA_SIZE);
			dev_dbg(&st->sdev->dev, "FWUp (%04d/%04d)line \n",Now_len,Totallen);
		} else {
			dev_dbg(&st->sdev->dev, "FWUp RecvDeta Error (%04d/%04d)line \n",Now_len,Totallen);
		}
	}
	return 0;
}

static ssize_t Msensors_Read( struct file* file, char* buf, size_t count, loff_t* offset )
{
	int cnt=0;
	char* tmp_buf;
	int buf_index=0;
	int size = sizeof(struct Msensors_data);
	int ret;
	int data_num = count / sizeof(struct Msensors_data);
	struct Msensors_state *st;

	st = file->private_data;

	tmp_buf = kzalloc(sizeof(struct Msensors_data) * count, GFP_KERNEL);
	if (tmp_buf == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	/* Wait until dataBuffWriteIndex moved */
	wait_event_interruptible(wait_rd, dataBuffWriteIndex != dataBuffReadIndex);
	for (cnt = 0; cnt < data_num; cnt++) {
		if( dataBuffWriteIndex == dataBuffReadIndex )
			break; 	/* no data */
		memcpy(&tmp_buf[buf_index], &Msensors_data_buff[dataBuffReadIndex], size);
		buf_index += size;
		INC_INDEX(dataBuffReadIndex, MSENSORS_DATA_MAX);
	}
	ret = copy_to_user(buf, tmp_buf, buf_index);
	if (ret < 0) {
		kfree(tmp_buf);
		goto error_ret;
	}
	ret = buf_index;

	kfree(tmp_buf);

error_ret:
	return ret;
}


static ssize_t Msensors_Write( struct file* file, const char* buf, size_t count, loff_t* offset )
{
	struct Msensors_state *st;
	int ret;
	unsigned char write_buff[WRITE_DATA_SIZE];

	st = file->private_data;
	dev_dbg(&st->sdev->dev, "Msensors_Write Start count=%d\n", count);

	ret = copy_from_user(&write_buff[1], buf, count);

	if (!ret) {
		write_buff[0] = SUB_COM_TYPE_WRITE;	//0xA1
		Set_WriteDataBuff(&write_buff[0]);
		if (write_buff[1] == SUB_COM_SETID_FIFO_FLUSH) {
			Msensors_data_buff[0].sensor_type = MSENSORS_TYPE_META;
			Msensors_data_buff[0].handle = write_buff[2];
			dataBuffReadIndex = 0;
			dataBuffWriteIndex = 1;
		}
	}

	/* [for demo] if pnlcd on/off command is issued, backlight off/on */
	if (write_buff[1] == SUB_COM_SETID_DEMO_CMD &&
			write_buff[2] == SUB_LCD_CONTROL) {
		if (write_buff[3] & SUB_LCD_ONOFF_MASK) {
			msleep(300);
			on_off_light_for_pnlcd_demo(0);
		} else
			on_off_light_for_pnlcd_demo(1);
	}

	return ret;
}

static int ioctl_fwupdate(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct Msensors_state *st;
	int ret;
	int tmp_interval;
	unsigned char write_buff[WRITE_DATA_SIZE];
	struct file* filp=NULL;
	int readsize=1;
	char buf[256] = {0};
	char buf2[256] = {0};
	char* t;
	int len=0;
	mm_segment_t oldfs;

		st = file->private_data;

		/* Open the file and count lines */
		oldfs = get_fs();
		set_fs(get_ds());
		filp = filp_open(path, O_RDONLY, 0);
		set_fs(oldfs);
		if (IS_ERR(filp)) {
			dev_dbg(&st->sdev->dev, "File Open Error\n");
			return -1;
		}

		/* Read line by line */
		Totallen = 0;
		readsize = 1;
		while (readsize != 0) {
			oldfs = get_fs();
			set_fs(get_ds());
			readsize = filp->f_op->read(filp, buf, (sizeof(buf)-1), &filp->f_pos);
			set_fs(oldfs);
			if (readsize != 0) {
				t = memchr(buf, '\n', readsize);
				if (t != NULL) {
					len = ++t - buf;
					memset(buf2, 0x00, sizeof(buf2));
					memcpy(buf2, buf, len);
					filp->f_pos = (filp->f_pos - (readsize - len));
				}
				Totallen++;
			}
		}
		filp_close(filp, NULL);

		/* Request FW update to SUBCPU */
		memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
		ret = copy_from_user((unsigned char*)&tmp_interval, (unsigned char*)arg, sizeof(int));

		write_buff[0] = SUB_COM_TYPE_WRITE;
		write_buff[1] = SUB_COM_SETID_SUB_FIRM_UPDATE;
		ioctl_complete = 0;
		Set_WriteDataBuff(&write_buff[0]);
		wait_event_interruptible(wait_ioctl, ioctl_complete == 1);

	return ret;
}

static void sub_read_command(uint8_t command)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	write_buff[0] = SUB_COM_TYPE_READ;
	write_buff[1] = command;
	Set_WriteDataBuff(&write_buff[0]);
	ioctl_complete = 0;
	wait_event_interruptible(wait_ioctl, ioctl_complete == 1);
}

static long Msensors_Ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	pr_info("%s: cmd %d\n", __func__, cmd);
	if (cmd == IOC_FW_UPDATE) {
		ret = ioctl_fwupdate(file, cmd, arg);
	} else if (cmd == IOC_ACCEL_ADJ) {
		/* accelerometer factory adjustment */
		sub_read_command(SUB_COM_GETID_ACC_ADJ);
		ret = copy_to_user((void __user *)arg, SubReadData, 3);
	} else if (cmd == IOC_GET_VERSION) {
		ret = copy_to_user((void __user *)arg, SubCpuVersion, 3);
	} else {
		ret = -1;
		pr_info("%s: unknown command %x\n", __func__, cmd);
	}
	return ret;
}

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
};

static void Msensors_init(struct Msensors_state *st)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	SpiErrorCount = 0;

	memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, SPI_DATA_MAX);
	SpiSendBuff[0] = SUB_COM_TYPE_WRITE;
	SpiSendBuff[1] = SUB_COM_SETID_MAIN_STATUS;
	SpiSendBuff[2] = 0x0;	/* Nomal */
	PreSendType =  SpiSendBuff[0];
	Msensors_Spi_Send(st, &SpiSendBuff[0], &SpiRecvBuff[0], WRITE_DATA_SIZE);

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);
	write_buff[0] = SUB_COM_TYPE_READ;
	write_buff[1] = SUB_COM_GETID_SUB_CPU_VER;
	Set_WriteDataBuff(&write_buff[0]);
}

static struct wake_lock wlock;
static irqreturn_t sub_main_int_wake_isr(int irq, void *dev)
{
	sub_main_int_occur = 1;
	wake_up_interruptible(&wait_subint);
	wake_lock_timeout(&wlock, HZ/5);
	return IRQ_HANDLED;
}

static int Msensors_probe(struct spi_device *spi)
{
	int ret, err;
	int rc, irq;
	struct Msensors_state *st;

	dev_dbg(&spi->dev, "Msensors_probe Start\n");

	gSUBCPU_Firmware = kzalloc(sizeof(SUBCPU_Firmware), GFP_KERNEL);
	memcpy(gSUBCPU_Firmware, SUBCPU_Firmware, sizeof(SUBCPU_Firmware));

	init_waitqueue_head(&wait_rd);
	init_waitqueue_head(&wait_subint);
	init_waitqueue_head(&wait_ioctl);
	spin_lock_init(&slock);
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

	spi->max_speed_hz = 1024 * 1024;
	spi->mode = SPI_MODE_1;	/* CPHA=1 CPOL=0 */
	spi->bits_per_word = 8;
	spi_setup(spi);

	st->sub_main_int = of_get_named_gpio(spi->dev.of_node,	"nsw,sub_main_int", 0);

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

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		gpio_free(st->sub_main_int);
		return ret;
	}

	add_sysfs_interfaces(&spi->dev);

	disable_irq_wake(irq);

	Flg_driver_ready = 1;
	Flg_driver_probed = 1;
	Msensors_init(st);

	pSensorReadThread = kthread_run(SensorReadThread, st,DRV_NAME);
	if (IS_ERR(pSensorReadThread)) {
		ret = PTR_ERR(pSensorReadThread);
		goto error_free_dev;
	}

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

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x2;	/* shutdown */

	Set_WriteDataBuff(&write_buff[0]);

	while (!list_empty(&wd_queue))
		msleep(10);

//	kthread_stop(pSensorReadThread);
	Flg_driver_shutdown = 1;
	wake_up_interruptible(&wait_subint);
}

static int Msensors_suspend(struct device *dev)
{
	int irq;	/* for SUB_MAIN_INT resumu */
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x1;	/* suspend */

	Set_WriteDataBuff(&write_buff[0]);
	while (!list_empty(&wd_queue))
		msleep(10);

	Flg_driver_ready = 0;

	/* for SUB_MAIN_INT resume */
	irq = gpio_to_irq(g_st->sub_main_int);
	enable_irq_wake(irq);

	return 0;
}

static int Msensors_resume(struct device *dev)
{
	unsigned char write_buff[WRITE_DATA_SIZE];
	int irq;	/* for SUB_MAIN_INT resumu */

	/* for SUB_MAIN_INT resume */
	irq = gpio_to_irq(g_st->sub_main_int);
	disable_irq_wake(irq);

	Flg_driver_ready = 1;
	/* Write Buffer Clear for Resume */
	dataBuffReadIndex = 0;
	dataBuffWriteIndex = 0;

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x0;	/* Nomal */

	Set_WriteDataBuff(&write_buff[0]);

	return 0;
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
	.resume = Msensors_resume,
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

	Set_WriteDataBuff(&write_buff[0]);
}

int SUBCPU_rtc_read_time(uint8_t *data)
{
	/* if RTC was not set, return error */
	if (HeaderData[3] == 0) return 1;

	data[RTC_YEAR]  = (uint8_t)(HeaderData[3] >> 1);
	data[RTC_MONTH] = (uint8_t)(((HeaderData[3] & 0x01) << 3) |
			                    ((HeaderData[4] & 0xE0) >> 5));
	data[RTC_DATE]  = (uint8_t)(HeaderData[4] & 0x1F);
	data[RTC_WEEKDAY] = BIT((uint8_t)(HeaderData[5] >> 5));
	data[RTC_HOUR]  = (uint8_t)(HeaderData[5] & 0x1F);
	data[RTC_MIN]   = HeaderData[6];
	data[RTC_SEC]   = HeaderData[7];

	return 0;
}

int SUBCPU_rtc_set_time(uint8_t *data)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_RTC;
	write_buff[3]  = ( data[RTC_YEAR] << 1 );
	write_buff[3] |= ( data[RTC_MONTH] >> 3 ) & 0x01;
	write_buff[4]  = ( data[RTC_MONTH] & 0x07 ) << 5;
	write_buff[4] |= ( data[RTC_DATE] & 0x1F );
	write_buff[5]  = ( (__fls(data[RTC_WEEKDAY])) << 5 );
	write_buff[5] |= ( data[RTC_HOUR] & 0x1F );
	write_buff[6]  = data[RTC_MIN];
	write_buff[7]  = data[RTC_SEC];

	Set_WriteDataBuff(&write_buff[0]);

	return 0;
}

int SUB_IsTheaterMode(unsigned char flg_theater_mode)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_THEATER_MODE;
	write_buff[2] = flg_theater_mode;	/* theater mode:1 else:0 */

	Set_WriteDataBuff(&write_buff[0]);
	return 0;
}

#ifdef CONFIG_BACKLIGHT_SUBCPU
int SUB_LCDBrightnessSet(unsigned char LCDBrightness)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_LCD_SET;
	write_buff[2] = LCDBrightness;	/* LCD brightness */

	return Set_WriteDataBuff(&write_buff[0]);
}
#endif

MODULE_AUTHOR("PET");
MODULE_DESCRIPTION("MultiSensors driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:" DRV_NAME);

