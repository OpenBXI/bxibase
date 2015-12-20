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
#include <syslog.h>
#include <inttypes.h>

#include <CUnit/Basic.h>

#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/log.h"

#include "bxi/base/log/console_handler.h"
#include "bxi/base/log/file_handler.h"
#include "bxi/base/log/snmplog_handler.h"
#include "bxi/base/log/syslog_handler.h"

SET_LOGGER(TEST_LOGGER, "test.bxibase.log");
SET_LOGGER(BAD_LOGGER1, "test.bad.logger");
SET_LOGGER(BAD_LOGGER2, "test.bad.logger");


extern char * PROGNAME;
extern char * FULLFILENAME;

size_t produce_simple_logs(bxilog_logger_p logger) {
    char ** level2str;
    bxilog_get_all_level_names(&level2str);

    size_t logged_msg_nb = 0;

    // Log at all level, we use a loop here to generate many messages
    // and watch how post processing deal with them.
    // Do not change it!
    srand((unsigned int)time(NULL));
    for (bxilog_level_e level = BXILOG_PANIC; level <= BXILOG_LOWEST; level++) {
        size_t n = (size_t)(rand()% 3 + 3);
        for (size_t i = 0; i < n; i++) {
            size_t len = (size_t) (rand() % 18 + 2);
            char buf[len];
            for (size_t i = 0; i < len; i++) {
                buf[i] = (char) ('A' + (char) (rand()%50));
            }
            buf[len-1] = '\0';
            bxilog_logger_log(logger, level,
                              __FILE__, ARRAYLEN(__FILE__),
                              __FUNCTION__, ARRAYLEN(__FUNCTION__),
                              __LINE__, "One log line at level %s with some garbage: %s",
                              level2str[level], buf);
            logged_msg_nb++;
        }
    }
    // Random level order
    size_t n = (size_t)(rand() % 3 + 3);
    for (size_t i = 0; i < n; i++) {
        bxilog_level_e level = (bxilog_level_e)(rand() % (BXILOG_LOWEST + 1
                                                          - BXILOG_OFF)
                                                          + BXILOG_OFF);
        size_t len = (size_t)(rand()% 18 + 2);
        char buf[len];
        for (size_t i = 0; i < len; i++) {
            buf[i] = (char) ('A' + (char) (rand()%50));
        }
        buf[len-1] = '\0';
        bxilog_logger_log(logger, level,
                          __FILE__, ARRAYLEN(__FILE__),
                          __FUNCTION__, ARRAYLEN(__FUNCTION__),
                          __LINE__,
                          "One log line at level %s with some garbage: %s",
                          level2str[level], buf);
        if (BXILOG_OFF != level) logged_msg_nb++;
    }

    PANIC(logger, "One log line at PANIC level");
    ALERT(logger, "One log line at ALERT level");
    CRITICAL(logger, "One log line at CRITICAL level");
    ERROR(logger, "One log line at ERROR level");
    WARNING(logger, "One log line at WARNING level");
    NOTICE(logger, "One log line at NOTICE level");
    OUT(logger, "One log line at OUTPUT level");
    INFO(logger, "One log line at INFO level");
    DEBUG(logger, "One log line at DEBUG level");
    FINE(logger, "One log line at FINE level");
    TRACE(logger, "One log line at TRACE level");
    LOWEST(logger, "One log line at LOWEST level");
    logged_msg_nb += (BXILOG_LOWEST - BXILOG_PANIC + 1);

    return logged_msg_nb;
}

size_t produce_complex_logs() {
    size_t logged_msg_nb = 0;

    char* str = bxierr_backtrace_str();
    INFO(TEST_LOGGER, "One backtrace at level INFO: %s", str);
    logged_msg_nb++;
    BXIFREE(str);
    char buf[2048];
    for (size_t i = 0;i < sizeof (buf);i++){
        buf[i] = (char)('A' + (char)(i % 50));
    }
    buf[2047] = '\0';
    OUT(TEST_LOGGER, "One big log at level OUTPUT: %s", buf);
    logged_msg_nb++;

    bxierr_p err = bxilog_flush();
    bxierr_abort_ifko(err);
    bxierr_p err2 = bxierr_gen("An error to report");
    BXIERR_CHAIN(err, err2);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_OUTPUT, err,
                       "Don't worry, this is just a test for error reporting "
                       "at level OUTPUT");
    logged_msg_nb++;
    CU_ASSERT_FALSE(bxierr_isok(err));
    BXILOG_REPORT(TEST_LOGGER, BXILOG_OUTPUT, err,
                  "Don't worry, this is just another test for error reporting "
                  "at level OUTPUT");
    logged_msg_nb++;
    err2 = bxierr_gen("An error to report");
    err2->str = NULL;
    BXIERR_CHAIN(err, err2);
    err->str = NULL;
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_OUTPUT, err,
                       "Don't worry, this is just a test for error reporting "
                       "at level OUTPUT");
    logged_msg_nb++;
    CU_ASSERT_FALSE(bxierr_isok(err));
    BXILOG_REPORT(TEST_LOGGER, BXILOG_OUTPUT, err,
                  "Don't worry, this is just another test for error reporting "
                  "at level OUTPUT");
    logged_msg_nb++;
    CU_ASSERT_TRUE(bxierr_isok(err));
    OUT(TEST_LOGGER, "Ending test at level OUTPUT");
    logged_msg_nb++;

    return logged_msg_nb;
}

/*
 *  Check logger initialization
 */
void test_logger_levels(void) {
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report_keep(err, STDERR_FILENO);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "Starting test");

    produce_simple_logs(TEST_LOGGER);
    produce_complex_logs();
    err = bxilog_finalize(true);
    bxierr_abort_ifko(err);
}

void test_very_long_log(void) {
    char * dirtmp = strdup(FULLFILENAME);
    char * basetmp = strdup(FULLFILENAME);
    char * dir = dirname(dirtmp);
    char * base = basename(basetmp);

    char * longlog_filename = bxistr_new("%s/long-%s", dir, base);
    BXIFREE(dirtmp);
    BXIFREE(basetmp);

    bxilog_filters_p all_except_long = bxilog_filters_new();
    bxilog_filters_add(&all_except_long, "", BXILOG_ALL);
    bxilog_filters_add(&all_except_long, "test.bxi.base.log.long", BXILOG_OFF);

    bxilog_filters_p only_long = bxilog_filters_new();
    bxilog_filters_add(&only_long, "", BXILOG_OFF);
    bxilog_filters_add(&only_long, "test.bxi.base.log.long", BXILOG_ALL);

    bxilog_config_p config = bxilog_config_new(PROGNAME);
    bxilog_config_add_handler(config,
                              BXILOG_FILE_HANDLER,
                              all_except_long,
                              PROGNAME, FULLFILENAME, BXI_APPEND_OPEN_FLAGS);
    bxilog_config_add_handler(config,
                              BXILOG_FILE_HANDLER,
                              only_long,
                              PROGNAME, longlog_filename, BXI_TRUNC_OPEN_FLAGS);

    bxierr_p err = bxilog_init(config);
    bxierr_report_keep(err, STDERR_FILENO);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    OUT(TEST_LOGGER, "Finding new long logger");

    bxilog_logger_p long_logger;
    err = bxilog_registry_get("test.bxi.base.log.long", &long_logger);
    bxierr_abort_ifko(err);

    const size_t size = 1024*1024;
    char buf[size];

    for (size_t i = 0; i < size; i++) {
        buf[i] = (char) ('a' + (i % ('z' - 'a')));
    }
    buf[size-1] = '\0';
    OUT(TEST_LOGGER, "Generating a very long message of size %zu in %s",
        size, longlog_filename);

    OUT(long_logger, "%s", buf);

    bxilog_flush();

    struct stat stat_s;
    int rc = stat(longlog_filename, &stat_s);
    bxiassert(0 == rc);
    size_t fsize = (size_t) stat_s.st_size;

    OUT(TEST_LOGGER, "Size of %s: %"PRIu64, longlog_filename, (uint64_t)stat_s.st_size);

    CU_ASSERT_TRUE(fsize > size);
    CU_ASSERT_TRUE(fsize < 2*size);

    err = bxilog_finalize(true);
    bxierr_abort_ifko(err);
    BXIFREE(longlog_filename);
}

void test_logger_init() {
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "Starting test");
    // Initializing the library twice must not be possible
    // since the options/parameters might vary between the two
    // and checking that params and options are indeed same is
    // too heavy
    err = bxilog_init(config);
    CU_ASSERT_TRUE_FATAL(bxierr_isko(err));
    CU_ASSERT_EQUAL(err->code, BXILOG_ILLEGAL_STATE_ERR);
    bxierr_destroy(&err);
    OUT(TEST_LOGGER, "Finalizing");
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxierr_destroy(&err);
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
        bxierr_p err = bxierr_errno("Calling stat(%s) failed", name);
        bxierr_report(&err, STDERR_FILENO);
        return -1;
    }
    return info.st_size;
}

void test_logger_existing_file(void) {
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);
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

    bxilog_config_p new_config = bxilog_unit_test_config(PROGNAME,
                                                         name,
                                                         BXI_APPEND_OPEN_FLAGS);
    err = bxilog_init(new_config);

    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "One log on file: %s", name);
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(0 < _get_filesize(name));
    int rc = unlink(name);
    bxiassert(0 == rc);
    BXIFREE(template);
    BXIFREE(name);
}

void test_logger_non_existing_file(void) {
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);
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

    bxilog_config_p new_config = bxilog_unit_test_config(PROGNAME,
                                                         name,
                                                         BXI_APPEND_OPEN_FLAGS);
    err = bxilog_init(new_config);
    bxierr_report_keep(err, STDERR_FILENO);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    OUT(TEST_LOGGER, "One log on file: %s", name);
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(0 < _get_filesize(name));
    rc = unlink(name);
    bxiassert(0 == rc);
    BXIFREE(template);
    BXIFREE(name);
}

void test_logger_non_existing_dir(void) {
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    char * template = strdup("test_logger_XXXXXX");
    char * dirname = mkdtemp(template);
    bxiassert(NULL != dirname);
    char * name = bxistr_new(" %s/test_logger_non_existing_dir.bxilog", dirname);
    OUT(TEST_LOGGER, "Filename: %s", name);
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    int rc = rmdir(dirname);
    bxiassert(0 == rc);


    bxilog_config_p new_config = bxilog_unit_test_config(PROGNAME,
                                                         name,
                                                         BXI_APPEND_OPEN_FLAGS);
    // Failed because the directory does not exist
    err = bxilog_init(new_config);
    CU_ASSERT_TRUE_FATAL(bxierr_isko(err));
    bxierr_destroy(&err);
    // Fail because the library has not been initialized correctly
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    BXIFREE(name);
    BXIFREE(template);
}

static bool _is_logger_in_registered(bxilog_logger_p logger) {
    bxilog_logger_p *loggers;
    size_t n = bxilog_registry_getall(&loggers);
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
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);
    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    bxilog_logger_p logger;
    err = bxilog_registry_get("test.bxibase.log", &logger);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger);
    CU_ASSERT_PTR_EQUAL(logger, TEST_LOGGER);
    CU_ASSERT_TRUE(_is_logger_in_registered(TEST_LOGGER));

    err = bxilog_registry_get("tmp.foo.bar.single.tmp", &logger);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger);
    CU_ASSERT_PTR_NOT_EQUAL(logger, TEST_LOGGER);
    CU_ASSERT_TRUE(_is_logger_in_registered(logger));

    err = bxilog_registry_get("test.bad.logger", &logger);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger);
    CU_ASSERT_PTR_EQUAL(logger, BAD_LOGGER1);
    CU_ASSERT_PTR_NOT_EQUAL(logger, BAD_LOGGER2);
    CU_ASSERT_TRUE(_is_logger_in_registered(logger));

    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
}

void test_registry(void) {
    bxilog_registry_reset();
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    bxilog_logger_p logger_a;
    err = bxilog_registry_get("a", &logger_a);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_a);
    OUT(TEST_LOGGER, "Logger %s level: %d", logger_a->name, logger_a->level);
    CU_ASSERT_NOT_EQUAL(logger_a->level, BXILOG_OUTPUT);

    bxilog_filters_p filters = bxilog_filters_new();
    bxilog_filters_add(&filters, "", BXILOG_DEBUG);
    bxilog_filters_add(&filters, "a", BXILOG_OUTPUT);
    bxilog_filters_add(&filters, "a.b", BXILOG_WARNING);

    err = bxilog_registry_set_filters(&filters);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NULL(filters);

    CU_ASSERT_EQUAL(TEST_LOGGER->level, BXILOG_DEBUG);
    CU_ASSERT_EQUAL(logger_a->level, BXILOG_OUTPUT);

    bxilog_logger_p logger_a2;
    err = bxilog_registry_get("a.c", &logger_a2);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_a2);
    CU_ASSERT_EQUAL(logger_a2->level, BXILOG_OUTPUT);

    bxilog_logger_p logger_ab;
    err = bxilog_registry_get("a.b", &logger_ab);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_ab);
    CU_ASSERT_EQUAL(logger_ab->level, BXILOG_WARNING);

    filters = bxilog_filters_new();
    bxilog_filters_add(&filters, "", BXILOG_DEBUG);
    bxilog_filters_add(&filters, "a", BXILOG_ALERT);
    bxilog_filters_add(&filters, "a.b", BXILOG_WARNING);

    err = bxilog_registry_set_filters(&filters);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NULL(filters);

    CU_ASSERT_EQUAL(logger_a->level, BXILOG_ALERT);
    CU_ASSERT_EQUAL(logger_a2->level, BXILOG_ALERT);
    CU_ASSERT_EQUAL(logger_ab->level, BXILOG_WARNING);

    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxilog_registry_reset();
}


void test_filter_parser(void) {
    bxilog_registry_reset();

    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    bxilog_logger_p logger_z;
    err = bxilog_registry_get("z", &logger_z);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_z);
    CU_ASSERT_NOT_EQUAL(logger_z->level, BXILOG_OUTPUT);

    char filters_format[] = ":debug,z:output,z.w:WARNING";
    err = bxilog_registry_parse_set_filters(filters_format);
    CU_ASSERT_TRUE(bxierr_isok(err));

    CU_ASSERT_EQUAL(TEST_LOGGER->level, BXILOG_DEBUG);
    CU_ASSERT_EQUAL(logger_z->level, BXILOG_OUTPUT);

    bxilog_logger_p logger_z2;
    err = bxilog_registry_get("z.x", &logger_z2);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_z2);
    CU_ASSERT_EQUAL(logger_z2->level, BXILOG_OUTPUT);

    bxilog_logger_p logger_zw;
    err = bxilog_registry_get("z.w", &logger_zw);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_PTR_NOT_NULL_FATAL(logger_zw);
    CU_ASSERT_EQUAL(logger_zw->level, BXILOG_WARNING);

    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    bxilog_registry_reset();
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
        char * child_progname = bxistr_new("%s.%zu", PROGNAME, n);
        bxilog_config_p config = bxilog_unit_test_config(child_progname,
                                                         FULLFILENAME,
                                                         BXI_APPEND_OPEN_FLAGS);
        bxierr_p err = bxilog_init(config);
        bxierr_report_keep(err, STDERR_FILENO);
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
        BXIFREE(child_progname);
        BXIFREE(FULLFILENAME);
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
    bxilog_config_p config = bxilog_unit_test_config(PROGNAME,
                                                     FULLFILENAME,
                                                     BXI_APPEND_OPEN_FLAGS);
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    DEBUG(TEST_LOGGER, "Starting test");
    _fork_childs((size_t)(rand()%5 + 5));
    OUT(TEST_LOGGER, "Ending test");
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
}

void * logging_thread(void * data) {
    bxilog_logger_p logger = (bxilog_logger_p) data;

    size_t loop_nb = 5;
    size_t log_nb = 0;
    for (size_t i = 0; i < loop_nb; i++) {
        PANIC(logger, "One log line at PANIC level");
        ALERT(logger, "One log line at ALERT level");
        CRITICAL(logger, "One log line at CRITICAL level");
        ERROR(logger, "One log line at ERROR level");
        WARNING(logger, "One log line at WARNING level");
        NOTICE(logger, "One log line at NOTICE level");
        OUT(logger, "One log line at OUTPUT level");
        INFO(logger, "One log line at INFO level");
        DEBUG(logger, "One log line at DEBUG level");
        FINE(logger, "One log line at FINE level");
        TRACE(logger, "One log line at TRACE level");
        LOWEST(logger, "One log line at LOWEST level");
        log_nb += (BXILOG_LOWEST - BXILOG_PANIC + 1);

        log_nb += produce_simple_logs(logger);
    }
    return (void *) log_nb;
}

void test_logger_threads(void) {
    // Create N threads, each thread logs into its own logger
    // Create N file handlers, one for each thread/logger
    // Each thread do logs and returns the total number of logs produced
    // We then count the number of lines in each file and compare with the returned value

    size_t threads_nb = 3;
    char * filenames[threads_nb];
    size_t filenames_len[threads_nb];
    int fds[threads_nb];
    bxilog_logger_p loggers[threads_nb];
    size_t logs_nb[threads_nb];

    // Create files for each thread/console handler
    for (size_t i = 0; i < threads_nb; i++) {
        filenames[i] = strdup("/tmp/test_logger_threads.XXXXXX");
        filenames_len[i] = strlen(filenames[i]);
        fds[i] = mkstemp(filenames[i]);
        bxiassert(0 < fds[i]);
    }

    // Create the normal configuration
    bxilog_config_p config = bxilog_config_new(PROGNAME);
    bxilog_config_add_handler(config,
                              BXILOG_FILE_HANDLER,
                              BXILOG_FILTERS_ALL_ALL,
                              PROGNAME, FULLFILENAME, BXI_APPEND_OPEN_FLAGS);

    // Create each thread/console handler config
    for (size_t i = 0; i < threads_nb; i++) {
        char * logger_name = bxistr_new("test.counting-%zu", i);
        bxierr_p err = bxilog_registry_get(logger_name, &loggers[i]);
        bxierr_abort_ifko(err);
        bxilog_filters_p filters = bxilog_filters_new();
        bxilog_filters_add(&filters, logger_name, BXILOG_ALL);
        bxilog_config_add_handler(config,
                                  BXILOG_FILE_HANDLER,
                                  filters,
                                  PROGNAME, filenames[i], BXI_APPEND_OPEN_FLAGS);
        BXIFREE(logger_name);
    }

    // Init the library
    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

    char * str;
    bxistr_join(", ", strlen(", "), filenames, filenames_len, threads_nb, &str);
    OUT(TEST_LOGGER, "Starting %zu threads, logger files: %s",
        threads_nb, str);
    BXIFREE(str);

    // Start threads
    pthread_t threads[threads_nb];
    for (size_t i = 0; i < threads_nb; i++) {
        int rc = pthread_create(&threads[i], NULL, logging_thread, loggers[i]);
        CU_ASSERT_TRUE_FATAL(0 == rc);
    }

    // Join threads
    size_t total_log_nb = 0;
    for (size_t i = 0; i < threads_nb; i++) {
        int rc = pthread_join(threads[i], (void**)&logs_nb[i]);
        CU_ASSERT_TRUE_FATAL(0 == rc);
        total_log_nb += logs_nb[i];
    }

    err = bxilog_flush();
    bxierr_abort_ifko(err);

    // Check the number of lines
    size_t total_expected = 0;
    size_t total_found = 0;
    for (size_t i = 0; i < threads_nb; i++) {
        size_t lines_nb = 0;
        off_t rc = lseek(fds[i], 0, SEEK_SET);
        bxiassert(0 == rc);
        CU_ASSERT_TRUE_FATAL(0 < fds[i]);
        while(true) {
            char c;
            ssize_t n = read(fds[i], &c, 1);
            if (0 >= n) break;
            if ('\n' == c) lines_nb++;
        }
        //    lines_nb++; // Consider the EOF as a new line.
        OUT(TEST_LOGGER,
            "Number of lines expected for logger %s in file %s: %zu, found: %zu",
            loggers[i]->name, filenames[i], logs_nb[i], lines_nb);
        CU_ASSERT_TRUE(lines_nb == logs_nb[i]);
        total_expected += logs_nb[i];
        total_found += lines_nb;
    }

    // This last check is not absolutely required. However, making it
    // FATAL, prevent the files from being unlinked if the test fail.
    // That will ease debugging!
    CU_ASSERT_TRUE_FATAL(total_expected == total_found);

    for (size_t i = 0; i < threads_nb; i++) {
        unlink(filenames[i]);
        BXIFREE(filenames[i]);
    }
    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

}

void test_handlers(void) {
    bxilog_config_p config = bxilog_config_new(PROGNAME);

    bxilog_config_add_handler(config,
                              BXILOG_CONSOLE_HANDLER,
                              BXILOG_FILTERS_ALL_OFF,
                              BXILOG_WARNING);
    bxilog_config_add_handler(config,
                              BXILOG_SYSLOG_HANDLER,
                              BXILOG_FILTERS_ALL_OFF,
                              PROGNAME, LOG_CONS | LOG_PERROR, LOG_LOCAL0);
#ifdef HAVE_LIBNETSNMP
    bxilog_config_add_handler(config,
                              BXILOG_SNMPLOG_HANDLER,
                              BXILOG_FILTERS_ALL_OFF);
#endif
    bxilog_config_add_handler(config,
                              BXILOG_FILE_HANDLER,
                              BXILOG_FILTERS_ALL_ALL,
                              PROGNAME, FULLFILENAME, BXI_APPEND_OPEN_FLAGS);
//    bxilog_config_add_handler(config,
//                              BXILOG_FILE_HANDLER_STDIO,
//                              BXILOG_FILTERS_ALL_ALL,
//                              PROGNAME, FULLFILENAME);


    bxierr_p err = bxilog_init(config);
    bxierr_report(&err, STDERR_FILENO);

    err = bxilog_install_sighandler();
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
    
    OUT(TEST_LOGGER, "Starting test_handlers");
    produce_simple_logs(TEST_LOGGER);

    err = bxilog_finalize(true);
    CU_ASSERT_TRUE_FATAL(bxierr_isok(err));
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
//        snprintf(PROGNAME, strlen(PROGNAME), "%s", child_progname);
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
