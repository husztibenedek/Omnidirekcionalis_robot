#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ultrasonic.h"
#include "i2c_expander.h"
#include "motor.h"
#include "adc_read.h"

#define max_distance_cm 500

extern ultrasonic_sensor_t UV_sensor1;
extern ultrasonic_sensor_t UV_sensor2;
extern ultrasonic_sensor_t UV_sensor3;
extern motorControl_t motors[3];

static const char *TAG = "MAIN";

extern i2c_master_dev_handle_t dev_handle_W;

uint8_t leds;
uint8_t led_state;
volatile bool meres_aktiv = false;
esp_err_t error = -1;



void app_main(void)
{
    ESP_LOGI(TAG,"Rendszer indítása");  
    
    initExpander();
    ESP_LOGI(TAG, "I2C incializálva");


    init_Button_IT(IT_pin_num);
    ESP_LOGI(TAG, "Button IT incializálva");

    
    error = initUltraSensor(&UV_sensor1, dev_handle_W,UV_echo1,UV_trigger1); 
    if(error == ESP_OK){
        ESP_LOGI("Main" ,"UV_sensor1 inicializálva");

        // A mérést végző Task elindítása    
        // xTaskCreate(Függvény_neve, "Név", Stack_méret, paraméter(amit beküldünk a tasknak), Prioritás, Task_handle)
        xTaskCreate(ultrasonic_task,"ultra_measure_task",4096, &UV_sensor1, 5, &UV_sensor1.mes_handle_t);
    }

    uint8_t dummy = 0;
    readButtons(&dummy); // Ezzel nullazom a portbővítőt induláskor
    ESP_LOGI("Main", "Portbővítő IT reset.");
    

    UV_sensor1.name = "Senzor_1";
    ESP_LOGI(TAG, "UV senzor 1 named");
    
    
    
    ESP_LOGI(TAG, "Motorok init");
    ESP_ERROR_CHECK(initMotor());

    ESP_LOGI(TAG,"Enkoder init");
    initEncoder();
    
    ESP_LOGI(TAG,"Battery read init");
    error = battery_monitor_task_init();
    if(error != ESP_OK){
       ESP_LOGE(TAG, "BATTERY init failed"); 
    }
    
    while(1){
        
        if(meres_aktiv){
            xTaskNotifyStateClear(UV_sensor1.mes_handle_t);
            ultrasonic_trigger(&UV_sensor1);
        }
          
        float battery = 0.0;
        setMotor(0, 50);
        setMotor(1, 50);
        setMotor(2, 50);
        ESP_LOGI("TESZT", "Motor PWM kiadva!");
        current_omega_calculation();
        ESP_LOGI("MAIN","Omega értéke motor[1]:%.3frad/s | motor[2]:%.3frad/s | motor[3]:%.3frad/s", motors[0].omega, motors[1].omega,motors[2].omega);
        vTaskDelay(pdMS_TO_TICKS(2000));
        setMotor(2,-50);
        ESP_LOGI("MAIN","Omega értéke motor[1]:%.3frad/s | motor[2]:%.3frad/s | motor[3]:%.3frad/s", motors[0].omega, motors[1].omega,motors[2].omega);
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGW("MAIN", "szendvics ready");
        battery = battery_read_task();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
