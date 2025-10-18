#include "autotuner.h"
#include "driver-s21.h"

#include <stdio.h>

autotuner_t *g_autotuner=NULL;

void init_fan_pwm()
{
	char clbuf[64];
	sprintf(clbuf, "echo 1 > " SYSTEM_PWM_DEVICE_0 "/enable");
	system(clbuf);
	sprintf(clbuf, "echo %u > " SYSTEM_PWM_DEVICE_0 "/period", FAN_PWM_PERIOD_NS);
	system(clbuf);
	sprintf(clbuf, "echo %u > " SYSTEM_PWM_DEVICE_0 "/duty_cycle", FAN_PWM_PERIOD_NS/2);
	system(clbuf);
	sprintf(clbuf, "echo 1 > " SYSTEM_PWM_DEVICE_1 "/enable");
	system(clbuf);
	sprintf(clbuf, "echo %u > " SYSTEM_PWM_DEVICE_1 "/period", FAN_PWM_PERIOD_NS);
	system(clbuf);
	sprintf(clbuf, "echo %u > " SYSTEM_PWM_DEVICE_1 "/duty_cycle", FAN_PWM_PERIOD_NS/2);
	system(clbuf);
}

void apply_fan_pwm_setting(int32_t pwm_value)
{
	int32_t duty_cycle;
	char clbuf[64];
	if(pwm_value<FAN_PWM_MIN_VALUE)
	{
		pwm_value=FAN_PWM_MIN_VALUE;
	}
	else if(pwm_value>FAN_PWM_MAX_VALUE)
	{
		pwm_value=FAN_PWM_MAX_VALUE;
	}
	if(g_autotuner->fan_pwm_value==pwm_value)
	{
		return;
	}
	g_autotuner->fan_pwm_value=pwm_value;
	duty_cycle=FAN_PWM_PERIOD_NS*pwm_value/FAN_PWM_MAX_VALUE;
	sprintf(clbuf, "echo %u > " SYSTEM_PWM_DEVICE_0 "/duty_cycle", duty_cycle);
	system(clbuf);
	sprintf(clbuf, "echo %u > " SYSTEM_PWM_DEVICE_1 "/duty_cycle", duty_cycle);
	system(clbuf);
}

void adjust_fan_pwm_according_to_temperature(float temperature)
{
	float fan_pwm_new_value=0.0, e;
	if(g_device->hash_boards_active<1)
	{
		return;
	}
	if(temperature>S21_WORKING_TEMPERATURE_MAX)
	{
		fprintf(stderr, "Overheat! Temperature is %.2fC\n", temperature);
		apply_fan_pwm_setting(FAN_PWM_MAX_VALUE);
		return;
	}
	if(g_autotuner->fan_pidc_active)
	{
		e=temperature-opt_target_temperature;
		g_autotuner->fan_control_d=e-g_autotuner->fan_control_p;
		g_autotuner->fan_control_p=e;
		g_autotuner->fan_control_i+=e;
	}
	else
	{
		if((int)temperature<(int)opt_target_temperature)
		{
			g_autotuner->fan_control_i-=0.5F;
		}
		else if((int)temperature>(int)opt_target_temperature)
		{
			g_autotuner->fan_control_i+=1.0F;
		}
		else
		{
			g_autotuner->fan_pidc_active=1;
		}
	}
	fan_pwm_new_value=g_autotuner->fan_control_p*FAN_CONTROL_PK;
	fan_pwm_new_value+=g_autotuner->fan_control_i*FAN_CONTROL_IK;
	fan_pwm_new_value+=g_autotuner->fan_control_d*FAN_CONTROL_DK;
	apply_fan_pwm_setting(fan_pwm_new_value);
}

void autotuner_init()
{
	init_fan_pwm();
	if(!g_autotuner)
	{
		g_autotuner=calloc(1, sizeof(autotuner_t));
	}
	if(!g_autotuner)
	{
		fprintf(stderr, "%s: Error. 'autotuner' calloc failed.\n", __FUNCTION__);
		return;
	}
	g_autotuner->mode=opt_autotuner_mode;
	if(opt_autotuner_mode)
	{
		g_autotuner->state=AUTOTUNER_PREPARE;
		fprintf(stdout, "%s: autotuner enabled\n", __FUNCTION__);
	}
	else
	{
		g_autotuner->state=AUTOTUNER_DONE;
		fprintf(stdout, "%s: autotuner disabled\n", __FUNCTION__);
	}
	fprintf(stdout, "%s: autotuner_mode=%i\n", __FUNCTION__, opt_autotuner_mode);
	if(opt_target_temperature<S21_WORKING_TEMPERATURE_MIN)
	{
		opt_target_temperature=S21_WORKING_TEMPERATURE_MIN;
	}
	else if(opt_target_temperature>S21_WORKING_TEMPERATURE_MAX)
	{
		opt_target_temperature=S21_WORKING_TEMPERATURE_MAX;
	}
	fprintf(stdout, "%s: target_temperature=%0.2f\n", __FUNCTION__, opt_target_temperature);
	if(opt_psu_voltage<APW17_MIN_VOLTAGE)
	{
		opt_psu_voltage=APW17_MIN_VOLTAGE;
	}
	else if(opt_psu_voltage>APW17_MAX_VOLTAGE)
	{
		opt_psu_voltage=APW17_MAX_VOLTAGE;
	}
	fprintf(stdout, "%s: psu_voltage=%0.2f\n", __FUNCTION__, opt_psu_voltage);
	if(opt_fan_speed_percentage<S21_FAN_SPEED_PERCENTAGE_MIN)
	{
		opt_fan_speed_percentage=S21_FAN_SPEED_PERCENTAGE_MIN;
	}
	else if(opt_fan_speed_percentage>S21_FAN_SPEED_PERCENTAGE_MAX)
	{
		opt_fan_speed_percentage=S21_FAN_SPEED_PERCENTAGE_MAX;
	}
	fprintf(stdout, "%s: fan_speed_percentage=%i\n", __FUNCTION__, opt_fan_speed_percentage);
	apply_fan_pwm_setting(FAN_PWM_MAX_VALUE*opt_fan_speed_percentage/100);
	g_autotuner->fan_control_i=g_autotuner->fan_pwm_value/FAN_CONTROL_IK;
}
