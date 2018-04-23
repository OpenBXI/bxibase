/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: Oct 2, 2014
 # Contributors:
 ###############################################################################
 # Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <time.h>

#include <CUnit/Basic.h>

#include "bxi/base/time.h"

void test_time(void) {
    struct timespec time;
    bxierr_p err = bxitime_get(CLOCK_MONOTONIC, &time);
    CU_ASSERT_TRUE(bxierr_isok(err));
    bxierr_destroy(&err);
    err = bxitime_sleep(CLOCK_MONOTONIC, 0, 1000);
    CU_ASSERT_TRUE(bxierr_isok(err));
    bxierr_destroy(&err);
    double duration;
    err = bxitime_duration(CLOCK_MONOTONIC, time, &duration);
    CU_ASSERT_TRUE(bxierr_isok(err));
    bxierr_destroy(&err);
    CU_ASSERT_TRUE(duration > 1e-6);
    char * time_char = bxitime_duration_str(1000.0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(time_char);
    free(time_char);
}
