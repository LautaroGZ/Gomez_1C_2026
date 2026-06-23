/*! @mainpage Guía 2 - Ejercicio 2: Interrupciones, Temporizadores y UART
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "switch.h"
#include "hc_sr04.h"
#include "lcditse0803.h"
#include "timer_mcu.h"
#include "uart_mcu.h"

/*==================[macros and definitions]=================================*/
#define PERIODO_MEDICION_US 1000000

/*==================[internal data definition]===============================*/
volatile bool medir_activa = true;
volatile bool hold_activo = false;

uint16_t distancia_cm = 0;
TaskHandle_t medicion_task_handle = NULL;

/*==================[function declarations]==================================*/
void TareaMedicion(void *pvParameter);
void CambiarEstadoMedicion(void *args);
void CambiarEstadoHold(void *args);
void AtenderInterrupcionTimer(void* param);
void EnviarDistanciaUART(uint16_t distancia);

/*==================[functions definition]===================================*/

void AtenderInterrupcionTimer(void* param)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(medicion_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void CambiarEstadoMedicion(void *args)
{
    medir_activa = !medir_activa;
}

void CambiarEstadoHold(void *args)
{
    hold_activo = !hold_activo;
}

void EnviarDistanciaUART(uint16_t distancia)
{
    char buffer[10];

    if (distancia > 999)
        distancia = 999;

    buffer[0] = (distancia / 100) % 10 + '0';
    buffer[1] = (distancia / 10) % 10 + '0';
    buffer[2] = (distancia % 10) + '0';
    buffer[3] = ' ';
    buffer[4] = 'c';
    buffer[5] = 'm';
    buffer[6] = '\r';
    buffer[7] = '\n';
    buffer[8] = '\0';

    UartSendString(UART_PC, buffer);
}

void TareaMedicion(void *pvParameter)
{
    uint8_t dato;

    while (1)
    {
        /* --- Lectura continua de UART (no bloqueante) --- */
        if (UartReadByte(UART_PC, &dato))
        {
            if (dato == 'O' || dato == 'o')
            {
                CambiarEstadoMedicion(NULL);
            }
            else if (dato == 'H' || dato == 'h')
            {
                CambiarEstadoHold(NULL);
            }
        }

        /* --- Evento del timer (no bloqueante) --- */
        if (ulTaskNotifyTake(pdTRUE, 0))
        {
            if (medir_activa)
            {
                distancia_cm = HcSr04ReadDistanceInCentimeters();

                /* Envío UART */
                EnviarDistanciaUART(distancia_cm);

                /* LCD */
                if (!hold_activo)
                {
                    LcdItsE0803Write(distancia_cm);
                }

                /* LEDs */
                if (distancia_cm < 10)
                {
                    LedsOffAll();
                }
                else if (distancia_cm < 20)
                {
                    LedOn(LED_1);
                    LedOff(LED_2);
                    LedOff(LED_3);
                }
                else if (distancia_cm <= 30)
                {
                    LedOn(LED_1);
                    LedOn(LED_2);
                    LedOff(LED_3);
                }
                else
                {
                    LedOn(LED_1);
                    LedOn(LED_2);
                    LedOn(LED_3);
                }
            }
            else
            {
                LedsOffAll();
                LcdItsE0803Off();
            }
        }

        /* Evita saturar CPU */
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    /* Inicialización de periféricos */
    LedsInit();
    SwitchesInit();
    HcSr04Init(GPIO_3, GPIO_2);
    LcdItsE0803Init();

    /* UART */
    serial_config_t uart_config = {
        .port = UART_PC,
        .baud_rate = 115200,
        .func_p = UART_NO_INT,
        .param_p = NULL
    };
    UartInit(&uart_config);

    /* Interrupciones switches */
    SwitchActivInt(SWITCH_1, &CambiarEstadoMedicion, NULL);
    SwitchActivInt(SWITCH_2, &CambiarEstadoHold, NULL);

    /* Timer */
    timer_config_t mi_timer;
    mi_timer.timer = TIMER_A;
    mi_timer.period = PERIODO_MEDICION_US;
    mi_timer.func_p = &AtenderInterrupcionTimer;
    mi_timer.param_p = NULL;

    TimerInit(&mi_timer);

    /* Tarea */
    xTaskCreate(&TareaMedicion,
                "Medicion",
                2048,
                NULL,
                5,
                &medicion_task_handle);

    /* Start timer */
    TimerStart(mi_timer.timer);
}