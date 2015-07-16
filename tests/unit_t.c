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
#include <assert.h>
#include <libgen.h>

#include <sched.h>

#include <linux/limits.h>

#include <bxi/base/str.h>
#include <bxi/base/log.h>


SET_LOGGER(TEST_LOGGER, "test.bxibase");

char * FULLPROGNAME = NULL;
char * PROGNAME = NULL;
char * FULLFILENAME = NULL;
char ** ARGV = NULL;

// Actual test functions are separated into their respective test file.
// However, to prevent defining related .h, we define all functions here

// From test_mem.c
void test_free();

// From test_err.c
void test_bxierr();
void test_bxierr_chain();

// From test_time.c
void test_time(void);

// From test_logger.c
void test_logger_init(void);
void test_logger_levels(void);
void test_logger_existing_file(void);
void test_logger_non_existing_file(void);
void test_logger_non_existing_dir(void);
void test_logger_fork(void);
void test_logger_signal(void);
void test_single_logger_instance(void);
void test_config(void);
void test_config_parser(void);


/* The suite initialization function.
 * Opens the temporary file used by the tests.
 * Returns zero on success, non-zero otherwise.
 */
int init_suite_logger(void) {
    bxierr_p err = BXIERR_OK, err2;
    err2 = bxilog_init(PROGNAME, FULLFILENAME);
    BXIERR_CHAIN(err, err2);
    err2 = bxilog_install_sighandler();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        bxierr_report(err, STDERR_FILENO);
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
        bxierr_report(err, STDERR_FILENO);
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
                "CU_get_number_of_tests_failed %d,"
                "  CU_get_number_of_failures %d\n",
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
    UNUSED(argc);
    ARGV = argv;
    FULLPROGNAME = strdup(argv[0]);
    PROGNAME = basename(FULLPROGNAME);
    char * filename = bxistr_new("%s%s", PROGNAME, ".bxilog");
    char cwd[PATH_MAX];
    char * res = getcwd(cwd, PATH_MAX);
    assert(NULL != res);
    FULLFILENAME = bxistr_new("%s/%s", cwd, filename);
    BXIFREE(filename);
    // TODO: provides an option in bxilog for
    // "Do not append into logfile, but overwrite it"
    errno = 0;
    int rc = unlink(FULLFILENAME);
    // We just don't care if we encounter an error here!
    //    if (0 != rc) perror("Calling unlink() failed");


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
        || (NULL == CU_add_test(bxilog_suite, "test sinle logger instance", test_single_logger_instance))
        || (NULL == CU_add_test(bxilog_suite, "test logger fork", test_logger_fork))
//        || (NULL == CU_add_test(bxilog_suite, "test logger signal", test_logger_signal))
        || (NULL == CU_add_test(bxilog_suite, "test logger config", test_config))
        || (NULL == CU_add_test(bxilog_suite, "test logger config parser", test_config_parser))

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

    /* Run all tests using the automated interface */
    CU_set_output_filename("./report/bxibase");
    CU_automated_run_tests();
    CU_list_tests_to_file();

    rc = _handle_rc_code();

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    printf("\n");
    CU_basic_show_failures(CU_get_failure_list());
    printf("\n\n");

    int rc2 = _handle_rc_code();
    rc = rc > rc2 ? rc : rc2;

    CU_cleanup_registry();
    bxierr_p err = bxilog_finalize(true);
    bxierr_report(err, STDERR_FILENO);

    BXIFREE(FULLFILENAME);
    BXIFREE(FULLPROGNAME);

    return rc;
}
