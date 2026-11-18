#ifndef ULTRASONIC_H
#define ULTRASONIC_H


#include <stdio.h>
#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "rom/ets_sys.h" // ets_delay_us-hez


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
        volatile float distance;
        volatile bool in_reach;

}ultrasonic_sensor_t;  

// Változók
extern ultrasonic_sensor_t UV_sensor1;
extern ultrasonic_sensor_t UV_sensor2;
extern ultrasonic_sensor_t UV_sensor3;


//Függvények
    void initUltraSenzors();
    esp_err_t InitUltraSensor(ultrasonic_sensor_t *sensor, gpio_num_t echo_pin, uint8_t trigger_pin);
    esp_err_t UltrasonicTrigger(const ultrasonic_sensor_t *sensor);
    esp_err_t UltrasonicMeasure(ultrasonic_sensor_t *pvParameters);

#endif // ULTRASONIC_H
