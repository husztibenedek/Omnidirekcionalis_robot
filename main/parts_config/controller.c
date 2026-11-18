#include "controller.h"
#include "motor.h"


pid_t PID;
extern motorControl_t motors[3];

extern float _Ts ; //10ms
extern float _Kp ; // proportional gain
extern float _Ki ; // integral gain
extern float _Kd ; // derivative gain
extern float _Tf ; //derivative filter time constant = aluláteresztő szűrő időállandója


const int table_size = 10;
const float table_pwm[] = {-100, -83.0174, -63.6241, -40.1157, -17.7504, 17.1016, 51.8858, 62.1894, 77.2195, 100};
//const float table_speed[] = {-16.2, -11.4, -8.54, -4, -1.4,  1.58, 4.7, 7.8, 11.45, 16.01};
const float table_speed[] = {-4.5, -3.0763, -2.3137, -1.2528,  -0.0003,  0.0003,  1.6854,  2.2122,  2.8073, 4.5};
const float maxSpeed = 4.5;

/**
 * @brief filtered PID controller for BLDC motor
 * @param motor_index Index of the target motor (0, 1, 2)
 */
void pidController(int motor_index){

    //pid_t *pid = &motors[motor_index].PID;
    float r = motors[motor_index].PID.targetOmega;
    float y = motors[motor_index].omega;
    float error = r - y;
    float dt = _Ts;

    
    if (fabs(r) < 0.001f) {
        motors[motor_index].PID.integralError = 0.0f;
        motors[motor_index].PID.output = 0.0f;
        //motors[motor_index].PID.prevError = 0.0f;
        //motors[motor_index].PID.prev_D = 0.0f;
        SetMotor(motor_index, 0.0f);
        return;
    }

    // P tag 
    float P = _Kp * error;

    // D tag
    float d_error = (error - motors[motor_index].PID.prevError) / dt;
    // alpha egy diszkrét elsőrendű szűrő (vagy exponenciális simító) együtthatója -> magas frekvenciájú zajszűrő
    float alpha = _Tf/(_Tf + dt);
    float D = alpha*motors[motor_index].PID.prev_D + (1-alpha) * _Kd * d_error;

    // I tag 
    //Anti windup
    float dummy_I = _Ki * motors[motor_index].PID.integralError;
    float dummyOutput = P + dummy_I + D; //
    
    bool upperLimit = (dummyOutput < 16) || (error < 0);
    bool bottomlimit = (dummyOutput > -16) || (error > 0);

    if(upperLimit && bottomlimit){
        motors[motor_index].PID.integralError += error * dt;
    }
    
    float I = _Ki * motors[motor_index].PID.integralError;
    float output = P + I + D;

    // Állapotok mentése
    motors[motor_index].PID.prevError = error;
    motors[motor_index].PID.prev_D = D;
    motors[motor_index].PID.prevOmega = y;
    motors[motor_index].PID.prevTarget = r;
    motors[motor_index].PID.output = output;
    

    //nemlin lookup Table meghívása
    float p_output = pointLookUptable(motor_index, output);
    motors[motor_index].PID.pwm = p_output;
    SetMotor(motor_index, p_output);
}

/**
 * @brief rescales the PID output to compensate for motor nonlinearity and deadzone
 * @param motor_index index of the target motor (0, 1, 2)
 * @param input calculated PID output 
 */
float pointLookUptable(int motor_index ,float input){
    
    float y0 = 0, y1= 0, x0= 0, x1 = 0;
    
    //Holt sáv kezelése
    if (input >= -0.0003 && input <= 0.0003) {
        return 0.0f;
    }

    if(input >= table_speed[table_size - 1]){
        return table_pwm[table_size - 1];
    }

    if(input <= table_speed[0]){
        return table_pwm[0];
    }

    //Selecting between the individual intervals
    //Inverz Point Lookup table
    for (int i = 0; i < table_size - 1; i++) {
        if (input >= table_speed[i] && input <= table_speed[i + 1]){
            x0 = table_speed[i];
            x1 = table_speed[i+1];
            y0 = table_pwm[i]; 
            y1 = table_pwm[i+1];

            return ((y0*(x1 - input) + y1*(input - x0)) / (x1 - x0));
        }
    }
    return 0;
}

/**
 * @brief Calculates the wheel speeds from the chassis linear and angular velocities for a 3-wheel omni robot
 * @param vbx Angular velocity of the robot around the Z axis 
 * @param vby Linear velocity of the robot along the X axis
 * @param wbz Linear velocity of the robot along the Y axis
 */
void motionVector(float vbx, float vby, float wbz){

    float u1 = ((-0.5*vbx / 42) - (sin60 *vby / 42) + (d * wbz)) / radius;
    float u2 = ((-0.5*vbx / 42) + (sin60 *vby / 42) + (d * wbz)) / radius;
    float u3 = (vbx / 42 + (d * wbz)) / radius;
    
    motors[0].PID.targetOmega = u1;
    motors[1].PID.targetOmega = u2;
    motors[2].PID.targetOmega = u3;
}

/**
 * @brief set new parameters for PID controller
 * @param Kp proportional gain = P erősítés
 * @param Ki integral gain = I erősítés 
 * @param Kd derivative gain  = D erősítés
 * @param Tf derivative filter time constant = aluláteresztő szűrő időállandója
 */
void setPIDParam(float Kp, float Ki, float Kd, float Tf){
    _Kp = Kp;
    _Ki = Ki;
    _Kd = Kd;
    _Tf = Tf; 
}

/**
 * @brief infinite loop for PID controller 
 * @param pvParameter akkor van rá szükség ha ugyanezt a task fgv-t több példányban kell meghívni
 *  más-más beállítással
 */
void PIDloop(void *pvParameter){
    TickType_t wakeUpTime;
    const TickType_t loopTime = pdMS_TO_TICKS(_Ts * 1000.0f);
    wakeUpTime = xTaskGetTickCount();

    for(int i = 0; i < 3; i++) {
        motors[i].PID.deltaTime = _Ts;
        motors[i].PID.integralError = 0.0f;
        motors[i].PID.prevError = 0.0f;
    }

    //infinite loop 
        for(;;){
            for(int i=0; i<3;i++){
                pidController(i);
            }
            vTaskDelayUntil(&wakeUpTime, loopTime);
        }
} 

/**
 * @brief set speed for one individual wheel
 * @param motorId id of the seleceted wheel id = [1 - 3]
 * @param speed desired speed [rad/s] 
 */    
void velocity(int motorId, float speed){
    int id = motorId-1;
    motors[id].PID.targetOmega = speed;
}