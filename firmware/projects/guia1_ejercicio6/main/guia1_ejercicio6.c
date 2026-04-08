/*! @mainpage Código para manejar un display BCD mediante una placa ESP32C6
 *
 * @section genDesc General Description
 *
 * Este programa implementa la multiplexación de un display de 7 segmentos 
 * de múltiples dígitos utilizando un decodificador BCD en un entorno FreeRTOS.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |   Display      |   EDU-CIAA	|
 * |:--------------:|:-------------:|
 * | 	Vcc 	    |	5V      	|
 * | 	BCD1		| 	GPIO_20		|
 * | 	BCD2	 	| 	GPIO_21		|
 * | 	BCD3	 	| 	GPIO_22		|
 * | 	BCD4	 	| 	GPIO_23		|
 * | 	SEL1	 	| 	GPIO_19		|
 * | 	SEL2	 	| 	GPIO_18		|
 * | 	SEL3	 	| 	GPIO_9		|
 * | 	Gnd 	    | 	GND     	|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 08/04/2026 | Document creation		                         |
 *
 * @author Lautaro Gómez (lautaro.gomez@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
/**
 * @brief Librerías estándar de C para entrada/salida y tipos de datos.
 */
#include <stdio.h>
#include <stdint.h>

/**
 * @brief Driver personalizado para manejo de GPIO.
 * Proporciona funciones para inicializar, configurar y cambiar el estado de pines GPIO.
 */
#include "gpio_mcu.h"

/**
 * @brief Librerías de FreeRTOS para gestión de tareas y sincronización.
 * Se utiliza vTaskDelay para los retardos de multiplexación.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*==================[macros and definitions]=================================*/
/**
 * @defgroup DisplayConfig Configuración del Display
 * @brief Parámetros de configuración para el display BCD.
 * @{
 */

/** @brief Cantidad de dígitos en el display (3 dígitos) */
#define DISPLAY_DIGITS 3

/** @brief Cantidad de bits necesarios para codificar un dígito BCD (4 bits) */
#define BCD_BITS 4

/** @brief Retardo en milisegundos para la multiplexación del display (persistencia visual) */
#define DISPLAY_MULTIPLEX_DELAY_MS 5

/** @} */

/*==================[internal data definition]===============================*/
/**
 * @brief Almacena datos internos del estado del display.
 * 
 * Estos datos se utilizan para mantener el estado actual del número
 * mostrado en el display. (Se puede extender con buffers adicionales si es necesario)
 */

/*==================[internal functions declaration]=========================*/
/**
 * @brief Declaraciones internas de funciones auxiliares.
 * Las funciones principales están declaradas y documentadas en las secciones siguientes.
 */

/*==================[external functions definition]==========================*/


/*==================[Estructura GPIO]=======================================*/
/**
 * @defgroup StructDefinitions Definiciones de Estructuras
 * @brief Estructuras de datos utilizadas en la aplicación.
 * @{
 */

/**
 * @struct gpioConf_t
 * @brief Estructura de configuración para los pines GPIO.
 * 
 * Agrupa el número de pin y su dirección (entrada o salida) para 
 * facilitar la inicialización y el manejo de los periféricos GPIO de forma modular.
 * 
 * @details
 * Esta estructura es utilizada para:
 * - Configurar los pines de datos BCD (4 pines)
 * - Configurar los pines de selección de dígitos (3 pines)
 * 
 * @example
 * @code
 * gpioConf_t pin_config = {GPIO_20, GPIO_OUTPUT};
 * GPIOInit(pin_config.pin, pin_config.dir);
 * @endcode
 */
typedef struct {
    gpio_t pin; /**< Número del pin GPIO correspondiente (ej. GPIO_20, GPIO_21). */
    io_t dir;   /**< Dirección del pin (GPIO_OUTPUT para salida, GPIO_INPUT para entrada). */
} gpioConf_t;

/** @} */

/*==================[PUNTO 4]==============================================*/
/**
 * @defgroup ConversionFunctions Funciones de Conversión
 * @brief Funciones para convertir datos a formato BCD.
 * @{
 */

/**
 * @brief Convierte un número entero en un arreglo de dígitos BCD.
 * 
 * Toma un número (ej. 123) y extrae sus dígitos individuales, almacenándolos 
 * en orden dentro de un arreglo provisto (ej. bcd_number[0]=1, bcd_number[1]=2, bcd_number[2]=3).
 *
 * @param data Número entero de 32 bits sin signo que se desea convertir.
 * @param digits Cantidad de dígitos que componen el número y tamaño del arreglo destino.
 * @param bcd_number Puntero al arreglo donde se almacenarán los dígitos resultantes.
 *                    Debe tener al menos @p digits elementos.
 * 
 * @return int8_t
 *   - @retval 0 Conversión realizada exitosamente.
 *   - @retval -1 Error: puntero @p bcd_number es NULL.
 * 
 * @note
 * - Los dígitos se almacenan en orden de mayor a menor significancia.
 * - La función itera desde el final del arreglo hacia el principio.
 * - No valida si el número tiene más dígitos que los especificados.
 * 
 * @example
 * @code
 * uint8_t digitos[3];
 * int8_t resultado = convertToBcdArray(123, 3, digitos);
 * // Resultado: digitos[] = {1, 2, 3}
 * @endcode
 * 
 * @sa bcdToGpio()
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

/** @} */

/*==================[PUNTO 5]==============================================*/
/**
 * @defgroup OutputFunctions Funciones de Salida
 * @brief Funciones para transferir datos a los pines GPIO.
 * @{
 */

/**
 * @brief Envía un dígito BCD a un conjunto de pines GPIO.
 * 
 * Lee cada bit de un número BCD (de 4 bits) y establece el estado alto (1) 
 * o bajo (0) en los pines GPIO correspondientes para controlar un decodificador BCD.
 *
 * @param bcd Dígito BCD a enviar (valor entre 0 y 9).
 *            Valores válidos: 0-9. Valores > 9 mostrarán en binario los 4 bits menos significativos.
 * @param vec Puntero al arreglo de configuración de pines GPIO.
 *            Debe contener exactamente 4 elementos (b0, b1, b2, b3).
 *            El orden debe ser: vec[0]=bit0, vec[1]=bit1, vec[2]=bit2, vec[3]=bit3
 * 
 * @details
 * La función extrae cada uno de los 4 bits del número BCD mediante operaciones
 * de desplazamiento y enmascaramiento, luego establece cada pin GPIO al valor correspondiente.
 * 
 * @algorithm
 * - Bit 0 (LSB): (bcd >> 0) & 0x01 -> vec[0]
 * - Bit 1:       (bcd >> 1) & 0x01 -> vec[1]
 * - Bit 2:       (bcd >> 2) & 0x01 -> vec[2]
 * - Bit 3 (MSB): (bcd >> 3) & 0x01 -> vec[3]
 * 
 * @example
 * @code
 * // Para mostrar el número 5 (binario: 0101)
 * gpioConf_t bcd_pins[4] = {
 *     {GPIO_20, GPIO_OUTPUT}, // bit 0
 *     {GPIO_21, GPIO_OUTPUT}, // bit 1
 *     {GPIO_22, GPIO_OUTPUT}, // bit 2
 *     {GPIO_23, GPIO_OUTPUT}  // bit 3
 * };
 * bcdToGpio(5, bcd_pins);
 * // Resultado: GPIO_20=1, GPIO_21=0, GPIO_22=1, GPIO_23=0
 * @endcode
 * 
 * @note
 * - Los GPIOs deben estar previamente inicializados como GPIO_OUTPUT.
 * - Solo usa los 4 bits menos significativos del valor @p bcd.
 * - La función no valida los valores de entrada ni comprueba si son punteros válidos.
 * 
 * @sa convertToBcdArray(), displayNumber()
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

/** @} */

/*==================[EJERCICIO 6]==========================================*/
/**
 * @defgroup DisplayFunctions Funciones de Control del Display
 * @brief Funciones principales para controlar la visualización en el display BCD.
 * @{
 */

/**
 * @brief Muestra un número en un display de múltiples dígitos mediante multiplexación.
 * 
 * Implementa la técnica de multiplexación temporal para mostrar simultáneamente
 * múltiples dígitos en un display compartido. Convierte el número a BCD y luego itera
 * sobre cada dígito del display. Para evitar el solapamiento visual, apaga todos los 
 * dígitos, envía el valor del dígito actual a los pines de datos BCD, activa el dígito 
 * correspondiente y realiza un retardo bloqueante de FreeRTOS (vTaskDelay) para 
 * asegurar la persistencia visual.
 *
 * @param data Número entero sin signo que se desea mostrar en el display.
 * @param digits Cantidad total de dígitos que tiene el display.
 *               Debe coincidir con el tamaño de los arreglos pasados.
 * @param bcd_pins Puntero al arreglo de configuración de los pines de datos BCD.
 *                 Debe contener exactamente 4 elementos para los bits b0, b1, b2, b3.
 * @param digit_pins Puntero al arreglo de configuración de los pines de selección de dígito.
 *                   Debe contener @p digits elementos para activar cada dígito del display.
 * 
 * @details
 * El proceso de multiplexación sigue estos pasos para cada dígito:
 * 1. Desactiva todos los dígitos (establece en 0)
 * 2. Extrae el dígito actual del número (usando convertToBcdArray)
 * 3. Envía el valor BCD a los pines de datos (usando bcdToGpio)
 * 4. Activa solamente el dígito actual (establece en 1)
 * 5. Realiza un retardo de 5 ms (dMS_TO_TICKS) para la persistencia visual (POV)
 * 6. Repite para el siguiente dígito
 *
 * @note
 * - Esta función es bloqueante debido a vTaskDelay().
 * - No debe ser llamada desde ISRs (Interrupt Service Routines).
 * - El retardo de 5 ms es configurable mediante DISPLAY_MULTIPLEX_DELAY_MS.
 * - La persistencia visual humana (~30-60 Hz) hace que a 5 ms por dígito el display
 *   parezca estar mostrand os los dígitos simultáneamente.
 * 
 * @example
 * @code
 * // Configuración de pines
 * gpioConf_t bcd_pins[4] = {
 *     {GPIO_20, GPIO_OUTPUT},  // b0
 *     {GPIO_21, GPIO_OUTPUT},  // b1
 *     {GPIO_22, GPIO_OUTPUT},  // b2
 *     {GPIO_23, GPIO_OUTPUT}   // b3
 * };
 * gpioConf_t digit_pins[3] = {
 *     {GPIO_19, GPIO_OUTPUT},  // dígito 1
 *     {GPIO_18, GPIO_OUTPUT},  // dígito 2
 *     {GPIO_9,  GPIO_OUTPUT}   // dígito 3
 * };
 * 
 * // Mostrar el número 123 en un display de 3 dígitos
 * displayNumber(123, 3, bcd_pins, digit_pins);
 * @endcode
 *
 * @sa convertToBcdArray(), bcdToGpio(), app_main()
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
        // Permite que el ojo humano perciba el dígito (persistencia visual)
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_MULTIPLEX_DELAY_MS));
    }
}

/** @} */

/*==================[MAIN]=================================================*/
/**
 * @defgroup MainApplication Aplicación Principal
 * @brief Punto de entrada y función principal de la aplicación.
 * @{
 */

/**
 * @brief Función principal de la aplicación (punto de entrada).
 * 
 * Configura e inicializa todos los pines GPIO necesarios:
 * - Bus de datos BCD (4 pines): GPIO_20, GPIO_21, GPIO_22, GPIO_23
 * - Señales de control de dígitos (3 pines): GPIO_19, GPIO_18, GPIO_9
 * 
 * Luego ejecuta un bucle infinito encargándose de la multiplexación periódica
 * del display para mostrar el número almacenado en la variable `numero`.
 *
 * @details
 * La función realiza los siguientes pasos:
 * 1. Define dos arreglos de estructuras gpioConf_t:
 *    - bcd_pins[]: Configuración de los 4 pines para los bits BCD (b0, b1, b2, b3)
 *    - digit_pins[]: Configuración de los 3 pines para la selección de dígito
 * 2. Inicializa todos los pines GPIO usando GPIOInit()
 * 3. Entra en un bucle infinito llamando a displayNumber() para mostrar el número
 *
 * @note
 * - Esta función no retorna; es el punto de entrada de la aplicación ESP32-C6.
 * - El número mostrado actualmente es fijo (100), pero puede modificarse
 *   para recibir entrada de otras tareas o módulos.
 * - Se recomienda mover la multiplexación a una tarea separada de FreeRTOS
 *   para aplicaciones más complejas que requieran multitarea.
 *
 * @hardware
 * **Tabla de Conexión Hardware (ESP32-C6 -> Display BCD):**
 * | Función            | Pin ESP32-C6 | Display BCD |
 * |:------------------:|:------------:|:-----------:|
 * | BCD Bit 0 (b0)     | GPIO_20      | BCD1        |
 * | BCD Bit 1 (b1)     | GPIO_21      | BCD2        |
 * | BCD Bit 2 (b2)     | GPIO_22      | BCD3        |
 * | BCD Bit 3 (b3)     | GPIO_23      | BCD4        |
 * | Selección Dígito 1 | GPIO_19      | SEL1        |
 * | Selección Dígito 2 | GPIO_18      | SEL2        |
 * | Selección Dígito 3 | GPIO_9       | SEL3        |
 * | Alimentación       | 5V           | Vcc         |
 * | Tierra             | GND          | Gnd         |
 *
 * @example
 * @code
 * // Esta función se ejecuta automáticamente al encender el dispositivo.
 * // Muestra "100" en el display de 3 dígitos de forma continua mediante
 * // multiplexación a 5 ms por dígito (~200 Hz de refresco total).
 * @endcode
 *
 * @sa displayNumber(), convertToBcdArray(), bcdToGpio()
 */
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

    uint32_t numero = 100;

    while (1) {
        // Mostrar número continuamente mediante multiplexación
        displayNumber(numero, 3, bcd_pins, digit_pins);
    }
}

/** @} */

/*==================[end of file]============================================*/
/**
 * @file guia1_ejercicio6.c
 * @brief Implementación de un controlador de display BCD con multiplexación en ESP32-C6
 * 
 * @author Lautaro Gómez (lautaro.gomez@ingenieria.uner.edu.ar)
 * @date 08/04/2026
 * @version 1.0
 * 
 * @details
 * Este módulo implementa el control de un display de 7 segmentos con múltiples dígitos
 * usando un decodificador BCD (Binary-Coded Decimal) en lugar de controlar directamente
 * los segmentos. La multiplexación temporal permite mostrar todos los dígitos con un
 * conjunto reducido de pines GPIO, aprovechando la persistencia visual del ojo humano.
 * 
 * @section intro Introducción
 * El proyecto utiliza la técnica de multiplexación dinámica para controlar un display
 * de 3 dígitos con 7 segmentos cada uno, usando solo 7 pines GPIO:
 * - 4 pines para el bus de datos BCD
 * - 3 pines para seleccionar qué dígito está activo en cada momento
 * 
 * @section algo Algoritmo
 * El algoritmo es simple pero efectivo:
 * 1. Dividir el número en dígitos individuales
 * 2. Para cada dígito:
 *    - Apagar todos los dígitos
 *    - Enviar el dígito actual al decodificador BCD
 *    - Activar el dígito actual
 *    - Esperar ~5 ms (persistencia visual)
 * 3. Repetir continuamente
 *
 * @section deps Dependencias
 * - **gpio_mcu.h**: Driver de GPIO personalizado (funciones GPIOInit, GPIOState)
 * - **FreeRTOS**: Kernel de tiempo real para tareas y retardos (vTaskDelay)
 * - **Estándares C**: stdio.h, stdint.h para tipos de datos
 * 
 * @section compilation Compilación
 * Este archivo forma parte de un proyecto ESP-IDF y se compila con:
 * ```
 * idf.py build
 * ```
 * 
 * @section testing Pruebas
 * El programa muestra el número 123 en un display de 3 dígitos de forma continua.
 * Para cambiar el número mostrado, modificar el valor de la variable `numero` en app_main().
 */