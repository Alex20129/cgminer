/*
 * Copyright 2016-2017 Mikeqin <Fengling.Qin@gmail.com>
 * Copyright 2016 Con Kolivas <kernel@kolivas.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#ifndef DRIVER_AVALON_9_H
#define DRIVER_AVALON_9_H

#include "../i2c-context.h"

#define AVA9_FREQUENCY_MAX		1404

#define AVA9_DEFAULT_FAN_MIN		5 /* % */
#define AVA9_DEFAULT_FAN_MAX		100

#define AVA9_DEFAULT_TEMP_MIN		0
#define AVA9_DEFAULT_TEMP_MAX		100
#define AVA9_DEFAULT_TEMP_TARGET		87
#define AVA911V_DEFAULT_TEMP_TARGET	81
#define AVA911S_DEFAULT_TEMP_TARGET	69
#define AVA910_DEFAULT_TEMP_TARGET	87
#define AVA910S_DEFAULT_TEMP_TARGET	70 /* A910 BIN4(A910S) target temperature, but compatible real A910S type */
#define AVA9_DEFAULT_TEMP_OVERHEAT	105

#define AVA9_DEFAULT_VOLTAGE_LEVEL_MIN	0
#define AVA9_DEFAULT_VOLTAGE_LEVEL_MAX	35
#define AVA9_INVALID_VOLTAGE_LEVEL	-1

#define AVA9_DEFAULT_VOLTAGE_LEVEL_OFFSET_MIN	-2
#define AVA9_DEFAULT_VOLTAGE_LEVEL_OFFSET		0
#define AVA9_DEFAULT_VOLTAGE_LEVEL_OFFSET_MAX	1

#define AVA9_INVALID_ASIC_OTP				-1

#define AVA9_DEFAULT_FACTORY_INFO_0_MIN		0
#define AVA9_DEFAULT_FACTORY_INFO_0			0
#define AVA9_DEFAULT_FACTORY_INFO_0_MAX		1
#define AVA9_DEFAULT_FACTORY_INFO_0_CNT		1
#define AVA9_DEFAULT_FACTORY_INFO_0_IGNORE	36

#define AVA9_DEFAULT_FACTORY_INFO_1_CNT		3
#define AVA9_DEFAULT_FACTORY_INFO_2_CNT		4

#define AVA9_DEFAULT_OVERCLOCKING_OFF		0
#define AVA9_DEFAULT_OVERCLOCKING_ON			1

#define AVA9_DEFAULT_FREQUENCY_0M	0
#define AVA9_DEFAULT_FREQUENCY_450M	450
#define AVA9_DEFAULT_FREQUENCY_460M	460
#define AVA9_DEFAULT_FREQUENCY_470M	470
#define AVA9_DEFAULT_FREQUENCY_480M	480
//---
#define AVA9_DEFAULT_FREQUENCY_437M	437
#define AVA9_DEFAULT_FREQUENCY_462M	462
#define AVA9_DEFAULT_FREQUENCY_487M	487
#define AVA9_DEFAULT_FREQUENCY_512M	512
#define AVA9_DEFAULT_FREQUENCY_537M	537
#define AVA9_DEFAULT_FREQUENCY_562M	562
#define AVA9_DEFAULT_FREQUENCY_587M	587
#define AVA9_DEFAULT_FREQUENCY_612M	612
//---
#define AVA9_DEFAULT_FREQUENCY_490M	490
#define AVA9_DEFAULT_FREQUENCY_500M	500
#define AVA9_DEFAULT_FREQUENCY_510M	510
#define AVA9_DEFAULT_FREQUENCY_520M	520
#define AVA9_DEFAULT_FREQUENCY_530M	530
#define AVA9_DEFAULT_FREQUENCY_540M	540
#define AVA9_DEFAULT_FREQUENCY_550M	550
#define AVA9_DEFAULT_FREQUENCY_560M	560
#define AVA9_DEFAULT_FREQUENCY_570M	570
#define AVA9_DEFAULT_FREQUENCY_580M	580
#define AVA9_DEFAULT_FREQUENCY_590M	590
#define AVA9_DEFAULT_FREQUENCY_600M	600
#define AVA9_DEFAULT_FREQUENCY_MAX	1200
#define AVA9_DEFAULT_FREQUENCY		(AVA9_DEFAULT_FREQUENCY_MAX)
#define AVA9_DEFAULT_FREQUENCY_SEL	3

#define AVA9_DEFAULT_MODULARS		7	/* Only support 6 modules maximum with one AUC */
#define AVA9_DEFAULT_MINER_CNT		6
#define AVA9_DEFAULT_ASIC_MAX		34
#define AVA9_DEFAULT_PLL_CNT			4
#define AVA9_DEFAULT_CORE_VOLT_CNT	8

#define AVA9_DEFAULT_POLLING_DELAY	20 /* ms */
#define AVA9_DEFAULT_NTIME_OFFSET	2

#define AVA9_DEFAULT_SMARTSPEED_OFF		0
#define AVA9_DEFAULT_SMARTSPEED_MODE1	1
#define AVA9_DEFAULT_SMART_SPEED	(AVA9_DEFAULT_SMARTSPEED_MODE1)

#define AVA9_DEFAULT_TH_PASS		10
#define AVA9_DEFAULT_TH_FAIL		1000
#define AVA9_DEFAULT_TH_INIT		32767
#define AVA9_DEFAULT_TH_ADD		0
#define AVA9_DEFAULT_TH_MS		2
#define AVA9_DEFAULT_TH_TIMEOUT	1550000
#define AVA9_DEFAULT_NONCE_MASK 	24
#define AVA9_DEFAULT_NONCE_CHECK	1
#define AVA9_DEFAULT_MUX_L2H		0
#define AVA9_DEFAULT_MUX_H2L		1
#define AVA9_DEFAULT_H2LTIME0_SPD	3
#define AVA9_DEFAULT_ROLL_ENABLE	1
#define AVA9_DEFAULT_SPDLOW		1
#define AVA9_DEFAULT_SPDHIGH		3
#define AVA9_DEFAULT_TBASE		0

/* PID CONTROLLER*/
#define AVA9_DEFAULT_PID_P		2
#define AVA9_DEFAULT_PID_I		5
#define AVA9_DEFAULT_PID_D		0
#define AVA9_DEFAULT_PID_TEMP_MIN	50
#define AVA9_DEFAULT_PID_TEMP_MAX	100

#define AVA9_DEFAULT_ADJUST_VOLT_FREQ	1

#define AVA9_DEFAULT_ADJUST_VOLT_UP_INIT 		20000
#define AVA9_DEFAULT_ADJUST_VOLT_UP_FACTOR 		6
#define AVA9_DEFAULT_ADJUST_VOLT_UP_THRESHOLD 	18300
#define AVA9_DEFAULT_ADJUST_VOLT_DOWN_INIT 		0
#define AVA9_DEFAULT_ADJUST_VOLT_DOWN_FACTOR		4
#define AVA9_DEFAULT_ADJUST_VOLT_DOWN_THRESHOLD	19656
#define AVA9_DEFAULT_ADJUST_VOLT_TIME 			900

/* A911 adjust frequence parameters */
#define AVA9_DEFAULT_ADJUST_FREQ_UP_INIT 	0
#define AVA9_DEFAULT_ADJUST_FREQ_UP_FACTOR 	4
#define AVA9_DEFAULT_ADJUST_FREQ_UP_THRESHOLD 	5000
#define AVA9_DEFAULT_ADJUST_FREQ_DOWN_INIT 	4000
#define AVA9_DEFAULT_ADJUST_FREQ_DOWN_FACTOR 	6
#define AVA9_DEFAULT_ADJUST_FREQ_DOWN_THRESHOLD	2700
#define AVA9_DEFAULT_ADJUST_FREQ_TIME 		900

/* A910 adjust frequence parameters */
#define AVA910_DEFAULT_ADJUST_FREQ_UP_INIT 		0
#define AVA910_DEFAULT_ADJUST_FREQ_UP_FACTOR 		4
#define AVA910_DEFAULT_ADJUST_FREQ_UP_THRESHOLD 	5000
#define AVA910_DEFAULT_ADJUST_FREQ_DOWN_INIT 		3000
#define AVA910_DEFAULT_ADJUST_FREQ_DOWN_FACTOR 		6
#define AVA910_DEFAULT_ADJUST_FREQ_DOWN_THRESHOLD	3614
#define AVA910_DEFAULT_ADJUST_FREQ_TIME 		900

#define AVA9_DEFAULT_ASIC_AVERAGE_TEMP_START	12
#define AVA9_DEFAULT_ASIC_AVERAGE_TEMP_END	21

#define AVA9_DEFAULT_IIC_DETECT		false

#define AVA9_PWM_MAX                    0x3FF
#define AVA9_DRV_DIFFMAX		2700
#define AVA9_ASIC_TIMEOUT_CONST		419430400 /* (2^32 * 1000) / (256 * 40) */

#define AVA9_MODULE_DETECT_INTERVAL	30 /* 30 s */

#define AVA9_AUC_VER_LEN	12	/* Version length: 12 (AUC-YYYYMMDD) */
#define AVA9_AUC_SPEED		400000
#define AVA9_AUC_XDELAY  	19200	/* 4800 = 1ms in AUC (11U14)  */
#define AVA9_AUC_P_SIZE		64

#define AVA9_CONNECTER_AUC	1
#define AVA9_CONNECTER_IIC	2

/* avalon9 protocol package type from MM protocol.h */
#define AVA9_MM_VER_LEN		15
#define AVA9_MM_DNA_LEN		8
#define AVA9_H1			'C'
#define AVA9_H2			'N'

#define AVA9_P_COINBASE_SIZE	(6 * 1024 + 64)
#define AVA9_P_MERKLES_COUNT	30

#define AVA9_P_COUNT		40
#define AVA9_P_DATA_LEN		32

#define AVA9_OTP_LEN	        32

/* Broadcase with block iic_write*/
#define AVA9_P_DETECT		0x10

/* Broadcase With non-block iic_write*/
#define AVA9_P_STATIC		0x11
#define AVA9_P_JOB_ID		0x12
#define AVA9_P_COINBASE		0x13
#define AVA9_P_MERKLES		0x14
#define AVA9_P_HEADER		0x15
#define AVA9_P_TARGET		0x16
#define AVA9_P_JOB_FIN		0x17

/* Broadcase or with I2C address */
#define AVA9_P_SET		0x20
#define AVA9_P_SET_FIN		0x21
#define AVA9_P_SET_VOLT		0x22
#define AVA9_P_SET_PMU		0x24
#define AVA9_P_SET_PLL		0x25
#define AVA9_P_SET_SS		0x26
/* 0x27 reserved */
#define AVA9_P_SET_FAC		0x28
#define AVA9_P_SET_OC		0x29

/* Have to send with I2C address */
#define AVA9_P_POLLING		0x30
#define AVA9_P_SYNC		0x31
#define AVA9_P_TEST		0x32
#define AVA9_P_RSTMMTX		0x33
#define AVA9_P_GET_VOLT		0x34

/* Back to host */
#define AVA9_P_ACKDETECT	0x40
#define AVA9_P_STATUS		0x41
#define AVA9_P_NONCE		0x42
#define AVA9_P_TEST_RET		0x43
#define AVA9_P_STATUS_VOLT	0x46
#define AVA9_P_STATUS_POWER	0x48
#define AVA9_P_STATUS_PLL	0x49
#define AVA9_P_STATUS_LOG	0x4a
#define AVA9_P_STATUS_ASIC	0x4b
#define AVA9_P_STATUS_PVT	0x4c
#define AVA9_P_STATUS_FAC	0x4d
#define AVA9_P_STATUS_OC	0x4e
#define AVA9_P_STATUS_OTP	0x4f
#define AVA9_P_SET_ASIC_OTP	0x50
#define AVA9_P_SET_ADJUST_VOLT	0x51
#define AVA9_P_SET_ADJUST_FREQ	0x52

#define AVA9_MODULE_BROADCAST	0
/* End of avalon9 protocol package type */

#define AVA9_IIC_RESET		0xa0
#define AVA9_IIC_INIT		0xa1
#define AVA9_IIC_DEINIT		0xa2
#define AVA9_IIC_XFER		0xa5
#define AVA9_IIC_INFO		0xa6

#define AVA9_FREQ_INIT_MODE	0x0
#define AVA9_FREQ_PLLADJ_MODE	0x1

#define AVA9_DEFAULT_FACTORY_INFO_CNT	(AVA9_DEFAULT_FACTORY_INFO_0_CNT + AVA9_DEFAULT_FACTORY_INFO_1_CNT + AVA9_DEFAULT_FACTORY_INFO_2_CNT)

#define AVA9_DEFAULT_POWER_INFO_CNT	6

#define AVA9_DEFAULT_OVERCLOCKING_CNT	1

#define AVA9_OTP_INDEX_READ_STEP   	27
#define AVA9_OTP_INDEX_ASIC_NUM		28
#define AVA9_OTP_INDEX_CYCLE_HIT	29
#define AVA9_OTP_INFO_LOTIDCRC_OFFSET	0
#define AVA9_OTP_INFO_LOTID_OFFSET  	6

struct avalon9_pkg {
	u_int8_t head[2];
	u_int8_t type;
	u_int8_t opt;
	u_int8_t idx;
	u_int8_t cnt;
	u_int8_t data[32];
	u_int8_t crc[2];
};
#define avalon9_ret avalon9_pkg

struct avalon9_info
{
	/* Public data */
	int64_t last_diff1;
	int64_t pending_diff1;
	double last_rej;

	int mm_count;
	int xfer_err_cnt;

	struct timeval firsthash;
	struct timeval last_fan_adj;
	struct timeval last_stratum;
	struct timeval last_detect;

	bool work_restart;

	u_int32_t last_jobid;

	/* For connecter */
	char auc_version[AVA9_AUC_VER_LEN + 1];

	int auc_speed;
	int auc_xdelay;
	int auc_sensor;

	struct i2c_ctx *i2c_slaves[AVA9_DEFAULT_MODULARS];

	u_int8_t connecter; /* AUC or IIC */

	/* For modulars */
	bool enable[AVA9_DEFAULT_MODULARS];
	bool reboot[AVA9_DEFAULT_MODULARS];

	struct timeval elapsed[AVA9_DEFAULT_MODULARS];

	u_int8_t mm_dna[AVA9_DEFAULT_MODULARS][AVA9_MM_DNA_LEN];
	char mm_version[AVA9_DEFAULT_MODULARS][AVA9_MM_VER_LEN + 1]; /* It's a string */
	u_int32_t total_asics[AVA9_DEFAULT_MODULARS];
	u_int32_t max_ntime; /* Maximum: 7200 */

	u_int8_t otp_info[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT][AVA9_OTP_LEN + 1];

	int mod_type[AVA9_DEFAULT_MODULARS];
	u_int8_t miner_count[AVA9_DEFAULT_MODULARS];
	u_int8_t asic_count[AVA9_DEFAULT_MODULARS];

	u_int32_t freq_mode[AVA9_DEFAULT_MODULARS];
	int led_indicator[AVA9_DEFAULT_MODULARS];
	int fan_pct[AVA9_DEFAULT_MODULARS];
	int fan_cpm[AVA9_DEFAULT_MODULARS];

	int temp[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT][AVA9_DEFAULT_ASIC_MAX];
	int temp_mm[AVA9_DEFAULT_MODULARS];

	u_int32_t core_volt[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT] \
			  [AVA9_DEFAULT_ASIC_MAX][AVA9_DEFAULT_CORE_VOLT_CNT];

	u_int8_t cutoff[AVA9_DEFAULT_MODULARS];
	int temp_target[AVA9_DEFAULT_MODULARS];
	int temp_overheat[AVA9_DEFAULT_MODULARS];

	/* pid controler*/
	int pid_p[AVA9_DEFAULT_MODULARS];
	int pid_i[AVA9_DEFAULT_MODULARS];
	int pid_d[AVA9_DEFAULT_MODULARS];
	double pid_u[AVA9_DEFAULT_MODULARS];
	int pid_e[AVA9_DEFAULT_MODULARS][3];
	int pid_0[AVA9_DEFAULT_MODULARS];

	int set_voltage_level[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT];
	u_int32_t set_frequency[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT][AVA9_DEFAULT_PLL_CNT];
	u_int32_t get_frequency[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT][AVA9_DEFAULT_ASIC_MAX][AVA9_DEFAULT_PLL_CNT];

	int set_asic_otp[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT];

	u_int16_t get_voltage[AVA9_DEFAULT_MODULARS][1]; /* Output is the same */
	u_int32_t get_pll[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT][AVA9_DEFAULT_PLL_CNT];

	u_int32_t get_asic[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT][AVA9_DEFAULT_ASIC_MAX][6];

	int8_t factory_info[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_FACTORY_INFO_CNT + 1];
	int8_t overclocking_info[AVA9_DEFAULT_OVERCLOCKING_CNT];

	uint64_t local_works[AVA9_DEFAULT_MODULARS];
	uint64_t local_works_i[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT];
	uint64_t hw_works[AVA9_DEFAULT_MODULARS];
	uint64_t hw_works_i[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT];
	uint64_t chip_matching_work[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT][AVA9_DEFAULT_ASIC_MAX];

	u_int32_t error_code[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT + 1];
	u_int32_t error_crc[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_MINER_CNT];
	u_int8_t error_polling_cnt[AVA9_DEFAULT_MODULARS];

	uint64_t diff1[AVA9_DEFAULT_MODULARS];

	u_int16_t vin_adc_ratio[AVA9_DEFAULT_MODULARS];
	u_int16_t vout_adc_ratio[AVA9_DEFAULT_MODULARS];

	u_int16_t power_info[AVA9_DEFAULT_MODULARS][AVA9_DEFAULT_POWER_INFO_CNT];

	bool conn_overloaded;
};

struct avalon9_iic_info
{
	u_int8_t iic_op;
	union
	{
		u_int32_t aucParam[2];
		u_int8_t slave_addr;
	} iic_param;
};

struct avalon9_dev_description
{
	u_int8_t dev_id_str[8];
	int mod_type;
	u_int8_t miner_count;	/* it should not greater than AVA9_DEFAULT_MINER_CNT */
	u_int8_t asic_count;	/* asic count each miner, it should not great than AVA9_DEFAULT_ASIC_MAX */
	int set_voltage_level;
	u_int16_t set_freq[AVA9_DEFAULT_PLL_CNT];
	int set_asic_otp;
};

#define AVA9_WRITE_SIZE (sizeof(struct avalon9_pkg))
#define AVA9_READ_SIZE AVA9_WRITE_SIZE

#define AVA9_SEND_OK	0
#define AVA9_SEND_ERROR -1

extern char *set_avalon9_fan(char *arg);
extern char *set_avalon9_freq(char *arg);
extern char *set_avalon9_voltage_level(char *arg);
extern char *set_avalon9_voltage_level_offset(char *arg);
extern char *set_avalon9_adjust_volt_info(char *arg);
extern char *set_avalon9_adjust_freq_info(char *arg);
extern char *set_avalon9_asic_otp(char *arg);
extern int opt_avalon9_temp_target;
extern int opt_avalon9_polling_delay;
extern int opt_avalon9_aucspeed;
extern int opt_avalon9_aucxdelay;
extern int opt_avalon9_smart_speed;
extern bool opt_avalon9_iic_detect;
extern int opt_avalon9_freq_sel;
extern u_int32_t opt_avalon9_th_pass;
extern u_int32_t opt_avalon9_th_fail;
extern u_int32_t opt_avalon9_th_init;
extern u_int32_t opt_avalon9_th_ms;
extern u_int32_t opt_avalon9_th_timeout;
extern u_int32_t opt_avalon9_th_add;
extern u_int32_t opt_avalon9_nonce_mask;
extern u_int32_t opt_avalon9_nonce_check;
extern u_int32_t opt_avalon9_mux_l2h;
extern u_int32_t opt_avalon9_mux_h2l;
extern u_int32_t opt_avalon9_h2ltime0_spd;
extern u_int32_t opt_avalon9_roll_enable;
extern u_int32_t opt_avalon9_spdlow;
extern u_int32_t opt_avalon9_spdhigh;
extern u_int32_t opt_avalon9_tbase;
extern u_int32_t opt_avalon9_pid_p;
extern u_int32_t opt_avalon9_pid_i;
extern u_int32_t opt_avalon9_pid_d;
extern u_int32_t opt_avalon9_adjust_volt_freq;

#endif /* DRIVER_AVALON_9_H */
