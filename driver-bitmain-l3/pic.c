#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <string.h>

#include <errno.h>

#include "pic.h"
#include "driver-bitmain-l3.h"

const u_int8_t i2c_slave_addr[CHAINS_MAX]={0xA0, 0xA2, 0xA4, 0xA6};
extern pthread_mutex_t i2c_mutex;

void pic_select_slave_device(u_int8_t board_id)
{
	if(ioctl(device->i2c_fd, I2C_SLAVE, i2c_slave_addr[board_id] >> 1)<0)
	{
		perror(__FUNCTION__);
	}
}

void pic_send_command()
{
	u_int8_t send_command_cmd[2]={SEND_COMMAND_1, SEND_COMMAND_2};
	write(device->i2c_fd, send_command_cmd, 2);
}

void pic_send_heart_beat(u_int8_t board_id)
{
	u_int8_t send_heartbeat_cmd[1]={SEND_HEART_BEAT};
	pthread_mutex_lock(&i2c_mutex);
	pic_select_slave_device(board_id);
	pic_send_command();
	write(device->i2c_fd, send_heartbeat_cmd, 1);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_jump_from_loader_to_app(u_int8_t board_id)
{
	u_int8_t jump_from_loader_to_app_cmd[1]={JUMP_FROM_LOADER_TO_APP};
	pthread_mutex_lock(&i2c_mutex);
	pic_select_slave_device(board_id);
	pic_send_command();
	write(device->i2c_fd, jump_from_loader_to_app_cmd, 1);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_set_flash_pointer(u_int8_t flash_addr_h, u_int8_t flash_addr_l)
{
	u_int8_t set_flash_pointer_cmd[1]={SET_PIC_FLASH_POINTER};
    pic_send_command();
	write(device->i2c_fd, set_flash_pointer_cmd, 1);
	write(device->i2c_fd, &flash_addr_h, 1);
	write(device->i2c_fd, &flash_addr_l, 1);
}

void pic_read_flash_pointer(u_int8_t *read_back_flash_addr_h, u_int8_t *read_back_flash_addr_l)
{
	u_int8_t read_flash_pointer_cmd[1]={GET_PIC_FLASH_POINTER};
	pic_send_command();
	write(device->i2c_fd, read_flash_pointer_cmd, 1);
	read(device->i2c_fd, read_back_flash_addr_h, 1);
	read(device->i2c_fd, read_back_flash_addr_l, 1);
}

void pic_send_data_to_pic(unsigned char *data)
{
	u_int8_t send_data_to_pic_cmd[1]={SEND_DATA_TO_PIC};
	pic_send_command();
	write(device->i2c_fd, send_data_to_pic_cmd, 1);
	write(device->i2c_fd, data, 16);
}

void pic_write_data_into_flash()
{
	u_int8_t write_data_into_flash_cmd[1]={WRITE_DATA_INTO_FLASH};
	pic_send_command();
	write(device->i2c_fd, write_data_into_flash_cmd, 1);
}

void pic_read_data_from_pic_flash(unsigned char *data)
{
	u_int8_t Pic_read_data_from_pic_flash[1]={READ_DATA_FROM_PIC_FLASH};
	pic_send_command();
	write(device->i2c_fd, Pic_read_data_from_pic_flash, 1);
	read(device->i2c_fd, data, 16);
}

void pic_reset(u_int8_t board_id)
{
	u_int8_t reset_cmd[1]={RESET};
	pthread_mutex_lock(&i2c_mutex);
	pic_select_slave_device(board_id);
	pic_send_command();
	write(device->i2c_fd, reset_cmd, 1);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_set_voltage_setting_time(unsigned char *time)
{
	u_int8_t set_voltage_setting_time_cmd[1]={SET_VOLTAGE_SETTING_TIME};
	pic_send_command();
	write(device->i2c_fd, set_voltage_setting_time_cmd, 1);
	write(device->i2c_fd, time, 6);
}

void pic_get_voltage_setting_time(unsigned char *time)
{
	u_int8_t get_voltage_setting_time_cmd[1]={GET_VOLTAGE_SETTING_TIME};
	pic_send_command();
	write(device->i2c_fd, get_voltage_setting_time_cmd, 1);
	read(device->i2c_fd, time, 6);
}

void pic_set_voltage(u_int8_t board_id, u_int8_t volt_pic_val)
{
	u_int8_t set_voltage_cmd[2]={SET_VOLTAGE, volt_pic_val};
	pthread_mutex_lock(&i2c_mutex);
	pic_select_slave_device(board_id);
	pic_send_command();
	write(device->i2c_fd, set_voltage_cmd, 2);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_get_voltage(u_int8_t *volt_pic_val)
{
	u_int8_t get_voltage_cmd[1]={GET_VOLTAGE};
	pic_send_command();
	write(device->i2c_fd, get_voltage_cmd, 1);
	read(device->i2c_fd, volt_pic_val, 1);
}

void pic_set_hash_board_id_number(u_int8_t *id)
{
	u_int8_t set_hash_board_id_cmd[1]={SET_HASH_BOARD_ID};
	pic_send_command();
	write(device->i2c_fd, set_hash_board_id_cmd, 1);
	write(device->i2c_fd, id, 12);
}

void pic_get_hash_board_id_number(u_int8_t *id)
{
	u_int8_t read_hash_board_id_cmd[1]={READ_HASH_BOARD_ID};
	pic_send_command();
	write(device->i2c_fd, read_hash_board_id_cmd, 1);
	read(device->i2c_fd, id, 12);
}

void pic_enable_voltage(u_int8_t board_id)
{
	u_int8_t enable_voltage_cmd[2]={ENABLE_VOLTAGE, 1};
	pthread_mutex_lock(&i2c_mutex);
	pic_select_slave_device(board_id);
	pic_send_command();
	write(device->i2c_fd, enable_voltage_cmd, 2);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_disable_voltage(u_int8_t board_id)
{
	u_int8_t disable_voltage_cmd[2]={ENABLE_VOLTAGE, 0};
	pthread_mutex_lock(&i2c_mutex);
	pic_select_slave_device(board_id);
	pic_send_command();
	write(device->i2c_fd, disable_voltage_cmd, 2);
	pthread_mutex_unlock(&i2c_mutex);
}

void pic_read_pic_software_version(unsigned char *version)
{
	u_int8_t read_software_version_cmd[1]={READ_SOFTWARE_VERSION};
	pic_send_command();
	write(device->i2c_fd, read_software_version_cmd, 1);
	read(device->i2c_fd, version, 1);
}

void pic_erase_pic_flash()
{
	u_int8_t erase_pic_flash_cmd[1]={ERASE_PIC_FLASH};
	pic_send_command();
	write(device->i2c_fd, erase_pic_flash_cmd, 1);
}

void pic_erase_flash_all()
{
    unsigned int i=0, erase_loop=0;
    unsigned char start_addr_h=PIC_FLASH_POINTER_START_ADDRESS_H, start_addr_l=PIC_FLASH_POINTER_START_ADDRESS_L;
    unsigned char end_addr_h=PIC_FLASH_POINTER_END_ADDRESS_H, end_addr_l=PIC_FLASH_POINTER_END_ADDRESS_L;
    unsigned int pic_flash_length=0;
    pic_set_flash_pointer(PIC_FLASH_POINTER_START_ADDRESS_H, PIC_FLASH_POINTER_START_ADDRESS_L);
	pic_flash_length=(((unsigned int)end_addr_h << 8)+end_addr_l) - (((unsigned int)start_addr_h << 8)+start_addr_l)+1;
    erase_loop=pic_flash_length/PIC_FLASH_SECTOR_LENGTH;
    for(i=0; i<erase_loop; i++)
    {
        pic_erase_pic_flash();
    }
}

void update_pic_program()
{
	unsigned int pic_flash_length=0, i, data_int;
	unsigned char program_data[MAX_CHAR_NUM*10]={0};
    unsigned char start_addr_h=PIC_FLASH_POINTER_START_ADDRESS_H, start_addr_l=PIC_FLASH_POINTER_START_ADDRESS_L;
    unsigned char end_addr_h=PIC_FLASH_POINTER_END_ADDRESS_H, end_addr_l=PIC_FLASH_POINTER_END_ADDRESS_L;
	unsigned char buf[16]={0};
	char data_read[16];
	FILE *pic_program_file;
    // read upgrade file first, if it is wrong, don't erase pic, but just return;
    pic_program_file=fopen(PIC_PROGRAM, "r");
    if(!pic_program_file)
    {
        printf("\n%s: open hash_s8_app.txt failed\n", __FUNCTION__);
        return;
    }
	fseek(pic_program_file, 0, SEEK_SET);
	memset(program_data, 0, MAX_CHAR_NUM*10);

	memset(data_read, 0, 16);

	pic_flash_length=(((unsigned int)end_addr_h << 8)+end_addr_l) - (((unsigned int)start_addr_h << 8)+start_addr_l)+1;

    for(i=0; i<pic_flash_length; i++)
    {
		fgets(data_read, 16, pic_program_file);
        data_int=strtoul(data_read, NULL, 16);
		program_data[2*i+0]=(unsigned char)((data_int >> 8) & 0x000000ff);
		program_data[2*i+1]=(unsigned char)(data_int & 0x000000ff);
    }
    fclose(pic_program_file);

    // after read upgrade file correct, erase pic
//    pic_reset();
    pic_erase_flash_all();

    // write data into pic
    pic_set_flash_pointer(PIC_FLASH_POINTER_START_ADDRESS_H, PIC_FLASH_POINTER_START_ADDRESS_L);

    for(i=0; i<pic_flash_length/PIC_FLASH_SECTOR_LENGTH*4; i++)
    {
        memcpy(buf, program_data+i*16, 16);
        pic_send_data_to_pic(buf);
        pic_write_data_into_flash();
    }
}

void flash_pic_freq(unsigned char *buf1)
{
    unsigned char buf[16]={0};
	unsigned int i, erase_loop;
    unsigned char start_addr_h=PIC_FLASH_POINTER_FREQ_START_ADDRESS_H, start_addr_l=PIC_FLASH_POINTER_FREQ_START_ADDRESS_L;
    unsigned char end_addr_h=PIC_FLASH_POINTER_FREQ_END_ADDRESS_H, end_addr_l=PIC_FLASH_POINTER_FREQ_END_ADDRESS_L;
    unsigned int pic_flash_length=0;
    pic_set_flash_pointer(PIC_FLASH_POINTER_FREQ_START_ADDRESS_H, PIC_FLASH_POINTER_FREQ_START_ADDRESS_L);

	pic_flash_length=(((unsigned int)end_addr_h << 8)+end_addr_l) - (((unsigned int)start_addr_h << 8)+start_addr_l)+1;
    erase_loop=pic_flash_length/PIC_FLASH_SECTOR_LENGTH;

	for(i=0; i<erase_loop*4; i++)
    {
        memcpy(buf, buf1+i*16, 16);
        pic_send_data_to_pic(buf);
        pic_write_data_into_flash();
    }
}
