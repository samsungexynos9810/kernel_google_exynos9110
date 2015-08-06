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


/*============================================================================*/
/* define */
/*============================================================================*/
#define MSENSORS_DATA_MAX			(8192)
#define MSENSORS_BATCH_MAX_TIME_NS		(10000000000)	/* 10sec */

#define SUB_COM_TYPE_RES_NOMAL			(0xA0)		/* Nomal Response */
#define SUB_COM_TYPE_WRITE			(0xA1)		/* Write Request */
#define SUB_COM_TYPE_READ			(0xA2)		/* Read Request */
#define SUB_COM_TYPE_BIT_HEAD			(0x80)		/* HEADER Information */
#define SUB_COM_TYPE_GETDATA			(0xF1)		/* GetData */
#define SUB_COM_TYPE_SENSOR			(0xF2)		/* Sensor Data(Get Data Non) */
#define SUB_COM_TYPE_SENSOR_GETDATA		(0xF3)		/* Sensor Data(Get Data True) */
#define SUB_COM_SETID_MAIN_STATUS		(0x01)		/* MAIN status */
#define SUB_COM_SETID_SENSOR_PERIOD		(0x02)		/* Sensor Data Send Period */
#define SUB_COM_SETID_FIFO_FLUSH		(0x04)		/* FIFO flash */

#define SUB_COM_SETID_ONOFF_ACCELE		(0x21)		/* Acceleratio Non/OFF */
#define SUB_COM_SETID_ONOFF_MAG			(0x22)		/* Magnetic ON/OFF */
#define SUB_COM_SETID_ONOFF_GYRO		(0x23)		/* Gyroscope ON/OFF */
#define SUB_COM_SETID_ONOFF_PRESS		(0x24)		/* Pressure ON/OFF */
#define SUB_COM_SETID_ONOFF_BHA		(0x25)		/* BHA ON/OFF */
#define SUB_COM_SETID_ONOFF_STPCOUNTER		(0x26)		/* Step Counter ON/OFF */
#define SUB_COM_SETID_ONOFF_STPDETECTOR		(0x27)		/* Step Detector ON/OFF */
#define SUB_COM_SETID_ONOFF_WRIST_TILT	(0x28)	/* Wrist Tilt ON/OFF */

#define SUB_COM_SETID_HZ_ACCELE			(0x31)		/* Acceleration Hz */
#define SUB_COM_SETID_HZ_MAG			(0x32)		/* Magnetic Hz */
#define SUB_COM_SETID_HZ_GYRO			(0x33)		/* Gyroscope Hz */
#define SUB_COM_SETID_HZ_PRESS			(0x34)		/* Pressure Hz */
#define SUB_COM_SETID_HZ_BHA		(0x35)		/* BHA Hz */
#define SUB_COM_SETID_HZ_STPCOUNTER		(0x36)		/* Step Counter Hz */
#define SUB_COM_SETID_HZ_STPDETECTOR	(0x37)		/* Step Detector Hz */

#define SUB_COM_SETID_BATCH_ACCELE			(0x11)		/* Acceleration Hz */
#define SUB_COM_SETID_BATCH_MAG			(0x12)		/* Magnetic Hz */
#define SUB_COM_SETID_BATCH_GYRO			(0x13)		/* Gyroscope Hz */
#define SUB_COM_SETID_BATCH_PRESS			(0x14)		/* Pressure Hz */
#define SUB_COM_SETID_BATCH_BHA		(0x15)		/* BHA Hz */
#define SUB_COM_SETID_BATCH_STPCOUNTER		(0x16)		/* Step Counter Hz */
#define SUB_COM_SETID_BATCH_STPDETECTOR	(0x17)		/* Step Detector Hz */

#define SUB_COM_SETID_VIB_SET			(0x40)		/* Vibrator Settiong */
#define SUB_COM_SETID_BUZ_SET			(0x41)		/* Buzzer Settiong (no used) */
#define SUB_COM_SETID_LCD_SET			(0x42)		/* LCD Brightness Settiong */
#define SUB_COM_SETID_RTC			(0x50)		/* RTC */
#define SUB_COM_SETID_ALERM			(0x51)		/* Alarm */
#define SUB_COM_SETID_KEYMODE			(0x52)		/* Key Mode */
#define SUB_COM_SETID_THEATER_MODE	(0x53)		/* Theater mode flg */
#define SUB_COM_SETID_SUB_FIRM_UPDATE		(0x99)		/* SUB-CPU Firmware Update */
#define SUB_COM_SETID_DEMO_CMD			(0xFE)		/* Demo Command */
#define SUB_COM_SETID_DEMO			(0xFF)		/* Demo ID */
#define SUB_COM_GETID_SUB_CPU_VER		(0x00)		/* SUB-CPU Version */
#define SUB_COM_GETID_RESERVED1			(0x01)		/* Reserved */
#define SUB_COM_GETID_POWER_PROP		(0x02)		/* Power Property  */
#define SUB_COM_GETID_USB			(0x03)		/* USB Status */
#define SUB_COM_GETID_RTC			(0x50)		/* RTC */
#define SUB_COM_SENSID_ACCELE			(0x01)		/* Acceleration Data */
#define SUB_COM_SENSID_MAG			(0x02)		/* Magnetic Data */
#define SUB_COM_SENSID_GYRO			(0x03)		/* Gyroscope Data */
#define SUB_COM_SENSID_PRESS			(0x04)		/* Pressure Data */
#define SUB_COM_SENSID_WAKEMOTION		(0x05)		/* WakeMotion Data */
#define SUB_COM_SENSID_STPCOUNTER		(0x06)		/* Step Counter Data */
#define SUB_COM_SENSID_STPDETECTOR		(0x07)		/* Step Detector Data */
#define SUB_COM_SENSID_NUM			(0x08)		/* Number of SENSIDs */

#define MSENSORS_TYPE_META			(0x00)		/* Sensor Type Meta */
#define MSENSORS_TYPE_ACCELERATION		(0x01)		/* Sensor Type Acceleration */
#define MSENSORS_TYPE_MAGNETIC_FIELD		(0x02)		/* Sensor Type Magnetic */
#define MSENSORS_TYPE_GYRO			(0x03)		/* Sensor Type Gyroscope */
#define MSENSORS_TYPE_PRESSURE			(0x04)		/* Sensor Type Pressure */
#define MSENSORS_TYPE_BHA		(0x05)		/* Sensor Type WakeMotion */
#define MSENSORS_TYPE_STEP_COUNTER		(0x06)		/* Sensor Type Step Counter */
#define MSENSORS_TYPE_STEP_DETECTOR		(0x07)		/* Sensor Type Step Detector */
#define MSENSORS_TYPE_WRIST_TILT	(0x08)
#define MSENSORS_TYPE_NUM			(0x09)		/* Number of Sensor Types */
#define MSENSORS_TYPE_INVALID			(0xFF)		/* Invalid Sensor Type */

#define SUB_COM_HEAD_INDEX_TYPE			(0x00)		/* HEADER Information Index Type */
#define SUB_COM_HEAD_INDEX_PACKET		(0x01)		/* HEADER Information Index Packet */
#define SUB_COM_HEAD_INDEX_ALART		(0x02)		/* HEADER Information Index Alart */
#define SUB_COM_HEAD_INDEX_RTC			(0x03)		/* HEADER Information Index RTC */
#define SUB_COM_HEAD_INDEX_TEMP			(0x08)		/* HEADER Information Index Temperature */
#define SUB_COM_HEAD_INDEX_BATT_PAR		(0x0C)		/* HEADER Information Index Battery % */
#define SUB_COM_HEAD_INDEX_USB_STAT		(0x0E)		/* HEADER Information Index USB Status */

#define SUB_COM_HEAD_SIZE_RTC			(0x05)		/* HEADER Information Size RTC */

#define SUB_COM_TYPE_SIZE			(0x01)		/* Type Size */
#define SUB_COM_ID_SIZE				(0x01)		/* ID Size */

#define SUB_COM_HEAD_SIZE_SETDATA		(0x06)		/* Setting Data Size */

#define SUB_COM_DATA_INDEX_ID			(0x01)		/* Data Index ID */
#define SUB_COM_DATA_SIZE_GETDATA		(0x06)		/* Data Size GetData */
#define SUB_COM_DATA_SIZE_PACKET		(0x06)		/* Data Size Packet */
#define SUB_COM_DATA_SIZE_RTC			(0x05)		/* Data Size RTC */

#define SUB_COM_MAX_PACKET			(0xFF)		/* Packet MAX */

#define SUB_COM_SEND_DUMMY			(0xFF)		/* Send Dummy Data */

#define SUB_ALERT_BIT_POWER_CHG			(0)		/* Power property changed */
#define SUB_ALERT_BIT_RESERVE1			(1)		/* Reserve 1 */
#define SUB_ALERT_BIT_RESERVE2			(2)		/* Reserve 2 */
#define SUB_ALERT_BIT_RESERVE3			(3)		/* Reserve 3 */
#define SUB_ALERT_BIT_RESERVE4			(4)		/* Reserve 4 */
#define SUB_ALERT_BIT_RESERVE5			(5)		/* Reserve 5 */
#define SUB_ALERT_BIT_RESERVE6			(6)		/* Reserve 6 */
#define SUB_ALERT_BIT_RESERVE7			(7)		/* Reserve 7 */

#define DEMO_CMD_DATA_MAX  			(6)		/* Demo Command Data index max size */

#define PARAM_INDEX_ACCELE_X                	(  0U)
#define PARAM_INDEX_ACCELE_Y                	(  2U)
#define PARAM_INDEX_ACCELE_Z                	(  4U)
#define PARAM_INDEX_MAG_X                   	(  0U)
#define PARAM_INDEX_MAG_Y                   	(  2U)
#define PARAM_INDEX_MAG_Z                   	(  4U)
#define PARAM_INDEX_GYRO_X                  	(  0U)
#define PARAM_INDEX_GYRO_Y                  	(  2U)
#define PARAM_INDEX_GYRO_Z                  	(  4U)
#define PARAM_INDEX_PRESS                   	(  0U)

#define SUB_LCD_CONTROL				(0)
#define SUB_LCD_ONOFF_MASK			(0x80)

/*============================================================================*/
/* struct */
/*============================================================================*/
struct Msensors_data {
	__u64 timestamp;
	char sensor_type;
	char handle;
	char sensor_value[6];
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
extern int SUB_VibratorEnable(int enable);
extern void SUB_VibratorSet(int timeout);
extern int SUBCPU_rtc_set_time(uint8_t *data);
extern int SUBCPU_rtc_read_time(uint8_t *data);
extern int SUB_BatteryVoltageRead(void);
extern int SUB_BatteryLevelRead(void);
extern int SUB_BatteryTemperatureRead(void);
extern int SUB_BatteryStatusRead(void);
extern void Msensors_Spi_Recv(void);
extern int SUB_KeyModeChange(unsigned char KeyMode);
extern int SUB_IsTheaterMode(unsigned char flg_bl_dark);
extern int SUB_LCDBrightnessSet(unsigned char LCDBrightness);
extern int SUB_DemoCommandSend(unsigned char* demo_cmd);
extern int MSensors_get_timeout_value(void);

#endif	/* __MULTISENSORS_H */
/* PET nishino ADD End */

