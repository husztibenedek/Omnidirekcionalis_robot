
#include "i2c_expander.h"

/**
 * @brief Controls the specified LED via the port expander
 * @param[in] led_number Index of the LED to control:
 *                       - 1..5: Individual LEDs
 *                       - 6: All LEDs simultaneously (allLed)
 * @param[in] state      Target logical state:
 *                       - true:  Turn LED ON
 *                       - false: Turn LED OFF
 * @return esp_err_t Result of the operation
 * @note active-low inverted logic in use
*/
esp_err_t LedOnOff(int led_number, bool state){
    uint8_t led_mask;
    switch(led_number){
        case 1: led_mask = led1;
            break;
        case 2: led_mask = led2;
            break;
        case 3: led_mask = led3;
            break;
        case 4: led_mask = led4;
            break;
        case 5: led_mask = led5;
            break;
        case 6 : led_mask = allLed; 
            break;
        default : return  ESP_ERR_INVALID_ARG;

    }
    return expander_set_gpio_level(led_mask,(!state));
}

