/*
 * manual.h
 *
 *  Created on: Oct 20, 2026
 *      Author: nullpncl
 */

#ifndef MANUAL_H_
#define MANUAL_H_

#include "cyBot_Scan.h"
#include "open_interface.h"

// Manual navigation mode for CyBot
void manual_enter(void);
void manual_command(oi_t *oi, cyBOT_Scan_t *scan_data, char cmd);
int run_manual(oi_t *data, cyBOT_Scan_t *scan_data);

#endif /* MANUAL_H_ */
