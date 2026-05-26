#include <stdio.h>
#include <string.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/timer.h"
#include "driver/ledc.h"
#include "driver/uart.h" //driver uart


#define UART_PORT UART_NUM_0 // defino el uart que voy a usar(en este caso el del pc) 

#define pwm_led 4 //pwm pin

#define IN1 GPIO_NUM_18
#define IN2 GPIO_NUM_19
#define IN3 GPIO_NUM_21
#define IN4 GPIO_NUM_22
#define pin_lamp GPIO_NUM_23

volatile int dir=0;
volatile int contador=0;

void step(int a, int b, int c, int d) {
    gpio_set_level(IN1, a); gpio_set_level(IN2, b);
    gpio_set_level(IN3, c); gpio_set_level(IN4, d);
}


bool IRAM_ATTR timer_callback(void* arg) {
    if (dir==1){
        if (contador==0){
           step(1, 0, 0, 1); 
           contador++;
        }else if (contador==1){
           step(0, 1, 0, 1);
           contador++;
        }else if (contador==2){
           step(0, 1, 1, 0); 
           contador++;
        }else if (contador==3){
         step(1, 0, 1, 0);
         contador=0;  
        }
        
        
    }

    if (dir==0){

        if (contador==0){
          step(1, 0, 1, 0); 
          contador++;
        }else if (contador==1){
           step(0, 1, 1, 0);
           contador++;
        }else if (contador==2){
           step(0, 1, 0, 1);
           contador++; 
        }else if (contador==3){
         step(1, 0, 0, 1);
         contador=0;   
        }
    
    }
    
    return false; 
}

void app_main(void){
    //-------------uart------------------------------

    uart_config_t uart_config = {
        .baud_rate = 9600,  //poner en platformio.ini --> monitor_speed = 9600, es decir, velocidad del computador debe ser la misma del ESP32
        .data_bits = UART_DATA_8_BITS, // utilizo 8 bits por que voy a enviar un caracter
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // desactiva RTS/CTS ,O sea: no hay control automático para evitar que se llene el buffer
        .source_clk = UART_SCLK_DEFAULT
    };
    uart_param_config(UART_PORT, &uart_config); // & para mandar de una vez o lo convierte a pointer
    uart_driver_install(UART_PORT, 1024, 1024, 0, NULL, 0); // Inicializa el driver UART con buffers de 1024 bytes para RX y TX, el tamaño del buffer depende de lo que reciba o envie
                                                            // sin uso de cola de eventos (modo básico sin interrupciones manejadas por el usuario)

//------------pwm-----------------------------------
    // 1. Configuración del TIMER del LEDC
    // El timer define la frecuencia y la precisión (resolución) del PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_12_BIT, // Resolución de 0 a 4095 (2^12 - 1)
        .freq_hz          = 2000,              // Frecuencia de 2kHz
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
        .gpio_num       = pwm_led,                  // Pin de salida: GPIO 4
        .duty           = 0,                   // Brillo inicial (0%)
        .hpoint         = 0,
    };
    ledc_channel_config(&ledc_channel);


    //--------------inicializar pines salidas---------------------------
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << IN1) | (1ULL << IN2) | (1ULL << IN3) | (1ULL << IN4) | (1ULL << pin_lamp) ,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&out_cfg);  //mando config

    //------------interrupcion timer-------------------------
    timer_config_t config = {
        .divider = 80,                 // 1 tick = 1 us (80MHz / 80)
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,    // ¡activar alarma!
        .auto_reload = true            // Que se reinicie solo
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);


    // 4. Registramos la función que atenderá la interrupción
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_callback, NULL,0);

    // 5. Habilitamos la interrupción
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

/* //  Fijamos el valor de la alarma (1 segundo = 1,000,000 us)
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000000ULL);

    // 6. Arrancamos
    timer_start(TIMER_GROUP_0, TIMER_0);
    timer_pause(TIMER_GROUP_0, TIMER_0); // detiene el timer
 */



    // ------------------Inicializar ADC One-Shot----------------------------------
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_config, &adc1_handle);

    // Configuración compartida para ambos canales
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    // Configurar canal 6 (Temperatura) y canal 7 (Luz)
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &chan_config);
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &chan_config);

    // 2. Configurar calibración LINEAL (La misma para ambos si usan la misma atenuación)
    adc_cali_handle_t adc_cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);

    // 3. Variables de lectura
    int adc_raw_temp = 0;
    int volt_temp_mv = 0;
    
    int adc_raw_light = 0;
    //variables temperatura
    int temp=0;
    int tem_control=0;
     uint8_t data[12] ; //uint8 porque con data voy a leer bytes (8 bits)
    //variables luz
    int luz_porcentaje=0;
    int luz_porcentaje_leds=0;
    //variable paso
    bool paso=true;

    while (1)
    { 
        while (paso){
        
        int len = uart_read_bytes(UART_PORT, data, 11,portMAX_DELAY);
        
        if (len == 11) {
        data[11] = '\0'; // ¡ESTO ES LO MÁS IMPORTANTE! Le pongo un "stop" al string y digo que la posicion 11 es donde corta (el indice empiza desde cero ose que manda 11 caracteres)
                         //se necesita por la funcion sscanf por que ella para justo cuando encuentra este caracter                   
        
        
        int resultado = sscanf((char*)data, "SET_TEMP:%d", &tem_control);//devuelve 1 si encrontro un numero si no devuelve un 0 
                                                                         //(char*) para que me convierta los bytes de datas en char
                                                                         //%d un número entero en base 10 (tipo int)
        if (resultado == 1) {
            char buffer_salida[50]; // Creamos un buffer temporal para el mensaje
            
            // snprintf formatea el texto y el número en el buffer
            //calculo el tamaño y asigno el mensaje a buffer de salida
            //convierto un int a texto
            int len_msg = snprintf(buffer_salida, sizeof(buffer_salida), "Temperatura recibida: %d\n", tem_control);
            
            // Ahora enviamos el buffer ya formateado
            uart_write_bytes(UART_PORT, buffer_salida, len_msg);
            paso=false;
            
        } else {
            // Para mensajes fijos
            char *msg = "Error: Formato de comando incorrecto.\n";
            uart_write_bytes(UART_PORT, msg, strlen(msg));
            paso=true;
        }
      }
       
        }
        
        // Leer Temperatura
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw_temp);
        adc_cali_raw_to_voltage(adc_cali_handle, adc_raw_temp, &volt_temp_mv);
        temp=volt_temp_mv/10;
        // Leer Luz
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_7, &adc_raw_light);
        luz_porcentaje=(100*adc_raw_light)/4095;
        
        //-------------mandar temperatura uart-------------
        char buffer_salida[50]; // Creamos un buffer temporal para el mensaje
            
            // snprintf formatea el texto y el número en el buffer
            int len_msg1 = snprintf(buffer_salida, sizeof(buffer_salida), "Temperatura: %d\n", temp);
            
            // Ahora enviamos el buffer ya formateado
            uart_write_bytes(UART_PORT, buffer_salida, len_msg1);

            // snprintf formatea el texto y el número en el buffer
            int len_msg2 = snprintf(buffer_salida, sizeof(buffer_salida), "Temperatura_control: %d\n", tem_control);
            
            // Ahora enviamos el buffer ya formateado
            uart_write_bytes(UART_PORT, buffer_salida, len_msg2);

        //-------------mandar LUZ uart-------------
        
            
            // snprintf formatea el texto y el número en el buffer
            int len_msg3 = snprintf(buffer_salida, sizeof(buffer_salida), "porcentaje luz ambiente: %d\n", luz_porcentaje);
            
            // Ahora enviamos el buffer ya formateado
            uart_write_bytes(UART_PORT, buffer_salida, len_msg3);

            // snprintf formatea el texto y el número en el buffer
            int len_msg4 = snprintf(buffer_salida, sizeof(buffer_salida), "porcentaje luz leds: %d\n",luz_porcentaje_leds);
            
            // Ahora enviamos el buffer ya formateado
            uart_write_bytes(UART_PORT, buffer_salida, len_msg4);

       //-----------------------------control de temperatura---------------------------------------
        
        if ( temp<tem_control-1){ // frio esta por debajo de la temperatura de control
         gpio_set_level(pin_lamp, 1); 
         dir=1;
         timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 10000ULL); // seteo la alarma a 10.000 micro para que en un seg me de 100 pasos 
         timer_start(TIMER_GROUP_0, TIMER_0);

        }else if (temp>=tem_control-1 && temp<=tem_control+1){  //si esta en la temperatura de control un grado por arriba y por debajo como holgura
        gpio_set_level(pin_lamp, 0);                                
        gpio_set_level(IN1, 0);
        gpio_set_level(IN2, 0);
        gpio_set_level(IN3, 0);
        gpio_set_level(IN4, 0);
        timer_pause(TIMER_GROUP_0, TIMER_0); // detiene el timer
         
        }else if (temp>tem_control+1 && temp<tem_control+3){ //esta caliente pero no mas de 3 grados de la de control 
        
        gpio_set_level(pin_lamp, 0); 
         dir=0;
         timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 10000ULL); // seteo la alarma a 10.000 micro para que en un seg me de 100 pasos 
         timer_start(TIMER_GROUP_0, TIMER_0);
        }else if (temp>=tem_control+3 && temp<=tem_control+5){ //esta caliente entre 3 y 5 grados mas de la de control     
        
         gpio_set_level(pin_lamp, 0); 
         dir=0;
         timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 3333ULL); // seteo la alarma a 3.333 micro para que en un seg me de 300 pasos 
         timer_start(TIMER_GROUP_0, TIMER_0);

        }else if (temp>tem_control+5){ //esta caliente por mas de 5 grados
         gpio_set_level(pin_lamp, 0); 
         dir=0;
         timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1666ULL); // seteo la alarma a 1.666 micro para que en un seg me de 600 pasos 
         timer_start(TIMER_GROUP_0, TIMER_0);
        }
        
        
        //---------------------control luz----------------------------------
      
         if (luz_porcentaje<=20){ //leds encendidos 100%
            luz_porcentaje_leds=100;
           // Actualizar el ciclo de trabajo (duty cycle)
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4095);
            // Aplicar el cambio
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }else if (luz_porcentaje>20 && luz_porcentaje<30){ //leds encendidos 80%
            luz_porcentaje_leds=80;
         // Actualizar el ciclo de trabajo (duty cycle)
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 3276);
            // Aplicar el cambio
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }else if (luz_porcentaje>=30 && luz_porcentaje<=40){ //leds encendidos 60%
            luz_porcentaje_leds=60;
            // Actualizar el ciclo de trabajo (duty cycle)
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 2457);
            // Aplicar el cambio
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }else if (luz_porcentaje>40 && luz_porcentaje<60){ //leds encendidos 50%
            luz_porcentaje_leds=50;
        
             // Actualizar el ciclo de trabajo (duty cycle)
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 2048);
            // Aplicar el cambio
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }else if (luz_porcentaje>=60 && luz_porcentaje<=80){ //leds encendidos 30%
            luz_porcentaje_leds=30;
           // Actualizar el ciclo de trabajo (duty cycle)
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1229);
            // Aplicar el cambio
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }else if (luz_porcentaje>80 ){//leds apagados
            luz_porcentaje_leds=0;
         // Actualizar el ciclo de trabajo (duty cycle)
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            // Aplicar el cambio
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        }
    vTaskDelay(pdMS_TO_TICKS(800));
        

    }
}