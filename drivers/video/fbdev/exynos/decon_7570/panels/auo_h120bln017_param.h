/*
	auo_h120bln017_param.h
*/

#ifndef __AUO_H120BLN017_PARAM_H__
#define __AUO_H120BLN017_PARAM_H__

static const unsigned char cmd_set_1[2] = {
	/* command */
	0xFE,
	/* parameter */
	0x00
};

static const unsigned char cmd_set_2[5] = {
	/* command */
	0x2A,
	/* parameter */
	0x00, 0x04, 0x01, 0x89
};

static const unsigned char cmd_set_3[5] = {
	/* command */
	0x2B,
	/* parameter */
	0x00, 0x00, 0x01, 0x85
};

static const unsigned char cmd_set_4[5] = {
	/* command */
	0x30,
	/* parameter */
	0x00, 0x00, 0x01, 0x85
};

static const unsigned char cmd_set_5[5] = {
	/* command */
	0x31,
	/* parameter */
	0x00, 0x04, 0x01, 0x89
};

static const unsigned char cmd_set_6[2] = {
	/* command */
	0x12,
	/* parameter */
	0x00
};

static const unsigned char cmd_set_7[2] = {
	/* command */
	0x35,
	/* parameter */
	0x02
};

static const unsigned char cmd_set_8[2] = {
	/* command */
	0x53,
	/* parameter */
	0x20
};

static const unsigned char cmd_set_9[2] = {
	/* command */
	0x51,	/* brightness */
	/* parameter */
	0xa0
};

#endif /* __AUO_H120BLN017_PARAM_H__ */
