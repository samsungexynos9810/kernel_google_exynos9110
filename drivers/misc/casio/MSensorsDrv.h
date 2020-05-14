/*
 * Driver for MultiSensors
 *
 * If you change this file, you must change the following header file.
 * ${ANDORID_DIR}/device/casio/koi/sensors/MultiSensors.h
 */
#ifndef __MULTISENSORS_H
#define __MULTISENSORS_H
/* PET nishino ADD Start */
/*============================================================================*/
/* include */
/*============================================================================*/
#include <linux/types.h>
#include <linux/cdev.h>

/*============================================================================*/
/* define */
/*============================================================================*/
#define MSENSORS_DATA_MAX			(16384)
#define MSENSORS_BATCH_MAX_TIME_NS		(10000000000)	/* 10sec */

#define SUB_COM_TYPE_RES_NOMAL			(0xA0)		/* Nomal Response */
#define SUB_COM_TYPE_WRITE			(0xA1)		/* Write Request */
#define SUB_COM_TYPE_READ			(0xA2)		/* Read Request */
#define SUB_COM_TYPE_SET_FW_DATA		(0xAE)		/* Set SUBCPU FW Data */
#define SUB_COM_TYPE_SET_FW_SIZE		(0xAF)		/* Set SUBCPU FW Size */
#define SUB_COM_TYPE_BIT_HEAD			(0x80)		/* HEADER Information */
#define SUB_COM_TYPE_FWUP_READY			(0x81)		/* SubCPU is ready for FWUpdate */
#define SUB_COM_TYPE_FW_RECV_SIZE		(0x40)		/* SubCPU Receive FW size */
#define SUB_COM_TYPE_FW_RECV_PKT		(0x8F)		/* SubCPU Receive FW pkt */
#define SUB_COM_TYPE_GETDATA			(0xF1)		/* GetData */
#define SUB_COM_TYPE_SENSOR			(0xF2)		/* Sensor Data(Get Data Non) */
#define SUB_COM_TYPE_SENSOR_GETDATA		(0xF3)		/* Sensor Data(Get Data True) */
#define SUB_COM_SETID_MAIN_STATUS		(0x01)		/* MAIN status */

#define SUB_COM_SETID_VIB_SET			(0x40)		/* Vibrator Settiong */
#define SUB_COM_SETID_BUZ_SET			(0x41)		/* Buzzer Settiong (no used) */
#define SUB_COM_SETID_LCD_SET			(0x42)		/* LCD Brightness Settiong */
#define SUB_COM_SETID_LOWTEMP_BURNOFF	(0x44)
#define SUB_COM_SETID_RTC			(0x50)		/* RTC */
#define SUB_COM_SETID_ALERM			(0x51)		/* Alarm */
#define SUB_COM_SETID_KEYMODE			(0x52)		/* Key Mode */
#define SUB_COM_SETID_SHIPPING_MODE	(0x54)		/* Request shipping mode */
#define SUB_COM_SETID_CTRL_GREEN_LED	(0x55)	/* on/off green led */
#define SUB_COM_SETID_LIMIT_CHARGING	(0x56)	/* limit charging by threshold */
#define SUB_COM_SETID_SUB_FIRM_UPDATE		(0x99)		/* SUB-CPU Firmware Update */
#define SUB_COM_SETID_SEG_CMD		(0xFE)		/* SegLcd Command */

#define SUB_COM_GETID_SUB_CPU_VER		(0x00)		/* SUB-CPU Version */
#define SUB_COM_GETID_POWER_PROP1		(0x01)		/* Power Property1 */
#define SUB_COM_GETID_POWER_PROP2		(0x02)		/* Power Property2 */
#define SUB_COM_GETID_I2C_STATUS		(0x03)
#define SUB_COM_GETID_ACC_ADJ		(0x04)		/* Accele Adjust */
#define SUB_COM_GETID_ACC_READ_OFFSET	(0x05)
#define SUB_COM_GETID_ACC_READ_GAIN		(0x06)
#define SUB_COM_GETID_POWER_WARN		(0x07)
#define SUB_COM_GETID_HEIGHT_CORR		(0x08)
#define SUB_COM_GETID_PRESSURE_CORR		(0x09)
#define SUB_COM_GETID_STEP_TODAY		(0x0a)
#define SUB_COM_GETID_STEP_YESTERDAY	(0x0b)
#define SUB_COM_GETID_BATTERY_LOG_START	(0x0c)
#define SUB_COM_GETID_BATTERY_LOG_GET_DATA	(0x0d)
#define SUB_COM_GETID_RTC_DATE_TIME		(0x0e)
#define SUB_COM_GETID_FG_VER	(0x0f)
#define SUB_COM_GETID_HEART_NOW		0x10
#define SUB_COM_GETID_PDLC_MODE		0x11
#define SUB_COM_GETID_CENTER_HEART	0x12
#define SUB_COM_GETID_BHA_NOW	0x13
#define SUB_COM_GETID_HRM_THER_AD	0x14
#define SUB_COM_GETID_UNIX_TIME	0x15
#define SUB_COM_GETID_AAC_REG_DATA	0x16
#define SUB_COM_GETID_NUM	(0x17)	/* Number of GETIDs */

#define MSENSORS_TYPE_META			0x00		/* Sensor Type Meta */
#define MSENSORS_TYPE_PPG			0x07
#define MSENSORS_TYPE_TIMESTAMP		0x1f		/* timestamp */
#define NORMAL_SENSOR_NUM	7

#define SUB_COM_HEAD_INDEX_TYPE			(0x00)		/* HEADER Information Index Type */
#define SUB_COM_HEAD_INDEX_ALART		(0x01)		/* HEADER Information Index Alart */

#define SUB_COM_TYPE_SIZE			(0x01)		/* Type Size */
#define SUB_COM_ID_SIZE				(0x01)		/* ID Size */

#define SUB_COM_HEAD_SIZE_SETDATA		(0x06)		/* Setting Data Size */

#define SUB_COM_DATA_INDEX_ID			(0x01)		/* Data Index ID */
#define SUB_COM_DATA_SIZE_GETDATA		(0x06)		/* Data Size GetData */
#define SUB_COM_DATA_SIZE_PACKET		(0x06)		/* Data Size Packet */
#define SUB_COM_DATA_SIZE_RTC			(0x05)		/* Data Size RTC */

#define SUB_COM_MAX_PACKET			9216		/* Packet MAX */

#define SUB_COM_SEND_DUMMY			(0xFF)		/* Send Dummy Data */

#define SUB_ALERT_BIT_POWER_CHG2		(0)		/* Power property changed2  */
#define SUB_ALERT_BIT_POWER_CHG1		(1)		/* Power property changed1 */


#define IOC_GET_VERSION	_IOR('K', 0, __u32)
#define IOC_ACCEL_ADJ	_IOR('K', 2, __u32)
#define IOC_POWER_WARN	_IOR('K', 3, __u32)
#define IOC_HEIGHT_CORR		_IOR('K', 5, __u32)
#define IOC_PRESSURE_CORR	_IOR('K', 6, __u32)
#define IOC_STEP_TODAY		_IOR('K', 7, __u32)
#define IOC_STEP_YESTERDAY	_IOR('K', 8, __u32)
#define IOC_BATTERY_LOG_START		_IOR('K', 9, __u32)
#define IOC_BATTERY_LOG_GET_DATA	_IOR('K', 10, __u32)
#define IOC_GET_BATTERY_VERSION _IOR('K', 11, __u32) /* Supported only on Kingyo. Please ignore this on other models. */
#define IOC_HEART_NOW	_IOR('K', 12, __u32)
#define IOC_PDLC_MODE	_IOR('K', 13, __u32)
#define IOC_CENTER_HEART	_IOR('K', 14, __u32)
#define IOC_BHA_NOW	_IOR('K', 15, __u32)
#define IOC_HRM_THER_AD	_IOR('K', 16, __u32)
#define IOC_ACC_REG_DATA	_IOR('K', 17, __u32)


#define SPI_DATA_MAX (SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE + SUB_COM_DATA_SIZE_GETDATA + (SUB_COM_MAX_PACKET * ( SUB_COM_DATA_SIZE_PACKET + SUB_COM_ID_SIZE )))
#define HEADER_DATA_SIZE ( SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE +SUB_COM_HEAD_SIZE_SETDATA )
#define WRITE_DATA_SIZE ( SUB_COM_TYPE_SIZE + SUB_COM_ID_SIZE + SUB_COM_HEAD_SIZE_SETDATA)
#define WRITE_DATABUFF_SIZE 32

enum {
	MSENSORS_FW_UP_IDLE = 0,
	MSENSORS_FW_UP_WAIT,
	MSENSORS_FW_UP_READY,
	MSENSORS_FW_UP_UPDATING,
	MSENSORS_FW_UP_WAIT_RESET,
};

/*============================================================================*/
/* struct */
/*============================================================================*/
struct Msensors_fw {
	wait_queue_head_t wait;
	int status;
	int ver_loaded;
	uint8_t maj_ver;
	uint8_t min_ver;
	uint8_t revision;
	uint8_t subcpu_reset_cause;
};

struct Msensors_spi {
	struct task_struct *sensor_read_thread;
	unsigned char send_buf[SPI_DATA_MAX];
	unsigned int  pre_send_type;
};

struct Msensors_state {
	struct spi_device *sdev;
	struct cdev cdev;
	unsigned int sub_main_int;
	unsigned int sub_main_int3;
	unsigned int main_sub_int;
	struct Msensors_spi spi;
	struct Msensors_fw fw;
};

struct Msensors_data {
	__u64 timestamp;
	char sensor_type;
	char handle;
	union {
		char sensor_value[6];
		uint16_t sensor_val_u16[3];
	};
};

/*============================================================================*/
/* func */
/*============================================================================*/


/*============================================================================*/
/* macro */
/*============================================================================*/
#define PARAM_READ_2BYTE(a, index)    (((a[index + 1]<<8) | (a[index])))

/*============================================================================*/
/* extern(val) */
/*============================================================================*/


/*============================================================================*/
/* extern(func) */
/*============================================================================*/
struct Msensors_state *get_msensors_state(void);
int Msensors_PushData(unsigned char* write_buff);
ssize_t Msensors_Spi_Send(struct Msensors_state *st, char* send_buf, char* recv_buf, size_t count);
extern void SUB_VibratorSet(int timeout);
void SUBCPU_rtc_write_time(uint8_t *data);
extern int SUBCPU_rtc_read_time(uint8_t *data);
extern int SUB_LCDBrightnessSet(unsigned char LCDBrightness);
extern int SUB_LCDForceOnOffSet(unsigned char force_on);
void Msensors_set_backlight_zero_flag(int flg);
void Msensors_SetTimestamp(void);
void msensors_fw_up_init(struct Msensors_state *st);
void Msensors_set_fw_version(struct Msensors_state *st, uint8_t *data);
void spi_send_wrapper_for_fwup(uint8_t *sendbuf, uint8_t *recvbuf, size_t count);
void SUBCPU_send_lowtemp_burnoff_enable(void);

#endif	/* __MULTISENSORS_H */
/* PET nishino ADD End */

