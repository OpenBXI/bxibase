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

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include <sys/ioctl.h>

#include <zmq.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

#include "log/tsd_impl.h"
#include "log/config_impl.h"
#include "log/handler_impl.h"
#include "log/fork_impl.h"
#include "log/registry_impl.h"

#include "log/log_impl.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

// How many retries before falling back to SYNC sending.
#define RETRIES_MAX 3u
// Used when a sending failed with EAGAIN, we sleep for that amount of time (in ns).
#define RETRY_DELAY 500000l

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static void _check_set_params(bxilog_config_p config);
static bxierr_p _reset_globals();
static bxierr_p _cleanup(void);

static bxierr_p _start_handler_thread(bxilog_handler_p handler,
                                      bxilog_handler_param_p param);
static bxierr_p _sync_handler();
static bxierr_p _join_handler(size_t handler_rank, bxierr_p *handler_err);
static void _setprocname();
static bxierr_p _zmq_str_rcv_timeout(void * zocket, char ** reply, long timeout);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

const bxilog_const_s bxilog_const = {
                                     .LIB_PREFIX = BXILOG_LIB_PREFIX,
                                     .HB_PREFIX = BXILOG_HB_PREFIX,
};

// The internal logger
SET_LOGGER(LOGGER, BXILOG_LIB_PREFIX "bxilog");

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

bxierr_p bxilog_init(bxilog_config_p config) {
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
        if (bxierr_isko(err)) goto UNLOCK; // Stay in the BROKEN state
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
    _check_set_params(config);

    BXILOG__GLOBALS->state = INITIALIZING;

    _setprocname(BXILOG__GLOBALS->config->progname);

    err = bxilog__init_globals();
    if (bxierr_isko(err)) {
        BXILOG__GLOBALS->state = BROKEN;
        goto UNLOCK;
    }

    err = bxilog__start_handlers();
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

bxierr_p bxilog_finalize() {
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

    err = bxilog__finalize();

    if (bxierr_isko(err)) {
        BXILOG__GLOBALS->state = BROKEN;
        goto UNLOCK;
    }
    bxiassert(FINALIZING == BXILOG__GLOBALS->state);

    err = bxilog__config_destroy(&BXILOG__GLOBALS->config);

    if (bxierr_isko(err)) {
        BXILOG__GLOBALS->state = BROKEN;
        goto UNLOCK;
    }
    BXILOG__GLOBALS->state = FINALIZED;

UNLOCK:
    rc = pthread_mutex_unlock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        BXILOG__GLOBALS->state = ILLEGAL;
        bxierr_p tmp = bxierr_errno("Calling pthread_mutex_unlock() failed (rc=%d)", rc);
        bxierr_report(&tmp, STDERR_FILENO);
    }

    return err;
}

bool bxilog_is_ready() {
    bool result = false;
    int rc = pthread_mutex_lock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        bxierr_p tmp = bxierr_errno("Calling pthread_mutex_lock() failed (rc=%d)", rc);
        bxierr_report(&tmp, STDERR_FILENO);
    }
    result = INITIALIZED == BXILOG__GLOBALS->state;
    rc = pthread_mutex_unlock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        bxierr_p tmp = bxierr_errno("Calling pthread_mutex_unlock() failed (rc=%d)", rc);
        bxierr_report(&tmp, STDERR_FILENO);
    }
    return result;
}

bxierr_p bxilog_flush(void) {
    if (INITIALIZED != BXILOG__GLOBALS->state) return BXIERR_OK;
    FINE(LOGGER, "Requesting a flush()");
    tsd_p tsd = NULL;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;
    void * ctl_channel = tsd->ctrl_channel;
    bxierr_list_p errlist = bxierr_list_new();
    for (size_t i = 0; i < BXILOG__GLOBALS->config->handlers_nb; i++) {

        int ret = pthread_kill(BXILOG__GLOBALS->handlers_threads[i], 0);
        if (ESRCH == ret) continue;

        err = bxizmq_str_snd(FLUSH_CTRL_MSG_REQ, ctl_channel, 0, 0, 0);
        if (bxierr_isko(err)) {
            bxierr_list_append(errlist, err);
            continue;
        }
        char * reply = NULL;
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
        return bxierr_from_list(BXILOG_FLUSH_ERR,
                                 errlist,
                                 "At least one error occured while "
                                 "flushing %zu handlers.",
                                 BXILOG__GLOBALS->config->handlers_nb);
    }
    FINE(LOGGER, "flush() done succesfully on all %zu handlers",
         BXILOG__GLOBALS->config->handlers_nb);

    bxierr_list_destroy(&errlist);
    return BXIERR_OK;
}


void bxilog_display_loggers(int fd) {
    char ** level_names;
    ssize_t rc;
    size_t n = bxilog_get_all_level_names(&level_names);
    char * TMP = "Log level names:\n";
    rc = write(fd, TMP, strlen(TMP));
    bxiassert(-1 != rc);
    while(n-- > 0) {
        TMP = bxistr_new("\t%zu\t = %s\n", n, level_names[n]);
        rc = write(fd, TMP, strlen(TMP));
        bxiassert(-1 != rc);
        BXIFREE(TMP);
    }
    bxilog_logger_p * loggers;
    n = bxilog_registry_getall(&loggers);
    TMP = "Loggers name:\n";
    rc = write(fd, TMP, strlen(TMP));
    bxiassert(-1 != rc);
    while(n-- > 0) {
        TMP = bxistr_new("\t%s\n", loggers[n]->name);
        rc = write(fd, TMP, strlen(TMP));
        bxiassert(-1 != rc);
        BXIFREE(TMP);
    }

    BXIFREE(loggers);
}


void bxilog__wipeout() {
    // Remove allocated configuration
    bxilog_registry_reset();

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


bxierr_p bxilog__init_globals() {
    BXILOG__GLOBALS->pid = getpid();
    pthread_t * threads = bximem_calloc(BXILOG__GLOBALS->config->handlers_nb * sizeof(*threads));
    BXILOG__GLOBALS->handlers_threads = threads;
    BXILOG__GLOBALS->internal_handlers_nb = 0;

    bxiassert(NULL == BXILOG__GLOBALS->zmq_ctx);

    void * ctx = NULL;
    bxierr_p err = bxizmq_context_new(&ctx);
    if (bxierr_isko(err)) {
        BXILOG__GLOBALS->state = ILLEGAL;
        return err;
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

bxierr_p bxilog__start_handlers(void) {
    bxiassert(INITIALIZING == BXILOG__GLOBALS->state);
    bxierr_p err = BXIERR_OK;
    bxierr_list_p errlist = bxierr_list_new();

    // Starting handlers
    for (size_t i = 0; i < BXILOG__GLOBALS->config->handlers_nb; i++) {
        bxilog_handler_p handler = BXILOG__GLOBALS->config->handlers[i];
        if (NULL == handler) {
            bxierr_p ierr = bxierr_gen("Handler %zu is NULL!", i);
            bxierr_list_append(errlist, ierr);
            continue;
        }
        bxierr_p ierr = BXIERR_OK, ierr2;

        ierr2 = _start_handler_thread(handler,
                                      BXILOG__GLOBALS->config->handlers_params[i]);
        BXIERR_CHAIN(ierr, ierr2);

        if (bxierr_isko(ierr)) bxierr_list_append(errlist, ierr);
    }

    // Synchronizing handlers
    for (size_t i = 0; i < BXILOG__GLOBALS->config->handlers_nb; i++) {
        bxilog_handler_p handler = BXILOG__GLOBALS->config->handlers[i];
        if (NULL == handler) {
            bxierr_p ierr = bxierr_gen("Handler %zu is NULL!", i);
            bxierr_list_append(errlist, ierr);
            continue;
        }
        bxierr_p ierr = _sync_handler();
        if (bxierr_isko(ierr)) bxierr_list_append(errlist, ierr);
    }

    if (0 < errlist->errors_nb) {
        err = bxierr_from_list(BXIERR_GROUP_CODE,
                                errlist,
                                "Starting bxilog handlers yield %zu errors",
                                errlist->errors_nb);
    } else {
        bxierr_list_destroy(&errlist);
    }

    return err;
}


bxierr_p bxilog__finalize(void) {
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
    err2 = bxilog__stop_handlers();
    BXIERR_CHAIN(err, err2);

    err2 = _cleanup();
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p bxilog__stop_handlers(void) {
    // TODO: hack around zeromq issue
    // https://github.com/zeromq/libzmq/issues/1590
    bxierr_p err = BXIERR_OK, err2;
    bxierr_list_p errlist = bxierr_list_new();

    for (size_t i = 0; i < BXILOG__GLOBALS->config->handlers_nb; i++) {
        pthread_t handler_thread = BXILOG__GLOBALS->handlers_threads[i];
        int ret = pthread_kill(handler_thread, 0);

        if (ESRCH == ret) continue;

        char * url = BXILOG__GLOBALS->config->handlers_params[i]->ctrl_url;
        bxiassert(NULL != url);

        void * zocket = NULL;
        err2 = bxizmq_zocket_create_connected(BXILOG__GLOBALS->zmq_ctx,
                                              ZMQ_REQ,
                                              url,
                                              &zocket);
        BXIERR_CHAIN(err, err2);
        bxierr_abort_ifko(err);

        err2 = bxizmq_str_snd(EXIT_CTRL_MSG_REQ, zocket, 0, 0, 0);
        BXIERR_CHAIN(err, err2);

        char * msg = NULL;
        // FIXME: this _zmq_str_rcv_timeout() looks strange to me??
        // We might be able to replace it by a normal call to the bxizmq library.
        // For the moment, it seems to work though
        while (ret != ESRCH && msg == NULL) {
            bxierr_destroy(&err2);
            ret = pthread_kill(handler_thread, 0);
            err2 = _zmq_str_rcv_timeout(zocket, &msg, 500);
            if (ret == ESRCH && msg == NULL) {
                BXIERR_CHAIN(err, err2);
                break;
            }
            if (bxierr_isko(err2)) {
                bxierr_destroy(&err2);
            } else {
                break;
            }
        }
        
        err2 = bxizmq_zocket_destroy(zocket);
        BXIERR_CHAIN(err, err2);

        if (ESRCH == ret && msg == NULL) continue;

        if (NULL == msg || 0 != strncmp(EXIT_CTRL_MSG_REP, msg, ARRAYLEN(EXIT_CTRL_MSG_REP) - 1)) {
            // We just notify the end user there is a minor problem, but
            // returning such an error is useless at this point, we want to exit.
            bxierr_p tmp = bxierr_new(BXIZMQ_PROTOCOL_ERR, NULL, NULL, NULL, NULL,
                                      "Wrong message received. Expected: %s, received: %s",
                                      EXIT_CTRL_MSG_REP, msg);
            bxierr_report(&tmp, STDERR_FILENO);
            BXIFREE(msg);
            continue;
        }
        BXIFREE(msg);

        bxierr_p handler_err;
        err2 = _join_handler(i, &handler_err);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(handler_err)) bxierr_list_append(errlist, handler_err);
        if (bxierr_isko(err)) bxierr_list_append(errlist, err);
    }

    if (0 < errlist->errors_nb) {
        err2 = bxierr_from_list(BXIERR_GROUP_CODE, errlist,
                                 "Some errors occurred in at least"
                                 " one of %zu internal handlers.",
                                 BXILOG__GLOBALS->internal_handlers_nb);
        BXIERR_CHAIN(err, err2);
    } else {
        bxierr_list_destroy(&errlist);
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


void bxilog_rawprint(char* msg, int fd) {
    // TODO: use a better format?
    ssize_t n = write(fd, msg, strlen(msg));
    // Just don't care on error, the end-user might have redirected
    // the stderr stream to a non-writable place (such as a broken pipe after a
    // command such as 'cmd 2>&1 |head')...
    // bxiassert(n > 0);
    UNUSED(n);
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _reset_globals() {
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    if (NULL != BXILOG__GLOBALS->zmq_ctx) {
        err2 = bxizmq_context_destroy(&BXILOG__GLOBALS->zmq_ctx);
        BXIERR_CHAIN(err, err2);
    }
    int rc = pthread_key_delete(BXILOG__GLOBALS->tsd_key);
    UNUSED(rc); // Nothing to do on pthread_key_delete() see man page
    BXILOG__GLOBALS->tsd_key_once = PTHREAD_ONCE_INIT;
    BXIFREE(BXILOG__GLOBALS->handlers_threads);
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

void _check_set_params(bxilog_config_p config) {
    bxiassert(NULL != config->progname);
    bxiassert(0 < config->tsd_log_buf_size);

    BXILOG__GLOBALS->config = config;
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
    param->rank = BXILOG__GLOBALS->internal_handlers_nb;
    param->status = BXI_LOG_HANDLER_NOT_READY;
    bundle->param = param;

    pthread_t thread;
    rc = pthread_create(&thread, &attr,
                        (void* (*) (void*)) bxilog__handler_start,
                        bundle);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
        param->rank = 0;
    } else {
        BXILOG__GLOBALS->handlers_threads[BXILOG__GLOBALS->internal_handlers_nb] = thread;
        BXILOG__GLOBALS->internal_handlers_nb++;
    }
    return err;
}

bxierr_p _sync_handler() {
    tsd_p tsd;
    bxierr_p fatal_err = bxilog__tsd_get(&tsd);
    bxierr_abort_ifko(fatal_err);

    bxierr_p err = BXIERR_OK, err2;

    // We send only 1 message per handler but we don't know
    // which one will get it. The one who replies will be synced.
    err2 = bxizmq_str_snd(READY_CTRL_MSG_REQ, tsd->ctrl_channel,
                          ZMQ_DONTWAIT, 1000, 1e3); // Retry 1000, wait 1 µs max
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) return err;

    char * msg;
    err2 = bxizmq_str_rcv(tsd->ctrl_channel, 0, false, &msg);
    BXIERR_CHAIN(err, err2);
    // We always expect the rank to be sent!
    size_t *rank = NULL;
    size_t received_size;
    err2 = bxizmq_data_rcv((void**)&rank, sizeof(*rank), tsd->ctrl_channel,
                           0, true, &received_size);
    BXIERR_CHAIN(err, err2);

    if (0 != strncmp(READY_CTRL_MSG_REP, msg, ARRAYLEN(READY_CTRL_MSG_REP))) {
        // Ok, the handler sends us an error msg.
        // We expect the handler to display its own error message so we can free it
        BXILOG__GLOBALS->config->handlers_params[*rank]->status = BXI_LOG_HANDLER_ERROR;
        // We expect it to die and the actual error will be returned
        bxierr_p handler_err;
        fatal_err = _join_handler(*rank, &handler_err);
        bxierr_abort_ifko(fatal_err);

        bxiassert(bxierr_isko(handler_err));
        err2 = handler_err;
        BXIERR_CHAIN(err, err2);
    } else {
        BXILOG__GLOBALS->config->handlers_params[*rank]->status = BXI_LOG_HANDLER_READY;
    }
    BXIFREE(msg);
    BXIFREE(rank);

    return err;
}


bxierr_p _join_handler(size_t handler_rank, bxierr_p *handler_err) {
    int rc = pthread_join(BXILOG__GLOBALS->handlers_threads[handler_rank],
                          (void**) handler_err);

    if (0 != rc) return bxierr_fromidx(rc, NULL,
                                       "Can't join handler thread. "
                                       "Calling pthread_join() failed (rc=%d)", rc);
    return BXIERR_OK;
}


void _setprocname(char * name) {
#ifdef __linux__
    errno = 0;
    const char * progname = NULL;
    bxistr_rsub(name, strlen(name), '/', &progname);
    int rc = prctl(15 /* PR_SET_NAME */, progname, 0, 0, 0);
    if (0 != rc) {
        bxierr_p err = bxierr_errno("Setting process name to '%s' with "
                                    "prctl() failed", name);
        bxierr_report(&err, STDERR_FILENO);
    }
#endif
}

bxierr_p _zmq_str_rcv_timeout(void * zocket, char ** reply, long timeout) {
    bxierr_p err = BXIERR_OK, err2 = BXIERR_OK;
    zmq_pollitem_t poll_set[] = {{ zocket, 0, ZMQ_POLLIN, 0 },};
    int rc = zmq_poll(poll_set, 1, timeout);
    if (-1 == rc) {
        err2 = bxierr_errno("Calling zmq_poll() failed");
        BXIERR_CHAIN(err, err2);
        return err;
    }

    if (0 == (poll_set[0].revents & ZMQ_POLLIN)) {
        err2 = bxierr_gen("Calling zmq_poll() timeout %ld", timeout);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    err2 = bxizmq_str_rcv(zocket, 0, false, reply);
    BXIERR_CHAIN(err, err2);

    return err;
}
