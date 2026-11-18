#include "i2c_expander.h"


i2c_master_dev_handle_t I2C_LED_Handle; //Write
i2c_master_dev_handle_t I2C_Button_Handle; //Read
i2c_master_bus_handle_t I2CBus_handle;

static uint8_t pins_w_state = 0xFF; // 

//static const char *tag = "gomb_kezelo";
static const char *TAG = "I2C";

extern bool meres_aktiv;


/**
 * @brief Initialize U2C port expander
 * 
 */
void InitExpander(){

    //Bus config
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_Master_SLC_GPIO_NUM,  //órajel
        .sda_io_num = I2C_Master_SDA_GPIO_NUM,  //adat
        .glitch_ignore_cnt = I2C_Glitch_Ignore_Cnt,
        .flags.enable_internal_pullup = false,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &I2CBus_handle));

    //led config
    i2c_device_config_t led_cfg = { 
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_Led_Address,
        .scl_speed_hz = I2C_Scl_FreqHz,
    
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(I2CBus_handle, &led_cfg, &I2C_LED_Handle));

    //btn config
    i2c_device_config_t btn_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_Btn_Address,
        .scl_speed_hz = I2C_Scl_FreqHz,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(I2CBus_handle, &btn_cfg, &I2C_Button_Handle));
}

/**
 * @brief Sets the logical level of one or multiple output pins simultaneously.
 * @param dev      Handle to the I2C master device.
 * @param bit_mask Bitmask of the GPIO pins to modify.
 * @param state    Target state (true = HIGH, false = LOW).
 * @return ESP_OK on success or specific esp_err_t error code on failure
 */
esp_err_t expander_set_gpio_level(uint8_t bit_mask, bool state) { 
        
        if (state) {
            //ESP_LOGI("I2C","Pin on");
            pins_w_state = pins_w_state | bit_mask ; // Bit bekapcsolása 
        }
        else {
            //ESP_LOGI("I2C","Pin off");
            pins_w_state = pins_w_state & (~bit_mask); // Bit kikapcsolása
        }

    return i2c_master_transmit(I2C_LED_Handle, &pins_w_state, 1, pdMS_TO_TICKS(100));
        
}

/**
 * @brief Reads the port expanders legs
 * @return uint8_t The 8-bit port state byte, or 0xFF if the I2C transfer fails.
 */
uint8_t ReadI2C(){
    uint8_t byte = 0xFF;
    if((i2c_master_receive(I2C_Button_Handle, &byte, 1, pdMS_TO_TICKS(100))) != ESP_OK){
        //piros jelzés ha hiba
        ESP_LOGE(TAG, "Hiba az I/O bővítő olvasásakor!"); 
    }
    return byte;
}

