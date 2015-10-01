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
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <libgen.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <wait.h>
#include <error.h>
#include <poll.h>
#include <signal.h>

#include <CUnit/Basic.h>

#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/log.h"

SET_LOGGER(TEST_LOGGER, "test.bxibase.log");
SET_LOGGER(BAD_LOGGER1, "test.bad.logger");
SET_LOGGER(BAD_LOGGER2, "test.bad.logger");

extern char * FULLPROGNAME;
extern char * BXILOG__CORE_PROGNAME;
extern char * FULLFILENAME;
extern int ARGC;
extern char ** ARGV;

/*
 *  Check logger initialization
 */
void test_logger_levels(void) {
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "Starting test");

    // Log at all level, we use a loop here to generate many messages
    // and watch how post processing deal with them.
    // Do not change it!
    srand((unsigned int)time(NULL));
    for (bxilog_level_e level = BXILOG_PANIC; level <= BXILOG_LOWEST; level++) {
        size_t n = (size_t)(rand()% 3 + 3);
        for (size_t i = 0; i < n; i++) {
            bxilog_level_e len = (bxilog_level_e)(rand()% 18 + 2);
            char buf[len];
            for (size_t i = 0; i < len; i++) {
                buf[i] = (char) ('A' + (char) (rand()%50));
            }
            buf[len-1] = '\0';
            bxilog_log(TEST_LOGGER, level,
                       __FILE__, ARRAYLEN(__FILE__),
                       __FUNCTION__, ARRAYLEN(__FUNCTION__),
                       __LINE__, "One log line with some garbage: %s", buf);
        }
    }
    // Random level order
    size_t n = (size_t)(rand()% 3 + 3);
    for (size_t i = 0; i < n; i++) {
        bxilog_level_e level = (bxilog_level_e)(rand()%(BXILOG_LOWEST + 1
                                                        - BXILOG_PANIC)
                                                + BXILOG_PANIC);
        size_t len = (size_t)(rand()% 18 + 2);
        char buf[len];
        for (size_t i = 0; i < len; i++) {
            buf[i] = (char) ('A' + (char) (rand()%50));
        }
        buf[len-1] = '\0';
        bxilog_log(TEST_LOGGER, level,
                   __FILE__, ARRAYLEN(__FILE__),
                   __FUNCTION__, ARRAYLEN(__FUNCTION__),
                   __LINE__, "One log line with some garbage: %s", buf);
    }

    PANIC(TEST_LOGGER, "One log line");
    ALERT(TEST_LOGGER, "One log line");
    CRITICAL(TEST_LOGGER, "One log line");
    ERROR(TEST_LOGGER, "One log line");
    WARNING(TEST_LOGGER, "One log line");
    NOTICE(TEST_LOGGER, "One log line");
    OUT(TEST_LOGGER, "One log line");
    INFO(TEST_LOGGER, "One log line");
    DEBUG(TEST_LOGGER, "One log line");
    FINE(TEST_LOGGER, "One log line");
    TRACE(TEST_LOGGER, "One log line");
    LOWEST(TEST_LOGGER, "One log line");
    char * str = bxierr_backtrace_str();
    INFO(TEST_LOGGER, "One backtrace: %s", str);
    BXIFREE(str);
    char buf[2048];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = (char) ('A' + (char) (i % 50));
    }
    buf[2047] = '\0';
    OUT(TEST_LOGGER, "One big log: %s", buf);
    err = bxilog_flush();
    bxierr_abort_ifko(err);
    bxierr_p err2 = bxierr_gen("An error to report");
    BXIERR_CHAIN(err,err2);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_OUTPUT,
                       err,
                       "Don't worry, this is just a test for error reporting");
    CU_ASSERT_FALSE(bxierr_isok(err));
    BXILOG_REPORT(TEST_LOGGER, BXILOG_OUTPUT,
                  err,
                  "Don't worry, this is just another test for error reporting");
    err2 = bxierr_gen("An error to report");
    err2->str = NULL;
    BXIERR_CHAIN(err,err2);
    err->str = NULL;
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_OUTPUT,
                       err,
                       "Don't worry, this is just a test for error reporting");
    CU_ASSERT_FALSE(bxierr_isok(err));
    BXILOG_REPORT(TEST_LOGGER, BXILOG_OUTPUT,
                  err,
                  "Don't worry, this is just another test for error reporting");
    CU_ASSERT_TRUE(bxierr_isok(err));
    OUT(TEST_LOGGER, "Ending test");
    err = bxilog_finalize(true);
    bxierr_abort_ifko(err);
    bxilog_param_destroy(&param);
}

void test_logger_init() {
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "Starting test");
    // Initializing the library twice must not be possible
    // since the options/parameters might vary between the two
    // and checking that params and options are indeed same is
    // too heavy
    err = bxilog_init(param);
    CU_ASSERT_TRUE_FATAL(bxierr_isko(err));
    CU_ASSERT_EQUAL(err->code, BXILOG_ILLEGAL_STATE_ERR);
    bxierr_destroy(&err);
    OUT(TEST_LOGGER, "Finalizing");
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxierr_destroy(&err);
    bxilog_param_destroy(&param);
}

static char * _get_filename(int fd) {
    char path[512];
    const size_t n = 512 * sizeof(*path);
    char * const result = malloc(n);
    bxiassert(result != NULL);
    sprintf(path, "/proc/self/fd/%d", fd);
    memset(result, 0, n);
    errno = 0;
    /* Read out the link to our file descriptor. */
    const ssize_t rc = readlink(path, result, n - 1);
    bxiassert(-1 != rc);
    return result;
}

static long _get_filesize(char* name) {
    struct stat info;
    errno = 0;
    int rc = stat(name, &info);
    if (-1 == rc) {
        bxierr_report(bxierr_errno("Calling stat(%s) failed", name),
                      STDERR_FILENO);
        return -1;
    }
    return info.st_size;
}

void test_logger_existing_file(void) {
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "Starting test");
    char * template = strdup("test_logger_XXXXXX");
    int fd = mkstemp(template);
    bxiassert(-1 != fd);
    char * name = _get_filename(fd);
    OUT(TEST_LOGGER, "Filename: %s", name);
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    CU_ASSERT_TRUE_FATAL(0 == _get_filesize(name));
    close(fd);
    bxilog_param_destroy(&param);

    bxilog_param_p new_param;
    err= bxilog_unit_test_config(ARGV[0],
                             name, true, &new_param);
    bxierr_report(err, STDERR_FILENO);
    err = bxilog_init(new_param);

    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "One log on file: %s", name);
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(0 < _get_filesize(name));
    int rc = unlink(name);
    bxiassert(0 == rc);
    BXIFREE(template);
    BXIFREE(name);
    bxilog_param_destroy(&new_param);
}

void test_logger_non_existing_file(void) {
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    char * template = strdup("test_logger_XXXXXX");
    int fd = mkstemp(template);
    bxiassert(-1 != fd);
    char * name = _get_filename(fd);
    OUT(TEST_LOGGER, "Filename: %s", name);
    err = bxilog_finalize(true);

    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    CU_ASSERT_TRUE_FATAL(0 == _get_filesize(name));
    close(fd);
    int rc = unlink(name);
    bxiassert(0 == rc);
    bxilog_param_destroy(&param);

    bxilog_param_p new_param;
    err = bxilog_unit_test_config(ARGV[0], name, true, &new_param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(new_param);
    bxierr_report(err, STDERR_FILENO);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "One log on file: %s", name);
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(0 < _get_filesize(name));
    rc = unlink(name);
    bxiassert(0 == rc);
    BXIFREE(template);
    BXIFREE(name);
    bxilog_param_destroy(&new_param);
}

void test_logger_non_existing_dir(void) {
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    char * template = strdup("test_logger_XXXXXX");
    char * dirname = mkdtemp(template);
    bxiassert(NULL != dirname);
    char * name = bxistr_new(" %s/test_logger_non_existing_dir.bxilog", dirname);
    OUT(TEST_LOGGER, "Filename: %s", name);
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxilog_param_destroy(&param);

    int rc = rmdir(dirname);
    bxiassert(0 == rc);


    bxilog_param_p new_param;
    err = bxilog_unit_test_config(ARGV[0], name, true, &new_param);
    bxiassert(bxierr_isok(err));
    // Failed because the directory does not exist
    err = bxilog_init(new_param);
    CU_ASSERT_TRUE_FATAL(bxierr_isko(err));
    bxierr_destroy(&err);
    // Fail because the library has not been initialized correctly
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    BXIFREE(name);
    BXIFREE(template);
    bxilog_param_destroy(&new_param);
}

static bool _is_logger_in_registered(bxilog_p logger) {
    bxilog_p *loggers;
    size_t n = bxilog_get_registered(&loggers);
    bool found = false;
    while(n-- > 0) {
        if (loggers[n] == logger) {
            found = true;
            break;
        }
    }
    BXIFREE(loggers);
    return found;
}

void test_single_logger_instance(void) {
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    bxilog_p logger;
    err = bxilog_get("test.bxibase.log", &logger);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger);
    CU_ASSERT_PTR_EQUAL(logger, TEST_LOGGER);
    CU_ASSERT_TRUE(_is_logger_in_registered(TEST_LOGGER));

    err = bxilog_get("tmp.foo.bar.single.tmp", &logger);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger);
    CU_ASSERT_PTR_NOT_EQUAL(logger, TEST_LOGGER);
    CU_ASSERT_TRUE(_is_logger_in_registered(logger));

    err = bxilog_get("test.bad.logger", &logger);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger);
    CU_ASSERT_PTR_EQUAL(logger, BAD_LOGGER1);
    CU_ASSERT_PTR_NOT_EQUAL(logger, BAD_LOGGER2);
    CU_ASSERT_TRUE(_is_logger_in_registered(logger));

    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxilog_param_destroy(&param);
}

void test_config(void) {
    bxilog_reset_registry();
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    bxilog_p logger_a;
    err = bxilog_get("a", &logger_a);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_a);
    OUT(TEST_LOGGER, "Logger %s level: %d", logger_a->name, logger_a->level);
    CU_ASSERT_NOT_EQUAL(logger_a->level, BXILOG_OUTPUT);

    bxilog_cfg_item_s cfg[] = {{.prefix="", .level=BXILOG_DEBUG},
                               {.prefix="a", .level=BXILOG_OUTPUT},
                               {.prefix="a.b", .level=BXILOG_WARNING},
    };
    err = bxilog_cfg_registered(3, cfg);
    CU_ASSERT_TRUE(bxierr_isok(err));

    CU_ASSERT_EQUAL(TEST_LOGGER->level, BXILOG_DEBUG);
    CU_ASSERT_EQUAL(logger_a->level, BXILOG_OUTPUT);

    bxilog_p logger_a2;
    err = bxilog_get("a.c", &logger_a2);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_a2);
    CU_ASSERT_EQUAL(logger_a2->level, BXILOG_OUTPUT);

    bxilog_p logger_ab;
    err = bxilog_get("a.b", &logger_ab);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_ab);
    CU_ASSERT_EQUAL(logger_ab->level, BXILOG_WARNING);


    cfg[1].level = BXILOG_ALERT;
    err = bxilog_cfg_registered(3, cfg);
    CU_ASSERT_TRUE(bxierr_isok(err));

    CU_ASSERT_EQUAL(logger_a->level, BXILOG_ALERT);
    CU_ASSERT_EQUAL(logger_a2->level, BXILOG_ALERT);
    CU_ASSERT_EQUAL(logger_ab->level, BXILOG_WARNING);

    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxilog_param_destroy(&param);
}


void test_config_parser(void) {
    bxilog_reset_registry();

    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    bxilog_p logger_z;
    err = bxilog_get("z", &logger_z);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_z);
    CU_ASSERT_NOT_EQUAL(logger_z->level, BXILOG_OUTPUT);

    char cfg[] = ":debug,z:output,z.w:WARNING";
    err = bxilog_cfg_parse(cfg);
    CU_ASSERT_TRUE(bxierr_isok(err));

    CU_ASSERT_EQUAL(TEST_LOGGER->level, BXILOG_DEBUG);
    CU_ASSERT_EQUAL(logger_z->level, BXILOG_OUTPUT);

    bxilog_p logger_z2;
    err = bxilog_get("z.x", &logger_z2);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_z2);
    CU_ASSERT_EQUAL(logger_z2->level, BXILOG_OUTPUT);

    bxilog_p logger_zw;
    err = bxilog_get("z.w", &logger_zw);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_zw);
    CU_ASSERT_EQUAL(logger_zw->level, BXILOG_WARNING);

    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxilog_reset_registry();

    bxilog_param_destroy(&param);
}


void _fork_childs(size_t n) {
    if (n == 0) return;
    DEBUG(TEST_LOGGER, "Forking child #%zu", n - 1);
    errno = 0;
    pid_t cpid = fork();
    switch(cpid) {
    case -1: {
        BXIEXIT(EXIT_FAILURE,
                bxierr_errno("Can't fork()"),
                TEST_LOGGER, BXILOG_CRITICAL);
        break;
    }
    case 0: { // In the child
        n--;
        char * child_progname = bxistr_new("%s.%zu", ARGV[0], n);
        snprintf(ARGV[0], strlen(ARGV[0]), "%s", child_progname);
        BXIFREE(child_progname);
        bxilog_param_p param;
        bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
        bxiassert(bxierr_isok(err));
        err = bxilog_init(param);
        bxierr_report(err, STDERR_FILENO);
        BXIASSERT(TEST_LOGGER, bxierr_isok(err));
        // Check that logging works as expected in the child too
        CRITICAL(TEST_LOGGER, "Child #%zu: One log line", n);
        ERROR(TEST_LOGGER, "Child #%zu: One log line", n);
        WARNING(TEST_LOGGER, "Child #%zu: One log line", n);
        OUT(TEST_LOGGER, "Child #%zu: One log line", n);
        INFO(TEST_LOGGER, "Child #%zu: One log line", n);
        DEBUG(TEST_LOGGER, "Child #%zu: One log line", n);
        TRACE(TEST_LOGGER, "Child #%zu: One log line", n);
        _fork_childs(n);
        err = bxilog_finalize(true);
        if (bxierr_isko(err)) {
            error(EXIT_FAILURE, 0,
                  "Calling bxilog_finalize(true) failed in child #%zu: %s",
                  n, bxierr_str(err));
        }
        BXIFREE(FULLFILENAME);
        BXIFREE(FULLPROGNAME);
        bxilog_param_destroy(&param);
        exit(EXIT_SUCCESS);
        break;
    }
    default: {  // In the parent
        CRITICAL(TEST_LOGGER, "Parent #%zu: One log line", n);
        ERROR(TEST_LOGGER, "Parent #%zu: One log line", n);
        WARNING(TEST_LOGGER, "Parent #%zu: One log line", n);
        OUT(TEST_LOGGER, "Parent #%zu: One log line", n);
        INFO(TEST_LOGGER, "Parent #%zu: One log line", n);
        DEBUG(TEST_LOGGER, "Parent #%zu: One log line", n);
        TRACE(TEST_LOGGER, "Parent #%zu: One log line", n);
        int status;
        DEBUG(TEST_LOGGER,
              "Parent #%zu: Waiting termination of child #%zu pid:%d", n, n - 1, cpid);
        errno = 0;
        pid_t w = waitpid(cpid, &status, WUNTRACED);
        if (-1 == w) {
            BXIEXIT(EX_SOFTWARE, bxierr_errno("Can't wait()"),
                    TEST_LOGGER, BXILOG_CRIT);
        }
        CU_ASSERT_TRUE(WIFEXITED(status));
        CU_ASSERT_EQUAL_FATAL(WEXITSTATUS(status), EXIT_SUCCESS);
        DEBUG(TEST_LOGGER, "Child #%zu, pid: %d terminated", n-1, cpid);
        break;
    }
    }
}

void test_logger_fork(void) {
    bxilog_param_p param;
    bxierr_p err = bxilog_unit_test_config(ARGV[0], FULLFILENAME, true, &param);
    bxiassert(bxierr_isok(err));
    err = bxilog_init(param);
    bxierr_report(err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    DEBUG(TEST_LOGGER, "Starting test");
    _fork_childs((size_t)(rand()%5 + 5));
    OUT(TEST_LOGGER, "Ending test");
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxilog_param_destroy(&param);
}
//
//static volatile bool _DUMMY_LOGGING = false;
//
//void * _logger_thread_dummy(void * data) {
//    size_t signum = (size_t) data;
//
//    while(true) {
//        OUT(TEST_LOGGER, "Waiting for signal: %zu", signum);
//        bxitime_sleep(CLOCK_MONOTONIC, 0, 2e8);
//        _DUMMY_LOGGING = true;
//    }
//    BXIUNREACHABLE_STATEMENT(TEST_LOGGER);
//}
//
//void _fork_kill(int signum) {
//    OUT(TEST_LOGGER, "Starting test for signum %d", signum);
//    _DUMMY_LOGGING = true;
//    char * name = bxistr_new("%s.%d", FULLFILENAME, getpid());
//    OUT(TEST_LOGGER, "Child output (stdout/stderr) is redirected to %s", name);
//    int pipefd[2];
//    int rc = pipe(pipefd);
//    BXIASSERT(TEST_LOGGER, 0 == rc);
//    errno = 0;
//    pid_t cpid = fork();
//    switch(cpid) {
//    case -1: {
//        BXIEXIT(EXIT_FAILURE,
//                bxierr_errno("Can't fork()"),
//                TEST_LOGGER, BXILOG_CRITICAL);
//        break;
//    }
//    case 0: { // In the child
//        rc = close(pipefd[0]); // Close the read-end of the pipe, we won't use it!
//        bxiassert(0 == rc);
//        int fd = open(name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
//        bxiassert(fd != -1);
////         Redirect input/output to the file
//        dup2(fd, STDOUT_FILENO);
//        dup2(fd, STDERR_FILENO);
//        rc = close(fd);
//        bxiassert(0 == rc);
//        char * child_progname = bxistr_new("%s-sig-%d.child", BXILOG__CORE_PROGNAME, signum);
//        snprintf(ARGV[0], strlen(ARGV[0]), "%s", child_progname);
////        bxierr_p err = bxilog_init(child_progname, FULLFILENAME);
//        bxierr_p err = bxilog_init(NULL);
//        BXIASSERT(TEST_LOGGER, bxierr_isok(err));
//        BXIFREE(child_progname);
//        err = bxilog_install_sighandler();
//        BXIASSERT(TEST_LOGGER, bxierr_isok(err));
//        pthread_t thread;
//        UNUSED(thread);
//        size_t data = (size_t) signum;
//        int rc = pthread_create(&thread, NULL, _logger_thread_dummy, (void*)data);
//        BXIASSERT(TEST_LOGGER, 0 == rc);
//        while(!_DUMMY_LOGGING) {
//            OUT(TEST_LOGGER, "Waiting until dummy thread log something...");
//            bxitime_sleep(CLOCK_MONOTONIC, 0, 2e8);
//        }
//        OUT(TEST_LOGGER, "Informing parent %d, we are ready to be killed!", getppid());
//        errno = 0;
//        char readystr[] = "Ready";
//        size_t count = ARRAYLEN(readystr);
//        errno = 0;
//        ssize_t n = write(pipefd[1], readystr, count);
//        BXIASSERT(TEST_LOGGER, count == (size_t) n);
//        errno = 0;
//        rc = close(pipefd[1]);
//        BXIASSERT(TEST_LOGGER, 0 == rc || (-1 == rc && errno == EINTR));
//        OUT(TEST_LOGGER, "Waiting signal %d from %d", signum, getppid());
//        bxitime_sleep(CLOCK_MONOTONIC, 5, 0);
//        BXIEXIT(EXIT_FAILURE, bxierr_gen("Should have been killed, but was not"),
//                TEST_LOGGER, BXILOG_CRITICAL);
//        break;
//    }
//    default: {  // In the parent
//        char buf[512];
//        memset(buf, 0, 512);
//        rc = close(pipefd[1]); // Close the write-end of the pipe
//        BXIASSERT(TEST_LOGGER, 0 == rc);
//        OUT(TEST_LOGGER, "Waiting ready signal from child %d", cpid);
//        while(true) { // Read while EOF
//            ssize_t n = read(pipefd[0], &buf, 512);
//            BXIASSERT(TEST_LOGGER, -1 != n);
//            if (0 == n) break; // EOF
//        }
//        rc = close(pipefd[0]);
//        BXIASSERT(TEST_LOGGER, 0 == rc);
//        OUT(TEST_LOGGER, "Child sent us: %s", buf);
//        bxitime_sleep(CLOCK_MONOTONIC, 0, 5e8);
//        OUT(TEST_LOGGER, "Killing %d with signal %d", cpid, signum);
//        errno = 0;
//        int rc = kill(cpid, signum);
//        if (-1 == rc) {
//            BXIEXIT(EXIT_FAILURE,
//                    bxierr_errno("Calling kill(%d, %d) failed", cpid, signum),
//                    TEST_LOGGER, BXILOG_CRITICAL);
//        }
//        int status;
//        OUT(TEST_LOGGER, "Waiting termination of pid:%d", cpid);
//        errno = 0;
//        int times = 50000;
//        while(true) {
//            times--;
//            pid_t w = waitpid(cpid, &status, WNOHANG);
//            if (w > 0) break;
//            if (0 == w && 0 == times) {
//                BXIEXIT(EXIT_FAILURE,
//                        bxierr_errno("Unable to wait for %d", cpid),
//                        TEST_LOGGER, BXILOG_CRITICAL);
//            }
//            if (-1 == w) {
//                BXIEXIT(EXIT_FAILURE,
//                        bxierr_errno("Can't wait()"),
//                        TEST_LOGGER, BXILOG_CRITICAL);
//            }
//            bxitime_sleep(CLOCK_MONOTONIC, 0, 1e6); // Wait a bit...
//        }
//        if (WIFEXITED(status)) {
//            OUT(TEST_LOGGER, "Process pid: %d terminated, with error code=%d",
//                cpid, WEXITSTATUS(status));
//        }
//        if (WIFSIGNALED(status)) {
//            OUT(TEST_LOGGER, "Process pid: %d terminated, with signal=%d",
//                            cpid, WTERMSIG(status));
//        }
//        CU_ASSERT_TRUE_FATAL(WIFSIGNALED(status));
//        CU_ASSERT_EQUAL_FATAL(signum, WTERMSIG(status));
//        OUT(TEST_LOGGER, "Removing file %s", name);
//        errno = 0;
//        rc = unlink(name);
//        if (0 != rc) {
//            bxierr_p err = bxierr_errno("Calling unlink(%s) failed", name);
//            BXILOG_REPORT(TEST_LOGGER, BXILOG_WARNING,
//                                   err,
//                                   "Error during cleanup");
//        }
//        BXIFREE(name);
//        break;
//    }
//    }
//}
//
//void test_logger_signal(void) {
//    bxierr_p err = bxilog_init(&BXILOG_PARAM);
//    bxierr_report(err, STDERR_FILENO);
//    err = bxilog_install_sighandler();
//    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
//    int allsig_num[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGINT, SIGTERM, SIGQUIT};
//    for (size_t i = 0; i < ARRAYLEN(allsig_num); i++) {
//        _fork_kill(allsig_num[i]);
//    }
//    OUT(TEST_LOGGER, "Ending test");
//    err = bxilog_finalize(true);
//    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
//}
//
