#include "stdio.h"
#include "power.hpp"
#include "logging.hpp"
#include "driver-btm-soc.hpp"

power_info_t power_info;

u_int8_t power_iic_addr=16;
u_int8_t power_iic_no=1;

void set_working_voltage(double new_voltage)
{
  if(log_level>LOG_INFO)
  {
      fprintf(stdout, "%s:%d:%s: working_voltage=%lf\n", "power.c", 213, "set_working_voltage", new_voltage);
  }
  power_info.working_voltage=new_voltage;
  return;
}

void power_init()
{
    if(log_level>LOG_INFO)
    {
        fprintf(stdout, "%s:%d:%s: power init ...\n", "power.c", 0xc4, "power_init");
    }
    power_info.is_voltage_stable=false;
    power_info.current_voltage=0.0;
    power_info.working_voltage=18.3;
    power_info.higher_voltage=0.0;
    power_info.highest_voltage=20;
    power_info.current_iic_data=0;
    power_info.power_protocol_type=2; //power type APW9 3600W
}

double get_working_voltage()
{
    return power_info.working_voltage;
}

double get_current_voltage()
{
    return power_info.current_voltage;
}

unsigned int power_set_voltage(unsigned int data)
{
  unsigned int ret;
  pthread_mutex_lock((pthread_mutex_t *)iic_mutex);
  usleep(100000);
  ret=znyq_set_iic(power_iic_addr, power_iic_no, false, true, 2, data);
  pthread_mutex_unlock((pthread_mutex_t *)iic_mutex);
  return ret;
}

unsigned int get_power_iic_value_from_voltage(double voltage)
{
  unsigned int iic_index;

  voltage=765.411764-(voltage*35.833333);

  if(iic_index>255.0)
  {
    iic_index=255.0;
  }

  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%s:%d:%s: iic_index for voltage[%d]=%lf\n", "power.c", 0x20a, "get_power_iic_value_from_voltage", iic_index, voltage);
  }

  return(iic_index);
}

double get_power_voltage_from_iic_value(unsigned int iic_index)
{
  double voltage;

  voltage=(765.411764-(double)iic_index)/35.833333;

  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%s:%d:%s: iic_index for voltage[%d]=%lf\n", "power.c", 0x20a, "get_power_voltage_from_iic_value", iic_index, voltage);
  }
  return voltage;
}

bool set_iic_power_by_voltage(double target_vol, power_info_t *power)
{
  unsigned int iic_vol_data;

  power->is_voltage_stable=true;
  iic_vol_data=get_power_iic_value_from_voltage(target_vol);
  power_set_voltage(iic_vol_data);
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%s:%d:%s: now setting voltage to : %lf \n", "power.c", 0x21c, "set_iic_power_by_voltage", target_vol);
  }
  usleep(300000);
  power->current_voltage=target_vol;
  power->current_iic_data=iic_vol_data;
  return true;
}

bool set_iic_power_by_iic_data(unsigned int target_data, power_info_t *power)
{
  double target_vol;

  power->is_voltage_stable=true;
  target_vol=get_power_voltage_from_iic_value(target_data);
  power_set_voltage(target_data);
  if(log_level>LOG_INFO)
  {
    fprintf(stdout, "%s:%d:%s: now setting voltage to : %f \n", "power.c", 0x231, "set_iic_power_by_iic_data", target_vol);
  }
  usleep(300000);
  power->current_voltage=target_vol;
  power->current_iic_data=target_data;
  return true;
}
