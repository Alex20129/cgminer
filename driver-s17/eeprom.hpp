#ifndef EEPROM_HPP
#define EEPROM_HPP

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

#define ACTIVE_CHAINS_NUM 16

struct eeprom_tuning_result_t
{
    u_int8_t freq[48];
    u_int16_t voltage;
    u_int32_t hash_rate;
};

struct eeprom_layout_t
{
    u_int16_t fixture_header;
    u_int16_t fixture_version;
    u_int8_t hash_board_sn[20];
    u_int16_t pcb_version;
    u_int16_t bom_version;
    u_int8_t temp_sensor_type;
    u_int8_t product_id;
    u_int16_t crc_fixture;
    u_int16_t reserve_0[8];
    u_int16_t cgminer_header;
    eeprom_tuning_result_t tuning_ret[3];
    u_int16_t crc_cgminer;
    u_int8_t reserve_1[50];
};

extern bool g_is_eeprom_loaded;
extern eeprom_layout_t g_eeprom_buf[16];

enum FREQ_SCAN_ERRNO
{
    ASIC_NUM_ERR=0,
    EEPROM_SET_ERR=2,
    FAN_NUM_ERR=3,
    HASH_RATE_ERR=1,
    INBALANCE_NUM_ERR=4
};

struct SCAN_FREQ_RESULT
{
    unsigned char magic1;
    unsigned char magic2;
    unsigned char start_freq_high;
    unsigned char start_freq_low;
    unsigned char freq_step;
    unsigned char freq_decrease;
    unsigned char column_freq_level[4];
    unsigned char crc16_high;
    unsigned char crc16_low;
    unsigned int freq_eeprom[108];
};

u_int16_t CRC16(const u_int8_t *p_data, unsigned int w_len);
unsigned char CRC5(unsigned char *ptr, unsigned int len);

unsigned int get_iic();
unsigned char set_iic(unsigned int data);
unsigned char znyq_set_iic(unsigned char dev_addr, unsigned char which_iic, bool read, bool reg_addr_valid, unsigned char reg_addr, unsigned char data);

unsigned int _eeprom_write_iic(unsigned int chain, unsigned int reg_addr, unsigned char data);
unsigned char _eeprom_read_iic(unsigned int chain, unsigned int reg_addr);

int _eeprom_write_iic_bytes(unsigned int chain, unsigned int reg_addr_start, unsigned int reg_count, unsigned char *buf);
int _eeprom_read_iic_bytes(unsigned int chain, unsigned int reg_addr_start, unsigned int reg_count, unsigned char *buf);

void _eeprom_dump_raw(unsigned int *buf, int len);
void _eeprom_dump_fileds(eeprom_layout_t *eeprom_buf);
void _eeprom_dump(eeprom_layout_t *eeprom_buf);
bool _eeprom_is_fixture_crc_pass(eeprom_layout_t *eeprom_buf);
bool _eeprom_is_cgminer_crc_pass(eeprom_layout_t *eeprom_buf);
bool _eeprom_is_fixture_header_pass(eeprom_layout_t *eeprom_buf);
bool _eeprom_is_cgminer_header_pass(eeprom_layout_t *eeprom_buf);
int _eeprom_load_one_chain(unsigned int chain, eeprom_layout_t *eeprom_buf);
int _eeprom_flush_one_chain(unsigned int chain, eeprom_layout_t *eeprom_buf);
void eeprom_load();
int eeprom_flush();
void eeprom_dump();
unsigned int eeprom_get_pcb_version(unsigned int chain);
unsigned int eeprom_get_bom_version(unsigned int chain);
int eeprom_get_freq_store_step();
int eeprom_get_freq(unsigned int chain, int mode, unsigned int *buf, int len);
int eeprom_get_voltage(unsigned int chain, int mode, int *voltage);
int eeprom_get_hash_rate(int chain, int mode, unsigned int *hash_rate);
int eeprom_set_freq(unsigned int chain, int mode, int *buf, unsigned int len);
int eeprom_set_voltage(int chain, int mode, int voltage);
int eeprom_set_hash_rate(int chain, int mode, unsigned int hash_rate);

#endif //EEPROM_HPP
