#include "ultrasonic.h"
#include "i2c_expander.h"
#include "motor.h"
#include "battery_read.h"
#include "controller.h"

#define max_distance_cm 500
static const char *TAG = "MAIN";  

uint8_t leds;
uint8_t led_state;
bool meres_aktiv;
esp_err_t error = -1;
float battery = 0.0;
uint8_t dummy = 0;
int64_t timestamp = 0;
int duty = 0;

float _Ts = 0.01f ; //10[ms]
float _Kp = 0.4423; // proportional gain
float _Ki = 9.677;// integral gain
float _Kd = 0.001935;// derivative gain
float _Tf = 0.01546; //derivative filter time constant = aluláteresztő szűrő időállandója
 
void app_main(void)   
{
    ESP_LOGI(TAG,"Rendszer indítása");  
    
    InitExpander();
    ESP_LOGI(TAG, "I2C incializálva");

    InitButtonIT(IT_pin_num);
    ESP_LOGI(TAG, "Button IT incializálva");

    initUltraSenzors();
    ESP_LOGI(TAG, "Ultrahangszenzorok incializálva");
    
    ReadButtons(&dummy); // Ezzel nullazom a portbővítőt induláskor
    ESP_LOGI("Main", "Portbővítő IT reset.");
    
    ESP_LOGI(TAG, "Motorok init");
    ESP_ERROR_CHECK(InitMotor());

    ESP_LOGI(TAG,"Enkoder init");
    InitEncoder();
    
    ESP_LOGI(TAG,"Battery read init");
    initBatteryRead();

    xTaskCreate(PIDloop,"PID_LOOP",2048,NULL,3,NULL);

    ESP_LOGI("MAIN", "Motor test start");
        bool state = false;
        for(int k = 6; k>0; k--){
            state = !state;
            ESP_LOGI("TESZT", "In %d",k);
            LedOnOff(1,state);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    motionVector(0,0,0);
    while(1){

        
       
        vTaskDelay(10);
    }  
}
