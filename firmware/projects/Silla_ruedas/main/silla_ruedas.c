/**
 * @file silla_ruedas.c
 * @brief Sistema de control de una silla de ruedas mediante acelerómetro y Bluetooth Low Energy.
 *
 * @details
 * El programa implementa un sistema  para controlar
 * el movimiento de una silla de ruedas eléctrica utilizando un acelerómetro
 * analógico ADXL335 como dispositivo de entrada y Bluetooth Low Energy (BLE)
 * como mecanismo de habilitación remota.
 *
 * El sistema ejecuta tareas concurrentes para:
 * - Adquirir continuamente la señal analógica del acelerómetro.
 * - Transmitir los datos adquiridos por UART para monitoreo.
 * - Determinar el sentido de movimiento según la inclinación detectada.
 * - Accionar dos motores DC mediante un controlador L293.
 * - Indicar visualmente el estado mediante LEDs.
 * - Habilitar o bloquear el movimiento desde una aplicación móvil vía BLE.
 *
 * Funcionamiento:
 * - Si la silla está habilitada mediante Bluetooth:
 *      - Inclinación en rango alto  -> retroceso.
 *      - Inclinación en rango bajo  -> avance.
 *      - Posición intermedia        -> detención.
 *
 * - Si la silla está deshabilitada:
 *      - Motores detenidos.
 *      - LEDs apagados.
 *
 * El sistema utiliza una arquitectura multitarea basada en FreeRTOS,
 * donde la adquisición de datos y el control de movimiento se ejecutan
 * en tareas independientes.
 *
 * @defgroup silla_ruedas Sistema de control de silla de ruedas
 * @{
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "ADXL335.h"
#include "led.h"
#include "l293.h"
#include "ble_mcu.h"

/*==================[macros and definitions]=================================*/

/**
 * @brief Tamaño del stack asignado a cada tarea de FreeRTOS.
 */
#define TASK_STACK_SIZE     2048

/**
 * @brief Prioridad de ejecución de las tareas.
 */
#define TASK_PRIORITY       5

/**
 * @brief Período de muestreo y actualización del sistema.
 *
 * @details
 * Define el tiempo de espera entre iteraciones de las tareas de
 * adquisición y control.
 */
#define DELAY_MUESTREO_MS   20

/**
 * @brief Límites superiores para detectar inclinación en sentido de retroceso.
 */
#define ACCEL_ALTO_MAX    1800
#define ACCEL_ALTO_MIN    1700

/**
 * @brief Límites inferiores para detectar inclinación en sentido de avance.
 */
#define ACCEL_BAJO_MAX    1400
#define ACCEL_BAJO_MIN    1200

/*==================[internal functions declaration]=========================*/

/**
 * @brief Tarea de adquisición y transmisión de datos del acelerómetro.
 *
 * @details
 * Lee periódicamente el canal analógico CH1 conectado al acelerómetro
 * ADXL335 y almacena el resultado en la variable global `valor_accel`.
 *
 * Posteriormente transmite la muestra por UART 
 *
 * Tiempo de ejecución:
 * -20 ms.
 *
 * @param[in] pvParameter Parámetro no utilizado.
 */
void TaskAccel(void *pvParameter);

/**
 * @brief Tarea de control de motores y señalización mediante LEDs.
 *
 * @details
 * Evalúa continuamente el valor adquirido por el acelerómetro y el
 * estado de habilitación recibido por BLE.
 *
 * Dependiendo del rango de inclinación detectado:
 * - Retroceso.
 * - Avance.
 * - Detención.
 *
 * Además implementa una condición de parada segura cuando la silla
 * se encuentra deshabilitada.
 *
 * @param[in] pvParameter Parámetro no utilizado.
 */
void Control_Motores_Leds(void *pvParameter);

/**
 * @brief Procesa los comandos recibidos mediante Bluetooth Low Energy.
 *
 * @details
 * La función es invocada automáticamente por la pila BLE cuando
 * se reciben datos desde una aplicación móvil.
 *
 * Comandos soportados:
 * - 'C' : habilita el movimiento de la silla.
 * - 'c' : deshabilita el movimiento de la silla.
 *
 * @param[in] data Buffer con los datos recibidos.
 * @param[in] length Cantidad de bytes recibidos.
 */
void leer_data_bluetooth(uint8_t * data, uint8_t length);

/*==================[internal data definition]===============================*/

/**
 * @brief Última muestra adquirida desde el acelerómetro.
 *
 * @details
 * Variable compartida entre la tarea de adquisición y la tarea de
 * control. Contiene el valor digital obtenido mediante el ADC.
 */
uint16_t valor_accel = 0;

/**
 * @brief Buffer utilizado para transmisión UART.
 */
char uart_buffer[32];

/**
 * @brief Estado de habilitación de la silla.
 *
 * @details
 * Determina si los motores pueden ser accionados.
 *
 * true  -> movimiento habilitado.
 * false -> movimiento bloqueado.
 */
volatile bool silla_habilitada = false;

/**
 * @brief Configuración del conversor analógico-digital.
 */
static analog_input_config_t adc_config = {
    .input = CH1,
    .mode = ADC_SINGLE,
    .func_p = NULL,
    .param_p = NULL
};

/**
 * @brief Configuración de la UART.
 */
static serial_config_t uart_config = {
    .port = UART_PC,
    .baud_rate = 115200,
    .func_p = NULL,
    .param_p = NULL
};

/**
 * @brief Configuración del servicio Bluetooth Low Energy.
 */
ble_config_t ble_config = {
    "Silla_Ruedas",
    leer_data_bluetooth
};

/*==================[tasks definition]=======================================*/

void TaskAccel(void *pvParameter)
{
    while(true)
    {
        /* Leer acelerómetro */
        AnalogInputReadSingle(CH1, &valor_accel);

        /* Convertir a texto */
        sprintf(uart_buffer, "%d\r\n", valor_accel);

        /* Enviar por UART */
        UartSendString(UART_PC, uart_buffer);

        /* Esperar próximo período */
        vTaskDelay(pdMS_TO_TICKS(DELAY_MUESTREO_MS));
    }
}

void Control_Motores_Leds(void *pvParameter)
{
    while (1)
    {
        if (silla_habilitada)
        {
            /* Inclinación hacia atrás */
            if ((valor_accel >= ACCEL_ALTO_MIN) &&
                (valor_accel <= ACCEL_ALTO_MAX))
            {
                LedOn(LED_2);
                LedOff(LED_3);

                L293SetSpeed(MOTOR_1, -100);
                L293SetSpeed(MOTOR_2, -100);
            }

            /* Inclinación hacia adelante */
            else if ((valor_accel >= ACCEL_BAJO_MIN) &&
                     (valor_accel <= ACCEL_BAJO_MAX))
            {
                LedOn(LED_3);
                LedOff(LED_2);

                L293SetSpeed(MOTOR_1, 100);
                L293SetSpeed(MOTOR_2, 100);
            }

            /* Posición neutra */
            else if ((valor_accel > ACCEL_BAJO_MAX) &&
                     (valor_accel < ACCEL_ALTO_MIN))
            {
                LedOff(LED_2);
                LedOff(LED_3);

                L293SetSpeed(MOTOR_1, 0);
                L293SetSpeed(MOTOR_2, 0);
            }
        }
        else
        {
            /* Parada de emergencia */
            LedOff(LED_2);
            LedOff(LED_3);

            L293SetSpeed(MOTOR_1, 0);
            L293SetSpeed(MOTOR_2, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(DELAY_MUESTREO_MS));
    }
}

/*==================[external functions definition]==========================*/

void leer_data_bluetooth(uint8_t * data, uint8_t length)
{
    if(data[0] == 'C')
    {
        silla_habilitada = true;
    }
    else if(data[0] == 'c')
    {
        silla_habilitada = false;
    }
}

/**
 * @brief Punto de entrada principal de la aplicación.
 *
 * @details
 * Inicializa los periféricos necesarios para el funcionamiento
 * del sistema:
 *
 * - UART para transmisión de datos.
 * - ADC para adquisición del acelerómetro.
 * - LEDs indicadores.
 * - Driver de motores L293.
 * - Bluetooth Low Energy.
 *
 * Posteriormente crea las tareas de FreeRTOS encargadas de:
 * - Adquisición de datos.
 * - Control de motores y LEDs.
 *
 * Una vez creadas las tareas, el scheduler de FreeRTOS toma
 * el control de la ejecución.
 */
void app_main(void)
{
    /* Inicialización de periféricos */
    UartInit(&uart_config);
    AnalogInputInit(&adc_config);
    LedsInit();
    L293Init();
    BleInit(&ble_config);

    /* Tarea de adquisición */
    xTaskCreate(
        TaskAccel,
        "Accel_Task",
        TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY,
        NULL
    );

    /* Tarea de control */
    xTaskCreate(
        Control_Motores_Leds,
        "Control_Motores_Leds",
        TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY,
        NULL
    );
}

/** @} */