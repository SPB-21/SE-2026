#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/timer.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/ledc.h"

//bandera de cambio de porcentaje de display
static volatile int contador = 0; //banderas para los botones

//defino estados iniciales de las etapas
bool E1=true;
bool E2=false;
//estado del motor
bool Motor_Movieminto=false;
bool contador_cambio_de_giro=false;

//-------------------entradas---------------
//pines pusadores
#define horario 23
#define antihorario 5
//estado botones
bool last_button_state_horario=1;
bool last_button_state_antihorario=1;

//pin potenciometro(gpio 34)
//#define potenciometro 34
int adc_raw = 0;

//-------------------salidas---------------
//pines leds
#define led_pin_verde 1 // led verde 
#define led_pin_rojo 3  //led rojo


//pines velocidad y direccion
#define vel 18 //pwm
#define dir 13



// --- PINES SEGMENTOS ---
#define a 12
#define b 14
#define c 27
#define d 26
#define e 25
#define f 33
#define g 32

// --- PINES DISPLAYS 
#define D1 4
#define D2 16
#define D3 17  


// --- VARIABLE GLOBAL ---
volatile int numero_a_mostrar = 0;  //solo actualizo esta variable para mostrar el numero que quiero

// Tabla de números (Cátodo Común)

uint8_t tabla[10][7] = {
 {1,1,1,1,1,1,0},
 {0,1,1,0,0,0,0},
 {1,1,0,1,1,0,1},
 {1,1,1,1,0,0,1},
 {0,1,1,0,0,1,1},
 {1,0,1,1,0,1,1},
 {1,0,1,1,1,1,1},
 {1,1,1,0,0,0,0},
 {1,1,1,1,1,1,1},
 {1,1,1,1,0,1,1}
};

gpio_num_t seg_pins[7] = {a, b, c, d, e, f, g};
gpio_num_t dig_pins[3] = {D1, D2, D3}; // Array de dígitos

//funcion para setear pines del display
void set_segments(int num) {
    for(int i=0; i<7; i++) {
        gpio_set_level(seg_pins[i], tabla[num][i]);
    }
}

// --- INTERRUPCIÓN DEL TIMER (Multiplexado de 3 dígitos) ---
bool IRAM_ATTR timer_callback_multiplex(void* arg) {
    contador++;
    static int digit_index = 0; // Va de 0 a 2 ,static solo me la incializa una vez y cuando vuelva y entre ignora esta linea y toma su valor anterior

    // 1. Apagar TODOS los displays
    for(int i=0; i<3; i++){ 
        gpio_set_level(dig_pins[i], 0);
    }

    // 2. Descomponer el número (ej: 123)
    int valores[3];

    valores[0] = (numero_a_mostrar / 100) % 10;  // Cientos
    valores[1] = (numero_a_mostrar / 10) % 10;   // Decenas
    valores[2] = numero_a_mostrar % 10;          // Unidades

    // 3. Cargar segmentos y prender el dígito actual
    set_segments(valores[digit_index]);
    gpio_set_level(dig_pins[digit_index], 1);

    // 4. Rotar al siguiente dígito (0->1->2->0...)
    digit_index++;
    if(digit_index > 2) {
        digit_index = 0; 
    }
return false;
}


void app_main() {
    //--------------configuracion pines de entrada------------------
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << horario) | (1ULL << antihorario) ,  //asigno tres entradas
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&in_cfg); //mando configuracion

   
    // --- CONFIGURACIÓN DE PINES salida
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL<<a)|(1ULL<<b)|(1ULL<<c)|(1ULL<<d)|(1ULL<<e)|(1ULL<<f)|(1ULL<<g)|
                        (1ULL<<D1)|(1ULL<<D2)|(1ULL<<D3)|(1ULL<<led_pin_rojo)|(1ULL<<led_pin_verde)|(1ULL<<dir),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 2. Configuración del Timer
    timer_config_t config = {
        .divider = 80,                 // 1 tick = 1 us (80MHz / 80)
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,    // ¡activo la alarma!
        .auto_reload = true            // Que se reinicie solo
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    // 3. Fijamos el valor de la alarma (1 segundo = 1,000,000 us) 
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000);//cada milisegundo la llama


    
    // 4. Registramos la función que atenderá la interrupción
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_callback_multiplex, NULL,0);

    // 5. Habilitamos la interrupción
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

    // 6. Arrancamos
    timer_start(TIMER_GROUP_0, TIMER_0);

    // ==============================
    // 1. Crear handler del ADC
    // ==============================
    adc_oneshot_unit_handle_t adc1_handle;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,  // Usamos ADC1
    };

    // Inicializa el ADC en modo oneshot
    adc_oneshot_new_unit(&init_config, &adc1_handle);


    // ==============================
    // 2. Configurar canal
    // ==============================
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // Resolución (12 bits típicamente) (0 - 4095)
        .atten = ADC_ATTEN_DB_12,         // Hasta ~3.3V
    };

    // Canal 6 GPIO34 
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &chan_config);

    //--------------------configuracion pwm----------------------------

        // 1. Configuración del TIMER del LEDC
    // El timer define la frecuencia y la precisión (resolución) del PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_12_BIT, // Resolución de 0 a 4095 (2^12 - 1)
        .freq_hz          = 5000,              // Frecuencia de 5kHz
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    // 2. Configuración del CANAL del LEDC
    // El canal conecta el timer con un pin físico (GPIO)
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = vel,                  // Pin de salida: GPIO 18
        .duty           = 0,                   // Brillo inicial (0%)
        .hpoint         = 0,
    };
    ledc_channel_config(&ledc_channel);


    // --- BUCLE PRINCIPAL  ---
    while(1) {

        
        if(E1){
        //condiciones iniciales
        contador_cambio_de_giro=false;

        while (E1){  //etapa1 horario
        //seguridad etapas
        E1=true;
        E2=false;
        // estado del motor
        if (adc_raw!=0)
        {
           Motor_Movieminto=true; 
        }else{
          Motor_Movieminto=false;   
        }
        //estado leds
        gpio_set_level(led_pin_verde, 1);
        gpio_set_level(led_pin_rojo, 0);

        //direccion
         if (Motor_Movieminto==true && contador_cambio_de_giro==false)
        {
        //aplicar a pwm
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0); //aplico cambio
        vTaskDelay(pdMS_TO_TICKS(500));
        //cambio el sentido de giro 
        gpio_set_level(dir, 0);
        //aplico rampa
        for (int i = 0; i < adc_raw; i++){
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, i);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0); //aplico cambio  
        }
        contador_cambio_de_giro=true;
        }else{
        gpio_set_level(dir, 0);
        }
        

        //lectura potenciometro
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw);
        if (contador>=300)//cada 0.3 segundo me cambia la lectura para quitar valores fantasmas
        {
          numero_a_mostrar=(100*adc_raw)/4095;//regla de 3, 0-100%
          contador=0;
        }
        
        //aplicar a pwm
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, adc_raw);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0); //aplico cambio

        //----------cambio de etapa-------------------------
        bool current_button_state_antihorario = gpio_get_level(antihorario);                             
        
        if (last_button_state_antihorario == 1 && current_button_state_antihorario == 0 ) {  
                E1=false;
                E2=true;
                vTaskDelay(pdMS_TO_TICKS(200));                     //delay por el rebote del boton
            }

            last_button_state_antihorario = current_button_state_antihorario;


        }
        }       

        if(E2){
        //condiciones iniciales
        contador_cambio_de_giro=false;
        
        while (E2){ //etapa 2 antihorario
        //seguridad etapas
        E1=false;
        E2=true;
        // estado del motor
        if (adc_raw!=0)
        {
           Motor_Movieminto=true; 
        }else{
          Motor_Movieminto=false;   
        }
        //estado leds
        gpio_set_level(led_pin_verde, 0);
        gpio_set_level(led_pin_rojo, 1);

        //direccion
         if (Motor_Movieminto==true && contador_cambio_de_giro==false)
        {
        //aplicar a pwm
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0); //aplico cambio
        vTaskDelay(pdMS_TO_TICKS(500));
        //cambio el sentido de giro 
        gpio_set_level(dir, 1);
        //aplico rampa
        for (int i = 0; i < adc_raw; i++){
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, i);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0); //aplico cambio     
        }
        contador_cambio_de_giro=true;
        
        }else{
        gpio_set_level(dir, 1);
        }
        

        //lectura potenciometro
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw);
        if (contador>=300)//cada 0.3 segundo me cambia la lectura para quitar valores fantasmas
        {
          numero_a_mostrar=(100*adc_raw)/4095;//regla de 3, 0-100%
          contador=0;
        }
        //aplicar a pwm
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, adc_raw);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0); //aplico cambio
        
        
        //----------cambio de etapa-------------------------
        bool current_button_state_horario = gpio_get_level(horario);                             
        
        if (last_button_state_horario == 1 && current_button_state_horario == 0 ) {  
                E1=true;
                E2=false;
                vTaskDelay(pdMS_TO_TICKS(200));                     //delay por el rebote del boton
            }

            last_button_state_horario = current_button_state_horario;
        }


        }
        
    }
}
