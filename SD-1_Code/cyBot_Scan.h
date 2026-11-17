#ifndef CYBOT_SCAN_H_
#define CYBOT_SCAN_H_

// Author: Phillip Jones
// Data: Updated 10/27/2021
// Version: 10 27 2021  (you can use cyBOT_scan_version() to confirm version)

// Scan value
// NOTE: libcybotScan.lib was compiled with "int IR_raw_val". It is declared as
// float here because this project stores the calibrated IR distance (cm) in
// this field. Both are 4 bytes, so the struct layout still matches the library.
// The library's IR is NOT enabled (see cyBOT_init_Scan in main), so the library
// never fills this field with anything useful; sensor.c fills it from adc.c.
typedef struct
{
    float sound_dist; // Distance from Ping Sensor (cyBOT_Scan returns -1.0 if PING is not enabled)
    float IR_raw_val; // Raw ADC value / IR distance from our own ADC code
} cyBOT_Scan_t;

#endif
