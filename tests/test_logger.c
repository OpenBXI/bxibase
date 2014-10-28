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

/*
 *  Check logger initialization
 */
void test_logger(void) {
    OUT(TEST_LOGGER, "Starting test");
    // Log at all level, we use a loop here to generate many messages
    // and watch how post processing deal with them.
    // Do not change it!
    srand((unsigned int)time(NULL));
    for (size_t level = BXILOG_PANIC; level <= BXILOG_LOWEST; level++) {
        size_t n = (size_t)(rand()% 3 + 3);
        for (size_t i = 0; i < n; i++) {
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
    }
    // Random level order
    size_t n = (size_t)(rand()% 3 + 3);
    for (size_t i = 0; i < n; i++) {
        bxilog_level_e level = rand()%(BXILOG_LOWEST+1 - BXILOG_PANIC) + BXILOG_PANIC;
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
    bxilog_flush();
    char * oldprogname = strdup(PROGNAME);
    char * oldfilename = strdup(FILENAME);
    char * fullprogname = bxistr_new("fakeprogram");
    char * progname = basename(fullprogname);
    char * filename = bxistr_new("%s%s", progname, ".log");
    bxierr_p err = bxilog_init(progname, filename);
    CU_ASSERT_EQUAL(err->code, BXILOG_ILLEGAL_STATE_ERR);
    bxierr_destroy(&err);
    err = bxilog_finalize();
    CU_ASSERT_TRUE(bxierr_isok(err));
    STATE = FINALIZING;
    err = bxilog_finalize();
    CU_ASSERT_EQUAL(err->code, BXILOG_ILLEGAL_STATE_ERR);
    bxierr_destroy(&err);
    STATE = FINALIZED;
    BXIFREE(fullprogname);
    BXIFREE(filename);
    err = bxilog_init(oldprogname, oldfilename);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_EQUAL(STATE, INITIALIZED);
    BXIFREE(oldfilename);
    BXIFREE(oldprogname);

    BXILOG_REPORT(TEST_LOGGER, BXILOG_OUTPUT,
                  bxierr_gen("An error to report"),
                  "Don't worry, this is just a test for error reporting");
}

void _fork_childs(size_t n) {
    if (n == 0) return;
    DEBUG(TEST_LOGGER, "Forking child #%zu", n - 1);
    errno = 0;
    char * filename = strdup(FILENAME);
    char * progname = strdup(PROGNAME);
    pid_t cpid = fork();
    switch(cpid) {
    case -1: {
        BXIFREE(filename);
        BXIFREE(progname);
        BXIEXIT(EXIT_FAILURE,
                bxierr_error("Can't fork()"),
                TEST_LOGGER, BXILOG_CRITICAL);
        break;
    }
    case 0: { // In the child
        n--;
        char * child_progname = bxistr_new("%s.%zu", progname, n);
        snprintf(ARGV[0], strlen(ARGV[0]), "%s", child_progname);
        if (STATE != FINALIZED) {
            error(EXIT_FAILURE, 0,
                  "Unexpected state for bxilog in child #%zu: %d", n, STATE);
        }
        bxierr_p err = bxilog_init(child_progname, filename);
        BXIFREE(child_progname);
        BXIFREE(progname);
        BXIFREE(filename);
        CU_ASSERT_TRUE(bxierr_isok(err));
        // Check that logging works as expected in the child too
        CRITICAL(TEST_LOGGER, "Child #%zu: One log line", n);
        ERROR(TEST_LOGGER, "Child #%zu: One log line", n);
        WARNING(TEST_LOGGER, "Child #%zu: One log line", n);
        OUT(TEST_LOGGER, "Child #%zu: One log line", n);
        INFO(TEST_LOGGER, "Child #%zu: One log line", n);
        DEBUG(TEST_LOGGER, "Child #%zu: One log line", n);
        TRACE(TEST_LOGGER, "Child #%zu: One log line", n);
        _fork_childs(n);
        err = bxilog_finalize();
        if (bxierr_isko(err)) {
            error(EXIT_FAILURE, 0,
                  "Calling bxilog_finalize() failed in child #%zu: %s",
                  n, bxierr_str(err));
        }
        exit(EXIT_SUCCESS);
        break;
    }
    default: {  // In the parent
        BXIFREE(filename);
        BXIFREE(progname);
        CRITICAL(TEST_LOGGER, "Parent #%zu: One log line", n);
        ERROR(TEST_LOGGER, "Parent #%zu: One log line", n);
        WARNING(TEST_LOGGER, "Parent #%zu: One log line", n);
        OUT(TEST_LOGGER, "Parent #%zu: One log line", n);
        INFO(TEST_LOGGER, "Parent #%zu: One log line", n);
        DEBUG(TEST_LOGGER, "Parent #%zu: One log line", n);
        TRACE(TEST_LOGGER, "Parent #%zu: One log line", n);
        int status;
        DEBUG(TEST_LOGGER, "Parent #%zu: Waiting termination of child #%zu pid:%d", n, n - 1, cpid);
        errno = 0;
        pid_t w = waitpid(cpid, &status, WUNTRACED);
        if (-1 == w) {
            BXIEXIT(EX_SOFTWARE, bxierr_error("Can't wait()"),
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
    DEBUG(TEST_LOGGER, "Starting test");
    _fork_childs((size_t)(rand()%5 + 5));
}

void * _logger_thread_dummy(void * dummy) {
    UNUSED(dummy);
    while(true) {
        OUT(TEST_LOGGER, "Logging useless stuff");
        bxitime_sleep(CLOCK_MONOTONIC, 0, 1e6);
    }
    return NULL;
}

void _fork_kill(int signum) {
    DEBUG(TEST_LOGGER, "Starting test");
    errno = 0;
    pid_t cpid = fork();
    switch(cpid) {
    case -1: {
        BXIEXIT(EXIT_FAILURE,
                bxierr_error("Can't fork()"),
                TEST_LOGGER, BXILOG_CRITICAL);
        break;
    }
    case 0: { // In the child
        char * filename = strdup(FILENAME);
        char * progname = strdup(PROGNAME);
        int fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        assert(fd != -1);
        // Redirect input/output to the file
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
        char * child_progname = bxistr_new("noval-%s.child", progname);
        snprintf(ARGV[0], strlen(ARGV[0]), "%s", child_progname);
        bxierr_p err = bxilog_init(child_progname, filename);
        CU_ASSERT_TRUE(bxierr_isok(err));
        BXIFREE(child_progname);
        BXIFREE(filename);
        BXIFREE(progname);
        bxilog_install_sighandler();
        pthread_t thread;
        UNUSED(thread);
        int rc = pthread_create(&thread, NULL, _logger_thread_dummy, NULL);
        assert(rc == 0);
        OUT(TEST_LOGGER, "Waiting for being killed!");
        bxitime_sleep(CLOCK_MONOTONIC, 5, 0);
        BXIEXIT(EXIT_FAILURE, bxierr_gen("Should have been killed, but was not"),
                TEST_LOGGER, BXILOG_CRITICAL);
        break;
    }
    default: {  // In the parent
        bxitime_sleep(CLOCK_MONOTONIC, 0, 10e6); // Wait a bit...
        OUT(TEST_LOGGER, "Killing %d with signal %d", cpid, signum);
        errno = 0;
        int rc = kill(cpid, signum);
        if (-1 == rc) {
            BXIEXIT(EXIT_FAILURE,
                    bxierr_error("Calling kill(%d, %d) failed", cpid, signum),
                    TEST_LOGGER, BXILOG_CRITICAL);
        }
        int status;
        OUT(TEST_LOGGER, "Waiting termination of pid:%d", cpid);
        errno = 0;
        int times = 50000;
        while(true) {
            times--;
            pid_t w = waitpid(cpid, &status, WNOHANG);
            if (w > 0) break;
            if (0 == w && 0 == times) {
                BXIEXIT(EXIT_FAILURE,
                        bxierr_error("Unable to wait for %d", cpid),
                        TEST_LOGGER, BXILOG_CRITICAL);
            }
            if (-1 == w) {
                BXIEXIT(EXIT_FAILURE,
                        bxierr_error("Can't wait()"),
                        TEST_LOGGER, BXILOG_CRITICAL);
            }
            bxitime_sleep(CLOCK_MONOTONIC, 0, 1e6); // Wait a bit...
        }
        if (WIFEXITED(status)) {
            OUT(TEST_LOGGER, "Process pid: %d terminated, with error code=%d",
                cpid, WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status)) {
            OUT(TEST_LOGGER, "Process pid: %d terminated, with signal=%d",
                            cpid, WTERMSIG(status));
        }
        break;
    }
    }
}

void test_logger_signal(void) {
    int allsig_num[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGINT, SIGTERM, SIGQUIT};
    for (size_t i = 0; i < ARRAYLEN(allsig_num); i++) {
        _fork_kill(allsig_num[i]);
    }
//    _fork_kill(SIGSEGV);
}

