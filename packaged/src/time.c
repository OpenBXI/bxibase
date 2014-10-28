/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/time.h"

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************


bxierr_p bxitime_sleep(clockid_t clk_id, const time_t tv_sec, const long tv_nsec) {

    struct timespec rem;
    struct timespec delay = {.tv_sec = tv_sec, .tv_nsec = tv_nsec};
    errno = 0;

    while(true) {
        int rc = clock_nanosleep(clk_id, 0, &delay, &rem);
        if (0 == rc) break;
        if (-1 == rc && errno != EINTR) {
            return bxierr_error("Calling nanosleep(%lu, %lu) failed",
                                delay.tv_sec, delay.tv_nsec);
        }
        // Else we have been interrupted, start again
        delay = rem;
    }
    return BXIERR_OK;
}

bxierr_p bxitime_get(clockid_t clk_id, struct timespec * const time) {
    errno = 0;
    if (clock_gettime(clk_id, time) != 0) {
        return bxierr_error("Can't get the time with clock_gettime() and clk_id %d.",
                            clk_id);
    }
    return BXIERR_OK;
}


bxierr_p bxitime_duration(clockid_t clk_id, const struct timespec timestamp, double* result) {
    struct timespec now;
    bxierr_p err = bxitime_get(clk_id, &now);
    double sec =   (double) now.tv_sec  - (double) timestamp.tv_sec;
    double nsec =  (double) now.tv_nsec - (double) timestamp.tv_nsec;
    *result = (sec + nsec * 1e-9);

    return err;
}

bxierr_p bxitime_str(struct timespec * time, char ** result) {
    bxierr_p bxierr;
    struct timespec now;
    if (time == BXITIME_NOW) {
        bxierr = bxitime_get(CLOCK_REALTIME, &now);
        if (BXIERR_OK != bxierr) return bxierr;
        time = &now;
    }
    time_t t = time->tv_sec;
    errno = 0;
    struct tm dummy, * tm_p;
    tm_p = localtime_r(&t, &dummy);
    if (tm_p == NULL) return bxierr_error("Call to localtime_r() failed.");

    *result = bximem_calloc(64);
    size_t n = strftime(*result, 32, "%FT%T", tm_p);
    if (n == 0) return bxierr_gen("Function strftime() returned 0, this is a failure");

    // Add the nanoseconds at the end of the string
    int err = snprintf(*result+n, 32, ".%ld", time->tv_nsec);
    if (err < 0) return bxierr_gen("Function snprintf() returned < 0, this is a failure");

    return BXIERR_OK;
}

char * bxitime_duration_str(const double duration){
    long iduration = (long) duration;
    long double rest = (long double) duration - (long double) iduration;
    long seconds = iduration % 60;
    iduration /= 60;
    long minutes = iduration % 60;
    iduration /=60;
    long hours = iduration % 24;
    iduration /=24;
    long days = iduration;
    if(days > 99) {
        return bxistr_new("More than %ld days", days);
    } else {
        return bxistr_new("P%02ldDT%02ldH%02ldM%02ld.%06.0LfS",
                          days, hours, minutes, seconds, rest * 1e6);
    }
}


// *********************************************************************************
// ********************************** Static Functions  ****************************
// *********************************************************************************

