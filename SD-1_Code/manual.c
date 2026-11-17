/*
 * manual.c
 *
 *  Created on: Oct 20, 2026
 *      Author: nullpncl
 */

#include "manual.h"

#include "cyBot_Scan.h"
#include "lcd.h"
#include "movement.h"
#include "open_interface.h"
#include "putty.h"
#include "sensor.h"
#include "servo.h"

// Called when entering manual mode
void manual_enter(void)
{
    servo_move(90);
    lcd_printf("MANUAL mode");
    putty_printf("MANUAL. w/a/s/d, m=scan, t, x\n");
}

// Handles one key in manual mode
void manual_command(oi_t *oi, cyBOT_Scan_t *scan_data, char cmd)
{
    switch (cmd)
    {
    case 'w':
        move_forward(oi, 50, 100);
        handle_hazard(oi);  // prints BUMP/TAPE so you know a short object is there
        break;
    case 's':
        move_backward(oi, 50, 80);
        break;
    case 'a':
        turn_counterclockwise(oi, 15, 50);
        break;
    case 'd':
        turn_clockwise(oi, 15, 50);
        break;
    case 'm':
        scan_full_arc(scan_data, 0, 180);
        find_smallest_objects(scan_sensor_data_array, 0, 180, scan_data);
        servo_move(90);
        putty_printf("SCAN DONE\n");
        break;
    case 'q':
        turn_counterclockwise(oi, 90, 50);
        break;
    case 'e':
        turn_clockwise(oi, 90, 50);
        break;
    case 'x':
        turn_clockwise(oi, 90, 50);
        turn_clockwise(oi, 90, 50);
        break;

    default:
        break;
    }
}
