/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Sébastien Miquée <sebastien.miquee@atos.net>
 # Created on: 2016/07/27
 # Contributors:
 ###############################################################################
 # Copyright (C) 2016  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */


#include <string.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/zmq.h"
#include "bxi/base/time.h"

#include "bxi/base/log.h"

#include "bxi/base/log/remote_handler.h"


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define INTERNAL_LOGGER_NAME BXILOG_LIB_PREFIX "bxilog.handler.remote"

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

typedef struct bxilog_remote_handler_param_s_f * bxilog_remote_handler_param_p;
typedef struct bxilog_remote_handler_param_s_f {
    bxilog_handler_param_s generic;
    bool bind;
    bool synced;
    char * url;
    void * ctx;
    void * zock;
} bxilog_remote_handler_param_s;


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                         bxilog_filters_p filters,
                                         va_list ap);
static bxierr_p _init(bxilog_remote_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_remote_handler_param_p data);
static bxierr_p _process_ierr(bxierr_p * err, bxilog_remote_handler_param_p data);
static bxierr_p _process_implicit_flush(bxilog_remote_handler_param_p data);
static bxierr_p _process_explicit_flush(bxilog_remote_handler_param_p data);
static bxierr_p _process_exit(bxilog_remote_handler_param_p data);
static bxierr_p _process_cfg(bxilog_remote_handler_param_p data);
static bxierr_p _param_destroy(bxilog_remote_handler_param_p *data_p);
static bxierr_p _sync_pub_sub(bxilog_remote_handler_param_p data);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

static const bxilog_handler_s BXILOG_REMOTE_HANDLER_S = {
                  .name = "BXI Logging Monitor Handler",
                  .param_new = _param_new,
                  .init = (bxierr_p (*) (bxilog_handler_param_p)) _init,
                  .process_log = (bxierr_p (*)(bxilog_record_p record,
                                               char * filename,
                                               char * funcname,
                                               char * loggername,
                                               char * logmsg,
                                               bxilog_handler_param_p param)) _process_log,
                  .process_ierr = (bxierr_p (*) (bxierr_p*, bxilog_handler_param_p)) _process_ierr,
                  .process_implicit_flush = (bxierr_p (*) (bxilog_handler_param_p)) _process_implicit_flush,
                  .process_explicit_flush = (bxierr_p (*) (bxilog_handler_param_p)) _process_explicit_flush,
                  .process_exit = (bxierr_p (*) (bxilog_handler_param_p)) _process_exit,
                  .process_cfg = (bxierr_p (*) (bxilog_handler_param_p)) _process_cfg,
                  .param_destroy = (bxierr_p (*) (bxilog_handler_param_p*)) _param_destroy,
};
const bxilog_handler_p BXILOG_REMOTE_HANDLER = (bxilog_handler_p) &BXILOG_REMOTE_HANDLER_S;

static const char * const _LOG_LEVEL_HEADER[] = {
        "",                             // BXILOG_OFF
        "LTFDIONWECAP",                 // BXILOG_PANIC
        "LTFDIONWECA",                  // BXILOG_ALERT
        "LTFDIONWEC",                   // BXILOG_CRITICAL
        "LTFDIONWE",                    // BXILOG_ERROR
        "LTFDIONW",                     // BXILOG_WARNING
        "LTFDION",                      // BXILOG_NOTICE
        "LTFDIO",                       // BXILOG_OUTPUT
        "LTFDI",                        // BXILOG_INFO,
        "LTFD",                         // BXILOG_DEBUG,
        "LTF",                          // BXILOG_FINE,
        "LT",                           // BXILOG_TRACE,
        "L",                            // BXILOG_LOWEST
};

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filters_p filters,
                                  va_list ap) {

    bxiassert(BXILOG_REMOTE_HANDLER == self);

    char * url = va_arg(ap, char *);
    bool bind_flag = va_arg(ap, int); // YES, THIS IS *REQUIRED* IN C99,
                                 // bool will be promoted to int
    va_end(ap);

    bxilog_remote_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    result->url = strdup(url);
    result->bind = bind_flag;
    result->ctx = NULL;
    result->zock = NULL;

    return (bxilog_handler_param_p) result;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _init(bxilog_remote_handler_param_p data) {
    // Creating the ZMQ context
    bxierr_p err = BXIERR_OK, err2;

    err2 = bxizmq_context_new(&data->ctx);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) return err;

    if (data->bind) {
        int port;
        // Creating and binding the ZMQ socket
        err2 = bxizmq_zocket_create_binded(data->ctx,
                                           ZMQ_PUB,
                                           data->url,
                                           &port,
                                           &data->zock);
        BXIERR_CHAIN(err, err2);

    } else {
        // Creating and connecting the ZMQ socket
        err2 = bxizmq_zocket_create_connected(data->ctx,
                                              ZMQ_PUB,
                                              data->url,
                                              &data->zock);
        BXIERR_CHAIN(err, err2);

    }
    data->synced = false;

    return err;
}

bxierr_p _process_exit(bxilog_remote_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    if (NULL != data->zock) {
        err2 = bxizmq_zocket_destroy(data->zock);
        BXIERR_CHAIN(err, err2);

        data->zock = NULL;
    }

    err2 = bxizmq_context_destroy(&data->ctx);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _process_implicit_flush(bxilog_remote_handler_param_p data) {
    UNUSED(data);

    return BXIERR_OK;
}

bxierr_p _process_explicit_flush(bxilog_remote_handler_param_p data) {
    return _process_implicit_flush(data);
}

bxierr_p _process_log(bxilog_record_p record,
                      char * filename,
                      char * funcname,
                      char * loggername,
                      char * logmsg,
                      bxilog_remote_handler_param_p data) {

    bxierr_p err = BXIERR_OK, err2;

    UNUSED(filename);
    UNUSED(funcname);
    UNUSED(loggername);
    UNUSED(logmsg);

    if (NULL != data->zock) {

        if (!data->synced) {
            err2 = _sync_pub_sub(data);
            BXIERR_CHAIN(err, err2);

            if (bxierr_isok(err)) data->synced = true;
        }

        char * header = bxistr_new("%s%s",
                                   BXILOG_REMOTE_HANDLER_HEADER,
                                   _LOG_LEVEL_HEADER[record->level]);

        err2 = bxizmq_str_snd_zc(header, data->zock, ZMQ_SNDMORE,
                                 0, 0, true);
        BXIERR_CHAIN(err, err2);

        size_t record_len = sizeof(*record) +\
                record->filename_len +\
                record->funcname_len +\
                record->logname_len +\
                record->logmsg_len;

        err2 = bxizmq_data_snd(record, record_len, data->zock, 0, 0, 0);
        BXIERR_CHAIN(err, err2);
    }

    return err;
}


bxierr_p _process_ierr(bxierr_p *err, bxilog_remote_handler_param_p data) {
    UNUSED(err);
    UNUSED(data);

    return BXIERR_OK;
}


bxierr_p _process_cfg(bxilog_remote_handler_param_p data) {
    UNUSED(data);

    return BXIERR_OK;
}

bxierr_p _param_destroy(bxilog_remote_handler_param_p * data_p) {
    bxilog_remote_handler_param_p data = *data_p;
    bxilog_handler_clean_param(&data->generic);

    BXIFREE(data->url);

    bximem_destroy((char**) data_p);

    return BXIERR_OK;
}

bxierr_p _sync_pub_sub(bxilog_remote_handler_param_p data) {
    fprintf(stderr, "THERE Before\n");

    char * bind_sync_url;
    bxierr_p err = BXIERR_OK, err2;

    err2 = bxizmq_generate_new_url_from(data->url, &bind_sync_url);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) goto FAILOVER;

    void * sync_socket = NULL;
    int port;

    err2 = bxizmq_zocket_create_binded(data->ctx,
                                       ZMQ_REP,
                                       bind_sync_url,
                                       &port,
                                       &sync_socket);
    BXIERR_CHAIN(err, err2);

    char * sync_url;
    if (0 == strncmp("tcp", bind_sync_url, ARRAYLEN("tcp")-1)) {
        // Replace tcp://w.x.y.z:* into tcp://w.x.y.z:port
        char * colon = strrchr(bind_sync_url + ARRAYLEN("tcp://")-1, ':');
        *colon = '\0'; // Overwrite with the NUL terminating byte
        sync_url = bxistr_new("%s:%d", bind_sync_url, port);
        BXIFREE(bind_sync_url);
    } else {
        sync_url = bind_sync_url;
    }

    err2 = bxizmq_sync_pub(data->zock,
                           sync_socket,
                           sync_url,
                           strlen(sync_url),
                           0.5);
    BXIERR_CHAIN(err, err2);
    BXIFREE(sync_url);

    if (bxierr_isok(err)) goto QUIT;

FAILOVER:
    err2 = bxitime_sleep(CLOCK_MONOTONIC, 0, 5e6);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) {
        bxierr_p dummy = bxierr_new(1057322, NULL, NULL, NULL,
                                    err,
                                    "Problem with zeromq "
                                    "PUB/SUB synchronization in %s: "
                                    "messages might be lost", INTERNAL_LOGGER_NAME);
        bxierr_report(&dummy, STDERR_FILENO);
    }

QUIT:
    {
        fprintf(stderr, "Before\n");
        bxierr_p tmp = bxizmq_zocket_destroy(sync_socket);
        bxierr_report(&tmp, STDERR_FILENO);
        fprintf(stderr, "After\n");
    }

    return BXIERR_OK;

}
