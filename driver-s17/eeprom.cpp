#include <sys/mman.h>
#include <memory.h>
#include <pthread.h>
#include "driver-btm-soc.hpp"
#include "eeprom.hpp"
#include "logging.hpp"

bool g_is_eeprom_loaded=false;
eeprom_layout_t g_eeprom_buf[16];

// --------------------------------------------------------------
//      CRC16 check table
// --------------------------------------------------------------
const u_int8_t chCRCHTalbe[]=                                // CRC high byte table
{
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40
};

const u_int8_t chCRCLTalbe[]=                                // CRC low byte table
{
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7,
    0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E,
    0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9,
    0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC,
    0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32,
    0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D,
    0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38,
    0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF,
    0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1,
    0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4,
    0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB,
    0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA,
    0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0,
    0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97,
    0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E,
    0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89,
    0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83,
    0x41, 0x81, 0x80, 0x40
};

u_int16_t CRC16(const u_int8_t *p_data, unsigned int w_len)
{
    u_int8_t chCRCHi=0xFF; // CRC high byte initialize
    u_int8_t chCRCLo=0xFF; // CRC low byte initialize
    u_int16_t wIndex=0;    // CRC cycling index
    while(w_len--)
    {
        wIndex=chCRCLo ^ *p_data++;
        chCRCLo=chCRCHi ^ chCRCHTalbe[wIndex];
        chCRCHi=chCRCLTalbe[wIndex];
    }
    return((chCRCHi << 8) | chCRCLo);
}

unsigned char CRC5(unsigned char *ptr, unsigned int len)
{
    unsigned char i, j, k;
    unsigned char crc = 0x1f;
    unsigned char crcin[5] = {1, 1, 1, 1, 1};
    unsigned char crcout[5] = {1, 1, 1, 1, 1};
    unsigned char din = 0;

    j = 0x80;
    k = 0;
    for (i = 0; i<len; i++)
    {
        if (*ptr & j)
        {
            din = 1;
        }
        else
        {
            din = 0;
        }
        crcout[0] = crcin[4] ^ din;
        crcout[1] = crcin[0];
        crcout[2] = crcin[1] ^ crcin[4] ^ din;
        crcout[3] = crcin[2];
        crcout[4] = crcin[3];

        j = j >> 1;
        k++;
        if (k == 8)
        {
            j = 0x80;
            k = 0;
            ptr++;
        }
        memcpy(crcin, crcout, 5);
    }
    crc = 0;
    if(crcin[4])
    {
        crc |= 0x10;
    }
    if(crcin[3])
    {
        crc |= 0x08;
    }
    if(crcin[2])
    {
        crc |= 0x04;
    }
    if(crcin[1])
    {
        crc |= 0x02;
    }
    if(crcin[0])
    {
        crc |= 0x01;
    }
    return crc;
}

unsigned int get_iic()
{
  return axi_fpga_addr[0xc]; //12 ?
}

unsigned char set_iic(unsigned int data)
{
  unsigned int ret, wait_counter;

  wait_counter=0;
  axi_fpga_addr[0xc]=data & 0x3fffffff;
  while( true )
  {
    ret=get_iic();
    if((int)ret<0)
    {
      return(ret);
    }
    wait_counter++;
    if(wait_counter>100)
    {
      break;
    }
    usleep(5000);
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stderr, "%s:%d:%s: could not get iic, ret=0x%08x\n", "zynq.c", 0x29, "set_iic", ret);
  }
  return(ret);
}

unsigned char znyq_set_iic(unsigned char dev_addr, unsigned char which_iic, bool read, bool reg_addr_valid, unsigned char reg_addr, unsigned char data)
{
  unsigned char ret;
  unsigned int value=0;
  if(read)
  {
    value=0x2000000;
  }
  if(reg_addr_valid)
  {
    value=value | (unsigned int)reg_addr << 8 | 0x1000000;
  }
  ret=set_iic(value | ((unsigned int)which_iic & 3) << 0x1a | ((unsigned int)(dev_addr >> 3) & 0xf) << 0x14 | ((unsigned int)dev_addr & 7) << 0x10 | (unsigned int)data);
  return(ret);
}

unsigned int _eeprom_write_iic(unsigned int chain, unsigned int reg_addr, unsigned char data)
{
  bool reg_addr_valid=true;
  unsigned int uVar1;
  unsigned char which_iic=0;
  unsigned char dev_addr=chain & 7 | 0x50;

  uVar1=znyq_set_iic(dev_addr, which_iic, false, reg_addr_valid, reg_addr, data);
  return uVar1;
}

unsigned char _eeprom_read_iic(unsigned int chain, unsigned int reg_addr)
{
  bool reg_addr_valid=true;
  unsigned char uVar1;
  unsigned char which_iic=0;
  unsigned char dev_addr=chain & 7 | 0x50;

  uVar1=znyq_set_iic(dev_addr, which_iic, true, reg_addr_valid, reg_addr, 0);
  return uVar1;
}

int _eeprom_write_iic_bytes(unsigned int chain, unsigned int reg_addr_start, unsigned int reg_count, unsigned char *buf)
{
  int iVar1;
  unsigned int i;
  if(reg_addr_start+reg_count<257)
  {
    pthread_mutex_lock(iic_mutex);
    for(i=0; i<reg_count; i++)
    {
      _eeprom_write_iic(chain, reg_addr_start+i, buf[i]);
    }
    usleep(500000);
    pthread_mutex_unlock(iic_mutex);
    iVar1=0;
  }
  else
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stderr, "%s:%d:%s: Over EEPROM size.\n", "eeprom.c", 0x3f, "_eeprom_write_iic_bytes");
    }
    iVar1=-1;
  }
  return iVar1;
}

int _eeprom_read_iic_bytes(unsigned int chain, unsigned int reg_addr_start, unsigned int reg_count, unsigned char *buf)
{
  fprintf(stdout, "_eeprom_read_iic_bytes()\n");
  int iVar2;
  int i;
  if(reg_addr_start+reg_count<257)
  {
    pthread_mutex_lock(iic_mutex);
    for(i=0; i<reg_count; i++)
    {
      buf[i]=_eeprom_read_iic(chain, reg_addr_start+i);
    }
    pthread_mutex_unlock(iic_mutex);
    iVar2=0;
  }
  else
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stderr, "%s:%d:%s: Over EEPROM size.\n", "eeprom.c", 0x58, "_eeprom_read_iic_bytes");
    }
    iVar2=-1;
  }
  return iVar2;
}

void _eeprom_dump_raw(unsigned int *buf, int len)
{
  unsigned int uVar1;
  int i;

  for(i=0; i<len; i++)
  {
    if(((i & 0xfU)==0) && (log_level>LOG_INFO))
    {
      fprintf(stdout, "0x%04X ", i);
    }
    uVar1=i & 0xf;
    if(i<1)
    {
      uVar1=-(-i & 0xfU);
    }
    if((uVar1==8) && (log_level>LOG_INFO))
    {
      fprintf(stdout, "  ");
    }
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%02X ", buf[i]);
    }
    uVar1=i & 0xf;
    if(i<1)
    {
      uVar1=-(-i & 0xfU);
    }
    if((uVar1==0xf) && (log_level>LOG_INFO))
    {
      fprintf(stdout, "\n");
    }
  }
}

void _eeprom_dump_fileds(eeprom_layout_t *eeprom_buf)
{
  int mode_count_max=3, mode, i;

  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%-30s : 0x%04x\n", "fixture_header", (unsigned int)eeprom_buf->fixture_header);
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%-30s : 0x%04x\n", "fixture_version", (unsigned int)eeprom_buf->fixture_version);
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%-30s : ", "hash_board_sn");
  }

  for(i=0; i<0x14; i++)
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%c", eeprom_buf->hash_board_sn[i]);
    }
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "\n");
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%-30s : 0x%04x\n", "pcb_version", (unsigned int)eeprom_buf->pcb_version);
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%-30s : 0x%04x\n", "bom_version", (unsigned int)eeprom_buf->bom_version);
  }
  if(eeprom_buf->temp_sensor_type=='\x01')
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%-30s : %s\n", "temp_sensor_type", "TMP451");
    }
  }
  else
  {
    if(eeprom_buf->temp_sensor_type=='\x02')
    {
      if(log_level>LOG_INFO)
      {
        fprintf(stdout, "%-30s : %s\n", "temp_sensor_type", "TMP461");
      }
    }
    else
    {
      if(eeprom_buf->temp_sensor_type=='\x03')
      {
        if(log_level>LOG_INFO)
        {
          fprintf(stdout, "%-30s : %s\n", "temp_sensor_type", "TMP421");
        }
      }
      else
      {
        if(eeprom_buf->temp_sensor_type=='\x04')
        {
          if(log_level>LOG_INFO)
          {
            fprintf(stdout, "%-30s : %s\n", "temp_sensor_type", "TMP431");
          }
        }
        else
        {
          if(eeprom_buf->temp_sensor_type=='\x05')
          {
            if(log_level>LOG_INFO)
            {
              fprintf(stdout, "%-30s : %s\n", "temp_sensor_type", "ECT218");
            }
          }
          else
          {
            if(eeprom_buf->temp_sensor_type=='\x06')
            {
              if(log_level>LOG_INFO)
              {
                fprintf(stdout, "%-30s : %s\n", "temp_sensor_type", "TMP441B");
              }
            }
            else
            {
              if((eeprom_buf->temp_sensor_type=='\a') && (log_level>LOG_INFO))
              {
                fprintf(stdout, "%-30s : %s\n", "temp_sensor_type", "TMP411C");
              }
            }
          }
        }
      }
    }
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%-30s : 0x%02x\n", "product_id", eeprom_buf->product_id);
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%-30s : 0x%04x\n", "cgminer_header", eeprom_buf->cgminer_header);
  }
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "\n");
  }

  for(mode=0; mode<mode_count_max; mode++)
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "[Tuning Result Mode %d]:\n", mode);
    }
    for(i=0; i<48; i++)
    {
      if(log_level>LOG_INFO)
      {
        fprintf(stdout, "IC[%02d]:%03d ", i, (unsigned int)eeprom_buf->tuning_ret[mode].freq[i]*5);
      }
      if(((i+1U & 3)==0) && (log_level>LOG_INFO))
      {
        fprintf(stdout, "\n");
      }
    }
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s: %d\n", "voltage", (unsigned int)eeprom_buf->tuning_ret[mode].voltage);
    }
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s: %d\n", "hash_rate", eeprom_buf->tuning_ret[mode].hash_rate);
    }
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "\n");
    }
  }
}

void _eeprom_dump(eeprom_layout_t *eeprom_buf)
{
  _eeprom_dump_raw((unsigned int *)eeprom_buf, 256);
  if(log_level>LOG_INFO)
  {
      fprintf(stdout, "\n");
  }
  _eeprom_dump_fileds(eeprom_buf);
  return;
}

bool _eeprom_is_fixture_crc_pass(eeprom_layout_t *eeprom_buf)
{
    bool ret;
    u_int16_t crc;

    crc=CRC16((u_int8_t *)eeprom_buf, 30);

    if(crc==eeprom_buf->crc_fixture)
    {
        if(log_level>LOG_INFO)
        {
            fprintf(stdout, "%s:%s: Fixture EEPROM CRC is ok.\n", "eeprom.c", "_eeprom_is_fixture_crc_pass");
        }
        ret=1;
    }
    else
    {
        if(log_level>LOG_INFO)
        {
            fprintf(stderr, "%s:%s: Fixture EEPROM CRC check fail.\n", "eeprom.c", "_eeprom_is_fixture_crc_pass");
        }
        ret=0;
    }
    return(ret);
}

bool _eeprom_is_cgminer_crc_pass(eeprom_layout_t *eeprom_buf)
{
    bool ret;
    u_int16_t crc;

    crc=CRC16((u_int8_t *)&eeprom_buf->cgminer_header, 164);
    if(crc==eeprom_buf->crc_cgminer)
    {
        if(log_level>LOG_INFO)
        {
            fprintf(stderr, "%s:%s: Cgminer EEPROM CRC is ok.\n", "eeprom.c", "_eeprom_is_cgminer_crc_pass");
        }
        ret=1;
    }
    else
    {
        if(log_level>LOG_INFO)
        {
            fprintf(stderr, "%s:%s: Cgminer EEPROM CRC check fail.\n", "eeprom.c", "_eeprom_is_cgminer_crc_pass");
        }
        ret=0;
    }
    return(ret);
}

bool _eeprom_is_fixture_header_pass(eeprom_layout_t *eeprom_buf)
{
    bool ret;

    if(eeprom_buf->fixture_header==0x1397)
    {
        if(log_level>LOG_DEBUG)
        {
            fprintf(stderr, "%s:%s: Fixture EEPROM header is ok. fixture_header=0x%x\n", "eeprom.c", "_eeprom_is_fixture_header_pass", eeprom_buf->fixture_header);
        }
        ret=1;
    }
    else
    {
        if(log_level>LOG_DEBUG)
        {
            fprintf(stderr, "%s:%s: Fixture EEPROM header check fail. fixture_header=0x%x\n", "eeprom.c", "_eeprom_is_fixture_header_pass", eeprom_buf->fixture_header);
        }
        ret=0;
    }
    return(ret);
}

bool _eeprom_is_cgminer_header_pass(eeprom_layout_t *eeprom_buf)
{
    bool ret;

    if(eeprom_buf->cgminer_header==0x1397)
    {
        if(log_level>LOG_DEBUG)
        {
            fprintf(stderr, "%s:%s: Cgminer EEPROM header is OK. cgminer_header=0x%x\n", "eeprom.c", "_eeprom_is_cgminer_header_pass", eeprom_buf->cgminer_header);
        }
        ret=1;
    }
    else
    {
        if(log_level>LOG_DEBUG)
        {
          fprintf(stderr, "%s:%s: Cgminer EEPROM header check fail. cgminer_header=0x%x\n", "eeprom.c", "_eeprom_is_cgminer_header_pass", eeprom_buf->cgminer_header);
        }
        ret=0;
    }
    return(ret);
}

int _eeprom_load_one_chain(unsigned int chain, eeprom_layout_t *eeprom_buf)
{
  int ret;
  bool bVar1, bVar2;
  eeprom_layout_t eeprom_buf_tmp;

  memset(&eeprom_buf_tmp, 0, 256);
  ret=_eeprom_read_iic_bytes(chain, 0, 256, (unsigned char *)&eeprom_buf_tmp);

  if(ret==0)
  {
    bVar1=_eeprom_is_fixture_crc_pass(&eeprom_buf_tmp);
    bVar2=_eeprom_is_fixture_header_pass(&eeprom_buf_tmp);
    if(bVar1 && bVar2)
    {
      bVar1=_eeprom_is_cgminer_crc_pass(&eeprom_buf_tmp);
      bVar2=_eeprom_is_cgminer_header_pass(&eeprom_buf_tmp);
      if(bVar1 && bVar2)
      {
          memcpy(eeprom_buf, &eeprom_buf_tmp, 256);
          ret=0;
      }
      else
      {
        ret=-1;
      }
    }
    else
    {
      ret=-1;
    }
  }
  else
  {
    ret=-1;
  }
  return ret;
}

int _eeprom_flush_one_chain(unsigned int chain, eeprom_layout_t *eeprom_buf)
{
  int ret;
  eeprom_layout_t eeprom_buf_tmp;
  unsigned int addr_start=0x28;
  int crc_len;
  int flush_len=166; // ? 166 ?

  eeprom_buf->cgminer_header=0x1397;
  eeprom_buf->crc_cgminer=CRC16((u_int8_t *)&eeprom_buf->cgminer_header, 164);

  ret=_eeprom_write_iic_bytes(chain, addr_start, flush_len, (unsigned char *)&eeprom_buf->cgminer_header);
  if(ret==0)
  {
    memset(&eeprom_buf_tmp, 0xff, 0x100);
    ret=_eeprom_read_iic_bytes(chain, addr_start, flush_len, (unsigned char *)&eeprom_buf_tmp.cgminer_header);
    if(ret==0)
    {
      ret=memcmp(&eeprom_buf->cgminer_header, &eeprom_buf_tmp.cgminer_header, flush_len);
      if(ret)
      {
        if(log_level>LOG_INFO)
        {
          fprintf(stdout, "%s:%s: Read data is different of write data.\n", "eeprom.c", "_eeprom_flush_one_chain");
        }
        ret=-1;
      }
    }
    else
    {
      ret=-1;
    }
  }
  else
  {
    ret=-1;
  }
  return ret;
}

void eeprom_load()
{
  unsigned int chain;
  int loadRes;

  if(g_is_eeprom_loaded==false)
  {
    for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++)
    {
      if(dev->chain_exist[chain])
      {
        memset(&g_eeprom_buf[chain], 0xff, 256);
        memcpy(&g_eeprom_buf[chain].hash_board_sn, "deadbeefdeadbeefdead", 20);
        loadRes=_eeprom_load_one_chain(chain, &g_eeprom_buf[chain]);

        //===
            for(int r=0; r<256; r+=16)
            {
                for(int s=0; s<16; s++)
                {
                    fprintf(stdout, "%.2x ", ((unsigned char *)&g_eeprom_buf[chain])[r+s]);
                }
                fprintf(stdout, "\n");
            }
            fprintf(stdout, "\n");
        //===

        if(loadRes)
        {
          if(log_level>LOG_INFO)
          {
            fprintf(stderr, "%s:%s: Chain %u EEPROM data fail.\n", "eeprom.c", "eeprom_load", chain);
          }
        }
        else
        {
          if(log_level>LOG_INFO)
          {
            fprintf(stdout, "%s:%s: Chain %u EEPROM load success.\n", "eeprom.c", "eeprom_load", chain);
          }
        }
      }
    }
    g_is_eeprom_loaded=true;
  }
  else
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s:%s: EEPROM already loaded.\n", "eeprom.c", "eeprom_load");
    }
  }
  return;
}

int eeprom_flush()
{
  unsigned int uVar1, chain;
  int ret;

  if(g_is_eeprom_loaded)
  {
    ret=0;
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s:%d:%s: Flush EEPROM data now.\n", "eeprom.c", 0x18b, "eeprom_flush");
    }
    for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++)
    {
      if(dev->chain_exist[chain])
      {
        uVar1=_eeprom_flush_one_chain(chain, &g_eeprom_buf[chain]);
        ret=ret | uVar1;
      }
    }
  }
  else
  {
    ret=-1;
    if(log_level>LOG_INFO)
    {
      fprintf(stderr, "%s:%s: Can't flush EEPROM data without load.\n", "eeprom.c", "eeprom_flush");
    }
  }
  return(ret);
}

void eeprom_dump()
{
  unsigned int chain;

  for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++)
  {
    if(dev->chain_exist[chain])
    {
      if(log_level>LOG_INFO)
      {
        fprintf(stdout, "%s:%s: Dump chain %d EEPROM data now.\n", "eeprom.c", "eeprom_dump", chain);
      }
      _eeprom_dump(&g_eeprom_buf[chain]);
    }
  }
  return;
}

unsigned int eeprom_get_pcb_version(unsigned int chain)
{
  unsigned int ret, uVar1;

  if((dev->chain_exist[chain]==0) || (!g_is_eeprom_loaded))
  {
    if(log_level>LOG_INFO)
    {
        if(g_is_eeprom_loaded)
        {
          uVar1=0xbe74;
        }
        else
        {
          uVar1=0xbe7c;
        }
        fprintf(stderr, "%s:%s: Error: chain=%u, load_done=%s\n", "eeprom.c", "eeprom_get_pcb_version", chain, uVar1 | 0xa0000);
    }
    ret=0xffff;
  }
  else
  {
    ret=g_eeprom_buf[chain].pcb_version;
  }
  return(ret);
}

unsigned int eeprom_get_bom_version(unsigned int chain)
{
  unsigned int ret, uVar1;

  if((dev->chain_exist[chain]==0) || (!g_is_eeprom_loaded))
  {
    if(log_level>LOG_INFO)
    {
        if(g_is_eeprom_loaded)
        {
          uVar1=0xbe74;
        }
        else
        {
          uVar1=0xbe7c;
        }
        fprintf(stderr, "%s:%s: Error: chain=%u, load_done=%s\n", "eeprom.c", "eeprom_get_bom_version", chain, uVar1 | 0xa0000);
    }
    ret=0xffff;
  }
  else
  {
    ret=g_eeprom_buf[chain].bom_version;
  }
  return(ret);
}

int eeprom_get_freq_store_step()
{
  return 5;
}

int eeprom_get_freq(unsigned int chain, int mode, unsigned int *buf, int len)
{
  int ret, i;
  unsigned int uVar1;

  if((((dev->chain_exist[chain]==0) || (2<mode)) || (g_is_eeprom_loaded!=true)) || (len<0x30))
  {
    if(log_level>LOG_INFO)
    {
        if(g_is_eeprom_loaded)
        {
          uVar1=0xbe74;
        }
        else
        {
          uVar1=0xbe7c;
        }
        fprintf(stderr, "%s:%s: Error: chain=%d, mode=%d, load_done=%s, len=%d.\n", "eeprom.c", "eeprom_get_freq", chain, mode, uVar1 | 0xa0000, len);
    }
    ret=-1;
  }
  else
  {
    i=0;
    while(i<0x30)
    {
      buf[i]=g_eeprom_buf[chain].tuning_ret[mode].freq[i]*5;
      i=i+1;
    }
    ret=0;
  }
  return(ret);
}

int eeprom_get_voltage(unsigned int chain, int mode, int *voltage)
{
  int ret;
  unsigned int uVar1;

  if(((dev->chain_exist[chain]==0) || (2<mode)) || (g_is_eeprom_loaded!=true))
  {
    if(log_level>LOG_INFO)
    {
        if(g_is_eeprom_loaded)
        {
          uVar1=0xbe74;
        }
        else
        {
          uVar1=0xbe7c;
        }
        fprintf(stderr, "%s:%s: Error: chain=%u, mode=%d, load_done=%s.\n", "eeprom.c", "eeprom_get_voltage", chain, mode, uVar1 | 0xa0000);
    }
    ret=-1;
  }
  else
  {
    if(voltage!=(int *)0x0)
    {
      *voltage=(unsigned int)g_eeprom_buf[chain].tuning_ret[mode].voltage;
    }
    ret=0;
  }
  return(ret);
}

int eeprom_get_hash_rate(int chain, int mode, unsigned int *hash_rate)
{
  unsigned int uVar1;
  int iVar2;

  if(((dev->chain_exist[chain]==0) || (2<mode)) || (!g_is_eeprom_loaded))
  {
    if(log_level>LOG_INFO)
    {
        if(g_is_eeprom_loaded)
        {
          uVar1=0xbe74;
        }
        else
        {
          uVar1=0xbe7c;
        }
        fprintf(stderr, "%s:%s: Error: chain=%d, mode=%d, load_done=%s.\n", "eeprom.c", "eeprom_get_hash_rate", chain, mode, uVar1 | 0xa0000);
    }
    iVar2=-1;
  }
  else
  {
    if(*hash_rate!=0)
    {
      *hash_rate=g_eeprom_buf[chain].tuning_ret[mode].hash_rate;
    }
    iVar2=0;
  }
  return iVar2;
}

int eeprom_set_freq(unsigned int chain, int mode, int *buf, unsigned int len)
{
  int ret, i;
  unsigned int uVar1;

  if((((dev->chain_exist[chain]==0) || (!g_is_eeprom_loaded)) || (2<mode)) || (len<0x30))
  {
    if(log_level>LOG_INFO)
    {
        if(g_is_eeprom_loaded)
        {
          uVar1=0xbe74;
        }
        else
        {
          uVar1=0xbe7c;
        }
        fprintf(stderr, "%s:%s: Invalid parameter: chain=%u, load_done=%s, mode=%d, len=%u.\n", "eeprom.c", "eeprom_set_freq", chain, uVar1 | 0xa0000, mode, len);
    }
    ret=-1;
  }
  else
  {
    for(i=0; i<0x30; i++)
    {
      if((buf[i]%5!=0) && (log_level>LOG_INFO))
      {
          fprintf(stdout, "%s:%s: Note: buf[%d]=%d, which is not not multiple of %d\n", "eeprom.c", "eeprom_set_freq", i, buf[i], 5);
      }
      g_eeprom_buf[chain].tuning_ret[mode].freq[i]=(unsigned int)(buf[i] / 5);
    }
    ret=0;
  }
  return(ret);
}

int eeprom_set_voltage(int chain, int mode, int voltage)
{
  unsigned int uVar1;
  FILE *stdout;
  int iVar2;
  FILE *pFile;

  if(((dev->chain_exist[chain]==0) || (g_is_eeprom_loaded!=true)
      ) || (2<mode)) {
    if(log_level>LOG_INFO) {


      if(stdout) {
        if(g_is_eeprom_loaded==false) {
          uVar1=0xbe7c;
        }
        else {
          uVar1=0xbe74;
        }
        fprintf(stdout, "%s:%s: Invalid parameter: chain=%d, load_done=%s, mode=%d.\n", "eeprom.c", "eeprom_set_voltage", chain, uVar1 | 0xa0000, mode);
      }

    }
    iVar2=-1;
  }
  else {
    g_eeprom_buf[chain].tuning_ret[mode].voltage=voltage;
    iVar2=0;
  }
  return iVar2;
}

int eeprom_set_hash_rate(int chain, int mode, int hash_rate)
{
  int ret;
  unsigned int uVar1;

  if(((dev->chain_exist[chain]==0) || (g_is_eeprom_loaded!=true)) || (2<mode))
  {
    if(log_level>LOG_INFO)
    {
        if(g_is_eeprom_loaded)
        {
          uVar1=0xbe74;
        }
        else
        {
          uVar1=0xbe7c;
        }
        fprintf(stderr, "%s:%s: Invalid parameter: chain=%d, load_done=%s, mode=%d.\n", "eeprom.c", "eeprom_set_hash_rate", chain, uVar1 | 0xa0000, mode);
    }
    ret=-1;
  }
  else {
    g_eeprom_buf[chain].tuning_ret[mode].hash_rate=hash_rate;
    ret=0;
  }
  return(ret);
}
