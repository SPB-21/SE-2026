//desarrollo punto 3
void spi_bus_init(void) {
}
void mcp4132_writw_register(void) {
}
void mcp4132_read_register(void) {
}
//desarrollo punto 4

//desarrollo punto 5

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/uart.h" //driver uart
#include "driver/gpio.h"
#include "driver/timer.h"

#define UART_PORT UART_NUM_0 // defino el uart que voy a usar(en este caso el del pc) 
static volatile bool leer_adc = false;



//uso interupcion con timer para que me muestre la señal
bool IRAM_ATTR timer_callback(void* arg) {

  static volatile bool leer_adc = true;
    return false; 
}

void app_main(void)
{
    // 1. Creao handler del ADC

    adc_oneshot_unit_handle_t adc1_handle;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,  // Usamos ADC1
    };

    // Inicializa el ADC en modo oneshot
    adc_oneshot_new_unit(&init_config, &adc1_handle);


   
    // 2. Configurar canal

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // Resolución 12 bits 
        .atten = ADC_ATTEN_DB_12,         // Hasta ~3.3V
    };

    // Canal 6
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &chan_config);

   
    //  Configurar calibración LINEAL
    
    adc_cali_handle_t adc_cali_handle = NULL;  // Inicialmente NULL

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,        // Unidad ADC usada
        .atten = ADC_ATTEN_DB_12,     // Atenuación del canal
        .bitwidth = ADC_BITWIDTH_12,  // Resolución
    };

    // Crear calibración lineal
    adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);


    //  Variables de lectura

    int adc_raw = 0;     // Valor crudo 0-4095
    int voltage_mv = 0;  // Voltaje calibrado en mV

// configuracion uart

     uart_config_t uart_config = {
        .baud_rate = 115200,  
        .data_bits = UART_DATA_8_BITS, 
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  
        .source_clk = UART_SCLK_DEFAULT
    };
    uart_param_config(UART_PORT, &uart_config); 
    uart_driver_install(UART_PORT, 1024, 1024, 0, NULL, 0); 


    //interrupcion timer
    // Configuración del Timer
    timer_config_t config = {
        .divider = 80,                 // 1 tick = 1 us (80MHz / 80)
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,    // activar alarma
        .auto_reload = true            // Que se reinicie solo
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    // 3. Fijamos el valor de la alarma (1000 micro segundos que es el perido de 1khz)
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000);


    
    // 4. Registramos la función que atenderá la interrupción
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_callback, NULL,0);

    // 5. Habilitamos la interrupción
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

    // 6. Arrancamos
    timer_start(TIMER_GROUP_0, TIMER_0);

    while (1)
    {
        if (leer_adc)
        {
            adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw);
            // Convierto valor crudo a voltaje calibrado (mV)
            adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_mv);
            
            leer_adc=false; //reseto bandera
        }
        
        if (voltage_mv>1400)
        {
            //N=95  como no las he hecho las dejo comentadas
        }else if (voltage_mv<900)
        {
            //N=42 como no las he hecho las dejo comentadas
        }
        
        
        
    
    }

    
  


}

