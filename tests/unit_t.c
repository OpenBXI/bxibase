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

// From test_time.c
void test_time(void);

// From test_logger.c
void test_logger_init(void);
void test_logger_levels(void);
void test_logger_fork(void);
void test_logger_signal(void);


/* The suite initialization function.
 * Opens the temporary file used by the tests.
 * Returns zero on success, non-zero otherwise.
 */
int init_bxitp_suite(void) {
    errno = 0;
    return 0;
}

/* The suite cleanup function.
 * Closes the temporary file used by the tests.
 * Returns zero on success, non-zero otherwise.
 */
int clean_bxitp_suite(void) {
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
    bxierr_p err = bxilog_init(PROGNAME, FULLFILENAME);
    assert(BXIERR_OK == err);
//    bxilog_install_sighandler();

    fprintf(stderr, "Logging to file: %s\n", FULLFILENAME);

    CU_pSuite pSuite = NULL;

    /* initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();

    /* add a suite to the registry */
    pSuite = CU_add_suite("BXI_TP_TestSuite", init_bxitp_suite, clean_bxitp_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return (CU_get_error());
    }

    /* add the tests to the suite */
    if (false
        || (NULL == CU_add_test(pSuite, "test free", test_free))
        // Do not change this order: since unit test uses logging,
        // it is better to check that logging works first.
        || (NULL == CU_add_test(pSuite, "test logger init", test_logger_init))
        || (NULL == CU_add_test(pSuite, "test logger levels", test_logger_levels))
        || (NULL == CU_add_test(pSuite, "test logger fork", test_logger_fork))
//        || (NULL == CU_add_test(pSuite, "test logger signal", test_logger_signal))

        || (NULL == CU_add_test(pSuite, "test bxierr", test_bxierr))

        || (NULL == CU_add_test(pSuite, "test time", test_time))


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
    err = bxilog_finalize();
    if (BXIERR_OK != err) {
        char * str = bxierr_str(err);
        fprintf(stderr, "WARNING: bxilog finalization returned: %s", str);
        BXIFREE(str);
        bxierr_destroy(&err);
    }

    BXIFREE(FULLFILENAME);
    BXIFREE(FULLPROGNAME);

    return rc;
}
