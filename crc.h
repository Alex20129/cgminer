#ifndef CRC_H
#define CRC_H

#include <stdint.h>

u_int8_t crc5(unsigned char *ptr, int bits);
uint16_t crc16(const unsigned char *data, int len);
uint16_t crc16_itu(uint16_t crc, const unsigned char *data, int len);
uint16_t crc16_modbus(const unsigned char *data, int len);

#endif // CRC_H
