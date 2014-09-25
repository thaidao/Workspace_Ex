#ifndef _GH12_MODULES_
#define _GH12_MODULES_

/* Standard header files */
#include <RTL.h>                      /* RTL kernel functions & defines      */
#include <stdio.h>                    /* standard I/O .h-file                */
#include <ctype.h>                    /* character functions                 */
#include <string.h>                   /* string and memory functions         */

/* Include needed modules */
#include "rtc.h"					 // Realtime Clock Module
#include "serial.h"					 // Serial Communication Module
#include "logger.h"					 // Logger Module
#include "printer.h"				 // Printer module
#include "FWUpgrade.h"				 // Firmware upgrade Module
#include "Platform_Config.h"		 // Platform configuration
#include "debug.h"
#include "driverdb.h"				// Driver database
#include "drvKey.h"					// Driver key DB
//#include "buzzer.h"
#include "sdfifo.h"

#endif
