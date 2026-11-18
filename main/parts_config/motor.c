#include "motor.h"
#include "i2c_expander.h"
#include "controller.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846 // alt-227
#endif

static const char *TAG = "MOTOR";
motorControl_t motors[3];//motor1, motor2, motor3;

/**
 * @brief Initialize motor generator action
 * @param generator handle A
 * @param generator handle B
 * @param generator comparator A
 * @param generator comparator B
 */
static void GenActionConfig(mcpwm_gen_handle_t gena, mcpwm_gen_handle_t genb, mcpwm_cmpr_handle_t cmpa)
{
    // =========================================================================
    // GENERATOR A (Mot1_DrvA) - Szimmetrikus (Középre igazított) PWM
    // =========================================================================

    //1. KOMPARATOR alacsony   go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gena,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmpa, MCPWM_GEN_ACTION_LOW)));

    //2. Lefelé számolásnál (DOWN), amikor ismét eléri a 'cmpa' értéket -> MAGAS (3.3V) szintre vált
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gena,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, cmpa, MCPWM_GEN_ACTION_HIGH)));

    // =========================================================================
    // GENERATOR B (Mot1_DrvB) - Szimmetrikus (Középre igazított) PWM
    // =========================================================================
    



    mcpwm_dead_time_config_t deadtime = {
        .posedge_delay_ticks = 25,
        .negedge_delay_ticks = 0, //25 tick = 1 microsecundums szünet bekapcsolás előtt
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gena, gena, &deadtime));
    deadtime.posedge_delay_ticks = 0;
    deadtime.negedge_delay_ticks = 25;
    deadtime.flags.invert_output = true;
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gena, genb, &deadtime));
    // mcpwm_generator_set_dead_time(gena, genb, ...) 
    // hívás automatikusan a gena ellentétes (invertált) jelét kapcsolja a genb kimenetre
    // Dual Edge Symmetric Waveform - Complementary - ellenütemű jel 
    // pwmA jelből annak tökéletes ellentétje a pwmB   
                 
     
}

//motor driver A, motor driver B, motors[i = 0, 1, 2], timer(közös mindre)
/**
 * @brief setup motor parameters for initializing
 * @return esp_err_t ESP_OK on success, or an error code on failure
 */
static esp_err_t SetupMotorMcpwm(gpio_num_t mot_A_Gpio, gpio_num_t mot_B_Gpio, motorControl_t *motor, mcpwm_timer_handle_t timer){
    /**
     * Operátor (Operator) konfigurálása és létrehozása
     * Operátor egy logikai középszint, amely összeköti az időzítőt (Timer) a kimenetekkel
     * A group_id-nak MEG Kell egyeznie az átadott 'timer' csoportjával
    */ 
    mcpwm_oper_handle_t operator = NULL;
    mcpwm_operator_config_t operator_config = {
        // operator must be in the same group to the timer
        .group_id = 1,  
        
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operator));

    /**
     * Timer és Operátor összekapcsolása
     * Hozzárendeljük az operátorhoz a paraméterként timert (stoppert)
        ami alapján a PWM frekvenciája ketyegni fog
    */
    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator, timer));


    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    /**
     * Komparátorok konfigurálása 
     * Komparátor felel a kitöltési tényező (duty cycle) beállításáért
     * Folyamatosan összehasonlítja a saját értékét a Timer értékével
     * 'update_cmp_on_tez = true' (TEZ = Timer Equal Zero) biztosítja, hogy ha futás közben 
        sebességet váltasz, az új érték csak a következő PWM ciklus legelején lépjen életbe ==> megelőzi a motor rángatását (glitch)   
    */ 
    mcpwm_comparator_config_t comparator_config = { 
        .flags.update_cmp_on_tez = true, 
    };
    // Két komparátor létrehozása (A és B) a motorvezérlő H-hídjának két oldalához ==> (előre/hátra irány)
    ESP_ERROR_CHECK(mcpwm_new_comparator(operator, &comparator_config, &motor->cmpA));
    //ESP_ERROR_CHECK(mcpwm_new_comparator(operator, &comparator_config, &motor->cmpB));

    /**
     * Generátorok konfigurálása a fizikai kimenetekhez
     * Generátor átalakítja a komparátor logikai jelét fizikai feszültségszint-váltásokká
     * Hozzákötjük PWM vezérlést a mikrokontroller konkrét fizikai lábaihoz (GPIO)
    */
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
    // Generátorok létrehozása és az operátorhoz rendelése
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &generator_A_config, &genA));
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &generator_B_config, &genB));

    GenActionConfig(genA, genB,motor->cmpA);


    /**
     * Biztonsági beállítás
     * 0 sebességgel indul majd
    */ 
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(motor->cmpA, (halfPeriodTicks/2)));
    
    return ESP_OK;
}

/**
 * @brief Initialize motors
 * @return esp_err_t ESP_OK on success, or an error code on failure 
 */

esp_err_t InitMotor(){
    // Timer beállítása ==> közös mind a 3 motornak
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 1,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = TimerFreq, // timer sebessége : 1 MHz == 20 000 000 tick/s
        .period_ticks = periodTicks, //  1000 érték esetén : a counter elszámol 0 -> 1000ig így a PWM jel frekvenciaja f = resolution_hz / period_ticks = 25kHz 
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN, //.count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN,
        /*                                         szimmetrikus háromszög jel előállítása (Fel-Le)
            a counter elszámol 0 -> 20 000ig majd nullazodik => ez furesz jelet allit elo amibol lesz majd a
            alap beallitas szerint magas értékbe(3,3V) kezd majd vált alacsonyba (0V)
                ezt a generator beallotasanal lehet valtoztatni a .flags.invert_pwm = true parancsal 
            a kitoltesi tenyezo = komparator erteke / period_ticks 
                50% kitoltesi tenyezo => 10 000 ahol valt a negyszog jel erteket (komparator nezi mikor szamoltunk el 10 000ig)
            a komparator erteket a SetMotor fgvben alltjuk be a speed_MCPWM ertekkel ,hogy hol legyen a hatar ahol a negyszogjel erteket valt 
        */
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));
    ESP_LOGW(TAG, "Timer init");
    
    //bekötés
    SetupMotorMcpwm(Mot1_DrvA, Mot1_DrvB, &motors[0],timer);
    motors[0].serial_nmb = 1;
    SetupMotorMcpwm(Mot2_DrvA, Mot2_DrvB, &motors[1],timer);
    motors[1].serial_nmb = 2;
    SetupMotorMcpwm(Mot3_DrvA, Mot3_DrvB, &motors[2],timer);
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

/**
 * @brief Initalize encoder and counter unit for pulse counting
 * @param encoder  unit A port pin number
 * @param encoder  unit B port pin number
 * @param motor number, each one has its own encoder
 * @return esp_err_t ESP_OK on success, or an error code on failure
 * 
 */
//enkoder A pin, enkoder B pin , motor[i = 0, 1, 2]
static esp_err_t SetupEncoderPcnt(gpio_num_t enc_A, gpio_num_t enc_B, motorControl_t *motor){

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
    
    /**
     * 'A' csatorna konfigurálása
     * Encoder egyik fizikai jelét (enc_A) állítjuk be az élek (felfutó/lefutó) figyelésére
       míg a másik jelét (enc_B) a logikai szint (magas/alacsony) megállapítására használjuk
     */
    ESP_LOGW(TAG, "PCNT csatorna A");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = enc_A,
        .level_gpio_num = enc_B, 
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(motor->encoder, &chan_a_config, &pcnt_chan_a));//pcnt_unit, &chan_config, &pcnt_chan
    
    /**
     * 'B' csatorna konfigurálása
     * 'enc_B' lábon figyeljük az éleket, és az 'enc_A' lábon a logikai szintet ==> Kvadratúra kódolás
     */
    ESP_LOGW(TAG, "PCNT csatorna B");
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = enc_B,
        .level_gpio_num = enc_A,
    };    
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(motor->encoder,&chan_b_config,&pcnt_chan_b));





    //GPIO pins 34-39 are input-only. These pins do not feature an output driver or internal pull-up/pull-down circuitry.
    /**
     * Hardveres felhúzóellenállások kikapcsolása
    if(motor->serial_nmb<2){ 
        gpio_pullup_dis(enc_A);
        gpio_pullup_dis(enc_B);
    }
    */
   
    /*
    kvadratúra (irányfüggő 4X-es) logika beállítás

       chan_A_config : az A láb értékét állítja és a B láb állapotát figyeli
       set_edge_action : beallatja mit csinal elváltásra
       Parameters:
        chan – [in] PCNT channel handle created by pcnt_new_channel() : chan_A_config A láb erteket allitja 
        pos_act – [in] Action on posedge signal : 0-> 1 felfuto elre csökkenti a szamlalot
        neg_act – [in] Action on negedge signal : 1-> 0 lefuto elre noveli

       set_level_action
       Parameters:
        chan – [in] PCNT channel handle created by pcnt_new_channel() : chan_A_config B lab erteket nezi ha B magas akkor KEEP ha lacsony akkor Inverse
        high_act – [in] Action on high level signal KEEP -> Decrease
        low_act – [in] Action on low level signal   INVERSE -> ~(decrase) = Increase
    */
        
        
        ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
        // decrease the counter on rising edge, increase the counter on falling edge
        ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
        ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
        ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

        /**
         * Számláló egység bekapcsolása és indítása
         * hardveres egység engedélyezése és számláló nullázása és számláló indítása
         */
        ESP_LOGI(TAG, "enable pcnt unit");
        ESP_ERROR_CHECK(pcnt_unit_enable(motor->encoder));
        ESP_LOGI(TAG, "clear pcnt unit");
        ESP_ERROR_CHECK(pcnt_unit_clear_count(motor->encoder));
        ESP_LOGI(TAG, "start pcnt unit");
        ESP_ERROR_CHECK(pcnt_unit_start(motor->encoder));
    return ESP_OK;
}

/**
 * @brief Initalizes all 3 encoder
 * @return esp_err_t ESP_OK on success, or an error code on failure
 */
esp_err_t InitEncoder(){
    ESP_LOGI(TAG,"Encoders initialization");
    ESP_LOGW(TAG,"Enkoder 1");
    SetupEncoderPcnt(Mot1_EncA, Mot1_EncB, &motors[0]);
    ESP_LOGW(TAG,"Enkoder 2");
    SetupEncoderPcnt(Mot2_EncA, Mot2_EncB, &motors[1]);
    ESP_LOGW(TAG,"Enkoder 3");
    SetupEncoderPcnt(Mot3_EncA, Mot3_EncB, &motors[2]);

    esp_err_t error = InitEncTimer();
    if(error != ESP_OK) ESP_LOGE(TAG, "Omega timer init failed");

    return ESP_OK;
}

/**
 * @brief Calculates the angular velocity (omega) in [rad/s] for all wheel motors.
 *
 * Reads encoder pulse counts via PCNT, calculates the delta over a 20 ms sampling 
 * interval (50 Hz), and converts the pulse frequency to angular speed.
 *
 * @note Marked with IRAM_ATTR for fast execution from IRAM, making it safe for timer ISRs.
 */
void IRAM_ATTR OmegaCalculation(){
    static int64_t last_time_us = 0;
    int64_t cnt_time_us = esp_timer_get_time();
    if(0 == last_time_us){
        last_time_us = cnt_time_us;
        return;
    }
    float delta_time = (float)((cnt_time_us-last_time_us)/1000000.0); // 1s = 1 000 000 us
    last_time_us = cnt_time_us;
    if(delta_time <= 0.0001) return; // safety if delta is too small
    for(int i = 0; i < 3;i++){
        int currentValue = 0;
        pcnt_unit_get_count(motors[i].encoder, &currentValue);
        int lastDeltaEncoder = currentValue - motors[i].lastValue;
        motors[i].lastValue = currentValue;

        //Túlcsordulás védelem
        if(lastDeltaEncoder > High_LIMIT - 100){
            lastDeltaEncoder = lastDeltaEncoder - 2 * (High_LIMIT-100);
        }
        else if(lastDeltaEncoder < Low_LIMIT + 100){
            lastDeltaEncoder = lastDeltaEncoder + 2*(Low_LIMIT+100);
        }

        // Pulses Per Second  (másodpercenkénti lépésszám) 
        // Valós eltelt idő alapján számolt PPS
        float PPS =  (float)lastDeltaEncoder / delta_time;  
        motors[i].omega = (PPS/PPWR) * 2.0 * M_PI; // [rad/s]                                                                                     
    }
}

/**
 * @brief Initialize Timer for speed calculation 
 * @return  esp_err_t ESP_OK on success, or an error code on failure
 */
esp_err_t InitEncTimer(){
    ESP_LOGI(TAG,"Omega timer init");
    const esp_timer_create_args_t omega_timer_args = {
        // calls current_omega_calculation
        .callback =&OmegaCalculation, 
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

/**
 * @brief Sets the wheel speed for the specified motor.
 * @param[in] motor_i Motor ID (0, 1, or 2).
 * @param[in] speed   Desired speed in the range [-100, 100].
 * @note Values outside [-100, 100] are automatically clamped to the nearest limit.
 */
void SetMotor(int motor_i, float speed){
    // 1. Bemenet szigorú korlátozása a [-100.0, 100.0] tartományra
    if (speed > 100.0f) speed = 100.0f;
    if (speed < -100.0f) speed = -100.0f;

    if (motors[motor_i].cmpA == NULL) {
        ESP_LOGE("MOTOR_DEBUG", "Motor %d cmpA handle NULL!", motor_i);
        return;
    }

    // 2. Ticks kiszámítása (312.5 az alapérték 0% sebességnél)
    // speed = -100% -> ticks = 0
    // speed =    0% -> ticks = 250
    // speed = +100% -> ticks = 500
    float ticks = (1.0f + (speed / 100.0f))*(halfPeriodTicks/2);

    // 3. Biztonsági lefogás (Clamping) [0, periodTicks] közé
    if (ticks < 0.0f) ticks = 0.0f;
    if (ticks > periodTicks) ticks = periodTicks;

    uint32_t comparator_val = (uint32_t)ticks;

    //ESP_LOGI("MOTOR_DEBUG", "Motor %d | Speed: %.1f%% | Compare Val: %uint32", motor_i, speed, comparator_val);

    // 4. Komparátor érték beállítása
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(motors[motor_i].cmpA, comparator_val));
}
/**
 * @brief checks if there any fault on the motor or motors legs(pin)
 * @warning if the current is too big
 */
 
void CheckFaults(){
    if((ReadI2C() & DRV1_FAULT) == 0)
        ESP_LOGE(TAG,"Hiba a motoron (túláram v ) az 1-es modulon");
    if((ReadI2C() & DRV2_FAULT) == 0)
        ESP_LOGE(TAG,"Hiba a motoron (túláram v ) az 2-es modulon");  
}
