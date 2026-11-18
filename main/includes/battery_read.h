#ifndef ADC_READ
#define ADC_READ

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h" //pontosabb feszzültség számításra alkalmas fgvek tartalmaznak
#include "esp_adc/adc_cali_scheme.h"

#define BATTERy_adc_chan ADC_CHANNEL_7 //GPIO_27
#define BATTERY_adc_unit ADC_UNIT_2
#define BATTERY_adc_atten ADC_ATTEN_DB_12 // ~3.1V méréshatár
#define BATTERY_res_vcc 9.1 //battery resistor VCC [kOhm]
#define BATTERY_res_gnd 3 //battery resistpr GND [kOhm]

//függvények
void initBatteryRead();
esp_err_t battery_monitor_task_init();
float battery_read_task_V();
float battery_read_task_P(float battery_voltage);











#endif











#endif
