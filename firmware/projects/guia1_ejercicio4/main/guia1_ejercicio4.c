/*! @mainpage Template
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>

/*==================[external functions definition]==========================*/

int8_t convertToBcdArray (uint32_t data, uint8_t digits, uint8_t * bcd_number)
{
    for (int8_t i = digits - 1; i >= 0; i--)
    {
        bcd_number[i] = data % 10;
        data = data / 10;
    }

    return 1;
}


void app_main(void)
{
    uint32_t number = 1234;   // número a convertir
    uint8_t digits = 4;       // cantidad de dígitos
    uint8_t bcd_array[4];     // arreglo donde guardar el BCD

    // llamar a la función
    convertToBcdArray(number, digits, bcd_array);

    // imprimir resultado
    printf("Numero original: %lu\n", number);

    printf("BCD:\n");
    for(uint8_t i = 0; i < digits; i++)
    {
        printf("bcd_array[%d] = %d\n", i, bcd_array[i]);
    }
}