/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: Oct 2, 2014
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <stdlib.h>
#include <time.h>

#include <CUnit/Basic.h>

#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/log.h"

SET_LOGGER(TEST_LOGGER, "test.bxibase.err");

void test_bxierr() {
    srand((unsigned int)time(NULL));
    size_t nb = (size_t)(rand()% 8 + 2);
    bxierr_p leaf_err = bxierr_new((int) nb, "STATIC DATA", NULL, NULL, "%s", "LEAF");

    CU_ASSERT_PTR_NOT_NULL_FATAL(leaf_err);

    CU_ASSERT_EQUAL(leaf_err->code, (int) nb);
    CU_ASSERT_STRING_EQUAL(leaf_err->msg, "LEAF");
    CU_ASSERT_STRING_EQUAL(leaf_err->data, "STATIC DATA");
    CU_ASSERT_PTR_NULL(leaf_err->cause);

    for (size_t d = 0; d < nb + 2; d++) {
        char * str = bxierr_str_limit(leaf_err, d);
        OUT(TEST_LOGGER, "Depth=%zu %s", d, str);
        BXIFREE(str);
    }
    char * str = bxierr_str_limit(leaf_err, BXIERR_ALL_CAUSES);
    OUT(TEST_LOGGER, "Depth=ALL %s", str);
    BXIFREE(str);


    bxierr_p current = leaf_err;
    for (size_t i = nb - 1; i > 0; i--) {
        char * data = bxistr_new("data-%zu", i);
        current = bxierr_new((int) i, data, free, current, "err-%zu", i);
    }

    OUT(TEST_LOGGER, "Depth: %zu, nb: %zu", bxierr_get_depth(current), nb);
    CU_ASSERT_EQUAL(bxierr_get_depth(current), nb);

    for (size_t d = 0; d < nb + 2; d++) {
        char * str = bxierr_str_limit(current, d);
        OUT(TEST_LOGGER, "Depth=%zu %s", d, str);
        BXIFREE(str);
    }
    str = bxierr_str(current);
    OUT(TEST_LOGGER, "Depth=ALL %s", str);
    BXIFREE(str);

    bxierr_destroy(&current);

    errno = 5;
    current = bxierr_errno("Just a test, don't take this message into account");
    str = bxierr_str(current);
    OUT(TEST_LOGGER, "Test of perror: %s", str);
    BXIFREE(str);
    bxierr_destroy(&current);
}


