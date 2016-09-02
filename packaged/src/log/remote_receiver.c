/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Sébastien Miquée <sebastien.miquee@atos.net>
 # Created on: 2016-08-08
 # Contributors:
 ###############################################################################
 # Copyright (C) 2016  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */


#include <bxi/base/mem.h>
#include <bxi/base/zmq.h>
#include <bxi/base/log/remote_handler.h>
#include <bxi/base/log/remote_receiver.h>

#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "tsd_impl.h"
#include "log_impl.h"


SET_LOGGER(_REMOTE_LOGGER, "bxilog.remote");


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define _EXIT_NORMAL_ERR 38170247   // EXIT.OR.AL in leet speak
#define _BAD_HEADER_ERR  34034032   // B4D.EADER in leet speak
#define _BAD_RECORD_ERR  34023020   // B4DRE.ORD in leet speak
//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
//--------------------------------- Generic Helpers --------------------------------
static bxierr_p _process_ctrl_msg(void * zock);
static bxierr_p _recv_log_record(void * zock, bxilog_record_p * record_p, size_t * record_len);
static bxierr_p _dispatch_log_record(tsd_p tsd, bxilog_record_p record, size_t data_len);
static bxierr_p _connect_zocket(zmq_pollitem_t * poller,
                                void ** context, const char ** urls, int nb, bool bind);
static bxierr_p _bxilog_remote_recv_loop(zmq_pollitem_t * poller);
static bxierr_p _bxilog_remote_recv_async(bxilog_remote_recv_p param);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

#define BXILOG_RECEIVER_RETRIES_MAX 3u

#define BXILOG_RECEIVER_RETRY_DELAY 500000l

#define BXILOG_RECEIVER_POLLING_TIMEOUT 500
#define BXILOG_RECEIVER_SYNC_TIMEOUT 1000

#define BXILOG_REMOTE_RECEIVER_SYNC_URL "inproc://bxilog_remote_receiver_sync"
#define BXILOG_RECEIVER_SYNC_OK "OK"
#define BXILOG_RECEIVER_SYNC_NOK "NOK"
#define BXILOG_RECEIVER_EXIT "EXIT"
#define BXILOG_RECEIVER_EXITING "EXITING"

static void * _remote_receiver_ctx = NULL;


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxierr_p bxilog_remote_recv_async_start(bxilog_remote_recv_p param) {
    bxierr_p err = BXIERR_OK, err2;

    pthread_t thrd;
    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);

    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }

    void * context = NULL;
    zmq_pollitem_t * poller = bximem_calloc(sizeof(zmq_pollitem_t));

    if (NULL != _remote_receiver_ctx) {
        return bxierr_simple(1,
                             "Operation not permitted: "
                             "the internal context is already set");
    }

    err2 = bxizmq_context_new(&_remote_receiver_ctx);
    BXIERR_CHAIN(err, err2);

    context = _remote_receiver_ctx;

    assert(NULL != context);

    poller->events = ZMQ_POLLIN;

    TRACE(_REMOTE_LOGGER, "Creating and connecting the ZMQ socket to url: '%s'",
                          BXILOG_REMOTE_RECEIVER_SYNC_URL);
    err2 = bxizmq_zocket_create_connected(context, ZMQ_PAIR,
                                          BXILOG_REMOTE_RECEIVER_SYNC_URL,
                                          &poller->socket);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        return err;
    }

    rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    bxiassert(0 == rc);

    rc = pthread_create(&thrd, NULL, (void* (*) (void*)) _bxilog_remote_recv_async, param);

    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }

    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_SYNC_TIMEOUT);

    if (rc <= 0) {
        LOWEST(_REMOTE_LOGGER, "No answer received from receiver thread");
        return bxierr_simple(1, "Unable to synchronized with the bxilog receiver thread");
    }
    if (-1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive a new remote log");
    }

    char * msg;
    err2 = bxizmq_str_rcv(poller->socket, 0, false, &msg);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isok(err)) {
        if (0 != strncmp(BXILOG_RECEIVER_SYNC_OK, msg, ARRAYLEN(BXILOG_RECEIVER_SYNC_OK))) {
            // TODO: retrieve the error from the thread
            err2 = bxierr_simple(1, "An error occured during the receiver thread configuration");
            BXIERR_CHAIN(err, err2);
        }
    }

    BXIFREE(msg);

    err2 = bxizmq_zocket_destroy(poller->socket);
    BXIERR_CHAIN(err, err2);

    return err;
}


bxierr_p bxilog_remote_recv_async_stop(void) {
    bxierr_p err = BXIERR_OK, err2;
    int rc;

    void * context = _remote_receiver_ctx;
    zmq_pollitem_t * poller = bximem_calloc(sizeof(zmq_pollitem_t));

    assert(NULL != context);

    poller->events = ZMQ_POLLIN;

    TRACE(_REMOTE_LOGGER,
          "Creating and connecting the ZMQ socket to url: '%s'",
          BXILOG_REMOTE_RECEIVER_SYNC_URL);

    err2 = bxizmq_zocket_create_connected(context, ZMQ_PAIR,
                                          BXILOG_REMOTE_RECEIVER_SYNC_URL,
                                          &poller->socket);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        return err;
    }

    err2 = bxizmq_str_snd(BXILOG_RECEIVER_EXIT, poller->socket, 0, 2, 500);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        return err;
    }

    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_SYNC_TIMEOUT * 10);

    if (rc <= 0) {
        LOWEST(_REMOTE_LOGGER, "No answer received from receiver thread");
        return bxierr_simple(1, "Unable to synchronized with the bxilog receiver thread");
    }
    if (-1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive a new remote log");
    }

    char * msg;
    err2 = bxizmq_str_rcv(poller->socket, 0, false, &msg);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isok(err)) {
        if (0 != strncmp(BXILOG_RECEIVER_EXITING, msg, ARRAYLEN(BXILOG_RECEIVER_EXITING))) {
            // TODO: retrieve the error from the thread
            err2 = bxierr_simple(1, "An error occured during the "
                                 "receiver thread configuration");
            BXIERR_CHAIN(err, err2);
        }
    }

    BXIFREE(msg);

    err2 = bxizmq_zocket_destroy(poller->socket);
    BXIERR_CHAIN(err, err2);

    return err;
}


bxierr_p bxilog_remote_recv(bxilog_remote_recv_p param) {
    bxierr_p err = BXIERR_OK, err2;

    void * context = NULL;
    zmq_pollitem_t * poller = bximem_calloc(sizeof(*poller) * 2);

    err2 = _connect_zocket(poller, &context, param->urls, param->nb_urls, param->bind);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) return err;

    if (param->bind) {
        for (size_t i = 0; i < param->sync_nb; i++) {
            TRACE(_REMOTE_LOGGER, "PUB/SUB Synchronization #%zu/%zu",
                  i+1, param->sync_nb);
            err2 = bxizmq_sync_sub(_remote_receiver_ctx, &poller[0].socket, 0.5);
            BXIERR_CHAIN(err, err2);
        }
    }


    err2 = _bxilog_remote_recv_loop(poller);
    BXIERR_CHAIN(err, err2);

    DEBUG(_REMOTE_LOGGER, "Leaving");
    TRACE(_REMOTE_LOGGER, "Closing the sockets");

    err2 = bxizmq_zocket_destroy(poller[0].socket);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_zocket_destroy(poller[1].socket);
    BXIERR_CHAIN(err, err2);

    TRACE(_REMOTE_LOGGER, "Closing the context");
    err2 = bxizmq_context_destroy(&context);
    BXIERR_CHAIN(err, err2);

    BXIFREE(poller);

    return err;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _bxilog_remote_recv_async(bxilog_remote_recv_p param) {
    bxierr_p err = BXIERR_OK, err2;

    void * context = _remote_receiver_ctx;

    bxiassert(NULL != context);

    zmq_pollitem_t * poller = bximem_calloc(sizeof(*poller) * 2);

    err2 = _connect_zocket(poller, &context, param->urls, param->nb_urls, param->bind);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) {
        LOWEST(_REMOTE_LOGGER, "Sending synchronization message with error");
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_NOK, poller[1].socket, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_zocket_destroy(poller[1].socket);

        err2 = bxizmq_zocket_destroy(poller[0].socket);
        BXIERR_CHAIN(err, err2);

        TRACE(_REMOTE_LOGGER, "Closing the context");
        err2 = bxizmq_context_destroy(&context);
        BXIERR_CHAIN(err, err2);

        BXIFREE(poller);

        return err;
    } else {
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_OK, poller[1].socket, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err)) {
            CRITICAL(_REMOTE_LOGGER, "Unable to send synchronization message");
            BXIERR_CHAIN(err, err2);
            return err;
        }
    }

    if (param->bind) {
        for (size_t i = 0; i < param->sync_nb; i++) {
            TRACE(_REMOTE_LOGGER, "PUB/SUB Synchronization #%zu/%zu",
                  i + 1, param->sync_nb);
            err2 = bxizmq_sync_sub(context, poller[0].socket, 0.5);
            BXIERR_CHAIN(err, err2);
        }
    }

    err2 = _bxilog_remote_recv_loop(poller);
    BXIERR_CHAIN(err, err2);

    DEBUG(_REMOTE_LOGGER, "Leaving");
    TRACE(_REMOTE_LOGGER, "Closing the sockets");

    err2 = bxizmq_zocket_destroy(poller[0].socket);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_zocket_destroy(poller[1].socket);
    BXIERR_CHAIN(err, err2);

    TRACE(_REMOTE_LOGGER, "Closing the context");
    err2 = bxizmq_context_destroy(&context);
    BXIERR_CHAIN(err, err2);
    BXIFREE(poller);

    return BXIERR_OK;
}


bxierr_p _bxilog_remote_recv_loop(zmq_pollitem_t * poller) {
    bxierr_p err = BXIERR_OK, err2;

    tsd_p tsd;
    err2 = bxilog__tsd_get(&tsd);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) return err;

    bool loop = true;

    while(loop) {
        errno = 0;
        int rc =  zmq_poll(poller, 2, BXILOG_RECEIVER_POLLING_TIMEOUT);

        if (0 == rc) continue;
        if (-1 == rc) return bxierr_errno("A problem occurs while polling "
                                          "to receive a new remote log");

        if (poller[1].revents > 0) {
            bxierr_p tmp = _process_ctrl_msg(poller[1].socket);


            if (bxierr_isko(tmp)) {
                if (_EXIT_NORMAL_ERR == tmp->code) {
                    bxierr_destroy(&tmp);
                } else {
                    BXIERR_CHAIN(err, tmp);
                    BXILOG_REPORT(_REMOTE_LOGGER, BXILOG_CRITICAL, err,
                                  "An error occured, exiting");
                }
                break;
            }

        } else {

            bxilog_record_p record;
            size_t record_len;

            bxierr_p tmp = _recv_log_record(poller[0].socket, &record, &record_len);

            if (bxierr_isko(tmp)) {
                if (_BAD_HEADER_ERR == tmp->code) {
                    LOWEST(_REMOTE_LOGGER,
                           "Skipping bad header error, looks like a "
                            "PUB/SUB sync. Error message is: %s", tmp->msg);
                    bxierr_destroy(&tmp);
                } else {
                    BXILOG_REPORT(_REMOTE_LOGGER, BXILOG_WARNING, tmp,
                                  "An error occured while retrieving a bxilog record, "
                                  "a message might be missing");
                }
            } else {
                err2 = _dispatch_log_record(tsd, record, record_len);
                BXIERR_CHAIN(err, err2);
            }
        }
    }

    return err;
}

bxierr_p _process_ctrl_msg(void * zock) {
    bxierr_p err = BXIERR_OK, err2;

    char * msg;
    err2 = bxizmq_str_rcv(zock, 0, false, &msg);
    BXIERR_CHAIN(err, err2);


    if (bxierr_isko(err)) return bxierr_errno("An error occured while "
                                              "receiving control message");

    if (0 == strncmp(BXILOG_RECEIVER_EXIT, msg, ARRAYLEN(BXILOG_RECEIVER_EXIT))) {
        FINE(_REMOTE_LOGGER, "Received message '%s' indicating to exit", msg);
        BXIFREE(msg);

        FINE(_REMOTE_LOGGER, "Sending back the exit confirmation message");
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_EXITING, zock, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        return bxierr_simple(_EXIT_NORMAL_ERR, "Normal error significating exit");
    }

    BXIFREE(msg);
    return bxierr_gen("Unknown control message received: '%s'", msg);
}


bxierr_p _recv_log_record(void * zock, bxilog_record_p * record_p, size_t * record_len) {

    bxierr_p err = BXIERR_OK, err2;

    // Initialize
    *record_p = NULL;
    *record_len = 0;

    char * header;
    err2 = bxizmq_str_rcv(zock, 0, false, &header);
    BXIERR_CHAIN(err, err2);

    if (0 != strncmp(BXILOG_REMOTE_HANDLER_HEADER,
                     header,
                     ARRAYLEN(BXILOG_REMOTE_HANDLER_HEADER) - 1)) {

        err2 = bxierr_simple(_BAD_HEADER_ERR,
                             "Bad header: '%s', skipping", header);
        BXIERR_CHAIN(err, err2);
        BXIFREE(header);

        return err;
    }
    FINE(_REMOTE_LOGGER, "Header received: '%s'", header);
    BXIFREE(header);

    bxilog_record_p record = NULL;
    size_t size;
    err2 = bxizmq_data_rcv((void**)&record, 0, zock, 0, true, &size);
    BXIERR_CHAIN(err, err2);

    size_t expected_len = sizeof(*record) + \
            record->filename_len + \
            record->funcname_len + \
            record->logname_len + \
            record->logmsg_len;


    if (size != expected_len) {
        return bxierr_simple(_BAD_RECORD_ERR,
                             "Wrong bxi log record: expected size=%zu, received size=%zu",
                             expected_len, size);
    }
    FINE(_REMOTE_LOGGER, "Record received, size: %zu", size);

    if (bxierr_isok(err)) {
        *record_p = record;
        *record_len = size;
    }
    return err;
}


bxierr_p _dispatch_log_record(tsd_p tsd, bxilog_record_p record, size_t data_len) {

    bxierr_p err = BXIERR_OK, err2;

    DEBUG(_REMOTE_LOGGER,
          "Dispatching the log to all %zu handlers",
          BXILOG__GLOBALS->internal_handlers_nb);

    for (size_t i = 0; i< BXILOG__GLOBALS->internal_handlers_nb; i++) {
      // Send the frame
      // normal version if record comes from the stack 'buf'
      err2 = bxizmq_data_snd(record, data_len,
                             tsd->data_channel, ZMQ_DONTWAIT,
                             BXILOG_RECEIVER_RETRIES_MAX,
                             BXILOG_RECEIVER_RETRY_DELAY);

      // Zero-copy version (if record has been mallocated).
      //            err2 = bxizmq_data_snd_zc(record, data_len,
      //                                      tsd->data_channel, ZMQ_DONTWAIT,
      //                                      RETRIES_MAX, RETRY_DELAY,
      //                                      bxizmq_data_free, NULL);
      BXIERR_CHAIN(err, err2);
    }

    return err;
}


bxierr_p _connect_zocket(zmq_pollitem_t * poller,
                         void ** context,
                         const char ** urls, int nb, bool bind) {

    bxierr_p err = BXIERR_OK, err2;

    if (NULL == *context) {
        void * ctx = NULL;

        TRACE(_REMOTE_LOGGER, "Creating the ZMQ context");
        err2 = bxizmq_context_new(&ctx);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err)) return err;

        *context = ctx;
    }

    poller[0].events = ZMQ_POLLIN;
    poller[1].events = ZMQ_POLLIN;

    for (int i = 0; i < nb; i++) {
        TRACE(_REMOTE_LOGGER, "Creating and %s the ZMQ socket to url: '%s'",
              bind ? "binding" : "connecting", urls[i]);
        if (bind) {
            int port;
            err2 = bxizmq_zocket_create_binded(*context, ZMQ_SUB,
                                               urls[i], &port, &poller[0].socket);
            BXIERR_CHAIN(err, err2);

        } else {
            err2 = bxizmq_zocket_create_connected(*context, ZMQ_SUB,
                                                  urls[i], &poller[0].socket);
            BXIERR_CHAIN(err, err2);
        }
    }

    if (NULL != poller[0].socket) {
        TRACE(_REMOTE_LOGGER, "Updating the subcription to everything on the socket");
        char * tree = "";
        err2 = bxizmq_zocket_setopt(poller[0].socket, ZMQ_SUBSCRIBE, tree, strlen(tree));
        BXIERR_CHAIN(err, err2);
    }

    TRACE(_REMOTE_LOGGER, "Creating and connecting the control ZMQ socket to url: '%s'",
                          BXILOG_REMOTE_RECEIVER_SYNC_URL);
    err2 = bxizmq_zocket_create_binded(*context, ZMQ_PAIR,
                                       BXILOG_REMOTE_RECEIVER_SYNC_URL, NULL,
                                       &poller[1].socket);
    BXIERR_CHAIN(err, err2);

    return err;
}


