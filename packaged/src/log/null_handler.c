/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"

#include "bxi/base/log.h"

#include "bxi/base/log/null_handler.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define INTERNAL_LOGGER_NAME "bxi.base.log.handler.null"

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                         bxilog_filter_p * filters,
                                         va_list ap);
static bxierr_p _init(bxilog_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_handler_param_p data);
static bxierr_p _process_err(bxierr_p * err, bxilog_handler_param_p data);
static bxierr_p _process_implicit_flush(bxilog_handler_param_p data);
static bxierr_p _process_explicit_flush(bxilog_handler_param_p data);
static bxierr_p _process_exit(bxilog_handler_param_p data);
static bxierr_p _process_cfg(bxilog_handler_param_p data);
static bxierr_p _param_destroy(bxilog_handler_param_p *data_p);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

static const bxilog_handler_s BXILOG_NULL_HANDLER_S = {
                  .name = "BXI Logging Null Handler",
                  .param_new = _param_new,
                  .init = _init,
                  .process_log = _process_log,
                  .process_err = _process_err,
                  .process_implicit_flush = _process_implicit_flush,
                  .process_explicit_flush = _process_explicit_flush,
                  .process_exit = _process_exit,
                  .process_cfg = _process_cfg,
                  .param_destroy = _param_destroy,
};
const bxilog_handler_p BXILOG_NULL_HANDLER = (bxilog_handler_p) &BXILOG_NULL_HANDLER_S;

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filter_p * filters,
                                  va_list ap) {

    bxiassert(BXILOG_NULL_HANDLER == self);
    UNUSED(ap);

    bxilog_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, result);

    return result;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _init(bxilog_handler_param_p data) {
    UNUSED(data);
    return BXIERR_OK;
}

bxierr_p _process_exit(bxilog_handler_param_p data) {
    UNUSED(data);
    return BXIERR_OK;
}

bxierr_p _process_implicit_flush(bxilog_handler_param_p data) {
    UNUSED(data);
    return BXIERR_OK;
}

bxierr_p _process_explicit_flush(bxilog_handler_param_p data) {
    UNUSED(data);
    return BXIERR_OK;
}

bxierr_p _process_log(bxilog_record_p record,
                      char * filename,
                      char * funcname,
                      char * loggername,
                      char * logmsg,
                      bxilog_handler_param_p data) {

    UNUSED(record);
    UNUSED(filename);
    UNUSED(funcname);
    UNUSED(loggername);
    UNUSED(logmsg);
    UNUSED(data);

    return BXIERR_OK;
}


bxierr_p _process_err(bxierr_p *err, bxilog_handler_param_p data) {
    UNUSED(err);
    UNUSED(data);

    return BXIERR_OK;
}


bxierr_p _process_cfg(bxilog_handler_param_p data) {
    UNUSED(data);
    return BXIERR_OK;
}

bxierr_p _param_destroy(bxilog_handler_param_p * data_p) {
    bxilog_handler_clean_param(*data_p);
    bximem_destroy((char**) data_p);
    return BXIERR_OK;
}


