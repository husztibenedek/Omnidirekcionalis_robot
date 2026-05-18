#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "sdkconfig.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "rom/ets_sys.h"

#include "i2c_expander.h"


i2c_master_dev_handle_t dev_handle_W; //Write
i2c_master_dev_handle_t dev_handle_R; //Read
i2c_master_bus_handle_t bus_handle;

static uint8_t pins_w_state = 0xFF; // 

static const char *tag = "gomb_kezelo";
static const char *TAG = "I2C";

extern volatile bool meres_aktiv;
SemaphoreHandle_t button_sem = NULL;

//I2C config
void initExpander(){

    //Bus config
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_Master_SLC_GPIO_NUM,  //órajel
        .sda_io_num = I2C_Master_SDA_GPIO_NUM,  //adat
        .glitch_ignore_cnt = I2C_Glitch_Ignore_Cnt,
        .flags.enable_internal_pullup = false,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    //led config
    i2c_device_config_t led_cfg = { 
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_Led_Address,
        .scl_speed_hz = I2C_Scl_FreqHz,
    
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &led_cfg, &dev_handle_W));

    //btn config
    i2c_device_config_t btn_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_Btn_Address,
        .scl_speed_hz = I2C_Scl_FreqHz,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &btn_cfg, &dev_handle_R));
}

esp_err_t expander_set_gpio_level(i2c_master_dev_handle_t dev, uint8_t bit_mask, bool state) { // kimenet írására , akár többet is egyszerre

        if (state) 
            pins_w_state = pins_w_state | bit_mask ; // Bit bekapcsolása 
        else 
            pins_w_state = pins_w_state & (~bit_mask); // Bit kikapcsolása

    return i2c_master_transmit(dev, &pins_w_state, 1, pdMS_TO_TICKS(100));
}

void readButtons(uint8_t *button){
    uint8_t raw_d ;
    if(i2c_master_receive(dev_handle_R, &raw_d, 1, pdMS_TO_TICKS(100)) != ESP_OK){
        return;
    }

    uint8_t btn = (~raw_d) & btn_mask;

    switch(btn){
        case btn1:
            ESP_LOGI(tag,"P2 lenyomva");
            break;

        case btn2:
            ESP_LOGI(tag,"P3 lenyomva");
            break;

        case btn3:
            ESP_LOGI(tag,"P4 lenyomva");
            break;

        case 0x0C:
            ESP_LOGI(tag,"P2 && P3 lenyomva");
            break;

        case 0x14:
            ESP_LOGI(tag,"P2 && P4 lenyomva");
            break;

        case 0x18:
            ESP_LOGI(tag,"P3 && P4 lenyomva");
            break; 

        case 0x1C:
            ESP_LOGI(tag,"P2 && P3 && P4 lenyomva");
            break;

        default:
            ESP_LOGD(tag,"Semmi sincs lenyomva",btn);
            break;
    }
    *button = btn;
}

//Megszakítás kezelő
static void IRAM_ATTR button_IT_handler(void *data){
    BaseType_t xHighPriorTask = pdFALSE; 
    xSemaphoreGiveFromISR(button_sem,&xHighPriorTask); //szemafor átadás
    if(xHighPriorTask){
        portYIELD_FROM_ISR(); //ha if lefut átugrik a gomb-olvasó taskra
    }
}

// GOMB FIGYELŐ TASK
static void button_task( void *pvParam){ //pvParam íráshoz kell
    uint8_t btn_state = 0x00;
    uint8_t prev_btn_state = 0x00;

    while(1){ // "Tasks must not return" --> infinite loop for(;;) v while(1)
        if(xSemaphoreTake(button_sem, portMAX_DELAY)== pdTRUE){ // portMAX_DELAY == alszik, amíg meg nem kapja a szemafort.
            //kiolvasás
            readButtons(&btn_state);
            // pörges menetesítés
            if(btn_state == btn1 && prev_btn_state != btn1){
                meres_aktiv = !meres_aktiv;
                if(meres_aktiv){
                    ESP_LOGI("GOMB","+++Meres start+++"); 
                    expander_set_gpio_level(dev_handle_W, led1, false);                  
                } 
                else{
                    ESP_LOGI("GOMB","---Meres stop---");
                    expander_set_gpio_level(dev_handle_W, led1, true);
                } 
            } 
            prev_btn_state = btn_state;
        }
    }
} 

//IT inicializálás
void init_Button_IT(gpio_num_t IT_pin){
    button_sem = xSemaphoreCreateBinary(); // semaphore létrehozása --> két állapota tele=1 / ures=0
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // Földre húzásnál jelez
        .pin_bit_mask = (1ULL << IT_pin), //
        .mode = GPIO_MODE_INPUT, // láb bemenet 
        .pull_up_en = 1, //felhuzo elleneállás
        .pull_down_en = 0 //
    };gpio_config(&io_conf);

    gpio_install_isr_service(0); //megszakítási szolgáltatás ("Főkapcsoló") ki kell kommentezni ha már máshol megvan hívva
    
    // (melyik láb, melyik függvényt indítsuk el, láb szám átadás lehet NULL is)
    gpio_isr_handler_add(IT_pin,button_IT_handler, (void*) IT_pin); 

    //(fgv neve amit használunk," ",stack memória méret, paraméter(amit beküldünk a tasknak), task handle)
    xTaskCreate(button_task,"button_task",4096, NULL ,5, NULL);

}


uint8_t readbyte(){
    uint8_t byte = 0xFF;
    if((i2c_master_receive(dev_handle_R, &byte, 1, pdMS_TO_TICKS(100))) != ESP_OK){
        ESP_LOGE(TAG, "Hiba az I/O bővítő olvasásakor!"); //piros jelzés ha hiba
    }
    return byte;
}
