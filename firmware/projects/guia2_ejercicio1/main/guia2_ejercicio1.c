/*! @mainpage Actividad 1 - Medidor de distancia por ultrasonido
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
 * | 08/04/2026 | Creación de la Actividad 1                     |
 *
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 *
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

/*==================[macros and definitions]=================================*/
#define PERIODO_MEDICION_MS 1000
#define PERIODO_TECLADO_MS 50
#define REFRESH_TICKS (PERIODO_MEDICION_MS/PERIODO_TECLADO_MS)


/*==================[internal data definition]===============================*/
bool medir_activa = true;
bool hold_activo = false;
uint16_t distancia_cm = 0;
TaskHandle_t medicion_task_handle = NULL;
TaskHandle_t teclas_task_handle = NULL;

/*==================[internal functions declaration]=========================*/
void TareaMedicion(void *pvParameter);
void TareaTeclas(void *pvParameter);

/*==================[external functions definition]==========================*/

/**
 * @brief Tarea encargada de medir la distancia y actualizar salidas (LEDs y LCD)
 */
void TareaMedicion(void *pvParameter)
{
    while (1)
    {
        if (medir_activa)
        {
            // Leer sensor
            distancia_cm = HcSr04ReadDistanceInCentimeters();

            if (!hold_activo)
            {
                // Mostrar en LCD
                LcdItsE0803Write(distancia_cm);
            }
            // Lógica de LEDs según la distancia
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
            // Sistema detenido
            LedsOffAll();
            LcdItsE0803Off(); // O usar una función para limpiar el display
        }

        // Refresco de 1 segundo
        vTaskDelay(pdMS_TO_TICKS(PERIODO_MEDICION_MS));
    }
}

/**
 * @brief Tarea encargada de leer el estado de las teclas por polling
 */
void TareaTeclas(void *pvParameter)
{
    uint8_t teclas;
    while (1)
    {
        teclas = SwitchesRead();

        // TEC1: Activa y detiene la medición
        if (teclas & SWITCH_1)
        {
            medir_activa = !medir_activa;
        }

        // TEC2: Mantiene el resultado (HOLD)
        if (teclas & SWITCH_2)
        {
            hold_activo = !hold_activo;
        }

        // Retardo
        vTaskDelay(REFRESH_TICKS);
    }
}

void app_main(void)
{
    // 1. Inicialización de periféricos y drivers
    LedsInit();
    SwitchesInit();
    HcSr04Init(GPIO_3, GPIO_2); // ECHO = GPIO_3, TRIGGER = GPIO_2
    LcdItsE0803Init();

    // 2. Creación de tareas de FreeRTOS
    xTaskCreate(&TareaMedicion, "Medicion", 2048, NULL, 5, &medicion_task_handle);
    xTaskCreate(&TareaTeclas, "Teclado", 1024, NULL, 5, &teclas_task_handle);
}
/*==================[end of file]============================================*/