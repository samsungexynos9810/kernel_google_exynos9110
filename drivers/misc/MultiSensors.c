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

#include "../power/subcpu_battery.h"
#include "../video/backlight/bd82103.h"
#include "MSensorsDrv.h"
#include "MultiSensors_FW.h"

#define DRV_NAME	"MultiSensors"
#define DRV_NAME_CD 	"MultiSensors_CD"
#define DRV_NAME_CD_01 	"MultiSensors_CD_01"

#define MINOR_COUNT 	(1)

#define INTERVAL_STARTUP	(5000)
#define INTERVAL_MAX		(MSENSORS_BATCH_MAX_TIME_NS / 1000000)		/* ns -> ms */
#define INTERVAL_MIN		(100)
#define INTERVAL_INIT		(1000)
#define INTERVAL_SET		(30)
#define INTERVAL_SUBMIT		(70)
#define SPI_ERROR_COUNT_MAX	(5)

#define SUB_COM_OVERWRITE (1)
#define SUB_COM_NON_OVERWRITE	(0)

#define SPI_DATA_MAX (SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE + SUB_COM_DATA_SIZE_GETDATA + (SUB_COM_MAX_PACKET * ( SUB_COM_DATA_SIZE_PACKET + SUB_COM_ID_SIZE )))

#define INC_INDEX(a, r)	\
do {	\
	if ((a) < ((r)-1))	\
		(a)++;	\
	else		\
		(a)=0;	\
} while(0)

struct Msensors_state {
	struct mutex lock;
	struct spi_device *sdev;
	struct cdev cdev;
	unsigned int sub_main_int;
};


typedef struct {
	unsigned char	m_type;
	unsigned char	m_id;
	unsigned char	m_over;
}STR_PriorityTbl;

static STR_PriorityTbl Write_Priority_Tbl[] = {
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_DEMO_CMD,		SUB_COM_NON_OVERWRITE },/* Demo Command */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_VIB_SET,		SUB_COM_NON_OVERWRITE },/* Vibrator Settiong */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_SUB_FIRM_UPDATE,	SUB_COM_OVERWRITE },	/* SUB-CPU Firmware Update */
	{ SUB_COM_TYPE_READ, 	SUB_COM_GETID_RTC,		SUB_COM_OVERWRITE },	/* RTC */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_RTC,		SUB_COM_OVERWRITE },	/* RTC */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_MAIN_STATUS,	SUB_COM_OVERWRITE },	/* MAIN status */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_THEATER_MODE,	SUB_COM_OVERWRITE },	/* MAIN LCD Brightness is 0 or not */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_LCD_SET,		SUB_COM_OVERWRITE },	/* LCD Brightness Settiong */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_KEYMODE,		SUB_COM_OVERWRITE },	/* Key Mode */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_SENSOR_PERIOD,	SUB_COM_OVERWRITE },	/* Sensor Data Send Period */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_FIFO_FLUSH,	SUB_COM_OVERWRITE },	/* FIFO flash */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_HZ_ACCELE,	SUB_COM_OVERWRITE },	/* Acceleration Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_HZ_MAG,		SUB_COM_OVERWRITE },	/* Magnetic Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_HZ_GYRO,		SUB_COM_OVERWRITE },	/* Gyroscope Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_HZ_PRESS,		SUB_COM_OVERWRITE },	/* Pressure Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_HZ_BHA,	SUB_COM_OVERWRITE },	/* BHA Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_HZ_STPCOUNTER,	SUB_COM_OVERWRITE },	/* Step Counter Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_HZ_STPDETECTOR,	SUB_COM_OVERWRITE },	/* Step Detector Hz */

	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BATCH_ACCELE,	SUB_COM_OVERWRITE },	/* Acceleration Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BATCH_MAG,		SUB_COM_OVERWRITE },	/* Magnetic Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BATCH_GYRO,		SUB_COM_OVERWRITE },	/* Gyroscope Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BATCH_PRESS,		SUB_COM_OVERWRITE },	/* Pressure Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BATCH_BHA,	SUB_COM_OVERWRITE },	/* BHA Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BATCH_STPCOUNTER,	SUB_COM_OVERWRITE },	/* Step Counter Hz */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BATCH_STPDETECTOR,	SUB_COM_OVERWRITE },	/* Step Detector Hz */

	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_ACCELE,	SUB_COM_OVERWRITE },	/* Acceleratio Non/OFF */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_MAG,	SUB_COM_OVERWRITE },	/* Magnetic ON/OFF */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_GYRO,	SUB_COM_OVERWRITE },	/* Gyroscope ON/OFF */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_PRESS,	SUB_COM_OVERWRITE },	/* Pressure ON/OFF */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_BHA,	SUB_COM_OVERWRITE },	/* BHA ON/OFF */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_STPCOUNTER,	SUB_COM_OVERWRITE },	/* Step Counter ON/OFF */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_STPDETECTOR,SUB_COM_OVERWRITE },	/* Step Detector ON/OFF */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ONOFF_WRIST_TILT,	SUB_COM_OVERWRITE },	/* Wrist Tilt ON/OFF */

	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_ALERM,		SUB_COM_OVERWRITE },	/* Alarm */
	{ SUB_COM_TYPE_WRITE, 	SUB_COM_SETID_BUZ_SET,		SUB_COM_NON_OVERWRITE },/* Buzzer Settiong */
	{ SUB_COM_TYPE_READ, 	SUB_COM_GETID_SUB_CPU_VER,	SUB_COM_OVERWRITE },	/* SUB-CPU Version */
	{ SUB_COM_TYPE_READ, 	SUB_COM_GETID_POWER_PROP,		SUB_COM_OVERWRITE },	/* Battery % */
	{ SUB_COM_TYPE_READ, 	SUB_COM_GETID_USB,		SUB_COM_OVERWRITE },	/* USB Status */
};

#define HEADER_DATA_SIZE ( SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE +SUB_COM_HEAD_SIZE_SETDATA )
#define WRITE_DATA_SIZE ( SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE + SUB_COM_HEAD_SIZE_SETDATA)
#define WRITE_DATABUFF_SIZE ( sizeof(Write_Priority_Tbl) / sizeof(STR_PriorityTbl) )

typedef void (*Write_cb)(void);

typedef struct {
	unsigned char	m_data[WRITE_DATA_SIZE];
	unsigned char	m_sendcomp;
	Write_cb	m_cb;
}STR_WriteTbl;

STR_WriteTbl WriteDataBuf[WRITE_DATABUFF_SIZE];
atomic_t WriteDataNum;

static struct Msensors_state 	*g_st=0;
static dev_t 			dev_id;
static struct class		*Msensors_class;
static struct device		*class_dev;
static struct task_struct	*pSensorReadThread;
static unsigned int		sensorReadThreaad_interval=(INTERVAL_INIT - INTERVAL_SUBMIT);
static unsigned int		request_interval=0;

static struct Msensors_data	Msensors_data_buff[MSENSORS_DATA_MAX];
static unsigned int		dataBuffReadIndex;
static unsigned int		dataBuffWriteIndex;
static unsigned int		PacketDataNum=0;
static unsigned int		PreSendType=0;
static unsigned int		SpiDataLen=0;
static unsigned int		SpiErrorCount=0;	/*** pet yasui ADD ***/
       unsigned char		HeaderData[HEADER_DATA_SIZE];
static unsigned char 		SpiSendBuff[SPI_DATA_MAX];
static unsigned char 		SpiRecvBuff[SPI_DATA_MAX];
static unsigned char		ThreaadRunFlg=0;
static unsigned char		HeaderRcvFlg=0;
static unsigned char		SetCommandOnly=0;
static unsigned char		Flg_driver_ready = 0;

/*** for SubCpuEdit.c ***/
unsigned char SubCpuVersion[SUB_COM_DATA_SIZE_GETDATA];
unsigned char SubUsbStatus[SUB_COM_DATA_SIZE_GETDATA];
unsigned char SubDateTime[SUB_COM_DATA_SIZE_GETDATA];
/*** for SubCpuEdit.c ***/


/** FWUpdate **/
static const char path[] = "/data/local/tmp/SUB_CPU_TZ.srec";
static	int Totallen = 0;
static	int Now_len = 1;
static uint8_t gFlg_AutoUpdate = 0;
static wait_queue_head_t wait_fwu;
static int fwupdate_complete;
static struct sub_FW_data *gSUBCPU_Firmware;
/**************/

static void sub_HeaderInfoProc(void);
static int SensorFWUpdateLoop(void);
static int Msensors_resume(struct device *dev);//TEMS.198 2015.01.24 anzai
int Set_WriteDataBuff( unsigned char* write_buff, Write_cb cb );

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

	if((tx_type == SUB_COM_TYPE_WRITE) || (tx_type == SUB_COM_TYPE_READ))
	{
		xfer.cs_change = 1;
	}

	SpiDataLen = (unsigned int)count;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret_spi = spi_sync(st->sdev, &msg);
	if (ret_spi)
	{
		ret = -EIO;
	}

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

static int Get_WriteDataBuffIdx( void )
{
	int i;

	for (i = 0; i < WRITE_DATABUFF_SIZE; i++) {
		if (WriteDataBuf[i].m_sendcomp == 1)
			return i;
	}
	return -1;
}

static int64_t getTimestamp(void)
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    ktime_get_ts(&t);
    return (int64_t)t.tv_sec * 1000000000LL + t.tv_nsec;
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

static void start_subcpu_update(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_SUB_FIRM_UPDATE;
	Set_WriteDataBuff(&write_buff[0], (void*)0);
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

static wait_queue_head_t wait;
static wait_queue_head_t wait_subint;
static int sub_main_int_occur;
static spinlock_t slock;

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
	int64_t soc_time, event_time;
	uint32_t elapsed_time = 0;
	int bidx;
	unsigned long flags;
	static uint8_t flg_first = 1;

	ThreaadRunFlg = 1;
	HeaderRcvFlg = 0;

	spin_lock_init(&slock);

	request_interval=(INTERVAL_INIT - INTERVAL_SUBMIT);

	while (!kthread_should_stop()) {

		next_recv_size = SUB_COM_TYPE_SIZE;
		send_type = SUB_COM_TYPE_RES_NOMAL;
		memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, SPI_DATA_MAX);

		/* Get Type */
		type = recv_buf[SUB_COM_HEAD_INDEX_TYPE];

		/* HEADER Information */
		if(type == SUB_COM_TYPE_BIT_HEAD )
		{
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
			if(PreSendType==SUB_COM_TYPE_READ)
			{
				next_recv_size += SUB_COM_DATA_SIZE_GETDATA + SUB_COM_ID_SIZE;
				if(PacketDataNum == 0)
					send_type = SUB_COM_TYPE_GETDATA;
				else
					send_type = SUB_COM_TYPE_SENSOR_GETDATA;
			}
			else
			{
				if(PacketDataNum == 0)
					send_type = SUB_COM_TYPE_RES_NOMAL;
				else
					send_type = SUB_COM_TYPE_SENSOR;
			}
		}
		/* FW Update Response */
		else if(type == SUB_COM_TYPE_FWUP_READY)
		{
			/* Thread Start */
			if (gFlg_AutoUpdate) {
				gFlg_AutoUpdate = 0;
				SensorFWUpdateLoop_auto();
			} else {
				SensorFWUpdateLoop();
			}
		}
		/* Data */
		else
		{
			recv_index = SUB_COM_DATA_INDEX_ID;
			if(( type == SUB_COM_TYPE_GETDATA ) ||		/* Get Data Only */
			   ( type == SUB_COM_TYPE_SENSOR_GETDATA ))	/* Get Data and Sensor Data */
			{
				if (recv_buf[recv_index] == SUB_COM_GETID_SUB_CPU_VER)
				{
					memcpy(SubCpuVersion, &recv_buf[recv_index + 1], SUB_COM_DATA_SIZE_GETDATA);
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
				}
				else if (recv_buf[recv_index] == SUB_COM_GETID_POWER_PROP)
				{
					subcpu_battery_update_status(recv_buf);
				}
				else if (recv_buf[recv_index] == SUB_COM_GETID_USB)
				{
					memcpy(SubUsbStatus, &recv_buf[recv_index + 1], SUB_COM_DATA_SIZE_GETDATA);
				}
				else if (recv_buf[recv_index] == SUB_COM_GETID_RTC)
				{
					memcpy(SubDateTime, &recv_buf[recv_index + 1], SUB_COM_DATA_SIZE_GETDATA);
				}

				/* Get Data Proc */
				recv_index += SUB_COM_DATA_SIZE_GETDATA + SUB_COM_ID_SIZE;
			}


			if(( type == SUB_COM_TYPE_SENSOR ) ||		/* Sensor Data Only */
			   ( type == SUB_COM_TYPE_SENSOR_GETDATA ))	/* Get Data and Sensor Data */
			{
				soc_time = getTimestamp();
				event_time = soc_time - elapsed_time * 1000000LL;
				/* Sensor Data Proc */
				for ( cnt= 0; cnt < PacketDataNum; cnt++ )
				{
					sensor_type = recv_buf[recv_index++];

					if (sensor_type == 0) {
						elapsed_time = recv_buf[recv_index+3]<<24 | recv_buf[recv_index+2]<<16 |
										recv_buf[recv_index+1]<<8 | recv_buf[recv_index];
						event_time = soc_time - elapsed_time * 1000000LL;
					} else {
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
				wake_up_interruptible_sync(&wait);
			}
		}

		bidx = -1;
		if( send_type == SUB_COM_TYPE_RES_NOMAL )
		{
			next_recv_size += SUB_COM_HEAD_SIZE_SETDATA + SUB_COM_ID_SIZE;
			bidx = Get_WriteDataBuffIdx();
			if (bidx != -1) {
				send_type = WriteDataBuf[bidx].m_data[0];
				memcpy(&SpiSendBuff[0], &WriteDataBuf[bidx].m_data[0], WRITE_DATA_SIZE);
			}
		}
		/* Next SPI Send Proc */
		SpiSendBuff[SUB_COM_HEAD_INDEX_TYPE] = send_type;
		PreSendType =  send_type;
		if (send_type == SUB_COM_TYPE_RES_NOMAL) {
			wait_event_interruptible(wait_subint, sub_main_int_occur);
			sub_main_int_occur = 0;
		}
		Msensors_Spi_Send(st, &SpiSendBuff[0], &SpiRecvBuff[0], next_recv_size);
		if (bidx != -1) {
			if (WriteDataBuf[bidx].m_cb)
				WriteDataBuf[bidx].m_cb();
			atomic_dec(&WriteDataNum);
			WriteDataBuf[bidx].m_sendcomp = 0;
		}
	}
	ThreaadRunFlg = 0;

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

	if(IS_ERR(filp))
	{
		dev_dbg(&st->sdev->dev, "File Open Error\n");
		return 0;
	}
	else
	{
		oldfs = get_fs();
		set_fs(get_ds());
		readsize = filp->f_op->read(filp, buf, (sizeof(buf)-1), &filp->f_pos);
		set_fs(oldfs);

		t = memchr(buf, '\n', readsize);
		if(t != NULL)
		{
			len = ++t - buf;
			memset(buf2, 0x00, sizeof(buf2));
			memcpy(buf2, buf, len);
			filp->f_pos = (filp->f_pos - (readsize - len));
		}
	}

	Now_len = 1; //init

	if(buf2[0] != 'S')
	{
		dev_dbg(&st->sdev->dev, "Error Not S-Record %d \n", buf2[0]);
	}

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

	request_interval=(INTERVAL_INIT - INTERVAL_SUBMIT);

	while (!kthread_should_stop()) {
		next_recv_size = SUB_COM_TYPE_SIZE;
		send_type = SUB_COM_TYPE_RES_NOMAL;
		memset(&SpiSendBuff[0], SUB_COM_SEND_DUMMY, SPI_DATA_MAX);

		/* Get Type */
		type = recv_buf[SUB_COM_HEAD_INDEX_TYPE];

		/* HEADER Information */
		if(type == SUB_COM_TYPE_FW_REQ_DATA)
		{
			SpiSendBuff[0] = SUB_COM_TYPE_SET_FW_DATA;
			memcpy(&SpiSendBuff[1], buf2, len);
			PreSendType = SpiSendBuff[0];
			Msensors_Spi_Send(st, &SpiSendBuff[0], &SpiRecvBuff[0], 1 + len);
			dev_dbg(&st->sdev->dev, "Snd Data = %s \n",buf2);
		}
		else if(type == SUB_COM_TYPE_FW_REQ_HEAD)
		{
			oldfs = get_fs();
			set_fs(get_ds());
			readsize = filp->f_op->read(filp, buf, (sizeof(buf)-1), &filp->f_pos);
			set_fs(oldfs);

			if(readsize == 0)
			{
				dev_dbg(&st->sdev->dev, "FWUpdate Complete!!!!\n");
				filp_close(filp, NULL);
				fwupdate_complete = 1;
				wake_up_interruptible(&wait_fwu);

				/* Communicate with SUB_CPU */
				/* Thread Start */
			//	(void)Msensors_resume(NULL);//TEMS.198 2015.01.24 anzai

				return 0;
			}

			t = memchr(buf, '\n', readsize);
			if(t != NULL)
			{
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
		}
		else
		{
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
	wait_event_interruptible(wait, dataBuffWriteIndex != dataBuffReadIndex);
	for(cnt=0; cnt<data_num; cnt++)
	{
		if( dataBuffWriteIndex == dataBuffReadIndex )
			break; 	/* no data */
		memcpy(&tmp_buf[buf_index], &Msensors_data_buff[dataBuffReadIndex], size);
		buf_index += size;
		INC_INDEX(dataBuffReadIndex, MSENSORS_DATA_MAX);
	}
	ret = copy_to_user(buf, tmp_buf, buf_index);
	if(ret<0)
        {
	        kfree(tmp_buf);
		goto error_ret;
        }
	ret = buf_index;

	kfree(tmp_buf);

error_ret:
	return ret;
}

int Set_WriteDataBuff( unsigned char* write_buff, Write_cb cb )
{
	int i;

	if (!(Flg_driver_ready)) {
		printk("command ignored. MultiSensors is not ready.\n");
		return -EPROBE_DEFER;
	}

	mutex_lock(&g_st->lock);
	for(i=0; i<WRITE_DATABUFF_SIZE; i++)
	{
		if(( write_buff[0] == Write_Priority_Tbl[i].m_type ) &&
		   ( write_buff[1] == Write_Priority_Tbl[i].m_id ))
		{
			if ( WriteDataBuf[i].m_sendcomp == 1 ) {
				continue;
			}

			memcpy(&WriteDataBuf[i].m_data[0], &write_buff[0], WRITE_DATA_SIZE);
			WriteDataBuf[i].m_cb = cb;

			atomic_inc(&WriteDataNum);
			WriteDataBuf[i].m_sendcomp = 1;
			if( ThreaadRunFlg == 1 )
			{
				sub_main_int_occur = 1;
				wake_up_interruptible(&wait_subint);
			}
			break;
		}
	}
	mutex_unlock(&g_st->lock);
	return 0;
}

static ssize_t Msensors_Write( struct file* file, const char* buf, size_t count, loff_t* offset )
{
	struct Msensors_state *st;
	int ret;
	unsigned char write_buff[WRITE_DATA_SIZE];

	st = file->private_data;
	dev_dbg(&st->sdev->dev, "Msensors_Write Start count=%d\n", count);

	ret = copy_from_user(&write_buff[1], buf, count);

	/* [for demo] if pnlcd on/off command is issued, backlight off/on */
	if (write_buff[1] == SUB_COM_SETID_DEMO_CMD &&
			write_buff[2] == SUB_LCD_CONTROL) {
		if (write_buff[3] & SUB_LCD_ONOFF_MASK)
			on_off_light_for_pnlcd_demo(0);
		else
			on_off_light_for_pnlcd_demo(1);
	}

	if(!ret)
	{
		write_buff[0] = SUB_COM_TYPE_WRITE;	//0xA1
		Set_WriteDataBuff( &write_buff[0], (void*)0 );
		if( write_buff[1] == SUB_COM_SETID_FIFO_FLUSH )
		{
			Msensors_data_buff[0].sensor_type = MSENSORS_TYPE_META;
			Msensors_data_buff[0].handle = write_buff[2];
			dataBuffReadIndex = 0;
			dataBuffWriteIndex = 1;
		}
	}

	return ret;
}

void CB_Ioctl(void)
{
	sensorReadThreaad_interval = request_interval;
}

static long Msensors_Ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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

	if(cmd == 1)
	{
		st = file->private_data;

		/* Open the file and count lines */
		oldfs = get_fs();
		set_fs(get_ds());
		filp = filp_open(path, O_RDONLY, 0);
		set_fs(oldfs);
		if(IS_ERR(filp))
		{
			dev_dbg(&st->sdev->dev, "File Open Error\n");
			return -1;
		}

		/* Read line by line */
		Totallen = 0;
		readsize = 1;
		while(readsize != 0)
		{
			oldfs = get_fs();
			set_fs(get_ds());
			readsize = filp->f_op->read(filp, buf, (sizeof(buf)-1), &filp->f_pos);
			set_fs(oldfs);
			if(readsize != 0)
			{
				t = memchr(buf, '\n', readsize);
				if(t != NULL)
				{
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
		fwupdate_complete = 0;
		Set_WriteDataBuff(&write_buff[0], (void*)0);
		wait_event_interruptible(wait_fwu, fwupdate_complete == 1);
	} else {
		copy_to_user((void __user *)arg, SubCpuVersion, 3);
	}
	return 0;
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
	Set_WriteDataBuff( &write_buff[0], (void*)0 );

	request_interval=(INTERVAL_INIT - INTERVAL_SUBMIT);
}

static irqreturn_t sub_main_int_wake_isr(int irq, void *dev)
{
	sub_main_int_occur = 1;
	wake_up_interruptible(&wait_subint);

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

	init_waitqueue_head(&wait);
	init_waitqueue_head(&wait_subint);
	init_waitqueue_head(&wait_fwu);
	dataBuffReadIndex = 0;
	dataBuffWriteIndex = 0;
	memset((unsigned char*)&WriteDataBuf[0], 0x00, sizeof(WriteDataBuf));
	memset(&HeaderData[0], 0x00, HEADER_DATA_SIZE);

	SetCommandOnly = 0;

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
	mutex_init(&st->lock);

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

	disable_irq_wake(irq);

	Flg_driver_ready = 1;
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

	SetCommandOnly = 1;

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x2;	/* shutdown */

	Set_WriteDataBuff( &write_buff[0], (void *)0 );

	Flg_driver_ready = 0;
	while (atomic_read(&WriteDataNum) > 0)
	{
		msleep(10);
	}

	dev_dbg(&spi->dev, "Msensors_shutdown %d\n", ThreaadRunFlg);
}

static int Msensors_suspend(struct device *dev)
{
	int irq;	/* for SUB_MAIN_INT resumu */
	unsigned char write_buff[WRITE_DATA_SIZE];

	SetCommandOnly = 1;

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x1;	/* suspend */

	Set_WriteDataBuff( &write_buff[0], (void*)0 );
	Flg_driver_ready = 0;
	while (atomic_read(&WriteDataNum) > 0)
	{
		msleep(10);
	}

	/* for SUB_MAIN_INT resume */
	irq = gpio_to_irq(g_st->sub_main_int);
	enable_irq_wake(irq);

	return 0;
}

static int Msensors_resume(struct device *dev)
{
	unsigned char write_buff[WRITE_DATA_SIZE];
	int irq;	/* for SUB_MAIN_INT resumu */

	SetCommandOnly = 0;

	/* for SUB_MAIN_INT resume */
	irq = gpio_to_irq(g_st->sub_main_int);
	disable_irq_wake(irq);

	Flg_driver_ready = 1;
	/* Write Buffer Clear for Resume */
	dataBuffReadIndex = 0;
	dataBuffWriteIndex = 0;
	memset((unsigned char*)&WriteDataBuf[0], 0x00, sizeof(WriteDataBuf));

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_MAIN_STATUS;
	write_buff[2] = 0x0;	/* Nomal */

	Set_WriteDataBuff( &write_buff[0], (void*)0 );

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

static void sub_HeaderInfoProc(void)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	/* charger or battery property changed */
	if (HeaderData[2] & (1 << SUB_ALERT_BIT_POWER_CHG))
	{
		write_buff[0] = SUB_COM_TYPE_READ;
		write_buff[1] = SUB_COM_GETID_POWER_PROP;
		Set_WriteDataBuff( &write_buff[0], (void*)0 );
	}
}

int SUB_VibratorEnable(int enable)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_VIB_SET;
	write_buff[2] = enable;	/* on / off */

	Set_WriteDataBuff( &write_buff[0], (void*)0 );
	return 0;
}

void SUB_VibratorSet(int timeout)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_VIB_SET;
	write_buff[2] = (timeout >> 0) & 0xFF;	/* ms */
	write_buff[3] = (timeout >> 8) & 0xFF;	/* ms */

	Set_WriteDataBuff( &write_buff[0], (void*)0 );
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

	Set_WriteDataBuff( &write_buff[0], (void*)0 );

	return 0;
}

int SUB_KeyModeChange(unsigned char KeyMode)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_KEYMODE;
	write_buff[2] = KeyMode;	/* key mode */

	Set_WriteDataBuff( &write_buff[0], (void*)0 );
	return 0;
}

int SUB_IsTheaterMode(unsigned char flg_theater_mode)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_THEATER_MODE;
	write_buff[2] = flg_theater_mode;	/* theater mode:1 else:0 */

	Set_WriteDataBuff( &write_buff[0], (void*)0 );
	return 0;
}

int SUB_LCDBrightnessSet(unsigned char LCDBrightness)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_LCD_SET;
	write_buff[2] = LCDBrightness;	/* LCD brightness */

	return Set_WriteDataBuff( &write_buff[0], (void*)0 );
}

int SUB_DemoCommandSend(unsigned char* demo_cmd)
{
	unsigned char write_buff[WRITE_DATA_SIZE];

	memset(write_buff, SUB_COM_SEND_DUMMY, WRITE_DATA_SIZE);

	write_buff[0] = SUB_COM_TYPE_WRITE;
	write_buff[1] = SUB_COM_SETID_DEMO_CMD;
	memcpy(&write_buff[2], &demo_cmd[0], DEMO_CMD_DATA_MAX);	/* Demo command */

	Set_WriteDataBuff( &write_buff[0], (void*)0 );
	return 0;
}

MODULE_AUTHOR("PET");
MODULE_DESCRIPTION("MultiSensors driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:" DRV_NAME);

