#include <stdbool.h>
#include <stdint.h>
#include <inc/tm4c123gh6pm.h>
#include "driverlib/interrupt.h"

#include "Timer.h"
#include "lcd.h"
#include "servo.h"

// Pick which part to run: 1, 2, or 3
#define PART 3

#if PART == 1
// ---------------------------------------------------------------
// Part 1: move the servo to 0 degrees
// ---------------------------------------------------------------
int main(void)
{
    timer_init();
    lcd_init();
    servo_init();

    servo_move(0);
    lcd_printf("0 degrees");

    while (1)
    {
    }
}

#elif PART == 2
// ---------------------------------------------------------------
// Part 2: pushbutton control
// ---------------------------------------------------------------
#include "button.h"

int main(void)
{
    int angle = 90;
    int direction = 1;      // 1 = toward higher degrees, -1 = toward lower
    int match;
    int button;

    timer_init();
    lcd_init();
    button_init();
    servo_init();
    match = servo_move(angle);

    while (1)
    {
        button = button_getButton();

        if (button == 1)
        {
            angle = angle + direction * 1;
        }
        if (button == 2)
        {
            angle = angle + direction * 5;
        }
        if (button == 3)
        {
            direction = -direction;
        }
        if (button == 4)
        {
            if (direction == 1)
            {
                angle = 180;
            }
            else
            {
                angle = 0;
            }
        }

        if (angle < 0)
        {
            angle = 0;
        }
        if (angle > 180)
        {
            angle = 180;
        }

        if (button == 1 || button == 2 || button == 4)
        {
            match = servo_move(angle);
        }

        if (direction == 1)
        {
            lcd_printf("Counter Clockwise\nAngle: %d\nCount: %d", angle, match);
        }
        else
        {
            lcd_printf("Clockwise\nAngle: %d\nCount: %d", angle, match);
        }

        timer_waitMillis(10);
    }
}

#elif PART == 3
// ---------------------------------------------------------------
// Part 3: mission using only your own UART, ADC, PING, and servo
// ---------------------------------------------------------------
#include "adc.h"
#include "auto.h"
#include "cyBot_Scan.h"
#include "manual.h"
#include "open_interface.h"
#include "ping.h"
#include "putty.h"
#include "sensor.h"
#include "uart.h"

int main(void)
{
    oi_t *oi;
    cyBOT_Scan_t scan_data;
    int auto_mode = 0;
    char cmd;

    timer_init();
    lcd_init();
    servo_init();
    adc_init();
    ping_init();
    uart_init(115200);
    uart_interrupt_init();

    oi = oi_alloc();
    oi_init(oi);

    manual_enter();

    while (1)
    {
        if (uart_flag)
        {
            cmd = uart_data;
            uart_flag = 0;

            if (cmd == 't')            // toggle manual / autonomous
            {
                auto_mode = !auto_mode;
                if (auto_mode)
                {
                    auto_enter();
                }
                else
                {
                    manual_enter();
                }
            }
            else if (cmd == 'x')       // stop
            {
                oi_setWheels(0, 0);
                putty_printf("STOP\n");
            }
            else if (auto_mode)
            {
                if (cmd == 'h') { auto_step(oi, &scan_data); }
            }
            else
            {
                manual_command(oi, &scan_data, cmd);
            }
        }
    }
}

#endif
