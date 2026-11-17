/*
 * sensor.c
 *
 *  Created on: Oct 20, 2026
 *      Author: nullpncl
 */

#include "sensor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "adc.h"
#include "calibration.h"
#include "lcd.h"
#include "ping.h"
#include "putty.h"
#include "servo.h"
#include "timer.h"

#define PI 3.14159
// Smoothing factor for ping sensor
#define SCAN_ALPHA_SOUND 0.1
// Smoothing factor for IR sensor
#define SCAN_ALPHA_IR 0.25

#define SCAN_STEP_DEG 2        // degrees between scan samples
#define MIN_OBJECT_WIDTH_CM 3  // anything narrower is treated as noise
#define DIST_DISAGREE_CM 5.0   // if PING and IR differ by more than this, trust IR

int scan_index = 0;
float center_distance;  // CM

// PING pulse width (16 MHz ticks) to cm: speed of sound 34300 cm/s, round trip / 2
float sensor_ping_cm(void)
{
    return ping_read() * (34300.0f / 2.0f / 16000000.0f);
}

// Performs a servo scan across specified angle range, calls callback on each
// angle scanned
void scan_range(int lower_angle, int upper_angle, int step,
                cyBOT_Scan_t *scan_data,
                void (*callback_ptr)(int angle, cyBOT_Scan_t *scan_data),
                int num_scans)
{
    scan_index = 0;
    int i, j;
    cyBOT_Scan_t avg_scan;  // 8 bytes on the stack instead of calloc/free

    for (i = lower_angle; i <= upper_angle; i += step)
    {
        avg_scan.IR_raw_val = 0;
        avg_scan.sound_dist = 0;

        servo_move(i);  // waits until the servo has arrived
        for (j = 0; j < num_scans; j++)
        {
            avg_scan.IR_raw_val += adc_read();
            avg_scan.sound_dist += sensor_ping_cm();
        }

        avg_scan.IR_raw_val /= num_scans;
        avg_scan.sound_dist /= num_scans;
        callback_ptr(i, &avg_scan);
    }
}

// Process noisy data using exponential smoothing
void process_noisy_data(int angle, cyBOT_Scan_t *scan_data)
{
    // Voltage to cm
    double vToCm = VTOCM_COEFF * pow(scan_data->IR_raw_val, VTOCM_EXP);

    // First scan value
    if (scan_index == 0)
    {
        scan_sensor_data_array[scan_index].sound_dist = scan_data->sound_dist;
        scan_sensor_data_array[scan_index].IR_raw_val =
                vToCm < 100 ? vToCm : 100;
        goto process_noisy_data_exit;
    }

    cyBOT_Scan_t obj;

    // Smooth ping
    if (scan_data->sound_dist
            > 0.85 * scan_sensor_data_array[scan_index - 1].sound_dist
            && scan_data->sound_dist
                    < 1.15 * scan_sensor_data_array[scan_index - 1].sound_dist)
    {
        obj.sound_dist = SCAN_ALPHA_SOUND * scan_data->sound_dist
                + (1 - SCAN_ALPHA_SOUND)
                        * scan_sensor_data_array[scan_index - 1].sound_dist;
    }
    else
    {
        obj.sound_dist = scan_data->sound_dist;
    }

    // Smooth IR
    if (vToCm > 0.7 * scan_sensor_data_array[scan_index - 1].IR_raw_val
            && vToCm < 1.30 * scan_sensor_data_array[scan_index - 1].IR_raw_val)
    {
        obj.IR_raw_val = SCAN_ALPHA_IR * vToCm + (1 - SCAN_ALPHA_IR) * scan_sensor_data_array[scan_index - 1].IR_raw_val;
    }
    else
    {
        // Check for spike
        if (scan_index > 1
                && (vToCm
                        > 0.9
                                * scan_sensor_data_array[scan_index - 2].IR_raw_val
                        && vToCm
                                < 1.10
                                        * scan_sensor_data_array[scan_index - 2].IR_raw_val))
        {
            obj.IR_raw_val =
            SCAN_ALPHA_IR * vToCm
                    + (1 - SCAN_ALPHA_IR)
                            * scan_sensor_data_array[scan_index - 2].IR_raw_val;

            // Replace old spike
            scan_sensor_data_array[scan_index - 1].IR_raw_val = obj.IR_raw_val < 100 ? obj.IR_raw_val : 100;
            putty_printf("%3d\t%10f\t%10.2f\n", angle - 2, scan_sensor_data_array[scan_index - 1].sound_dist, scan_sensor_data_array[scan_index - 1].IR_raw_val);
            timer_waitMillis(100);

        }
        else
        {
            obj.IR_raw_val = vToCm;
        }
    }

    // Max distance of 100 cm because smoothing goes off to infinity
    if (obj.IR_raw_val > 100)
    {
        obj.IR_raw_val = 100;
    }

    scan_sensor_data_array[scan_index] = obj;

    process_noisy_data_exit:
    // Send scan data to GUI
    putty_send_scan_data(angle, &scan_sensor_data_array[scan_index]);
    lcd_clear();
    lcd_printf("%3d %5f %5.1f\n", angle,
               scan_sensor_data_array[scan_index].sound_dist,
               scan_sensor_data_array[scan_index].IR_raw_val);
    scan_index++;
}

// Callback to get center distance of object
void get_center_distance(int angle, cyBOT_Scan_t *scan_data)
{
    lcd_printf("%3d %5f %5.1f\n", angle, scan_data->sound_dist,
               scan_data->IR_raw_val);

    center_distance = scan_data->sound_dist;
}

// Performs a scan between two angles with 2-degree increments
void scan_full_arc(cyBOT_Scan_t *scan_data, int start_angle, int stop_angle)
{
    // Move to start angle (servo_move waits until it has arrived)
    servo_move(start_angle);

    // Tell GUI we're doing a scan
    putty_printf("STARTSCAN\n");
    timer_waitMillis(100);
    scan_range(start_angle, stop_angle, 2, scan_data, process_noisy_data, 3);
    timer_waitMillis(200);
    putty_printf("ENDSCAN\n");
}

// Creates an object struct
Object_t create_object(float dist, int start_angle, int end_angle)
{
    struct detect_object object;

    object.center_angle = (start_angle + end_angle) / 2.0;
    object.distance = dist;
    object.linear_width = object.distance * ((end_angle - start_angle) * (PI / 180.0));

    return object;
}

// Updates the linear width of an object
void object_update_linear_width(Object_t *obj, int start_angle, int end_angle)
{
    obj->linear_width = obj->distance * ((end_angle - start_angle) * (PI / 180.0));
}

// Re-measures one object with the PING at its mid-angle. Returns 1 if the
// object is real and was stored in *out, 0 if it was rejected.
static int measure_object(cyBOT_Scan_t data_array[], int start_angle,
                          int start_idx, int end_idx, cyBOT_Scan_t *scan_data,
                          Object_t *out)
{
    int start_deg = start_angle + start_idx * SCAN_STEP_DEG;
    int end_deg = start_angle + end_idx * SCAN_STEP_DEG;
    int mid_idx = (start_idx + end_idx) / 2;
    float ir_dist = data_array[mid_idx].IR_raw_val; // calibrated cm, capped at 100
    float ping_dist;
    int ping_ok, ir_ok;

    Object_t obj = create_object(0, start_deg, end_deg);
    obj.start = start_deg;
    obj.end = end_deg;
    obj.center_angle = (start_deg + end_deg) / 2.0;

    // scan_range moves the servo to the mid-angle and waits for it
    scan_range((int) obj.center_angle, (int) obj.center_angle, 1, scan_data, get_center_distance, 5);
    ping_dist = center_distance;

    ping_ok = (ping_dist > 0 && ping_dist < 150);
    ir_ok = (ir_dist < 100);

    if (ping_ok && ir_ok)
    {
        // PING cone is wide and may be seeing a neighbor; IR is the tiebreaker
        obj.distance =
                (fabs(ping_dist - ir_dist) > DIST_DISAGREE_CM) ?
                        ir_dist : ping_dist;
    }
    else if (ping_ok)
    {
        obj.distance = ping_dist;
    }
    else if (ir_ok)
    {
        obj.distance = ir_dist;   // PING missed (thin object): fall back to IR
    }
    else
    {
        return 0;                 // neither sensor sees anything real
    }

    // Edges lie up to one step outside the first/last sample inside the object,
    // so widen by half a step on each side
    object_update_linear_width(&obj, start_deg - SCAN_STEP_DEG / 2, end_deg + SCAN_STEP_DEG / 2);

    if (obj.linear_width < MIN_OBJECT_WIDTH_CM)
    {
        return 0;
    }

    *out = obj;
    return 1;
}

// Finds all objects in a finished scan, sends each to the PC, and returns the
// number found. An object that is already in view at the first angle, or still
// in view at the last, is closed at the scan edge (its width is underestimated).
int find_all_objects(cyBOT_Scan_t data_array[], int start_angle, int end_angle,cyBOT_Scan_t *scan_data, Object_t objects[], int max_objects)
{
    int last = (end_angle - start_angle) / SCAN_STEP_DEG;
    int in_object = 0;
    int start_idx = 0;
    int count = 0;
    int i;
    float ir, prev;

    for (i = 0; i <= last && count < max_objects; i++)
    {
        ir = data_array[i].IR_raw_val;

        prev = (i > 0) ? data_array[i - 1].IR_raw_val : 100;

        if (!in_object && ir < 150 && ir < 0.85 * prev)
        {
            in_object = 1;
            start_idx = i;
        }
        else if (in_object && ir > 1.15 * prev)
        {
            in_object = 0;
            if (measure_object(data_array, start_angle, start_idx, i - 1, scan_data, &objects[count]))
            {
                putty_send_object(&objects[count]);
                count++;
            }
        }
    }

    // Object still in view at the end of the scan
    if (in_object && count < max_objects)
    {
        if (measure_object(data_array, start_angle, start_idx, last, scan_data, &objects[count]))
        {

            putty_send_object(&objects[count]);
            count++;
        }
    }

    return count;
}

// Finds the smallest-width object in a finished scan. Returns
// linear_width = 999.0 when nothing was found.
Object_t find_smallest_objects(cyBOT_Scan_t data_array[], int start_angle, int end_angle, cyBOT_Scan_t *scan_data)
{
    Object_t objs[10];
    Object_t smallest;
    int n, i;

    smallest.center_angle = 0.0;
    smallest.distance = 0.0;
    smallest.linear_width = 999.0;
    smallest.start = 0;
    smallest.end = 0;

    n = find_all_objects(data_array, start_angle, end_angle, scan_data, objs,
                         10);
    for (i = 0; i < n; i++)
    {
        if (objs[i].linear_width < smallest.linear_width)
        {
            smallest = objs[i];
        }
    }
    return smallest;
}
