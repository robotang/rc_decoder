/*
    ENEL675 - Advanced Embedded Systems
    File: 		gtx_rc.c
    Authors: 	        Robert Tang, John Howe
    Date:  		11 September 2010

    Remote Control API.

    Manages the acquisition of RC signals. Channel assignment is handled
    by the radio XML. Enums

 */


/*  http://www.waspuav.org/doc/sw/doc/onboard-api.html#onboardrc_8h
{{{

file rc.h


        RCStatus_t

            Connectivity of RC.

            The RC_LOST and RC_REALLY_LOST state seperation is designed
            to act as hysteresis for intermittant signal loss. Backends
            may switch between RC_OK and RC_REALLY_LOST if they wish.

    Functions

        void rc_init(void )

            To be called at startup.


            Backend dependant
        void rc_periodic_task(void )

            To be called at periodic frequency.


            Backend dependant
        bool_t rc_event_task(void )

            Return TRUE if there is a valid RC signal.

            
    Variables

        SystemStatus_t rc_system_status

        pprz_t rc_values

            Array of values of each of the RC channels. Scaled from
            -9600 - +9600. The index in the array is determined from the
            radio.xml uint8_t rc_values_contains_avg_channels

            FIXME.

        RCStatus_t rc_status

            Connectivity of RC.

        uint16_t ppm_pulses

            Raw unscaled RC values.

            Backend dependent
}}} */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "std.h"
#include "rc.h"

#include "generated/radio.h"

#include "led.h"

#define FP_DEV_NAME     "/dev/rc"
#define FP_LINE_WIDTH   128

SystemStatus_t rc_system_status = STATUS_UNINITIAIZED;

int fp_dev; 
int ThisNormalizePpm(int val);

void rc_init ( void )
{
    fp_dev = open(FP_DEV_NAME, O_RDONLY);
    if(fp_dev != -1)
    {
        led_log ("Opened %s\n", FP_DEV_NAME);
        rc_system_status = STATUS_INITIALIZED; // Should we be doing this?
    }
    else
    {
        led_log ("Failed to open %s\n", FP_DEV_NAME);
        rc_system_status = STATUS_FAIL;
    }
}

void rc_periodic_task ( void )
{ 
    int channel = 0, len;
    char line [FP_LINE_WIDTH];
    
    lseek(fp_dev, 0, SEEK_SET);
    len = read(fp_dev, line, FP_LINE_WIDTH);
    if (len > 0)
    {
        char *token;
        token = strtok (line, ",");
        if (strcmp (token, "RC_OK"))
            rc_status = RC_OK;
        else if (strcmp (token, "RC_LOST"))
            rc_status = RC_LOST;
        else
            rc_status = RC_REALLY_LOST;

        while ((token = strtok (NULL, ",")) != NULL)
        {
            ppm_pulses[channel] = atoi(token);
            rc_values[channel] = ThisNormalizePpm(ppm_pulses[channel]);
            channel++;
        }
    }
}

bool_t rc_event_task ( void )
{
    /* See docs, return true if valid */
    return (rc_status == RC_OK);
}

#define MIN_PULSE_LIMIT    50
#define MAX_PULSE_LIMIT    250
#define NEUTRAL_PULSE      150
int ThisNormalizePpm(int val)
{
    int ret = val - NEUTRAL_PULSE;
    if(ret > 0)
    {
        ret *= (9600 / MAX_PULSE_LIMIT);
    }
    else
    {
        ret *= (9600 / MIN_PULSE_LIMIT);
    }
    return ret;
}

