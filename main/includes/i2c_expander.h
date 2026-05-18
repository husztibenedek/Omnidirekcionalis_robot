#ifndef I2C_EXPANDER_H
#define I2C_EXPANDER_H

//library-s
#include <stdio.h>
#include "ultrasonic.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

  

//BUS settings
    //port kiválasztás
    #define I2C_MASTER_NUM I2C_NUM_0
    //Adatport
    #define I2C_Master_SDA_GPIO_NUM GPIO_NUM_21
    #define I2C_Master_SLC_GPIO_NUM GPIO_NUM_22

    #define I2C_Glitch_Ignore_Cnt 7

    extern i2c_master_bus_handle_t bus_handle;
    //IT port
    #define IT_pin_num  GPIO_NUM_14
    

//I2C settings
    #define I2C_Btn_Address 0x20
    #define I2C_Led_Address 0x21
    #define I2C_Scl_FreqHz 100000

    extern i2c_master_dev_handle_t dev_handle_W; //write
    extern i2c_master_dev_handle_t dev_handle_R; //read
    

//Leds
    #define LedHeart GPIO_NUM_15 //Heart Led
    #define led1 0x01 
    #define led2 0x02
    #define led3 0x04
    #define led4 0x08
    #define led5 0x10
    
//Buttons && Switches

    #define btn1 0x04
    #define btn2 0x08
    #define btn3 0x10
    #define btn_mask 0x1C

    #define sw1  0x20
    #define sw2  0x40
    #define sw3  0x80
    #define sw_mask 0xE0

//Ultrasonic sensor
    #define UV_trigger1 0x20
    #define UV_trigger2 0x40
    #define UV_trigger3 0x80


    #define UV_echo1 GPIO_NUM_17
    #define UV_echo2 GPIO_NUM_16
    #define UV_echo3 GPIO_NUM_4
    
    #define ROUNDTRIP_CM 58 // 
    #define Max_Sensor_Dist_cm

    extern ultrasonic_sensor_t UV_sensor1;
    extern ultrasonic_sensor_t UV_sensor2;
    extern ultrasonic_sensor_t UV_sensor3;

//fuggvenyek
    void initExpander(void); //port bovito
    void init_Button_IT(gpio_num_t IT_pin);
    esp_err_t expander_set_gpio_level(i2c_master_dev_handle_t dev, uint8_t bit_mask, bool level);
    void readButtons(uint8_t *button);
    static void button_task(void *pvParam);
    uint8_t readbyte();
    
#endif