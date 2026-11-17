/*
 * adc.c
 *
 *  Created on: Oct 21, 2026
 *      Author: nullpncl
 */

#include "adc.h"

#include <inc/tm4c123gh6pm.h>
#include <stdbool.h>
#include <stdint.h>
#include "putty.h"

volatile short adc_data = 0x000;



// Initialize ADC0 SS1 on Port B, software trigger, 16x hardware averaging (polled)
void adc_init(void)
{
    SYSCTL_RCGCGPIO_R |= 0x2;  // enables port b    Ch.5 Register 60
    SYSCTL_RCGCADC_R |= 0x1;   // enables ADC0      Ch.5 Register 68

    while ((SYSCTL_PRGPIO_R & 0x2) == 0)    // wait until port B clock is ready
    {
    }
    while ((SYSCTL_PRADC_R & 0x1) == 0)     // wait until ADC0 clock is ready
    {
    }

    GPIO_PORTB_AFSEL_R |= 0x10;     // sets PB4 to peripheral                                       Ch.10 Register 10
    GPIO_PORTB_DIR_R &= ~0x10;      // sets PB4 as input, the rest as output                        Ch.10 Register 2
    GPIO_PORTB_DEN_R &= ~0x10;      // disabled PB4 digital function, enables the rest              Ch.10 Register 18
    GPIO_PORTB_AMSEL_R |= 0x10;     // enables the analog function, disables isolation for PB4      Ch.10 Register 21
    GPIO_PORTB_ADCCTL_R &= 0x00;    // disables all pins from triggering ADC                        Ch.10 Register 23

    ADC0_ACTSS_R &= ~0x02;      // disabled SS1 for ADC0 and enables the rest to configure the setting              Ch.13 Register 1
    ADC0_EMUX_R &= ~0x00F0;     // selects processor trigger and clears it for SS1 while setting others to always   Ch.13 Register 6
    ADC0_SSMUX1_R &= 0xFFFFFFF0;// Preserved all bits except 1st sample input, clears MUX0                          Ch.13 Register 27
    ADC0_SSMUX1_R |= 0x0A;      // selects AIN10 for the first sample input                                         Ch.13 Register 27
    ADC0_SSCTL1_R &= 0xFFF0;    // clears first sample controls, preserves the rest                                 Ch.13 Register 29
    ADC0_SSCTL1_R |= 0x06;      // IE0 (bit 2) sets the raw interrupt status (RIS) flag when sample 0 finishes,
                                // END0 (bit 1) makes the first sample the end of the sequence                      Ch.13 Register 29

    ADC0_SAC_R &= ~0x7;     //Preserves all bits except bits [2:0], no hardware oversampling        Ch.13 Register 12
    ADC0_SAC_R |= 0x4;      // 16x hardware oversampling                                            Ch.13 Register 12
    ADC0_ACTSS_R |= 0x02;   // reenables SS1 after configurations                                   Ch.13 Register 1

    return;
}

// Take one conversion of the IR sensor and return the 12-bit result
int adc_read(void)
{
    ADC0_PSSI_R |= 0x2; //Begin sampling on SS1     Ch.13 Register 11

    while((ADC0_RIS_R & 0x02) == 0)     //Wait for SS1 conversion to complete                       Ch.13 Register 2
    {
        //wait for conversion to complete
    }

    int result = ADC0_SSFIFO1_R & 0xFFF;    //Read conversion results (12 bits) from SS1 FIFO       Ch.13 Register 18

    ADC0_ISC_R = 0x02;                  //Clear SS1 completion flag                                 Ch,3 Register 4

    adc_data = result;                  //Keep the latest result available globally

    return result;
}
