#ifndef MOTOR_H
#define MOTOR_H
#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "driver/pulse_cnt.h"
#include "controller.h"




typedef struct {
    //motor
    int serial_nmb;
    mcpwm_cmpr_handle_t cmpA;
    mcpwm_cmpr_handle_t cmpB;

    //enkoder
    pcnt_unit_handle_t encoder;

    //vezérlés
    pid_t PID;

    //szamitas
    int lastValue; //elozo ciklusbeli encoder allas
    float omega;   //szogsebesseg (radian/masodperc)

}motorControl_t;


// Motor bemenet
#define Mot1_EncA GPIO_NUM_36
#define Mot1_EncB GPIO_NUM_39
#define Mot1_DrvA GPIO_NUM_33
#define Mot1_DrvB GPIO_NUM_32

#define Mot2_EncA GPIO_NUM_34
#define Mot2_EncB GPIO_NUM_35
#define Mot2_DrvA GPIO_NUM_26
#define Mot2_DrvB GPIO_NUM_25

#define Mot3_EncA GPIO_NUM_12
#define Mot3_EncB GPIO_NUM_13
#define Mot3_DrvA GPIO_NUM_0
#define Mot3_DrvB GPIO_NUM_2

#define High_LIMIT 32767
#define Low_LIMIT -32767
#define PPWR 6144.0f 
#define TimerFreq 20000000.0f
#define periodTicks 500.0f
#define halfPeriodTicks (periodTicks/2)
// Feszültség = (2 * Kitöltési_tényező - 1) * Tápfesz
// Komparátor_érték = halfPeriodTicks + offset
// offset = Kitöltési_tényező/100 * halfPeriodTicks




/*  pulse per wheel revolution - PPWR = hány darab impulzust ad le a szenzor, mialatt a robot kereke pontosan egyetlen teljest fordulatot (360 fokot) 
                                        tesz meg.
    áttétel: 1:48
    encoder "inkremens" felbontása (32): 128 <= 4x-es PPR <== 10 kerék fordulat ->Fullquad mód 6243 pulzus PPR= 6243/48 = 130
                                       <== 10 kerék fordulat -> SingleEdge mód 1534 pulzus PPR = 1534/48 = 32 amit ha felszorzunk 4-el akkor 128-t kapunk
    PPWR = 128 * 48 = 6144
    kerék viszont 48-szor lassabban forog, mint a motor
*/
#define max_speed 255
#define min_speed -255

// Motor hibakód
#define DRV1_FAULT 0x01
#define DRV2_FAULT 0x02

//motor változók
extern motorControl_t motors[3];

// fuggvenyek
static void GenActionConfig(mcpwm_gen_handle_t gena, mcpwm_gen_handle_t genb, mcpwm_cmpr_handle_t cmpa);
static esp_err_t SetupMotorMcpwm(gpio_num_t mot_A_Gpio, gpio_num_t mot_B_Gpio, motorControl_t *motor, mcpwm_timer_handle_t timer);
esp_err_t InitMotor();
void SetMotor(int motor, float speed);// i = 0 ,1 ,2 ; speed [-100 , 100]
void CheckFaults();
static esp_err_t SetupEncoderPcnt(gpio_num_t pin_a, gpio_num_t pin_b, motorControl_t *motor);
esp_err_t InitEncoder();
void IRAM_ATTR OmegaCalculation();
esp_err_t InitEncTimer();

#endif
