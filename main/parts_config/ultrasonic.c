#include "ultrasonic.h"
#include "i2c_expander.h"



extern bool meres_aktiv;
ultrasonic_sensor_t UV_sensor1;
ultrasonic_sensor_t UV_sensor2;
ultrasonic_sensor_t UV_sensor3;
extern i2c_master_dev_handle_t I2C_LED_Handle;

/**
 * Captures the pulse length
 * @param channel used for sending and receiving pulse
 * @brief MCPWM capture event data
 * @param user data for saved for measurments
 * @return if the capture was succesful or not
*/
static bool IRAM_ATTR CapturePulse(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data) {
    ultrasonic_sensor_t *sensor = (ultrasonic_sensor_t *) user_data; // void átkasztolása -> void* univerzális típus változó  

    //      Felfutó él (0 -> 1)
    if(edata->cap_edge == MCPWM_CAP_EDGE_POS){
        sensor->start_edge = edata->cap_value;
        sensor->in_reach = false;
    }
    //      Lefutó él ( 1 -> 0)
    else{
        sensor->down_edge = edata->cap_value; 
        sensor->in_reach = true;   
    }
    return false;
}

/**
 * Initializing Ultrassonic sensor
 * @param sensor struct address
 * @param port expansion tag
 * @param ESP32 leg physical tag 
 * @param port expander pin number or pin for TRIGGER connection
 * @return esp_err_t ESP_OK on success, or an error code on failure
 */  
esp_err_t InitUltraSensor(ultrasonic_sensor_t *sensor, gpio_num_t echo_pin, uint8_t trigger_pin){
    sensor->i2c_device = I2C_LED_Handle;
    sensor->echo_pin = echo_pin;
    sensor->trigger_pin =  trigger_pin;
    sensor->start_edge = 0;

    

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
    mcpwm_capture_event_callbacks_t cbs = {.on_cap = CapturePulse};
    
    //mcpwm_capture_channel_register_event_callbacks(ahol valtozas nezzuk, cbs cime, amibe menteni akarom a mert adatot)
    ESP_ERROR_CHECK(mcpwm_capture_channel_register_event_callbacks(sensor->cap_chan,&cbs,sensor));
    //Ultrahang senzor név kiosztás
    

    return mcpwm_capture_channel_enable(sensor->cap_chan);
}

/**
 * @brief Initializing Ultrassonic sensors (all 3)
 */
void initUltraSenzors(){

    esp_err_t error = -1;
    UV_sensor1.name = "Senzor_1";
    error = InitUltraSensor(&UV_sensor1,UV_echo1, UV_trigger1); 
    if(error == ESP_OK){
        ESP_LOGI("Main" ,"UV_sensor1 inicializálva");

        // A mérést végző Task elindítása    
        // xTaskCreate(Függvény_neve, "Név", Stack_méret, paraméter(amit beküldünk a tasknak), Prioritás, Task_handle)
        //xTaskCreate(UltrasonicTask,"Ultrasonic_Measure",4096, &UV_sensor1, 5, &UV_sensor1.mes_handle_t);
        error = -1;
    }
    UV_sensor2.name = "Senzor_2";
    error = InitUltraSensor(&UV_sensor2,UV_echo2, UV_trigger2);
    if(error == ESP_OK){
        ESP_LOGI("Main" ,"UV_sensor2 inicializálva");

        // A mérést végző Task elindítása    
        // xTaskCreate(Függvény_neve, "Név", Stack_méret, paraméter(amit beküldünk a tasknak), Prioritás, Task_handle)
        //xTaskCreate(UltrasonicTask,"Ultrasonic_Measure",4096, &UV_sensor2, 5, &UV_sensor2.mes_handle_t);
        error = -1;
    }
    UV_sensor3.name = "Senzor_3";
    error = InitUltraSensor(&UV_sensor3,UV_echo3, UV_trigger3);
    if(error == ESP_OK){
        ESP_LOGI("Main" ,"UV_sensor3 inicializálva");

        // A mérést végző Task elindítása    
        // xTaskCreate(Függvény_neve, "Név", Stack_méret, paraméter(amit beküldünk a tasknak), Prioritás, Task_handle)
        //xTaskCreate(UltrasonicTask,"Ultrasonic_Measure",4096, &UV_sensor3, 5, &UV_sensor3.mes_handle_t);
        error = -1;
    }
}

/**
 * @brief Sets the trigger pulse for ultrasonic sensor to receive
 * @param ID number of the u.sensor
 * @return esp_err_t ESP_OK on success, or an error code on failure
 */
esp_err_t UltrasonicTrigger(const ultrasonic_sensor_t *sensor){
    if(sensor == NULL || sensor->i2c_device == NULL){
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t error;

    //trigger jel kuldes folymata alacsony -> magas -> alacsony
    error = expander_set_gpio_level(sensor->trigger_pin, false);
    if(error!= ESP_OK) return error;
    ets_delay_us(2);

    error = expander_set_gpio_level(sensor->trigger_pin, true);
    if(error!= ESP_OK) return error;
    ets_delay_us(10);

    error = expander_set_gpio_level(sensor->trigger_pin, false);
    if(error!= ESP_OK) return error;
    ets_delay_us(2);

    return ESP_OK;
}


/**
 * FreeRTOS task to periodically process and log ultrasonic sensor measurements
 * 
 * @param sensor Pointer to the ultrasonic_sensor_t configuration structure
 */
esp_err_t UltrasonicMeasure(ultrasonic_sensor_t *sensor){

    if (sensor == NULL) {
        vTaskDelete(NULL);
        return ESP_ERR_INVALID_ARG;
    }
    
    
        int timeout = 25;
        if(UltrasonicTrigger(sensor) == ESP_OK){
            while (!sensor->in_reach && timeout > 0) {
                vTaskDelay(pdMS_TO_TICKS(1));
                timeout--;
            }   

        // Túlcsordulás elleni védelem
        if (sensor->down_edge < sensor->start_edge) {
            sensor->distance = 0;
            return ESP_ERR_INVALID_STATE; 
        }

        if(sensor->in_reach){
            float dist =(float)(sensor->down_edge-sensor->start_edge) / Mhz_oszto;
            sensor->distance = dist / ROUNDTRIP_CM;
            ESP_LOGI(sensor->name,"Distance:  %.2f cm",sensor->distance);
            return ESP_OK;
        }

        else{
            ESP_LOGW(sensor->name,"Error, no object in reach");
            return ESP_ERR_TIMEOUT;
            }
        }
        return ESP_FAIL;
}


