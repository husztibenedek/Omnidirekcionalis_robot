#include "i2c_expander.h"


//Változók
SemaphoreHandle_t button_sem = NULL;
extern  bool meres_aktiv;
static const char *tag = "gomb_kezelo";


/**
 * @brief ISR Interrupt Service Routine 
 */
static void IRAM_ATTR ButtonITHandler(void *data){
    BaseType_t xHighPriorTask = pdFALSE; 
    //szemafor átadás
    xSemaphoreGiveFromISR(button_sem,&xHighPriorTask); 
    //ha if lefut átugrik a gomb-olvasó taskra
    if(xHighPriorTask){
        portYIELD_FROM_ISR(); 
    }
}



/**
 * @brief FreeRTOS task for handling button events and toggling measurement mode.
 * 
 * Waits for a semaphore signal triggered by a hardware interrupt, reads the current 
 * button state from the port expander, detects rising edge presses, and toggles the 
 * measurement state and status LED accordingly.
 * 
 * @param[in] pvParam Task parameter pointer pvParam íráshoz kell
 */
void button_task( void *pvParam){ 
    uint8_t btn_state = 0x00;
    uint8_t prev_btn_state = 0x00;

    while(1){ // "Tasks must not return" --> infinite loop for(;;) v while(1)
        if(xSemaphoreTake(button_sem, portMAX_DELAY)== pdTRUE){ // portMAX_DELAY == alszik, amíg meg nem kapja a szemafort.
            //kiolvasás
            ReadButtons(&btn_state);
            // pörges menetesítés
            if(btn_state == btn1 && prev_btn_state != btn1){
                meres_aktiv = !meres_aktiv;
                if(meres_aktiv){
                    ESP_LOGI("GOMB","+++Meres start+++"); 
                    expander_set_gpio_level(led1, false);                  
                } 
                else{
                    ESP_LOGI("GOMB","---Meres stop---");
                    expander_set_gpio_level(led1, true);
                } 
            } 
            prev_btn_state = btn_state;
        }
    }
} 


/**
 * @brief Initializes the button GPIO interrupt (ISR), binary semaphore, and worker task.
 * @param IT_pin GPIO pin number connected to the button's interrupt line
 */

void InitButtonIT(gpio_num_t IT_pin){
    // semaphore létrehozása --> két állapota tele=1 / ures=0
    button_sem = xSemaphoreCreateBinary(); 
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // Földre húzásnál jelez
        .pin_bit_mask = (1ULL << IT_pin), //
        .mode = GPIO_MODE_INPUT, // láb bemenet 
        .pull_up_en = 1, //felhuzo elleneállás
        .pull_down_en = 0 //
    };gpio_config(&io_conf);
    //megszakítási szolgáltatás ("Főkapcsoló") ki kell kommentezni ha már máshol megvan hívva
    gpio_install_isr_service(0); 
    
    // (melyik láb, melyik függvényt indítsuk el, láb szám átadás lehet NULL is)
    gpio_isr_handler_add(IT_pin,ButtonITHandler, (void*) IT_pin); 

    //(fgv neve amit használunk," ",stack memória méret, paraméter(amit beküldünk a tasknak), task handle)
    xTaskCreate(button_task,"button_task",4096, NULL ,5, NULL);

}

/**
 * @brief read the buttons on the port expander
 * @param button pointer to the variable storing the debounced button bitmask.
 */

void ReadButtons(uint8_t *button){
    uint8_t raw_d ;
    if(i2c_master_receive(I2C_Button_Handle, &raw_d, 1, pdMS_TO_TICKS(100)) != ESP_OK){
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


/**
 * @brief Reads the state of the switches which is the ID for the robot
 *  Only need to run one time during initialization
 * @return the ID of the robot in binary
 */
uint8_t ReadSwitches(){
    uint8_t rawData;
    if (i2c_master_receive(I2C_Button_Handle, &rawData, 1, pdMS_TO_TICKS(100)) != ESP_OK){
       return 0x00;
    }
    uint8_t swh = ~rawData & sw_mask;
    return swh;
}