/*
 * putty.c
 *
 *  Created on: Oct 20, 2026
 *      Author: nullpncl
 */

#include "putty.h"

#include <stdio.h>

#include "movement.h"
#include "uart.h"

#define TOTAL_CHARS 50

// Send a Object_t over UART
void putty_send_object(Object_t *object)
{
    putty_printf("OBJECT %.4f %.4f %.4f\n", object->center_angle, object->distance,
                 object->linear_width);
}

// Send a scan interval over UART
void putty_send_scan_data(int angle, cyBOT_Scan_t *scan_data)
{
    putty_printf("%3d  %7.3f %7.2f\n", angle, scan_data->sound_dist,
                 scan_data->IR_raw_val);
}

// Send the bot's current position over UART
void putty_send_pos()
{
    putty_printf("POS %d %d %d\n", (int) pos_x, (int) pos_y, (int) pos_angle);
}

// Send the string over UART
void putty_puts(char string[], int length)
{
    string[length] = '\0';
    uart_sendStr(string);
}

// Print but over UART
void putty_printf(const char *format, ...)
{
    char buffer[TOTAL_CHARS + 1];
    va_list arglist;
    va_start(arglist, format);
    vsnprintf(buffer, TOTAL_CHARS + 1, format, arglist);
    buffer[TOTAL_CHARS] = '\0';

    char *str = buffer;
    while (*str)
    {
        if (*str == '\n')
        {
            uart_sendChar('\r');
            uart_sendChar(*str);
        }
        else if (*str == 9 || (*str >= 32 && *str <= 126))
        {
            uart_sendChar(*str);
        }

        str++;
    }

    va_end(arglist);
}
