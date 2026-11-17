/*
 * auto.h
 *
 *  Created on: Oct 20, 2026
 *      Author: nullpncl
 */

#ifndef AUTO_H_
#define AUTO_H_

#include "cyBot_Scan.h"
#include "open_interface.h"

// Autonomous navigation mode for CyBot
// Scans for objects, finds gaps, and navigates autonomously
void auto_enter(void);
int auto_step(oi_t *oi, cyBOT_Scan_t *scan_data);
int run_auto(oi_t *data, cyBOT_Scan_t *scan_data);

#endif /* AUTO_H_ */
