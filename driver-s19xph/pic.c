#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <string.h>

#include <errno.h>

#include "pic.h"
#include "driver-bm1366.h"

const uint8_t i2c_pic_addr[CHAINS_MAX]={0x20, 0x40, 0x60, 0x80};
const uint8_t i2c_lm75a_sensor_address[TEMPERATURE_SENSORS_PER_BOARD]={74, 72, 73, 75};
extern pthread_mutex_t i2c_mutex;

static uint8_t pic_reply[32];

static struct __attribute__((packed))
{
	uint16_t preamble;
	uint8_t length;
	uint8_t command;
	uint8_t data[256];
} pic_packet=
{
	.preamble=PIC_PACKET_PREAMBLE,
	.length=0,
	.command=0
};

static inline void pic_select_slave_device(uint8_t board_id)
{
	if(ioctl(g_device->i2c_fd, I2C_SLAVE, i2c_pic_addr[board_id])<0)
	{
		fprintf(stderr, "cannot select %02x\n", i2c_pic_addr[board_id]);
	}
}

void pic_refresh_packet_check_summ()
{
	uint32_t crc_acc, i, data_len;
	if(pic_packet.length<3)
	{
		return;
	}
	data_len=pic_packet.length-3;
	crc_acc=pic_packet.length+pic_packet.command;
	for(i=0; i<data_len; i++)
	{
		crc_acc+=pic_packet.data[i];
	}
	if(crc_acc&0xFF00)
	{
		crc_acc-=(crc_acc&0xFF00)>>8;
	}
	pic_packet.data[i]=crc_acc&0xFF;
}

void pic_heart_beat(uint8_t board_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=4;
	pic_packet.command=PIC_HEART_BEAT;
	pic_packet.data[0]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, PIC_HEART_BEAT_REPLY_LENGTH);
	if(pic_reply[0]!=PIC_HEART_BEAT_REPLY_LENGTH || pic_reply[1]!=PIC_HEART_BEAT)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	else
	{
		// WIP
		pic_reply;
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_jump_from_loader_to_app(uint8_t board_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=4;
	pic_packet.command=PIC_JUMP_FROM_LOADER_TO_APP;
	pic_packet.data[0]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, PIC_JUMP_REPLY_LENGTH);
	if(pic_reply[0]!=PIC_JUMP_FROM_LOADER_TO_APP)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	else
	{
		// WIP
		pic_reply;
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_reset(uint8_t board_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=4;
	pic_packet.command=PIC_RESET;
	pic_packet.data[0]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, PIC_RESET_REPLY_LENGTH);
	if(pic_reply[0]!=PIC_RESET)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_enable_voltage(uint8_t board_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=5;
	pic_packet.command=PIC_ENABLE_VOLTAGE;
	pic_packet.data[0]=0x01;
	pic_packet.data[1]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, PIC_ENABLE_VOLTAGE_REPLY_LENGTH);
	if(pic_reply[0]!=PIC_ENABLE_VOLTAGE)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_disable_voltage(uint8_t board_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=5;
	pic_packet.command=PIC_ENABLE_VOLTAGE;
	pic_packet.data[0]=0x00;
	pic_packet.data[1]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, PIC_ENABLE_VOLTAGE_REPLY_LENGTH);
	if(pic_reply[0]!=PIC_ENABLE_VOLTAGE)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_request_temperature_from_sensor(uint8_t board_id, uint8_t sensor_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=5;
	pic_packet.command=PIC_REQUEST_TEMPERATURE_FROM_SENSOR;
	pic_packet.data[0]=i2c_lm75a_sensor_address[sensor_id];
	pic_packet.data[1]=0x00;
//	pic_packet.data[2]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, 2);
	if(pic_reply[0]!=PIC_REQUEST_TEMPERATURE_FROM_SENSOR)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_report_temperature(uint8_t board_id, uint8_t sensor_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=5;
	pic_packet.command=PIC_REPORT_TEMPERATURE;
	pic_packet.data[0]=i2c_lm75a_sensor_address[sensor_id];
	pic_packet.data[1]=0x02;
//	pic_packet.data[2]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, PIC_REPORT_TEMPERATURE_REPLY_LENGTH);
	if(pic_reply[0]!=PIC_REPORT_TEMPERATURE_REPLY_LENGTH || pic_reply[1]!=PIC_REPORT_TEMPERATURE)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	else
	{
		g_boards[board_id]->temperature[sensor_id]=pic_reply[3];
		g_boards[board_id]->temperature[sensor_id]+=(float)(pic_reply[4]>>5)*0.125;
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_read_software_version(uint8_t board_id)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=4;
	pic_packet.command=PIC_READ_SOFTWARE_VERSION;
	pic_packet.data[0]=0x00;
	pic_refresh_packet_check_summ();
	pic_select_slave_device(board_id);
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, pic_reply, PIC_SOFTWARE_VERSION_REPLY_LENGTH);
	if(pic_reply[0]!=PIC_SOFTWARE_VERSION_REPLY_LENGTH || pic_reply[1]!=PIC_READ_SOFTWARE_VERSION)
	{
		fprintf(stderr, "board[%u] pic error!\n", board_id);
	}
	else
	{
		sprintf(&g_boards[board_id]->pic_software_version[0], "%02X", pic_reply[2]);
		sprintf(&g_boards[board_id]->pic_software_version[2], "%02X", pic_reply[3]);
		sprintf(&g_boards[board_id]->pic_software_version[4], "%02X", pic_reply[4]);
		g_boards[board_id]->pic_software_version[6]=0;
	}
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_send_data_to_pic(uint8_t *data, uint8_t d_len)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=d_len+3;
	pic_packet.command=SEND_DATA_TO_PIC;
	memcpy(pic_packet.data, data, d_len);
	pic_refresh_packet_check_summ();
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_write_data_into_flash()
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=4;
	pic_packet.command=PIC_WRITE_DATA_INTO_FLASH;
	pic_packet.data[0]=0x00;
	pic_refresh_packet_check_summ();
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_read_data_from_pic_flash(uint8_t *data, uint8_t d_len)
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=4;
	pic_packet.command=PIC_READ_DATA_FROM_FLASH;
	pic_packet.data[0]=0x00;
	pic_refresh_packet_check_summ();
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	usleep(PIC_PACKET_INTERVAL);
	read(g_device->i2c_fd, data, d_len);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_erase_pic_flash()
{
	pthread_mutex_lock(&i2c_mutex);
	pic_packet.length=4;
	pic_packet.command=PIC_ERASE_FLASH;
	pic_packet.data[0]=0x00;
	pic_refresh_packet_check_summ();
	write(g_device->i2c_fd, &pic_packet, pic_packet.length+PACKET_PREAMBLE_LENGTH);
	pthread_mutex_unlock(&i2c_mutex);
}
