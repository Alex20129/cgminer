#ifndef DRIVER_BITMAIN_L3_H
#define DRIVER_BITMAIN_L3_H

#include <sys/poll.h>
#include <sys/types.h>
#include <stdbool.h>

#define FAN_CHECK_INTERVAL 1
#define FAN0 "256:"
#define FAN1 "254:"
#define PROCFILENAME "/proc/interrupts"

#define GPIO_DEVICE_TEMPLATE    "/sys/class/gpio/gpio%i/value"
#define TTY_DEVICE_TEMPLATE     "/dev/ttyO%d"
#define I2C_DEVICE             "/dev/i2c-0"

#define BEEPER_PIN		20
#define RED_LED_PIN		45
#define GREEN_LED_PIN	23

//Command Description
#define CMD_ALL             (0x01 << 4)
#define SET_ADDR            0x0
#define SET_CONFIG          0x1
#define GET_STATUS          0x2
#define CHAIN_INACTIVE      0x3

#define CMD_LENGTH			0x4
#define CONFIG_LENGTH		0x8
//Register description
#define CHIP_ADDR           0x00
#define HASHRATE            0x04
#define PLL_PARAMETER       0x08
#define SNO                 0x0C
#define HCN                 0x10
#define TICKET_MASK         0x14
#define MISC_CONTROL        0x18
#define GENERAL_IIC         0x1C
#define SECURITY_IIC        0x20
#define SIG_INPUT           0x24
#define SIG_NONCE_0         0x28
#define SIG_NONCE_1         0x2c
#define SIG_ID              0x30
#define SEC_CTRL_STATUS     0x34
#define MEMORY_STATUS       0x38
#define CORE_CMD_IN         0x3c
#define CORE_RESP_OUT       0x40
#define EXT_TEMP_SENSOR     0x44

//core command in value
#define CORE_CMD_ALL        (0x1 << 7)
#define CORE_INDEX(X)       (X & 0xf)
#define CORE_DATA(X)        (X & 0xff)
#define CLK_EN(X)           (X & 0x3)
#define LCM(X)              ((X & 0x3) << 1)
#define PM_START            (0x1 << 2)
#define PM_SEL(X)           (X & 0x3)           // 1:select vdd;0:select vss

//cmd type
#define CLOCK_EN_CTRL       0x00
#define CALC_MODE_SET       0x01
#define PRO_MONI_CTRL       0x02
#define TEMP_DIODE_SEL      0x03
#define VOLT_MONI_SEL       0x04

//Register bits value
#define CMD_TYPE            (0x2 << 5)
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

union REG_DATA
{
    struct MISC_CTRL_DATA
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
    } misc_ctrl_data;

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
    } general_iic_data;

    struct SECURITY_IIC_DATA
    {
		uint8_t reserved            :6;
		uint8_t rw_fail             :1;
		uint8_t busy                :1;

		uint8_t rw_ctrl             :1;  // 0:read  1:write
		uint8_t reserved1           :7;

		uint8_t regaddr;
		uint8_t data;
    } security_iic_data;

    struct SCS_DATA
    {
		uint8_t reserved;

		uint8_t rn_ctrl     :1;
		uint8_t reserved1   :7;

		uint8_t eprom_addr  :7;
		uint8_t addr_rf     :1;

		uint8_t disa_chip   :1;
		uint8_t sig_pass    :1;
		uint8_t wp          :1;
		uint8_t ecc_clken   :1;
		uint8_t sig_cnt     :4;
    } scs_data;

    struct CORE_CMD_DATA
    {
		uint8_t ana_sel     :5;
		uint8_t reserved    :2;
		uint8_t all         :1;

		uint8_t core_index  :4;
		uint8_t reserved1   :4;

		uint8_t cmd_type;
		uint8_t cmd_data;
    } core_cmd_data;

    struct TM_DATA
    {
		uint8_t reg_data[4];
    } tm_data;

    struct HCN_DATA
    {
		uint8_t reg_data[4];
    } hcn_data;

    struct PLL_DATA
    {
		uint8_t reg_data[4];
    } pll_data;

    struct SNO_DATA
    {
		uint8_t reg_data[4];
    } sno_data;

} __attribute__((packed, aligned(4)));

#define IIC_SLEEP					250 //ms

// TMP451 register
#define INT_TEMP_VALUE_HIGH_BYTE	0x00
#define EXT_TEMP_VALUE_HIGH_BYTE	0x01
#define STATUS						0x02
#define CONFIGURATION				0x03
#define EXT_TEMP_VALUE_LOW_BYTE		0x10
#define EXT_TEMP_OFFSET_HIGH_BYTE	0x11
#define EXT_TEMP_OFFSET_LOW_BYTE	0x12
#define INT_TEMP_VALUE_LOW_BYTE		0x15

// macro define about miner
#ifdef L3
#define CHAIN_ASIC_NUM                  36
#define HAVE_TEMP                       0xe7
#define HAVE_TEMP_2                     0xa8
#define HAVE_TEMP_3                     0x69
#else
#define ASICS_ON_CHAIN_MAX			72
#endif

#define CHAINS_MAX					4
#define DOMAIN_SIZE					12
#define BM1485_CORE_NUM				12
#define FANS_MAX					2
#define DEFAULT_FREQUENCY			350
#define DEFAULT_VOLTAGE				180 // '255' is lowest, '0' is maximum
#define MAX_NONCE_NUMBER_IN_FIFO	ASICS_ON_CHAIN_MAX*CHAINS_MAX*3
#define DEFAULT_BAUD_RATE			115200
#define DEVICE_DIFF					8
#define CHECK_SYSTEM_INTERVAL		1 // s

// fan control
#define WORKING_TEMPERATURE_MIN		5
#define WORKING_TEMPERATURE_MAX		80
#define FAN_PWM_MIN_VALUE			15
#define FAN_PWM_MAX_VALUE			255
#define FAN_CONTROL_PK				5.25
#define FAN_CONTROL_IK				1.26
#define FAN_CONTROL_DK				5.27
#define FAN_PWM_PERIOD_NS			100000

#define NONCE_BIN_RBUF_SIZE		64
#define WORK_QUEUE_SIZE			128
#define SCRYPTDATA_SIZE			76

typedef enum
{
	BUF_IDLE,
	BUF_READING,
	BUF_WRITING,
} buf_state_e;

typedef struct
{
	float ext_sensor_value;
	float int_sensor_value;
	int8_t offset;
} asic_temp_t;

struct nonce_ctx
{
	uint8_t nonce[4];
	uint8_t diff;   //Bit[7:6] reserved  Bit[5:0] diff
	uint8_t wc;     //Bit[7]: Reserved. Bit[6:0]: work count
	uint8_t crc5;   //Bit[7] fixed as 1. Bit[6]:sig  1: signature; 0:non signature. Bit[5] sig parity. Bit[4:0] crc5
	uint8_t chainid;
};

struct nonce_buf
{
	uint32_t p_wr;
	uint32_t p_rd;
	uint32_t nonce_num;
	uint8_t reserved[2];
	struct nonce_ctx nonce_data[MAX_NONCE_NUMBER_IN_FIFO];
	uint16_t crc16;
} __attribute__((packed, aligned(4)));

typedef struct
{
	pthread_t		uart_tx_thr;
	pthread_t		uart_rx_thr;
	int32_t			tty_fd;
	pthread_mutex_t	tty_rw_mutex;
	uint32_t		hwe_count;
	uint32_t		nonce_count;
	uint32_t		hash_rate;
	uint32_t	asic_hwe_count[ASICS_ON_CHAIN_MAX];
	uint32_t	asic_nonce_count[ASICS_ON_CHAIN_MAX];
	uint32_t	asic_hash_rate[ASICS_ON_CHAIN_MAX];
	uint8_t	asic_alive[ASICS_ON_CHAIN_MAX];
	uint32_t	freq;
	uint8_t	volt;
	uint8_t	chain_id;
	uint8_t	exist;
	uint8_t	number_of_asics;
	uint8_t	work_update_interval;
	asic_temp_t	temperature;
	asic_temp_t	temperature_prev;
	uint8_t	powered_on;
	uint8_t	new_block;
	uint8_t	glitch_detected;
} board_parameters_t;

typedef struct __attribute__((packed, aligned(4)))
{
	struct thr_info	*mining_control_thr;
	uint32_t		valid_nonce_count;
	uint32_t		hwe_count;
	float			temperature_max;
	uint16_t		fan_speed_value[FANS_MAX];
	uint8_t		fan_exist[FANS_MAX];
	uint8_t		number_of_chains;
	uint8_t		fan_num;
	uint8_t		addrInterval;
	uint8_t		max_asic_num_in_one_chain;
	uint8_t		baud;
	uint8_t		diff;
	int32_t			i2c_fd;
} device_t;

struct work_ltc
{
	uint8_t type;       //Bit[7:5]: Type,fixed as 0x01. Bit[4:1]:Reserved   Bit[0]:start nonce valid
	uint8_t length;     //data length from Byte0 to the end, whitout crc bytes.
	uint8_t wc_base;    //Bit[7]: Reserved. Bit[6:0]: Work count base
	uint8_t reserved;
	uint8_t s_data[SCRYPTDATA_SIZE];  // ScryptData
	uint16_t crc16;
};

struct work_buf
{
	uint8_t reserved[3];
	uint8_t p_wr;
	uint8_t p_rd;
	uint8_t work_num;
	struct work_ltc workdata[CHAINS_MAX];
	uint16_t crc16;
} __attribute__((packed, aligned(4)));

typedef struct
{
	uint16_t freq;
	uint32_t vilpll;
} pll_freq_t;

// == TUNER
#define AUTOTUNER_PREPARE_INTERVAL 55 // sec
typedef enum
{
	AUTOTUNER_DISABLED,
	AUTOTUNER_PREPARE,
	AUTOTUNER_WORKING,
	AUTOTUNER_DONE
} autotuner_state_e;

typedef struct
{
	autotuner_state_e	state;
	uint32_t			chain_hwe_prev[CHAINS_MAX], chain_pll_index_last_time[CHAINS_MAX];
	uint8_t			chain_tunable[CHAINS_MAX], chain_tuned[CHAINS_MAX];
	uint8_t			target_volt[CHAINS_MAX];
	uint32_t			fan_pwm_value;
	float				fan_control_i, fan_control_p_prev, target_temperature;
	uint8_t			fan_pid_control_enabled;
} autotuner_t;
// ==

extern device_t *device;

extern int opt_target_temp;
extern int opt_target_freq;
extern int opt_freq[CHAINS_MAX];
extern int opt_volt[CHAINS_MAX];
extern int opt_core_temp;
extern bool opt_enable_autotuner;

#endif // DRIVER_BITMAIN_L3_H
