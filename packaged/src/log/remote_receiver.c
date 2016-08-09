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


SET_LOGGER(_REMOTE_LOGGER, "bxilog.remote");


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
static bxierr_p _connect_zocket(zmq_pollitem_t * poller, void ** context, const char ** urls, int nb);
static bxierr_p _bxilog_remote_recv_loop(zmq_pollitem_t * poller);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

#define BXILOG_RECEIVER_RETRIES_MAX 3u

#define BXILOG_RECEIVER_RETRY_DELAY 500000l

#define BXILOG_RECEIVER_POLLING_TIMEOUT 500
#define BXILOG_RECEIVER_SYNC_TIMEOUT 1000

#define BXILOG_REMOTE_RECEIVER_SYNC "inproc://bxilog_remote_receiver_sync"
#define BXILOG_RECEIVER_SYNC_OK "OK"
#define BXILOG_RECEIVER_SYNC_NOK "NOK"

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxierr_p bxilog_remote_recv_async(bxilog_remote_recv_p param) {
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

    err = bxizmq_context_new(&context);
    if (bxierr_isko(err)) {
        return err;
    }

    assert(NULL != context);

    poller->events = ZMQ_POLLIN;

    TRACE(_REMOTE_LOGGER, "Creating and connecting the ZMQ socket to url: '%s'",
                          BXILOG_REMOTE_RECEIVER_SYNC);
    err = bxizmq_zocket_connect(context, ZMQ_PAIR,
                                BXILOG_REMOTE_RECEIVER_SYNC,
                                &poller->socket);
    if (bxierr_isko(err)) {
      return err;
    }

    rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    bxiassert(0 == rc);

    rc = pthread_create (&thrd, NULL, (void* (*) (void*)) bxilog_remote_recv, param);

    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }

    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_SYNC_TIMEOUT);

    if (0 <= rc) {
      LOWEST(_REMOTE_LOGGER, "No remote log received");
      return bxierr_simple(1, "Unable to synchronized with the bxilog receiver thread");
    }
    if (-1 == rc) {
      return bxierr_errno("A problem occurs while polling to receive a new remote log");
    }

    char * msg;
    err2 = bxizmq_str_rcv(poller->socket, 0, false, &msg);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isok(err)) {
      if (strcmp(msg, BXILOG_RECEIVER_SYNC_OK) != 0) {
        // TODO: retrieve the error from the thread
        err2 = bxierr_simple(1, "An error occured during the receiver thread configuration");
        BXIERR_CHAIN(err, err2);
      }
    }

    BXIFREE(msg);

    err2 = bxizmq_zocket_destroy(poller->socket);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_context_destroy(context);
    BXIERR_CHAIN(err, err2);

    return err;
}



bxierr_p _bxilog_remote_recv_sync(bxilog_remote_recv_p param) {
  bxierr_p err = BXIERR_OK, err2;

    void * context = NULL;
    zmq_pollitem_t * poller = bximem_calloc(sizeof(zmq_pollitem_t));

    err2 = _connect_zocket(poller, &context, param->urls, param->nb_urls);
    BXIERR_CHAIN(err, err2);

    void *socket = zmq_socket (context, ZMQ_PAIR);
    assert (socket);
    /* Connect it to an in-process transport with the address 'my_publisher' */
    int rc = zmq_connect (socket, BXILOG_REMOTE_RECEIVER_SYNC);
    assert (0 == rc);

    if (bxierr_isko(err)) {
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_NOK, socket, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    } else {
       err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_OK, socket, 0, 2, 500);
       BXIERR_CHAIN(err, err2);

       if (bxierr_isko(err)) {
           err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_OK, socket, 0, 2, 500);
           BXIERR_CHAIN(err, err2);
           return err;
       }
    }

    err = _bxilog_remote_recv_loop(poller);

    DEBUG(_REMOTE_LOGGER, "Leaving");
    TRACE(_REMOTE_LOGGER, "CLosing the socket");
    bxizmq_zocket_destroy(poller->socket);

    TRACE(_REMOTE_LOGGER, "CLosing the context");
    bxizmq_context_destroy(&context);

    BXIFREE(poller);

    return BXIERR_OK;
}


bxierr_p bxilog_remote_recv(bxilog_remote_recv_p param) {
    bxierr_p err;

    void * context = NULL;
    zmq_pollitem_t * poller = bximem_calloc(sizeof(zmq_pollitem_t));

    err = _connect_zocket(poller, &context, param->urls, param->nb_urls);
    if (bxierr_isko(err)) return err;

    err = _bxilog_remote_recv_loop(poller);

    DEBUG(_REMOTE_LOGGER, "Leaving");
    TRACE(_REMOTE_LOGGER, "CLosing the socket");
    bxizmq_zocket_destroy(poller->socket);

    TRACE(_REMOTE_LOGGER, "CLosing the context");
    bxizmq_context_destroy(&context);

    BXIFREE(poller);

    return BXIERR_OK;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _bxilog_remote_recv_loop(zmq_pollitem_t * poller) {
    int rc;
    bool more;
    tsd_p tsd;
    bxierr_p err;

    err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;

    while(true) {
        bxilog_record_p record_simple = bximem_calloc(sizeof(*record_simple));

        TRACE(_REMOTE_LOGGER, "Waiting for a record");
        rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_POLLING_TIMEOUT);

        if (0 == rc) {
            LOWEST(_REMOTE_LOGGER, "No remote log received");
            BXIFREE(record_simple);
            continue;
        }
        if (-1 == rc) {
            BXIFREE(record_simple);
            return bxierr_errno("A problem occurs while polling to receive a new remote log");
        }

        err = _receive_record(poller->socket, record_simple);
        assert(BXIERR_OK == err);
        DEBUG(_REMOTE_LOGGER, "Record received");

        TRACE(_REMOTE_LOGGER, "Checking for more message");
        err = bxizmq_msg_has_more(poller->socket, &more);
        assert(BXIERR_OK == err);
        assert(more);

        size_t var_len = record_simple->filename_len \
                       + record_simple->funcname_len \
                       + record_simple->logname_len;

        size_t data_len = sizeof(*record_simple) + var_len + record_simple->logmsg_len;

        TRACE(_REMOTE_LOGGER, "Reallocate record size to %lu", data_len);
        bxilog_record_p record = bximem_realloc(record_simple, sizeof(*record), data_len);

        DEBUG(_REMOTE_LOGGER, "Receiving the data");
        err = _receive_data(poller, &record);
        if (bxierr_isko(err)) {
            BXIFREE(record);
            return err;
        }

        DEBUG(_REMOTE_LOGGER, "Dispatching the log to all local handlers");
        err = _dispatch_log(tsd, record, data_len);

        BXIFREE(record);

        if (bxierr_isko(err)){
            return err;
        }
    }
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

    assert (NULL != poller->socket);

    data = (char*) record + sizeof(*record);

    TRACE(_REMOTE_LOGGER, "Waiting for filename");
    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive the remote log filename variable");
    }
    rc = zmq_recv(poller->socket, data, record->filename_len, 0);
    if (-1 == rc) {
       return bxierr_errno("Error while receiving filename value");
    }
    data += record->filename_len;

    DEBUG(_REMOTE_LOGGER, "Filename received");

    TRACE(_REMOTE_LOGGER, "Checking for more message");
    err = bxizmq_msg_has_more(poller->socket, &more);
    assert(BXIERR_OK == err);
    assert(more);

    TRACE(_REMOTE_LOGGER, "Waiting for funcname");
    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive the remote log funcname variable");
    }
    rc = zmq_recv(poller->socket, data, record->funcname_len, 0);
    if (-1 == rc) {
        return bxierr_errno("Error while receiving funcname value");
    }
    data += record->funcname_len;
    DEBUG(_REMOTE_LOGGER, "Funcname received");

    TRACE(_REMOTE_LOGGER, "Checking for more message");
    err = bxizmq_msg_has_more(poller->socket, &more);
    assert(BXIERR_OK == err);
    assert(more);

    TRACE(_REMOTE_LOGGER, "Waiting for loggername");
    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive the remote log loggername variable");
    }
    rc = zmq_recv(poller->socket, data, record->logname_len, 0);
    if (-1 == rc) {
        return bxierr_errno("Error while receiving loggername value");
    }
    data += record->logname_len;
    DEBUG(_REMOTE_LOGGER, "Loggername received");

    TRACE(_REMOTE_LOGGER, "Checking for more message");
    err = bxizmq_msg_has_more(poller->socket, &more);
    assert(BXIERR_OK == err);
    assert(more);

    TRACE(_REMOTE_LOGGER, "Waiting for logmsg");
    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_POLLING_TIMEOUT);

    if (0 == rc || -1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive the remote log message variable");
    }
    rc = zmq_recv(poller->socket, data, record->logmsg_len, 0);
    if (-1 == rc) {
        return bxierr_errno("Error while receiving logmsg value");
    }
    DEBUG(_REMOTE_LOGGER, "Logmsg received");

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


bxierr_p _connect_zocket(zmq_pollitem_t * poller, void ** context, const char ** urls, int nb) {
    bxierr_p err = BXIERR_OK;

    void * ctx = NULL;

    TRACE(_REMOTE_LOGGER, "Creating the ZMQ context");
    err = bxizmq_context_new(&ctx);
    if (bxierr_isko(err)) {
        return err;
    }

    *context = ctx;

    poller->events = ZMQ_POLLIN;

    for (int i = 0; i < nb; i++) {
        TRACE(_REMOTE_LOGGER, "Creating and connecting the ZMQ socket to url: '%s'",
              urls[i]);
        err = bxizmq_zocket_connect(ctx, ZMQ_SUB, urls[i], &poller->socket);
        if (bxierr_isko(err)) {
          return err;
        }
    }

    TRACE(_REMOTE_LOGGER, "Updating the subcription to everything on the socket");
    char * tree = "";
    err = bxizmq_zocket_setopt(poller->socket, ZMQ_SUBSCRIBE, tree, strlen(tree));

    return err;
}
