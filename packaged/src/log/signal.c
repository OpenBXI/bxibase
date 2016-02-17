/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <string.h>
#include <errno.h>
#include <error.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

#include "bxi/base/err.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"

#include "bxi/base/log.h"

#include "log_impl.h"



//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static void _sig_handler(int signum, siginfo_t * siginfo, void * dummy);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The internal logger
SET_LOGGER(LOGGER, BXILOG_LIB_PREFIX "bxilog.signal");

/* For signals handler */
static volatile sig_atomic_t FATAL_ERROR_IN_PROGRESS = 0;


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

// Synchronous signals produces a log and kill the initializer thread
// Asynchronous signals should be handled by the initializer thread
// and this thread is the only one allowed to call bxilog_finalize(true)
bxierr_p bxilog_install_sighandler(void) {
    // Allocate a special signal stack for SIGSEGV and the like
    stack_t sigstack;
    sigstack.ss_sp = bximem_calloc(SIGSTKSZ);
    sigstack.ss_size = SIGSTKSZ;
    sigstack.ss_flags = 0;
    errno = 0;
    int rc = sigaltstack(&sigstack, NULL);
    if (-1 == rc) return bxierr_errno("Calling sigaltstack() failed");
    FINE(LOGGER,
         "Alternate signal stack set at %p (%zu B)",
         sigstack.ss_sp,
         sigstack.ss_size);

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));

    DEBUG(LOGGER, "Setting signal handler process wide");
    // Keep default action for SIGQUIT so we can bypass bxilog specific
    // signal handling
    int allsig_num[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGINT, SIGTERM};
    // Mask all signals during the execution of a signal handler.
    sigset_t allsig_blocked;
    bxierr_p err = bxilog_sigset_new(&allsig_blocked, allsig_num, ARRAYLEN(allsig_num));
    if (bxierr_isko(err)) return err;

    sa.sa_sigaction = _sig_handler;
    sa.sa_mask = allsig_blocked;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    // Install signal handlers
    for (size_t i = 0; i < ARRAYLEN(allsig_num); i++) {
        errno = 0;
        int rc = sigaction(allsig_num[i], &sa, NULL);
        if (0 != rc) return bxierr_errno("Calling sigaction() failed for signum %d",
                                         allsig_num[i]);
        char * str = strsignal(allsig_num[i]);
        DEBUG(LOGGER,
              "Signal handler set for %d: %s", allsig_num[i], str);
        // Do not BXIFREE(str) since it is statically allocated as specified in the manual.
        // BXIFREE(str);
    }
    INFO(LOGGER, "Signal handlers set");
    return BXIERR_OK;
}

bxierr_p bxilog_sigset_new(sigset_t *sigset, int * signum, size_t n) {
    bxiassert(NULL != sigset && NULL != signum);
    bxierr_p err = BXIERR_OK, err2;
    int rc = sigemptyset(sigset);
    if (0 != rc) {
        err2 = bxierr_errno("Calling sigemptyset(%p) failed", sigset);
        BXIERR_CHAIN(err, err2);
    }
    for (size_t i = 0; i < n; i++) {
       rc = sigaddset(sigset, signum[i]);
       if (0 != rc) {
           err2 = bxierr_errno("Calling sigaddset() with signum='%d' failed", signum[i]);
           BXIERR_CHAIN(err, err2);
       }
    }
    return err;
}

char * bxilog_signal_str(const siginfo_t * siginfo,
                         const struct signalfd_siginfo * sfdinfo) {

    bxiassert(siginfo != NULL || sfdinfo != NULL);

    int signum = (NULL == siginfo) ? (int)sfdinfo->ssi_signo: siginfo->si_signo;
    char * sigstr = strsignal(signum);
    bxiassert(NULL != sigstr);
    int code = (NULL == siginfo) ? (int) sfdinfo->ssi_code: siginfo->si_code;
    pid_t pid = (NULL == siginfo) ? (pid_t) sfdinfo->ssi_pid : siginfo->si_pid;
    uid_t uid = (NULL == siginfo) ? (uid_t) sfdinfo->ssi_uid : siginfo->si_uid;

    switch(signum) {
    case SIGTERM:
    case SIGINT:
        if (SI_USER == code) {
            return bxistr_new("Signal=%d ('%s'), Sender PID:UID='%d:%u'",
                              signum, sigstr, pid, uid);
        }
        if (SI_KERNEL == code) {
            return bxistr_new("Signal=%d ('%s'), Sender=KERNEL",
                                        signum, sigstr);
        }
        return bxistr_new("Signal=%d ('%s'), Sender=Unknown",
                                    signum, sigstr);
    case SIGABRT:
    case SIGILL:
    case SIGFPE:
    case SIGSEGV:
    case SIGBUS:
        return bxistr_new("Signal=%d ('%s'), Signal Code=%d (man 2 sigaction)",
                          signum, sigstr, code);
    default:
        return bxistr_new("Signal=%d ('%s'), This should not happen!",
                          signum, sigstr);
    }
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************


// Handler of Signals (such as SIGSEGV, ...)
void _sig_handler(int signum, siginfo_t * siginfo, void * dummy) {
    (void) (dummy); // Unused, prevent warning == error at compilation time
    char * sigstr = bxilog_signal_str(siginfo, NULL);
#ifdef __linux__
    pid_t tid = (pid_t) syscall(SYS_gettid);

#else
    uint16_t tid = bxilog_get_thread_rank(pthread_self());
#endif
    /* Since this handler is established for more than one kind of signal,
       it might still get invoked recursively by delivery of some other kind
       of signal.  Use a static variable to keep track of that. */
    if (FATAL_ERROR_IN_PROGRESS) {
        error(signum, 0,
              "(%s#tid-%u) %s\n. Already handling a signal... Exiting",
              BXILOG__GLOBALS->config->progname, tid, sigstr);
    }
    FATAL_ERROR_IN_PROGRESS = 1;
    char * str;
    if (SIGINT == signum || SIGTERM == signum) {
        str = bxistr_new("%s: %s\n", BXILOG__GLOBALS->config->progname, sigstr);
    } else {
        char * trace = bxierr_backtrace_str();
        str = bxistr_new("%s: %s\n%s\n", BXILOG__GLOBALS->config->progname, sigstr, trace);
        BXIFREE(trace);
    }
    BXIFREE(sigstr);

    bxilog_rawprint(str, STDERR_FILENO);

    CRITICAL(LOGGER, "%s", str);
    BXIFREE(str);
    // Flush all logs before terminating -> ask handlers to stop.
    bxierr_p err = BXIERR_OK, err2;
    err2 = bxilog_flush();
    BXIERR_CHAIN(err, err2);
    err2 = bxilog__stop_handlers();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        char * err_str = bxierr_str(err);
        char * str = bxistr_new("Error while processing signal - %s\n",
                                err_str);
        bxilog_rawprint(str, STDERR_FILENO);
        BXIFREE(str);
        BXIFREE(err_str);
    }

    // Wait some time before exiting
    err2 = bxitime_sleep(CLOCK_MONOTONIC, 1, 0);
    BXIERR_CHAIN(err, err2);

    // Use default sighandler to end.
    struct sigaction dft_action;
    memset(&dft_action, 0, sizeof(struct sigaction));
    dft_action.sa_handler = SIG_DFL;
    errno = 0;
    int rc = sigaction(signum, &dft_action, NULL);
    if (-1 == rc) {
        error(128 + signum, errno, "Calling sigaction(%d, ...) failed", signum);
    }
    errno = 0;
    // Unblock all signals
    sigset_t mask;
    rc = sigfillset(&mask);
    bxiassert(0 == rc);
    rc = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    if (-1 == rc) bxilog_rawprint("Calling pthread_sigmask() failed\n", STDERR_FILENO);
    rc = pthread_kill(pthread_self(), signum);
    if (0 != rc) {
        error(128 + signum, errno, "Calling pthread_kill(self, %d) failed", signum);
    }
    _exit(128 + signum);
}
