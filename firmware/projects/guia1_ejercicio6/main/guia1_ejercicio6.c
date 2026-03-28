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
#include "gpio_mcu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/


/*==================[Estructura GPIO]=======================================*/
typedef struct {
    gpio_t pin;
    io_t dir;
} gpioConf_t;

/*==================[PUNTO 4]==============================================*/
/*
 * Convierte un número en un array de dígitos BCD
 * Ej: 123 → [1,2,3]
 */
int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
{
    if (bcd_number == NULL) return -1;

    // Lleno desde el final para mantener orden correcto
    for (int i = digits - 1; i >= 0; i--) {
        bcd_number[i] = data % 10;  // obtengo último dígito
        data = data / 10;           // elimino ese dígito
    }

    return 0;
}

/*==================[PUNTO 5]==============================================*/
/*
 * Envía un dígito BCD a los pines (b0–b3)
 */
void bcdToGpio(uint8_t bcd, gpioConf_t *vec)
{
    for (int i = 0; i < 4; i++) {

        // Extraigo el bit i del número
        uint8_t bit = (bcd >> i) & 0x01;

        // Escribo ese bit en el GPIO correspondiente
        if (bit) {
            GPIOState(vec[i].pin, 1);
        } else {
            GPIOState(vec[i].pin, 0);
        }
    }
}

/*==================[EJERCICIO 6]==========================================*/
/*
 * Muestra un número en un display de múltiples dígitos
 */
void displayNumber(uint32_t data, uint8_t digits,
                   gpioConf_t *bcd_pins,
                   gpioConf_t *digit_pins)
{
    uint8_t bcd_array[digits];

    // Convierto el número a array de dígitos
    convertToBcdArray(data, digits, bcd_array);

    // Recorro cada dígito del display
    for (int i = 0; i < digits; i++) {

        // Apago todos los dígitos antes de cambiar datos
        for (int j = 0; j < digits; j++) {
            GPIOState(digit_pins[j].pin, 0);
        }

        // Envío el valor BCD del dígito actual
        bcdToGpio(bcd_array[i], bcd_pins);

        // Activo SOLO el dígito correspondiente
        GPIOState(digit_pins[i].pin, 1);

        // Pequeño delay para que parezca que no se borran
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/*==================[MAIN]=================================================*/
void app_main(void)
{
    /* Pines BCD (datos) */
    gpioConf_t bcd_pins[4] = {
        {GPIO_20, GPIO_OUTPUT}, // b0
        {GPIO_21, GPIO_OUTPUT}, // b1
        {GPIO_22, GPIO_OUTPUT}, // b2
        {GPIO_23, GPIO_OUTPUT}  // b3
    };

    /* Pines de selección de dígitos */
    gpioConf_t digit_pins[3] = {
        {GPIO_19, GPIO_OUTPUT}, // dígito 1
        {GPIO_18, GPIO_OUTPUT}, // dígito 2
        {GPIO_9,  GPIO_OUTPUT}  // dígito 3
    };

    // Inicializo todos los GPIOs
    for (int i = 0; i < 4; i++) {
        GPIOInit(bcd_pins[i].pin, bcd_pins[i].dir);
    }

    for (int i = 0; i < 3; i++) {
        GPIOInit(digit_pins[i].pin, digit_pins[i].dir);
    }

    uint32_t numero = 123;

    while (1) {
        // Mostrar número continuamente
        displayNumber(numero, 3, bcd_pins, digit_pins);
    }
}

/*==================[end of file]============================================*/