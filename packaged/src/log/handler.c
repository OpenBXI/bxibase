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
#include <sys/syscall.h>
#include <pthread.h>
#include <sysexits.h>
#include <string.h>
#include <error.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

#include "handler_impl.h"
#include "log_impl.h"


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

typedef struct {
    bool zmq_context_created;
    void * ctrl_zocket;
    void * data_zocket;

#ifdef __linux__
    pid_t tid;                              // the thread pid
#endif
    uint16_t rank;                          // the thread rank
} handler_data_s;

typedef handler_data_s * handler_data_p;

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static bxierr_p _mask_signals(bxilog_handler_p);
static bxierr_p _set_zmq_ctx(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _create_zockets(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _bind_ctrl_zocket(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _bind_data_zocket(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _init_handler(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _send_ready_status(bxilog_handler_p, bxilog_handler_param_p, handler_data_p, bxierr_p err);
static bxierr_p _loop(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _cleanup(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _internal_flush(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _process_err(bxilog_handler_p handler, bxilog_handler_param_p,  bxierr_p err);
static bxierr_p _process_log_record(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _process_ctrl_cmd(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _process_implicit_flush(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _process_explicit_flush(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);
static bxierr_p _process_exit(bxilog_handler_p, bxilog_handler_param_p, handler_data_p);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

void bxilog_handler_init_param(bxilog_handler_p handler, bxilog_handler_param_p param) {
    bxiassert(NULL != param);

    param->mask_signals = true;
    param->zmq_context = NULL;
    param->data_hwm = 65536;
    param->ctrl_hwm = 100;
    param->connect_timeout_s = 1.0;
    param->poll_timeout_ms = 1000;

    // Use the param pointer to guarantee a unique URL name for different instances of
    // the same handler

    // WARNING: IF YOU CHANGE FOR ipc:// or tcp://
    // ENSURE THE ZMQ_IO_THREADS IS NO MORE
    // SET TO 0 AT ZMQ_CTX CREATION (in core.c).
    param->ctrl_url = bxistr_new("inproc://%s/%p.ctrl", handler->name, param);
    param->data_url = bxistr_new("inproc://%s/%p.data", handler->name, param);

//    param->ctrl_url = bxistr_new("ipc:///tmp/%s.ctrl", handler->name);
//    param->data_url = bxistr_new("ipc:///tmp/%s.data", handler->name);

    // Subscribe to all messages
    param->subscriptions_nb = 1;
    param->subscriptions = bximem_calloc(param->subscriptions_nb *\
                                         sizeof(*param->subscriptions));
    param->subscriptions[0] = strdup("");

}

void bxilog_handler_clean_param(bxilog_handler_param_p param) {
    BXIFREE(param->ctrl_url);
    BXIFREE(param->data_url);
    for (size_t j = 0; j < param->subscriptions_nb; j++) {
        BXIFREE(param->subscriptions[j]);
    }
    BXIFREE(param->subscriptions);
    // Do not free param since it has not been allocated by init()
    // BXIFREE(param);
 }

bxierr_p bxilog__handler_start(bxilog__handler_thread_bundle_p bundle) {
    bxilog_handler_p handler = bundle->handler;
    bxilog_handler_param_p param = bundle->param;

    BXIFREE(bundle);

    bxierr_p eerr = BXIERR_OK, eerr2; // External errors
    bxierr_p ierr = BXIERR_OK;        // Internal errors
    handler_data_s data;
    memset(&data, 0, sizeof(data));

    // Constants for the IHT
#ifdef __linux__
    data.tid = (pid_t) syscall(SYS_gettid);
#endif
    // TODO: find something better for the rank of the subscriber thread
    // Maybe, define already the related string instead of
    // a rank number?
    data.rank = (uint16_t) pthread_self();

    eerr2 = _init_handler(handler, param, &data);
    BXIERR_CHAIN(eerr, eerr2);
    // Do not quit immediately, we need to send a ready message to BC.

    ierr = _set_zmq_ctx(handler, param, &data);
    eerr2 = _process_err(handler, param, ierr);
    BXIERR_CHAIN(eerr, eerr2);

    ierr = _create_zockets(handler, param, &data);
    eerr2 = _process_err(handler, param, ierr);
    BXIERR_CHAIN(eerr, eerr2);

    if (param->mask_signals) {
        ierr = _mask_signals(handler);
        eerr2 = _process_err(handler, param, ierr);
        BXIERR_CHAIN(eerr, eerr2);
    }

    ierr = _send_ready_status(handler, param, &data, eerr);
    eerr2 = _process_err(handler, param, ierr);
    BXIERR_CHAIN(eerr, eerr2);

    // Ok, we synced with BC. If now there is an error at that stage
    // no need to go in the loop, just cleanup and exit.
    if (bxierr_isko(eerr)) goto CLEANUP;

    ierr = _loop(handler, param, &data);

CLEANUP:
    eerr2 = _process_err(handler, param, ierr);
    BXIERR_CHAIN(eerr, eerr2);

    ierr = _cleanup(handler, param, &data);
    eerr2 = _process_err(handler, param, ierr);
    BXIERR_CHAIN(eerr, eerr2);

    eerr2 = _process_exit(handler, param, &data);
    BXIERR_CHAIN(eerr, eerr2);

    return eerr;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

// All signals are blocked in this thread, they must be dealt with
// by other threads. Note: this is only true for asynchronous signals
// such as SIGINT, SIGQUIT and so on...
// In particular synchronous signals such as SIGSEGV, SIGBUS, and the like
// are always sent to the thread that generated them. Therefore,
// there is no real thing to do unless implementing a complex sigsetjmp/siglongjmp
// in order to produce a log, and flush the log.
// However, this is complex too, since signal handlers is per process, not per-thread
// Therefore, all threads will share same signal handler.
bxierr_p _mask_signals(bxilog_handler_p handler) {
    bxierr_p err = BXIERR_OK, err2 = BXIERR_OK;
    sigset_t mask;
    int rc = sigfillset(&mask);
    if (0 != rc) err2 = bxierr_errno("%s: calling sigfillset() failed", handler->name);
    BXIERR_CHAIN(err, err2);

    // block all signals.
    // Note: undefined behaviour for SIGBUS, SIGFPE, SIGILL and SIGSEGV
    // See sigprocmask(2)
    rc = pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if (-1 == rc) err2 = bxierr_errno("%s: calling pthread_sigmask() failed",
                                      handler->name);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _set_zmq_ctx(bxilog_handler_p handler,
                      bxilog_handler_param_p param,
                      handler_data_p data) {

    UNUSED(data);
    errno = 0;

    if (NULL == param->zmq_context) {
        void * ctx = zmq_ctx_new();
        if (NULL == ctx) return bxierr_errno("%s: calling zmq_ctx_new() failed",
                                             handler->name);
        param->zmq_context = ctx;
        data->zmq_context_created = true;
    } else {
        data->zmq_context_created = false;
    }
    return BXIERR_OK;
}

bxierr_p _create_zockets(bxilog_handler_p handler,
                         bxilog_handler_param_p param,
                         handler_data_p data) {

    bxierr_p err = BXIERR_OK, err2;

    err2 = _bind_ctrl_zocket(handler, param, data);
    BXIERR_CHAIN(err, err2);

    err2 = _bind_data_zocket(handler, param, data);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _bind_ctrl_zocket(bxilog_handler_p handler,
                           bxilog_handler_param_p param,
                           handler_data_p data) {
    UNUSED(handler);
    bxierr_p err = BXIERR_OK, err2;



    int affected_port;
    err2 = bxizmq_zocket_bind(param->zmq_context,
                              ZMQ_REP,
                              param->ctrl_url,
                              &affected_port,
                              &data->ctrl_zocket);
    BXIERR_CHAIN(err, err2);

//    fprintf(stderr, "Binding %p to %s\n", data->ctrl_zocket, param->ctrl_url);

    err2 = bxizmq_zocket_setopt(data->ctrl_zocket,
                                ZMQ_RCVHWM,
                                &param->ctrl_hwm,
                                sizeof(param->ctrl_hwm));
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _bind_data_zocket(bxilog_handler_p handler,
                           bxilog_handler_param_p param,
                           handler_data_p data) {
    UNUSED(handler);
    bxierr_p err = BXIERR_OK, err2;
    int affected_port;
    err2 = bxizmq_zocket_bind(param->zmq_context,
                              ZMQ_SUB,
                              param->data_url,
                              &affected_port,
                              &data->data_zocket);
    BXIERR_CHAIN(err, err2);

//    fprintf(stderr, "Binding %p to %s\n", data->data_zocket, param->data_url);

    bxierr_group_p errlist = bxierr_group_new();
    for (size_t i = 0; i < param->subscriptions_nb; i++) {
        bxierr_p suberr = bxizmq_zocket_setopt(data->data_zocket,
                                               ZMQ_SUBSCRIBE,
                                               param->subscriptions[i],
                                               strlen(param->subscriptions[i]));
        if (bxierr_isko(suberr)) bxierr_list_append(errlist, suberr);
    }
    if (0 < errlist->errors_nb) {
        err2 = bxierr_from_group(BXIERR_GROUP_CODE,
                                 errlist,
                                 "Error during zeromq subscriptions");
        BXIERR_CHAIN(err, err2);
    } else {
        bxierr_group_destroy(&errlist);
    }

    err2 = bxizmq_zocket_setopt(data->data_zocket,
                                ZMQ_RCVHWM,
                                &param->data_hwm,
                                sizeof(param->data_hwm));
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _init_handler(bxilog_handler_p handler,
                       bxilog_handler_param_p param,
                       handler_data_p data) {
    UNUSED(data);
    return (NULL == handler->init) ? BXIERR_OK : handler->init(param);
}

bxierr_p _send_ready_status(bxilog_handler_p handler,
                            bxilog_handler_param_p param,
                            handler_data_p data,
                            bxierr_p err) {
    UNUSED(handler);
    UNUSED(param);

    char * msg;
    bxierr_p fatal_err = bxizmq_str_rcv(data->ctrl_zocket, 0, 0, &msg);
    bxierr_abort_ifko(fatal_err);

    if (0 != strncmp(READY_CTRL_MSG_REQ, msg, ARRAYLEN(READY_CTRL_MSG_REQ))) {
        bxierr_p result = bxierr_new(BXIZMQ_PROTOCOL_ERR, NULL, NULL, NULL, NULL,
                                     "Expected message '%s' but received '%s'",
                                     READY_CTRL_MSG_REQ, msg);
        BXIFREE(msg);
        return result;
    }
    BXIFREE(msg);
    if (bxierr_isok(err)) {
        fatal_err = bxizmq_str_snd(READY_CTRL_MSG_REP, data->ctrl_zocket, 0, 0, 0);
        bxierr_abort_ifko(fatal_err);

        return BXIERR_OK;
    }

    // Send the error message
    char * err_str = bxierr_str(err);
    fatal_err = bxizmq_str_snd(err_str, data->ctrl_zocket, 0, 0, 0);
    BXIFREE(err_str);
    bxierr_abort_ifko(fatal_err);

    return BXIERR_OK;
}

bxierr_p _loop(bxilog_handler_p handler,
               bxilog_handler_param_p param,
               handler_data_p data) {

    bxierr_p err = BXIERR_OK, err2;

    zmq_pollitem_t items[] = { { data->ctrl_zocket, 0, ZMQ_POLLIN, 0 },
                               { data->data_zocket, 0, ZMQ_POLLIN, 0 } };
    while (true) {
        errno = 0;
        int rc = zmq_poll(items, 2, param->poll_timeout_ms);
        if (-1 == rc) {
            if (EINTR == errno) continue; // One interruption happened
                                          // (e.g. with profiling)

            int code = zmq_errno();
            const char * msg = zmq_strerror(code);
            err2 = bxierr_new(code, NULL, NULL, NULL, NULL,
                              "Calling zmq_poll() failed: %s", msg);
            BXIERR_CHAIN(err, err2);
            err2 = _process_implicit_flush(handler, param, data);
            BXIERR_CHAIN(err, err2);
            err = _process_err(handler, param, err);
            if (bxierr_isko(err)) return err;
        }
        if (0 == rc) {
            // Nothing to poll -> do a flush() and start again
            err2 = _process_implicit_flush(handler, param, data);
            BXIERR_CHAIN(err, err2);
            err = _process_err(handler, param, err);
            if (bxierr_isko(err)) return err;
            continue;
        }
        if (items[0].revents & ZMQ_POLLIN) {
            // Process ctrl message
            err2 = _process_ctrl_cmd(handler, param, data);
            BXIERR_CHAIN(err, err2);
            // Treat explicit exit message
            if (BXILOG_HANDLER_EXIT_CODE == err->code) return err;
            err = _process_err(handler, param, err);
            if (bxierr_isko(err)) return err;
        }
        if (items[1].revents & ZMQ_POLLIN) {
            // Process data, this is the normal case
            err2 = _process_log_record(handler, param, data);
            BXIERR_CHAIN(err, err2);
            err = _process_err(handler, param, err);
            if (bxierr_isko(err)) return err;
        }
    }

    bxiunreachable_statement;
    return err;
}

bxierr_p _cleanup(bxilog_handler_p handler,
                  bxilog_handler_param_p param,
                  handler_data_p data) {
    UNUSED(handler);

    bxierr_p err = BXIERR_OK, err2;

    err2 =  bxizmq_zocket_destroy(data->data_zocket);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_zocket_destroy(data->ctrl_zocket);
    BXIERR_CHAIN(err, err2);

    if (data->zmq_context_created) {
        errno = 0;
        int rc = zmq_ctx_destroy(&param->zmq_context);
        if (0 != rc) {
            err2 = bxierr_errno("Calling zmq_ctx_destroy() failed.");
            BXIERR_CHAIN(err, err2);
        }
    }

    return err;
}

bxierr_p _internal_flush(bxilog_handler_p handler,
                         bxilog_handler_param_p param,
                         handler_data_p data) {

    bxierr_p err = BXIERR_OK, err2;

    // Check that no new messages arrived in between
    zmq_pollitem_t data_items[] = {{data->data_zocket, 0, ZMQ_POLLIN,0}};
    while(true) {
        errno = 0;
        int rc = zmq_poll(data_items, 1, 0); // 0 -> do not wait
        if (-1 == rc) {
            if(EINTR == errno) continue; // One interruption happens
                                         // (e.g. when profiling)
            int code = zmq_errno();
            const char * msg = zmq_strerror(code);
            err2 = bxierr_new(code, NULL, NULL, NULL, NULL,
                              "Calling zmq_poll() failed: %s", msg);
            BXIERR_CHAIN(err, err2);
            break;
        }
        if (!(data_items[0].revents & ZMQ_POLLIN)) {
            // No message are pending
            break;
        }
        // Some messages are still pending
        err2 = _process_log_record(handler, param, data);
        BXIERR_CHAIN(err, err2);
    }

    return err;
}

bxierr_p _process_log_record(bxilog_handler_p handler,
                             bxilog_handler_param_p param,
                             handler_data_p data) {

    bxierr_p err = BXIERR_OK, err2;
    zmq_msg_t zmsg;
    errno = 0;
    int rc = zmq_msg_init(&zmsg);
    bxiassert(0 == rc);
    err2 = bxizmq_msg_rcv(data->data_zocket, &zmsg, ZMQ_DONTWAIT);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        // Nothing to process, this might happened if a flush has been asked
        // and processed before this function call
        err2 = bxizmq_msg_close(&zmsg);
        BXIERR_CHAIN(err, err2);
        if (EAGAIN == err->code) {
            bxierr_destroy(&err);
            return BXIERR_OK;
        }
        return err;
    }

    // bxiassert we received enough data
//    const size_t received_size = zmq_msg_size(&zmsg);
//    bxiassert(received_size >= BXILOG__GLOBALS->RECORD_MINIMUM_SIZE);

    bxilog_record_s * record = zmq_msg_data(&zmsg);
    // Fetch other strings: filename, funcname, loggername, logmsg
    char * filename = (char *) record + sizeof(*record);
    char * funcname = filename + record->filename_len;
    char * loggername = funcname + record->funcname_len;
    char * logmsg = loggername + record->logname_len;

    if (NULL != handler->process_log) {
        err2 = handler->process_log(record,
                                    filename, funcname, loggername, logmsg,
                                    param);
        BXIERR_CHAIN(err, err2);
    }

    /* Release */
    err2 = bxizmq_msg_close(&zmsg);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _process_ctrl_cmd(bxilog_handler_p handler,
                           bxilog_handler_param_p param,
                           handler_data_p data) {

    bxierr_p err = BXIERR_OK, err2;

    char * cmd = NULL;
    err2 = bxizmq_str_rcv(data->ctrl_zocket, ZMQ_DONTWAIT, false, &cmd);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        // Nothing to process, this might happened if a flush has been asked
        // and processed before this function call
        bxiassert(NULL == cmd);
        return (EAGAIN == err->code) ? BXIERR_OK : err;
    }

    if (0 == strncmp(FLUSH_CTRL_MSG_REQ, cmd, ARRAYLEN(FLUSH_CTRL_MSG_REQ))) {
        BXIFREE(cmd);
        err2 = _process_explicit_flush(handler, param, data);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_str_snd(FLUSH_CTRL_MSG_REP, data->ctrl_zocket, 0, 0, 0);
        BXIERR_CHAIN(err, err2);
        return err;
    }
    if (0 == strncmp(EXIT_CTRL_MSG_REQ, cmd, ARRAYLEN(EXIT_CTRL_MSG_REQ))) {
        BXIFREE(cmd);
        // Flushing must be asked explicitly.
        // Therefore, there is nothing to flush on exit.
        // Exit must exit as fast as possible.
        //        err2 = _process_flush_cmd(handler, data);
        //        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_str_snd(EXIT_CTRL_MSG_REP, data->ctrl_zocket, 0, 0, 0);
        BXIERR_CHAIN(err, err2);

        return bxierr_new(BXILOG_HANDLER_EXIT_CODE, err, NULL, NULL, NULL,
                          "Exit requested");
    }
    err2 = bxierr_gen("%s: unknown control command: %s", handler->name, cmd);
    BXIERR_CHAIN(err, err2);
    BXIFREE(cmd);
    return err;
}

bxierr_p _process_implicit_flush(bxilog_handler_p handler,
                                 bxilog_handler_param_p param,
                                 handler_data_p data) {

    bxierr_p err = BXIERR_OK, err2;

    err2 = _internal_flush(handler, param, data);
    BXIERR_CHAIN(err, err2);

    err2 = (NULL == handler->process_implicit_flush) ? BXIERR_OK :
            handler->process_implicit_flush(param);

    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _process_explicit_flush(bxilog_handler_p handler,
                                 bxilog_handler_param_p param,
                                 handler_data_p data) {

    bxierr_p err = BXIERR_OK, err2;

    err2 = _internal_flush(handler, param, data);
    BXIERR_CHAIN(err, err2);

    err2 = (NULL == handler->process_explicit_flush) ? BXIERR_OK :
            handler->process_explicit_flush(param);

    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _process_exit(bxilog_handler_p handler,
                       bxilog_handler_param_p param,
                       handler_data_p data) {

    UNUSED(handler);
    UNUSED(data);

    return (NULL == handler->process_exit) ?
            BXIERR_OK :
            handler->process_exit(param);
}

bxierr_p _process_err(bxilog_handler_p handler,
                      bxilog_handler_param_p param,
                      bxierr_p err) {

    if (bxierr_isok(err) || NULL == handler->process_err) return err;

    bxierr_p actual_err = err;
    if (BXILOG_HANDLER_EXIT_CODE == err->code) {
        bxiassert(NULL == err->cause);
        actual_err = (NULL == err->data) ? BXIERR_OK : err->data;
        err->data = NULL;
        bxierr_destroy(&err);
    }

    return handler->process_err(&actual_err, param);
}
