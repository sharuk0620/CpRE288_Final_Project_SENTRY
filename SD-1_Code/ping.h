/*
 * ping.h
 *
 *  Created on: Oct 20, 2026
 *      Author: nullpncl
 */

#ifndef PING_H_
#define PING_H_

#include <inc/tm4c123gh6pm.h>
#include <stdbool.h>
#include <stdint.h>
#include "driverlib/interrupt.h"

volatile enum
{
    LOW, HIGH, DONE
} state;  // set by ISR

volatile unsigned int rising_time;        // Pulse start time: Set by ISR
volatile unsigned int falling_time;       // Pulse end time: Set by ISR

extern int overflows;               //running counter of timer overflows
extern int pulse;          //last pulse width in clock cycles

// Initialize the Ping))) Sensor
void ping_init(void);

// Handler to calculate distance
unsigned ping_read(void);

// ISR: Capture rising edge and falling edge time of PING sensor
void TIMER3B_Handler(void);

//Send the start pulse, then switch PB3 to the timer input
void send_pulse(void);

#endif /* PING_H_ */
