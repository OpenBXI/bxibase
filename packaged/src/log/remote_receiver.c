/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Sébastien Miquée <sebastien.miquee@atos.net>
 # Created on: 2016-08-08
 # Contributors: Pierre Vignéras <pierre.vigneras@atos.net>
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
#include <bxi/base/time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


#include "tsd_impl.h"
#include "log_impl.h"


SET_LOGGER(LOGGER, BXILOG_LIB_PREFIX "bxilog.remote");


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define _EXIT_NORMAL_ERR 38170247   // EXIT.OR.AL in leet speak
#define _BAD_HEADER_ERR  34034032   // B4D.EADER in leet speak
#define _BAD_RECORD_ERR  34023020   // B4DRE.ORD in leet speak
//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


/**
 * BXILog remote receiver parameters
 */
struct bxilog_remote_receiver_s {
    size_t pub_connected;      //!< Number of publishers connected at a given moment
    bool bind;                 //!< If true, bind instead of connect
    void * zmq_ctx;            //!< The ZMQ context used
    void * bc2it_zock;         //!< Control zocket for Business Code to
                               //!< Internal Thread communication
    void * it2bc_zock;         //!< Control zocket for IT to BC communication
    void * cfg_zock;           //!< The socket that receive configuration request
                               //!< NULL if bind is true.
    void * ctrl_zock;          //!< The control zocket
    void * data_zock;          //!< The socket that actually receive logs
    size_t urls_nb;            //!< Number of urls to connect/bind to
    const char ** urls;        //!< The urls to connect/bind to
    const char ** cfg_urls;    //!< Config urls used (if bind is true)
    const char ** ctrl_urls;   //!< Control urls used
    const char ** data_urls;   //!< Data urls used
    const char *  hostname;    //!< hostname of the remote handler
};


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
//--------------------------------- Generic Helpers --------------------------------
static bxierr_p _process_ctrl_msg(bxilog_remote_receiver_p self, tsd_p tsd);
static bxierr_p _process_new_log(bxilog_remote_receiver_p self, tsd_p tsd);
static bxierr_p _recv_log_record(void * zock, bxilog_record_p * record_p, size_t * record_len);
static bxierr_p _dispatch_log_record(tsd_p tsd, bxilog_record_p record, size_t data_len);
static bxierr_p _connect_zocket(bxilog_remote_receiver_p self);
static bxierr_p _recv_loop(bxilog_remote_receiver_p self);
static bxierr_p _recv_async(bxilog_remote_receiver_p self);
//static void _sync_sub(bxilog_remote_receiver_p self);
static bxierr_p _process_cfg_request(bxilog_remote_receiver_p self);
static bxierr_p _process_data_header(bxilog_remote_receiver_p self, char *header,
                                     tsd_p tsd, bool exiting);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

#define BXILOG_RECEIVER_RETRIES_MAX 3u

#define BXILOG_RECEIVER_RETRY_DELAY 500000l

#define BXILOG_RECEIVER_POLLING_TIMEOUT 500
#define BXILOG_RECEIVER_SYNC_TIMEOUT 1000

#define BXILOG_REMOTE_RECEIVER_BC2IT_URL "inproc://bxilog_remote_receiver_sync"
#define BXILOG_RECEIVER_SYNC_OK "OK"
#define BXILOG_RECEIVER_SYNC_NOK "NOK"
#define BXILOG_RECEIVER_EXIT "EXIT"
#define BXILOG_RECEIVER_EXITING "EXITING"


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_remote_receiver_p bxilog_remote_receiver_new(const char ** urls, size_t urls_nb,
                                                    bool bind, char * hostname) {

    if (bind && urls_nb > 1) {
        bxierr_p err = bxierr_gen("Binding on multiple urls is not supported yet!");
        BXILOG_REPORT(LOGGER, BXILOG_ERROR, err, "Cannot instanciate a remote receiver");
        return NULL;
    }

    bxilog_remote_receiver_p result = bximem_calloc(sizeof(*result));

    if (NULL != hostname) {
        result->hostname = strdup(hostname);
    }

    result->urls_nb = urls_nb;
    result->urls = bximem_calloc(urls_nb * sizeof(*result->urls));
    for (size_t i = 0; i < urls_nb; i++) {
        result->urls[i] = strdup(urls[i]);  // Strdup because the string can come from
                                            // higher level language which might garbage
                                            // collect it!
    }
    if (bind) {
        result->cfg_urls = bximem_calloc(urls_nb * sizeof(*result->cfg_urls));
    }
    result->ctrl_urls = bximem_calloc(urls_nb * sizeof(*result->data_urls));
    result->data_urls = bximem_calloc(urls_nb * sizeof(*result->data_urls));
    result->bind = bind;
    result->pub_connected = 0;

    return result;
}

void bxilog_remote_receiver_destroy(bxilog_remote_receiver_p *self_p) {
    bxilog_remote_receiver_p self = *self_p;
    for (size_t i = 0; i < self->urls_nb; i++) {
        BXIFREE(self->urls[i]);
        if (self->bind) {
            BXIFREE(self->cfg_urls[i]);
        }
        BXIFREE(self->ctrl_urls[i]);
        BXIFREE(self->data_urls[i]);

    }
    BXIFREE(self->urls);
    BXIFREE(self->hostname);
    if (self->bind) BXIFREE(self->cfg_urls);
    BXIFREE(self->ctrl_urls);
    BXIFREE(self->data_urls);
    bximem_destroy((char**) self_p);
}

bxierr_p bxilog_remote_receiver_start(bxilog_remote_receiver_p self) {
    bxierr_p err = BXIERR_OK, err2;

    if (NULL != self->zmq_ctx) {
        return bxierr_simple(1,
                             "Operation not permitted: this receiver %p has already "
                             "been started. Stop it first!", self);
    }

    TRACE(LOGGER, "Creating the ZMQ context");
    err2 = bxizmq_context_new(&self->zmq_ctx);
    BXIERR_CHAIN(err, err2);

    TRACE(LOGGER,
          "Creating and connecting bc2it zocket to url: '%s'",
          BXILOG_REMOTE_RECEIVER_BC2IT_URL);
    err2 = bxizmq_zocket_create_connected(self->zmq_ctx, ZMQ_PAIR,
                                          BXILOG_REMOTE_RECEIVER_BC2IT_URL,
                                          &self->bc2it_zock);
    BXIERR_CHAIN(err, err2);

    pthread_t thrd;
    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }

    rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_setdetachstate() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }

    rc = pthread_create(&thrd, NULL, (void* (*) (void*)) _recv_async, self);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_create() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }

    zmq_pollitem_t poller[] = {{self->bc2it_zock, 0, ZMQ_POLLIN, 0}};

    rc =  zmq_poll(poller, 1, BXILOG_RECEIVER_SYNC_TIMEOUT);

    if (rc <= 0) {
        LOWEST(LOGGER, "No answer received from receiver thread");
        return bxierr_simple(BXIZMQ_TIMEOUT_ERR,
                             "Unable to synchronize with "
                             "the bxilog receiver thread");
    }
    if (-1 == rc) {
        return bxierr_errno("A problem occurs while polling to receive a new remote log");
    }

    char * msg;
    err2 = bxizmq_str_rcv(self->bc2it_zock, 0, false, &msg);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isok(err)) {
        TRACE(LOGGER, "IT gaves the following state: '%s'", msg);
        if (0 != strncmp(BXILOG_RECEIVER_SYNC_OK, msg,
                         ARRAYLEN(BXILOG_RECEIVER_SYNC_OK) - 1)) {
            // TODO: retrieve the error from the thread
            err2 = bxierr_simple(BXIZMQ_PROTOCOL_ERR,
                                 "An error occured during the receiver "
                                 "thread configuration");
            BXIERR_CHAIN(err, err2);
        }
    }

    BXIFREE(msg);

    return err;
}


bxierr_p bxilog_remote_receiver_stop(bxilog_remote_receiver_p self,
                                     bool wait_remote_exit) {
    BXIASSERT(LOGGER, NULL != self);
    BXIASSERT(LOGGER, NULL != self->zmq_ctx);
    BXIASSERT(LOGGER, NULL != self->bc2it_zock);

    bxierr_p err = BXIERR_OK, err2;
    TRACE(LOGGER, "Sending the exit message: '%s'", BXILOG_RECEIVER_EXIT);
    err2 = bxizmq_str_snd(BXILOG_RECEIVER_EXIT, self->bc2it_zock, ZMQ_SNDMORE, 0, 0);
    BXIERR_CHAIN(err, err2);
    err2 = bxizmq_data_snd(&wait_remote_exit, sizeof(wait_remote_exit), self->bc2it_zock,
                           0, 0, 0);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) return err;

    long int timeout = BXILOG_RECEIVER_SYNC_TIMEOUT * 10;
    TRACE(LOGGER, "Polling the reply for %lu ms", timeout);
    zmq_pollitem_t poller[] = {{self->bc2it_zock, 0, ZMQ_POLLIN, 0}};
    int rc =  zmq_poll(poller, 1, timeout);

    if (rc <= 0) {
        LOWEST(LOGGER, "No answer received from receiver thread");
        return bxierr_gen("Unable to synchronize with the bxilog receiver internal thread");
    }
    if (-1 == rc) {
        return bxierr_errno("A problem occurred while polling internal thread messages");
    }

    char * msg;
    err2 = bxizmq_str_rcv(self->bc2it_zock, 0, false, &msg);
    BXIERR_CHAIN(err, err2);
    TRACE(LOGGER, "Reply received: '%s'", msg);

    if (bxierr_isok(err)) {
        if (0 != strncmp(BXILOG_RECEIVER_EXITING, msg,
                         ARRAYLEN(BXILOG_RECEIVER_EXITING) - 1)) {
            // TODO: retrieve the error from the thread
            err2 = bxierr_gen("An error occured during the receiver thread configuration");
            BXIERR_CHAIN(err, err2);
        }
    }

    BXIFREE(msg);

    TRACE(LOGGER, "Cleaning up");
    err2 = bxizmq_zocket_destroy(&self->bc2it_zock);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_context_destroy(&self->zmq_ctx);
    BXIERR_CHAIN(err, err2);

    return err;
}

size_t bxilog_get_binded_urls(bxilog_remote_receiver_p self, const char*** result) {
    BXIASSERT(LOGGER, NULL != self);

    *result = self->cfg_urls;
    return self->bind ? self->urls_nb : 0;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _recv_async(bxilog_remote_receiver_p self) {
    bxierr_p err = BXIERR_OK, err2;

    BXIASSERT(LOGGER, NULL != self->zmq_ctx);

    err2 = _connect_zocket(self);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) {
        BXILOG_REPORT_KEEP(LOGGER, BXILOG_FINE, err,
                           "An error occurred in the internal thread.");
        LOWEST(LOGGER, "Sending synchronization message with error");
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_NOK, self->it2bc_zock, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_zocket_destroy(&self->it2bc_zock);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_zocket_destroy(&self->data_zock);
        BXIERR_CHAIN(err, err2);

        if (NULL != self->cfg_zock) {
            err2 = bxizmq_zocket_destroy(&self->cfg_zock);
            BXIERR_CHAIN(err, err2);
        }

        TRACE(LOGGER, "Closing the context");
        err2 = bxizmq_context_destroy(&self->zmq_ctx);
        BXIERR_CHAIN(err, err2);

        return err;
    } else {
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_SYNC_OK, self->it2bc_zock,
                              0, 2, 500);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err)) {
            CRITICAL(LOGGER, "Unable to send synchronization message");
            BXIERR_CHAIN(err, err2);
            return err;
        }
    }

    if (self->bind) {
        err2 = _process_cfg_request(self);
        BXIERR_CHAIN(err, err2);
    }

    BXILOG_REPORT(LOGGER, BXILOG_ERROR, err,
                  "Some errors occurred during log receiving thread configuration"
                  "Continuing, various problems related to the logging systems can "
                  "be expected, such as logs lost and non-termination of program.");

    err2 = _recv_loop(self);
    BXIERR_CHAIN(err, err2);

    DEBUG(LOGGER, "Leaving");
    TRACE(LOGGER, "Closing the sockets");

    err2 = bxizmq_zocket_destroy(&self->data_zock);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_zocket_destroy(&self->ctrl_zock);
    BXIERR_CHAIN(err, err2);

    if (NULL != self->cfg_zock) {
        err2 = bxizmq_zocket_destroy(&self->cfg_zock);
        BXIERR_CHAIN(err, err2);
    }

    err2 = bxizmq_zocket_destroy(&self->it2bc_zock);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _recv_loop(bxilog_remote_receiver_p self) {
    bxierr_p err = BXIERR_OK, err2;

    tsd_p tsd;
    err2 = bxilog__tsd_get(&tsd);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) return err;

    bool loop = true;

    zmq_pollitem_t poller[] = {{self->it2bc_zock, 0, ZMQ_POLLIN, 0},
                               {self->cfg_zock, 0, ZMQ_POLLIN, 0},
                               {self->data_zock, 0, ZMQ_POLLIN, 0}};

    while (loop) {
        errno = 0;
        int rc =  zmq_poll(poller, 3, BXILOG_RECEIVER_POLLING_TIMEOUT);

        if (0 == rc) continue;
        if (-1 == rc) return bxierr_errno("A problem occurs while polling");

        if (poller[0].revents & ZMQ_POLLIN) {
            // Control command received from BC
            bxierr_p tmp = _process_ctrl_msg(self, tsd);
            if (bxierr_isko(tmp)) {
                if (_EXIT_NORMAL_ERR == tmp->code) {
                    bxierr_destroy(&tmp);
                } else {
                    BXIERR_CHAIN(err, tmp);
                    BXILOG_REPORT(LOGGER, BXILOG_CRITICAL, err,
                                  "An error occured, exiting");
                }
                break;
            }
        }
        if (poller[1].revents & ZMQ_POLLIN) {
            // Configuration request received from remote side
            _process_cfg_request(self);
        }

        if (poller[2].revents & ZMQ_POLLIN) {
            // Log received from remote side
            char * header;
            err2 = bxizmq_str_rcv(poller[2].socket, 0, false, &header);
            BXIERR_CHAIN(err, err2);

            err2 = _process_data_header(self, header, tsd, false);
            BXIERR_CHAIN(err, err2);
            BXIFREE(header);
            if (bxierr_isko(err)) break;
        }
    }
    return err;
}

bxierr_p _process_ctrl_msg(bxilog_remote_receiver_p self, tsd_p tsd) {
    bxierr_p err = BXIERR_OK, err2;

    char * msg;
    err2 = bxizmq_str_rcv(self->it2bc_zock, 0, false, &msg);
    BXIERR_CHAIN(err, err2);

    FINE(LOGGER, "Processing control message: %s", msg);

    if (bxierr_isko(err)) return bxierr_errno("An error occurred while "
                                              "receiving control message");

    if (0 == strncmp(BXILOG_RECEIVER_EXIT, msg,
                     ARRAYLEN(BXILOG_RECEIVER_EXIT) - 1)) {
        FINE(LOGGER, "Received message '%s' indicating to exit", msg);
        BXIFREE(msg);

        bool wait_remote_exit;
        bool *tmp_p = &wait_remote_exit;
        err2 = bxizmq_data_rcv((void**)&tmp_p, sizeof(*tmp_p), self->it2bc_zock,
                               0, true, NULL);
        BXIERR_CHAIN(err, err2);

        struct timespec last_message;
        err2 = bxitime_get(CLOCK_MONOTONIC, &last_message);
        BXIERR_CHAIN(err, err2);

        // Fetch all remaining logs before exiting
        while (true) {
            char * header;
            err2 = bxizmq_str_rcv(self->data_zock, ZMQ_DONTWAIT, false, &header);
            BXIERR_CHAIN(err, err2);

            // When ZMQ_DONTWAIT, if header == NULL it means we have nothing to receive
            if (NULL == header) {
                if (wait_remote_exit) {
                    double duration = 0;
                    err2 = bxitime_duration(CLOCK_MONOTONIC, last_message,
                                            &duration);
                    BXIERR_CHAIN(err, err2);
                    if (0 < self->pub_connected && duration < 5.0) {
                        LOWEST(LOGGER,
                               "%zu publishers still connected and wait remote "
                               "exit requested", self->pub_connected);
                        bxierr_p tmp = bxitime_sleep(CLOCK_MONOTONIC, 0, 500000);
                        bxierr_destroy(&tmp);
                        continue;
                    }
                }
                TRACE(LOGGER, "No remaining stuff to process, ready to exit");
                break;
            }

            TRACE(LOGGER, "Header '%s' remains to be processed while exiting", header);
            err2 = _process_data_header(self, header, tsd, true);
            BXIERR_CHAIN(err, err2);
            BXIFREE(header);
            if (bxierr_isko(err)) break;
            err2 = bxitime_get(CLOCK_MONOTONIC, &last_message);
            BXIERR_CHAIN(err, err2);
        }

        FINE(LOGGER, "Sending back the exit confirmation message");
        err2 = bxizmq_str_snd(BXILOG_RECEIVER_EXITING, self->it2bc_zock, 0, 2, 500);
        BXIERR_CHAIN(err, err2);

        return bxierr_simple(_EXIT_NORMAL_ERR, "Normal error meaning exit");
    }

    BXIFREE(msg);
    return bxierr_gen("Unknown control message received: '%s'", msg);
}


bxierr_p _process_data_header(bxilog_remote_receiver_p self, char *header, tsd_p tsd,
                              bool exiting) {
    BXIASSERT(LOGGER, NULL != self);
    BXIASSERT(LOGGER, NULL != header);

    if (0 == strncmp(BXIZMQ_PUBSUB_SYNC_HEADER, header,
                     ARRAYLEN(BXIZMQ_PUBSUB_SYNC_HEADER) - 1)) {
        // Synchronization required
        TRACE(LOGGER, "Received sync message");
        if (exiting) {
            char * sync_url = NULL;
            bxierr_p err = bxizmq_str_rcv(self->data_zock,
                                          ZMQ_DONTWAIT, false, &sync_url);
            BXIFREE(sync_url);
            return err;
        }
        bxierr_p err = bxizmq_sub_sync_manage(self->zmq_ctx, self->data_zock);
        BXILOG_REPORT(LOGGER, BXILOG_WARNING, err,
                      "Problem during SUB synchronization - continuing (best effort)");
        return BXIERR_OK;
    }
    if (0 == strncmp(BXILOG_REMOTE_HANDLER_EXITING_HEADER, header,
                     ARRAYLEN(BXILOG_REMOTE_HANDLER_EXITING_HEADER) - 1)) {
        // One other end has exited, fetch its URL
        char * url = NULL;
        bxierr_p err = bxizmq_str_rcv(self->data_zock, 0, true, &url);
        if (bxierr_isko(err)) return err;
        self->pub_connected--;
        FINE(LOGGER,
             "Publisher %s has sent its exit message. "
             "Number of connected publishers: %zu",
             url, self->pub_connected);
        BXIFREE(url);
        return BXIERR_OK;
    }
    if (0 == strncmp(BXILOG_REMOTE_HANDLER_RECORD_HEADER,
                     header, ARRAYLEN(BXILOG_REMOTE_HANDLER_RECORD_HEADER)-1)) {
        bxierr_p err  = _process_new_log(self, tsd);
        BXILOG_REPORT(LOGGER, BXILOG_WARNING, err,
                      "Problem while receiving bxilog record - continuing (best effort)");
        return BXIERR_OK;
    }
    bxierr_p tmp_err = bxierr_simple(_BAD_HEADER_ERR,
                                     "Wrong bxilog header: %s",
                                     header);
    BXILOG_REPORT(LOGGER, BXILOG_WARNING, tmp_err,
                  "Error detected but continuing anyway (best-effort).");
    return BXIERR_OK;
}

bxierr_p _recv_log_record(void * zock, bxilog_record_p * record_p, size_t * record_len) {

    bxierr_p err = BXIERR_OK, err2;

    // Initialize
    *record_p = NULL;
    *record_len = 0;

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
                             "Wrong bxilog record: expected size=%zu, received size=%zu",
                             expected_len, size);
    }
    LOWEST(LOGGER, "Record received, size: %zu", size);

    if (bxierr_isok(err)) {
        *record_p = record;
        *record_len = size;
    }
    return err;
}


bxierr_p _dispatch_log_record(tsd_p tsd, bxilog_record_p record, size_t data_len) {

    bxierr_p err = BXIERR_OK, err2;

    LOWEST(LOGGER,
           "Dispatching the log to all %zu handlers",
           BXILOG__GLOBALS->internal_handlers_nb);

    for (size_t i = 0; i < BXILOG__GLOBALS->internal_handlers_nb; i++) {
      // Send the frame
      // normal version if record comes from the stack 'buf'
      err2 = bxizmq_data_snd(&i, sizeof(i),
                             tsd->data_channel, ZMQ_DONTWAIT|ZMQ_SNDMORE,
                             BXILOG_RECEIVER_RETRIES_MAX,
                             BXILOG_RECEIVER_RETRY_DELAY);
      BXIERR_CHAIN(err, err2);
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


bxierr_p _connect_zocket(bxilog_remote_receiver_p self) {
    bxierr_p err = BXIERR_OK, err2;

    if (self->bind) {
        TRACE(LOGGER, "Creating config zocket");
        err2 = bxizmq_zocket_create(self->zmq_ctx, ZMQ_ROUTER, &self->cfg_zock);
        BXIERR_CHAIN(err, err2);
    }

    TRACE(LOGGER, "Creating control zocket");
    err2 = bxizmq_zocket_create(self->zmq_ctx, ZMQ_DEALER, &self->ctrl_zock);
    BXIERR_CHAIN(err, err2);

    TRACE(LOGGER, "Creating data zocket");
    err2 = bxizmq_zocket_create(self->zmq_ctx, ZMQ_SUB, &self->data_zock);
    BXIERR_CHAIN(err, err2);

    FINE(LOGGER, "Binding/connecting zocket to %zu urls", self->urls_nb);
    for (size_t i = 0; i < self->urls_nb; i++) {
        if (self->bind) {
            TRACE(LOGGER, "Binding config zocket to url: '%s'", self->urls[i]);
            int port = 0;
            err2 = bxizmq_zocket_bind(self->cfg_zock, self->urls[i], &port);
            BXIERR_CHAIN(err, err2);

            if (0 != port) {
                self->cfg_urls[i] =  bxizmq_create_url_from(self->urls[i], port);
            } else {
                self->cfg_urls[i] = strdup(self->urls[i]);
            }
            FINE(LOGGER, "Config zocket binded to '%s", self->cfg_urls[i]);

            char * url = NULL;
            bxizmq_generate_new_url_from(self->cfg_urls[i], &url);
            TRACE(LOGGER,
                  "Binding control zocket to url: '%s'", url);
            port = 0;
            err2 = bxizmq_zocket_bind(self->ctrl_zock, url, &port);
            BXIERR_CHAIN(err, err2);
            self->ctrl_urls[i] = bxizmq_create_url_from(url, port);
            FINE(LOGGER, "Control zocket binded to '%s", self->ctrl_urls[i]);

            url = NULL;
            bxizmq_generate_new_url_from(self->cfg_urls[i], &url);
            TRACE(LOGGER,
                  "Binding data zocket to url: '%s'", url);
            port = 0;
            err2 = bxizmq_zocket_bind(self->data_zock, url, &port);
            BXIERR_CHAIN(err, err2);
            self->data_urls[i] = bxizmq_create_url_from(url, port);
            FINE(LOGGER, "Data zocket binded to '%s", self->data_urls[i]);
        } else {
            self->ctrl_urls[i] = strdup(self->urls[i]);
            TRACE(LOGGER, "Connecting control zocket to url: '%s'", self->ctrl_urls[i]);
            err2 = bxizmq_zocket_connect(self->ctrl_zock, self->ctrl_urls[i]);
            BXIERR_CHAIN(err, err2);

            FINE(LOGGER,
                 "Requesting configuration through control zocket '%s'",
                 self->ctrl_urls[i]);

            err2 = bxizmq_str_snd(BXILOG_REMOTE_HANDLER_URLS, self->ctrl_zock, 0, 0 ,0);
            BXIERR_CHAIN(err, err2);
            if (bxierr_isko(err)) {
                BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err,
                                   "Can't retrieve the URLs through '%s'",
                                   self->ctrl_urls[i]);
                return err;
            }
            TRACE(LOGGER, "Waiting for URL reception from '%s", self->ctrl_urls[i]);
            char * url = NULL;
            err2 = bxizmq_str_rcv(self->ctrl_zock, 0, false, &url);
            BXIERR_CHAIN(err, err2);

            TRACE(LOGGER, "Connecting data zocket to url: '%s'", url);
            err2 = bxizmq_zocket_connect(self->data_zock, url);
            BXIERR_CHAIN(err, err2);

            self->data_urls[i] = url;
        }
    }

    if (NULL != self->data_zock) {
        TRACE(LOGGER, "Updating the subscription to everything on data zocket");
        char * tree = "";
        err2 = bxizmq_zocket_setopt(self->data_zock, ZMQ_SUBSCRIBE, tree, strlen(tree));
        BXIERR_CHAIN(err, err2);
    }

    TRACE(LOGGER,
          "Creating and binding the it2bc zocket to url: '%s'",
          BXILOG_REMOTE_RECEIVER_BC2IT_URL);
    err2 = bxizmq_zocket_create_binded(self->zmq_ctx, ZMQ_PAIR,
                                       BXILOG_REMOTE_RECEIVER_BC2IT_URL, NULL,
                                       &self->it2bc_zock);

    BXIERR_CHAIN(err, err2);

    return err;
}


//void _sync_sub(bxilog_remote_receiver_p self) {
//    TRACE(LOGGER, "Performing SUB Synchronization");
//    // TODO: make the timeout configurable
//    double timeout = BXILOG_REMOTE_HANDLER_SYNC_DEFAULT_TIMEOUT;
//
//    bxierr_p tmp = bxizmq_sync_sub(self->zmq_ctx, self->data_zock, timeout);
//    if (bxierr_isko(tmp)){
//        BXILOG_REPORT(LOGGER, BXILOG_NOTICE,
//                      tmp,
//                      "SUB synchronization failed. First published messages "
//                      "might have been lost!");
//    }
//}


bxierr_p _process_new_log(bxilog_remote_receiver_p self, tsd_p tsd) {
    bxierr_p err = BXIERR_OK, err2;

    bxilog_record_p record;
    size_t record_len;

    bxierr_p tmp = _recv_log_record(self->data_zock, &record, &record_len);

    if (bxierr_isko(tmp)) {
        bxierr_report_keep(tmp, STDERR_FILENO);
        if (_BAD_HEADER_ERR == tmp->code) {
            LOWEST(LOGGER,
                   "Skipping bad header error, looks like a "
                   "PUB/SUB sync. Error message is: %s", tmp->msg);
            bxierr_destroy(&tmp);
        } else {
            BXILOG_REPORT(LOGGER, BXILOG_WARNING, tmp,
                          "An error occured while retrieving a bxilog record, "
                          "a message might be missing");
        }
    } else {
        err2 = _dispatch_log_record(tsd, record, record_len);
        BXIERR_CHAIN(err, err2);
    }

    return err;
}

bxierr_p _process_cfg_request(bxilog_remote_receiver_p self) {
    // The other side must first ask for the connection URLs through the
    // configuration zocket
    bxierr_p err = BXIERR_OK, err2;

    DEBUG(LOGGER, "Config request received");

    // First frame of ROUTER: id
    zmq_msg_t id;
    err2 = bxizmq_msg_init(&id);
    BXIERR_CHAIN(err, err2);
    err2 = bxizmq_msg_rcv(self->cfg_zock, &id, 0);
    BXIERR_CHAIN(err, err2);

    // Second frame: the message
    char * msg = NULL;
    err2 = bxizmq_str_rcv(self->cfg_zock, 0, true, &msg);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        return err;
    }

    if (0 != strncmp(msg, BXILOG_REMOTE_HANDLER_URLS,
                     ARRAYLEN(BXILOG_REMOTE_HANDLER_URLS) - 1)) {
        err2 = bxierr_gen("Bad request through config zocket. "
                          "Expected: '%s', received: '%s'",
                          BXILOG_REMOTE_HANDLER_URLS, msg);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    DEBUG(LOGGER, "Sending back %zu ctrl urls", self->urls_nb);
    // Send a multi-part message
    // First frame: id
    err2 = bxizmq_msg_snd(&id, self->cfg_zock, ZMQ_SNDMORE, 0, 0);
    BXIERR_CHAIN(err, err2);
    size_t hostnames_nb = 0;
    if (NULL != self->hostname) {
        hostnames_nb++;
    }
    err2 = bxizmq_data_snd(&hostnames_nb, sizeof(hostnames_nb), self->cfg_zock,
                            ZMQ_SNDMORE, 0, 0);
    BXIERR_CHAIN(err, err2);
    if (1 == hostnames_nb) {
        DEBUG(LOGGER, "Sending back hostname %s", self->hostname);
        err2 = bxizmq_str_snd(self->hostname, self->cfg_zock, ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);
    }

    // Next frame: number of urls
    err2 = bxizmq_data_snd_zc(&self->urls_nb, sizeof(self->urls_nb), self->cfg_zock,
                              ZMQ_SNDMORE, 0, 0, NULL, NULL);

    // Then all ctrl urls
    for (size_t i = 0; i < self->urls_nb; i++) {
        DEBUG(LOGGER, "Sending back url %s", self->ctrl_urls[i]);
        err2 = bxizmq_str_snd(self->ctrl_urls[i], self->cfg_zock, ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);
    }
    // Then all data urls
    for (size_t i = 0; i < self->urls_nb - 1; i++) {
        err2 = bxizmq_str_snd(self->data_urls[i], self->cfg_zock, ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);
    }
    // Then last frame
    err2 = bxizmq_str_snd(self->data_urls[self->urls_nb - 1], self->cfg_zock, 0, 0, 0);
    BXIERR_CHAIN(err, err2);

    self->pub_connected++;
    FINE(LOGGER,
         "New publisher synchronization completed. "
         "Number of connected publishers: %zu",
         self->pub_connected);

    return err;
}
