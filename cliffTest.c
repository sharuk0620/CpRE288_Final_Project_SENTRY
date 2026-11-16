/*
 * cliffTest.c
 *
 *  Created on: Nov 15, 2026
 *      Author: ssharuk
 */

#include "movement.h"
#include "open_interface.h"
#include "uart.h"
#include <string.h>

char message[50];

void sendMessage(char messageSent[]);
//CLIFF SENSOR:

//Black/Felt for Hole - Value <= 250
//Normal Gray Floor - 250 <= Value <= 2500
//White Tape for boundary Value >= 2500


int main(void)
{

    //Initializes UART functionality for cyBot
    uart_init();


    //Initializes sensor_data struct to be used in movement functions
    oi_t *sensor_data = oi_alloc();
    oi_init(sensor_data);


    cliff_move_forward(sensor_data, 250);


    /*
    while(1){
        sprintf(message, "Left: %d\n\rRight: %d\n\n\r", sensor_data->cliffFrontLeftSignal, sensor_data->cliffFrontRightSignal);
        sendMessage(message);
        memset(message, 0, sizeof(message)); //clears message to be reused

        oi_update(sensor_data);
    }
    */


    //Frees the sensor_data, and de-initializes it.
    oi_free(sensor_data);

    return 0;
}


//Sends a message to putty byte by byte
void sendMessage(char messageSent[])
{
//While not equal to null
    while (*messageSent != '\0')
    {
        uart_sendChar(*messageSent);
        messageSent++;
    }
}
