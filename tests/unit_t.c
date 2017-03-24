/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 21, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <CUnit/Basic.h>
#include <CUnit/Console.h>
#include <CUnit/Automated.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <libgen.h>
#include <fcntl.h>

#include <sched.h>

#include <linux/limits.h>

#include <bxi/base/err.h>
#include <bxi/base/str.h>
#include <bxi/base/log.h>
#include <bxi/base/log/file_handler.h>


SET_LOGGER(TEST_LOGGER, "test.bxibase");

char * FULLPROGNAME = NULL;
char * PROGNAME = NULL;
char * FULLFILENAME = NULL;
int ARGC;
char ** ARGV = NULL;

// Actual test functions are separated into their respective test file.
// However, to prevent defining related .h, we define all functions here

// From test_mem.c
void test_free(void);


// From test_str.c
void test_bxistr_apply_lines(void);
void test_bxistr_prefix_lines(void);
void test_bxistr_join(void);
void test_bxistr_rfind(void);
void test_bxistr_count(void);
void test_bxistr_mkshorter(void);
void test_bxistr_hex(void);

// From test_err.c
void test_bxierr(void);
void test_bxierr_chain(void);

// From test_time.c
void test_time(void);

// From test_zmq.c
void test_bxizmq_generate_url(void);

// From test_logger.c
void test_logger_init(void);
void test_logger_levels(void);
void test_logger_existing_file(void);
void test_logger_non_existing_file(void);
void test_logger_non_existing_dir(void);
void test_logger_fork(void);
void test_logger_signal(void);
void test_single_logger_instance(void);
void test_registry(void);
void test_filters_parser(void);
void test_filter_merge_same(void);
void test_filter_merge_distinct(void);
void test_filter_merge_prefix(void);
void test_filters_merge_same(void);
void test_filters_merge_distinct(void);
void test_filters_merge_simple(void);
void test_filters_merge_symetric(void);
void test_filters_merge_complex(void);
void test_logger_threads(void);
void test_handlers(void);
void test_very_long_log(void);
void test_strange_log(void);


/* The suite initialization function.
 * Opens the temporary file used by the tests.
 * Returns zero on success, non-zero otherwise.
 */
int init_suite_logger(void) {
    bxierr_p err = BXIERR_OK, err2;
    bxilog_config_p config = bxilog_unit_test_config(ARGV[0],
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    err2 = bxilog_init(config);
    BXIERR_CHAIN(err, err2);
    err2 = bxilog_install_sighandler();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        bxierr_report(&err, STDERR_FILENO);
        exit(1);
    }
    return 0;
}

/* The suite cleanup function.
 * Closes the temporary file used by the tests.
 * Returns zero on success, non-zero otherwise.
 */
int clean_suite_logger(void) {
    bxierr_p err = bxilog_finalize(true);
    if (bxierr_isko(err)) {
        bxierr_report(&err, STDERR_FILENO);
        fprintf(stderr, "Error while finalizing bxilog, EXITING!\n");
        exit(1);
    }
    return 0;
}

/* Test if the test have been run correctly */
int _handle_rc_code(){
    if(CU_get_error()!=0){
        return 99;
    } else {
        fprintf(stderr, "CU_get_number_of_suites_failed %d, "
                "CU_get_number_of_tests_failed %d, "
                "CU_get_number_of_failures %d\n",
                CU_get_number_of_suites_failed(),
                CU_get_number_of_tests_failed(),
                CU_get_number_of_failures());
        if(CU_get_number_of_failures() != 0){
            return 1;
        }
    }
    return 0;
}


/* The main() function for setting up and running the tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main(int argc, char * argv[]) {
    ARGC = argc;
    ARGV = argv;
    FULLPROGNAME = strdup(argv[0]);
    PROGNAME = basename(FULLPROGNAME);
    char * filename = bxistr_new("%s%s", PROGNAME, ".bxilog");
    char cwd[PATH_MAX];
    char * res = getcwd(cwd, PATH_MAX);
    bxiassert(NULL != res);
    FULLFILENAME = bxistr_new("%s/%s", cwd, filename);
    BXIFREE(filename);

    fprintf(stderr, "Logging to file: %s\n", FULLFILENAME);

    /* initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();


    /* add suites to the registry */
    CU_pSuite bximem_suite = CU_add_suite("bximem_suite", NULL, NULL);
    if (NULL == bximem_suite) {
        CU_cleanup_registry();
        return (CU_get_error());
    }
    /* add the tests to the suite */
    if (false
            || (NULL == CU_add_test(bximem_suite, "test free", test_free))
            || false) {

            CU_cleanup_registry();
            return (CU_get_error());
        }

    /* add suites to the registry */
    CU_pSuite bxistr_suite = CU_add_suite("bxistr_suite", NULL, NULL);
    if (NULL == bxistr_suite) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    /* add the tests to the suite */
    if (false
        || (NULL == CU_add_test(bxistr_suite, "test bxistr_apply_lines", test_bxistr_apply_lines))
        || (NULL == CU_add_test(bxistr_suite, "test bxistr_prefix_lines", test_bxistr_prefix_lines))
        || (NULL == CU_add_test(bxistr_suite, "test bxistr_join", test_bxistr_join))
        || (NULL == CU_add_test(bxistr_suite, "test bxistr_rfind", test_bxistr_rfind))
        || (NULL == CU_add_test(bxistr_suite, "test bxistr_count", test_bxistr_count))
        || (NULL == CU_add_test(bxistr_suite, "test bxistr_mkshorter", test_bxistr_mkshorter))
        || (NULL == CU_add_test(bxistr_suite, "test bxistr_hex", test_bxistr_hex))
        || false) {
        CU_cleanup_registry();
        return (CU_get_error());
    }


    /* add suites to the registry */
    CU_pSuite bxierr_suite = CU_add_suite("bxierr_suite",
                                          init_suite_logger,
                                          clean_suite_logger);
    if (NULL == bxierr_suite) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    /* add the tests to the suite */
    if (false
        || (NULL == CU_add_test(bxierr_suite, "test bxierr", test_bxierr))
        || (NULL == CU_add_test(bxierr_suite,
                                "test bxierr_chain", test_bxierr_chain))
        || false) {
        CU_cleanup_registry();
        return (CU_get_error());
    }


    CU_pSuite bxitime_suite = CU_add_suite("bxitime_suite",
                                           init_suite_logger,
                                           clean_suite_logger);
    if (NULL == bxitime_suite) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    /* add the tests to the suite */
    if (false
            || (NULL == CU_add_test(bxitime_suite, "test time", test_time))
            || false) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    CU_pSuite bxizmq_suite = CU_add_suite("bxizmq_suite",
                                          init_suite_logger,
                                          clean_suite_logger);
    if (NULL == bxizmq_suite) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    /* add the tests to the suite */
    if (false
            || (NULL == CU_add_test(bxizmq_suite, "test bxizmq generate url", test_bxizmq_generate_url))
            || false) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    CU_pSuite bxilog_suite = CU_add_suite("bxilog_suite", NULL, NULL);
    if (NULL == bxilog_suite) {
        CU_cleanup_registry();
        return (CU_get_error());
    }
    /* add the tests to the suite */
    if (false
        || (NULL == CU_add_test(bxilog_suite, "test logger init", test_logger_init))
        || (NULL == CU_add_test(bxilog_suite, "test logger existing file", test_logger_existing_file))
        || (NULL == CU_add_test(bxilog_suite, "test logger non existing file", test_logger_non_existing_file))
        || (NULL == CU_add_test(bxilog_suite, "test logger non existing dir", test_logger_non_existing_dir))
        || (NULL == CU_add_test(bxilog_suite, "test logger levels", test_logger_levels))
        || (NULL == CU_add_test(bxilog_suite, "test very long log", test_very_long_log))
        || (NULL == CU_add_test(bxilog_suite, "test strange log", test_strange_log))
        || (NULL == CU_add_test(bxilog_suite, "test single logger instance", test_single_logger_instance))
        || (NULL == CU_add_test(bxilog_suite, "test logger registry", test_registry))
        || (NULL == CU_add_test(bxilog_suite, "test logger filters parser", test_filters_parser))
        || (NULL == CU_add_test(bxilog_suite, "test logger filter merge same", test_filter_merge_same))
        || (NULL == CU_add_test(bxilog_suite, "test logger filter merge distinct", test_filter_merge_distinct))
        || (NULL == CU_add_test(bxilog_suite, "test logger filter merge prefix", test_filter_merge_prefix))
        || (NULL == CU_add_test(bxilog_suite, "test logger filters merge same", test_filters_merge_same))
        || (NULL == CU_add_test(bxilog_suite, "test logger filters merge distinct", test_filters_merge_distinct))
        || (NULL == CU_add_test(bxilog_suite, "test logger filters merge simple", test_filters_merge_simple))
        || (NULL == CU_add_test(bxilog_suite, "test logger filters merge symetric", test_filters_merge_symetric))
        || (NULL == CU_add_test(bxilog_suite, "test logger filters merge complex", test_filters_merge_complex))
        || (NULL == CU_add_test(bxilog_suite, "test handlers", test_handlers))
        || (NULL == CU_add_test(bxilog_suite, "test logger threads", test_logger_threads))
        || (NULL == CU_add_test(bxilog_suite, "test logger fork", test_logger_fork))
//        || (NULL == CU_add_test(bxilog_suite, "test logger signal", test_logger_signal))

        || false) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    /* Run all tests using the automated interface */
    CU_set_output_filename("./report/cunit");
    CU_automated_run_tests();
    CU_list_tests_to_file();

    int rc = _handle_rc_code();

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    printf("\n");
    CU_basic_show_failures(CU_get_failure_list());
    printf("\n\n");

    int rc2 = _handle_rc_code();
    rc = rc > rc2 ? rc : rc2;

    CU_cleanup_registry();

    BXIFREE(FULLFILENAME);
    BXIFREE(FULLPROGNAME);

    return rc;
}
