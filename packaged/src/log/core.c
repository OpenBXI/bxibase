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

#include <unistd.h>
#include <error.h>
#include <sysexits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <libgen.h>
#include <fcntl.h>
#include <execinfo.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <sys/ioctl.h>

#include <zmq.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

#include "tsd.h"
#include "fork.h"
#include "cfg.h"
#include "tsd.h"
#include "handler.h"


#include "core.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************



//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************

static void _report_err(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                        char * file, size_t filelen,
                        const char * func, size_t funclen,
                        int line,
                        const char * fmt, va_list arglist);
static bxierr_p _send2handlers(const bxilog_p logger, const bxilog_level_e level,
                            void * log_channel,
#ifdef __linux__
                            pid_t tid,
#endif
                            uint16_t thread_rank,
                            char * filename, size_t filename_len,
                            const char * funcname, size_t funcname_len,
                            int line,
                            const char * rawstr, size_t rawstr_len);
static void _check_set_params(bxilog_param_p param);
static bxierr_p _reset_globals();
static bxierr_p _cleanup(void);

static bxierr_p _start_handler_thread(bxilog_handler_p handler,
                                      bxilog_handler_param_p param);
static bxierr_p _sync_handler(size_t);
static bxierr_p _join_handler(size_t handler_rank, bxierr_p *handler_err);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The internal logger
SET_LOGGER(LOGGER, "bxi.base.log.core");


static pthread_mutex_t BXILOG_INITIALIZED_MUTEX = PTHREAD_MUTEX_INITIALIZER;

/* Once-only initialisation of the pthread_atfork() */
static pthread_once_t ATFORK_ONCE = PTHREAD_ONCE_INIT;

// All fields will be initialized to their default values (0)
static bxilog__core_globals_s BXILOG__CORE_GLOBALS_S = {
                                                        .zmq_ctx = NULL,
                                                        .RECORD_MINIMUM_SIZE = sizeof(bxilog_record_s),
};

bxilog__core_globals_p BXILOG__GLOBALS = &BXILOG__CORE_GLOBALS_S;


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxierr_p bxilog_init(bxilog_param_p param) {
    bxierr_p err = BXIERR_OK;
    int rc = pthread_mutex_lock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        BXILOG__GLOBALS->state = BROKEN;
        return bxierr_fromidx(rc, NULL,
                              "Calling pthread_mutex_lock() failed (rc=%d)", rc);
    }
    // Ok, the user wants to retry...
    if (BROKEN == BXILOG__GLOBALS->state) {
        err = _cleanup();
        if (bxierr_isko(err)) return err; // Stay in the BROKEN state
        BXILOG__GLOBALS->state = UNSET;
    }

    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if(UNSET != BXILOG__GLOBALS->state && FINALIZED != BXILOG__GLOBALS->state) {
        err = bxierr_new(BXILOG_ILLEGAL_STATE_ERR,
                         NULL, NULL, NULL, NULL,
                         "Illegal state: %d",
                         BXILOG__GLOBALS->state);
        goto UNLOCK;
    }
    _check_set_params(param);

    BXILOG__GLOBALS->state = INITIALIZING;

    err = bxilog__core_init_globals();
    if (bxierr_isko(err)) {
        BXILOG__GLOBALS->state = BROKEN;
        goto UNLOCK;
    }

    err = bxilog__core_start_handlers();
    if (bxierr_isko(err)) {
        BXILOG__GLOBALS->state = BROKEN;
        goto UNLOCK;
    }

    // Install the fork handler once only
    rc = pthread_once(&ATFORK_ONCE, bxilog__fork_install_handlers);
    if (0 != rc) {
        err = bxierr_fromidx(rc, NULL,
                             "Calling pthread_once() failed (rc = %d)", rc);
        BXILOG__GLOBALS->state = BROKEN;
        goto UNLOCK;
    }

    /* now the log library is initialized */
    BXILOG__GLOBALS->state = INITIALIZED;
    DEBUG(LOGGER, "Initialization done");
UNLOCK:
    rc = pthread_mutex_unlock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        BXILOG__GLOBALS->state = ILLEGAL;
        bxierr_p err2 =  bxierr_fromidx(rc, NULL,
                                        "Calling pthread_mutex_unlock() failed (rc=%d)",
                                        rc);
        BXIERR_CHAIN(err, err2);
    }

    return err;
}

bxierr_p bxilog_finalize(bool flush) {
    bxierr_p err = BXIERR_OK;
    int rc = pthread_mutex_lock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        err = bxierr_fromidx(rc, NULL,
                             "Calling pthread_mutex_lock() failed (rc=%d)", rc);
        goto UNLOCK;
    }

    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (FINALIZED == BXILOG__GLOBALS->state || UNSET == BXILOG__GLOBALS->state) {
        err = BXIERR_OK;
        goto UNLOCK;
    }
    if (BXILOG__GLOBALS->state != INITIALIZED && BXILOG__GLOBALS->state != BROKEN) {
        err = bxierr_new(BXILOG_ILLEGAL_STATE_ERR,
                         NULL, NULL, NULL, NULL,
                         "Illegal state: %d", BXILOG__GLOBALS->state);
        goto UNLOCK;
    }
    DEBUG(LOGGER, "Exiting bxilog");
    bxierr_p err2;
    if (flush) {
        err2 = bxilog_flush();
        BXIERR_CHAIN(err, err2);
    }

    err2 = bxilog__core_finalize();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        BXILOG__GLOBALS->state = BROKEN;
        goto UNLOCK;
    }
    bxiassert(FINALIZING == BXILOG__GLOBALS->state);
    BXILOG__GLOBALS->state = FINALIZED;
UNLOCK:
    rc = pthread_mutex_unlock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        BXILOG__GLOBALS->state = ILLEGAL;
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_mutex_unlock() failed (rc=%d)",
                              rc);
        BXIERR_CHAIN(err, err2);
    }

    return err;
}

void bxilog_destroy(bxilog_p * self_p) {
    if (NULL == *self_p) return;
    if (!(*self_p)->allocated) return;

    BXIFREE((*self_p)->name);
    BXIFREE(*self_p);
}


bxierr_p bxilog_flush(void) {
    if (INITIALIZED != BXILOG__GLOBALS->state) return BXIERR_OK;
    DEBUG(LOGGER, "Requesting a flush()");
    tsd_p tsd = NULL;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;
    void * ctl_channel = tsd->ctrl_channel;
    bxierr_group_p errlist = bxierr_group_new();
    for (size_t i = 0; i < BXILOG__GLOBALS->param->handlers_nb; i++) {
        err = bxizmq_str_snd(FLUSH_CTRL_MSG_REQ, ctl_channel, 0, 0, 0);
        if (bxierr_isko(err)) {
            bxierr_list_append(errlist, err);
            continue;
        }
        char * reply;
        err = bxizmq_str_rcv(ctl_channel, 0, false, &reply);
        if (bxierr_isko(err)) bxierr_list_append(errlist, err);
        // Warning, no not introduce recursive call here (using ERROR() for example
        // or any log message: we are currently flushing!
        if (0 != strcmp(FLUSH_CTRL_MSG_REP, reply)) {
            bxierr_list_append(errlist,
                               bxierr_new(BXILOG_IHT2BC_PROTO_ERR,
                                          NULL, NULL, NULL, NULL,
                                          "Wrong message received in reply "
                                          "to %s: %s. Expecting: %s",
                                          FLUSH_CTRL_MSG_REQ, reply,
                                          FLUSH_CTRL_MSG_REP));
        }
        BXIFREE(reply);
    }
    if (errlist->errors_nb != 0) {
        return bxierr_from_group(BXILOG_FLUSH_ERR,
                                 errlist,
                                 "At least one error occured while "
                                 "flushing %zu handlers.",
                                 BXILOG__GLOBALS->param->handlers_nb);
    }
    DEBUG(LOGGER, "flush() done succesfully on all %zu handlers",
          BXILOG__GLOBALS->param->handlers_nb);

    bxierr_group_destroy(&errlist);
    return BXIERR_OK;
}

bxierr_p bxilog_log_rawstr(const bxilog_p logger, const bxilog_level_e level,
                           char * filename, size_t filename_len,
                           const char * funcname, size_t funcname_len,
                           int line,
                           const char * rawstr, size_t rawstr_len) {
    if (INITIALIZED != BXILOG__GLOBALS->state) return BXIERR_OK;
    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;

    err = _send2handlers(logger, level, tsd->data_channel,
#ifdef __linux__
                    tsd->tid,
#endif
                    tsd->thread_rank,
                    filename, filename_len,
                    funcname, funcname_len,
                    line,
                    rawstr, rawstr_len);
    return err;
}

bxierr_p bxilog_vlog_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                  char * filename, size_t filename_len,
                                  const char * funcname, size_t funcname_len,
                                  const int line,
                                  const char * fmt, va_list arglist) {

    if (INITIALIZED != BXILOG__GLOBALS->state) return BXIERR_OK;

    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;

    // Start creating the logmsg so the length can be computed
    // We start in the thread local buffer

    char * logmsg = tsd->log_buf;
    size_t logmsg_len = BXILOG__GLOBALS->param->tsd_log_buf_size;
    bool logmsg_allocated = false; // When true,  means that a new special buffer has been
                                   // allocated -> it will have to be freed
    while(true) {
        va_list arglist_copy;
        va_copy(arglist_copy, arglist);
        // Does not include the null terminated byte
        int n = vsnprintf(logmsg, logmsg_len, fmt, arglist_copy);
        va_end(arglist_copy);

        // Check error
        bxiassert(n >= 0);

        // Ok
        if ((size_t) n < logmsg_len) {
            logmsg_len = (size_t) n + 1; // Record the actual size of the message
            break;
        }

        if (logmsg_allocated) BXIFREE(logmsg);
        // Not enough space, mallocate a new special buffer of the precise size
        logmsg_len = (size_t) (n + 1);
        logmsg = malloc(logmsg_len); // Include the null terminated byte
        bxiassert(NULL != logmsg);
        logmsg_allocated = true;

        tsd->rsz_log_nb++;
    }

    tsd->max_log_size = tsd->max_log_size > logmsg_len ? tsd->max_log_size : logmsg_len;
    tsd->min_log_size = tsd->min_log_size < logmsg_len ? tsd->min_log_size : logmsg_len;
    tsd->sum_log_size += logmsg_len;
    tsd->log_nb++;

    err = _send2handlers(logger, level, tsd->data_channel,
#ifdef __linux__
                    tsd->tid,
#endif
                    tsd->thread_rank,
                    filename, filename_len,
                    funcname, funcname_len,
                    line,
                    logmsg, logmsg_len);

    if (logmsg_allocated) BXIFREE(logmsg);
    // Either record comes from the stack
    // or it comes from a mallocated region that will be freed
    // by bxizmq_snd_data_zc itself
    // Therefore it should not be freed here
    // FREE(record);
    return err;
}

bxierr_p bxilog_log_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                 char * filename, size_t filename_len,
                                 const char * funcname, size_t funcname_len,
                                 const int line,
                                 const char * fmt, ...) {
    va_list ap;
    bxierr_p err;

    va_start(ap, fmt);
    err = bxilog_vlog_nolevelcheck(logger, level,
                                   filename, filename_len, funcname, funcname_len, line,
                                   fmt, ap);
    va_end(ap);

    return err;
}


bxilog_level_e bxilog_get_level(const bxilog_p logger) {
    bxiassert(NULL != logger);
    return logger->level;
}

void bxilog_set_level(const bxilog_p logger, const bxilog_level_e level) {
    bxiassert(NULL != logger);
    bxiassert(BXILOG_LOWEST >= level);
    logger->level = level;
}

// Defined inline in bxilog.h
extern bool bxilog_is_enabled_for(const bxilog_p logger, const bxilog_level_e level);

void bxilog_exit(int exit_code,
                 bxierr_p err,
                 bxilog_p logger,
                 bxilog_level_e level,
                 char * file,
                 size_t filelen,
                 const char * func,
                 size_t funclen,
                 int line) {

    bxiassert(NULL != logger);
    char * str = bxierr_str_limit(err, BXIERR_ALL_CAUSES);
    bxilog_log(logger, level,
               file, filelen,
               func, funclen,
               line,
               "Exiting with code %d, Error is: %s", exit_code, str);
    bxierr_destroy(&err);
    err = bxitime_sleep(CLOCK_MONOTONIC, 0, 50000000);
    bxierr_destroy(&err);
    err = bxilog_flush();
    bxierr_destroy(&err);
    exit((exit_code));
}

void bxilog_assert(bxilog_p logger, bool result,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line, char * expr) {
    if (!result) {
        bxierr_p err = bxierr_new(BXIASSERT_CODE,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  "From file %s:%d: assertion %s is false"\
                                  BXIBUG_STD_MSG,
                                  file, line, expr);
        bxilog_exit(EX_SOFTWARE, err, logger, BXILOG_CRITICAL,
                    file, filelen, func, funclen, line);
        bxierr_destroy(&err);
    }
}

void bxilog_report(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line,
                   const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _report_err(logger, level,
                err, file, filelen, func, funclen, line, fmt, ap);
    va_end(ap);
    bxierr_destroy(err);
    *err = BXIERR_OK;
}

void bxilog_report_keep(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                        char * file, size_t filelen,
                        const char * func, size_t funclen,
                        int line,
                        const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _report_err(logger, level,
                err, file, filelen, func, funclen, line, fmt, ap);
    va_end(ap);
}

bxierr_p bxilog_set_thread_rank(uint16_t rank) {
    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;
    tsd->thread_rank = rank;
    return BXIERR_OK;
}

bxierr_p bxilog_get_thread_rank(uint16_t * rank_p) {
    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;
    *rank_p = tsd->thread_rank;
    return BXIERR_OK;
}

void bxilog_display_loggers() {
    char ** level_names;
    size_t n = bxilog_get_all_level_names(&level_names);
    fprintf(stderr, "Log level names:\n");
    while(n-- > 0) {
        fprintf(stderr, "\t%zu\t = %s\n", n, level_names[n]);
    }
    bxilog_p * loggers;
    n = bxilog_get_registered(&loggers);
    fprintf(stderr, "Loggers name:\n");
    while(n-- > 0) {
        fprintf(stderr, "\t%s\n", loggers[n]->name);
    }
    BXIFREE(loggers);
}


void bxilog__core_wipeout() {
    // Remove allocated configuration
    bxilog_reset_config();

    // Remove allocated loggers
    bxilog__cfg_release_loggers();

    // Remove mallocated signal stack
    stack_t sigstack;
    errno = 0;
    int rc = sigaltstack(NULL, &sigstack);
    bxiassert(0 == rc);
    if (SS_ONSTACK == sigstack.ss_flags && NULL != sigstack.ss_sp) {
//        fprintf(stderr, "[I] Removing sigstack\n");
        BXIFREE(sigstack.ss_sp);
    }
}


bxierr_p bxilog__core_init_globals() {

    BXILOG__GLOBALS->pid = getpid();
    pthread_t * threads = bximem_calloc(BXILOG__GLOBALS->param->handlers_nb * sizeof(*threads));
    BXILOG__GLOBALS->internal_handlers = threads;
    BXILOG__GLOBALS->internal_handlers_nb = 0;

    bxiassert(NULL == BXILOG__GLOBALS->zmq_ctx);
    errno = 0;

    void * ctx = zmq_ctx_new();
    if (NULL == ctx) {
        BXILOG__GLOBALS->state = ILLEGAL;
        return bxierr_errno("Can't create a zmq context");
    }
    errno = 0;
    int rc;

    // Since we normally use inproc://, there is no need for ZMQ_IO_THREADS
    rc = zmq_ctx_set(ctx, ZMQ_IO_THREADS, 0);
    if (0 != rc) {
        return bxierr_errno("Calling zmq_ctx_set() failed");
    }

    BXILOG__GLOBALS->zmq_ctx = ctx;

    rc = pthread_once(&BXILOG__GLOBALS->tsd_key_once, bxilog__tsd_key_new);
    if (0 != rc) {
        BXILOG__GLOBALS->state = ILLEGAL;
        return bxierr_fromidx(rc, NULL,
                              "Calling pthread_once() failed (rc=%d)", rc);
    }

    return BXIERR_OK;
}

bxierr_p bxilog__core_start_handlers(void) {
    bxiassert(INITIALIZING == BXILOG__GLOBALS->state);
    bxierr_p err = BXIERR_OK;
    bxierr_group_p errlist = bxierr_group_new();

    // Starting handlers
    for (size_t i = 0; i < BXILOG__GLOBALS->param->handlers_nb; i++) {
        bxilog_handler_p handler = BXILOG__GLOBALS->param->handlers[i];
        if (NULL == handler) {
            bxierr_p ierr = bxierr_gen("Handler %zu is NULL!", i);
            bxierr_list_append(errlist, ierr);
            continue;
        }
        BXILOG__GLOBALS->param->handlers_params[i]->zmq_context = BXILOG__GLOBALS->zmq_ctx;
        bxierr_p ierr = BXIERR_OK, ierr2;

        ierr2 = _start_handler_thread(handler,
                                      BXILOG__GLOBALS->param->handlers_params[i]);
        BXIERR_CHAIN(ierr, ierr2);

        if (bxierr_isko(ierr)) bxierr_list_append(errlist, ierr);
    }

    // Synchronizing handlers
    for (size_t i = 0; i < BXILOG__GLOBALS->param->handlers_nb; i++) {
        bxilog_handler_p handler = BXILOG__GLOBALS->param->handlers[i];
        if (NULL == handler) {
            bxierr_p ierr = bxierr_gen("Handler %zu is NULL!", i);
            bxierr_list_append(errlist, ierr);
            continue;
        }
        bxierr_p ierr = _sync_handler(i);
        if (bxierr_isko(ierr)) bxierr_list_append(errlist, ierr);
    }

    if (0 < errlist->errors_nb) {
        err = bxierr_from_group(BXIERR_GROUP_CODE,
                                errlist,
                                "Starting bxilog handlers yield %zu errors",
                                errlist->errors_nb);
    } else {
        bxierr_group_destroy(&errlist);
    }

    return err;
}


bxierr_p bxilog__core_finalize(void) {
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (INITIALIZED != BXILOG__GLOBALS->state && BROKEN != BXILOG__GLOBALS->state) {
        bxierr_p err = bxierr_new(BXILOG_ILLEGAL_STATE_ERR, NULL, NULL, NULL, NULL,
                                  "Illegal state: %d", BXILOG__GLOBALS->state);
        BXILOG__GLOBALS->state = ILLEGAL;
        return err;
    }
    BXILOG__GLOBALS->state = FINALIZING;
    bxierr_p err = BXIERR_OK, err2;
    err2 = bxilog__core_stop_handlers();
    BXIERR_CHAIN(err, err2);

    err2 = _cleanup();
    BXIERR_CHAIN(err, err2);
    return err;
}

bxierr_p bxilog__core_stop_handlers(void) {
    // TODO: hack around zeromq issue
    // https://github.com/zeromq/libzmq/issues/1590
    bxierr_p err = BXIERR_OK, err2;
    bxierr_group_p errlist = bxierr_group_new();

    for (size_t i = 0; i < BXILOG__GLOBALS->param->handlers_nb; i++) {

        int ret = pthread_kill(BXILOG__GLOBALS->internal_handlers[i], 0);
        if (ESRCH == ret) continue;

        char * url = BXILOG__GLOBALS->param->handlers_params[i]->ctrl_url;
        bxiassert(NULL != url);

        void * zocket = NULL;

        err2 = bxizmq_zocket_connect(BXILOG__GLOBALS->zmq_ctx,
                                     ZMQ_REQ,
                                     url,
                                     &zocket);
        BXIERR_CHAIN(err, err2);
        bxierr_abort_ifko(err);

        err2 = bxizmq_str_snd(EXIT_CTRL_MSG_REQ, zocket, 0, 0, 0);
        BXIERR_CHAIN(err, err2);

        bxierr_p handler_err;
        err2 = _join_handler(i, &handler_err);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_zocket_destroy(zocket);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(handler_err)) bxierr_list_append(errlist, handler_err);
        if (bxierr_isko(err)) bxierr_list_append(errlist, err);
    }
    if (0 < errlist->errors_nb) {
        err2 = bxierr_from_group(BXIERR_GROUP_CODE, errlist,
                                 "Some errors occurred in at least"
                                 " one of %zu internal handlers.",
                                 BXILOG__GLOBALS->internal_handlers_nb);
        BXIERR_CHAIN(err, err2);
    } else {
        bxierr_group_destroy(&errlist);
    }

    return err;

//    errno = 0;
//    tsd_p tsd;
//    bxierr_p err = BXIERR_OK, err2;
//
//    err2 = bxilog__tsd_get(&tsd);
//    BXIERR_CHAIN(err, err2);
//    if (bxierr_isko(err)) return err;
//
//    bxierr_group_p errlist = bxierr_group_new();
//
//    size_t handlers_nb = BXILOG__GLOBALS->param->handlers_nb;
//
//    for (size_t i = 0; i < handlers_nb; i++) {
//        bxierr_p herr = BXIERR_OK, herr2;
//        fprintf(stderr, "Sending EXIT\n");
//        herr2 = bxizmq_str_snd(EXIT_CTRL_MSG_REQ, tsd->ctrl_channel, 0, 0, 0);
//        BXIERR_CHAIN(herr, herr2);
//
//        bxierr_p handler_err;
//        herr2 = _join_handler(i, &handler_err);
//        BXIERR_CHAIN(herr, herr2);
//
//        if (bxierr_isko(handler_err)) bxierr_list_append(errlist, handler_err);
//        if (bxierr_isko(herr)) bxierr_list_append(errlist, herr);
//    }
//
//    if (0 < errlist->errors_nb) {
//        err2 = bxierr_from_group(BXIERR_GROUP_CODE, errlist,
//                                 "Some errors occurred in at least"
//                                 " one of %zu internal handlers.",
//                                 BXILOG__GLOBALS->internal_handlers_nb);
//        BXIERR_CHAIN(err, err2);
//    } else {
//        bxierr_group_destroy(&errlist);
//    }
//
//    return err;
}


void bxilog__core_display_err_msg(char* msg) {
    // TODO: use a better format?
    ssize_t n = write(STDERR_FILENO, msg, strlen(msg) + 1);
    // Just don't care on error, the end-user might have redirected
    // the stderr stream to a non-writable place (such as a broken pipe after a
    // command such as 'cmd 2>&1 |head')...
    // bxiassert(n > 0);
    UNUSED(n);
    const char details_msg[] = "\nSee bxilog for details.\n";
    n = write(STDERR_FILENO, details_msg, ARRAYLEN(details_msg));
    UNUSED(n);
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

void _report_err(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                 char * file, size_t filelen,
                 const char * func, size_t funclen,
                 int line,
                 const char * fmt, va_list arglist) {

    if (*err == NULL) return;
    if (bxierr_isok(*err)) return;
    if (!bxilog_is_enabled_for(logger, level)) return ;

    char * msg;
    bxistr_vnew(&msg, fmt, arglist);
    char * err_str = bxierr_str(*err);;
    bxierr_p logerr = bxilog_log_nolevelcheck(logger, level,
                                              file, filelen,
                                              func, funclen, line,
                                              "%s: %s", msg, err_str);
    BXIFREE(msg);
    BXIFREE(err_str);
    bxierr_report(logerr, STDERR_FILENO);
}

bxierr_p _send2handlers(const bxilog_p logger,
                        const bxilog_level_e level,
                        void * log_channel,
#ifdef __linux__
                        pid_t tid,
#endif
                        uint16_t thread_rank,
                        char * filename, size_t filename_len,
                        const char * funcname, size_t funcname_len,
                        int line,
                        const char * rawstr, size_t rawstr_len) {

    bxilog_record_p record;
    char * data;

    size_t var_len = filename_len\
            + funcname_len\
            + logger->name_length;

    size_t data_len = sizeof(*record) + var_len + rawstr_len;

    // We need a mallocated buffer to prevent ZMQ from making its own copy
    // We use malloc() instead of calloc() for performance reason
    record = malloc(data_len);
    bxiassert(NULL != record);
    // Fill the buffer
    record->level = level;
    bxierr_p err = bxitime_get(CLOCK_REALTIME, &record->detail_time);
    // TODO: be smarter here, if the time can't be fetched, just fake it!
    if (bxierr_isko(err)) {
        BXIFREE(record);
        return err;
    }
    record->pid = BXILOG__GLOBALS->pid;
#ifdef __linux__
    record->tid = tid;
#endif
    record->thread_rank = thread_rank;
    record->line_nb = line;
    record->filename_len = filename_len;
    record->funcname_len = funcname_len;
    record->logname_len = logger->name_length;
    record->variable_len = var_len;
    record->logmsg_len = rawstr_len;

    // Now copy the rest after the record
    data = (char *) record + sizeof(*record);
    memcpy(data, filename, filename_len);
    data += filename_len;
    memcpy(data, funcname, funcname_len);
    data += funcname_len;
    memcpy(data, logger->name, logger->name_length);
    data += logger->name_length;
    memcpy(data, rawstr, rawstr_len);

    // Send the frame
    // normal version if record comes from the stack 'buf'
    //    size_t retries = bxizmq_snd_data(record, data_len,
    //                                      _get_tsd()->zocket, ZMQ_DONTWAIT,
    //                                      RETRIES_MAX, RETRY_DELAY);

    // Zero-copy version (if record has been mallocated).
    err = bxizmq_data_snd_zc(record, data_len,
                             log_channel, ZMQ_DONTWAIT,
                             RETRIES_MAX, RETRY_DELAY,
                             bxizmq_data_free, NULL);
    if (bxierr_isko(err)) {
        if (BXIZMQ_RETRIES_MAX_ERR != err->code) return err;
        // Recursive call!
        WARNING(LOGGER,
                "Sending last log required %lu retries", (long) err->data);
        bxierr_destroy(&err);
    }
    return BXIERR_OK;
}

bxierr_p _reset_globals() {
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    if (NULL != BXILOG__GLOBALS->zmq_ctx) {
        int rc = zmq_ctx_destroy(BXILOG__GLOBALS->zmq_ctx);
        if (-1 == rc) {
            while (-1 == rc && EINTR == errno) {
                rc = zmq_ctx_destroy(BXILOG__GLOBALS->zmq_ctx);
            }
            if (-1 == rc) {
                err2 = bxierr_errno("Can't destroy context (rc=%d)", rc);
                BXIERR_CHAIN(err, err2);
            }
        }
    }
    BXILOG__GLOBALS->zmq_ctx = NULL;
    pthread_key_delete(BXILOG__GLOBALS->tsd_key);
    // Nothing to do on pthread_key_delete() see man page
    BXILOG__GLOBALS->tsd_key_once = PTHREAD_ONCE_INIT;
    BXIFREE(BXILOG__GLOBALS->internal_handlers);
    BXILOG__GLOBALS->internal_handlers_nb = 0;

    return err;
}

bxierr_p _cleanup(void) {
    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (tsd != NULL) {
        bxilog__tsd_free(tsd);
    }
    if (bxierr_isko(err)) return err;

    return _reset_globals();
}

void _check_set_params(bxilog_param_p param) {
    bxiassert(NULL != param->progname);
    bxiassert(0 < param->tsd_log_buf_size);

    BXILOG__GLOBALS->param = param;
}


bxierr_p _start_handler_thread(bxilog_handler_p handler, bxilog_handler_param_p param) {
    bxierr_p err = BXIERR_OK, err2;
    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }
    rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    bxiassert(rc == 0);

    bxilog__handler_thread_bundle_p bundle = bximem_calloc(sizeof(*bundle));
    bundle->handler = handler;
    bundle->param = param;

    pthread_t thread;
    rc = pthread_create(&thread, &attr,
                        (void* (*) (void*)) bxilog__handler_start,
                        bundle);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    } else {
        BXILOG__GLOBALS->internal_handlers[BXILOG__GLOBALS->internal_handlers_nb] = thread;
        BXILOG__GLOBALS->internal_handlers_nb++;
    }
    return err;
}

bxierr_p _sync_handler(size_t handler_rank) {
    tsd_p tsd;
    bxierr_p fatal_err = bxilog__tsd_get(&tsd);
    bxierr_abort_ifko(fatal_err);

//    ENVOI DANS LA DEMANDE DE SYNCHRO LE RANG DU HANDLER
//    LA RÉPONSE DOIT CONTENIR CE RANG
//    SI CE N'EST PAS LE CAS, C_EST QUE LE HANDLER CORRESPONDANT
//    N'EST PAS ENCORE PRÊT, IL FAUT DONC RECOMMENCER À ENVOYER JUSQU'À
//    L'OBTENTION DE LA BONNE RÉPONSE'.
//    LE HANDLER REÇOIT UNE DEMANDE AVEC UN RANG ET RÉPOND UN MESSAGE D'ERREUR
//    LORSQUE CE N'EST PAS LE SIEN, (AVEC SON RANG) CE QUI PERMETTRA AU BC DE
//    SAVOIR QUOI FAIRE.
//
//    MIEUX: ON DISPOSE D'UNE FONCTION DE DEMANDE DE STATUS (PLUTOT QUE DE SYNC).
//    LA DEMANDE INCLUS UN RANG.
//    LA REPONSE INCLUS AUSSI UN RANG. SI LA RÉPONSE EST 'OK/READY...', TANT MIEUX,
//    ON PASSE AU SUIVANT. SINON, ON DISPOSE D'UNE ERREUR.
//    LE HANDLER NE DISPOSE PLUS D'UNE FONCTION DE SYNCHRO, IL RENVOI SIMPLEMENT
//    SON STATUS NORMALEMENT. IL FAUT QU'IL PUISSE RÉPONDRE À PLUSIEURS SIMULTANEMENT.



    bxierr_p err = BXIERR_OK, err2;
    size_t retry = 1000;
    while(true) {
        err2 = bxizmq_str_snd(READY_CTRL_MSG_REQ, tsd->ctrl_channel, ZMQ_DONTWAIT, 0, 0);
        if (bxierr_isok(err2)) break;
        if (EAGAIN != err2->code) break;
        if (retry == 0) break;
        bxierr_destroy(&err2);
        err2  = bxitime_sleep(CLOCK_MONOTONIC_COARSE, 0, 1000);
        bxierr_report(err2, STDERR_FILENO);
        retry--;
    }
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) return err;

    char * msg;
    err2 = bxizmq_str_rcv(tsd->ctrl_channel, 0, 0, &msg);
    BXIERR_CHAIN(err, err2);

    if (0 != strncmp(READY_CTRL_MSG_REP, msg, ARRAYLEN(READY_CTRL_MSG_REP))) {
        // Ok, the handler sends us an error msg.
        // We expect it to die and the actual error will be returned
        bxierr_p handler_err;
        fatal_err = _join_handler(handler_rank, &handler_err);
        bxierr_abort_ifko(fatal_err);

        bxiassert(bxierr_isko(handler_err));
        err2 = handler_err;
        BXIERR_CHAIN(err, err2);
    }
    BXIFREE(msg);

    return err;
}


bxierr_p _join_handler(size_t handler_rank, bxierr_p *handler_err) {
    int rc = pthread_join(BXILOG__GLOBALS->internal_handlers[handler_rank],
                          (void**) handler_err);

    if (0 != rc) return bxierr_fromidx(rc, NULL,
                                       "Can't join handler thread. "
                                       "Calling pthread_join() failed (rc=%d)", rc);
    return BXIERR_OK;
}
