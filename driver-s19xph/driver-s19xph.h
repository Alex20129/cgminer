#ifndef DRIVER_S19XPH_H
#define DRIVER_S19XPH_H

#include <sys/poll.h>
#include <pthread.h>
#include <stdint.h>

#define FAN_CHECK_INTERVAL 1
#define FAN0 "256:"
#define FAN1 "254:"
#define PROCFILENAME "/proc/interrupts"

#define TTY_DEVICE_TEMPLATE	"/dev/ttyAMA%d"
#define I2C_DEVICE			"/dev/i2c-1"

//Command Description
#define CMD_ALL             0x10 // b 0001 0000
#define SET_ADDR            0x00 // b 0000 0000
#define SET_CONFIG          0x01 // b 0000 0001
#define GET_STATUS          0x02 // b 0000 0010
#define CHAIN_INACTIVE      0x03 // b 0000 0011

#define CMD_LENGTH				5
#define CONFIG_LENGTH			9

#define PACKET_PREAMBLE_1			0x55
#define PACKET_PREAMBLE_2			0xAA
#define PACKET_PREAMBLE_LENGTH		2

//Register description
#define CHIP_ADDR						0x00
#define HASHRATE						0x04
#define PLL_PARAMETER					0x08
#define NONCE_OFFSET					0x0C
#define HASH_COUNTER					0x10
#define TICKET_MASK						0x14
#define MISC_CONTROL					0x18
#define I2C_CONTROL						0x1C
#define ORDERED_CLOCK					0x20
#define FAST_UART_CONFIGURATION			0x28
#define UART_RELAY						0x2C
#define SEC_CTRL_STATUS					0x34
#define TICKET_MASK_2					0x38
#define CORE_CONTROL					0x3C
#define CORE_REGISTER_STATUS				0x40
#define EXTERNAL_TEMPERATURE_SENSOR_READ	0x44
#define ERROR_FLAG							0x48
#define NONCE_ERROR_COUNTER				0x4C
#define NONCE_OVERFLOW_COUNTER			0x50
#define ANALOG_MUX_CONTROL				0x54
#define IO_DRIVER_STRENGTH				0x58
#define TIME_OUT						0x5C
#define PLL1_PARAMETER					0x60
#define PLL2_PARAMETER					0x64
#define PLL3_PARAMETER					0x68
#define ORDERED_CLOCK_MONITOR			0x6C
#define PLL0_DIVIDER					0x70
#define PLL1_DIVIDER					0x74
#define PLL2_DIVIDER					0x78
#define PLL3_DIVIDER					0x7C
#define CLOCK_ORDER_CONTROL0			0x80
#define CLOCK_ORDER_CONTROL1			0x84
#define CLOCK_ORDER_STATUS				0x8C
#define FREQUENCY_SWEEP_CONTROL0		0x90
#define GOLDEN_NONCE_FOR_SWEEP_RETURN	0x94
#define RETURNED_GROUP_PATTERN_STATUS	0x98
#define NONCE_RETURNED_TIMEOUT			0x9C
#define RETURNED_SINGLE_PATTERN_STATUS	0xA0
#define VERSION_MASK					0xA4
#define UNKNOWN_REG_A8					0xA8

//core command in value
#define CORE_CMD_ALL        (0x1 << 7) // b 1000 0000
#define CORE_INDEX(X)       (X & 0xf)
#define CORE_DATA(X)        (X & 0xff)
#define CLK_EN(X)           (X & 0x3)
#define LCM(X)              ((X & 0x3) << 1)
#define PM_START            (0x1 << 2)
#define PM_SEL(X)           (X & 0x3)           // 1:select vdd; 0:select vss

//cmd type
#define CLOCK_EN_CTRL       0x00
#define CALC_MODE_SET       0x01
#define PRO_MONI_CTRL       0x02
#define TEMP_DIODE_SEL      0x03
#define VOLT_MONI_SEL       0x04

//Register bits value
#define CMD_TYPE            0x40 // b 0100 0000
#define HASHRATE_CTRL1(X)   (X & 0x07)
#define HASHRATE_CTRL2(X)   ((X & 0x07) << 4)
#define PAT                 (0x0 << 7)
#define LDO18CTRL(X)        ((X & 0x7) << 4)
#define GAP_ERROR           (0x1 << 2)
#define WORK_CRC_ERROR      (0x1 << 1)
#define CMD_CRC_ERROR       0x1
#define VM_SEL(X)           (X & 0x7)
#define INV_CLKO            (0x1 << 5)
#define GATEBCLK            0x1 << 7
#define RFS                 (0x1 << 6)
#define BT8D                0x1A
#define MMEN                (0x1 << 7)
#define TFS(X)              ((X & 0x03) << 5)
#define LDO09_PD            (0x1 << 7)
#define LDO09_CTRL(X)       ((X & 0x07) << 2)
#define RN_CTRL(X)          (X & 0x1)
#define IIC_BUSY            (0x1 << 7)
#define IIC_RW_FAIL         (0x1 << 6)
#define AUTOREADTEMP        (0x1 << 1)
#define REGADDR_VALID       0x1
#define DEVICE_ADDR(X)      ((X & 0x7F) << 1)
#define IIC_WRITE           0x1                 //0: READ  1: WRITE
#define ADDR_RF             (0x1 << 7)
#define EPROM_ADDR          0x50
#define SIG_CNT(X)          (X & 0x0F) << 4)
#define ECC_CLKEN           (0x1 << 3)
#define SIG_PASS            (0x1 << 1)
#define DISA_CHIP           0x1
#define ADDR_RF             (0x1 << 7)
#define NONCE_BIT           (0x1 << 7)
#define SIG_BIT             (0x1 << 6)
#define PARITY_BIT          (0x1 << 5)

struct GENERAL_IIC_DATA
{
	uint8_t regaddrvalid        :1;
	uint8_t autoreadtemp        :1;
	uint8_t reserved            :4;
	uint8_t rw_fail             :1;
	uint8_t busy                :1;
	uint8_t rw_ctrl             :1;  // 0:read  1:write
	uint8_t deviceaddr          :7;
	uint8_t regaddr;
	uint8_t data;
};

struct ORDERED_CLOCK_DATA
{
	uint8_t reserved            :6;
	uint8_t rw_fail             :1;
	uint8_t busy                :1;
	uint8_t rw_ctrl             :1;  // 0:read  1:write
	uint8_t reserved1           :7;
	uint8_t regaddr;
	uint8_t data;
};

struct CORE_CONTROL_DATA
{
	uint8_t reg_data[4];
};

struct HASH_COUNTER_DATA
{
	uint8_t reg_data[4];
};

struct TICKET_MASK_DATA
{
	uint8_t reg_data[4];
};

struct MISC_CONTROL_DATA
{
	uint8_t hashratrectrl1  : 3;
	uint8_t reserved        : 1;
	uint8_t hashratrectrl2  : 3;
	uint8_t reserved1       : 1;

	uint8_t cmd_crc_err     : 1;
	uint8_t work_crc_err    : 1;
	uint8_t gap_crc_err     : 1;
	uint8_t reserved2       : 1;
	uint8_t ldo18ctrl       : 3;
	uint8_t reserved3       : 1;

	uint8_t bt8d            : 5;
	uint8_t inv_clko        : 1;
	uint8_t rfs             : 1;
	uint8_t reserved4       : 1;

	uint8_t reserved5       : 2;
	uint8_t ldo09ctrl       : 3;
	uint8_t tfs             : 2;
	uint8_t ldo09_pd        : 1;
};

struct FAST_UART_CONFIGURATION_DATA
{
	uint8_t reg_data[4];
};

struct UART_RELAY_DATA
{
	uint8_t co_relay_en	:1;
	uint8_t ro_relay_en	:1;
	uint8_t reserved1	:6;
	uint8_t reserved2;
	uint16_t gap;
};

struct ANALOG_MUX_CONTROL_DATA
{
	uint8_t reg_data[4];
};

struct IO_DRIVER_STRENGTH_DATA
{
	uint8_t reg_data[4];
};

struct HCN_DATA
{
	uint8_t reg_data[4];
};

struct PLL_DATA
{
	uint8_t reg_data[4];
};

struct NONCE_OFFSET_DATA
{
	uint16_t reg_data1;
	uint16_t reg_data2;
};

struct VERSION_MASK_DATA
{
	uint8_t reg_data[4];
};

struct UNKNOWN_REG_A8_DATA
{
	uint8_t reg_data[4];
};

// macro define about miner
#define ASICS_ON_CHAIN_MAX				204
#define CHAINS_MAX						4
#define DOMAIN_SIZE						17
#define TEMPERATURE_SENSORS_PER_BOARD	4
#define BM1366_CORE_NUM					112
#define DEFAULT_FREQUENCY			100 //MHz
#define INIT_VOLTAGE				21.00F // volts
#define DEFAULT_VOLTAGE				18.75F // volts
#define DEFAULT_TARGET_TEMPERATURE	65
#define MAX_NONCE_NUMBER_IN_FIFO	ASICS_ON_CHAIN_MAX*CHAINS_MAX*3
#define WORKS_PER_CHAIN				32
#define WORK_UPDATE_INTERVAL		20000 // uSec

#define ASIC_REG_VALUE_PACKET_LENGTH	9
#define ASIC_NONCE_PACKET_LENGTH		11
#define ASIC_WORK_PACKET_LENGTH			86
#define ASIC_RESPONSE_BUF_SIZE			512

#define CHAIN_CONFIG_INTERVAL			50000

typedef enum
{
	BUF_IDLE,
	BUF_READING,
	BUF_WRITING,
} buf_state_e;

typedef struct
{
	pthread_t		uart_tx_thr;
	pthread_t		uart_rx_thr;
	pthread_mutex_t	tty_rw_mutex;
	int32_t			tty_fd;
	uint32_t		hw_errors;
	uint32_t		hash_rate;
	uint32_t	freq;
	uint32_t	work_update_interval;
	uint32_t	asic_nonce_count[ASICS_ON_CHAIN_MAX];
	uint8_t		asic_alive[ASICS_ON_CHAIN_MAX];
	float		temperature[TEMPERATURE_SENSORS_PER_BOARD];
	char		pic_software_version[8];
	struct work	*works[WORKS_PER_CHAIN];
	uint8_t		id;
	uint8_t		exist;
	uint8_t		active;
	uint8_t		number_of_asics;
	uint8_t		powered_on;
} board_parameters_t;

typedef struct
{
	struct thr_info	*mining_control_thr;
	uint8_t		asic_addr_interval;
	uint8_t		baud;
	uint8_t		hash_boards_active;
	float		temperature_max;
	uint32_t	valid_nonce_count;
	int32_t		i2c_fd;
} device_parameters_t;

typedef struct __attribute__((packed))
{
	uint8_t preamble[2];
	uint32_t reg_value;
	uint8_t chip_addr;
	uint8_t reg_addr;
	uint8_t crc;
	uint8_t reserved[3];
} asic_reg_value_result_t;

// | preamble          | nonce                               | mids_num |  ?  | w_id  | version           | crc-5 |  ?  |
// | 00000000 00000000 | 00000000 00000000 00000000 00000000 | 00000000 | 000 | 00000 | 00000000 00000000 | 00000 | 000 |
typedef struct __attribute__((packed))
{
	uint8_t preamble[2];
	uint32_t nonce;
	uint8_t midstate_num;
	uint8_t work_id;
	uint16_t version;
	uint8_t crc;
	uint8_t board_id;
} asic_nonce_result_t;

typedef struct __attribute__((packed, aligned(4)))
{
	uint32_t p_wr;
	uint32_t p_rd;
	uint32_t nonce_num;
	asic_nonce_result_t nonce_data[MAX_NONCE_NUMBER_IN_FIFO];
} nonce_buf_t;

typedef struct __attribute__((__packed__))
{
	uint8_t preamble[2];
	uint8_t command;
	uint8_t not_a_length;
	uint8_t work_id;
	uint8_t num_midstates;
	uint32_t starting_nonce;
	uint32_t nbits;
	uint32_t ntime;
	uint8_t merkle_root[32];
	uint8_t prev_block_hash[32];
	uint32_t version;
	uint16_t crc16;
} work_1366_t;

typedef struct
{
	uint16_t freq;
	uint32_t vilpll;
} pll_freq_t;

extern device_parameters_t	*g_device;
extern board_parameters_t	*g_boards[CHAINS_MAX];

extern int opt_frequency;
extern int opt_target_temperature;
extern float opt_psu_voltage;

#endif // DRIVER_S19XPH_H
