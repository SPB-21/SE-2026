#include "freertos/FreeRTOS.h" //sistema operativo
#include "freertos/task.h"  //crear y controlar tareas (threads) en FreeRTOS  
#include "driver/gpio.h"  // drivers gpios

#define LED_PIN 2 // Declaramos una constante para el pin que vamos a usar

void app_main() {
    gpio_config_t io_conf = {             //estrutura de configuracion(es una clase)
        .pin_bit_mask = (1UL << LED_PIN),  // asignar el pin a la mascara
        .mode = GPIO_MODE_OUTPUT,         //salida pin gpio
        .pull_up_en = GPIO_PULLUP_DISABLE,   //desabilitar pull up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,   //desabilitar pull down
        .intr_type = GPIO_INTR_DISABLE     //desabilitar interrupciones
    };

    gpio_config(&io_conf);          //mandar configuracion

    while(1) {        //ciclo infinito
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
