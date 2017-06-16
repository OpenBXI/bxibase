/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Sébastien Miquée <sebastien.miquee@atos.net>
 # Created on: 2016-07-27
 # Contributors: Pierre Vignéras <pierre.vigneras@atos.net>
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
#include "log_impl.h"

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
    double timeout_s;
    char * hostname;
    char * cfg_url;   // Only when bind is false
    char * ctrl_url;
    char * pub_url;
    void * ctx;
    void * cfg_zock;  // Only when bind is false
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
static bxierr_p _process_get_cfg_msg(bxilog_remote_handler_param_p data,
                                     zmq_msg_t id_frame);
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
        BXILOG_REMOTE_HANDLER_RECORD_HEADER,                             // BXILOG_OFF
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDIONWECAP",              // BXILOG_PANIC
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDIONWECA",               // BXILOG_ALERT
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDIONWEC",                // BXILOG_CRITICAL
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDIONWE",                 // BXILOG_ERROR
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDIONW",                  // BXILOG_WARNING
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDION",                   // BXILOG_NOTICE
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDIO",                    // BXILOG_OUTPUT
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFDI",                     // BXILOG_INFO,
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTFD",                      // BXILOG_DEBUG,
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LTF",                       // BXILOG_FINE,
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "LT",                        // BXILOG_TRACE,
        BXILOG_REMOTE_HANDLER_RECORD_HEADER "L",                         // BXILOG_LOWEST
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

    if (bind_flag) {
        result->cfg_url = NULL;
        result->ctrl_url = strdup(url);
    } else {
        result->cfg_url = strdup(url);
        result->ctrl_url = NULL;
    }
    result->pub_url = NULL;
    result->bind = bind_flag;
    result->timeout_s = BXILOG_REMOTE_HANDLER_SYNC_DEFAULT_TIMEOUT;
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

        DBG("Binding control zocket to %s\n", data->ctrl_url);

        // Creating and binding the ZMQ control socket
        err2 = bxizmq_zocket_create_binded(data->ctx,
                                           ZMQ_ROUTER,
                                           data->ctrl_url,
                                           &port,
                                           &data->ctrl_zock);
        BXIERR_CHAIN(err, err2);

        char * pub_url = NULL;
        err2 = bxizmq_generate_new_url_from(data->ctrl_url, &pub_url);
        BXIERR_CHAIN(err, err2);

        DBG("Binding data zocket to %s\n", data->ctrl_url);
        // Creating and binding the ZMQ data socket
        err2 = bxizmq_zocket_create_binded(data->ctx,
                                           ZMQ_PUB,
                                           pub_url,
                                           &port,
                                           &data->data_zock);
        BXIERR_CHAIN(err, err2);

        data->pub_url = bxizmq_create_url_from(pub_url, port);
        BXIFREE(pub_url);

        DBG("Data zocket binded to %s\n", data->pub_url);

    } else {
        DBG("Connecting config zocket to %s\n", data->cfg_url);
        // Creating and connecting the ZMQ config socket
        err2 = bxizmq_zocket_create_connected(data->ctx,
                                              ZMQ_DEALER,
                                              data->cfg_url,
                                              &data->cfg_zock);
        BXIERR_CHAIN(err, err2);
        if (bxierr_isko(err)) return err;

        err2 = bxizmq_zocket_create(data->ctx, ZMQ_ROUTER, &data->ctrl_zock);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_zocket_create(data->ctx, ZMQ_PUB, &data->data_zock);
        BXIERR_CHAIN(err, err2);

        DBG("Requesting urls on %s\n", data->cfg_url);
        err2 = bxizmq_str_snd(BXILOG_REMOTE_HANDLER_URLS, data->cfg_zock, 0, false, 0);
        BXIERR_CHAIN(err, err2);
        if (bxierr_isko(err)) return err;

        size_t hostnames_nb;
        size_t * hostnames_nb_p = &hostnames_nb;
        err2 = bxizmq_data_rcv((void**)&hostnames_nb_p, sizeof(*hostnames_nb_p), data->cfg_zock, 0, false, NULL);
        BXIERR_CHAIN(err, err2);

        if (1 == hostnames_nb) {
            char * hostname = NULL;
            err2 = bxizmq_str_rcv(data->cfg_zock, 0, true, &hostname);
            BXIERR_CHAIN(err, err2);
            DBG("Received localhost name: '%s'\n", hostname);
            if (NULL == hostname) return err;
            data->hostname = hostname;
        }

        size_t urls_nb;
        size_t * urls_nb_p = &urls_nb;
        err2 = bxizmq_data_rcv((void**)&urls_nb_p, sizeof(*urls_nb_p), data->cfg_zock, 0, false, NULL);
        BXIERR_CHAIN(err, err2);

        bxiassert(1 == urls_nb);

        for (size_t i = 0; i < urls_nb; i++) {
            char * url = NULL;
            err2 = bxizmq_str_rcv(data->cfg_zock, 0, true, &url);
            BXIERR_CHAIN(err, err2);
            DBG("Received control zocket url: '%s'\n", url);
            if (NULL == url) return err;
            data->ctrl_url = url;
            DBG("Connecting control zocket to %s\n", data->ctrl_url);
            err2 = bxizmq_zocket_connect(data->ctrl_zock, data->ctrl_url);
            BXIERR_CHAIN(err, err2);
        }
        for (size_t i = 0; i < urls_nb - 1; i++) {
            char * url = NULL;
            err2 = bxizmq_str_rcv(data->cfg_zock, 0, true, &url);
            BXIERR_CHAIN(err, err2);
            DBG("Received data zocket url: '%s'\n", url);
            if (NULL == url) return err;
            data->pub_url = url;
            DBG("Connecting data zocket to %s\n", data->pub_url);
            err2 = bxizmq_zocket_connect(data->data_zock, data->pub_url);
            BXIERR_CHAIN(err, err2);
        }
        // Last frame:
        char * url = NULL;
        err2 = bxizmq_str_rcv(data->cfg_zock, 0, true, &url);
        BXIERR_CHAIN(err, err2);
        DBG("Received data zocket url: '%s'\n", url);
        if (NULL == url) return err;
        data->pub_url = url;
        DBG("Connecting data zocket to %s\n", data->pub_url);
        err2 = bxizmq_zocket_connect(data->data_zock, data->pub_url);
        BXIERR_CHAIN(err, err2);

        bxierr_p tmp = _sync_pub(data);
        if (bxierr_isko(tmp)) bxierr_report(&tmp, STDERR_FILENO);
    }
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

    // Inform potential receiver that we are exiting
    const char * header =  BXILOG_REMOTE_HANDLER_EXITING_HEADER;

    err2 = bxizmq_str_snd_zc(header, data->data_zock, ZMQ_SNDMORE,
                             0, 0, false);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_str_snd(data->pub_url, data->data_zock, 0, 0, 0);
    BXIERR_CHAIN(err, err2);

    if (NULL != data->cfg_zock) {
        err2 = bxizmq_zocket_destroy(&data->cfg_zock);
        BXIERR_CHAIN(err, err2);
    }

    if (NULL != data->ctrl_zock) {
        err2 = bxizmq_zocket_destroy(&data->ctrl_zock);
        BXIERR_CHAIN(err, err2);
    }

    // Prevent message lost!
//    int linger = -1;
    int linger = -1;
    int rc = zmq_setsockopt(data->data_zock, ZMQ_LINGER, &linger, sizeof(linger));
    if (rc != 0) {
        err2 = bxizmq_err(errno, "Can't set linger for socket %p", data->data_zock);
        BXIERR_CHAIN(err, err2);
    }

    if (NULL != data->data_zock) {
        err2 = bxizmq_zocket_destroy(&data->data_zock);
        BXIERR_CHAIN(err, err2);
    }

    err2 = bxizmq_context_destroy(&data->ctx);
    BXIERR_CHAIN(err, err2);

    BXIFREE(data->pub_url);
    BXIFREE(data->generic.private_items);
    BXIFREE(data->generic.cbs);

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

    const char * header =  _LOG_LEVEL_HEADER[record->level];

    err2 = bxizmq_str_snd_zc(header, data->data_zock, ZMQ_SNDMORE,
                             0, 0, false);
    BXIERR_CHAIN(err, err2);

    size_t record_len = sizeof(*record) +\
            record->filename_len +\
            record->funcname_len +\
            record->logname_len +\
            record->logmsg_len;

    err2 = bxizmq_data_snd(record, record_len, data->data_zock, 0, 0, 0);
    BXIERR_CHAIN(err, err2);

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

    BXIFREE(data->ctrl_url);
    BXIFREE(data->hostname);

    bximem_destroy((char**) data_p);

    return BXIERR_OK;
}

bxierr_p _process_ctrl_msg(bxilog_remote_handler_param_p data, int revent) {
    bxiassert(NULL != data);

    bxierr_p err = BXIERR_OK, err2;

    if (revent & ZMQ_POLLIN) {
        zmq_msg_t id_frame;
        err2 = bxizmq_msg_init(&id_frame);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_msg_rcv(data->ctrl_zock, &id_frame, ZMQ_DONTWAIT);
        BXIERR_CHAIN(err, err2);

        char * msg = NULL;
        err2 = bxizmq_str_rcv(data->ctrl_zock, 0, true, &msg);
        BXIERR_CHAIN(err, err2);

        if (0 == strncmp(BXILOG_REMOTE_HANDLER_URLS, msg,
                         ARRAYLEN(BXILOG_REMOTE_HANDLER_URLS) - 1)) {
            DBG("URLs requested\n");
            err2 = bxizmq_msg_snd(&id_frame, data->ctrl_zock, ZMQ_SNDMORE, 0, 0);
            BXIERR_CHAIN(err, err2);
            err2 = bxizmq_str_snd_zc(data->pub_url, data->ctrl_zock, 0, 0, 0, false);
            BXIERR_CHAIN(err, err2);
        } else if (0 == strncmp(BXILOG_REMOTE_HANDLER_CFG_CMD, msg,
                                ARRAYLEN(BXILOG_REMOTE_HANDLER_CFG_CMD) - 1)) {

            DBG("Configuration requested\n");
            err2 = _process_get_cfg_msg(data, id_frame);
            BXIERR_CHAIN(err, err2);

        } else {
            DBG("Bad control message received: %s\n", msg);
        }

    }

    return err;
}

bxierr_p _process_get_cfg_msg(bxilog_remote_handler_param_p data, zmq_msg_t id_frame) {
    bxiassert(NULL != data);

    bxierr_p err = BXIERR_OK, err2;

    err2 = bxizmq_msg_snd(&id_frame, data->ctrl_zock, ZMQ_SNDMORE, 0, 0);
    BXIERR_CHAIN(err, err2);

    bxilog_logger_p * loggers = NULL;
    size_t loggers_nb = bxilog_registry_getall(&loggers);

    // We will create the JSON string. It is made of several parts:
    // '{'
    // 'global': ...
    // 'handler-0': ...
    // ...
    // 'handler-N': ...
    // 'logger-0': ...
    // 'logger-N: ...
    // '}'
    char * global_str = bxistr_new("{"
            "\"ctrl_url\": \"%s\", "
            "\"pub_url\": \"%s\", "
            "\"pid\": %d, "
            "\"progname\": \"%s\", "
            "\"state\": %d, "
            "\"handlers_nb\": %zu, "
            "\"loggers_nb\": %zu, "
            "\"ctrl_hwm\": %d, "
            "\"data_hwm\": %d, "
            "\"buf_size\": %zu"
            "}",
            data->ctrl_url,
            data->pub_url,
            BXILOG__GLOBALS->pid,
            BXILOG__GLOBALS->config->progname,
            BXILOG__GLOBALS->state,
            BXILOG__GLOBALS->internal_handlers_nb,
            loggers_nb,
            BXILOG__GLOBALS->config->ctrl_hwm,
            BXILOG__GLOBALS->config->data_hwm,
            BXILOG__GLOBALS->config->tsd_log_buf_size);

    char * handlers_str_parts[BXILOG__GLOBALS->internal_handlers_nb];
    size_t handlers_str_parts_len[BXILOG__GLOBALS->internal_handlers_nb];
    for (size_t i = 0; i < BXILOG__GLOBALS->internal_handlers_nb; i++) {
        bxilog_handler_param_p param = BXILOG__GLOBALS->config->handlers_params[i];
        bxilog_filters_p filters = param->filters;
        char * filters_str_parts[filters->nb];
        size_t filters_str_parts_len[filters->nb];

        for (size_t f = 0; f < filters->nb; f++) {
            bxilog_filter_p filter = filters->list[f];
            filters_str_parts[f] = bxistr_new("%s:%u", filter->prefix, filter->level);
            filters_str_parts_len[f] = strlen(filters_str_parts[f]);
        }
        char * filters_str = NULL;
        bxistr_join(", ", strlen(", "),
                    filters_str_parts, filters_str_parts_len,
                    filters->nb,
                    &filters_str);

        handlers_str_parts[i] = bxistr_new("{"
                            "\"name\": \"%s\", "
                            "\"filters\": \"%s\", "
                            "\"status\": %d, "
                            "\"flush_freq_ms\": %lu, "
                            "\"ierr_max\": %zu, "
                            "\"ctrl_hwm\": %d, "
                            "\"data_hwm\": %d"
                            "}",
                            BXILOG__GLOBALS->config->handlers[i]->name,
                            filters_str,
                            param->status,
                            param->flush_freq_ms,
                            param->ierr_max,
                            param->ctrl_hwm,
                            param->data_hwm);
        handlers_str_parts_len[i] = strlen(handlers_str_parts[i]);

        for (size_t f = 0; f < filters->nb; f++) {
            BXIFREE(filters_str_parts[f]);
        }
        BXIFREE(filters_str);
    }
    char * handlers_str = NULL;
    bxistr_join(", ", strlen(", "),
                handlers_str_parts, handlers_str_parts_len,
                BXILOG__GLOBALS->internal_handlers_nb,
                &handlers_str);

    char * loggers_str_parts[loggers_nb];
    size_t loggers_str_parts_len[loggers_nb];
    for (size_t i = 0; i < loggers_nb; i++) {
        loggers_str_parts[i] = bxistr_new("{"
                                "\"name\": \"%s\", "
                                "\"allocated\": %s, "
                                "\"level\": %d"
                                "}",
                                loggers[i]->name,
                                loggers[i]->allocated ? "true" : "false",
                                loggers[i]->level);
        loggers_str_parts_len[i] = strlen(loggers_str_parts[i]);
    }

    char * loggers_str = NULL;
    bxistr_join(", ", strlen(", "),
                loggers_str_parts, loggers_str_parts_len,
                loggers_nb,
                &loggers_str);

    // Now we join all parts together
    char * json_str = bxistr_new("{"
            "\"global\": %s, "
            "\"handlers\": [%s], "
            "\"loggers\": [%s] "
            "}",
            global_str,
            handlers_str,
            loggers_str);

    DBG("Sending: %s", json_str);
    err2 = bxizmq_str_snd(json_str, data->ctrl_zock, 0, 0, 0);
    BXIERR_CHAIN(err, err2);

    BXIFREE(global_str);
    BXIFREE(handlers_str);
    for (size_t i = 0; i < BXILOG__GLOBALS->internal_handlers_nb; i++) {
        BXIFREE(handlers_str_parts[i]);
    }

    BXIFREE(loggers_str);
    for (size_t i = 0; i < loggers_nb; i++) {
        BXIFREE(loggers_str_parts[i]);
    }
    BXIFREE(json_str);

    BXIFREE(loggers);

    return err;

}

bxierr_p _sync_pub(bxilog_remote_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    char * url;
    if ((0 == strncmp("tcp", data->pub_url, ARRAYLEN("tcp") - 1))
        && (NULL != data->hostname)) {
        url = bxistr_new("tcp://%s:*", data->hostname);
    } else {
        err2 = bxizmq_generate_new_url_from(data->pub_url, &url);
        BXIERR_CHAIN(err, err2);
    }
    if (bxierr_isko(err)) return err;

    void * sync_zock = NULL;
    int port;
    err2 = bxizmq_zocket_create_binded(data->ctx, ZMQ_REP, url, &port, &sync_zock);
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) return err;

    char * actual_url = bxizmq_create_url_from(url, port);
    DBG("Syncing on '%s' '%s' '%s'\n", data->pub_url, url, actual_url);
    err2 = bxizmq_sync_pub(data->data_zock,
                           sync_zock,
                           actual_url,
                           strlen(actual_url),
                           data->timeout_s);
    BXIERR_CHAIN(err, err2);
    BXIFREE(url);
    BXIFREE(actual_url);
    err2 = bxizmq_zocket_destroy(&sync_zock);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) {
        bxierr_p dummy = bxierr_new(1057322, NULL, NULL, NULL,
                                    err,
                                    "Problem with zeromq "
                                    "PUB synchronization in %s: "
                                    "messages might be lost", INTERNAL_LOGGER_NAME);
        bxierr_report(&dummy, STDERR_FILENO);
        err = BXIERR_OK;
    }

    return err;

}
