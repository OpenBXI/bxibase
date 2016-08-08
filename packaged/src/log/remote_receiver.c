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
static bxierr_p _connect_zocket(zmq_pollitem_t * poller, void ** context, char ** urls, int nb);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

#define RETRIES_MAX 3u

#define RETRY_DELAY 500000l

#define POLLING_TIMEOUT 500


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxierr_p bxilog_remote_recv(int nb_urls, char ** urls) {
    int rc;
    bool more;
    bxierr_p err;

    void * context = NULL;
    zmq_pollitem_t * poller = bximem_calloc(sizeof(zmq_pollitem_t));

    err = _connect_zocket(poller, &context, urls, nb_urls);
    if (bxierr_isko(err)) return err;

    tsd_p tsd;
    err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;

    while(true) {
        bxilog_record_p record_simple = bximem_calloc(sizeof(*record_simple));

        TRACE(_REMOTE_LOGGER, "Waiting for a record");
        rc =  zmq_poll(poller, 1, POLLING_TIMEOUT);

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
    rc =  zmq_poll(poller, 1, POLLING_TIMEOUT);

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
    rc =  zmq_poll(poller, 1, POLLING_TIMEOUT);

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
    rc =  zmq_poll(poller, 1, POLLING_TIMEOUT);

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
    rc =  zmq_poll(poller, 1, POLLING_TIMEOUT);

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
                             RETRIES_MAX, RETRY_DELAY);

      // Zero-copy version (if record has been mallocated).
      //            err2 = bxizmq_data_snd_zc(record, data_len,
      //                                      tsd->data_channel, ZMQ_DONTWAIT,
      //                                      RETRIES_MAX, RETRY_DELAY,
      //                                      bxizmq_data_free, NULL);
      BXIERR_CHAIN(err, err2);
    }

    return err;
}


bxierr_p _connect_zocket(zmq_pollitem_t * poller, void ** context, char ** urls, int nb) {
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
