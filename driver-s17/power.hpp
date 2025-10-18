#ifndef POWER_HPP
#define POWER_HPP

struct power_info_t
{
    bool is_voltage_stable;
    char field_0x1;
    char field_0x2;
    char field_0x3;
    char field_0x4;
    char field_0x5;
    char field_0x6;
    char field_0x7;
    double current_voltage;
    double highest_voltage;
    double working_voltage;
    double higher_voltage;
    unsigned int current_iic_data;
    unsigned int power_protocol_type;
    char field_0x2a;
    char field_0x2b;
    char field_0x2c;
    char field_0x2d;
    char field_0x2e;
    char field_0x2f;
};

extern power_info_t power_info;

void set_working_voltage(double new_voltage);
void power_init();
double get_working_voltage();
double get_current_voltage();
unsigned int power_set_voltage(unsigned int data);
unsigned int get_power_iic_value_from_voltage(double voltage);
double get_power_voltage_from_iic_value(unsigned int iic_index);
bool set_iic_power_by_voltage(double target_vol, power_info_t *power);
bool set_iic_power_by_iic_data(unsigned int target_data, power_info_t *power);

#endif //POWER_HPP
