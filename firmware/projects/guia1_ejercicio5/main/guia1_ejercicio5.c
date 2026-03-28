/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 12/09/2023 | Document creation		                         |
 *
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"

/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/
/*
 * Estructura que define un GPIO:
 * - pin: número de pin físico
 * - dir: dirección (entrada o salida)
 */
typedef struct {
    gpio_t pin;
    io_t dir;
} gpioConf_t;

/*
 * Función: bcdToGpio
 * ------------------
 * Recibe:
 *   - bcd: número de 4 bits (0–9 en BCD)
 *   - vec: vector de 4 GPIOs
 *
 * Función:
 *   Toma cada bit del número y lo escribe en un GPIO.
 */
void bcdToGpio(uint8_t bcd, gpioConf_t *vec)
{
    // Recorro los 4 bits del número (b0 a b3)
    for (int i = 0; i < 4; i++) {

        /*
         * (bcd >> i) desplaza el bit i a la posición menos significativa
         * & 0x01 (0000 0001) permite quedarnos solo con ese bit
         *
         * Resultado:
         *   bit = 0 o 1
         */
        uint8_t bit = (bcd >> i) & 0x01;

        /*
         * Si el bit vale 1 → pongo el GPIO en alto
         * Si vale 0 → lo pongo en bajo
         */
        if (bit) {
            GPIOState(vec[i].pin, 1);
        } else {
            GPIOState(vec[i].pin, 0);
        }
    }
}

/*
 * Función principal del sistema 
 */
void app_main(void)
{
    /*
     * Vector de configuración de GPIOs
     * Cada posición corresponde a un bit:
     *   vec[0] → bit 0 (b0)
     *   vec[1] → bit 1 (b1)
     *   vec[2] → bit 2 (b2)
     *   vec[3] → bit 3 (b3)
     */
    gpioConf_t gpioVector[4] = {
        {GPIO_20, GPIO_OUTPUT}, // b0
        {GPIO_21, GPIO_OUTPUT}, // b1
        {GPIO_22, GPIO_OUTPUT}, // b2
        {GPIO_23, GPIO_OUTPUT}  // b3
    };

    /*
     * Inicializo cada GPIO como salida
     */
    for (int i = 0; i < 4; i++) {
        GPIOInit(gpioVector[i].pin, gpioVector[i].dir);
    }

    /*
     * Número a mostrar en BCD
     * Ejemplo: 5 → 0101
     */
    uint8_t numero = 5;


    while(1) {

        // Envío el número a los GPIOs (bit a bit)
        bcdToGpio(numero, gpioVector);

        // Delay de 1 segundo
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/*==================[end of file]============================================*/