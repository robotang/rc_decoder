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

#include "std.h"
#include "rc.h"

#include "generated/radio.h"

#include "led.h"

#define FP_DEV_NAME     "/dev/rc"
#define FP_LINE_WIDTH   128

SystemStatus_t rc_system_status = STATUS_UNINITIAIZED;
FILE* fp_dev;

void rc_init ( void )
{
    fp_dev = fopen ("FP_DEV_NAME", "r");
    if (fp_dev)
    {
        led_log ("Opened FP_DEV_NAME\n");
        rc_system_status = STATUS_INITIALIZED; // Should we be doing this?
    }
    else
    {
        led_log ("Failed to open FP_DEV_NAME\n");
        rc_system_status = STATUS_FAIL;
    }
}

void rc_periodic_task ( void )
{
    char line [FP_LINE_WIDTH]; 
    int channel = 0;
    if (fgets (line, sizeof (line), fp_dev)) 
    {
        char *token;

        /* Is there a better way to do this? */
        token = strtok (line, " ");
        if (strcmp (token, "RC_OK"))
            rc_status = RC_OK;
        else if (strcmp (token, "RC_LOST"))
            rc_status = RC_LOST;
        else
            rc_status = RC_REALLY_LOST;

        while ((token = strtok (NULL, " ")) != NULL)
        {
            ppm_pulses[channel] = atoi(token);
            NormalizePpm();
            channel ++;
        }
    }
}

bool_t rc_event_task ( void )
{
    /* See docs, return true if valid */
    return (rc_status == RC_OK);
}
