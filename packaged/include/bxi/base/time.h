/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#ifndef BXITIME_H_
#define BXITIME_H_

#ifndef BXICFFI
#include <time.h>
#endif

#include "bxi/base/err.h"


/**
 * @file    time.h
 * @brief   Time Handling Module
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

#define BXITIME_NOW NULL


// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************


/**
 * Get the time according to the given clock_id.
 *
 * See clock_gettime(2) for clk_id options.
 *
 * If unsure, use the following rules:
 *
 * - CLOCK_REALTIME: for highly accurate timestamp with actual
 *   wall clock time
 * - CLOCK_MONOTIC: for highly accurate duration computation
 *
 * - CLOCK_REALTIME_COARSE: for less accurate timestamp
 * - CLOCK_MONOTONIC_COARSE: for less accurate duration computation
 *
 * _COARSE variants are more performant but less accurate.
 * See misc/bench/timeclock for a bench. On my hardware:
 * the output is the following:
 *
 *      $ ./benchclock 1000000
 *      Resolution:
 *      CLOCK_REALTIME: 1ns
 *      CLOCK_REALTIME_COARSE: 1000000ns
 *      CLOCK_MONOTONIC: 1ns
 *      CLOCK_MONOTONIC_COARSE: 1000000ns
 *      CLOCK_MONOTONIC_RAW: 1ns
 *      Calling cost:
 *      33210695 ns   3.01108e+07 calls/s     33.2107 ns/call  1000000 CLOCK_REALTIME
 *      9195934 ns    1.08744e+08 calls/s     9.19593 ns/call  1000000 CLOCK_REALTIME_COARSE
 *      26141696 ns   3.82531e+07 calls/s     26.1417 ns/call  1000000 CLOCK_MONOTONIC
 *      9136039 ns    1.09457e+08 calls/s     9.13604 ns/call  1000000 CLOCK_MONOTONIC_COARSE
 *      72388694 ns   1.38143e+07 calls/s     72.3887 ns/call  1000000 CLOCK_MONOTONIC_RAW
 *
 *
 * @param[in] clk_id a clock (see clock_gettime(2) for details)
 * @param[out] time the timespec data structure to fill with the result
 *
 * @return BXIERR_OK on success
 */
bxierr_p bxitime_get(clockid_t clk_id, struct timespec * const time);

/**
 * Sleep for the given amount of time according to the given clock.
 *
 * Note, if the process is interrupted while sleeping, it goes back to sleep
 * until the given amount of remaining time has passed.
 *
 * @param[in] clk_id the clock (see clock_gettime(2) for details)
 * @param[in] tv_sec the number of seconds to sleep
 * @param[in] tv_nsec the number of nanoseconds to sleep
 *
 * @return BXIERR_OK on success.
 */
bxierr_p bxitime_sleep(clockid_t clk_id, const time_t tv_sec, const long tv_nsec);

/**
 * Return a duration in seconds from the given timestamp (filled by bxitime_get()).
 * (see clock_gettime(2) for clk_id options, use BXITIME_DEFAULT_CLOCK if unsure).
 *
 * @param[in] clk_id a clock (see clock_gettime(2) for details)
 * @param[in] timestamp the timestamp to count the duration from
 * @param[out] duration a pointer on the result
 *
 * @return BXIERR_OK on success
 */
bxierr_p bxitime_duration(clockid_t clk_id,
                          const struct timespec timestamp,
                          double * duration);

/**
 * Return a string representing the given timespec (filled by bxitime_get()).
 *
 * Use BXITIME_NOW, to get the current time.
 *
 * Note: the returned string has been allocated on the heap.
 * It should be freed using BXIFREE().
 *
 * @param[in] time the time to get the string representation of
 * @param[out] result a pointer on a string for storing the result
 *
 * @return BXIERR_OK on success
 */
bxierr_p bxitime_str(struct timespec * time, char ** result);


/**
 * Return an ISO8601 string for representing the given duration.
 *
 * @param[in] duration the duration to get the human string representation of
 *
 * @return an ISO8601 string for representing the given duration.
 */
char * bxitime_duration_str(double duration);

#endif /* BXIMISC_H_ */
