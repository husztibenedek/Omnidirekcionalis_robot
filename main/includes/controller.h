#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



#define radius 0.0203f //wheel radious [mm]
#define d  0.0985f // wheel distance from the center of the robot[mm]
#define sin60 0.866f
 
typedef struct
{
    //inputs
    float targetOmega;

    // belső változó
    float prevOmega;
    float prevError;
    float prevTarget;
    float prev_D;
    float integralError;
    float deltaTime; // _Ts
 
    //outputs
    float pwm;
    float output; // controller output
}pid_t;


void PIDloop(void *pvParameter);
void pidController(int motor_index);
float pointLookUptable(int motor_index ,float output);
void motionVector(float vbx, float vby, float wbz);
void velocity(int motorId, float speed);
void setPIDParam(float Kp, float Ki, float Kd, float Tf);
#endif