#ifndef AUTOTUNER_H
#define AUTOTUNER_H

#include <stdint.h>

#define FAN_PWM_PERIOD_NS			100000
#define FAN_PWM_MIN_VALUE			0
#define FAN_PWM_MAX_VALUE			1023
#define FAN_CONTROL_PK				20.0F
#define FAN_CONTROL_IK				1.0F
#define FAN_CONTROL_DK				30.0F

#define SKIP_MEASUREMENT			24

#define SYSTEM_PWM_DEVICE_0 "/sys/class/pwm/pwmchip0/pwm0"
#define SYSTEM_PWM_DEVICE_1 "/sys/class/pwm/pwmchip0/pwm1"

typedef enum
{
	AUTOTUNER_DISABLED,
	AUTOTUNER_ONLY_FANS,
	AUTOTUNER_TARGET_HASHRATE,
	AUTOTUNER_TARGET_CONSUMPTION,
	AUTOTUNER_MAXIMUM_PERFORMANCE
} autotuner_mode_e;

typedef enum
{
	AUTOTUNER_PREPARE,
	AUTOTUNER_WORKING,
	AUTOTUNER_DONE
} autotuner_state_e;

typedef struct
{
	autotuner_mode_e	mode;
	autotuner_state_e	state;
//	uint8_t				chain_tunable[CHAINS_MAX], chain_tuned[CHAINS_MAX];
	uint32_t			pll_setting_index, target_pll_setting_index;
	int32_t				fan_pwm_value;
	float				voltage_setting, target_voltage_setting;
	float				fan_control_p, fan_control_i, fan_control_d;
	uint8_t				fan_pidc_active;
} autotuner_t;

void apply_fan_pwm_setting(int32_t pwm_value);
void adjust_fan_pwm_according_to_temperature(float temperature);
void autotuner_init();

extern autotuner_t *g_autotuner;

#endif // AUTOTUNER_H
