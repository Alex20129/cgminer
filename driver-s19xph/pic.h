#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include <sys/ioctl.h>

#define PIC_PACKET_INTERVAL					25000 // us
#define PIC_HEART_BEAT_INTERVAL				10 // sec
#define PIC_FLASH_POINTER_START_ADDRESS_L       0x00
#define PIC_FLASH_POINTER_START_ADDRESS_H       0x03
#define PIC_FLASH_POINTER_END_ADDRESS_H         0x0F
#define PIC_FLASH_POINTER_END_ADDRESS_L         0x7F
#define PIC_FREQ_START_ADDRESS_H                0x0F
#define PIC_FREQ_START_ADDRESS_L                0xA0
#define PIC_FLASH_POINTER_FREQ_START_ADDRESS_H  0x0F
#define PIC_FLASH_POINTER_FREQ_START_ADDRESS_L  0xA0
#define PIC_FLASH_POINTER_FREQ_END_ADDRESS_H    0x0F
#define PIC_FLASH_POINTER_FREQ_END_ADDRESS_L    0xDF

#define PIC_SOFTWARE_VERSION_REPLY_LENGTH	5
#define PIC_HEART_BEAT_REPLY_LENGTH			6
#define PIC_REPORT_TEMPERATURE_REPLY_LENGTH	7
#define PIC_JUMP_REPLY_LENGTH				2
#define PIC_RESET_REPLY_LENGTH				2
#define PIC_ENABLE_VOLTAGE_REPLY_LENGTH		2

#define PIC_PACKET_PREAMBLE			0xAA55

#define SEND_DATA_TO_PIC			0x02    // just send data into pic's cache
#define PIC_READ_DATA_FROM_FLASH	0x03
#define PIC_ERASE_FLASH				0x04    // erase 32 bytes one time
#define PIC_WRITE_DATA_INTO_FLASH	0x05    // tell pic write data into flash from cache
#define PIC_JUMP_FROM_LOADER_TO_APP	0x06
#define PIC_RESET					0x07

#define PIC_SET_HOST_MAC_ADDRESS	0x14
#define PIC_ENABLE_VOLTAGE			0x15
#define PIC_HEART_BEAT				0x16
#define PIC_READ_SOFTWARE_VERSION	0x17

#define PIC_REQUEST_TEMPERATURE_FROM_SENSOR	0x3B
#define PIC_REPORT_TEMPERATURE				0x3C

#define PIC_PROGRAM "/etc/config/pic.txt"

void pic_heart_beat(uint8_t board_id);
void pic_jump_from_loader_to_app(uint8_t board_id);
void pic_reset(uint8_t board_id);
void pic_enable_voltage(uint8_t board_id);
void pic_disable_voltage(uint8_t board_id);
void pic_request_temperature_from_sensor(uint8_t board_id, uint8_t sensor_id);
void pic_report_temperature(uint8_t board_id, uint8_t sensor_id);
void pic_read_software_version(uint8_t board_id);
void pic_send_data_to_pic(uint8_t *data, uint8_t d_len);
void pic_write_data_into_flash();
void pic_read_data_from_pic_flash(uint8_t *data, uint8_t d_len);
void pic_erase_pic_flash();

#endif // PIC_H

