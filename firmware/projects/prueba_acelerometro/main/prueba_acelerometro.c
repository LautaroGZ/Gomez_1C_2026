/**
 * @file acelerometro.c
 * @brief Adquisición de datos de un acelerómetro analógico con ADC + UART + FreeRTOS.
 *
 * @details
 * Este programa lee la señal de un acelerómetro a una frecuencia de muestreo fija utilizando:
 * - ADC para adquirir la señal desde la entrada analógica CH1.
 * - UART para enviar los datos al Serial Plotter de la PC.
 * - FreeRTOS para ejecutar la adquisición y transmisión en una tarea.
 */

/*==================[inclusions]=============================================*/

#include <stdio.h>       /**< Librería estándar de entrada/salida */
#include <stdint.h>      /**< Definiciones de enteros estándar */
#include <stdbool.h>     /**< Definiciones booleanas */

#include "freertos/FreeRTOS.h"   /**< Núcleo FreeRTOS */
#include "freertos/task.h"       /**< Manejo de tareas */

#include "analog_io_mcu.h"       /**< Driver ADC/DAC */
#include "uart_mcu.h"            /**< Driver UART */
#include "ADXL335.h"
#include "led.h"               /**< Driver de LEDs */
#include "l293.h"

/*==================[macros and definitions]=================================*/

/**
 * @brief Tamaño del stack de la tarea.
 */
#define TASK_STACK_SIZE     2048

/**
 * @brief Prioridad de la tarea principal.
 */
#define TASK_PRIORITY       5

/**
 * @brief Delay fijo entre muestras (Muestreo).
 */
#define DELAY_MUESTREO_MS   20

#define ACCEL_ALTO_MAX    1800
#define ACCEL_ALTO_MIN    1700

#define ACCEL_BAJO_MAX    1400  
#define ACCEL_BAJO_MIN    1200
/*==================[internal functions declaration]=========================*/

/**
 * @brief Tarea principal de adquisición y transmisión del acelerómetro.
 *
 * @param pvParameter Parámetro de la tarea (no utilizado).
 * 
 */
void Prender_Leds(void *pvParameter);

void TaskAccel(void *pvParameter);

/*==================[internal data definition]===============================*/

/**
 * @brief Variable donde se almacena el valor leído por ADC (Acelerómetro).
 */
uint16_t valor_accel = 0;

/**
 * @brief Buffer utilizado para transmisión UART.
 */
char uart_buffer[32];

/**
 * @brief Configuración del ADC.
 */
static analog_input_config_t adc_config = {
    .input = CH1,          /**< Canal analógico CH1 (Conectar aca el acelerómetro) */
    .mode = ADC_SINGLE,    /**< Modo lectura simple */
    .func_p = NULL,        /**< Callback no utilizado */
    .param_p = NULL        /**< Parámetro callback */
};

/**
 * @brief Configuración de la UART.
 */
static serial_config_t uart_config = {
    .port = UART_PC,          /**< Puerto UART hacia PC */
    .baud_rate = 115200,      /**< Velocidad de transmisión */
    .func_p = NULL,           /**< Sin callback de recepción */
    .param_p = NULL           /**< Parámetro callback */
};



/*==================[external functions definition]==========================*/


/*==================[tasks definition]=======================================*/

/**
 * @brief Tarea principal de adquisición del acelerómetro.
 *
 * @details
 * La tarea realiza continuamente:
 * 1. Leer la señal del acelerómetro desde el ADC.
 * 2. Formatear y enviar datos por UART.
 * 3. Esperar un tiempo fijo (frecuencia de muestreo).
 *
 * @param pvParameter Parámetro de la tarea (no utilizado).
 */
void TaskAccel(void *pvParameter)
{
    while(true)
    {
        /* Leer señal desde CH1 (Acelerómetro) */
        AnalogInputReadSingle(CH1, &valor_accel);

        /* Formato compatible con Serial Plotter: Valor Acelerómetro */
        sprintf(uart_buffer, "%d\r\n", valor_accel);

        /* Transmitir datos por UART */
        UartSendString(UART_PC, uart_buffer);

        /* Espera fija según la frecuencia de muestreo configurada */
        vTaskDelay(pdMS_TO_TICKS(DELAY_MUESTREO_MS));
    }
}

void Prender_Leds(void *pvParameter)
{
    while (1)
    {
        /* --- ESTADO 1: Inclinación hacia un lado (Rango Alto) --- */
        if ((valor_accel >= ACCEL_ALTO_MIN) && (valor_accel <= ACCEL_ALTO_MAX))
        {
            LedOn(LED_2);
            LedOff(LED_3);
            L293SetSpeed(MOTOR_1, -50);
            L293SetSpeed(MOTOR_2, -50);
        }
        
        /* --- ESTADO 2: Inclinación hacia el otro lado (Rango Bajo) --- */
        if ((valor_accel >= ACCEL_BAJO_MIN) && (valor_accel <= ACCEL_BAJO_MAX))
        {
            LedOn(LED_3);
            LedOff(LED_2);
            L293SetSpeed(MOTOR_1, 50);
            L293SetSpeed(MOTOR_2, 50);
        }
        
        /* --- ESTADO NEUTRO: En el medio de los dos rangos --- */
        if ((valor_accel > ACCEL_BAJO_MAX) && (valor_accel < ACCEL_ALTO_MIN))
        {
            LedOff(LED_3);
            LedOff(LED_2);
            L293SetSpeed(MOTOR_1, 0);
            L293SetSpeed(MOTOR_2, 0);
        }

        /* Espera para no saturar el procesador (usa el delay de tu sistema) */
        vTaskDelay(pdMS_TO_TICKS(DELAY_MUESTREO_MS));
    }
}

/**
 * @brief Función principal del programa.
 *
 * @details
 * Inicializa UART y ADC.
 * Luego crea la tarea principal TaskAccel().
 */
void app_main(void)
{
    /* Inicializar UART */
    UartInit(&uart_config);

    /* Inicializar ADC */
    AnalogInputInit(&adc_config);

    LedsInit();
    L293Init();  // Inicializar el controlador L293
    
    /* Crear tarea principal */
    xTaskCreate(
        TaskAccel,         /**< Función de la tarea */
        "Accel_Task",      /**< Nombre de la tarea */
        TASK_STACK_SIZE,   /**< Tamaño del stack */
        NULL,              /**< Parámetros */
        TASK_PRIORITY,     /**< Prioridad */
        NULL               /**< Handle no utilizado */
    );
    xTaskCreate(
        Prender_Leds,         /**< Función de la tarea */
        "Leds_Task",      /**< Nombre de la tarea */
        TASK_STACK_SIZE,   /**< Tamaño del stack */
        NULL,              /**< Parámetros */
        TASK_PRIORITY,     /**< Prioridad */
        NULL               /**< Handle no utilizado */
    );
}

/*==================[end of file]============================================*/