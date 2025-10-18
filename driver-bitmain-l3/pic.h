#ifndef PIC_H
#define PIC_H

#include <sys/types.h>
#include <sys/ioctl.h>

#define PIC_HEART_BEAT_INTERVAL					20 // s
#define PIC_FLASH_POINTER_START_ADDRESS_L       0x00
#define PIC_FLASH_POINTER_START_ADDRESS_H       0x03
#define PIC_FLASH_POINTER_END_ADDRESS_H         0x0f
#define PIC_FLASH_POINTER_END_ADDRESS_L         0x7f
#define PIC_FREQ_START_ADDRESS_H                0x0f
#define PIC_FREQ_START_ADDRESS_L                0xA0
#define PIC_FLASH_POINTER_FREQ_START_ADDRESS_H  0x0F
#define PIC_FLASH_POINTER_FREQ_START_ADDRESS_L  0xA0
#define PIC_FLASH_POINTER_FREQ_END_ADDRESS_H    0x0f
#define PIC_FLASH_POINTER_FREQ_END_ADDRESS_L    0xDF
#define PIC_FLASH_SECTOR_LENGTH             32
#define PIC_SOFTWARE_VERSION_LENGTH         1
#define PIC_VOLTAGE_TIME_LENGTH             6

#define SEND_COMMAND_1                      0x55
#define SEND_COMMAND_2                      0xAA
#define SET_PIC_FLASH_POINTER               0x01
#define SEND_DATA_TO_PIC                    0x02    // just send data into pic's cache
#define READ_DATA_FROM_PIC_FLASH            0x03
#define ERASE_PIC_FLASH                     0x04    // erase 32 bytes one time
#define WRITE_DATA_INTO_FLASH               0x05    // tell pic write data into flash from cache
#define JUMP_FROM_LOADER_TO_APP             0x06
#define RESET                               0x07
#define GET_PIC_FLASH_POINTER               0x08

#define SET_VOLTAGE                         0x10
#define SET_VOLTAGE_SETTING_TIME            0x11
#define SET_HASH_BOARD_ID                   0x12
#define READ_HASH_BOARD_ID                  0x13
#define SET_HOST_MAC_ADDRESS                0x14
#define ENABLE_VOLTAGE                      0x15
#define SEND_HEART_BEAT                     0x16
#define READ_SOFTWARE_VERSION               0x17
#define GET_VOLTAGE                         0x18
#define GET_VOLTAGE_SETTING_TIME            0x19
#define READ_WHICH_MAC                      0x20
#define READ_MAC                            0x21

#define MAX_CHAR_NUM		4028

#define PIC_PROGRAM "/etc/config/pic.txt"

void pic_select_slave_device(u_int8_t board_id);
void pic_send_command();
void pic_send_heart_beat(u_int8_t board_id);
void pic_jump_from_loader_to_app(u_int8_t board_id);
void pic_set_flash_pointer(u_int8_t flash_addr_h, u_int8_t flash_addr_l);
void pic_read_flash_pointer(u_int8_t *read_back_flash_addr_h, u_int8_t *read_back_flash_addr_l);
void pic_send_data_to_pic(unsigned char *data);
void pic_write_data_into_flash();
void pic_read_data_from_pic_flash(unsigned char *data);
void pic_reset(u_int8_t board_id);
void pic_set_voltage_setting_time(unsigned char *time);
void pic_get_voltage_setting_time(unsigned char *time);
void pic_set_voltage(u_int8_t board_id, u_int8_t volt_pic_val);
void pic_get_voltage(u_int8_t *volt_pic_val);
void pic_set_hash_board_id_number(u_int8_t *id);
void pic_get_hash_board_id_number(u_int8_t *id);
void pic_enable_voltage(u_int8_t board_id);
void pic_disable_voltage(u_int8_t board_id);
void pic_read_pic_software_version(unsigned char *version);
void pic_erase_pic_flash();
void pic_erase_flash_all();
void update_pic_program();
void flash_pic_freq(unsigned char *buf1);

#endif // PIC_H

