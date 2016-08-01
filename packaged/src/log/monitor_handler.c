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


#include <zmq.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"

#include "bxi/base/log.h"

#include "bxi/base/log/monitor_handler.h"


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define INTERNAL_LOGGER_NAME BXILOG_LIB_PREFIX "bxilog.handler.monitor"

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

typedef struct bxilog_monitor_handler_param_s_f * bxilog_monitor_handler_param_p;
typedef struct bxilog_monitor_handler_param_s_f {
  bxilog_handler_param_s generic;
  char * url;
  //pid_t pid;
  void * ctx;
  void * zock;
} bxilog_monitor_handler_param_s;


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                         bxilog_filters_p filters,
                                         va_list ap);
static bxierr_p _init(bxilog_monitor_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_monitor_handler_param_p data);
static bxierr_p _process_ierr(bxierr_p * err, bxilog_monitor_handler_param_p data);
static bxierr_p _process_implicit_flush(bxilog_monitor_handler_param_p data);
static bxierr_p _process_explicit_flush(bxilog_monitor_handler_param_p data);
static bxierr_p _process_exit(bxilog_monitor_handler_param_p data);
static bxierr_p _process_cfg(bxilog_monitor_handler_param_p data);
static bxierr_p _param_destroy(bxilog_monitor_handler_param_p *data_p);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

static const bxilog_handler_s BXILOG_MONITOR_HANDLER_S = {
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
const bxilog_handler_p BXILOG_MONITOR_HANDLER = (bxilog_handler_p) &BXILOG_MONITOR_HANDLER_S;

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filters_p filters,
                                  va_list ap) {

    bxiassert(BXILOG_MONITOR_HANDLER == self);

    char * url = va_arg(ap, char *);
    va_end(ap);

    bxilog_monitor_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    result->url = url;
    result->ctx = NULL;
    result->zock = NULL;

    return (bxilog_handler_param_p) result;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _init(bxilog_monitor_handler_param_p data) {
    //data->pid = getpid();

    // Creating the ZMQ context
    data->ctx = zmq_ctx_new();

    if (NULL == data->ctx) {
      return bxierr_errno("ZMQ context creation failed");
    }

    // Creating the ZMQ socket
    data->zock = zmq_socket(data->ctx, ZMQ_PUB);
    if (NULL == data->zock) {
      return bxierr_errno("ZMQ socket creation failed");
    }

    // Binding the zocket
    int rc = zmq_bind(data->zock, data->url);
    if (-1 == rc) {
      return bxierr_errno("ZMQ socket binding failed on url '%s'", data->url);
    }

    return BXIERR_OK;
}

bxierr_p _process_exit(bxilog_monitor_handler_param_p data) {
    int rc;
    int linger = 100;

    if(NULL != data->zock) {
      rc = zmq_setsockopt(data->zock, ZMQ_LINGER, &linger, sizeof(int));
      if (-1 == rc) {
        return bxierr_errno("Unable to change linger on ZMQ socket");
      }
      rc = zmq_close(data->zock);
      if (-1 == rc) {
        return bxierr_errno("Unable to close ZMQ socket");
      }
    }

    if(NULL != data->ctx) {
      rc = zmq_ctx_shutdown(data->ctx);
      if (-1 == rc) {
        return bxierr_errno("Unable to shutdown ZMQ context");
      }
      rc = zmq_ctx_term(data->ctx);
      if (-1 == rc) {
        return bxierr_errno("Unable to terminate ZMQ context");
      }
    }

    return BXIERR_OK;
}

bxierr_p _process_implicit_flush(bxilog_monitor_handler_param_p data) {
    UNUSED(data);

    return BXIERR_OK;
}

bxierr_p _process_explicit_flush(bxilog_monitor_handler_param_p data) {
    UNUSED(data);

    return BXIERR_OK;
}

bxierr_p _process_log(bxilog_record_p record,
                      char * filename,
                      char * funcname,
                      char * loggername,
                      char * logmsg,
                      bxilog_monitor_handler_param_p data) {

    UNUSED(filename);
    UNUSED(funcname);
    UNUSED(loggername);
    UNUSED(logmsg);

    int rc;

    if (NULL != data->zock) {
      rc = zmq_send(data->zock, (void *) record, sizeof(record), 0);

      if (-1 == rc) {
        return bxierr_errno("Unable to send the log record through the socket");
      }
    }

    return BXIERR_OK;
}


bxierr_p _process_ierr(bxierr_p *err, bxilog_monitor_handler_param_p data) {
    UNUSED(err);
    UNUSED(data);

    return BXIERR_OK;
}


bxierr_p _process_cfg(bxilog_monitor_handler_param_p data) {
    UNUSED(data);

    return BXIERR_OK;
}

bxierr_p _param_destroy(bxilog_monitor_handler_param_p * data_p) {
    bxilog_monitor_handler_param_p data = *data_p;
    bxilog_handler_clean_param(&data->generic);

    bximem_destroy((char**) data_p);

    return BXIERR_OK;
}
