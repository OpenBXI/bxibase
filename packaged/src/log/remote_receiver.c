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
#include <bxi/base/log/remote_receiver.h>

#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "tsd_impl.h"
#include "log_impl.h"


SET_LOGGER(LOGGER, BXILOG_LIB_PREFIX "bxilog.remote");


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
//--------------------------------- Generic Helpers --------------------------------

static bxierr_p _receive_record(void * sock, bxilog_record_p record);
static bxierr_p _receive_data(zmq_pollitem_t * poller, bxilog_record_p * rec);
static bxierr_p _dispatch_log(tsd_p tsd, bxilog_record_p record, size_t data_len);
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

    TRACE(LOGGER, "Creating and connecting the ZMQ socket to url: '%s'",
                          BXILOG_REMOTE_RECEIVER_SYNC_URL);
    err2 = bxizmq_zocket_connect(context, ZMQ_PAIR,
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
        LOWEST(LOGGER, "No answer received from receiver thread");
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

    TRACE(LOGGER,
          "Creating and connecting the ZMQ socket to url: '%s'",
          BXILOG_REMOTE_RECEIVER_SYNC_URL);

    err2 = bxizmq_zocket_connect(context, ZMQ_PAIR,
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
        LOWEST(LOGGER, "No answer received from receiver thread");
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
            err2 = bxierr_simple(1, "An error occured during the receiver thread configuration");
            BXIERR_CHAIN(err, err2);
        }
    }

    BXIFREE(msg);

    err2 = bxizmq_zocket_destroy(poller->socket);
    BXIERR_CHAIN(err, err2);

    return err;
}


bxierr_p bxilog_remote_recv(bxilog_remote_recv_p param) {
    bxierr_p err;

    void * context = NULL;
    zmq_pollitem_t * poller = bximem_calloc(sizeof(*poller) * 2);

    err = _connect_zocket(poller, &context, param->urls, param->nb_urls, param->bind);
    if (bxierr_isko(err)) return err;

    err = _bxilog_remote_recv_loop(poller);

    DEBUG(LOGGER, "Leaving");
    TRACE(LOGGER, "Closing the sockets");
    bxizmq_zocket_destroy(poller[0].socket);
    bxizmq_zocket_destroy(poller[1].socket);

    TRACE(LOGGER, "Closing the context");
    bxizmq_context_destroy(&context);

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
        LOWEST(LOGGER, "Sending synchronization message with error");
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_NOK, poller[1].socket, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_zocket_destroy(poller[1].socket);
        err2 = bxizmq_zocket_destroy(poller[0].socket);

        TRACE(LOGGER, "Closing the context");
        err2 = bxizmq_context_destroy(&context);

        BXIERR_CHAIN(err, err2);

        BXIFREE(poller);

        return err;
    } else {
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_OK, poller[1].socket, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err)) {
            CRITICAL(LOGGER, "Unable to send synchronization message");
            BXIERR_CHAIN(err, err2);
            return err;
        }
    }

    err = _bxilog_remote_recv_loop(poller);

    DEBUG(LOGGER, "Leaving");
    TRACE(LOGGER, "Closing the sockets");
    bxizmq_zocket_destroy(poller[0].socket);
    bxizmq_zocket_destroy(poller[1].socket);

    TRACE(LOGGER, "Closing the context");
    bxizmq_context_destroy(&context);
    BXIFREE(poller);

    return BXIERR_OK;
}


bxierr_p _bxilog_remote_recv_loop(zmq_pollitem_t * poller) {
    int rc;
    bool more, loop;
    tsd_p tsd;
    bxierr_p err = BXIERR_OK, err2;

    err2 = bxilog__tsd_get(&tsd);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) return err;

    loop = true;

    while(loop) {
        bxilog_record_p record_simple = bximem_calloc(sizeof(*record_simple));

        LOWEST(LOGGER, "Waiting for a record");
        rc =  zmq_poll(poller, 2, BXILOG_RECEIVER_POLLING_TIMEOUT);

        if (0 == rc) {
            LOWEST(LOGGER, "No remote log received");
            BXIFREE(record_simple);
            continue;
        }
        if (-1 == rc) {
            BXIFREE(record_simple);
            return bxierr_errno("A problem occurs while polling to receive a new remote log");
        }

        if (poller[1].revents > 0) {
            TRACE(LOGGER, "A control message has been received");
            char * msg;
            errno = 0;
            err2 = bxizmq_str_rcv(poller[1].socket, 0, false, &msg);
            BXIERR_CHAIN(err, err2);
            BXIFREE(record_simple);

            if (bxierr_isok(err)) {
              if (0 == strncmp(BXILOG_RECEIVER_EXIT, msg, ARRAYLEN(BXILOG_RECEIVER_EXIT))) {
                  DEBUG(LOGGER, "Received message indicating to exit");
                  BXIFREE(msg);

                  DEBUG(LOGGER, "Sending back the exit confirmation message");
                  err2 = bxizmq_str_snd(BXILOG_RECEIVER_EXITING, poller[1].socket, 0, 2, 500);
                  sleep(1);
                  BXIERR_CHAIN(err, err2);
                  if (bxierr_isko(err)) {
                    return err;
                  }
                  loop = false;
              } else {
                  WARNING(LOGGER, "Received unknown control message: '%s'", msg);
                  BXIFREE(msg);
              }
            } else {
                return bxierr_errno("An error occured while receiving control message");
            }
        } else {

            err = _receive_record(poller[0].socket, record_simple);
            assert(BXIERR_OK == err);
            LOWEST(LOGGER, "Record received");

            LOWEST(LOGGER, "Checking for more message");
            err = bxizmq_msg_has_more(poller[0].socket, &more);
            assert(BXIERR_OK == err);
            assert(more);

            size_t var_len = record_simple->filename_len \
                           + record_simple->funcname_len \
                           + record_simple->logname_len;

            size_t data_len = sizeof(*record_simple) + var_len + record_simple->logmsg_len;

            LOWEST(LOGGER, "Reallocate record size to %zu", data_len);
            bxilog_record_p record = bximem_realloc(record_simple, sizeof(*record), data_len);

            LOWEST(LOGGER, "Receiving the data");
            err = _receive_data(poller, &record);
            if (bxierr_isko(err)) {
                BXIFREE(record);
                return err;
            }

            LOWEST(LOGGER, "Dispatching the log to all local handlers");
            err = _dispatch_log(tsd, record, data_len);

            BXIFREE(record);

            if (bxierr_isko(err)){
                return err;
            }
        }
    }

    return err;
}


bxierr_p _receive_record(void * sock, bxilog_record_p record) {
    int rc;
    rc = zmq_recv(sock, record, sizeof(bxilog_record_s), 0);
    if (rc == -1) {
        return bxierr_errno("Error while receiving the bxilog record");
    }

    return BXIERR_OK;
}


bxierr_p _receive_data(zmq_pollitem_t * poller, bxilog_record_p * rec) {
    int rc;
    bool more;
    char * data;
    bxierr_p err;

    bxilog_record_p record = *rec;

    assert (NULL != poller[0].socket);
    assert (NULL != poller[1].socket);

    data = (char*) record + sizeof(*record);

    LOWEST(LOGGER, "Waiting for filename");
    rc =  zmq_poll(poller, 2, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive "
                            "the remote log filename variable");
    }
    rc = zmq_recv(poller[0].socket, data, record->filename_len, 0);
    if (-1 == rc) {
       return bxierr_errno("Error while receiving filename value");
    }
    data += record->filename_len;

    LOWEST(LOGGER, "Filename received");

    LOWEST(LOGGER, "Checking for more message");
    err = bxizmq_msg_has_more(poller[0].socket, &more);
    assert(BXIERR_OK == err);
    assert(more);

    LOWEST(LOGGER, "Waiting for funcname");
    rc =  zmq_poll(poller, 2, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive "
                            "the remote log funcname variable");
    }
    rc = zmq_recv(poller[0].socket, data, record->funcname_len, 0);
    if (-1 == rc) {
        return bxierr_errno("Error while receiving funcname value");
    }
    data += record->funcname_len;
    LOWEST(LOGGER, "Funcname received");

    LOWEST(LOGGER, "Checking for more message");
    err = bxizmq_msg_has_more(poller[0].socket, &more);
    assert(BXIERR_OK == err);
    assert(more);

    LOWEST(LOGGER, "Waiting for loggername");
    rc =  zmq_poll(poller, 2, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive"
                            " the remote log loggername variable");
    }
    rc = zmq_recv(poller[0].socket, data, record->logname_len, 0);
    if (-1 == rc) {
        return bxierr_errno("Error while receiving loggername value");
    }
    data += record->logname_len;
    LOWEST(LOGGER, "Loggername received");

    LOWEST(LOGGER, "Checking for more message");
    err = bxizmq_msg_has_more(poller[0].socket, &more);
    assert(BXIERR_OK == err);
    assert(more);

    LOWEST(LOGGER, "Waiting for logmsg");
    rc =  zmq_poll(poller, 2, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive "
                            "the remote log message variable");
    }
    rc = zmq_recv(poller[0].socket, data, record->logmsg_len, 0);
    if (-1 == rc) {
        return bxierr_errno("Error while receiving logmsg value");
    }
    LOWEST(LOGGER, "Logmsg received");

    return BXIERR_OK;
}


bxierr_p _dispatch_log(tsd_p tsd, bxilog_record_p record, size_t data_len) {
    bxierr_p err = BXIERR_OK, err2;

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

        TRACE(LOGGER, "Creating the ZMQ context");
        err2 = bxizmq_context_new(&ctx);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err)) return err;

        *context = ctx;
    }

    poller[0].events = ZMQ_POLLIN;
    poller[1].events = ZMQ_POLLIN;

    for (int i = 0; i < nb; i++) {
        TRACE(LOGGER, "Creating and %s the ZMQ socket to url: '%s'",
              bind ? "binding" : "connecting", urls[i]);
        if (bind) {
            int port;
            err2 = bxizmq_zocket_bind(*context, ZMQ_SUB, urls[i], &port, &poller[0].socket);
            BXIERR_CHAIN(err, err2);
        } else {
            err2 = bxizmq_zocket_connect(*context, ZMQ_SUB, urls[i], &poller[0].socket);
            BXIERR_CHAIN(err, err2);
        }
    }

    if (NULL != poller[0].socket) {
        TRACE(LOGGER, "Updating the subcription to everything on the socket");
        char * tree = "";
        err2 = bxizmq_zocket_setopt(poller[0].socket, ZMQ_SUBSCRIBE, tree, strlen(tree));
        BXIERR_CHAIN(err, err2);
    }

    TRACE(LOGGER,
          "Creating and connecting the control ZMQ socket to url: '%s'",
          BXILOG_REMOTE_RECEIVER_SYNC_URL);

    err2 = bxizmq_zocket_bind(*context, ZMQ_PAIR, BXILOG_REMOTE_RECEIVER_SYNC_URL, NULL,
                              &poller[1].socket);
    BXIERR_CHAIN(err, err2);

    return err;
}
