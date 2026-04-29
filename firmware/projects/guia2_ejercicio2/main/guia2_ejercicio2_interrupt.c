/**
 * @file guia2_ejercicio2.c
 * @brief Sistema de medición de distancia con sensor ultrasónico HC-SR04 implementado con interrupciones
 * 
 * @details Este módulo implementa un sistema de medición de distancia utilizando
 * un sensor HC-SR04. El sistema cuenta con:
 * - Medición de distancia en centímetros
 * - Visualización en display LCD de 8 dígitos
 * - Indicadores LED que se encienden según rangos de distancia
 * - Dos pulsadores para controlar:
 *   * SWITCH_1: Activar/Desactivar medición
 *   * SWITCH_2: Congelar/Liberar lectura en LCD (HOLD)
 * - Procesamiento con FreeRTOS en tarea independiente
 * 
 * @author Gómez Lautaro - Facultad de Ingenieria UNER
 * @date 2026
 * @version 1.0
 * 
 * @note Utiliza interrupciones para los pulsadores y una tarea FreeRTOS para
 * el procesamiento periódico de mediciones.
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>     /**< Incluye función printf para salida por consola */
#include <stdint.h>    /**< Tipos enteros de ancho fijo */
#include <stdbool.h>   /**< Tipo booleano y valores true/false */
#include "freertos/FreeRTOS.h" /**< Kernel de FreeRTOS */
#include "freertos/task.h"     /**< APIs de tareas de FreeRTOS */
#include "led.h"        /**< Driver de LEDs */
#include "switch.h"     /**< Driver de pulsadores/switches */
#include "hc_sr04.h"    /**< Driver del sensor ultrasónico HC-SR04 */
#include "lcditse0803.h" /**< Driver del display LCD ITS-E0803 */

/*==================[macros and definitions]=================================*/

/**
 * @def PERIODO_MEDICION_MS
 * @brief Período de medición en milisegundos
 * 
 * Define el intervalo de tiempo entre cada lectura del sensor ultrasónico
 * HC-SR04 y actualización de LEDs y display LCD. Valor: 1000 ms (1 segundo).
 */
#define PERIODO_MEDICION_MS 1000

/*==================[internal data definition]===============================*/

/**
 * @var medir_activa
 * @brief Indicador de estado de medición
 * 
 * Variable global volátil que controla si el sistema está activo o inactivo.
 * Se modifica por interrupción de SWITCH_1. Debe ser volatile porque se
 * modifica en la ISR CambiarEstadoMedicion() y se lee en TareaMedicion().
 * 
 * - @c true: Sistema activo, realiza mediciones
 * - @c false: Sistema inactivo, LEDs apagados y LCD apagado
 * 
 * @note Obligatorio usar volatile cuando una variable se modifica dentro de
 * una ISR y se lee en otra parte del programa.
 */
volatile bool medir_activa = true;

/**
 * @var hold_activo
 * @brief Indicador de estado HOLD (congelación de lectura)
 * 
 * Variable global volátil que controla si la lectura del display está
 * congelada o actualiza dinámicamente. Se modifica por interrupción de
 * SWITCH_2. Debe ser volatile por la misma razón que medir_activa.
 * 
 * - @c true: Lectura congelada (HOLD), no se actualiza display
 * - @c false: Lectura actualizada dinámicamente
 * 
 * @note La distancia medida se sigue procesando internamente,
 * solo se congela la visualización en el LCD.
 */
volatile bool hold_activo = false;

/**
 * @var distancia_cm
 * @brief Última distancia medida en centímetros
 * 
 * Almacena el valor de distancia más reciente obtenido del sensor
 * HC-SR04. Se actualiza periódicamente en la tarea TareaMedicion.
 * Rango típico: 0-400 cm (depende del sensor).
 */
uint16_t distancia_cm = 0;

/**
 * @var medicion_task_handle
 * @brief Handle (identificador) de la tarea de medición
 * 
 * Almacena el handle de la tarea FreeRTOS TareaMedicion para
 * referencia posterior si es necesario (suspender, reanudar, eliminar, etc.).
 */
TaskHandle_t medicion_task_handle = NULL;

/*==================[internal functions declaration]=========================*/

/**
 * @brief Tarea FreeRTOS para medición periódica de distancia
 * 
 * @param pvParameter Parámetro de entrada de la tarea (no utilizado)
 * 
 * @details Esta tarea se ejecuta de forma periódica cada PERIODO_MEDICION_MS ms.
 * En cada iteración:
 * - Lee la distancia del sensor HC-SR04
 * - Actualiza el display LCD (si HOLD no está activo)
 * - Controla los LEDs según el rango de distancia:
 *   * < 10 cm: todos los LEDs apagados
 *   * 10-19 cm: LED_1 encendido
 *   * 20-30 cm: LED_1 y LED_2 encendidos
 *   * > 30 cm: todos los LEDs encendidos
 * - Si medir_activa es false, apaga todos los LEDs y el LCD
 * 
 * @note Esta tarea es de ejecución continua (bucle infinito)
 */
void TareaMedicion(void *pvParameter);

/**
 * @brief Callback de interrupción para SWITCH_1 (Inicio/Pausa de medición)
 * 
 * @param args Parámetro de entrada (no utilizado)
 * 
 * @details Función de servicio de interrupción que se ejecuta cuando se
 * presiona SWITCH_1. Alterna el estado de medición (activa/inactiva).
 * 
 * @note Debe ser lo más corta y rápida posible (buena práctica ISR).
 * Por eso solo modifica la variable volatile medir_activa.
 */
void CambiarEstadoMedicion(void *args);

/**
 * @brief Callback de interrupción para SWITCH_2 (HOLD de lectura)
 * 
 * @param args Parámetro de entrada (no utilizado)
 * 
 * @details Función de servicio de interrupción que se ejecuta cuando se
 * presiona SWITCH_2. Alterna el estado de congelación de lectura (HOLD).
 * Esto congela/libera la actualización del display LCD manteniendo
 * las mediciones internas activas.
 * 
 * @note Debe ser lo más corta y rápida posible (buena práctica ISR).
 */
void CambiarEstadoHold(void *args);

/*==================[external functions definition]==========================*/

/**
 * @brief ISR para SWITCH_1 - Activar/Desactivar medición
 * 
 * @param args Parámetro de entrada de la ISR (no utilizado)
 * 
 * @details Alterna el estado de medición (medir_activa) entre activo e inactivo.
 * Se ejecuta automáticamente al presionar SWITCH_1.
 * Imprime un mensaje en la consola indicando que se presionó SWITCH_1.
 * 
 * @note Esta es una ISR, debe ser lo más breve posible.
 * Solo modifica una variable volatile (buena práctica).
 */
void CambiarEstadoMedicion(void *args)
{
    printf("Interrupción: Se presionó SWITCH_1\n");
    medir_activa = !medir_activa;
}

/**
 * @brief ISR para SWITCH_2 - Activar/Desactivar HOLD (congelación de lectura)
 * 
 * @param args Parámetro de entrada de la ISR (no utilizado)
 * 
 * @details Alterna el estado de HOLD (hold_activo) para congelar/liberar
 * la actualización del display LCD. Se ejecuta automáticamente al presionar SWITCH_2.
 * Las mediciones internas continúan cuando HOLD está activo.
 * Imprime un mensaje en la consola indicando que se presionó SWITCH_2.
 * 
 * @note Esta es una ISR, debe ser lo más breve posible.
 * Solo modifica una variable volatile (buena práctica).
 */
void CambiarEstadoHold(void *args)
{
    printf("Interrupción: Se presionó SWITCH_2\n");
    hold_activo = !hold_activo;
}


/**
 * @brief Tarea FreeRTOS - Medición periódica de distancia con HC-SR04
 * 
 * @param pvParameter Parámetro de entrada de la tarea (no utilizado)
 * 
 * @details Tarea de tiempo real que se ejecuta periódicamente cada PERIODO_MEDICION_MS.
 * 
 * Funcionalidad:
 * 1. Si medir_activa es true:
 *    - Lee distancia del sensor HC-SR04
 *    - Actualiza display LCD (solo si hold_activo es false)
 *    - Controla LEDs según rango de distancia:
 *      * < 10 cm: LEDs apagados
 *      * 10-19 cm: LED_1 encendido
 *      * 20-30 cm: LED_1 y LED_2 encendidos
 *      * > 30 cm: LED_1, LED_2 y LED_3 encendidos
 * 2. Si medir_activa es false:
 *    - Apaga todos los LEDs
 *    - Apaga el display LCD
 * 3. Espera PERIODO_MEDICION_MS antes de la siguiente iteración
 * 
 * @note Esta es una tarea de ejecución continua (bucle infinito).
 * Prioridad: 5 (medio)
 * Stack: 2048 bytes
 */
void TareaMedicion(void *pvParameter)
{
    while (1)
    {
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

        /* Esperar al siguiente período de medición */
        vTaskDelay(pdMS_TO_TICKS(PERIODO_MEDICION_MS));
    }

/**
 * @brief Función principal - Punto de entrada de la aplicación
 * 
 * @details Inicializa todos los periféricos y configura el sistema:
 * 1. Inicializa los drivers de hardware:
 *    - LEDs (3 indicadores luminosos)
 *    - Switches/Pulsadores (SWITCH_1 y SWITCH_2)
 *    - Sensor ultrasónico HC-SR04 (GPIO_3: TRIG, GPIO_2: ECHO)
 *    - Display LCD de 8 dígitos (ITS-E0803)
 * 2. Configura interrupciones para los pulsadores:
 *    - SWITCH_1 -> CambiarEstadoMedicion (activar/desactivar medición)
 *    - SWITCH_2 -> CambiarEstadoHold (congelar/liberar lectura LCD)
 * 3. Crea la tarea FreeRTOS de medición
 * 
 * @note Una vez que esta función termina, el scheduler de FreeRTOS
 * toma control y ejecuta las tareas creadas.
 */
void app_main(void)
{
    /* ============ Inicialización de Periféricos ============ */
    LedsInit();              /**< Inicializa driver de LEDs */
    SwitchesInit();          /**< Inicializa driver de pulsadores */
    HcSr04Init(GPIO_3, GPIO_2); /**< Inicializa sensor HC-SR04 (TRIG: GPIO_3, ECHO: GPIO_2) */
    LcdItsE0803Init();       /**< Inicializa display LCD */

    /* ============ Configuración de Interrupciones ============ */
    /* Configurar ISR para SWITCH_1 - Control de medición */
    SwitchActivInt(SWITCH_1, &CambiarEstadoMedicion, NULL);
    
    /* Configurar ISR para SWITCH_2 - Control de HOLD */
    SwitchActivInt(SWITCH_2, &CambiarEstadoHold, NULL);

    /* ============ Creación de Tareas FreeRTOS ============ */
    /* Crear tarea de medición periódica */
    xTaskCreate(&TareaMedicion,      /**< Función de la tarea */
                "Medicion",          /**< Nombre de la tarea (para depuración) */
                2048,                /**< Tamaño del stack (palabras) */
                NULL,                /**< Parámetro de entrada (no utilizado) */
                5,                   /**< Prioridad (0-configMAX_PRIORITIES-1) */
                &medicion_task_handle); /**< Handle para referencia posterior */
}