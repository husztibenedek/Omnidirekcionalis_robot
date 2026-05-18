#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "driver/pulse_cnt.h"
#include "motor.h"
#include "i2c_expander.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846 // alt-227
#endif

static const char *TAG = "MOTOR";
motorControl_t motors[3];//motor1, motor2, motor3;

//
static void gen_action_config(mcpwm_gen_handle_t gena, mcpwm_gen_handle_t genb, mcpwm_cmpr_handle_t cmpa, mcpwm_cmpr_handle_t cmpb)
{
    //Single Edge Asymmetric PWM (egyélű, aszimmetrikus) 
    // go high on counter empty 
    // PWM Jelalak beállítása =>High induláskor, Low amikor eléri a komparátor értékét
    //START magas
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gena,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    //KOMPARATOR alacsony      
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gena,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmpa, MCPWM_GEN_ACTION_LOW)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(genb,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(genb,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmpb, MCPWM_GEN_ACTION_LOW)));

    //Dual Edge Symmetric Waveform - Complementary - ellenütemű jel 
    //Dead Time (DT) modul
    //pwmA jelből annak tökéletes ellentétje a pwmB                
     
}

//motor driver A, motor driver B, motors[i = 0, 1, 2], timer(közös mindre)
static esp_err_t setupMotorMcpwm(gpio_num_t mot_A_Gpio, gpio_num_t mot_B_Gpio, motorControl_t *motor, mcpwm_timer_handle_t timer){
      
    mcpwm_oper_handle_t operator = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 1,  // operator must be in the same group to the timer
        
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operator));

    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator, timer));

    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    
    mcpwm_comparator_config_t comparator_config = { 
        .flags.update_cmp_on_tez = true, 
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(operator, &comparator_config, &motor->cmpA));
    ESP_ERROR_CHECK(mcpwm_new_comparator(operator, &comparator_config, &motor->cmpB));

    mcpwm_gen_handle_t genA = NULL; 
    mcpwm_generator_config_t generator_A_config = {
        .gen_gpio_num = mot_A_Gpio,
    // .flags.invert_pwm = true, 
    }; 
    mcpwm_gen_handle_t genB = NULL;
    mcpwm_generator_config_t generator_B_config = {
        .gen_gpio_num = mot_B_Gpio,
    // .flags.invert_pwm = true,
    }; 
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &generator_A_config, &genA));
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &generator_B_config, &genB));

    gen_action_config(genA, genB,motor->cmpA, motor->cmpB);


    // 0 sebességgel indul majd
    mcpwm_comparator_set_compare_value(motor->cmpA, 0);
    mcpwm_comparator_set_compare_value(motor->cmpB, 0);

    return ESP_OK;
}

esp_err_t initMotor(){
    // Timer beállítása ==> közös mind a 3 motornak
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 1,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // timer sebessége : 1 MHz == 1 000 000 tick/s
        .period_ticks = 1000, // 20 000 érték esetén : a counter elszámol 0 -> 20 000ig így a PWM jel frekvenciaja f = resolution_hz / period_ticks = 50Hz 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP, //.count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN,
        /*                                         szimmetrikus háromszög jel előállítása (Fel-Le)
            a counter elszámol 0 -> 20 000ig majd nullazodik => ez furesz jelet allit elo amibol lesz majd a
            alap beallitas szerint magas értékbe(3,3V) kezd majd vált alacsonyba (0V)
                ezt a generator beallotasanal lehet valtoztatni a .flags.invert_pwm = true parancsal 
            a kitoltesi tenyezo = komparator erteke / period_ticks 
                50% kitoltesi tenyezo => 10 000 ahol valt a negyszog jel erteket (komparator nezi mikor szamoltunk el 10 000ig)
            a komparator erteket a setMotor fgvben alltjuk be a speed_MCPWM ertekkel ,hogy hol legyen a hatar ahol a negyszogjel erteket valt 




        */

    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));
    ESP_LOGW(TAG, "Timer init");
    
    //bekötés
    setupMotorMcpwm(Mot1_DrvA, Mot1_DrvB, &motors[0],timer);
    motors[0].serial_nmb = 1;
    setupMotorMcpwm(Mot2_DrvA, Mot2_DrvB, &motors[1],timer);
    motors[1].serial_nmb = 2;
    setupMotorMcpwm(Mot3_DrvA, Mot3_DrvB, &motors[2],timer);
    motors[2].serial_nmb = 3;
    ESP_LOGW(TAG, "Bekotes init");

    //indítás
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));

    //MCPWM_TIMER_START_NO_STOP => elszámol  0 - 20k és újraindul 0-ról 
    ESP_LOGW(TAG, "Timer enable");
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    ESP_LOGW(TAG, "Timer Bekotve");

    return ESP_OK;
}


//enkoder A pin, enkoder B pin , motor[i = 0, 1, 2]
static esp_err_t setup_encoder_pcnt(gpio_num_t enc_A, gpio_num_t enc_B, motorControl_t *motor){

    //counter unit
     ESP_LOGI(TAG, "PCNT unit");
     pcnt_unit_config_t unit_config = {
        .high_limit =  High_LIMIT,
        .low_limit = Low_LIMIT,
     };
    motor->encoder = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &motor->encoder));

    
    //glitch szűrő bekapcsolása ==> mikroszekundumos zajok szűrése
    //minden tuske ami rövidebb 1000ns-ál => elektromos zaj a szenkefes motorbol, nem szamolja bele PCNT  
    
    ESP_LOGI(TAG, "glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(motor->encoder,&filter_config));
    
    
    ESP_LOGW(TAG, "PCNT csatorna A");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = enc_A,
        .level_gpio_num = enc_B, 
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(motor->encoder, &chan_a_config, &pcnt_chan_a));//pcnt_unit, &chan_config, &pcnt_chan
    

    ESP_LOGW(TAG, "PCNT csatorna B");
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = enc_B,
        .level_gpio_num = enc_A,
    };    
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(motor->encoder,&chan_b_config,&pcnt_chan_b));





    //GPIO pins 34-39 are input-only. These pins do not feature an output driver or internal pull-up/pull-down circuitry.
    /*
    if(motor->serial_nmb<2){ 
        gpio_pullup_dis(enc_A);
        gpio_pullup_dis(enc_B);
    }
    */

    //  kvadratúra (irányfüggő 4X-es) logika beállítás

    //    chan_A_config : az A káb értékét állítja és a B láb állapotát figyeli
    //    set_edge_action : beallatja mit csinal elváltásra
    //    Parameters:
    //       chan – [in] PCNT channel handle created by pcnt_new_channel() : chan_A_config A láb erteket allitja 
    //        pos_act – [in] Action on posedge signal : 0-> 1 felfuto elre csökkenti a szamlalot
    //        neg_act – [in] Action on negedge signal : 1-> 0 lefuto elre noveli

    //    set_level_action
    //    Parameters:
    //        chan – [in] PCNT channel handle created by pcnt_new_channel() : chan_A_config B lab erteket nezi ha B magas akkor KEEP ha lacsony akkor Inverse
    //        high_act – [in] Action on high level signal KEEP -> Decrease
    //       low_act – [in] Action on low level signal   INVERSE -> ~(decrase) = Increase

        
        
        ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
        // decrease the counter on rising edge, increase the counter on falling edge
        ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
        ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

        ESP_LOGI(TAG, "enable pcnt unit");
        ESP_ERROR_CHECK(pcnt_unit_enable(motor->encoder));
        ESP_LOGI(TAG, "clear pcnt unit");
        ESP_ERROR_CHECK(pcnt_unit_clear_count(motor->encoder));
        ESP_LOGI(TAG, "start pcnt unit");
        ESP_ERROR_CHECK(pcnt_unit_start(motor->encoder));
    return ESP_OK;
}

esp_err_t initEncoder(){
    ESP_LOGI(TAG,"Encoders initialization");
    ESP_LOGW(TAG,"Enkoder 1");
    setup_encoder_pcnt(Mot1_EncA, Mot1_EncB, &motors[0]);
    ESP_LOGW(TAG,"Enkoder 2");
    setup_encoder_pcnt(Mot2_EncA, Mot2_EncB, &motors[1]);
    ESP_LOGW(TAG,"Enkoder 3");
    setup_encoder_pcnt(Mot3_EncA, Mot3_EncB, &motors[2]);

    esp_err_t error = initEncTimer();
    if(error != ESP_OK) ESP_LOGE(TAG, "Omega timer init failed");

    return ESP_OK;
}

void IRAM_ATTR current_omega_calculation(){
    for(int i = 0; i < 3;i++){
        int currentValue = 0;
        pcnt_unit_get_count(motors[i].encoder, &currentValue); //
        int lastDeltaEncoder = currentValue - motors[i].lastValue;
        motors[i].lastValue = currentValue;

        float PPS =  lastDeltaEncoder *50;  // Pulses Per Second  (másodpercenkénti lépésszám) , 50 <= 1sec = 1000 ms
                                            // omega timer 20ms-ént mér 1000/20 = 50 
        float freq;
        if(0 == PPS) { motors[i].omega = 0.0;}
        else { 
            freq = PPS/PPWR; 
            motors[i].omega = 2.0 * M_PI * freq ;
        }
    }
}

esp_err_t initEncTimer(){
    ESP_LOGI(TAG,"Omega timer init");
    const esp_timer_create_args_t omega_timer_args = {
        .callback =&current_omega_calculation, // ezt fogja hívni
        .name = "omega_timer"
    };
    esp_timer_handle_t omega_timer;
    ESP_ERROR_CHECK(esp_timer_create(&omega_timer_args, &omega_timer));
    /*  
        20ms = 20 000 mikrosec
        periodus allitas és start
    */ 
    ESP_ERROR_CHECK(esp_timer_start_periodic(omega_timer, 20000)); 

    return ESP_OK;
}

void setMotor(int motor_i, int speed){
    // ha tul nagy v tul kicsi ertek jon
    if(100 < speed ) speed = 100;
    if(-100 > speed) speed = -100;

    // sebesség átváltása 8 bites skáláról (255) => 1000-es MCPWM Timer skálára
    uint32_t speed_MCPWM = 0;
    if(0 < speed){
        speed_MCPWM = (uint32_t)((abs(speed) / 100.0) * 1000.0); //PWM kitoltesi tenyeojet allitjuk be
        // elore : DrvA = 0, DrvB = speed
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpA, 0);
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpB, speed_MCPWM);
    }
    else if(0 > speed){ 
        speed_MCPWM = (uint32_t)((abs(speed) / 100.0) * 1000.0); //PWM kitoltesi tenyeojet allitjuk be
        // hatra : Drva = speed, DrvB = 0
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpA, speed_MCPWM);
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpB, 0);
    }
    else{  // stop : speed = 0 
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpA, 0);
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpB, 0);

        //Forditott PWM jel eseten
        /*
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpA, 1000);
        mcpwm_comparator_set_compare_value(motors[motor_i].cmpB, 1000);
        */
    }
    //checkFaults();
}


void checkFaults(){
    if((readbyte() & DRV1_FAULT) == 0)
        ESP_LOGE(TAG,"Hiba a motoron (túláram v ) az 1-es modulon");
    if((readbyte() & DRV2_FAULT) == 0)
        ESP_LOGE(TAG,"Hiba a motoron (túláram v ) az 2-es modulon");  
}
