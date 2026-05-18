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
#include "adc_read.h"


const char *TAG = "BATTERY ADC"; 
static adc_oneshot_unit_handle_t adc2_handle;
float BATTERY_res = (BATTERY_res_gnd + BATTERY_res_vcc) / BATTERY_res_gnd; // Uki  = U_battery (Rgnd/(Rgnd+Rvcc)) => Ubattery = Uki*((Rgnd+Rvcc)/Rgnd) 


esp_err_t battery_monitor_task_init(){
    ESP_LOGI(TAG, "ADC2 init");
    
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = BATTERY_adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE //letiltja a mélyalvásért felelős segédprocesszor (ULP - Ultra Low Power) hozzáférését.
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    ESP_LOGI(TAG, "ADC2 Calibration Init");
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // mérés felbontásának (részletességének) beállítása
        .atten = BATTERY_adc_atten // elszint-csillapítás beállítása=> meghatározza a láb maximális feszültség-méréshatárát (pl. 12dB esetén kb. 3.1V a plafon).
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, BATTERy_adc_chan, &config));
    ESP_LOGI(TAG, "ADC2 initalized");

    return ESP_OK;
}

float battery_read_task(){
    int adc_raw = 0;
    float battery_voltage = 0.0;
    float battery_percent = 0.0;
    esp_err_t read = adc_oneshot_read(adc2_handle, BATTERy_adc_chan, &adc_raw); // adc_raw 0-4095 közti érték
    if(read != ESP_OK) return -1.0f;

    // feszültség a lábon (max ~3.1V) <= .atten = ADC_ATTEN_DB_12 
    // adc_raw/4095) * 3.1 esp lábon mennyi a feszültség => akkumlátor feszültség szintje = ((adc_raw/4095) * 3.1) * BATTERY_res)
    battery_voltage = ((adc_raw/4095.0) * 3.1) * BATTERY_res;
    //12V táp => min 3*3.3V = 9.9V, max = 12V lenne de egy aksi max 4.35V | max = 3 *4,35V = 13,05V
    battery_percent = ((battery_voltage - 9.9)/ (13.05 - 9.9)) * 100.0; //
    if(battery_percent > 100) battery_percent = 100.0;
    if(battery_percent < 0) battery_percent = 0.0;

    ESP_LOGI(TAG, "Akku: %2f[V] | %2f[%] " ,battery_voltage, battery_percent);

    return battery_percent;

}
