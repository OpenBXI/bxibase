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
    size_t sub_nb;
    double timeout_s;
    char * url;
    void * ctx;
    void * ctrl_zock;
    void * data_zock;

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
static bxierr_p _process_ctrl_msg(bxilog_remote_handler_param_p data, int revent);
static bxierr_p _sync_pub(bxilog_remote_handler_param_p data);

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
    size_t sub_nb = va_arg(ap, size_t);
    va_end(ap);

    bxilog_remote_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    result->url = strdup(url);
    result->bind = bind_flag;
    result->sub_nb = sub_nb;
    result->timeout_s = 1;
    result->ctx = NULL;
    result->ctrl_zock = NULL;
    result->data_zock = NULL;

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

        // Creating and binding the ZMQ control socket
        err2 = bxizmq_zocket_create_binded(data->ctx,
                                           ZMQ_ROUTER,
                                           data->url,
                                           &port,
                                           &data->ctrl_zock);
        BXIERR_CHAIN(err, err2);

        char * pub_url = NULL;
        err2 = bxizmq_generate_new_url_from(data->url, &pub_url);
        BXIERR_CHAIN(err, err2);

        // Creating and binding the ZMQ data socket
        err2 = bxizmq_zocket_create_binded(data->ctx,
                                           ZMQ_PUB,
                                           pub_url,
                                           &port,
                                           &data->data_zock);
        BXIERR_CHAIN(err, err2);

    } else {
        // Creating and connecting the ZMQ control socket
        err2 = bxizmq_zocket_create_connected(data->ctx,
                                              ZMQ_DEALER,
                                              data->url,
                                              &data->ctrl_zock);
        BXIERR_CHAIN(err, err2);

        // Creating and connecting the ZMQ data socket
        err2 = bxizmq_zocket_create_connected(data->ctx,
                                              ZMQ_PUB,
                                              data->url,
                                              &data->data_zock);
        BXIERR_CHAIN(err, err2);

    }
    data->synced = false;
    data->generic.private_items_nb = 1;
    data->generic.private_items = bximem_calloc(data->generic.private_items_nb * \
                                                sizeof(*data->generic.private_items));
    data->generic.private_items[0].socket = data->ctrl_zock;
    data->generic.private_items[0].events = ZMQ_POLLIN;
    data->generic.cbs = bximem_calloc(data->generic.private_items_nb * \
                                      sizeof(*data->generic.cbs));
    data->generic.cbs[0] = (bxilog_handler_cbs) _process_ctrl_msg;

    return err;
}

bxierr_p _process_exit(bxilog_remote_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    if (NULL != data->ctrl_zock) {
        err2 = bxizmq_zocket_destroy(data->ctrl_zock);
        BXIERR_CHAIN(err, err2);

        data->ctrl_zock = NULL;
    }

    if (NULL != data->data_zock) {
        err2 = bxizmq_zocket_destroy(data->data_zock);
        BXIERR_CHAIN(err, err2);

        data->data_zock = NULL;
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

    if (NULL != data->data_zock) {

        if (!data->synced) {
            bxierr_p tmp = _sync_pub(data);
            if (bxierr_isko(tmp)) bxierr_report(&tmp, STDERR_FILENO);
            data->synced = true;
        }

        char * header = bxistr_new("%s%s",
                                   BXILOG_REMOTE_HANDLER_HEADER,
                                   _LOG_LEVEL_HEADER[record->level]);

        err2 = bxizmq_str_snd_zc(header, data->data_zock, ZMQ_SNDMORE,
                                 0, 0, true);
        BXIERR_CHAIN(err, err2);

        size_t record_len = sizeof(*record) +\
                record->filename_len +\
                record->funcname_len +\
                record->logname_len +\
                record->logmsg_len;

        err2 = bxizmq_data_snd(record, record_len, data->data_zock, 0, 0, 0);
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

bxierr_p _process_ctrl_msg(bxilog_remote_handler_param_p data, int revent) {
    bxiassert(NULL != data);

    fprintf(stderr, "CALLING _PROCESS CTRL MSG: %d\n", revent);

    return BXIERR_OK;
}

bxierr_p _sync_pub(bxilog_remote_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    err2 = bxizmq_sync_pub(data->data_zock,
                           data->data_zock,
                           data->url,
                           data->sub_nb,
                           data->timeout_s);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) {
        bxierr_p dummy = bxierr_new(1057322, NULL, NULL, NULL,
                                    err,
                                    "Problem with zeromq "
                                    "PUB/SUB synchronization in %s: "
                                    "messages might be lost", INTERNAL_LOGGER_NAME);
        bxierr_report(&dummy, STDERR_FILENO);
    }

    return err;

}
