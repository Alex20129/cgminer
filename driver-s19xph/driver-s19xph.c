#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/select.h>
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

#include "driver-bm1366.h"
#include "apw11.h"
#include "pic.h"

pthread_t pic_heart_beat_thr=0;

const int tty[CHAINS_MAX]={0, 0, 4, 5};
const int board_enable_gpio_pin[CHAINS_MAX]={17, 20, 23, 26};

int opt_frequency;
int opt_target_temperature=DEFAULT_TARGET_TEMPERATURE;
float opt_psu_voltage=DEFAULT_VOLTAGE;

pthread_mutex_t i2c_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t works_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t nonce_fifo_mutex=PTHREAD_MUTEX_INITIALIZER;

bool update_asic_num=false;

device_parameters_t	*g_device;
board_parameters_t	*g_boards[CHAINS_MAX];

nonce_buf_t		g_nonce_fifo;

// fake pll table
static const pll_freq_t freq_pll_1366[] =
{
	{56.25,	0x40A20255},
	{62.50,	0x40AF0264},
	{68.75,	0x40A50254},
	{75.00,	0x40A80263},
	{81.25,	0x40B60263},
	{87.50,	0x40A80253},
	{93.75,	0x40B40253},
	{100.00,	0x40A80262},
	{106.25,	0x40AA0243},
	{112.50,	0x40A20252},
	{118.75,	0x40AB0252},
	{125.00,	0x40B40252},
	{131.25,	0x40BD0252},
	{137.50,	0x40A50242},
	{143.75,	0x40A10261},
	{150.00,	0x40A80261},
	{156.25,	0x40AF0261},
	{162.50,	0x40B60261},
	{168.75,	0x40A20251},
	{175.00,	0x40A80251},
	{181.25,	0x40AE0251},
	{187.50,	0x40B40251},
	{193.75,	0x40BA0251},
	{200.00,	0x40A00241},
	{206.25,	0x40A50241},
	{212.50,	0x40AA0241},
	{218.75,	0x40AF0241},
	{225.00,	0x40B40241},
	{231.25,	0x40B90241},
	{237.50,	0x40BE0241},
	{243.75,	0x50C30241},
	{250.00,	0x40A00231},
	{256.25,	0x40A40231},
	{262.50,	0x40A80231},
	{268.75,	0x40AC0231},
	{275.00,	0x40B00231},
	{281.25,	0x40B40231},
	{287.50,	0x40A10260},
	{293.75,	0x40BC0231},
	{300.00,	0x40A80260},
	{306.25,	0x50C40231},
	{312.50,	0x40AF0260},
	{318.75,	0x50CC0231},
	{325.00,	0x40B60260},
	{331.25,	0x50D40231},
	{337.50,	0x40A20250},
	{343.75,	0x40A50250},
	{350.00,	0x40A80250},
	{356.25,	0x40AB0250},
	{362.50,	0x40AE0250},
	{368.75,	0x40B10250},
	{375.00,	0x40B40250},
	{381.25,	0x40B70250},
	{387.50,	0x40BA0250},
	{393.75,	0x40BD0250},
	{400.00,	0x40A00240},
	{406.25,	0x50C30250},
	{412.50,	0x40A50240},
	{418.75,	0x50C90250},
	{425.00,	0x40AA0240},
	{431.25,	0x50CF0250},
	{437.50,	0x40AF0240},
	{443.75,	0x50D50250},
	{450.00,	0x40B40240},
//	{456.25,	0x50DB0250},
//	{462.50,	0x40B90240},
//	{468.75,	0x50E10250},
//	{475.00,	0x40BE0240},
//	{481.25,	0x50E70250},
//	{487.50,	0x50C20240}
};

const char chipname[]="gpiochip0";

static void reset_board(uint8_t board_id)
{
	struct gpiod_chip *chip;
	struct gpiod_line *board_reset_line;

	chip=gpiod_chip_open_by_name(chipname);

	board_reset_line=gpiod_chip_get_line(chip, board_enable_gpio_pin[board_id]);

	gpiod_line_request_output(board_reset_line, "reset_board", 1);

	gpiod_line_set_value(board_reset_line, 0);
	usleep(50000);
	gpiod_line_set_value(board_reset_line, 1);

	gpiod_line_release(board_reset_line);
	gpiod_chip_close(chip);
}

static inline void psu_power_on()
{
	struct gpiod_chip *chip;
	struct gpiod_line *lineRed;

	// Open GPIO chip
	chip=gpiod_chip_open_by_name(chipname);

	// Open GPIO lines
	lineRed=gpiod_chip_get_line(chip, 4);

	// Open LED lines for output
	gpiod_line_request_output(lineRed, "PSU", 0);

	gpiod_line_set_value(lineRed, 0);

	// Release lines and chip
	gpiod_line_release(lineRed);
	gpiod_chip_close(chip);
}

static inline void psu_power_off()
{
	struct gpiod_chip *chip;
	struct gpiod_line *lineRed;

	// Open GPIO chip
	chip=gpiod_chip_open_by_name(chipname);

	// Open GPIO lines
	lineRed=gpiod_chip_get_line(chip, 4);

	// Open LED lines for output
	gpiod_line_request_output(lineRed, "PSU", 1);

	gpiod_line_set_value(lineRed, 1);

	// Release lines and chip
	gpiod_line_release(lineRed);
	gpiod_chip_close(chip);
}

void check_asic_reg_all_chains(uint8_t chip_addr, uint8_t reg_addr, uint8_t all_chips)
{
	uint8_t rdreg_buf[7], board_id, byte;
	rdreg_buf[0]=PACKET_PREAMBLE_1;
	rdreg_buf[1]=PACKET_PREAMBLE_2;

	rdreg_buf[2]=CMD_TYPE | GET_STATUS; // 0x42
	if(all_chips)
	{
		rdreg_buf[2] |= CMD_ALL;		// 0x52
	}
	rdreg_buf[3]=CMD_LENTH;
	rdreg_buf[4]=chip_addr;
	rdreg_buf[5]=reg_addr;
	rdreg_buf[6]=crc5(rdreg_buf+PACKET_PREAMBLE_LENGTH, 4*8);

	fprintf(stdout, "rdreg_buf: ");
	for(byte=0; byte<7; byte++)
	{
		fprintf(stdout, "%02X ", rdreg_buf[byte]);
	}
	fprintf(stdout, "\n");

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
    {
		if(g_boards[board_id]->exist)
        {
			pthread_mutex_lock(&g_boards[board_id]->tty_rw_mutex);
			write(g_boards[board_id]->tty_fd, rdreg_buf, CMD_LENTH+PACKET_PREAMBLE_LENGTH);
			pthread_mutex_unlock(&g_boards[board_id]->tty_rw_mutex);
        }
    }
}

uint32_t nearest_pll_setting_index(uint32_t frequency)
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

void check_chains()
{
//	uint8_t chain_id;
//	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
//    {
		//WIP
		//check chains by GPIO
//    }
	g_boards[0]->exist=1;
	g_boards[0]->active=1;
	g_device->hash_boards_active=1;
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
		fprintf(stderr, "Chain[%u] open %s failed\n", chain_id, dev_fname);
		g_boards[chain_id]->tty_fd=0;
		exit(EXIT_FAILURE);
	}

	if(tcgetattr(g_boards[chain_id]->tty_fd, &options)<0)
	{
		fprintf(stderr, "tcgetattr() returned -1\n");
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
		fprintf(stderr, "tcsetattr() returned -1\n");
		exit(EXIT_FAILURE);
	}

	if(tcflush(g_boards[chain_id]->tty_fd, TCIOFLUSH)<0)
	{
		fprintf(stderr, "tcflush() returned -1\n");
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
	cmd_buf[3]=CONFIG_LENTH;
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
//		fprintf(stdout, "%02x ", cmd_buf[byte]);
//	}
//	fprintf(stdout, "%02x\n", cmd_buf[byte]);

	pthread_mutex_lock(&g_boards[chain_id]->tty_rw_mutex);
	write(g_boards[chain_id]->tty_fd, cmd_buf, CONFIG_LENTH+PACKET_PREAMBLE_LENGTH);
	pthread_mutex_unlock(&g_boards[chain_id]->tty_rw_mutex);
}

void chain_inactive(uint8_t chain_id)
{
	uint8_t cmd_buf[7];
	cmd_buf[0]=PACKET_PREAMBLE_1;
	cmd_buf[1]=PACKET_PREAMBLE_2;
	cmd_buf[2]=CMD_ALL | CMD_TYPE | CHAIN_INACTIVE;
	cmd_buf[3]=CMD_LENTH;
	cmd_buf[4]=0;
	cmd_buf[5]=0;
	cmd_buf[6]=crc5(cmd_buf+PACKET_PREAMBLE_LENGTH, 4*8);
	pthread_mutex_lock(&g_boards[chain_id]->tty_rw_mutex);
	write(g_boards[chain_id]->tty_fd, cmd_buf, CMD_LENTH+PACKET_PREAMBLE_LENGTH);
	pthread_mutex_unlock(&g_boards[chain_id]->tty_rw_mutex);
}

void set_asic_address_one_chain(uint8_t chain_id)
{
	uint8_t chip_addr, cmd_buf[7];
	unsigned int j;

	cmd_buf[0]=PACKET_PREAMBLE_1;
	cmd_buf[1]=PACKET_PREAMBLE_2;
	cmd_buf[2]=CMD_TYPE | SET_ADDR;
	cmd_buf[3]=CMD_LENTH;

	pthread_mutex_lock(&g_boards[chain_id]->tty_rw_mutex);
	for(j=0, chip_addr=0; j<ASICS_ON_CHAIN_MAX; j++)
	{
		cmd_buf[4]=chip_addr;
		cmd_buf[5]=CHIP_ADDR;
		cmd_buf[6]=crc5(cmd_buf+PACKET_PREAMBLE_LENGTH, 4*8);
		write(g_boards[chain_id]->tty_fd, cmd_buf, CMD_LENTH+PACKET_PREAMBLE_LENGTH);
		chip_addr+=g_device->asic_addr_interval;
		usleep(25000);
	}
	pthread_mutex_unlock(&g_boards[chain_id]->tty_rw_mutex);
}

static uint32_t nonce_offset_reg_data[ASICS_ON_CHAIN_MAX]=
{
	0x80000000,
	0x80000142,
	0x80000283,
	0x800003C4,
	0x80000506,
	0x80000647,
	0x80000788,
	0x800008C9,
	0x80000A0B,
	0x80000B4C,
	0x80000C8D,
	0x80000DCE,
	0x80000F10,
	0x80001051,
	0x80001192,
	0x800012D3,
	0x80001415,
	0x80001556,
	0x80001697,
	0x800017D8,
	0x8000191A,
	0x80001A5B,
	0x80001B9C,
	0x80001CDD,
	0x80001E1F,
	0x80001F60,
	0x800020A1,
	0x800021E2,
	0x80002324,
	0x80002465,
	0x800025A6,
	0x800026E7,
	0x80002829,
	0x8000296A,
	0x80002AAB,
	0x80002BEC,
	0x80002D2E,
	0x80002E6F,
	0x80002FB0,
	0x800030F1,
	0x80003233,
	0x80003374,
	0x800034B5,
	0x800035F6,
	0x80003738,
	0x80003879,
	0x800039BA,
	0x80003AFB,
	0x80003C3D,
	0x80003D7E,
	0x80003EBF,
	0x80004000,
	0x80004142,
	0x80004283,
	0x800043C4,
	0x80004506,
	0x80004647,
	0x80004788,
	0x800048C9,
	0x80004A0B,
	0x80004B4C,
	0x80004C8D,
	0x80004DCE,
	0x80004F10,
	0x80005051,
	0x80005192,
	0x800052D3,
	0x80005415,
	0x80005556,
	0x80005697,
	0x800057D8,
	0x8000591A,
	0x80005A5B,
	0x80005B9C,
	0x80005CDD,
	0x80005E1F,
	0x80005F60,
	0x800060A1,
	0x800061E2,
	0x80006324,
	0x80006465,
	0x800065A6,
	0x800066E7,
	0x80006829,
	0x8000696A,
	0x80006AAB,
	0x80006BEC,
	0x80006D2E,
	0x80006E6F,
	0x80006FB0,
	0x800070F1,
	0x80007233,
	0x80007374,
	0x800074B5,
	0x800075F6,
	0x80007738,
	0x80007879,
	0x800079BA,
	0x80007AFB,
	0x80007C3D,
	0x80007D7E,
	0x80007EBF,
	0x80008000,
	0x80008142,
	0x80008283,
	0x800083C4,
	0x80008506,
	0x80008647,
	0x80008788,
	0x800088C9,
	0x80008A0B,
	0x80008B4C,
	0x80008C8D,
	0x80008DCE,
	0x80008F10,
	0x80009051,
	0x80009192,
	0x800092D3,
	0x80009415,
	0x80009556,
	0x80009697,
	0x800097D8,
	0x8000991A,
	0x80009A5B,
	0x80009B9C,
	0x80009CDD,
	0x80009E1F,
	0x80009F60,
	0x8000A0A1,
	0x8000A1E2,
	0x8000A324,
	0x8000A465,
	0x8000A5A6,
	0x8000A6E7,
	0x8000A829,
	0x8000A96A,
	0x8000AAAB,
	0x8000ABEC,
	0x8000AD2E,
	0x8000AE6F,
	0x8000AFB0,
	0x8000B0F1,
	0x8000B233,
	0x8000B374,
	0x8000B4B5,
	0x8000B5F6,
	0x8000B738,
	0x8000B879,
	0x8000B9BA,
	0x8000BAFB,
	0x8000BC3D,
	0x8000BD7E,
	0x8000BEBF,
	0x8000C000,
	0x8000C142,
	0x8000C283,
	0x8000C3C4,
	0x8000C506,
	0x8000C647,
	0x8000C788,
	0x8000C8C9,
	0x8000CA0B,
	0x8000CB4C,
	0x8000CC8D,
	0x8000CDCE,
	0x8000CF10,
	0x8000D051,
	0x8000D192,
	0x8000D2D3,
	0x8000D415,
	0x8000D556,
	0x8000D697,
	0x8000D7D8,
	0x8000D91A,
	0x8000DA5B,
	0x8000DB9C,
	0x8000DCDD,
	0x8000DE1F,
	0x8000DF60,
	0x8000E0A1,
	0x8000E1E2,
	0x8000E324,
	0x8000E465,
	0x8000E5A6,
	0x8000E6E7,
	0x8000E829,
	0x8000E96A,
	0x8000EAAB,
	0x8000EBEC,
	0x8000ED2E,
	0x8000EE6F,
	0x8000EFB0,
	0x8000F0F1,
	0x8000F233,
	0x8000F374,
	0x8000F4B5,
	0x8000F5F6,
	0x8000F738,
	0x8000F879,
	0x8000F9BA,
	0x8000FAFB,
	0x8000FC3D,
	0x8000FD7E,
	0x8000FEBF
};

void set_nonce_offset_one_chain(uint8_t chain_id, uint16_t step)
{
	struct NONCE_OFFSET_DATA nonce_offset_data;
	uint16_t chip_id;
	nonce_offset_data.reg_data1=0x0080;
	for(chip_id=0; chip_id<ASICS_ON_CHAIN_MAX; chip_id++)
	{
//		*(uint32_t *)nonce_offset_data.reg_data=bswap_32(nonce_offset_reg_data[chip_id]);
		nonce_offset_data.reg_data2=bswap_16(step*chip_id);
//		fprintf(stdout, "nonce_offset_data : 0x%08X\n", *(uint32_t *)&nonce_offset_data);
		set_config(chain_id, 0, chip_id, NONCE_OFFSET, &nonce_offset_data);
		usleep(10000);
	}
}

void set_hash_counter_one_chain(uint8_t chain_id, uint32_t hash_counter)
{
	struct HASH_COUNTER_DATA hash_counter_data;
	*(uint32_t *)hash_counter_data.reg_data=bswap_32(hash_counter);
	fprintf(stdout, "hash_counter_data : 0x%08X\n", *(uint32_t *)&hash_counter_data);
	set_config(chain_id, 1, 0, HASH_COUNTER, &hash_counter_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_ticket_mask_one_chain(uint8_t chain_id, uint32_t ticket_mask)
{
	struct TICKET_MASK_DATA ticket_mask_data;
	*(uint32_t *)ticket_mask_data.reg_data=bswap_32(ticket_mask);
	fprintf(stdout, "ticket_mask_data : 0x%08X\n", *(uint32_t *)&ticket_mask_data);
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
	fprintf(stdout, "misc_ctrl_data : 0x%08X\n", *(uint32_t *)&misc_ctrl_data);
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
	fprintf(stdout, "core_ctrl_data : 0x%08X\n", *(uint32_t *)&core_ctrl_data);
	set_config(chain_id, 1, 0, CORE_CONTROL, &core_ctrl_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_frequency_by_index_on_chain(uint8_t chain_id, unsigned int pll_index)
{
	struct PLL_DATA pll_data;
	uint32_t vilpll_bswapped=bswap_32(freq_pll_1366[pll_index].vilpll);
	memcpy(pll_data.reg_data, &vilpll_bswapped, 4);
	g_boards[chain_id]->freq=freq_pll_1366[pll_index].freq;
	fprintf(stdout, "set_frequency : %u MHz\n", freq_pll_1366[pll_index].freq);
	set_config(chain_id, 1, 0, PLL_PARAMETER, &pll_data);
	usleep(CHAIN_CONFIG_INTERVAL*4);
}

void set_analog_mux_control_one_chain(uint8_t chain_id, uint32_t mux_control)
{
	struct ANALOG_MUX_CONTROL_DATA analog_mux_control_data;
	*(uint32_t *)analog_mux_control_data.reg_data=bswap_32(mux_control);
	fprintf(stdout, "analog_mux_control_data : 0x%08X\n", *(uint32_t *)&analog_mux_control_data);
	set_config(chain_id, 1, 0, ANALOG_MUX_CONTROL, &analog_mux_control_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_io_driver_strength_one_chain(uint8_t chain_id, uint32_t io_driver_strength)
{
	struct IO_DRIVER_STRENGTH_DATA io_driver_strength_data;
	*(uint32_t *)io_driver_strength_data.reg_data=bswap_32(io_driver_strength);
	fprintf(stdout, "io_driver_strength_data : 0x%08X\n", *(uint32_t *)&io_driver_strength_data);
	set_config(chain_id, 1, 0, IO_DRIVER_STRENGTH, &io_driver_strength_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_fast_uart_configuration_one_chain(uint8_t chain_id, uint32_t fast_uart_configuration)
{
	struct FAST_UART_CONFIGURATION_DATA fast_uart_configuration_data;
	*(uint32_t *)fast_uart_configuration_data.reg_data=bswap_32(fast_uart_configuration);
	fprintf(stdout, "fast_uart_configuration_data : 0x%08X\n", *(uint32_t *)&fast_uart_configuration_data);
	set_config(chain_id, 1, 0, FAST_UART_CONFIGURATION, &fast_uart_configuration_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void set_uart_relay_one_chain(uint8_t chain_id, uint8_t tx_relay_en, uint8_t rx_relay_en, uint8_t shift)
{
	struct UART_RELAY_DATA uart_relay_data;
	uint32_t data_swapped;
	uint8_t chip_id;
	uart_relay_data.co_relay_en=tx_relay_en;
	uart_relay_data.ro_relay_en=rx_relay_en;
	uart_relay_data.gap=shift;
	for(chip_id=1; chip_id<ASICS_ON_CHAIN_MAX; chip_id+=DOMAIN_SIZE)
	{
		data_swapped=bswap_32(*(uint32_t *)(&uart_relay_data));
		set_config(chain_id, 0, ASICS_ON_CHAIN_MAX-chip_id, UART_RELAY, &data_swapped);
		set_config(chain_id, 0, ASICS_ON_CHAIN_MAX-chip_id-DOMAIN_SIZE+1, UART_RELAY, &data_swapped);
		uart_relay_data.gap+=DOMAIN_SIZE;
		usleep(10000);
	}
}

void set_version_mask_one_chain(uint8_t chain_id, uint32_t mask)
{
	struct VERSION_MASK_DATA version_mask_data;
	*(uint32_t *)version_mask_data.reg_data=bswap_32(mask);
	fprintf(stdout, "version_mask_data : 0x%08X\n", *(uint32_t *)&version_mask_data);
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
	fprintf(stdout, "unknown_reg_a8_data : 0x%08X\n", *(uint32_t *)&unknown_reg_a8_data);
	set_config(chain_id, 1, 0, UNKNOWN_REG_A8, &unknown_reg_a8_data);
	usleep(CHAIN_CONFIG_INTERVAL);
}

void _simple_send_raw_data(uint8_t chain, const uint8_t reg_data[], uint32_t length)
{
	pthread_mutex_lock(&g_boards[chain]->tty_rw_mutex);
	write(g_boards[chain]->tty_fd, reg_data, length);
	pthread_mutex_unlock(&g_boards[chain]->tty_rw_mutex);
}

void i2c_init()
{
	g_device->i2c_fd=open(I2C_DEVICE, O_RDWR | O_NONBLOCK);
	if(g_device->i2c_fd<0)
    {
		fprintf(stderr, "i2c init error. Cannot open %s\n", I2C_DEVICE);
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

static inline void clear_nonce_buf()
{
	pthread_mutex_lock(&nonce_fifo_mutex);
	g_nonce_fifo.p_wr=0;
	g_nonce_fifo.p_rd=0;
	g_nonce_fifo.nonce_num=0;
	pthread_mutex_unlock(&nonce_fifo_mutex);
}

static inline uint8_t *detect_packet(uint8_t *buffer, uint32_t buffer_size, uint32_t packet_length)
{
	uint8_t *scan_ptr;
	for(scan_ptr=buffer; scan_ptr+packet_length<buffer+buffer_size; scan_ptr++)
	{
		if(scan_ptr[0]==PACKET_PREAMBLE_2 && scan_ptr[1]==PACKET_PREAMBLE_1)
		{
			if(scan_ptr[packet_length]==PACKET_PREAMBLE_2)
			{
				return(scan_ptr);
			}
		}
	}
	return(NULL);
}

void *pic_heart_beat_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	uint8_t board_id=0, sensor_id;
	while(1)
	{
		sleep(PIC_HEART_BEAT_INTERVAL);
		for(board_id=0; board_id<CHAINS_MAX; board_id++)
		{
			if(g_boards[board_id]->exist)
			{
//				pic_heart_beat(board_id);
//				usleep(PIC_PACKET_INTERVAL);
//				pic_read_software_version(board_id);
//				usleep(PIC_PACKET_INTERVAL);
//				fprintf(stdout, "board[%u] pic_software_version: %s\n", board_id, g_boards[board_id]->pic_software_version);

				g_device->temperature_max=0.0;
				for(sensor_id=0; sensor_id<TEMPERATURE_SENSORS_PER_BOARD; sensor_id++)
				{
//					pic_request_temperature_from_sensor(board_id, sensor_id);
//					usleep(PIC_PACKET_INTERVAL);
//					pic_report_temperature(board_id, sensor_id);
//					usleep(PIC_PACKET_INTERVAL);
					if(g_device->temperature_max<g_boards[board_id]->temperature[sensor_id])
					{
						g_device->temperature_max=g_boards[board_id]->temperature[sensor_id];
					}
				}
//				fprintf(stdout, "board[%u] sensor[%u] temperature=%.3f\n", board_id, sensor_id, g_boards[board_id]->temperature[sensor_id]);
			}
		}
	}
}

// remove all excessive bits and preserve only one least significant bit
static inline uint64_t round_diff_value_down(uint64_t diff_val)
{
	uint64_t diff=1;
	uint8_t i;
	for(i=0; i<63 && diff<diff_val; i++)
	{
		diff=diff<<1;
	}
	return diff;
}

static int64_t s19xph_scanwork(struct thr_info *mining_thr)
{
	uint32_t nonce, version;
	uint8_t work_id, board_id, submit_nonce_ok;
//	uint8_t byte;

	struct work *work=NULL;
	int64_t hashes=0;

	pthread_mutex_lock(&nonce_fifo_mutex);
	pthread_mutex_lock(&works_mutex);
	while(g_nonce_fifo.nonce_num)
	{
		nonce=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].nonce;
		work_id=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].work_id;
		board_id=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].board_id;
		version=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].version;
		version=version<<13;

//		fprintf(stdout, "board_id=%u work_id=%u nonce=%u\n", board_id, work_id, nonce);

		work=g_boards[board_id]->works[work_id];
		if(work)
		{
			work->version=version;
			submit_nonce_ok=submit_nonce(mining_thr, work, nonce);
			if(submit_nonce_ok)
			{
//				fprintf(stdout, "work->hash ");
//				for(byte=0; byte<32; byte++)
//				{
//					fprintf(stdout, "%02x ", work->hash[byte]);
//				}
//				fprintf(stdout, "\n");

//				fprintf(stdout, "work->target ");
//				for(byte=0; byte<32; byte++)
//				{
//					fprintf(stdout, "%02x ", work->target[byte]);
//				}
//				fprintf(stdout, "\n");
				hashes+=round_diff_value_down(work->work_difficulty);
				free_work(&g_boards[board_id]->works[work_id]);
				g_device->valid_nonce_count++;
			}
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
	return hashes;
}

void *fill_work_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	board_parameters_t *brd=(board_parameters_t *)arg;
	struct timeval now, last_send, send_elapsed;
	uint8_t merkle_root_le[32];
	uint8_t prev_block_hash_le[32];
	uint8_t work_id=0;
//	uint8_t i;
	struct work *work=NULL;
	work_1366_t workdata;

	gettimeofday(&last_send, NULL);
	gettimeofday(&now, NULL);

	workdata.preamble[0]=PACKET_PREAMBLE_1;
	workdata.preamble[1]=PACKET_PREAMBLE_2;
	workdata.command=0x21;
	workdata.not_a_length=0x36;

	while(brd->powered_on)
	{
		gettimeofday(&now, NULL);
		timersub(&now, &last_send, &send_elapsed);
		if(send_elapsed.tv_usec>brd->work_update_interval)
		{
			work=get_work(g_device->mining_control_thr);
			pthread_mutex_lock(&works_mutex);
//			for(i=0; i<=WORKS_PER_CHAIN && brd->works[work_id]; i++)
			{
				work_id++;
				if(work_id>=WORKS_PER_CHAIN)
				{
					work_id=0;
				}
			}
			if(brd->works[work_id])
			{
				free_work(&brd->works[work_id]);
			}
			brd->works[work_id]=copy_work(work);
			free_work(&work);
			work=brd->works[work_id];
			workdata.work_id=work_id << 3; // B11111000
			workdata.num_midstates=1;
			workdata.starting_nonce=0;
			hex2bin((uint8_t *)&workdata.nbits, work->pool->nbit, 4);
			workdata.nbits=bswap_32(workdata.nbits);
			hex2bin((uint8_t *)&workdata.ntime, work->ntime, 4);
			workdata.ntime=bswap_32(workdata.ntime);
			memcpy(merkle_root_le, work->data+36, 32);
			swab256(workdata.merkle_root, merkle_root_le);
			hex2bin(prev_block_hash_le, work->pool->prev_hash, 32);
			swab256(workdata.prev_block_hash, prev_block_hash_le);
			workdata.version=*(uint32_t *)work->pool->header_bin;
			workdata.version=bswap_32(workdata.version);
			workdata.crc16=crc16_itu(0xFFFF, PACKET_PREAMBLE_LENGTH+(uint8_t *)&workdata, 84);
			workdata.crc16=bswap_16(workdata.crc16);
			pthread_mutex_unlock(&works_mutex);
			pthread_mutex_lock(&brd->tty_rw_mutex);
			write(brd->tty_fd, &workdata, PACKET_PREAMBLE_LENGTH+ASIC_WORK_PACKET_LENGTH);
			pthread_mutex_unlock(&brd->tty_rw_mutex);
			gettimeofday(&last_send, NULL);
		}
	}
	return NULL;
}

void *get_asic_response_thr_func(void *arg)
{
	pthread_detach(pthread_self());
	board_parameters_t *brd=(board_parameters_t *)arg;
	uint8_t in_data_buf[ASIC_RESPONSE_BUF_SIZE];
	asic_nonce_result_t *nonce_response;
	asic_reg_value_result_t *reg_value_response;
	uint8_t *packet_ptr;
//	uint8_t crc_packet, crc_true;
	uint32_t bytes_available, bytes_stored=0;
	while(brd->powered_on)
	{
		if(bytes_stored>=ASIC_RESPONSE_BUF_SIZE)
		{
			bytes_stored=0;
		}
		pthread_mutex_lock(&brd->tty_rw_mutex);
		bytes_available=get_bytes_num_in_fd(brd->tty_fd);
		if(bytes_available+bytes_stored>ASIC_RESPONSE_BUF_SIZE)
		{
			bytes_available=ASIC_RESPONSE_BUF_SIZE-bytes_stored;
		}
		if(bytes_available)
		{
			if(read(brd->tty_fd, in_data_buf+bytes_stored, bytes_available)!=bytes_available)
			{
				fprintf(stderr, "brd->tty_fd read error!\n");
				pthread_mutex_unlock(&brd->tty_rw_mutex);
				continue;
			}
			bytes_stored+=bytes_available;
		}
		pthread_mutex_unlock(&brd->tty_rw_mutex);
//		crc_true=crc5(packet_ptr+ASIC_PACKET_PREAMBLE_LENGTH, 7*8-5); //data minus 5-bit CRC
//		crc_packet	=packet_ptr[8] & 0x1F; //5-bit len CRC
//		if(crc_packet!=crc_true)
//		{
//			fprintf(stderr, "Chain[%u] CRC5 error, should be 0x%02x, received 0x%02x\n", brd->id, crc_true, crc_packet);
//			continue;
//		}
		packet_ptr=detect_packet(in_data_ptr, bytes_stored, ASIC_NONCE_PACKET_LENGTH);
		if(packet_ptr)
		{
			nonce_response=(asic_nonce_result_t *)packet_ptr;
			pthread_mutex_lock(&nonce_fifo_mutex);
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].nonce		=bswap_32(nonce_response->nonce);
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].midstate_num	=nonce_response->midstate_num;
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].work_id		=nonce_response->work_id >> 3; // B00011111
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].version		=bswap_16(nonce_response->version);
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].crc			=nonce_response->crc;
			g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].board_id		=brd->id;
			if(++g_nonce_fifo.p_wr>=MAX_NONCE_NUMBER_IN_FIFO)
			{
				g_nonce_fifo.p_wr=0;
			}
			if(g_nonce_fifo.nonce_num<MAX_NONCE_NUMBER_IN_FIFO)
			{
				g_nonce_fifo.nonce_num++;
			}
			pthread_mutex_unlock(&nonce_fifo_mutex);
			packet_ptr+=ASIC_NONCE_PACKET_LENGTH;
			bytes_stored-=(packet_ptr-in_data_ptr);
			memmove(in_data_ptr, packet_ptr, bytes_stored);
			continue;
		}

		packet_ptr=detect_packet(in_data_ptr, bytes_stored, ASIC_REG_VALUE_PACKET_LENGTH);
		if(packet_ptr)
		{
			reg_value_response=(asic_reg_value_result_t *)packet_ptr;
			reg_value_response->reg_value=bswap_32(reg_value_response->reg_value);
			switch(reg_value_response->reg_addr)
			{
				case CHIP_ADDR:
				{
//					fprintf(stdout, "CHIP_ADDR: 0x%X CHIP_ADDR: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					if(update_asic_num)
					{
						if(++brd->number_of_asics>ASICS_ON_CHAIN_MAX)
						{
							brd->number_of_asics=ASICS_ON_CHAIN_MAX;
						}
					}
					break;
				}
				case HASHRATE:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%X HASHRATE: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case PLL_PARAMETER:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%X PLL_PARAMETER: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case NONCE_OFFSET:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%X NONCE_OFFSET: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case HASH_COUNTER:
				{
					fprintf(stdout, "CHIP_ADDR: 0x%X HASH_COUNTER: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case TICKET_MASK:
				{
//					fprintf(stdout, "CHIP_ADDR: 0x%X TICKET_MASK: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case MISC_CONTROL:
				{
//					fprintf(stdout, "CHIP_ADDR: 0x%X MISC_CONTROL: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				case I2C_CONTROL:
				{
//					fprintf(stdout, "CHIP_ADDR: 0x%X I2C_CONTROL: 0x%X\n", reg_value_response->chip_addr, reg_value_response->reg_value);
					break;
				}
				default:
				{
					break;
				}
			}
			packet_ptr+=ASIC_REG_VALUE_PACKET_LENGTH;
			bytes_stored-=(packet_ptr-in_data_ptr);
			memmove(in_data_ptr, packet_ptr, bytes_stored);
		}
	}
	return NULL;
}

void power_up_board(uint8_t board_id)
{
	fprintf(stdout, "Board[%u] will be powered up now.\n", board_id);
	pic_enable_voltage(board_id);
	g_boards[board_id]->powered_on=1;
}

void shutdown_board(uint8_t board_id)
{
	fprintf(stdout, "Board[%u] will be turned off now.\n", board_id);
	pic_disable_voltage(board_id);
	g_boards[board_id]->powered_on=0;
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
	power_up_board(board_id);
	sleep(1);
	reset_board(board_id);
	usleep(500000);
	tty_init_one_chain(board_id, B115200);
	ptcret=pthread_create(&g_boards[board_id]->uart_rx_thr, NULL, get_asic_response_thr_func, &g_boards[board_id]);
	if(ptcret)
	{
		fprintf(stderr, "Create RX thread for Chain[%u] : failed\n", board_id);
	}
	else
	{
		fprintf(stdout, "Create RX thread for Chain[%u] : ok\n", board_id);
	}
	ptcret=pthread_create(&g_boards[board_id]->uart_tx_thr, NULL, fill_work_thr_func, &g_boards[board_id]);
	if(ptcret)
	{
		fprintf(stderr, "Create TX thread for Chain[%u] : failed\n", board_id);
	}
	else
	{
		fprintf(stdout, "Create TX thread for Chain[%u] : ok\n", board_id);
	}
	chain_inactive(board_id);
	usleep(250000);
	set_asic_address_one_chain(board_id);
	set_core_ctrl_one_chain(board_id, 0x80, 0x00, 0x85, 0x40);
	set_core_ctrl_one_chain(board_id, 0x80, 0x00, 0x80, 0x20);
	set_ticket_mask_one_chain(board_id, 0x000000FF);
	set_analog_mux_control_one_chain(board_id, 0x00000003);
	set_io_driver_strength_one_chain(board_id, 0x02111111);
	set_uart_relay_one_chain(board_id, 1, 1, 24);
//	set_fast_uart_configuration_one_chain(board_id, 0x11300000); // 3M
	set_fast_uart_configuration_one_chain(board_id, 0x11300200); // 1M
	sleep(4);
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
	shutdown_board(board_id);
	g_boards[board_id]->hw_errors=0;
	tcflush(g_boards[board_id]->tty_fd, TCIOFLUSH);
	close(g_boards[board_id]->tty_fd);
}

static void s19xph_detect(bool hotplug)
{
	struct cgpu_info *cgpu=calloc(1, sizeof(struct cgpu_info));
	if(!cgpu)
	{
		quit(1, "Failed to calloc 'cgpu'");
	}
	cgpu->drv=&antminer_s19xph_drv;
	cgpu->name=antminer_s19xph_drv.name;
	cgpu->deven=DEV_ENABLED;
	cgpu->threads=1;
	add_cgpu(cgpu);
}

static bool s19xph_prepare(struct thr_info *mining_thr)
{
	int check_asic_times=0;
	uint8_t board_id, chip_id;
	bool check_asic_fail=false;

	g_device=calloc(1, sizeof(device_parameters_t));
	if(!g_device)
	{
		return false;
	}

	g_device->mining_control_thr=mining_thr;
	g_device->asic_addr_interval=256/ASICS_ON_CHAIN_MAX;

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		g_boards[board_id]=calloc(1, sizeof(board_parameters_t));
		if(!g_boards[board_id])
		{
			return false;
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		g_boards[board_id]->id=board_id;
		g_boards[board_id]->work_update_interval=__UINT32_MAX__;
		opt_frequency[board_id]=DEFAULT_FREQUENCY;
		pthread_mutex_init(&g_boards[board_id]->tty_rw_mutex, NULL);
	}

	check_chains();
	usleep(10000);

	i2c_init();
	usleep(10000);

	if(apw11_init(g_device->i2c_fd, &i2c_mutex)<0)
	{
		return false;
	}
	psu_power_on();
	usleep(750000);
	apw11_jump_from_loader_to_app();
	usleep(650000);

	apw11_set_voltage_instantly(INIT_VOLTAGE);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			pic_reset(board_id);
		}
	}
	usleep(500000);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			pic_jump_from_loader_to_app(board_id);
		}
	}
	usleep(500000);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			power_up_board(board_id);
		}
	}
	usleep(500000);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			fprintf(stdout, "Board[%u] reset asics\n", board_id);
			reset_board(board_id);
		}
	}
	usleep(500000);

	clear_nonce_buf();

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		int ptcret;
		if(g_boards[board_id]->exist)
		{
			tty_init_one_chain(board_id, B115200);
			ptcret=pthread_create(&g_boards[board_id]->uart_rx_thr, NULL, get_asic_response_thr_func, (void *)g_boards[board_id]);
			if(ptcret)
			{
				fprintf(stderr, "Create RX thread for Chain[%u] : fail\n", board_id);
			}
			else
			{
				fprintf(stdout, "Create RX thread for Chain[%u] : ok\n", board_id);
			}
			ptcret=pthread_create(&g_boards[board_id]->uart_tx_thr, NULL, fill_work_thr_func, (void *)g_boards[board_id]);
			if(ptcret)
			{
				fprintf(stderr, "Create TX thread for Chain[%u] : fail\n", board_id);
			}
			else
			{
				fprintf(stdout, "Create TX thread for Chain[%u] : ok\n", board_id);
			}
		}
	}

	//check ASIC number for every chain
	fprintf(stdout, "send cmd to get chip address\n");

check_asic_num:
	update_asic_num=true;
	check_asic_reg_all_chains(0, CHIP_ADDR, 1);
	sleep(2);
	update_asic_num=false;

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			if(g_boards[board_id]->number_of_asics!=ASICS_ON_CHAIN_MAX)
			{
				check_asic_fail=true;
			}
			else
			{
				for(chip_id=0; chip_id<ASICS_ON_CHAIN_MAX; chip_id++)
				{
					g_boards[board_id]->asic_alive[chip_id]=1;
				}
			}
			fprintf(stdout, "Board[%u].number_of_asics=%u\n", board_id, g_boards[board_id]->number_of_asics);
		}
	}

	if(check_asic_fail && check_asic_times<3)
	{
		fprintf(stderr, "Need to recheck asic num...\n");
		for(board_id=0; board_id<CHAINS_MAX; board_id++)
		{
			g_boards[board_id]->number_of_asics=0;
			for(chip_id=0; chip_id<ASICS_ON_CHAIN_MAX; chip_id++)
			{
				g_boards[board_id]->asic_alive[chip_id]=0;
			}
		}
		check_asic_fail=false;
		check_asic_times++;
		goto check_asic_num;
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			if(g_boards[board_id]->number_of_asics!=ASICS_ON_CHAIN_MAX)
			{
				fprintf(stdout, "Board[%u] looks bad.\n", board_id);
				disable_board(board_id);
			}
			else
			{
				fprintf(stdout, "Board[%u] looks ok.\n", board_id);
				g_boards[board_id]->hash_rate=1;
			}
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			set_unknown_reg_a8_one_chain(board_id, 0x00, 0x07, 0x00, 0x00);
			set_misc_ctrl_one_chain(board_id, 0xFF, 0x0F, 0xC1, 0x00);

			chain_inactive(board_id);
			usleep(250000);
			set_asic_address_one_chain(board_id);
			set_core_ctrl_one_chain(board_id, 0x80, 0x00, 0x85, 0x40);
			set_core_ctrl_one_chain(board_id, 0x80, 0x00, 0x80, 0x20);

			set_ticket_mask_one_chain(board_id, 0x000000FF);
			set_version_mask_one_chain(board_id, 0x9000FFFF);
			set_analog_mux_control_one_chain(board_id, 0x00000003);

			set_io_driver_strength_one_chain(board_id, 0x02111111);

//			set_uart_relay_one_chain(board_id, 1, 1, 24);
//			set_uart_relay_one_chain(board_id, 1, 1, 32);

//			set_nonce_offset_one_chain(board_id, 322); // 0x0142 default step
			set_nonce_offset_one_chain(board_id, 328); // 0x0148
//			set_hash_counter_one_chain(board_id,  0x00001618);
//			set_hash_counter_one_chain(board_id,  0x0000152C);
			set_hash_counter_one_chain(board_id,  0x0000147A);

//			set_fast_uart_configuration_one_chain(board_id, 0x11300000); // 3M
			set_fast_uart_configuration_one_chain(board_id, 0x11300200); // 1M
			sleep(1);
//			tty_init_one_chain(board_id, B3000000);
			tty_init_one_chain(board_id, B1000000);
		}
	}

	sleep(3);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			for(chip_id=0; chip_id<ASICS_ON_CHAIN_MAX; chip_id++)
			{
//				set_misc_ctrl_one_chip(chain_id, chip_id, 0xFF, 0x0F, 0xC0, 0x00);
//				set_misc_ctrl_one_chip(chain_id, chip_id, 0xF0, 0x00, 0xC0, 0x00);
//				set_misc_ctrl_one_chip(chain_id, chip_id, 0xF0, 0x00, 0xC3, 0x00);
				set_misc_ctrl_one_chip(board_id, chip_id, 0xF0, 0x0F, 0xC1, 0x00);

				set_unknown_reg_a8_one_chip(board_id, chip_id, 0x00, 0x07, 0x01, 0xF0);

				set_core_ctrl_one_chip(board_id, chip_id, 0x80, 0x00, 0x85, 0x40);
				set_core_ctrl_one_chip(board_id, chip_id, 0x80, 0x00, 0x80, 0x20);
				set_core_ctrl_one_chip(board_id, chip_id, 0x80, 0x00, 0x82, 0xAA);
			}
		}
	}

	sleep(1);

	uint32_t freq_ind;
	for(freq_ind=0; freq_ind<sizeof(freq_pll_1366)/sizeof(freq_pll_1366[0]); freq_ind++)
	{
		for(board_id=0; board_id<CHAINS_MAX; board_id++)
		{
			if(g_boards[board_id]->exist)
			{
				set_frequency_by_index_on_chain(board_id, freq_ind);
			}
		}
	}

	if(pthread_create(&pic_heart_beat_thr, NULL, pic_heart_beat_thr_func, NULL))
	{
		fprintf(stderr, "Create thread for pic_heart_beat_thr_func() failed\n");
		return false;
	}

	apw11_set_voltage_gradually(opt_psu_voltage);

	return true;
}

static struct api_data *s19xph_get_api_stats(struct cgpu_info *cgpu)
{
	unsigned int board_id;
	char field_name_str[32], board_data_str[768];
	struct api_data *root=NULL;

	root=api_add_uint8(root, "chain_num", &(g_device->hash_boards_active), false);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			double chmhs=g_boards[board_id]->hash_rate/1000000.0;
			sprintf(field_name_str, "board%u_mhs", board_id+1);
			root=api_add_mhs(root, field_name_str, &chmhs, true);
		}
	}

	sprintf(field_name_str, "chain");
	sprintf(board_data_str, "[");
	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			sprintf(&board_data_str[strlen(board_data_str)], "\"index\":%u,", g_boards[board_id]->id);
			sprintf(&board_data_str[strlen(board_data_str)], "\"freq_avg\":%u,", g_boards[board_id]->freq);
			sprintf(&board_data_str[strlen(board_data_str)], "\"asic_num\":%u,", g_boards[board_id]->number_of_asics);
			sprintf(&board_data_str[strlen(board_data_str)], "\"temp_pic\":[%.2f,%.2f,%.2f,%.2f],", g_boards[board_id]->temperature[0], g_boards[board_id]->temperature[1], g_boards[board_id]->temperature[2], g_boards[board_id]->temperature[3]);
			sprintf(&board_data_str[strlen(board_data_str)], "\"hw\":%u", g_boards[board_id]->hw_errors);
		}
	}
	strcat(board_data_str, "]");
	root=api_add_string(root, field_name_str, board_data_str, true);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			sprintf(field_name_str, "board%u_frequency", board_id+1);
			root=api_add_uint32(root, field_name_str, &(g_boards[board_id]->freq), false);
		}
	}

	float chain_voltage, chain_power;

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			chain_voltage=apw11_get_voltage();
			chain_power=0;
			sprintf(field_name_str, "board%u_power", board_id+1);
			root=api_add_volts(root, field_name_str, &chain_power, true);
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			sprintf(field_name_str, "chain_hw%u", board_id+1);
			root=api_add_uint32(root, field_name_str, &(g_boards[board_id]->hw_errors), false);
		}
	}

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			sprintf(field_name_str, "board%u_temperature", board_id+1);
			root=api_add_temp(root, field_name_str, &(g_boards[board_id]->temperature[0]), false);
		}
	}

	root=api_add_temp(root, "temp_max", &g_device->temperature_max, false);
	uint32_t n_summ=g_device->valid_nonce_count+cgpu->hw_errors;

	double hwp=n_summ ? (double)(cgpu->hw_errors) / (double)(n_summ) : 0;
	root=api_add_percent(root, "hwe_percentage", &hwp, true);
	root=api_add_int(root, "hwe_total", &cgpu->hw_errors, false);

	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist && g_boards[board_id]->active)
		{
			sprintf(field_name_str, "board%u_acn", board_id+1);
			root=api_add_uint8(root, field_name_str, &(g_boards[board_id]->number_of_asics), false);
		}
	}

	return root;
}

static void s19xph_update_work_stub(struct cgpu_info *cgpu)
{
//	fprintf(stdout, "%s | s19xph_update_work_stub()\n", cgpu->name);
}

static void s19xph_update_work(struct cgpu_info *cgpu)
{
//	fprintf(stdout, "%s | s19xph_update_work()\n", cgpu->name);
	uint8_t board_id;
	for(board_id=0; board_id<CHAINS_MAX; board_id++)
	{
		if(g_boards[board_id]->exist)
		{
			g_boards[board_id]->work_update_interval=WORK_UPDATE_INTERVAL;
		}
	}
	cgpu->drv->update_work=&s19xph_update_work_stub;
//	pthread_mutex_lock(&works_mutex);
//	for(board_id=0; board_id<CHAINS_MAX; board_id++)
//	{
//		if(g_boards[board_id]->exist)
//		{
//		}
//	}
//	pthread_mutex_unlock(&works_mutex);
}

void s19xph_shutdown()
{
	uint8_t board_id;
	for(board_id=0; board_id<CHAINS_MAX; ++board_id)
	{
		disable_board(board_id);
	}
	psu_power_off();
	pthread_cancel(pic_heart_beat_thr);
}

struct device_drv antminer_s19xph_drv=
{
	.drv_id=DRIVER_antminer_s19xph,
	.dname="Antminer_S19XPh",
	.name="S19XPh",
	.drv_detect=s19xph_detect,
	.hash_work=hash_driver_work,
	.scanwork=s19xph_scanwork,
//	.flush_work=s19xph_update_work,
	.update_work=s19xph_update_work,
// WIP
//	.hw_error=s19xph_hw_error,
// WIP
//	.reinit_device=s19xph_reinit_device,
	.get_api_stats=s19xph_get_api_stats,
	.thread_prepare=s19xph_prepare,
	.thread_shutdown=s19xph_shutdown
};
