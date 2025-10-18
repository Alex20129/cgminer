#include <sys/mman.h>
#include <memory.h>
#include <fcntl.h>
#include "driver-btm-soc.hpp"
#include "logging.hpp"

all_parameters *dev;

int fd_fpga_dev;                // axi fpga device
int fd_fpga_mem;                // fpga memory

unsigned int *fpga_mem_addr=NULL;             // fpga memory address
unsigned int *axi_fpga_addr=NULL;             // axi address

unsigned int All_Chain, *job_start_address_1, *job_start_address_2;
int scan_freq_average[16], total_freq;

bool opt_use_pll_list=true;

SCAN_FREQ_RESULT scan_result[16];
pthread_mutex_t *iic_mutex;

int axi_init()
{
  fprintf(stdout, "axi_init()\n");
  unsigned int data;
  int ret=0;

  fd_fpga_dev=open("/dev/axi_fpga_dev", 2);
  if(fd_fpga_dev<0)
  {
    if(log_level>LOG_DEBUG)
    {
      fprintf(stderr, "%s:%s: /dev/axi_fpga_dev open failed. fd=%d.\n", "driver-btm-soc.cpp", "axi_init", fd_fpga_dev);
    }
    perror("open");
    ret=-1;
  }
  else
  {
    axi_fpga_addr=(unsigned int *)mmap((void *)0x0, 0x160, 3, 1, fd_fpga_dev, 0);
    if(axi_fpga_addr==0)
    {
      if(log_level>LOG_DEBUG)
      {
        fprintf(stderr, "%s:%s: mmap axi_fpga_addr failed. axi_fpga_addr=%p.\n", "driver-btm-soc.cpp", "axi_init", axi_fpga_addr);
      }
      ret=-1;
    }
    else
    {
      if(log_level>LOG_DEBUG)
      {
        fprintf(stdout, "%s:%s: mmap axi_fpga_addr=%p.\n", "driver-btm-soc.cpp", "axi_init", axi_fpga_addr);
      }
      data=*axi_fpga_addr;
      if((data & 0xffff)!=0xc501)
      {
          if(log_level>LOG_DEBUG)
          {
            fprintf(stderr, "%s:%s: data=0x%x, and it\'s not equal to HARDWARE_VERSION_VALUE : 0x%x.\n", "driver-btm-soc.cpp", "axi_init", data, 0xc501);
          }
      }
      if(log_level>LOG_DEBUG)
      {
        fprintf(stdout, "%s:%s: axi_fpga_addr data=0x%x.\n", "driver-btm-soc.cpp", "axi_init", data);
      }
      fd_fpga_mem=open("/dev/fpga_mem", 2);
      if(fd_fpga_mem<0)
      {
        if(log_level>LOG_DEBUG)
        {
          fprintf(stderr, "%s:%s: /dev/fpga_mem open failed. fd_fpga_mem=%d.\n", "driver-btm-soc.cpp", "axi_init", fd_fpga_mem);
        }
        perror("open");
        ret=-1;
      }
      else
      {
        fpga_mem_addr=(unsigned int *)mmap((void *)0x0, 0x1000000, 3, 1, fd_fpga_mem, 0);
        if(!fpga_mem_addr)
        {
          if(log_level>LOG_DEBUG)
          {
            fprintf(stderr, "%s:%s: mmap fpga_mem_addr failed. fpga_mem_addr=%p.\n", "driver-btm-soc.cpp", "axi_init", fpga_mem_addr);
          }
          ret=-1;
        }
        else
        {
          if(log_level>LOG_DEBUG)
          {
            fprintf(stdout, "%s:%s: mmap fpga_mem_addr=%p.\n", "driver-btm-soc.cpp", "axi_init", fpga_mem_addr);
          }
          job_start_address_1=fpga_mem_addr+0x80000;
          job_start_address_2=fpga_mem_addr+0x84000;
          /*
          nonce2_jobid_address=fpga_mem_addr;
          if(log_level>LOG_DEBUG)
          {
            fprintf(__stream, "%s:%s: job_start_address_1=%p.\n", "driver-btm-soc.cpp", "axi_init", job_start_address_1);
          }
          if(log_level>LOG_DEBUG)
          {
            fprintf(__stream, "%s:%s: job_start_address_2=%p.\n", "driver-btm-soc.cpp", "axi_init", job_start_address_2);
          }
          set_nonce2_and_job_id_store_address(PHY_MEM_NONCE2_JOBID_ADDRESS);
          set_job_start_address(PHY_MEM_NONCE2_JOBID_ADDRESS+0x200000);
          */
          dev=new(all_parameters);
          if(dev==0)
          {
            ret=-1;
            if(log_level>LOG_DEBUG)
            {
              fprintf(stderr, "%s:%s: malloc for dev failed.\n", "driver-btm-soc.cpp", "axi_init");
            }
          }
          else
          {
            dev->current_job_start_address=job_start_address_1;
            if(log_level>LOG_DEBUG)
            {
              fprintf(stdout, "%s:%s: malloc for dev success.\n", "driver-btm-soc.cpp", "axi_init");
            }
          }
        }
      }
    }
  }
  return ret;
}

unsigned int get_hash_on_plug()
{
  unsigned int ret;

  ret=axi_fpga_addr[2];
  if(log_level>LOG_DEBUG)
  {
    fprintf(stdout, "%s:%s: HASH_ON_PLUG is 0x%x.\n", "zynq.cpp", "get_hash_on_plug", ret);
  }
  return ret;
}

void check_chains()
{
  int iVar1, i;

  dev->chain_num=0;
  iVar1=get_hash_on_plug();
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%s:%s: get_hash_on_plug is 0x%x.\n", "driver-btm-soc.cpp", "check_chain", iVar1);
  }
  if(iVar1<0)
  {
    if(log_level>LOG_DEBUG)
    {
      fprintf(stderr, "%s:%s: get_hash_on_plug functions error.\n", "driver-btm-soc.cpp", "check_chain");
    }
  }
  else
  {
    for(i=0; i<ACTIVE_CHAINS_NUM; i++)
    {
      if((iVar1 >> (i & 0xffU) & 1U)==0)
      {
        dev->chain_exist[i]=0;
      }
      else
      {
        dev->chain_exist[i]=1;
        if(log_level>LOG_INFO)
        {
          fprintf(stdout, "%s:%s: chain[%d] is exist.\n", "driver-btm-soc.cpp", "check_chain", i);
        }
        dev->chain_num++;
      }
    }
  }
}

void get_plldata_from_index(int index, unsigned int *vil_pll)
{
  size_t sVar2;
  char plldivider1[32];

  memset(plldivider1, 0, 0x20);
  sprintf(plldivider1, "%08x", freq_pll_1391[index].vilpll);
  sVar2=strlen(plldivider1);
  hex2bin((unsigned char *)vil_pll, plldivider1, sVar2 >> 1);
}

void send_set_config_command(int chain, unsigned int mode, unsigned int chip_addr, unsigned int reg_addr, unsigned int reg_data)
{
  unsigned char bVar1;
  unsigned int uVar2;
  unsigned int bc_cmd[3];
  set_config_t set_cfg;
  unsigned int value;
  unsigned int origin;

  if(dev->chain_exist[chain]==0)
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s:%s: Chain %d not exist.\n", "chip1391.cpp", "send_set_config_command", chain);
    }
  }
  else
  {
    memset(&set_cfg, 0, 9);
    ((unsigned char *)&set_cfg)[0]=(mode!=0) << 4 | 0x41;
    ((unsigned char *)&set_cfg)[1]=0x09;
    ((unsigned char *)&set_cfg)[2]=chip_addr;
    ((unsigned char *)&set_cfg)[3]=reg_addr;
    ((unsigned char *)&set_cfg)[4]=(unsigned char)(reg_data >> 24);
    ((unsigned char *)&set_cfg)[5]=(unsigned char)(reg_data >> 16);
    ((unsigned char *)&set_cfg)[6]=(unsigned char)(reg_data >> 8);
    ((unsigned char *)&set_cfg)[7]=(unsigned char)(reg_data);
    bVar1=CRC5((unsigned char *)&set_cfg, 8*8);
    ((unsigned char *)&set_cfg)[8]=((unsigned char *)&set_cfg)[8] & 0xe0 | bVar1 & 0x1f;
    memset(bc_cmd, 0, 0xc);
    bc_cmd[0]=(uint)((unsigned char *)&set_cfg)[0] << 0x18 | ((unsigned char *)&set_cfg)[1] << 0x10 | ((unsigned char *)&set_cfg)[2] << 8 | ((unsigned char *)&set_cfg)[3];
    bc_cmd[1]=(uint)((unsigned char *)&set_cfg)[4] << 0x18 | ((unsigned char *)&set_cfg)[5] << 0x10 | ((unsigned char *)&set_cfg)[6] << 8 | ((unsigned char *)&set_cfg)[7];
    bc_cmd[2]=(uint)((unsigned char *)&set_cfg)[8] << 0x18;
    set_BC_command_buffer(bc_cmd);
    uVar2=get_BC_write_command();
    set_BC_write_command(chain << 0x10 | uVar2 & 0xfff0ffff | 0x80800000);
  }
}

void set_config_BM1391_t(unsigned char which_chain, unsigned char chip_addr, unsigned char mode, unsigned char reg, unsigned int reg_data)
{
  send_set_config_command(which_chain, mode, chip_addr, reg, reg_data);
  return;
}

int get_index_from_high_pll(unsigned int freq)
{
  int i;

  i=0;
  while((i<0x21 && (freq_high_pll_1391[i].freq!=freq)) &&
         (freq_high_pll_1391[i].freq <= freq || (freq <= freq_high_pll_1391[i+-1].freq)))
  {
    i++;
  }
  if(i==0x21)
  {
    if(log_level>LOG_WARNING)
    {
      fprintf(stderr, "%s:%s: high freq index set error, return default pll index.\n", "driver-btm-soc.cpp", "get_index_from_high_pll");
    }
    i=get_index_from_high_pll(200);
  }
  return i;
}

int get_index_from_pll_B1391(unsigned int pll)
{
  int i;

  for(i=0; i<179 && (pll!=freq_pll_1391[i].freq); i++);

  if(i<179)
  {
      return i;
  }

  return 90;
}

void change_high_pll_test(unsigned int chain, float freq, int index)
{
  unsigned int vil_pll, pll, divider;
  float pll_freq;
  int vil_pll_t, pll_index;

  divider=freq_high_pll_1391[index].divider;
  pll=freq_high_pll_1391[index].pll_out;
  if(opt_use_pll_list==false)
  {
    //get_pllparam_divider(freq, &vil_pll_t, &divider, &pll_freq);
    //vil_pll=__bswap_32(vil_pll_t);
  }
  else
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s:%s: set freq %d, pll_out %d.\n", "driver-btm-soc.cpp", "change_high_pll_test", freq_high_pll_1391[index].freq, pll);
    }
    pll_index=get_index_from_pll_B1391(pll);
    get_plldata_from_index(pll_index, &vil_pll);
  }
  vil_pll=vil_pll | 0x40;
  set_config_BM1391_t(chain, '\0', '\x01', 'p', (uint)divider-1 | 0xf0f0f00);
  set_config_BM1391_t(chain, '\0', '\x01', '\b', vil_pll << 0x18 | (vil_pll >> 8 & 0xff) << 0x10 | (vil_pll >> 0x10 & 0xff) << 8 | vil_pll >> 0x18);
  set_config_BM1391_t(chain, '\0', '\x01', 'p', (uint)divider-1 | 0xf0f0f00);
  set_config_BM1391_t(chain, '\0', '\x01', '\b', vil_pll << 0x18 | (vil_pll >> 8 & 0xff) << 0x10 | (vil_pll >> 0x10 & 0xff) << 8 | vil_pll >> 0x18);
  return;
}
/*
void change_high_pll_by_aisc(unsigned int chain, unsigned int asic, float freq, int index)
{
  unsigned char which_chain;
  unsigned char chip_addr;
  unsigned int divider;
  float pll_freq;
  int vil_pll_t;
  unsigned int vil_pll;
  int pll_index;
  int pll;

  divider=freq_high_pll_1391[index].divider;
  pll=freq_high_pll_1391[index].pll_out;
  chip_addr=(char)asic *'\x05';
  if(opt_use_pll_list==false)
  {
    //get_pllparam_divider(freq, &vil_pll_t, &divider, &pll_freq);
    //vil_pll=__bswap_32(vil_pll_t);
  }
  else
  {
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s:%s: set freq %d, pll_out %d.\n", "driver-btm-soc.cpp", "change_high_pll_by_aisc", freq_high_pll_1391[index].freq, pll);
    }
    pll_index=get_index_from_pll_B1391(pll);
    get_plldata_from_index(pll_index, vil_pll);
  }
  vil_pll=vil_pll | 0x40;
  which_chain=(uchar)chain;
  set_config_BM1391_t(which_chain, chip_addr, '\0', 'p', (uint)divider-1 | 0xf0f0f00);
  set_config_BM1391_t(which_chain, chip_addr, '\0', '\b', vil_pll << 0x18 | (vil_pll >> 8 & 0xff) << 0x10 | (vil_pll >> 0x10 & 0xff) << 8 | vil_pll >> 0x18);
  set_config_BM1391_t(which_chain, chip_addr, '\0', 'p', (uint)divider-1 | 0xf0f0f00);
  set_config_BM1391_t(which_chain, chip_addr, '\0', '\b', vil_pll << 0x18 | (vil_pll >> 8 & 0xff) << 0x10 | (vil_pll >> 0x10 & 0xff) << 8 | vil_pll >> 0x18);
  return;
}

void set_pll(float pll_value)
{
  int local_pll_index;
  unsigned int chain;

  local_pll_index=0;
  if(opt_use_pll_list)
  {
    local_pll_index=get_index_from_high_pll((int)pll_value);
  }

  for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++;)
  {
    if(dev->chain_exist[chain])
    {
      change_high_pll_test(chain, pll_value, local_pll_index);
    }
  }
  return;
}

void get_statistics_of_asic_freq(unsigned int *asic_freq, unsigned int *min, unsigned int *max, unsigned int *total)
{
  unsigned int _total;
  unsigned int _max;
  unsigned int _min;
  unsigned int asic;

  _min=0xffffff;
  _max=0;
  _total=0;
  asic=0;
  while(asic<0x30)
  {
    if(_max<asic_freq[asic])
    {
      _max=asic_freq[asic];
    }
    if(_min > asic_freq[asic])
    {
      _min=asic_freq[asic];
    }
    _total+=asic_freq[asic];
    asic++;
  }
  *min=_min;
  *max=_max;
  *total=_total;
  return;
}

void increase_asic_diff_freq_slowly_one_chain(int *asic_diff_freq, int start_freq, int freq_step, int chain)
{
  int asic_00;
  int this_diff;
  int domain_col;
  int domain;
  int step;
  int steps;
  int asic_freq;
  int max;
  int asic;

  max=0;
  asic=0;
  while(asic<0x30)
  {
    if(max<asic_diff_freq[asic])
    {
      max=asic_diff_freq[asic];
    }
    asic=asic+1;
  }
  steps=max/freq_step;
  if(steps * freq_step<max)
  {
    steps=steps+1;
  }
  step=0;
  while(step<steps)
  {
    this_diff=freq_step *(step+1);
    domain_col=0;
    while(domain_col<4)
    {
      domain=0;
      while(domain<0xc)
      {
        asic_00=get_physical_chip_no(domain, domain_col);
        if((this_diff <= asic_diff_freq[asic_00]) || (this_diff-asic_diff_freq[asic_00]<(uint)freq_step))
        {
          asic_freq=this_diff;
          if(asic_diff_freq[asic_00] <= this_diff)
          {
            asic_freq=asic_diff_freq[asic_00];
          }
          asic_freq=start_freq+asic_freq;`
          change_high_pll_by_aisc(chain, asic_00, (float)(unsigned long long int)asic_freq, 0);
        }
        domain=domain+1;
      }
      usleep(100000);
      domain_col=domain_col+1;
    }
    step=step+1;
  }
  return;
}
*/
void increase_freq_slowly(float init_freq, float final_freq, float freq_step, unsigned int chain)
{
  char in_NG;
  bool in_ZR;
  char in_OV;
  float freq_tmp;
  int steps;
  int i;

  steps=(int)((final_freq-init_freq) / freq_step);
  if(!in_ZR && in_NG==in_OV)
  {
    steps=steps+1;
  }

  for(i=0; i<steps; i++)
  {
    freq_tmp=init_freq+freq_step;
    if(steps<i)
    {
      freq_tmp=final_freq;
    }
    if(log_level>LOG_INFO)
    {
      fprintf(stdout, "%s:%s: Increase frequency to %.2fM.\n", "driver-btm-soc.cpp", "increase_freq_slowly", final_freq);
    }
    change_high_pll_test(chain, freq_tmp, 0);
    usleep(500000);
  }
  return;
}
/*
unsigned int increase_freq_by_eeprom_slowly(int init_freq, int freq_step)
{
  int diff_freq[48];
  unsigned int total;
  unsigned int max;
  unsigned int min;
  int asic;
  int chain;
  int total_all;

  total_all=0;

  for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++)
  {
    if(dev->chain_exist[chain])
    {
      min=0xffffff;
      max=0;
      total=0;
      get_statistics_of_asic_freq(scan_result[chain].freq_eeprom, &min, &max, &total);
      asic=0;
      while(asic<0x30)
      {
        diff_freq[asic]=*(int *)(&scan_result[0].freq_step+(chain *0x6f+asic+2) *4)-min;
        asic=asic+1;
      }
      if(log_level>LOG_INFO)
      {
        fprintf(stdout, "%s:%s: Increase eeprom frequency slowly for chain %d. to %u.\n", "driver-btm-soc.cpp", "increase_freq_by_eeprom_slowly", chain, min);
      }
      increase_freq_slowly((float)init_freq, (float)min, (float)freq_step, (unsigned int)chain);
      if(min!=max)
      {
        increase_asic_diff_freq_slowly_one_chain(diff_freq, min, freq_step, chain);
      }
      scan_freq_average[chain]=total / 0x30;
      total_all=total_all+total;
    }
  }
  total_freq=total_all;
  return max;
}

int get_eeprom_total_hash_rate()
{
  unsigned int hashrate_tmp, total_rate;
  int work_mode;
  int chain;

  total_rate=0;
  hashrate_tmp=0;

  for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++)
  {
    if(dev->chain_exist[chain])
    {
      eeprom_get_hash_rate(chain, work_mode, &hashrate_tmp);
      total_rate=total_rate+hashrate_tmp;
      usleep(100000);
    }
  }
  if(log_level>LOG_INFO)
  {
    fprintf(__stream, "%s:%s: total rate=%d.\n", "driver-btm-soc.cpp", "get_eeprom_total_hash_rate", total_rate);
  }
  return total_rate;
}
*/
void set_working_voltage_by_eeprom()
{
  double defaultVoltageVal;
  int voltage;
  double avg;
  int sum=0;
  int work_mode;
  int chain_num=0;
  int chain;

  for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++)
  {
    if(dev->chain_exist[chain])
    {
      eeprom_get_voltage(chain, work_mode, &voltage);
      sum+=voltage;
      chain_num+=1;
      if(log_level>LOG_INFO)
      {
        fprintf(stdout, "%s:%s: eeprom voltage[%d]=%d.\n", "driver-btm-soc.cpp", "set_working_voltage_by_eeprom", chain, voltage);
      }
    }
  }
  if(chain_num!=0)
  {
    avg=sum;
    avg/=chain_num;
    if(sum==(chain_num*avg))
    {
      set_working_voltage(avg);
    }
    else
    {
      if(log_level>LOG_INFO)
      {
        defaultVoltageVal=get_working_voltage();
        fprintf(stdout, "%s:%s: Voltage are different. Will use default setting (voltage=%lf).\n", "driver-btm-soc.cpp", "set_working_voltage_by_eeprom", defaultVoltageVal);
      }
    }
  }
}

void _get_freq_from_eeprom()
{
  int iVar2;
  unsigned int *buf_00;
  int max_freq;
  int *buf;
  bool is_eeprom_read_success;
  int work_mode;
  bool is_freq_valid;
  unsigned int asic;
  unsigned int chain;

  /* // ! need to fix it later ! //

  _Var1=freq_tuning_get_max_freq(&max_freq);
  if(_Var1!=true)
  {
    power_off_hash_board(All_Chain);
    stop_mining("Get max freq failed!\n");
  }

  */ // ! need to fix it later ! //

  for(chain=0; chain<ACTIVE_CHAINS_NUM; chain++)
  {
    if(dev->chain_exist[chain])
    {
      buf_00=scan_result[chain].freq_eeprom;
      memset(buf_00, 0, 0x1b0);
      iVar2=eeprom_get_freq(chain, work_mode, buf_00, 0x30);
      if(iVar2==0)
      {
        is_freq_valid=true;
        for(asic=0; asic<0x30; asic++)
        {
          if((unsigned int)max_freq<buf_00[asic])
          {
            if(log_level>LOG_INFO)
            {
              fprintf(stderr, "%s:%s: Freq(%d) > Max_freq(%d), invalid!.\n", "driver-btm-soc.cpp", "_get_freq_from_eeprom", buf_00[asic], max_freq);
            }
            is_freq_valid=false;
            break;
          }
        }
      }
      /*
      if((iVar2!=0) || (is_freq_valid!=true))
      {
        power_off_hash_board(All_Chain);
        stop_mining("Get frequency from eeprom failed!\n");
      }
      */
    }
  }
}

void set_BC_command_buffer(unsigned int *value)
{
  unsigned int buf[4];
  buf[0]=0;
  buf[1]=0;
  buf[2]=0;
  buf[3]=0;
  axi_fpga_addr[0x31]=*value;
  axi_fpga_addr[0x32]=value[1];
  axi_fpga_addr[0x33]=value[2];
  if(log_level>LOG_DEBUG)
  {
    fprintf(stdout, "%s:%s: set BC_COMMAND_BUFFER value[0]: 0x%x, value[1]: 0x%x, value[2]: 0x%x.\n", "zynq.cpp", "set_BC_command_buffer", *value, value[1], value[2]);
  }
  get_BC_command_buffer(buf);
}

void set_BC_write_command(unsigned int value)
{
  int iVar1;
  int wait_count;

  axi_fpga_addr[48]=value;
  if((int)value<0)
  {
    for(wait_count=0; wait_count<3000; wait_count++)
    {
      iVar1=get_BC_write_command();
      if(-1<iVar1)
      {
        return;
      }
      usleep(1000);
    }
    if(log_level>LOG_INFO)
    {
      fprintf(stderr, "%s:%s: Error: set_BC_write_command wait buffer ready timeout!.\n", "zynq.cpp", "set_BC_write_command");
    }
  }
  else
  {
    get_BC_write_command();
  }
}

unsigned int get_BC_command_buffer(unsigned int *buf)
{
  unsigned int ret;

  *buf=axi_fpga_addr[0x31];
  buf[1]=axi_fpga_addr[0x32];
  ret=axi_fpga_addr[0x33];
  buf[2]=ret;
  if(log_level>LOG_DEBUG)
  {
    fprintf(stdout, "%s:%s: BC_COMMAND_BUFFER buf[0]: 0x%x, buf[1]: 0x%x, buf[2]: 0x%x.\n", "zynq.cpp", "get_BC_command_buffer", *buf, buf[1], buf[2]);
  }
  return ret;
}

unsigned int get_BC_write_command()
{
  unsigned int ret;

  ret=axi_fpga_addr[0x30];
  if(log_level>LOG_DEBUG)
  {
    fprintf(stdout, "%s:%s: BC_WRITE_COMMAND is 0x%x.\n", "zynq.cpp", "get_BC_write_command", ret);
  }
  return ret;
}
