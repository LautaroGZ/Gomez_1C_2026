/**
 * @file guia2_ejercicio2_timers.c
 * @brief Sistema de medición de distancia con sensor ultrasónico HC-SR04.
 *
 * @details Este módulo implementa un sistema de medición de distancia basado en
 * el sensor ultrasónico HC-SR04. La aplicación utiliza un timer de hardware y
 * una tarea FreeRTOS para realizar mediciones periódicas. La información se
 * muestra en un display LCD y se controla un conjunto de LEDs según el rango de
 * distancia detectado.
 *
 * Características principales:
 * - Medición de distancia en centímetros.
 * - Visualización en display LCD ITS E0803.
 * - Indicadores LED de rango de proximidad.
 * - Dos pulsadores para activar/desactivar la medición y congelar la pantalla.
 * - Uso de interrupciones y notificaciones desde ISR a tarea FreeRTOS.
 *
 * @author Lautaro Gómez
 * @email lautaro.gomez@ingenieria.uner.edu.ar
 * @date 2026
 * @version 1.0
 */

/**
 * @defgroup guia2_ejercicio2_timers Guía 2 - Ejercicio 2: Temporizadores
 * @brief Documentación del proyecto de medición de distancia con temporizador.
 *
 * Este módulo forma parte de la guía de trabajos prácticos y muestra el uso de
 * timers, interrupciones y FreeRTOS en un sistema de adquisición de distancia.
 * @{
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

/*==================[macros and definitions]=================================*/
/**
 * @def PERIODO_MEDICION_US
 * @brief Período de medición en microsegundos
 * Define el intervalo de tiempo (1000000 us = 1 segundo) para el timer de hardware.
 */
#define PERIODO_MEDICION_US 1000000

/*==================[internal data definition]===============================*/
/**
 * @brief Indica si la medición está activa.
 *
 * Cuando es verdadero, la tarea de medición lee el sensor y actualiza el LCD.
 */
volatile bool medir_activa = true;

/**
 * @brief Indica si la función HOLD está activa.
 *
 * Cuando está activo, la lectura en el display LCD no se actualiza.
 */
volatile bool hold_activo = false;

/**
 * @brief Última distancia medida en centímetros.
 */
uint16_t distancia_cm = 0;

/**
 * @brief Handle de la tarea de medición FreeRTOS.
 */
TaskHandle_t medicion_task_handle = NULL;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Tarea periódica que realiza la medición de distancia.
 *
 * La tarea queda bloqueada y se despierta mediante una notificación enviada por
 * la ISR del timer de hardware. Si la medición está habilitada, actualiza el
 * LCD y controla los LEDs de rango.
 *
 * @param pvParameter Parámetro de creación de la tarea (no usado).
 */
void TareaMedicion(void *pvParameter);

/**
 * @brief Cambia el estado de la medición.
 *
 * Esta función se invoca desde la interrupción del pulsador SWITCH_1.
 *
 * @param args Parámetro de callback (no usado).
 */
void CambiarEstadoMedicion(void *args);

/**
 * @brief Cambia el estado de HOLD.
 *
 * Esta función se invoca desde la interrupción del pulsador SWITCH_2.
 *
 * @param args Parámetro de callback (no usado).
 */
void CambiarEstadoHold(void *args);

/**
 * @brief Maneja la interrupción periódica del timer de hardware.
 *
 * Esta ISR notifica a la tarea de medición para que realice una nueva lectura.
 *
 * @param param Parámetro de la ISR (no usado).
 */
void AtenderInterrupcionTimer(void* param);

/*==================[external functions definition]==========================*/

/**
 * @brief Función de interrupción del timer de hardware
 */
void AtenderInterrupcionTimer(void* param)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* Notifica a la tarea de medición para que se despierte y ejecute un ciclo */
    vTaskNotifyGiveFromISR(medicion_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief ISR para SWITCH_1 - Activar/Desactivar medición
 */
void CambiarEstadoMedicion(void *args)
{
    medir_activa = !medir_activa;
}

/**
 * @brief ISR para SWITCH_2 - Activar/Desactivar HOLD (congelación de lectura)
 */
void CambiarEstadoHold(void *args)
{
    hold_activo = !hold_activo;
}

/**
 * @brief Tarea FreeRTOS - Medición de distancia
 */
void TareaMedicion(void *pvParameter)
{
    while (1)
    {
        /* La tarea se bloquea aquí hasta recibir la notificación del Timer */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        if (medir_activa)
        {
            /* Lectura del sensor ultrasónico */
            distancia_cm = HcSr04ReadDistanceInCentimeters();

            /* Actualizar display LCD si no está en HOLD */
            if (!hold_activo)
            {
                LcdItsE0803Write(distancia_cm);
            }
            
            /* Controlar LEDs según rango de distancia */
            if (distancia_cm < 10)
            {
                LedsOffAll();
            }
            else if (distancia_cm >= 10 && distancia_cm < 20)
            {
                LedOn(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);
            }
            else if (distancia_cm >= 20 && distancia_cm <= 30)
            {
                LedOn(LED_1);
                LedOn(LED_2);
                LedOff(LED_3);
            }
            else if (distancia_cm > 30)
            {
                LedOn(LED_1);
                LedOn(LED_2);
                LedOn(LED_3);
            }
        }
        else
        {
            /* Sistema inactivo: apagar periféricos */
            LedsOffAll();
            LcdItsE0803Off();
        }
    }
}

/**
 * @brief Función principal - Punto de entrada de la aplicación
 */
void app_main(void)
{
    /* ============ Inicialización de Periféricos ============ */
    LedsInit();
    SwitchesInit();
    HcSr04Init(GPIO_3, GPIO_2);
    LcdItsE0803Init();

    /* ============ Configuración de Interrupciones ============ */
    SwitchActivInt(SWITCH_1, &CambiarEstadoMedicion, NULL);
    SwitchActivInt(SWITCH_2, &CambiarEstadoHold, NULL);

    /* ============ Configuración del Timer ============ */
    timer_config_t mi_timer;
    mi_timer.timer = TIMER_A;
    mi_timer.period = PERIODO_MEDICION_US;
    mi_timer.func_p = &AtenderInterrupcionTimer;    
    mi_timer.param_p = NULL;    

    TimerInit(&mi_timer);

    /* ============ Creación de Tareas FreeRTOS ============ */
    xTaskCreate(&TareaMedicion,
                "Medicion",
                2048,
                NULL,
                5,
                &medicion_task_handle);
                
    /* ============ Arranque del Timer ============ */
    TimerStart(mi_timer.timer);
}

/** @} */