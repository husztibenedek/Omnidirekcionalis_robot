#ifndef ULTRASONIC_H
#define ULTRASONIC_H


#include <stdio.h>
#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define ROUNDTRIP_CM 58 
#define Mhz_oszto 80
#define Max_Sensor_Dist_cm

//ultrasonic sensor UV

typedef struct {
        const char* name;
        i2c_master_dev_handle_t i2c_device; //kimenet indításhoz
        uint8_t trigger_pin; 
        gpio_num_t echo_pin;
        mcpwm_cap_channel_handle_t cap_chan; //echo_pin feszultseg valtozasat "nezi"

        uint32_t down_edge;
        uint32_t start_edge;     
        volatile uint32_t distance;

        //megszakításhoz
        TaskHandle_t mes_handle_t;    
        
    }ultrasonic_sensor_t;  


//Függvények
    esp_err_t initUltraSensor(ultrasonic_sensor_t *sensor,i2c_master_dev_handle_t dev_handleW, gpio_num_t echo_pin, uint8_t trigger_pin);
    esp_err_t ultrasonic_trigger(const ultrasonic_sensor_t *sensor);
    esp_err_t ultrasonic_measure_cm(const ultrasonic_sensor_t *sensor,uint32_t *distance_cm);
    void ultrasonic_task(void *pvParameters);

#endif // ULTRASONIC_H