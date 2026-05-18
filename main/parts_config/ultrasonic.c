#include <stdio.h>
#include "ultrasonic.h"
#include "rom/ets_sys.h" // ets_delay_us-hez
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "i2c_expander.h"

extern volatile bool meres_aktiv;
static const char *TAG = "UV_SZENZOR";

ultrasonic_sensor_t UV_sensor1;
//ultrasonic_sensor_t UV_sensor2;
//ultrasonic_sensor_t UV_sensor3;


static bool IRAM_ATTR capture_pulse(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data) {
    ultrasonic_sensor_t *sensor = (ultrasonic_sensor_t *) user_data; // void átkasztolása -> void* univerzális típus változó  
    //
    BaseType_t high_task_wp = pdFALSE;
    
    
    //      Felfutó él (0 -> 1)
    if(edata->cap_edge == MCPWM_CAP_EDGE_POS){
        sensor->start_edge = edata->cap_value;
    }
    //      Lefutó él ( 1 -> 0)
    else{
        uint32_t dist = (edata->cap_value - sensor->start_edge) / Mhz_oszto;
        sensor->distance = dist / ROUNDTRIP_CM; 

        // IT task jelez a mainbe hogy kész
        if(sensor->mes_handle_t != NULL){
            vTaskNotifyGiveFromISR(sensor->mes_handle_t, &high_task_wp);
        }
    }
    //megvizsgálni, hogy a megszakítás során a high_task_wp értéke igazzá (pdTRUE) vált-e, és ezt az igaz/hamis eredményt akarjuk visszaadni.
    return high_task_wp == pdTRUE;
}

// Az ultrahangos szenzor inicializálása
    // Paraméterek:
    // - &UV_sensor1: A szenzor struktúra címe
    // - dev_handle_W: A portbővítő (író) azonosítója
    // - GPIO_NUM_4: Az ESP32 fizikai lába, ahova az ECHO van kötve 
    // - 5: A portbővítő lábának sorszáma vagy lába, ahova a TRIGGER van kötve
esp_err_t initUltraSensor(ultrasonic_sensor_t *sensor,i2c_master_dev_handle_t dev_handleW, gpio_num_t echo_pin, uint8_t trigger_pin){
    sensor->i2c_device = dev_handleW;
    sensor->echo_pin = echo_pin;
    sensor->trigger_pin =  trigger_pin;
    sensor->start_edge = 0;
    sensor->mes_handle_t = NULL; 

    

    // MCPWM Capture Timer konfig
    static mcpwm_cap_timer_handle_t cap_timer = NULL;
        if(cap_timer == NULL){
            mcpwm_capture_timer_config_t timer_config = { 
                .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
                .resolution_hz = 1000000 , // (1 MHz = 1 us felbontás)
                .group_id = 0
            };
            ESP_ERROR_CHECK(mcpwm_new_capture_timer(&timer_config,&cap_timer));
            ESP_ERROR_CHECK(mcpwm_capture_timer_enable(cap_timer));
            ESP_ERROR_CHECK(mcpwm_capture_timer_start(cap_timer));
        }
        
    mcpwm_capture_channel_config_t chan_config = {
        .gpio_num = echo_pin,
        .prescale = 1,          //órajel osztás
        .flags.pos_edge = true,
        .flags.neg_edge = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_capture_channel(cap_timer,&chan_config,&sensor->cap_chan));

    //Megszakítás (Interrupt Callback) regisztráslása --> feszultseg ugras van akkor meghívja a capture_pulse fgv-t
    mcpwm_capture_event_callbacks_t cbs = {.on_cap = capture_pulse};
    
    //mcpwm_capture_channel_register_event_callbacks(ahol valtozas nezzuk, cbs cime, amibe menteni akarom a mert adatot)
    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(sensor->cap_chan,&cbs,sensor));

    
    return mcpwm_capture_channel_enable(sensor->cap_chan);
}

esp_err_t ultrasonic_trigger(const ultrasonic_sensor_t *sensor){
    if(sensor == NULL || sensor->i2c_device == NULL){
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t data;

    //trigger jel kuldes folymata alacsony -> magas -> alacsony
    data = expander_set_gpio_level(sensor->i2c_device, sensor->trigger_pin, false);
    if(data!= ESP_OK) return data;
    ets_delay_us(2);

    data = expander_set_gpio_level(sensor->i2c_device, sensor->trigger_pin, true);
    if(data!= ESP_OK) return data;
    ets_delay_us(10);

    data = expander_set_gpio_level(sensor->i2c_device, sensor->trigger_pin, false);
    if(data!= ESP_OK) return data;

    return ESP_OK;
}


esp_err_t ultrasonic_measure_cm(const ultrasonic_sensor_t *sensor,uint32_t *distance_cm){
    if (sensor == NULL || distance_cm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t start = sensor->start_edge;
    uint32_t down = sensor->down_edge;
    
    uint32_t pulse_width = (down - start)/Mhz_oszto; // 80 MHz miatt kell leosztani
    *distance_cm = pulse_width / ROUNDTRIP_CM;

    return ESP_OK;
}

void ultrasonic_task(void *pvParameters){
    ultrasonic_sensor_t *sensor = (ultrasonic_sensor_t *)pvParameters;
    sensor->mes_handle_t = xTaskGetCurrentTaskHandle();
    while(1){

        if(ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(50))){
            ESP_LOGI(sensor->name,"Distance:  %.2f cm",sensor->distance);
        }
        else{
            ESP_LOGW(sensor->name,"Error, no object in reach");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


