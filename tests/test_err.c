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

SET_LOGGER(TEST_LOGGER, "test.bxierr");
bxierr_define(BXIERR_TEST_CHAIN, 10, "Error to test chain with static error");

void test_bxierr() {
    srand((unsigned int)time(NULL));
    size_t nb = (size_t)(rand()% 8 + 2);
    bxierr_p leaf_err = bxierr_new((int) nb, "STATIC DATA", NULL, NULL, NULL, "%s", "LEAF");

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
        current = bxierr_new((int) i, data, free, NULL,  current, "err-%zu", i);
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

    current = bxierr_new(420, "Test Static Chain", NULL, NULL, BXIERR_TEST_CHAIN,
                         "Just a test, don't take this message into account");
    str = bxierr_str(current);
    CU_ASSERT_PTR_NULL(current->cause);
    CU_ASSERT_PTR_NULL(BXIERR_TEST_CHAIN->cause);
    /* Chain with static error should not work*/
    BXIERR_CHAIN(current, BXIERR_TEST_CHAIN);
    CU_ASSERT_PTR_NULL(current->cause);
    CU_ASSERT_PTR_NULL(BXIERR_TEST_CHAIN->cause);
    bxierr_p test = (bxierr_p)BXIERR_TEST_CHAIN;
    BXIERR_CHAIN(test, current);
    CU_ASSERT_PTR_NULL(current->cause);
    CU_ASSERT_PTR_NULL(BXIERR_TEST_CHAIN->cause);
    OUT(TEST_LOGGER, "Test of perror: %s", str);
    BXIFREE(str);

    bxierr_destroy(&current);
}

void test_bxierr_chain() {
    bxierr_p err = BXIERR_OK, err2;

    err2 = bxierr_new(420, "Level 0", NULL, NULL, NULL, "Initial Cause of error");
    BXIERR_CHAIN(err, err2);

    err2 = bxierr_new(421, "Level 1", NULL, NULL, NULL, "Intermediate Error");
    BXIERR_CHAIN(err, err2);

    err2 = bxierr_new(422, "Level 2", NULL, NULL, NULL, "Top Level Error");
    BXIERR_CHAIN(err, err2);

    CU_ASSERT_TRUE_FATAL(bxierr_isko(err));



    CU_ASSERT_EQUAL_FATAL(err->code, 422);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->data);
    CU_ASSERT_STRING_EQUAL(err->data, "Level 2");

    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause);
    CU_ASSERT_EQUAL_FATAL(err->cause->code, 421);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->data);
    CU_ASSERT_STRING_EQUAL(err->cause->data, "Level 1");

    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause);
    CU_ASSERT_EQUAL_FATAL(err->cause->cause->code, 420);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->data);
    CU_ASSERT_STRING_EQUAL(err->cause->cause->data, "Level 0");

    bxierr_p nerr = BXIERR_OK, nerr2;
    nerr2 = bxierr_new(430, "Level 10", NULL, NULL, NULL, "Initial Cause of error");
    BXIERR_CHAIN(nerr, nerr2);

    nerr = bxierr_new(431, "Level 11", NULL, NULL, nerr, "Intermediate Error");

    nerr2 = bxierr_new(432, "Level 12", NULL, NULL, NULL, "Top Level Error");
    BXIERR_CHAIN(nerr, nerr2);

    BXIERR_CHAIN(err, nerr);

    CU_ASSERT_EQUAL_FATAL(err->code, 432);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->data);
    CU_ASSERT_STRING_EQUAL(err->data, "Level 12");

    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause);
    CU_ASSERT_EQUAL_FATAL(err->cause->code, 431);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->data);
    CU_ASSERT_STRING_EQUAL(err->cause->data, "Level 11");

    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause);
    CU_ASSERT_EQUAL_FATAL(err->cause->cause->code, 430);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->data);
    CU_ASSERT_STRING_EQUAL(err->cause->cause->data, "Level 10");

    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->cause);
    CU_ASSERT_EQUAL_FATAL(       err->cause->cause->cause->code, 422);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->cause->data);
    CU_ASSERT_STRING_EQUAL(      err->cause->cause->cause->data, "Level 2");

    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->cause->cause);
    CU_ASSERT_EQUAL_FATAL(       err->cause->cause->cause->cause->code, 421);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->cause->cause->data);
    CU_ASSERT_STRING_EQUAL(      err->cause->cause->cause->cause->data, "Level 1");

    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->cause->cause->cause);
    CU_ASSERT_EQUAL_FATAL(       err->cause->cause->cause->cause->cause->code, 420);
    CU_ASSERT_PTR_NOT_NULL_FATAL(err->cause->cause->cause->cause->cause->data);
    CU_ASSERT_STRING_EQUAL(      err->cause->cause->cause->cause->cause->data, "Level 0");

    bxierr_destroy(&err);
}

