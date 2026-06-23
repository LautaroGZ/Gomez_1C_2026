/**
 *
 * @details
 * Este programa implementa un osciloscopio básico para señales ECG utilizando:
 * - DAC para generar una señal ECG almacenada en un arreglo.
 * - ADC para leer la señal desde la entrada analógica CH1.
 * - UART para enviar los datos al Serial Plotter.
 * - FreeRTOS para ejecutar la adquisición y transmisión en una tarea.
 *
 * Además, permite modificar la frecuencia de reproducción del ECG:
 * - TEC1 o tecla 'T' -> aumenta la frecuencia (taquicardia).
 * - TEC2 o tecla 'B' -> disminuye la frecuencia (bradicardia).
 * - Tecla 'R' -> restaura la frecuencia normal.
 */

/*==================[inclusions]=============================================*/

#include <stdio.h>      /**< Librería estándar de entrada/salida */
#include <stdint.h>     /**< Definiciones de enteros estándar */
#include <stdbool.h>    /**< Definiciones booleanas */

#include "freertos/FreeRTOS.h"   /**< Núcleo FreeRTOS */
#include "freertos/task.h"       /**< Manejo de tareas */

#include "analog_io_mcu.h"       /**< Driver ADC/DAC */
#include "uart_mcu.h"            /**< Driver UART */
#include "switch.h"              /**< Driver de teclas */


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
 * @brief Delay inicial entre muestras ECG.
 */
#define DEFAULT_DELAY_MS   20

/**
 * @brief Incremento/decremento del delay.
 */
#define STEP_DELAY_MS      1

/**
 * @brief Delay mínimo permitido.
 */
#define MIN_DELAY_MS       1

/**
 * @brief Delay máximo permitido.
 */
#define MAX_DELAY_MS       100

/*==================[ECG data]===============================================*/

/**
 * @brief Tabla de muestras de una señal ECG.
 *
 * @details
 * Cada valor representa una muestra de amplitud
 * que será enviada al DAC para reconstruir la señal.
 */
unsigned char ECG[] = {

17,17,17,17,17,17,17,17,17,17,17,18,18,18,17,17,17,17,17,17,17,18,18,18,18,18,18,18,17,17,16,16,16,16,17,17,18,18,18,17,17,17,17,

18,18,19,21,22,24,25,26,27,28,29,31,32,33,34,34,35,37,38,37,34,29,24,19,15,14,15,16,17,17,17,16,15,14,13,13,13,13,13,13,13,12,12,

10,6,2,3,15,43,88,145,199,237,252,242,211,167,117,70,35,16,14,22,32,38,37,32,27,24,24,26,27,28,28,27,28,28,30,31,31,31,32,33,34,36,

38,39,40,41,42,43,45,47,49,51,53,55,57,60,62,65,68,71,75,79,83,87,92,97,101,106,111,116,121,125,129,133,136,138,139,140,140,139,137,

133,129,123,117,109,101,92,84,77,70,64,58,52,47,42,39,36,34,31,30,28,27,26,25,25,25,25,25,25,25,25,24,24,24,24,25,25,25,25,25,25,25,

24,24,24,24,24,24,24,24,23,23,22,22,21,21,21,20,20,20,20,20,19,19,18,18,18,19,19,19,19,18,17,17,18,18,18,18,18,18,18,18,17,17,17,17,

17,17,17

};

/*==================[internal functions declaration]=========================*/

/**
 * @brief Tarea principal de adquisición y transmisión.
 *
 * @param pvParameter Parámetro de la tarea (no utilizado).
 */
void TaskADC(void *pvParameter);

/**
 * @brief Interrupción asociada a TEC1.
 *
 * @details
 * Disminuye el delay entre muestras para aumentar
 * la frecuencia del ECG simulado.
 *
 * @param param Parámetro de interrupción.
 */
void tecla1_isr(void *param);

/**
 * @brief Interrupción asociada a TEC2.
 *
 * @details
 * Aumenta el delay entre muestras para disminuir
 * la frecuencia del ECG simulado.
 *
 * @param param Parámetro de interrupción.
 */
void tecla2_isr(void *param);

/**
 * @brief Callback de recepción UART.
 *
 * @details
 * Permite controlar la frecuencia desde el teclado serial.
 *
 * @param param Parámetro del callback.
 */
void FuncUart(void *param);

/*==================[internal data definition]===============================*/

/**
 * @brief Variable donde se almacena el valor leído por ADC.
 */
uint16_t valor_adc = 0;

/**
 * @brief Índice actual del arreglo ECG.
 */
uint16_t ecg_index = 0;

/**
 * @brief Delay variable entre muestras ECG.
 *
 * @details
 * Controla la velocidad de reproducción de la señal.
 */
volatile uint32_t delay_ecg = DEFAULT_DELAY_MS;

/**
 * @brief Buffer utilizado para transmisión UART.
 */
char uart_buffer[32];

/**
 * @brief Configuración del ADC.
 */
static analog_input_config_t adc_config = {

    .input = CH1,          /**< Canal analógico CH1 */
    .mode = ADC_SINGLE,   /**< Modo lectura simple */
    .func_p = NULL,       /**< Callback no utilizado */
    .param_p = NULL       /**< Parámetro callback */
};

/**
 * @brief Configuración de la UART.
 */
static serial_config_t uart_config = {

    .port = UART_PC,          /**< Puerto UART hacia PC */
    .baud_rate = 115200,      /**< Velocidad de transmisión */
    .func_p = FuncUart,       /**< Callback de recepción */
    .param_p = NULL           /**< Parámetro callback */
};

/*==================[external functions definition]==========================*/

/**
 * @brief Función principal del programa.
 *
 * @details
 * Inicializa:
 * - UART
 * - ADC
 * - DAC
 * - teclas
 * - interrupciones
 *
 * Luego crea la tarea principal TaskADC().
 */
void app_main(void)
{
    /* Inicializar UART */
    UartInit(&uart_config);

    /* Inicializar ADC */
    AnalogInputInit(&adc_config);

    /* Inicializar DAC */
    AnalogOutputInit();

    /* Inicializar teclas */
    SwitchesInit();

    /* Activar interrupciones de teclas */
    SwitchActivInt(SWITCH_1, tecla1_isr, NULL);
    SwitchActivInt(SWITCH_2, tecla2_isr, NULL);

    /* Crear tarea principal */
    xTaskCreate(
        TaskADC,           /**< Función de la tarea */
        "ADC_Task",        /**< Nombre de la tarea */
        TASK_STACK_SIZE,   /**< Tamaño del stack */
        NULL,              /**< Parámetros */
        TASK_PRIORITY,     /**< Prioridad */
        NULL               /**< Handle no utilizado */
    );
}

/*==================[interrupts definition]==================================*/

/**
 * @brief ISR de TEC1.
 *
 * @details
 * Reduce el delay para aumentar la frecuencia cardíaca simulada.
 *
 * @param param Parámetro no utilizado.
 */
void tecla1_isr(void *param)
{
    if(delay_ecg > MIN_DELAY_MS)
    {
        delay_ecg -= STEP_DELAY_MS;
    }
}

/**
 * @brief ISR de TEC2.
 *
 * @details
 * Incrementa el delay para disminuir la frecuencia cardíaca simulada.
 *
 * @param param Parámetro no utilizado.
 */
void tecla2_isr(void *param)
{
    if(delay_ecg < MAX_DELAY_MS)
    {
        delay_ecg += STEP_DELAY_MS;
    }
}

/**
 * @brief Callback de recepción UART.
 *
 * @details
 * Comandos disponibles:
 * - T/t -> aumenta frecuencia.
 * - B/b -> disminuye frecuencia.
 * - R/r -> restaura frecuencia normal.
 *
 * @param param Parámetro no utilizado.
 */
void FuncUart(void *param)
{
    uint8_t dato;

    /* Leer byte recibido */
    UartReadByte(UART_PC, &dato);

    switch(dato)
    {
        case 'T':
        case 't':

            if(delay_ecg > MIN_DELAY_MS)
            {
                delay_ecg -= STEP_DELAY_MS;
            }

            break;

        case 'B':
        case 'b':

            if(delay_ecg < MAX_DELAY_MS)
            {
                delay_ecg += STEP_DELAY_MS;
            }

            break;

        case 'R':
        case 'r':

            delay_ecg = DEFAULT_DELAY_MS;

            break;
    }
}

/*==================[tasks definition]=======================================*/

/**
 * @brief Tarea principal de generación y adquisición ECG.
 *
 * @details
 * La tarea realiza continuamente:
 * 1. Enviar una muestra ECG al DAC.
 * 2. Leer la señal desde el ADC.
 * 3. Enviar datos por UART.
 * 4. Avanzar al siguiente punto del ECG.
 * 5. Esperar un tiempo configurable.
 *
 * @param pvParameter Parámetro de la tarea (no utilizado).
 */
void TaskADC(void *pvParameter)
{
    while(true)
    {
        /* Enviar muestra ECG al DAC */
        AnalogOutputWrite(ECG[ecg_index]);

        /* Leer señal desde CH1 */
        AnalogInputReadSingle(CH1, &valor_adc);

        /* Formato compatible con Serial Plotter */
        //sprintf(uart_buffer, ">ECG:%d,Delay:%ld\r\n", valor_adc, delay_ecg);
        sprintf(uart_buffer, "%d,%ld\r\n", valor_adc, delay_ecg);


        /* Transmitir datos por UART */
        UartSendString(UART_PC, uart_buffer);

        /* Avanzar índice ECG */
        ecg_index++;

        /* Reiniciar índice al finalizar el arreglo */
        if(ecg_index >= (sizeof(ECG) / sizeof(ECG[0])))
        {
            ecg_index = 0;
        }

        /* Espera variable según frecuencia seleccionada */
        vTaskDelay(pdMS_TO_TICKS(delay_ecg));
    }
}

/*==================[end of file]============================================*/