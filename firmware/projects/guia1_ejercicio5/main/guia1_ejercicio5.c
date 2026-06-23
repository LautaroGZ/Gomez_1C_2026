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
//“Esto es una ficha que le dice al programa qué pin usar y cómo configurarlo”
//gpioConf_t es una estructura que contiene dos campos: pin, que es de tipo gpio_t y representa el número de pin físico del microcontrolador; y dir, que es de tipo io_t y representa la dirección del GPIO (si es entrada o salida). Esta estructura se utiliza para configurar los GPIOs que se van a usar en el programa, indicando qué pin se va a usar y cómo se va a configurar (entrada o salida).
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
void bcdToGpio(uint8_t bcd, gpioConf_t *vec) // recibe un número de 4 bits sin signo (0–9 en BCD) y un vector de 4 GPIOs, y toma cada bit del número y lo escribe en un GPIO.
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
        uint8_t bit = (bcd >> i) & 0x01; // (bcd >> i) desplaza el bit i a la posición menos significativa, y & 0x01 (0000 0001) permite quedarnos solo con ese bit. El resultado es que bit será igual a 0 o 1 dependiendo del valor del bit i en el número bcd. Por ejemplo, si bcd es 5 (0101 en binario) y i es 0, entonces (bcd >> i) será 0101 >> 0 = 0101, y (bcd >> i) & 0x01 será 0101 & 0001 = 0001, por lo que bit será igual a 1. Si i es 1, entonces (bcd >> i) será 0101 >> 1 = 0010, y (bcd >> i) & 0x01 será 0010 & 0001 = 0000, por lo que bit será igual a 0. Si i es 2, entonces (bcd >> i) será 0101 >> 2 = 0001, y (bcd >> i) & 0x01 será 0001 & 0001 = 0001, por lo que bit será igual a 1. Si i es 3, entonces (bcd >> i) será 0101 >> 3 = 0000, y (bcd >> i) & 0x01 será 0000 & 0001 = 0000, por lo que bit será igual a 0.
        // esto sirve para extraer cada bit del número bcd y asignarlo a la variable bit, que luego se usará para determinar si el GPIO correspondiente se pone en alto o en bajo.
        /*
         * Si el bit vale 1 → pongo el GPIO en alto
         * Si vale 0 → lo pongo en bajo
         */
        if (bit) {
            GPIOState(vec[i].pin, 1); // Si el bit vale 1, pongo el GPIO en alto usando la función GPIOState, que recibe el número de pin y el estado (1 para alto, 0 para bajo). En este caso, vec[i].pin es el número de pin del GPIO correspondiente al bit i, y 1 indica que se debe poner en alto.
        } else {
            GPIOState(vec[i].pin, 0); // Si el bit vale 0, pongo el GPIO en bajo usando la función GPIOState, que recibe el número de pin y el estado (1 para alto, 0 para bajo). En este caso, vec[i].pin es el número de pin del GPIO correspondiente al bit i, y 0 indica que se debe poner en bajo.
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
    gpioConf_t gpioVector[4] = { //defino un vector de configuración de GPIOs llamado gpioVector, que es un arreglo de 4 elementos de tipo gpioConf_t. Cada elemento del arreglo corresponde a un bit del número BCD que se va a mostrar en los GPIOs. En este caso, se asignan los GPIOs 20, 21, 22 y 23 como salidas para representar los bits b0, b1, b2 y b3 respectivamente.
        {GPIO_20, GPIO_OUTPUT}, // b0
        {GPIO_21, GPIO_OUTPUT}, // b1
        {GPIO_22, GPIO_OUTPUT}, // b2
        {GPIO_23, GPIO_OUTPUT}  // b3
    };

    /*
     * Inicializo cada GPIO como salida
     */
    for (int i = 0; i < 4; i++) {
        GPIOInit(gpioVector[i].pin, gpioVector[i].dir); // Inicializo cada GPIO como salida usando la función GPIOInit, que recibe el número de pin y la dirección (entrada o salida). En este caso, gpioVector[i].pin es el número de pin del GPIO correspondiente al bit i, y gpioVector[i].dir es la dirección (GPIO_OUTPUT) que indica que se debe configurar como salida. Tiene i debido a que en cada iteración se vera el elemento del arreglo gpioVector correspondiente al bit i y se inicializará ese GPIO como salida.
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
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay de 1 segundo usando la función vTaskDelay, que recibe el tiempo de retraso en ticks. En este caso, se usa la macro pdMS_TO_TICKS para convertir 1000 milisegundos (1 segundo) a ticks, lo que hace que el programa espere 1 segundo antes de volver a enviar el número a los GPIOs. Esto permite que el número se muestre en los GPIOs durante 1 segundo antes de actualizarlo nuevamente.
    }
}
/*==================[end of file]============================================*/