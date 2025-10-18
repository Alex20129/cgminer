#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/select.h>
#include <linux/i2c-dev.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <byteswap.h>
#include <errno.h>

#include <gpiod.h>

#include "../crc.h"
#include "../cgminer.h"

#include "driver-s21.h"
#include "pic.h"
#include "autotuner.h"

pthread_t read_temperature_thread=0;
pthread_t read_fan_rpm_thread=0;
pthread_t autotuner_thread=0;

const int tty[CHAINS_MAX]={1, 2, 3, 0};

// gpio 439 line 28
// gpio 440 line 29
// gpio 441 line 30
const int board_plug_gpio_line[CHAINS_MAX]={28, 29, 30, 0};

// gpio 447 line 36
// gpio 448 line 37
// gpio 449 line 38
// gpio 450 line 39
const int fan_rpm_gpio_line[S21_FAN_NUM]={36, 37, 38, 39};

// gpio 454 line 43
// gpio 455 line 44
// gpio 456 line 45
const int board_reset_gpio_line[CHAINS_MAX]={43, 44, 45, 0};

const int i2c_lm75a_addr[CHAINS_MAX][TEMPERATURE_SENSORS_PER_BOARD]={{0x48, 0x4C}, {0x49, 0x4D}, {0x4a, 0x4E}};

int opt_autotuner_mode=DEFAULT_AUTOTUNER_MODE;
int opt_frequency=S21_FREQUENCY_DEFAULT;
int opt_target_hashrate=S21_TARGET_HASHRATE_DEFAULT;
int opt_target_power_consumption=S21_TARGET_POWER_CONSUMPTION_DEFAULT;
int opt_fan_speed_percentage=S21_FAN_SPEED_PERCENTAGE_DEFAULT;
float opt_target_temperature=DEFAULT_TARGET_TEMPERATURE;
float opt_psu_voltage=DEFAULT_PSU_VOLTAGE;

pthread_mutex_t i2c_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t works_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t nonce_fifo_mutex=PTHREAD_MUTEX_INITIALIZER;

bool update_asic_num=false;

device_t	*g_device;
board_parameters_t	*g_boards[CHAINS_MAX];

nonce_buf_t			g_nonce_fifo;

struct gpiod_chip *g_gpio_chip=NULL;
static const char g_gpio_chip_dev_full_path[]="/dev/gpiochip1";

static const pll_freq_t freq_pll_1368[97] =
{
	{50.00,	20000,	0x40A80265},
	{56.25,	22500,	0x40A20255},
	{62.50,	25000,	0x40AF0264},
	{68.75,	27500,	0x40A50254},
	{75.00,	30000,	0x40A80263},
	{81.25,	32500,	0x40B60263},
	{87.50,	35000,	0x40A80253},
	{93.75,	37500,	0x40B40253},
	{100.00,	40000,	0x40A80262},
	{106.25,	42500,	0x40AA0243},
	{112.50,	45000,	0x40A20252},
	{118.75,	47500,	0x40AB0252},
	{125.00,	50000,	0x40B40252},
	{131.25,	52500,	0x40BD0252},
	{137.50,	55000,	0x40A50242},
	{143.75,	57500,	0x40A10261},
	{150.00,	60000,	0x40A80261},
	{156.25,	62500,	0x40AF0261},
	{162.50,	65000,	0x40B60261},
	{168.75,	67500,	0x40A20251},
	{175.00,	70000,	0x40A80251},
	{181.25,	72500,	0x40AE0251},
	{187.50,	75000,	0x40B40251},
	{193.75,	77500,	0x40BA0251},
	{200.00,	80000,	0x40A00241},
	{206.25,	82500,	0x40A50241},
	{212.50,	85000,	0x40AA0241},
	{218.75,	87500,	0x40AF0241},
	{225.00,	90000,	0x40B40241},
	{231.25,	92500,	0x40B90241},
	{237.50,	95000,	0x40BE0241},
	{243.75,	97500,	0x50C30241},
	{250.00,	100000,	0x40A00231},
	{256.25,	102500,	0x40A40231},
	{262.50,	105000,	0x40A80231},
	{268.75,	107500,	0x40AC0231},
	{275.00,	110000,	0x40B00231},
	{281.25,	112500,	0x40B40231},
	{287.50,	115000,	0x40A10260},
	{293.75,	117500,	0x40BC0231},
	{300.00,	120000,	0x40A80260},
	{306.25,	122500,	0x50C40231},
	{312.50,	125000,	0x40AF0260},
	{318.75,	127500,	0x50CC0231},
	{325.00,	130000,	0x40B60260},
	{331.25,	132500,	0x50D40231},
	{337.50,	135000,	0x40A20250},
	{343.75,	137500,	0x40A50250},
	{350.00,	140000,	0x40A80250},
	{356.25,	142500,	0x40AB0250},
	{362.50,	145000,	0x40AE0250},
	{368.75,	147500,	0x40B10250},
	{375.00,	150000,	0x40B40250},
	{381.25,	152500,	0x40B70250},
	{387.50,	155000,	0x40BA0250},
	{393.75,	157500,	0x40BD0250},
	{400.00,	160000,	0x40A00240},
	{406.25,	162500,	0x50C30250},
	{412.50,	165000,	0x40A50240},
	{418.75,	167500,	0x50C90250},
	{425.00,	170000,	0x40AA0240},
	{431.25,	172500,	0x50CF0250},
	{437.50,	175000,	0x40AF0240},
	{443.75,	177500,	0x50D50250},
	{450.00,	180000,	0x40B40240},
	{456.25,	182500,	0x50DB0250},
	{462.50,	185000,	0x40B90240},
	{468.75,	187500,	0x50E10250},
	{475.00,	190000,	0x40BE0240},
	{481.25,	192500,	0x50E70250},
	{487.50,	195000,	0x50C30240},
	{493.75,	197500,	0x50C60240},
	{500.00,	200000,	0x40A00230},
	{506.25,	202500,	0x40A20230},
	{512.50,	205000,	0x40A40230},
	{518.75,	207500,	0x40A60230},
	{525.00,	210000,	0x40A80230},
	{531.25,	212500,	0x40AA0230},
	{537.50,	215000,	0x40AC0230},
	{543.75,	217500,	0x40AE0230},
	{550.00,	220000,	0x40B00230},
	{556.25,	222500,	0x40B20230},
	{562.50,	225000,	0x40B40230},
	{568.75,	227500,	0x40B60230},
	{575.00,	230000,	0x40B80230},
	{581.25,	232500,	0x40BA0230},
	{587.50,	235000,	0x40BC0230},
	{593.75,	237500,	0x40BE0230},
	{600.00,	240000,	0x50C00230},
	{606.25,	242500,	0x50C20230},
	{612.50,	245000,	0x50C40230},
	{618.75,	247500,	0x50C60230},
	{625.00,	250000,	0x50C80230},
	{631.25,	252500,	0x50CA0230},
	{637.50,	255000,	0x50CC0230},
	{643.75,	257500,	0x50CE0230},
	{650.00,	260000,	0x50D00230},
};

static void reset_board(uint8_t board_id)
{
	struct gpiod_line *board_reset_line;
	if(!g_gpio_chip)
	{
		g_gpio_chip=gpiod_chip_open(g_gpio_chip_dev_full_path);
	}
	if(!g_gpio_chip)
	{
		applog(LOG_ERR, "Error: g_gpio_chip is nullptr");
		return;
	}
	board_reset_line=gpiod_chip_get_line(g_gpio_chip, board_reset_gpio_line[board_id]);
	if(!board_reset_line)
	{
		applog(LOG_ERR, "gpiod_chip_get_line(%i) error", board_reset_gpio_line[board_id]);
		return;
	}
	if(gpiod_line_request_output(board_reset_line, "reset_board", 1)<0)
	{
		applog(LOG_ERR, "gpiod_line_request_output() error");
		return;
	}
	gpiod_line_set_value(board_reset_line, 0);
	usleep(50000);
	gpiod_line_set_value(board_reset_line, 1);
	gpiod_line_release(board_reset_line);
}

void check_asic_reg_all_chains(uint8_t chip_addr, uint8_t reg_addr, uint8_t all_chips)
{
	uint8_t rdreg_buf[7], board_id;
//	uint8_t byte;
	rdreg_buf[0]=PACKET_PREAMBLE_1;
	rdreg_buf[1]=PACKET_PREAMBLE_2;

	rdreg_buf[2]=CMD_TYPE | GET_STATUS; // 0x42
	if(all_chips)
	{
		rdreg_buf[2] |= CMD_ALL;		// 0x52
	}
	rdreg_buf[3]=CMD_LENGTH;
	rdreg_buf[4]=chip_addr;
	rdreg_buf[5]=reg_addr;
	rdreg_buf[6]=crc5(rdreg_buf+PACKET_PREAMBLE_LENGTH, 4*8);

//	fprintf(stdout, "rdreg_buf: ");
//	for(byte=0; byte<7; byte++)
//	{
//		fprintf(stdout, "%02X ", rdreg_buf[byte]);
//	}
//	fprintf(stdout, "\n");

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
    {
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
        {
			pthread_mutex_lock(&g_boards[board_id]->tty_rw_mutex);
			write(g_boards[board_id]->tty_fd, rdreg_buf, CMD_LENGTH+PACKET_PREAMBLE_LENGTH);
			pthread_mutex_unlock(&g_boards[board_id]->tty_rw_mutex);
        }
    }
}

uint32_t pll_setting_index_from_frequency(float frequency)
{
	uint32_t i;
	for(i=0; i<sizeof(freq_pll_1368)/sizeof(freq_pll_1368[0]); i++)
	{
		if(freq_pll_1368[i].frequency>=frequency)
		{
			return(i);
		}
	}
	i--;
	return(i);
}

uint32_t pll_setting_index_from_hashrate(uint32_t hashrate)
{
	uint32_t i;
	for(i=0; i<sizeof(freq_pll_1368)/sizeof(freq_pll_1368[0]); i++)
	{
		if(freq_pll_1368[i].hashrate>=hashrate)
		{
			return(i);
		}
	}
	i--;
	return(i);
}

void check_chains()
{
//	uint8_t chain_id;
//	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
//    {
		//WIP
		//check chains by GPIO
//    }
	g_boards[0]->exist=1;
	g_boards[1]->exist=1;
	g_boards[2]->exist=1;

	g_boards[0]->active=1;
	g_boards[1]->active=1;
	g_boards[2]->active=1;

	g_device->hash_boards_active=3;
}

void tty_init_one_chain(uint8_t chain_id, uint32_t baud_rate)
{
	char dev_fname[64];
	struct termios options;
	sprintf(dev_fname, TTY_DEVICE_TEMPLATE, tty[chain_id]);

	if(g_boards[chain_id]->tty_fd>0)
	{
		close(g_boards[chain_id]->tty_fd);
		g_boards[chain_id]->tty_fd=0;
	}

	if(g_boards[chain_id]->tty_fd==0)
	{
		g_boards[chain_id]->tty_fd=open(dev_fname, O_RDWR|O_NOCTTY);
	}

	if(g_boards[chain_id]->tty_fd<0)
	{
		applog(LOG_ERR, "Chain[%u] open %s failed", chain_id, dev_fname);
		g_boards[chain_id]->tty_fd=0;
		exit(EXIT_FAILURE);
	}

	if(tcgetattr(g_boards[chain_id]->tty_fd, &options)<0)
	{
		applog(LOG_ERR, "tcgetattr() returned -1");
		exit(EXIT_FAILURE);
	}
	cfsetispeed(&options, baud_rate);
	cfsetospeed(&options, baud_rate);
	options.c_cflag &= ~(CSIZE | PARENB);
	options.c_cflag |= CS8;
	options.c_cflag |= CREAD;
	options.c_cflag |= CLOCAL;
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	options.c_cc[VTIME]=0;
	options.c_cc[VMIN]=7;
	if(tcsetattr(g_boards[chain_id]->tty_fd, TCSANOW, &options)<0)
	{
		applog(LOG_ERR, "tcsetattr() returned -1");
		exit(EXIT_FAILURE);
	}

	if(tcflush(g_boards[chain_id]->tty_fd, TCIOFLUSH)<0)
	{
		applog(LOG_ERR, "tcflush() returned -1");
		exit(EXIT_FAILURE);
	}
}

void set_config(uint8_t chain_id, uint8_t all, uint8_t chip_id, uint8_t reg_addr, void *reg_data)
{
	uint8_t cmd_buf[11], asic_addr=(chip_id*g_device->asic_addr_interval)&0xFF;
//	uint8_t byte;

	cmd_buf[0]=PACKET_PREAMBLE_1;
	cmd_buf[1]=PACKET_PREAMBLE_2;
	cmd_buf[2]=CMD_TYPE | SET_CONFIG;
	if(all)
	{
		cmd_buf[2] |= CMD_ALL;
	}
	cmd_buf[3]=CONFIG_LENGTH;
	if(all)
	{
		cmd_buf[4]=0;
	}
	else
	{
		cmd_buf[4]=asic_addr;
	}
	cmd_buf[5]=reg_addr;
	memcpy(&cmd_buf[6], reg_data, 4);
	cmd_buf[10]=crc5(cmd_buf+2, 8*8);

//	fprintf(stdout, "cmd_buf: ");
//	for(byte=0; byte<10; byte++)
//	{
//		fprintf(stdout, "%02X ", cmd_buf[byte]);
//	}
//	fprintf(stdout, "%02X\n", cmd_buf[byte]);

	pthread_mutex_lock(&g_boards[chain_id]->tty_rw_mutex);
	write(g_boards[chain_id]->tty_fd, cmd_buf, CONFIG_LENGTH+PACKET_PREAMBLE_LENGTH);
	pthread_mutex_unlock(&g_boards[chain_id]->tty_rw_mutex);
}

void chain_inactive(uint8_t chain_id)
{
	uint8_t cmd_buf[7];
	cmd_buf[0]=PACKET_PREAMBLE_1;
	cmd_buf[1]=PACKET_PREAMBLE_2;
	cmd_buf[2]=CMD_ALL | CMD_TYPE | CHAIN_INACTIVE;
	cmd_buf[3]=CMD_LENGTH;
	cmd_buf[4]=0;
	cmd_buf[5]=0;
	cmd_buf[6]=crc5(cmd_buf+PACKET_PREAMBLE_LENGTH, 4*8);
	pthread_mutex_lock(&g_boards[chain_id]->tty_rw_mutex);
	write(g_boards[chain_id]->tty_fd, cmd_buf, CMD_LENGTH+PACKET_PREAMBLE_LENGTH);
	pthread_mutex_unlock(&g_boards[chain_id]->tty_rw_mutex);
}

void set_asic_address_one_chain(uint8_t chain_id)
{
	uint8_t chip_addr, cmd_buf[7];
	unsigned int j;

	cmd_buf[0]=PACKET_PREAMBLE_1;
	cmd_buf[1]=PACKET_PREAMBLE_2;
	cmd_buf[2]=CMD_TYPE | SET_ADDR;
	cmd_buf[3]=CMD_LENGTH;

	pthread_mutex_lock(&g_boards[chain_id]->tty_rw_mutex);
	for(j=0, chip_addr=0; j<ASICS_PER_BOARD; j++)
	{
		cmd_buf[4]=chip_addr;
		cmd_buf[5]=0x00;
		cmd_buf[6]=crc5(cmd_buf+PACKET_PREAMBLE_LENGTH, 4*8);
		write(g_boards[chain_id]->tty_fd, cmd_buf, CMD_LENGTH+PACKET_PREAMBLE_LENGTH);
		chip_addr+=g_device->asic_addr_interval;
		usleep(25000);
	}
	pthread_mutex_unlock(&g_boards[chain_id]->tty_rw_mutex);
}

void set_nonce_offset_one_chain(uint8_t chain_id, uint16_t step)
{
	struct NONCE_OFFSET_DATA nonce_offset_data;
	uint16_t chip_id;
	nonce_offset_data.reg_data1=0x0080;
	for(chip_id=0; chip_id<ASICS_PER_BOARD; chip_id++)
	{
		nonce_offset_data.reg_data2=bswap_16(step*chip_id);
		fprintf(stdout, "nonce_offset_data:0x%08X\n", *(uint32_t *)&nonce_offset_data);
		set_config(chain_id, 0, chip_id, NONCE_OFFSET, &nonce_offset_data);
		usleep(10000);
	}
}

void set_hash_counter_one_chain(uint8_t chain_id, uint32_t hash_counter)
{
	struct HASH_COUNTER_DATA hash_counter_data;
	*(uint32_t *)hash_counter_data.reg_data=bswap_32(hash_counter);
	fprintf(stdout, "hash_counter_data:0x%08X\n", *(uint32_t *)&hash_counter_data);
	set_config(chain_id, 1, 0, HASH_COUNTER, &hash_counter_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_ticket_mask_one_chain(uint8_t chain_id, uint32_t ticket_mask)
{
	struct TICKET_MASK_DATA ticket_mask_data;
	*(uint32_t *)ticket_mask_data.reg_data=bswap_32(ticket_mask);
	fprintf(stdout, "ticket_mask_data:0x%08X\n", *(uint32_t *)&ticket_mask_data);
	set_config(chain_id, 1, 0, TICKET_MASK, &ticket_mask_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_misc_ctrl_one_chip(uint8_t chain_id, uint8_t chip_id, uint8_t data_0, uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
	struct MISC_CONTROL_DATA misc_ctrl_data;
	((uint8_t *)&misc_ctrl_data)[0]=data_0;
	((uint8_t *)&misc_ctrl_data)[1]=data_1;
	((uint8_t *)&misc_ctrl_data)[2]=data_2;
	((uint8_t *)&misc_ctrl_data)[3]=data_3;
	set_config(chain_id, 0, chip_id, MISC_CONTROL, &misc_ctrl_data);
	usleep(10000);
}

void set_misc_ctrl_one_chain(uint8_t chain_id, uint8_t data_0, uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
	struct MISC_CONTROL_DATA misc_ctrl_data;
	((uint8_t *)&misc_ctrl_data)[0]=data_0;
	((uint8_t *)&misc_ctrl_data)[1]=data_1;
	((uint8_t *)&misc_ctrl_data)[2]=data_2;
	((uint8_t *)&misc_ctrl_data)[3]=data_3;
	fprintf(stdout, "misc_ctrl_data:0x%08X\n", *(uint32_t *)&misc_ctrl_data);
	set_config(chain_id, 1, 0, MISC_CONTROL, &misc_ctrl_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_core_ctrl_one_chip(uint8_t chain_id, uint8_t chip_id, uint8_t data_0, uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
	struct CORE_CONTROL_DATA core_ctrl_data;
	core_ctrl_data.reg_data[0]=data_0;
	core_ctrl_data.reg_data[1]=data_1;
	core_ctrl_data.reg_data[2]=data_2;
	core_ctrl_data.reg_data[3]=data_3;
	set_config(chain_id, 0, chip_id, CORE_CONTROL, &core_ctrl_data);
	usleep(10000);
}

void set_core_ctrl_one_chain(uint8_t chain_id, uint8_t data_0, uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
	struct CORE_CONTROL_DATA core_ctrl_data;
	core_ctrl_data.reg_data[0]=data_0;
	core_ctrl_data.reg_data[1]=data_1;
	core_ctrl_data.reg_data[2]=data_2;
	core_ctrl_data.reg_data[3]=data_3;
	fprintf(stdout, "core_ctrl_data:0x%08X\n", *(uint32_t *)&core_ctrl_data);
	set_config(chain_id, 1, 0, CORE_CONTROL, &core_ctrl_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_frequency_by_index_on_chain(uint8_t chain_id, uint32_t pll_index)
{
	struct PLL_DATA pll_data;
	uint32_t vilpll_bswapped=bswap_32(freq_pll_1368[pll_index].vilpll);
	if((int32_t)g_boards[chain_id]->frequency==(int32_t)freq_pll_1368[pll_index].frequency)
	{
		return;
	}
	g_boards[chain_id]->frequency=freq_pll_1368[pll_index].frequency;
	memcpy(pll_data.reg_data, &vilpll_bswapped, 4);
	fprintf(stdout, "Board[%u] set_frequency:%u MHz\n", chain_id, g_boards[chain_id]->frequency);
	set_config(chain_id, 1, 0, PLL_PARAMETER, &pll_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_analog_mux_control_one_chain(uint8_t chain_id, uint32_t mux_control)
{
	struct ANALOG_MUX_CONTROL_DATA analog_mux_control_data;
	*(uint32_t *)analog_mux_control_data.reg_data=bswap_32(mux_control);
	fprintf(stdout, "analog_mux_control_data:0x%08X\n", *(uint32_t *)&analog_mux_control_data);
	set_config(chain_id, 1, 0, ANALOG_MUX_CONTROL, &analog_mux_control_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_io_driver_strength_one_chain(uint8_t chain_id)
{
	struct IO_DRIVER_STRENGTH_DATA io_driver_strength_data;
	uint8_t chip_id;
	*(uint32_t *)io_driver_strength_data.reg_data=0x11111102;
	fprintf(stdout, "io_driver_strength_data:0x%08X\n", *(uint32_t *)&io_driver_strength_data);
	set_config(chain_id, 1, 0, IO_DRIVER_STRENGTH, &io_driver_strength_data);
	usleep(CHAIN_CONFIG_INTERVAL);
	*(uint32_t *)io_driver_strength_data.reg_data=0x11F11102;
	fprintf(stdout, "io_driver_strength_data:0x%08X\n", *(uint32_t *)&io_driver_strength_data);

	for(chip_id=1; chip_id<ASICS_PER_BOARD; chip_id+=DOMAIN_SIZE)
	{
		set_config(chain_id, 0, ASICS_PER_BOARD-chip_id, IO_DRIVER_STRENGTH, &io_driver_strength_data);
	}

	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_pll3_parameter_one_chain(uint8_t chain_id, uint32_t pll3_parameter)
{
	struct PLL3_PARAMETER_DATA pll3_parameter_data;
	*(uint32_t *)pll3_parameter_data.reg_data=bswap_32(pll3_parameter);
	fprintf(stdout, "pll3_parameter_data:0x%08X\n", *(uint32_t *)&pll3_parameter_data);
	set_config(chain_id, 1, 0, PLL3_PARAMETER, &pll3_parameter_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_fast_uart_configuration_one_chain(uint8_t chain_id, uint32_t fast_uart_configuration)
{
	struct FAST_UART_CONFIGURATION_DATA fast_uart_configuration_data;
	*(uint32_t *)fast_uart_configuration_data.reg_data=bswap_32(fast_uart_configuration);
	fprintf(stdout, "fast_uart_configuration_data:0x%08X\n", *(uint32_t *)&fast_uart_configuration_data);
	set_config(chain_id, 1, 0, FAST_UART_CONFIGURATION, &fast_uart_configuration_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_uart_relay_one_chain(uint8_t chain_id, uint8_t shift)
{
	struct UART_RELAY_DATA uart_relay_data;
	uint32_t data_swapped;
	uint8_t chip_id;
	uart_relay_data.unknown1=0x03;
	uart_relay_data.shift=shift;
	for(chip_id=1; chip_id<ASICS_PER_BOARD; chip_id+=DOMAIN_SIZE)
	{
		data_swapped=bswap_32(*(uint32_t *)(&uart_relay_data));
		set_config(chain_id, 0, ASICS_PER_BOARD-chip_id-DOMAIN_SIZE+1, UART_RELAY, &data_swapped);
		set_config(chain_id, 0, ASICS_PER_BOARD-chip_id, UART_RELAY, &data_swapped);
		uart_relay_data.shift+=DOMAIN_SIZE;
	}
}

void set_version_mask_one_chain(uint8_t chain_id, uint32_t version_mask)
{
	struct VERSION_MASK_DATA version_mask_data;
	*(uint32_t *)version_mask_data.reg_data=bswap_32(version_mask);
	fprintf(stdout, "version_mask_data:0x%08X\n", *(uint32_t *)&version_mask_data);
	set_config(chain_id, 1, 0, VERSION_MASK, &version_mask_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_unknown_reg_a8_one_chip(uint8_t chain_id, uint8_t chip_id, uint8_t data_0, uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
	struct UNKNOWN_REG_A8_DATA unknown_reg_a8_data;
	unknown_reg_a8_data.reg_data[0]=data_0;
	unknown_reg_a8_data.reg_data[1]=data_1;
	unknown_reg_a8_data.reg_data[2]=data_2;
	unknown_reg_a8_data.reg_data[3]=data_3;
	set_config(chain_id, 0, chip_id, UNKNOWN_REG_A8, &unknown_reg_a8_data);
	usleep(15000);
}

void set_unknown_reg_a8_one_chain(uint8_t chain_id, uint8_t data_0, uint8_t data_1, uint8_t data_2, uint8_t data_3)
{
	struct UNKNOWN_REG_A8_DATA unknown_reg_a8_data;
	unknown_reg_a8_data.reg_data[0]=data_0;
	unknown_reg_a8_data.reg_data[1]=data_1;
	unknown_reg_a8_data.reg_data[2]=data_2;
	unknown_reg_a8_data.reg_data[3]=data_3;
	fprintf(stdout, "unknown_reg_a8_data:0x%08X\n", *(uint32_t *)&unknown_reg_a8_data);
	set_config(chain_id, 1, 0, UNKNOWN_REG_A8, &unknown_reg_a8_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void i2c_init()
{
	g_device->i2c_fd=open(I2C_DEVICE, O_RDWR | O_NONBLOCK);
	if(g_device->i2c_fd<0)
    {
		applog(LOG_ERR, "i2c init error. Cannot open %s", I2C_DEVICE);
    }
	else
	{
		fprintf(stdout, "i2c init success.\n");
	}
}

static inline uint32_t get_bytes_num_in_fd(const int fd)
{
	uint32_t rx_len;
	if(ioctl(fd, FIONREAD, &rx_len)==0)
	{
		return rx_len;
	}
	return(0);
}

uint8_t extract_asic_id_from_nonce(uint32_t nonce)
{
	nonce=nonce << 7;
	return (nonce >> 16) * ASICS_PER_BOARD * 256 >> 24;
}

static inline void clear_nonce_buf()
{
	pthread_mutex_lock(&nonce_fifo_mutex);
	g_nonce_fifo.p_wr=0;
	g_nonce_fifo.p_rd=0;
	g_nonce_fifo.nonce_num=0;
	pthread_mutex_unlock(&nonce_fifo_mutex);
}

void process_hash_rate_summary()
{
	static time_t last_hr_calc_t=0;
	uint64_t asic_new_hr, t_interval;
	uint8_t board_id, asic_id;
	time_t now=time(NULL);
	t_interval=now-last_hr_calc_t;
	if(t_interval<10)
	{
		return;
	}
	last_hr_calc_t=now;
//	g_device->hash_rate=0;
	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			for(asic_id=0; asic_id<ASICS_PER_BOARD; asic_id++)
			{
				g_boards[board_id]->asic_nonce_counter[asic_id]/=2;

				asic_new_hr=g_boards[board_id]->asic_diff_a[asic_id];
				g_boards[board_id]->asic_diff_a[asic_id]=0;
				asic_new_hr/=t_interval;
				asic_new_hr*=0xFFFFFFFFULL;
				asic_new_hr/=1000000ULL;
				asic_new_hr/=2000000ULL;

				g_boards[board_id]->asic_hash_rate[asic_id+1]+=asic_new_hr;
				g_boards[board_id]->asic_hash_rate[asic_id+1]/=2;

				g_boards[board_id]->hash_rate+=g_boards[board_id]->asic_hash_rate[asic_id+1];
			}
			g_boards[board_id]->hash_rate/=2;
		}
//		g_device->hash_rate+=g_boards[board_id]->hash_rate;
	}
}

static inline uint8_t *detect_packet(uint8_t *buffer, uint32_t buffer_size, uint32_t packet_length)
{
	uint8_t *scan_ptr;
//	uint8_t crc_packet, crc_true;
	for(scan_ptr=buffer; scan_ptr+packet_length<buffer+buffer_size; scan_ptr++)
	{
		if(scan_ptr[0]==PACKET_PREAMBLE_2 && scan_ptr[1]==PACKET_PREAMBLE_1)
		{
			if(scan_ptr[packet_length]==PACKET_PREAMBLE_2)
			{
				return(scan_ptr);
			}
		}
//			crc_true=crc5(scan_ptr+PACKET_PREAMBLE_LENGTH, 8*(packet_length-PACKET_PREAMBLE_LENGTH)-5); //data minus 5-bit CRC
//			crc_packet=scan_ptr[packet_length-1] & 0x1F; //5-bit CRC
//			if(crc_packet==crc_true)
//			{
//				fprintf(stdout, "CRC5 ok, should be 0x%02x, received 0x%02x\n", crc_true, crc_packet);
//				return(scan_ptr);
//			}
//			else
//			{
//				applog(log_err, "CRC5 error, should be 0x%02x, received 0x%02x", crc_true, crc_packet);
//			}
	}
	return(NULL);
}

//void save_temperature_log(uint8_t board_id)
//{
//	temperature_log_t *t_log=&g_boards[board_id]->temperature_log;
//	int i, entry_index, t_log_fd;
//	char t_log_fname[32];
//	sprintf(t_log_fname, TEMPERATURE_LOG_PATH_TEMPLATE "tlog_b%u.json", board_id);
//	t_log_fd=open(t_log_fname , O_WRONLY|O_CREAT|O_TRUNC, 0666);
//	if(t_log_fd<0)
//	{
//		applog(LOG_ERR, "Cannon open %s", t_log_fname);
//		return;
//	}
//	json_t *t_log_json=json_object();
//	json_t *t_log_entries_array_json=json_array();
//	json_t *t_log_entry_json;
//	entry_index=t_log->last_entry_index;
//	for(i=0; i<TEMPERATURE_LOG_SIZE; i++, entry_index++)
//	{
//		if(entry_index>=TEMPERATURE_LOG_SIZE)
//		{
//			entry_index=0;
//		}
//		t_log_entry_json=json_object();
//		json_object_set_new(t_log_entry_json, "time", json_integer(t_log->entries[entry_index].time));
//		json_object_set_new(t_log_entry_json, "t1", json_real(t_log->entries[entry_index].sensor_data[0]));
//		json_object_set_new(t_log_entry_json, "t2", json_real(t_log->entries[entry_index].sensor_data[1]));
//		json_object_set_new(t_log_entry_json, "t3", json_real(t_log->entries[entry_index].sensor_data[2]));
//		json_array_append_new(t_log_entries_array_json, t_log_entry_json);
//	}
//	json_object_set_new(t_log_json, "temperature_log", t_log_entries_array_json);
//	json_dumpfd(t_log_json, t_log_fd, JSON_COMPACT);
//	close(t_log_fd);
//	json_decref(t_log_json);
//}

//void update_temperature_log(uint8_t board_id, uint8_t sensor_id, float temperature)
//{
//	temperature_log_t *t_log=&g_boards[board_id]->temperature_log;
//	time_t current_time=time(NULL);
//	if(0==current_time % TEMPERATURE_LOG_MEASUREMENT_INTERVAL)
//	{
//		if(t_log->update_index==t_log->last_entry_index)
//		{
//			t_log->last_entry_index++;
//			if(t_log->last_entry_index>=TEMPERATURE_LOG_SIZE)
//			{
//				t_log->last_entry_index=0;
//			}
//			if(0==current_time % TEMPERATURE_LOG_SAVE_INTERVAL)
//			{
//				save_temperature_log(board_id);
//			}
//			t_log->entries[t_log->last_entry_index].time=current_time;
//		}
//	}
//	else
//	{
//		t_log->update_index=t_log->last_entry_index;
//	}
//	t_log->entries[t_log->last_entry_index].sensor_data[sensor_id]=temperature;
//}

float read_temperature_lm75a(uint8_t board_id, uint8_t sensor_id)
{
	uint8_t lm75a_reply[2], lm75a_reg_addr[1]={0};
	float temperature;
	pthread_mutex_lock(&i2c_mutex);
	if(ioctl(g_device->i2c_fd, I2C_SLAVE, i2c_lm75a_addr[board_id][sensor_id])<0)
	{
		applog(LOG_ERR, "ioctl() error: cannot select %02X", i2c_lm75a_addr[board_id][sensor_id]);
		return;
	}
	write(g_device->i2c_fd, lm75a_reg_addr, 1);
	read(g_device->i2c_fd, lm75a_reply, 2);
	pthread_mutex_unlock(&i2c_mutex);
	temperature=lm75a_reply[1];
	temperature/=256.0;
	temperature+=lm75a_reply[0];
	return(temperature);
}

uint32_t configAsicChipSensor(uint8_t enableSensor, uint8_t enableCalibration, uint8_t temperature, uint8_t voltage)
{
	uint32_t sensorConfig;
	uint32_t sensorConfig1;
	uint32_t sensorConfig2;
	uint32_t sensorConfig3;
	sensorConfig=
	sensorConfig1=
	sensorConfig2=
	sensorConfig3=0;
	if(enableSensor )
	{
		sensorConfig1=0x20000;
	}
	if(!enableSensor )
	{
		sensorConfig1=sensorConfig1 & 0xFFFDFFFF | ((sensorConfig1 & 1) << 17);
	}
	if(enableCalibration )
	{
		sensorConfig2=sensorConfig1 | 0x1000000;
	}
	else
	{
		sensorConfig2=sensorConfig1 & 0xFEFFFFFF;
	}
	if(temperature )
	sensorConfig3=sensorConfig2 | 0x10000000;
	else
	sensorConfig3=sensorConfig2 & 0xEFFFFFFF;
	if(voltage )
	sensorConfig=sensorConfig3 | 0x80000000;
	else
	sensorConfig=sensorConfig3 & 0x7FFFFFFF;

	return(sensorConfig);
}

void *read_temperature_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	uint8_t board_id, sensor_id;
	uint32_t reg_value_data;

	while(42)
	{
		for(board_id=0; board_id<CHAINS_MAX; board_id++)
		{
			if(g_boards[board_id]->exist && g_boards[board_id]->active)
			{
				reg_value_data=configAsicChipSensor(1, 0, 0, 0);
				reg_value_data=bswap_32(reg_value_data);
				set_config(board_id, 1, 0, CHIP_SENSOR_CONFIG, &reg_value_data);
				usleep(10000);

//				reg_value_data=configAsicChipSensor(1, 1, 0, 0);
//				reg_value_data=bswap_32(reg_value_data);
//				set_config(board_id, 1, 0, CHIP_SENSOR_CONFIG, &reg_value_data);
//				usleep(10000);

				reg_value_data=configAsicChipSensor(1, 0, 1, 0);
				reg_value_data=bswap_32(reg_value_data);
				set_config(board_id, 1, 0, CHIP_SENSOR_CONFIG, &reg_value_data);
				usleep(10000);

//				0x00000080;
//				reg_value_data=configAsicChipSensor(0, 0, 0, 1);
//				reg_value_data=bswap_32(reg_value_data);
//				set_config(board_id, 1, 0, CHIP_SENSOR_CONFIG, &reg_value_data);
//				usleep(10000);
			}
		}
		check_asic_reg_all_chains(0, CHIP_SENSOR_MEASUREMENT_DATA, 1);
		usleep(400000);

		float t;
		for(board_id=0; board_id<CHAINS_MAX; board_id++)
		{
			if(g_boards[board_id]->exist && g_boards[board_id]->active)
			{
				for(sensor_id=0; sensor_id<TEMPERATURE_SENSORS_PER_BOARD; sensor_id++)
				{
					t=read_temperature_lm75a(board_id, sensor_id);
					g_boards[board_id]->temperature[sensor_id]=t;
//					update_temperature_log(board_id, sensor_id, t);
				}
			}
		}
		usleep(400000);
	}
	return(NULL);
}

void *read_fan_rpm_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	uint8_t fan_id, tacho_event_counter=0;
	float interval_sec;
	struct gpiod_line *fan_signal_line[S21_FAN_NUM];
	struct gpiod_line_event tacho_event;
	struct timeval now, last_tacho_event_time, elapsed;
	struct timespec tacho_signal_timeout;
	tacho_signal_timeout.tv_sec=1;
	if(!g_gpio_chip)
	{
		g_gpio_chip=gpiod_chip_open(g_gpio_chip_dev_full_path);
	}
	if(!g_gpio_chip)
	{
		applog(LOG_ERR, "Error: g_gpio_chip is nullptr");
		return(NULL);
	}
	for(fan_id=0; fan_id<S21_FAN_NUM; fan_id++)
	{
		fan_signal_line[fan_id]=gpiod_chip_get_line(g_gpio_chip, fan_rpm_gpio_line[fan_id]);
		if(!fan_signal_line[fan_id])
		{
			applog(LOG_ERR, "gpiod_chip_get_line(%i) error", fan_rpm_gpio_line[fan_id]);
			return(NULL);
		}
		if(gpiod_line_request_falling_edge_events(fan_signal_line[fan_id], "Fan_RPM_monitor")<0)
		{
			applog(LOG_ERR, "gpiod_line_request_falling_edge_events() error");
			return(NULL);
		}
	}
	fan_id=0;
	while(42)
	{
		if(gpiod_line_event_wait(fan_signal_line[fan_id], &tacho_signal_timeout)!=1)
		{
			g_device->fan_rpm[fan_id]=0;
			if(++fan_id>=S21_FAN_NUM)
			{
				fan_id=0;
			}
			applog(LOG_ERR, "Fan %u error: cannot read the RPM", fan_id);
			continue;
		}
		if(gpiod_line_event_read(fan_signal_line[fan_id], &tacho_event)!=0)
		{
			g_device->fan_rpm[fan_id]=0;
			if(++fan_id>=S21_FAN_NUM)
			{
				fan_id=0;
			}
			applog(LOG_ERR, "Fan %u error: cannot read the RPM", fan_id);
			continue;
		}
		if(tacho_event.event_type==GPIOD_LINE_EVENT_FALLING_EDGE)
		{
			if(++tacho_event_counter==0)
			{
				gettimeofday(&now, NULL);
				timersub(&now, &last_tacho_event_time, &elapsed);
				gettimeofday(&last_tacho_event_time, NULL);
				interval_sec=elapsed.tv_usec;
				interval_sec/=1000000.0F;
				interval_sec+=elapsed.tv_sec;
				if(interval_sec>0.0F)
				{
					g_device->fan_rpm[fan_id]=7680;
					g_device->fan_rpm[fan_id]/=interval_sec;
				}
				if(++fan_id>=S21_FAN_NUM)
				{
					fan_id=0;
				}
			}
		}
	}
	for(fan_id=0; fan_id<S21_FAN_NUM; fan_id++)
	{
		gpiod_line_release(fan_signal_line[fan_id]);
	}
	return(NULL);
}

void *autotuner_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	autotuner_t *autotuner=(autotuner_t *)arg;
	uint8_t board_id, chip_id, sensor_id, measurement;
	float most_hot_board_temperature, most_hot_asic_temperature;
//	struct timeval ti_start={0, 0}, ti_now={0, 0}, ti_elapsed;

	if(NULL==autotuner)
	{
		fprintf(stderr, "%s: Error. 'autotuner' is nullptr.\n", __FUNCTION__);
		return(NULL);
	}

	while(42)
	{
		usleep(500000);
		switch(autotuner->state)
		{
			case AUTOTUNER_PREPARE:
			{
//				gettimeofday(&ti_now, NULL);
//				timersub(&ti_now, &ti_start, &ti_elapsed);
				process_hash_rate_summary();

				switch(autotuner->mode)
				{
					case AUTOTUNER_DISABLED:
					case AUTOTUNER_ONLY_FANS:
					{
						autotuner->target_pll_setting_index=pll_setting_index_from_frequency(opt_frequency);
						break;
					}
					case AUTOTUNER_TARGET_HASHRATE:
					{
						autotuner->target_pll_setting_index=pll_setting_index_from_hashrate(opt_target_hashrate);
						break;
					}
					case AUTOTUNER_TARGET_CONSUMPTION:
					{
						break;
					}
					case AUTOTUNER_MAXIMUM_PERFORMANCE:
					{
						break;
					}
				}

				if(autotuner->target_pll_setting_index>autotuner->pll_setting_index)
				{
					autotuner->pll_setting_index++;
				}
				else if(autotuner->target_pll_setting_index<autotuner->pll_setting_index)
				{
					autotuner->pll_setting_index--;
				}

				autotuner->target_voltage_setting=opt_psu_voltage*VOLTAGE_STEP_DIV;
				autotuner->target_voltage_setting=(int32_t)autotuner->target_voltage_setting;
				autotuner->target_voltage_setting/=VOLTAGE_STEP_DIV;

				autotuner->voltage_setting=apw17_get_voltage_setting();
				if(autotuner->target_voltage_setting>autotuner->voltage_setting)
				{
					autotuner->voltage_setting+=1.0F/VOLTAGE_STEP_DIV;
				}
				else if(autotuner->target_voltage_setting<autotuner->voltage_setting)
				{
					autotuner->voltage_setting-=1.0F/VOLTAGE_STEP_DIV;
				}

				most_hot_board_temperature=0.0F;

				for(board_id=0; board_id<CHAINS_MAX; board_id++)
				{
					if(g_boards[board_id]->exist && g_boards[board_id]->active)
					{
						for(sensor_id=0; sensor_id<TEMPERATURE_SENSORS_PER_BOARD; sensor_id++)
						{
							if(most_hot_board_temperature<g_boards[board_id]->temperature[sensor_id])
							{
								most_hot_board_temperature=g_boards[board_id]->temperature[sensor_id];
							}
						}
						most_hot_asic_temperature=0.0F;
						for(chip_id=1; chip_id<ASICS_PER_BOARD+1; chip_id++)
						{
							if(most_hot_asic_temperature<g_boards[board_id]->asic_temperature[chip_id])
							{
								most_hot_asic_temperature=g_boards[board_id]->asic_temperature[chip_id];
							}
						}
//						update_temperature_log(board_id, 2, most_hot_asic_temperature);
					}
				}

				autotuner->state=AUTOTUNER_WORKING;
				break;
			}
			case AUTOTUNER_WORKING:
			{
				for(board_id=0; board_id<CHAINS_MAX; board_id++)
				{
					if(g_boards[board_id]->exist && g_boards[board_id]->active)
					{
						set_frequency_by_index_on_chain(board_id, autotuner->pll_setting_index);
					}
				}

				if(autotuner->mode>=AUTOTUNER_ONLY_FANS)
				{
					adjust_fan_pwm_according_to_temperature(most_hot_board_temperature);
				}
				else
				{
					apply_fan_pwm_setting(FAN_PWM_MAX_VALUE*opt_fan_speed_percentage/100);
				}

				// a crutch there:
				// it looks like our PSU can hang up if measurements
				// are taken too frequently
				measurement++;
				if(0==measurement%SKIP_MEASUREMENT)
				{
//					g_device->last_known_psu_voltage=apw17_measure_voltage();
					g_device->last_known_psu_power=apw17_measure_power();
				}
				else
				{
					apw17_set_voltage(autotuner->voltage_setting);
				}

				autotuner->state=AUTOTUNER_PREPARE;
				break;
			}
			case AUTOTUNER_DONE:
			{
				return(NULL);
			}
		}
	}

	return(NULL);
}

static int64_t s21_scanwork(struct thr_info *mining_thr)
{
	int64_t hashes=0;
	uint32_t nonce, version;
	uint8_t board_id, asic_id, submit_nonce_ok;
//	uint8_t unknown_field1, unknown_field2;
//	uint8_t byte;

	pthread_mutex_lock(&nonce_fifo_mutex);
	pthread_mutex_lock(&works_mutex);
	while(g_nonce_fifo.nonce_num)
	{
		nonce=bswap_32(g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].nonce);
//		unknown_field1=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].unknown_field1;
//		unknown_field2=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].unknown_field2;
		board_id=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].board_id;
		version=bswap_16(g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].version);
		version=version<<13;

		if(nonce)
		{
			asic_id=extract_asic_id_from_nonce(nonce);
			g_boards[board_id]->asic_nonce_counter[asic_id]++;
//				applog(LOG_INFO, "Nonce %u asic_id %u (total %u)", new_nonce_val, asic_id, g_boards[board_id]->asic_nonce_counter[asic_id]);
		}

		if(g_boards[board_id]->work_in_progress)
		{
			g_boards[board_id]->work_in_progress->version=version;
			g_boards[board_id]->work_in_progress->nonce=nonce;
			g_boards[board_id]->asic_diff_a[asic_id]+=g_boards[board_id]->work_in_progress->work_difficulty;
			hashes+=g_boards[board_id]->work_in_progress->work_difficulty;
			submit_nonce_ok=submit_nonce(mining_thr, g_boards[board_id]->work_in_progress);
			if(submit_nonce_ok)
			{
//				fprintf(stdout, "work->hash ");
//				for(byte=0; byte<32; byte++)
//				{
//					fprintf(stdout, "%02X ", g_boards[board_id]->work_in_progress->hash[byte]);
//				}
//				fprintf(stdout, "\n");

//				fprintf(stdout, "work->target ");
//				for(byte=0; byte<32; byte++)
//				{
//					fprintf(stdout, "%02X ", g_boards[board_id]->work_in_progress->target[byte]);
//				}
//				fprintf(stdout, "\n");

				g_device->valid_nonce_count++;
				free_work(&g_boards[board_id]->work_in_progress);
			}
//			free_work(&g_boards[board_id]->work_in_progress);
		}
		if(++g_nonce_fifo.p_rd>=MAX_NONCE_NUMBER_IN_FIFO)
		{
			g_nonce_fifo.p_rd=0;
		}
		g_nonce_fifo.nonce_num--;
	}
	pthread_mutex_unlock(&works_mutex);
	pthread_mutex_unlock(&nonce_fifo_mutex);
	hashes*=0xFFFFFFFFULL;
	hashes/=2000ULL;
	return hashes;
}

void *fill_work_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	board_parameters_t *board=(board_parameters_t *)arg;
	struct timeval now, last_send, send_elapsed;
	uint8_t merkle_root_le[32];
	uint8_t prev_block_hash_le[32];
	struct work *new_work=NULL;
	work_1368_t workdata;

	if(board==NULL)
	{
		return(NULL);
	}

	gettimeofday(&last_send, NULL);
	gettimeofday(&now, NULL);

	workdata.preamble[0]=PACKET_PREAMBLE_1;
	workdata.preamble[1]=PACKET_PREAMBLE_2;
	workdata.command=0x21;
	workdata.not_a_length=0x36;
	workdata.not_a_work_id=0;
	workdata.num_midstates=1;
	workdata.starting_nonce=0;

	while(board->active)
	{
		gettimeofday(&now, NULL);
		timersub(&now, &last_send, &send_elapsed);
		if(send_elapsed.tv_sec>=board->work_update_interval)
		{
			new_work=get_work(g_device->mining_control_thr);
			pthread_mutex_lock(&works_mutex);
			if(board->work_in_progress)
			{
				free_work(&board->work_in_progress);
			}
			board->work_in_progress=new_work;
			hex2bin((uint8_t *)&workdata.nbits, new_work->pool->nbit, 4);
			workdata.nbits=bswap_32(workdata.nbits);
			hex2bin((uint8_t *)&workdata.ntime, new_work->ntime, 4);
			workdata.ntime=bswap_32(workdata.ntime);
			memcpy(merkle_root_le, new_work->data+36, 32);
			swab256(workdata.merkle_root, merkle_root_le);
			hex2bin(prev_block_hash_le, new_work->pool->prev_hash, 32);
			swab256(workdata.prev_block_hash, prev_block_hash_le);
			workdata.version=*(uint32_t *)new_work->pool->header_bin;
			workdata.version=bswap_32(workdata.version);
			workdata.crc16=crc16_itu(0xFFFF, PACKET_PREAMBLE_LENGTH+(uint8_t *)&workdata, 84);
			workdata.crc16=bswap_16(workdata.crc16);
			pthread_mutex_unlock(&works_mutex);
			pthread_mutex_lock(&board->tty_rw_mutex);
			write(board->tty_fd, &workdata, PACKET_PREAMBLE_LENGTH+ASIC_WORK_PACKET_LENGTH);
			pthread_mutex_unlock(&board->tty_rw_mutex);
			gettimeofday(&last_send, NULL);
		}
	}
	return NULL;
}

void *get_asic_response_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	board_parameters_t *board=(board_parameters_t *)arg;
	uint8_t in_data_buf[ASIC_RESPONSE_BUF_SIZE];
	asic_nonce_result_t *nonce_response;
	asic_reg_value_result_t *reg_value_response;
	uint8_t *packet_ptr;
	uint32_t bytes_available, bytes_stored=0;
//	uint32_t i;

	if(board==NULL)
	{
		return(NULL);
	}

	while(board->active)
	{
		if(bytes_stored>=ASIC_RESPONSE_BUF_SIZE)
		{
			bytes_stored=0;
		}
		pthread_mutex_lock(&board->tty_rw_mutex);
		bytes_available=get_bytes_num_in_fd(board->tty_fd);
		if(bytes_available+bytes_stored>ASIC_RESPONSE_BUF_SIZE)
		{
			bytes_available=ASIC_RESPONSE_BUF_SIZE-bytes_stored;
		}
		if(bytes_available)
		{
			if(read(board->tty_fd, in_data_buf+bytes_stored, bytes_available)!=bytes_available)
			{
				applog(LOG_ERR, "brd[%u]->tty_fd read error", board->id);
				pthread_mutex_unlock(&board->tty_rw_mutex);
				continue;
			}
			bytes_stored+=bytes_available;

		}
		pthread_mutex_unlock(&board->tty_rw_mutex);

		packet_ptr=detect_packet(in_data_buf, bytes_stored, ASIC_RESPONSE_PACKET_LENGTH);

		if(packet_ptr)
		{
			nonce_response=
			reg_value_response=(asic_reg_value_result_t *)packet_ptr;
			// WIP
			// check the CRC here
		}
		else if(update_asic_num)
		{
			packet_ptr=detect_packet(in_data_buf, bytes_stored, ASIC_RESPONSE_PACKET_LENGTH-2);
			if(packet_ptr)
			{
				reg_value_response=(asic_reg_value_result_t *)packet_ptr;
				if(reg_value_response->reg_value==0x6813)
				{
					if(++board->number_of_asics>ASICS_PER_BOARD)
					{
						board->number_of_asics=ASICS_PER_BOARD;
					}
				}

//				for(i=0; i<ASIC_RESPONSE_PACKET_LENGTH-3; i++)
//				{
//					fprintf(stdout, "%02X ", packet_ptr[i]);
//				}
//				fprintf(stdout, "%02X\n", packet_ptr[i]);

				packet_ptr+=ASIC_RESPONSE_PACKET_LENGTH-2;
				bytes_stored-=(packet_ptr-in_data_buf);
				goto skip_work;
			}
			else
			{
				continue;
			}
		}
		else
		{
			continue;
		}

//		if(nonce_response->version)
		if((nonce_response->crc >> 7) == 1)
		{
			// === debug
//			fprintf(stdout, "nonce packet: ");
//			for(i=0; i<ASIC_RESPONSE_PACKET_LENGTH-1; i++)
//			{
//				fprintf(stdout, "%02X ", packet_ptr[i]);
//			}
//			fprintf(stdout, "%02X\n", packet_ptr[i]);
			// ===
			pthread_mutex_lock(&nonce_fifo_mutex);
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].nonce			=nonce_response->nonce;
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].unknown_field1	=nonce_response->unknown_field1;
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].unknown_field2	=nonce_response->unknown_field2;
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].version			=nonce_response->version;
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].crc				=nonce_response->crc;
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].board_id			=board->id;
			if(++g_nonce_fifo.p_wr>=MAX_NONCE_NUMBER_IN_FIFO)
			{
				g_nonce_fifo.p_wr=0;
			}
			if(g_nonce_fifo.nonce_num<MAX_NONCE_NUMBER_IN_FIFO)
			{
				g_nonce_fifo.nonce_num++;
			}
			pthread_mutex_unlock(&nonce_fifo_mutex);
		}
		else
		{
			// === debug
//			fprintf(stdout, "reg_value packet: ");
//			for(i=0; i<ASIC_RESPONSE_PACKET_LENGTH-1; i++)
//			{
//				fprintf(stdout, "%02X ", packet_ptr[i]);
//			}
//			fprintf(stdout, "%02X\n", packet_ptr[i]);
			// ===
			reg_value_response->reg_value=bswap_32(reg_value_response->reg_value);
			switch(reg_value_response->reg_addr)
			{
				case ASIC_MODEL:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X ASIC_MODEL: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case UNKNOWN_COUNTER:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X UNKNOWN_COUNTER: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case PLL_PARAMETER:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X PLL_PARAMETER: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case NONCE_OFFSET:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X NONCE_OFFSET: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case HASH_COUNTER:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X HASH_COUNTER: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case TICKET_MASK:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X TICKET_MASK: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case MISC_CONTROL:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X MISC_CONTROL: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case I2C_CONTROL:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X I2C_CONTROL: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case VERSION_MASK:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X VERSION_MASK: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case UNKNOWN_REG_8A:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X UNKNOWN_REG_8A: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case UNKNOWN_REG_A8:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%02X UNKNOWN_REG_A8: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case CHIP_SENSOR_MEASUREMENT_DATA:
				{
//					fprintf(stdout, "CHIP_ADDR: 0x%02X CHIP_SENSOR_MEASUREMENT_DATA: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					uint32_t chip_id=1+reg_value_response->chip_addr/g_device->asic_addr_interval;
					board->asic_temperature[chip_id]=(reg_value_response->reg_value&0xFFFF)/32.0F;
//					fprintf(stdout, "board[%u] chip[%u] temperature: %0.2f\n", board->id, chip_id, board->asic_temperature[chip_id]);
					break;
				}
				default:
				{
					break;
				}
			}
		}
		packet_ptr+=ASIC_RESPONSE_PACKET_LENGTH;
		bytes_stored-=(packet_ptr-in_data_buf);

		skip_work:

		memmove(in_data_buf, packet_ptr, bytes_stored);
	}
	return NULL;
}

void enable_board(uint8_t board_id)
{
	int ptcret;
	if(!g_boards[board_id]->exist)
	{
		return;
	}
	if(g_boards[board_id]->active)
	{
		return;
	}
	g_boards[board_id]->active=1;
	g_device->hash_boards_active++;
	sleep(1);
	reset_board(board_id);
	usleep(500000);
	tty_init_one_chain(board_id, B115200);
	ptcret=pthread_create(&g_boards[board_id]->uart_rx_thr, NULL, get_asic_response_thr_func, &g_boards[board_id]);
	if(ptcret)
	{
		applog(LOG_ERR, "Create RX thread for Chain[%u]:failed", board_id);
	}
	else
	{
		fprintf(stdout, "Create RX thread for Chain[%u]:ok\n", board_id);
	}
	ptcret=pthread_create(&g_boards[board_id]->uart_tx_thr, NULL, fill_work_thr_func, &g_boards[board_id]);
	if(ptcret)
	{
		applog(LOG_ERR, "Create TX thread for Chain[%u]:failed", board_id);
	}
	else
	{
		fprintf(stdout, "Create TX thread for Chain[%u]:ok\n", board_id);
	}
	tty_init_one_chain(board_id, B1000000);
}

void disable_board(uint8_t board_id)
{
	if(!g_boards[board_id]->exist)
	{
		return;
	}
	if(!g_boards[board_id]->active)
	{
		return;
	}
	g_boards[board_id]->active=0;
	g_device->hash_boards_active--;
	g_boards[board_id]->hw_errors=0;
	tcflush(g_boards[board_id]->tty_fd, TCIOFLUSH);
	close(g_boards[board_id]->tty_fd);
}

static void s21_detect(bool hotplug)
{
	struct cgpu_info *cgpu=calloc(1, sizeof(struct cgpu_info));
	if(!cgpu)
	{
		quit(1, "Failed to calloc 'cgpu'");
	}
	cgpu->drv=&antminer_s21_drv;
	cgpu->name=antminer_s21_drv.name;
	cgpu->deven=DEV_ENABLED;
	cgpu->threads=1;
	add_cgpu(cgpu);
}

static bool s21_prepare(struct thr_info *mining_thr)
{
	int check_asic_times=0;
	uint8_t board_id, chip_id;
	bool check_asic_fail=false;

	g_device=calloc(1, sizeof(device_t));
	if(!g_device)
	{
		return false;
	}

	g_device->mining_control_thr=mining_thr;
	g_device->asic_addr_interval=256/ASICS_PER_BOARD;

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		g_boards[board_id]=calloc(1, sizeof(board_parameters_t));
		if(!g_boards[board_id])
		{
			return false;
		}
		g_boards[board_id]->id=board_id;
		g_boards[board_id]->asic_temperature[0]=ASICS_PER_BOARD;
		g_boards[board_id]->asic_hash_rate[0]=ASICS_PER_BOARD;
		g_boards[board_id]->work_update_interval=__UINT32_MAX__;
		pthread_mutex_init(&g_boards[board_id]->tty_rw_mutex, NULL);
	}

	check_chains();
	usleep(10000);

	i2c_init();
	usleep(10000);

	if(apw17_init(&i2c_mutex)<0)
	{
		exit(555);
	}

	autotuner_init();

	clear_nonce_buf();

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		int ptcret;
		if(g_boards[board_id]->exist)
		{
			tty_init_one_chain(board_id, B115200);
			ptcret=pthread_create(&g_boards[board_id]->uart_rx_thr, NULL, get_asic_response_thr_func, g_boards[board_id]);
			if(ptcret)
			{
				applog(LOG_ERR, "Create RX thread for Chain[%u]:fail", board_id);
			}
			else
			{
				fprintf(stdout, "Create RX thread for Chain[%u]:ok\n", board_id);
			}
			ptcret=pthread_create(&g_boards[board_id]->uart_tx_thr, NULL, fill_work_thr_func, g_boards[board_id]);
			if(ptcret)
			{
				applog(LOG_ERR, "Create TX thread for Chain[%u]:fail", board_id);
			}
			else
			{
				fprintf(stdout, "Create TX thread for Chain[%u]:ok\n", board_id);
			}
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			fprintf(stdout, "Board[%u] reset\n", board_id);
			reset_board(board_id);
		}
	}
	usleep(250000);

	//check ASIC number for every chain
	fprintf(stdout, "send cmd to get chip address\n");

check_asic_num:
	update_asic_num=true;
	check_asic_reg_all_chains(0, ASIC_MODEL, 1);
	usleep(500000);
	update_asic_num=false;

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			if(g_boards[board_id]->number_of_asics!=ASICS_PER_BOARD)
			{
				check_asic_fail=true;
			}
			else
			{
//
			}
			fprintf(stdout, "Board[%u].number_of_asics=%u\n", board_id, g_boards[board_id]->number_of_asics);
		}
	}

	if(check_asic_fail && check_asic_times<3)
	{
		applog(LOG_ERR, "Need to recheck asic num...");
		for(board_id=0; board_id<CHAINS_MAX; board_id++)
		{
			g_boards[board_id]->number_of_asics=0;
		}
		check_asic_fail=false;
		check_asic_times++;
		goto check_asic_num;
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			if(g_boards[board_id]->number_of_asics!=ASICS_PER_BOARD)
			{
				fprintf(stdout, "Board[%u] looks bad.\n", board_id);
				disable_board(board_id);
			}
			else
			{
				fprintf(stdout, "Board[%u] looks ok.\n", board_id);
			}
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			set_unknown_reg_a8_one_chain(board_id, 0x00, 0x07, 0x00, 0x00);
			set_misc_ctrl_one_chain(board_id, 0xFF, 0x0F, 0xC1, 0x00);

			chain_inactive(board_id);
			usleep(250000);
			set_asic_address_one_chain(board_id);
			set_core_ctrl_one_chain(board_id, 0x80, 0x00, 0x8B, 0x00);
			set_core_ctrl_one_chain(board_id, 0x80, 0x00, 0x80, 0x18);

			set_ticket_mask_one_chain(board_id, 0x000000FF);
			set_version_mask_one_chain(board_id, 0x9000FFFF);
			set_analog_mux_control_one_chain(board_id, 0x00000003);

			set_io_driver_strength_one_chain(board_id);

			set_pll3_parameter_one_chain(board_id, 0x5AA55AA5);

			set_uart_relay_one_chain(board_id, 23);

//			set_nonce_offset_one_chain(board_id, 0x0142); // 0x0142 is default step for s19xp hydro
//			set_nonce_offset_one_chain(board_id, 0x0148);
			set_nonce_offset_one_chain(board_id, 0x025F);

//			set_hash_counter_one_chain(board_id,  0x00001618);
			set_hash_counter_one_chain(board_id,  0x000015E5);
//			set_hash_counter_one_chain(board_id,  0x0000147A);

			set_fast_uart_configuration_one_chain(board_id, 0x01300000); // 3M
//			set_fast_uart_configuration_one_chain(board_id, 0x01300200); // 1M
			sleep(1);
			tty_init_one_chain(board_id, B3000000);
//			tty_init_one_chain(board_id, B1000000);
		}
	}

	sleep(1);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			for(chip_id=0; chip_id<ASICS_PER_BOARD; chip_id++)
			{
				set_misc_ctrl_one_chip(board_id, chip_id, 0xF0, 0x00, 0xC1, 0x00);
				set_unknown_reg_a8_one_chip(board_id, chip_id, 0x00, 0x07, 0x01, 0xF0);
				set_core_ctrl_one_chip(board_id, chip_id, 0x80, 0x00, 0x8B, 0x00);
				set_core_ctrl_one_chip(board_id, chip_id, 0x80, 0x00, 0x80, 0x18);
				set_core_ctrl_one_chip(board_id, chip_id, 0x80, 0x00, 0x82, 0xAA);
			}
		}
	}

	if(pthread_create(&read_temperature_thread, NULL, read_temperature_thr_func, NULL))
	{
		applog(LOG_ERR, "Create thread for read_temperature_thr_func() failed");
		return false;
	}

	if(pthread_create(&read_fan_rpm_thread, NULL, read_fan_rpm_thr_func, NULL))
	{
		applog(LOG_ERR, "Create thread for read_fan_rpm_thr_func() failed");
		return false;
	}

	if(pthread_create(&autotuner_thread, NULL, autotuner_thr_func, g_autotuner))
	{
		applog(LOG_ERR, "Create thread for autotuner_thr_func() failed");
		return false;
	}

	return true;
}

static struct api_data *s21_get_api_stats(struct cgpu_info *cgpu)
{
	unsigned int board_id, sensor_id;
	char field_name_str[32];
	float psu_voltage_setting;
	struct api_data *root=NULL;

	psu_voltage_setting=apw17_get_voltage_setting();

	root=api_add_volts(root, "psu_voltage_setting", &psu_voltage_setting, true);
	root=api_add_volts(root, "psu_voltage", &g_device->last_known_psu_voltage, false);
	root=api_add_uint16(root, "psu_power", &g_device->last_known_psu_power, false);

	root=api_add_uint8(root, "board_num", &g_device->hash_boards_active, false);

	uint32_t n_summ=g_device->valid_nonce_count+cgpu->hw_errors;

	double hwp=n_summ ? (double)(cgpu->hw_errors) / (double)(n_summ):0;
	root=api_add_percent(root, "hwe_percentage", &hwp, true);
	root=api_add_int(root, "hwe_total", &cgpu->hw_errors, false);

	for(sensor_id=0; sensor_id<S21_FAN_NUM; sensor_id++)
	{
		sprintf(field_name_str, "fan%u_rpm", sensor_id+1);
		root=api_add_int32(root, field_name_str, &g_device->fan_rpm[sensor_id], false);
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			sprintf(field_name_str, "board%u_hash_rate", board_id+1);
			root=api_add_uint32(root, field_name_str, &g_boards[board_id]->hash_rate, false);
			sprintf(field_name_str, "board%u_frequency", board_id+1);
			root=api_add_int32(root, field_name_str, &g_boards[board_id]->frequency, false);
			sprintf(field_name_str, "board%u_hw_errors", board_id+1);
			root=api_add_uint32(root, field_name_str, &(g_boards[board_id]->hw_errors), false);
			sprintf(field_name_str, "board%u_number_of_asics", board_id+1);
			root=api_add_uint8(root, field_name_str, &g_boards[board_id]->number_of_asics, false);

			sprintf(field_name_str, "board%u_asic_temperature", board_id+1);
			root=api_add_float_array(root, field_name_str, g_boards[board_id]->asic_temperature, false);

			sprintf(field_name_str, "board%u_asic_hash_rate", board_id+1);
			root=api_add_uint_array(root, field_name_str, g_boards[board_id]->asic_hash_rate, false);
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			// WIP
			float fake_power=0;
			sprintf(field_name_str, "board%u_power", board_id+1);
			root=api_add_volts(root, field_name_str, &fake_power, true);
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			for(sensor_id=0; sensor_id<TEMPERATURE_SENSORS_PER_BOARD; sensor_id++)
			{
				sprintf(field_name_str, "board%u_temperature%u", board_id+1, sensor_id+1);
				root=api_add_temp(root, field_name_str, &g_boards[board_id]->temperature[sensor_id], false);
			}
		}
	}

	return root;
}

static void s21_update_work_stub(struct cgpu_info *cgpu)
{
//	fprintf(stdout, "%s | s21_update_work_stub()\n", cgpu->name);
}

static void s21_update_work(struct cgpu_info *cgpu)
{
//	fprintf(stdout, "%s | s21_update_work()\n", cgpu->name);
	uint8_t board_id;
	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			g_boards[board_id]->work_update_interval=WORK_UPDATE_INTERVAL;
		}
	}
	cgpu->drv->update_work=&s21_update_work_stub;
}

void s19xph_shutdown()
{
	uint8_t board_id;
	pthread_cancel(read_fan_rpm_thread);
	pthread_cancel(read_temperature_thread);
	pthread_cancel(autotuner_thread);
	for(board_id=0; board_id<CHAINS_MAX; ++board_id)
	{
		disable_board(board_id);
	}
	gpiod_chip_close(g_gpio_chip);
	apw17_power_off();
}

struct device_drv antminer_s21_drv=
{
	.drv_id=DRIVER_antminer_s21,
	.dname="Antminer_S21",
	.name="S21",
	.drv_detect=s21_detect,
	.hash_work=hash_driver_work,
	.scanwork=s21_scanwork,
//	.flush_work=s21_update_work,
	.update_work=s21_update_work,
// WIP
//	.hw_error=s19xph_hw_error,
// WIP
//	.reinit_device=s19xph_reinit_device,
	.get_api_stats=s21_get_api_stats,
	.thread_prepare=s21_prepare,
	.thread_shutdown=s19xph_shutdown
};
