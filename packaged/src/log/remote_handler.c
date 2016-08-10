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
    int port;
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

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filters_p filters,
                                  va_list ap) {

    bxiassert(BXILOG_REMOTE_HANDLER == self);

    char * url = va_arg(ap, char *);
    va_end(ap);

    bxilog_remote_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    result->url = strdup(url);
    result->ctx = NULL;
    result->zock = NULL;

    return (bxilog_handler_param_p) result;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _init(bxilog_remote_handler_param_p data) {
    // Creating the ZMQ context
    bxierr_p err = bxizmq_context_new(&data->ctx);

    if (bxierr_isko(err)) {
      return err;
    }

    // Creating and binding the ZMQ socket
    err = bxizmq_zocket_bind(data->ctx,
                             ZMQ_PUB,
                             data->url,
                             &data->port,
                             &data->zock);

    return err;
}

bxierr_p _process_exit(bxilog_remote_handler_param_p data) {
    bxierr_p err;
    int linger = 100;

    if(NULL != data->zock) {
      err = bxizmq_zocket_setopt(data->zock, ZMQ_LINGER, &linger, sizeof(int));

      if (BXIERR_OK != err) {
          return err;
      }

      err = bxizmq_zocket_destroy(data->zock);
      if (BXIERR_OK != err) {
          return err;
      }
    }

    err = bxizmq_context_destroy(&data->ctx);

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

    if (NULL != data->zock) {
        err2 = bxizmq_data_snd(record, sizeof(*record), data->zock,
                               ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_data_snd(filename, record->filename_len, data->zock,
                               ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_data_snd(funcname, record->funcname_len, data->zock,
                               ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_data_snd(loggername, record->logname_len, data->zock,
                               ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);

        err2 = bxizmq_data_snd(logmsg, record->logmsg_len, data->zock,
                               0, 0, 0);
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
