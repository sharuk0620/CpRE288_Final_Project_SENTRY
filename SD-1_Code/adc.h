/*
 * adc.h
 *
 *  Created on: Oct 21, 2026
 *      Author: nullpncl
 */

#ifndef ADC_H_
#define ADC_H_

// Most recent 12-bit conversion result (also returned by adc_read)
extern volatile short adc_data;

// Initialize ADC0 SS1 on PB4 (AIN10), software trigger, 16x hardware averaging
void adc_init(void);

// Take one ADC conversion of the IR sensor (blocks until done) and return the
// 12-bit result (0 - 4095)
int adc_read(void);

#endif /* ADC_H_ */
