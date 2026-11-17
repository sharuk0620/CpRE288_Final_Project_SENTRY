/*
 * auto.c
 *
 *  Created on: Oct 20, 2026
 *      Author: nullpncl
 */

#include "auto.h"

#include <math.h>

#include "cyBot_Scan.h"
#include "lcd.h"
#include "movement.h"
#include "open_interface.h"
#include "putty.h"
#include "sensor.h"
#include "servo.h"

#define PI 3.14159

// find_smallest_objects() returns linear_width = 999.0 when nothing was found
#define NO_OBJECT_WIDTH 999.0

// PING reading (cm) at which the CyBot's front is ~5 cm from the object.
// TUNE THIS with a ruler.
#define STOP_DISTANCE_CM 11.0
#define MAX_STEP_CM 20.0  // drive at most this far between scans

// Bump recovery
#define BACKUP_MM 100             // back up 10 cm after a bump
#define SIDESTEP_MM 100           // sidestep 10 cm
#define SIDESTEP_TURN_DEG 90      // turn away from the bump, and back again
#define ROBOT_HALF_WIDTH_CM 17.0  // CyBot is ~34 cm wide
#define CLEAR_SIDE_MARGIN_CM 3.0  // extra room on each side
#define CLEAR_AHEAD_CM 25.0       // 10 cm sidestep + 15 cm margin. TUNE
#define PING_CONE_DEG 10.0        // PING is checked this far either side of ahead

enum { PLAN_NONE, PLAN_SEARCH, PLAN_MOVE };

static int plan_kind = PLAN_NONE;
static int plan_turn = 0;
static float plan_step = 0.0;
static int stuck = 0;

// Called when entering autonomous mode. Drops any stale plan and stuck state
// (you may have freed the CyBot in manual mode).
void auto_enter(void)
{
    plan_kind = PLAN_NONE;
    stuck = 0;
    servo_move(90);
    lcd_printf("AUTO mode");
    putty_printf("AUTO mode. h=scan/go, t=toggle, x=stop\n");
}

// Looks at the scan just taken (scan angle 90 = straight ahead) and returns 1
// if nothing is in a robot-wide corridor CLEAR_AHEAD_CM long. The IR gives the
// corridor check; the PING is also checked near straight ahead because the IR
// can misread objects that are very close.
static int path_clear(void)
{
    int i;
    float angle, rel, r, forward, lateral;

    for (i = 0; i <= 90; i++)
    {
        angle = i * 2.0;
        rel = (angle - 90.0) * (PI / 180.0);

        r = scan_sensor_data_array[i].IR_raw_val;
        if (r < 100)  // 100 = capped = nothing seen
        {
            forward = r * cos(rel);
            lateral = r * sin(rel);
            if (forward > 0 && forward < CLEAR_AHEAD_CM
                    && fabs(lateral)
                            < ROBOT_HALF_WIDTH_CM + CLEAR_SIDE_MARGIN_CM)
            {
                return 0;
            }
        }

        if (fabs(angle - 90.0) <= PING_CONE_DEG)
        {
            r = scan_sensor_data_array[i].sound_dist;
            if (r > 0 && r < CLEAR_AHEAD_CM)
            {
                return 0;
            }
        }
    }
    return 1;
}

// Back up, turn away from the bump, scan, and if there is room sidestep 10 cm
// and turn back. Returns 1 if it got around, 0 if the CyBot is stuck.
static int bump_recover(oi_t *oi, cyBOT_Scan_t *scan_data, int bumped_left)
{
    putty_printf(bumped_left ? "BUMP LEFT\n" : "BUMP RIGHT\n");
    lcd_printf(bumped_left ? "Bump left" : "Bump right");

    move_backward(oi, BACKUP_MM, 80);

    // Turn away from the side that hit
    if (bumped_left)
    {
        turn_clockwise(oi, SIDESTEP_TURN_DEG, 50);
    }
    else
    {
        turn_counterclockwise(oi, SIDESTEP_TURN_DEG, 50);
    }

    // Is there room to sidestep?
    scan_full_arc(scan_data, 0, 180);
    if (!path_clear())
    {
        putty_printf("SIDE BLOCKED\n");
        return 0;
    }

    move_forward(oi, SIDESTEP_MM, 100);
    if (oi->bumpLeft || oi->bumpRight)
    {
        putty_printf("SIDESTEP HIT SOMETHING\n");
        return 0;
    }

    // Face the original direction again
    if (bumped_left)
    {
        turn_counterclockwise(oi, SIDESTEP_TURN_DEG, 50);
    }
    else
    {
        turn_clockwise(oi, SIDESTEP_TURN_DEG, 50);
    }
    return 1;
}

// One press of 'h': execute the stored plan (if any), scan, and report the
// next plan without executing it. Returns 0 normally, 1 when within range of
// the smallest object, 2 when stuck (press 't' for manual mode).
int auto_step(oi_t *oi, cyBOT_Scan_t *scan_data)
{
    Object_t smallest;
    int bumped_left;

    if (stuck)
    {
        lcd_printf("STUCK\nPress t");
        putty_printf("STUCK - press t for manual mode\n");
        return 2;
    }

    // 1. Execute the plan made on the previous 'h' (none on the first)
    if (plan_kind == PLAN_SEARCH)
    {
        turn_counterclockwise(oi, 90, 50);
    }
    else if (plan_kind == PLAN_MOVE)
    {
        if (plan_turn > 2)
        {
            turn_counterclockwise(oi, plan_turn, 50);
        }
        else if (plan_turn < -2)
        {
            turn_clockwise(oi, -plan_turn, 50);
        }

        move_forward(oi, (int) (plan_step * 10.0), 100);

        if (oi->bumpLeft || oi->bumpRight)
        {
            bumped_left = oi->bumpLeft;  // read before the back-up changes it
            if (!bump_recover(oi, scan_data, bumped_left))
            {
                stuck = 1;
                plan_kind = PLAN_NONE;
                servo_move(90);
                lcd_printf("STUCK\nPress t");
                putty_printf("STUCK - press t for manual mode\n");
                return 2;
            }
        }
        else
        {
            handle_hazard(oi);  // tape: back off and turn away
        }
    }

    // 2. Scan (cleaned data + OBJECT lines go to PuTTY/GUI)
    scan_full_arc(scan_data, 0, 180);
    smallest = find_smallest_objects(scan_sensor_data_array, 0, 180, scan_data);

    // 3. Decide the next move and report it, but do NOT execute it yet
    if (smallest.linear_width >= NO_OBJECT_WIDTH)
    {
        plan_kind = PLAN_SEARCH;
        lcd_printf("No objects\nPLAN: turn 90");
        putty_printf("NO OBJECTS\n");
        putty_printf("PLAN search turn 90\n");
        return 0;
    }

    if (smallest.distance <= STOP_DISTANCE_CM)
    {
        plan_kind = PLAN_NONE;
        servo_move(smallest.center_angle);
        lcd_printf("Arrived\n%.1f cm", smallest.distance);
        putty_printf("ARRIVED %.1f\n", smallest.distance);
        return 1;
    }

    plan_kind = PLAN_MOVE;
    plan_turn = (int) (smallest.center_angle - 90.0);
    plan_step = smallest.distance - STOP_DISTANCE_CM;
    if (plan_step > MAX_STEP_CM)
    {
        plan_step = MAX_STEP_CM;
    }
    lcd_printf("Turn %d\nMove %.1f cm", plan_turn, plan_step);
    putty_printf("PLAN turn %d move %.1f\n", plan_turn, plan_step);
    return 0;
}
