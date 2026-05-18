#ifndef ADC_READ
#define ADC_READ

#include "esp_adc/adc_oneshot.h"

#define BATTERy_adc_chan ADC_CHANNEL_7 //GPIO_27
#define BATTERY_adc_unit ADC_UNIT_2
#define BATTERY_adc_atten ADC_ATTEN_DB_12 // ~3.1V méréshatár
#define BATTERY_res_vcc 9.1 //battery resistor VCC [kOhm]
#define BATTERY_res_gnd 3 //battery resistpr GND [kOhm]

//függvények

esp_err_t battery_monitor_task_init();
float battery_read_task();











#endif
