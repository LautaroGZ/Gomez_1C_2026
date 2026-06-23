/*! @mainpage Template
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>

/*==================[external functions definition]==========================*/

int8_t convertToBcdArray (uint32_t data, uint8_t digits, uint8_t * bcd_number) // devuelve un nro entero de 8 bits sin signo. Devuelve 1 si se pudo convertir, 0 si no se pudo convertir
{
    for (int8_t i = digits - 1; i >= 0; i--)
    {
        bcd_number[i] = data % 10; // si el número es 1234, en la primera iteración me devolverá el número 4, luego 3, luego 2 y luego 1. Gracias al digits -1 que me permite empezar a llenar el arreglo desde el último índice (3 en este caso) y luego ir llenando hacia el primer índice (0 en este caso).
        data = data / 10; // si el número es 1234, en la primera iteración me devolverá el número 123, luego 12, luego 1 y en 0 se termina el ciclo porque data es 0. 
    }

    return 1;
}
//uint32_t data es el número a convertir (entero sin signo de hasta 32 bits), uint8_t digits es la cantidad de dígitos que se quieren convertir (entero sin signo de hasta 8 bits), uint8_t * bcd_number es un puntero a un arreglo donde se guardará el resultado de la conversión (arreglo de enteros sin signo de hasta 8 bits).

//for (int8_t i = digits - 1; i >= 0; i--) es un ciclo que recorre el arreglo de dígitos desde el último dígito hasta el primero. En cada iteración, se asigna el valor del dígito correspondiente al arreglo bcd_number y se actualiza el valor de data dividiéndolo por 10 para obtener el siguiente dígito en la siguiente iteración. Ejemplo: si data es 1234 y digits es 4, en la primera iteración i será 3, se asignará bcd_number[3] = 1234 % 10 = 4, luego data se actualizará a 1234 / 10 = 123. En la segunda iteración i será 2, se asignará bcd_number[2] = 123 % 10 = 3, luego data se actualizará a 123 / 10 = 12. En la tercera iteración i será 1, se asignará bcd_number[1] = 12 % 10 = 2, luego data se actualizará a 12 / 10 = 1. En la cuarta iteración i será 0, se asignará bcd_number[0] = 1 % 10 = 1, luego data se actualizará a 1 / 10 = 0. Al final del ciclo, el arreglo bcd_number tendrá los valores {1, 2, 3, 4}.

void app_main(void)
{
    uint32_t number = 1234;   // número a convertir
    uint8_t digits = 4;       // cantidad de dígitos
    uint8_t bcd_array[4];     // arreglo donde guardar el BCD

    // llamar a la función
    convertToBcdArray(number, digits, bcd_array); //llamo a esta función y le digo: “Tomá este número, separalo en dígitos y guardalos en este arreglo”

    // imprimir resultado
    printf("Numero original: %lu\n", number); // %lu es el formato para imprimir un número entero sin signo de 32 bits (uint32_t)

    printf("BCD:\n");
    for(uint8_t i = 0; i < digits; i++) //acá recorro el arreglo bcd_array e imprimo cada uno de sus elementos 
    {
        printf("bcd_array[%d] = %d\n", i, bcd_array[i]); // %d es el formato para imprimir un número entero de 8 bits sin signo (uint8_t), y %d es el formato para imprimir un número entero de 8 bits sin signo (uint8_t) que representa el índice del arreglo.
    }
}