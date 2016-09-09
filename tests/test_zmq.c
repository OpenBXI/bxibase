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
#include <pthread.h>

#include <CUnit/Basic.h>

#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/log.h"
#include "bxi/base/zmq.h"
#include "bxi/base/time.h"

SET_LOGGER(TEST_LOGGER, "test.bxizmq");

void test_bxizmq_generate_url() {

    bxierr_p err;

    char * url, *result;

    url = "inproc://something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "inproc:///something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc://something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc:///something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc://something/bar";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc:///something/bar";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "tcp://localhost:*";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_EQUAL(url, result);
    BXIFREE(result);

    url = "tcp://localhost:27182";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp("tcp://localhost:", result, strlen("tcp://localhost:")));
    BXIFREE(result);

    err = bxizmq_generate_new_url_from("pgm://wtf", &result);
    CU_ASSERT_TRUE(bxierr_isko(err));
    bxierr_destroy(&err);

}
