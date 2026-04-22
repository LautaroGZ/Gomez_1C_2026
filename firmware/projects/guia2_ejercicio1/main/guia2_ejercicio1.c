/*! @mainpage Firmware para placa EDU-ESP que mide distancia utilizando un sensor ultrasónico HC-SR04
 *
 * @section genDesc General Description
 *
 * Firmware para la placa EDU-ESP que mide distancia utilizando un sensor
 * ultrasónico HC-SR04. Muestra la distancia en un display LCD y controla
 * un arreglo de LEDs según rangos de distancia. Permite pausar/reanudar
 * la medición y mantener (hold) el valor en pantalla usando pulsadores.
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32       |
 * |:--------------:|:--------------|
 * |    ECHO        |   GPIO_3      |
 * |    TRIGGER     |   GPIO_2      |
 * |    +5V         |   +5V         |
 * |    GND         |   GND         |
 *
 * @section changelog Changelog
 *
 * |   Date     | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 22/04/2026 | Creación del proyecto                    |
 *
 * @author Lautaro Gómez (lautaro.gomez@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
/**
 * @file guia2_ejercicio1.c
 * @brief Medidor de distancia por ultrasonido con visualización en LCD
 * 
 * Sistema de medición de distancia basado en sensor ultrasónico HC-SR04
 * con control de LEDs y display LCD para visualización de resultados.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "switch.h"
#include "hc_sr04.h"
#include "lcditse0803.h"

/*==================[macros and definitions]=================================*/
/** 
 * @def PERIODO_MEDICION_MS
 * @brief Período de actualización de la medición de distancia en milisegundos
 * @details Se ejecuta la lectura del sensor cada 1000 ms (1 segundo)
 */
#define PERIODO_MEDICION_MS 1000

/** 
 * @def PERIODO_TECLADO_MS
 * @brief Período de escaneo del teclado en milisegundos
 * @details Se realiza polling de las teclas cada 50 ms
 */
#define PERIODO_TECLADO_MS 50

/** 
 * @def REFRESH_TICKS
 * @brief Factor de escala para convertir milisegundos a ticks de FreeRTOS
 * @details Calcula cuántos períodos de teclado caben en un período de medición
 */
#define REFRESH_TICKS (PERIODO_MEDICION_MS/PERIODO_TECLADO_MS)


/*==================[internal data definition]===============================*/
/**
 * @var medir_activa
 * @brief Bandera que controla si la medición está activa o pausada
 * @details Inicializada en true. Se alterna con la pulsación de TEC1
 */
bool medir_activa = true;

/**
 * @var hold_activo
 * @brief Bandera que controla si el display está en modo hold (retención)
 * @details Inicializada en false. Se alterna con la pulsación de TEC2.
 *          Cuando está activa, el valor en LCD no se actualiza
 */
bool hold_activo = false;

/**
 * @var distancia_cm
 * @brief Valor actual de distancia medida por el sensor en centímetros
 * @details Se actualiza en cada ciclo de la tarea de medición
 */
uint16_t distancia_cm = 0;

/**
 * @var medicion_task_handle
 * @brief Handle de la tarea de medición de distancia
 * @details Se utiliza para gestionar la tarea de medición
 */
TaskHandle_t medicion_task_handle = NULL;

/**
 * @var teclas_task_handle
 * @brief Handle de la tarea de lectura de teclas
 * @details Se utiliza para gestionar la tarea de lectura del teclado
 */
TaskHandle_t teclas_task_handle = NULL;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Tarea encargada de medir la distancia y actualizar salidas
 * @param[in] pvParameter Parámetro de la tarea (no utilizado)
 * @return void
 * 
 * @details
 * - Realiza la lectura del sensor ultrasónico HC-SR04
 * - Actualiza el valor en el display LCD (si no está en hold)
 * - Controla el encendido de LEDs según rangos de distancia:
 *   - < 10 cm: Todos los LEDs apagados
 *   - 10-20 cm: LED_1 encendido
 *   - 20-30 cm: LED_1 y LED_2 encendidos
 *   - > 30 cm: Todos los LEDs encendidos
 * - Período de ejecución: 1000 ms
 */
void TareaMedicion(void *pvParameter);

/**
 * @brief Tarea encargada de leer el estado de las teclas por polling
 * @param[in] pvParameter Parámetro de la tarea (no utilizado)
 * @return void
 * 
 * @details
 * - Lee el estado de las teclas cada 50 ms
 * - TEC1: Alterna entre pausar/reanudar la medición (medir_activa)
 * - TEC2: Alterna entre modo hold/normal del display (hold_activo)
 * - Período de ejecución: 50 ms
 */
void TareaTeclas(void *pvParameter);

/*==================[external functions definition]==========================*/

void TareaMedicion(void *pvParameter)
{
    while (1)
    {
        if (medir_activa)
        {
            /**
             * @brief Lectura del sensor ultrasónico HC-SR04
             * Obtiene la distancia en centímetros
             */
            distancia_cm = HcSr04ReadDistanceInCentimeters();

            if (!hold_activo)
            {
                /**
                 * @brief Actualización del display LCD
                 * Solo se actualiza si no está en modo hold
                 */
                LcdItsE0803Write(distancia_cm);
            }
            
            /**
             * @brief Control de LEDs según rangos de distancia
             * Los LEDs se encienden progresivamente conforme aumenta la distancia
             */
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
            /**
             * @brief Sistema pausado
             * Apaga todos los LEDs y desactiva el display
             */
            LedsOffAll();
            LcdItsE0803Off();
        }

        /**
         * @brief Espera antes del siguiente ciclo de medición
         * Período: 1000 ms
         */
        vTaskDelay(pdMS_TO_TICKS(PERIODO_MEDICION_MS));
    }
}

void TareaTeclas(void *pvParameter)
{
    /**
     * @var teclas
     * @brief Variable local para almacenar el estado de las teclas
     * @details Cada bit representa el estado de una tecla específica
     */
    uint8_t teclas;
    
    while (1)
    {
        /**
         * @brief Lectura del estado actual de todas las teclas
         */
        teclas = SwitchesRead();

        /**
         * @brief Procesamiento de TEC1
         * Alterna entre pausar y reanudar la medición
         * - Si tecla presionada: medir_activa = !medir_activa
         */
        if (teclas & SWITCH_1)
        {
            medir_activa = !medir_activa;
        }

        /**
         * @brief Procesamiento de TEC2
         * Alterna el modo hold (retención) del display
         * - Si tecla presionada: hold_activo = !hold_activo
         */
        if (teclas & SWITCH_2)
        {
            hold_activo = !hold_activo;
        }

        /**
         * @brief Espera antes del siguiente escaneo de teclado
         * Período: 50 ms
         */
        vTaskDelay(REFRESH_TICKS);
    }
}

/**
 * @brief Función principal de la aplicación
 * @details
 * - Inicializa todos los periféricos (LEDs, switches, sensor HC-SR04, LCD)
 * - Crea las tareas de FreeRTOS para medición y lectura de teclado
 * - Prioridades:
 *   - TareaMedicion: Prioridad 5
 *   - TareaTeclas: Prioridad 5
 * - Stack size:
 *   - TareaMedicion: 2048 bytes
 *   - TareaTeclas: 1024 bytes
 */
void app_main(void)
{
    /**
     * @section init_perifericos Inicialización de Periféricos y Drivers
     */
    
    /**
     * @brief Inicialización del módulo de LEDs
     * Configura los GPIO asociados a los LEDs como salidas
     */
    LedsInit();
    
    /**
     * @brief Inicialización del módulo de switches/botones
     * Configura los GPIO de entrada para lectura de teclado
     */
    SwitchesInit();
    
    /**
     * @brief Inicialización del sensor ultrasónico HC-SR04
     * @param GPIO_3 Pin de ECHO (entrada)
     * @param GPIO_2 Pin de TRIGGER (salida)
     */
    HcSr04Init(GPIO_3, GPIO_2);
    
    /**
     * @brief Inicialización del display LCD ITS-E0803
     * Configura comunicación I2C y display para mostrar valores
     */
    LcdItsE0803Init();

    /**
     * @section create_tasks Creación de Tareas de FreeRTOS
     */
    
    /**
     * @brief Tarea de medición
     * - Nombre: "Medicion"
     * - Stack: 2048 bytes
     * - Prioridad: 5
     */
    xTaskCreate(&TareaMedicion, "Medicion", 2048, NULL, 5, &medicion_task_handle);
    
    /**
     * @brief Tarea de teclado
     * - Nombre: "Teclado"
     * - Stack: 1024 bytes
     * - Prioridad: 5
     */
    xTaskCreate(&TareaTeclas, "Teclado", 1024, NULL, 5, &teclas_task_handle);
}
/*==================[end of file]============================================*/