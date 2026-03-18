/*! @mainpage Blinking switch
 *
 * \section genDesc General Description
 *
 * This example makes LED_1 and LED_2 blink if SWITCH_1 or SWITCH_2 are pressed.
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
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "switch.h"
/*==================[macros and definitions]=================================*/
#define CONFIG_BLINK_PERIOD 1000

/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/
enum{MODE_ON,MODE_OFF,TOGGLE};
struct leds
{
	uint8_t mode;
	uint8_t n_led;
	uint8_t n_ciclos;
	uint8_t periodo;
} my_leds;


void manejar_leds(struct leds *leds)
{
    switch(leds->mode)
    {
        case MODE_OFF:  
            LedOff(leds->n_led);
            break;
            
        case MODE_ON:   
            LedOn(leds->n_led);
            break;
		
        case TOGGLE:
        {
            int i = 0;
            while(i < leds->n_ciclos*2)
            {
                LedToggle(leds->n_led);
                vTaskDelay(leds->periodo / portTICK_PERIOD_MS);
                i++;
            }
            break;
        }
    }
}

void app_main(void){
	LedsInit();

	my_leds.mode = TOGGLE;
	my_leds.n_led = LED_2;
	my_leds.n_ciclos = 10;
	my_leds.periodo = 500;

	while(1){
	    if(my_leds.mode == TOGGLE){
	        manejar_leds(&my_leds);
	    }

	    vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}


