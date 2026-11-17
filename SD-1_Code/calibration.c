#include "calibration.h"

#include <math.h>

#include "adc.h"
#include "lcd.h"
#include "movement.h"
#include "ping.h"
#include "putty.h"
#include "sensor.h"
#include "servo.h"
#include "timer.h"

double ping_distances[10];
double ir_distances[10];

double avg_ping_scan();
double avg_ir_scan();
void calculate_power_regression(double *x, double *y, int num_values);

// ===========================================================================
// IR sensor calibration
// ===========================================================================

// Calibrates the IR sensor by comparing its readings with the PING sensor.
void calibrate_ir(oi_t *data)
{
    int i = 0;
    // Take readings at different distances
    for (i = 0; i < 7; i++)
    {
        ping_distances[i] = avg_ping_scan();
        ir_distances[i] = avg_ir_scan();

        move_backward(data, 70, 50);
    }

    calculate_power_regression(ir_distances, ping_distances, 7);
}

// Averages several PING readings (taken straight ahead at 90 degrees)
double avg_ping_scan()
{
    putty_printf("Ping: ");
    double sum = 0;
    int i = 0;

    servo_move(90);
    for (i = 0; i < 5; i++)
    {
        sum += sensor_ping_cm();
        timer_waitMillis(200);
    }

    putty_printf(" %5f\n", sum / 5.0);
    return sum / 5.0;
}

// Averages several IR readings
double avg_ir_scan()
{
    putty_printf("IR: ");
    int sum = 0;
    int reading;
    int i = 0;
    for (i = 0; i < 5; i++)
    {
        reading = adc_read();

        sum += reading;
        putty_printf("%4d,", reading);
    }

    putty_printf(" %5f\n", sum / 5.0);
    return sum / 5.0;
}

// Calculates a power regression and sends the formula over UART
void calculate_power_regression(double *x, double *y, int num_values)
{
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xy = 0.0;
    double sum_x2 = 0.0;

    double logx, logy;
    int i = 0;
    for (i = 0; i < num_values; i++)
    {
        logx = log(x[i]);
        logy = log(y[i]);

        sum_x += logx;
        sum_y += logy;
        sum_xy += logx * logy;
        sum_x2 += logx * logx;
    }

    // Calculate regression
    double exponent = (num_values * sum_xy - sum_x * sum_y)
            / (num_values * sum_x2 - sum_x * sum_x);
    double coeff = exp((sum_y - exponent * sum_x) / num_values);

    putty_printf("%.6f * x^{%.6f}\n", coeff, exponent);
}
