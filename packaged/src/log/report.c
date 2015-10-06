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


#include <stdarg.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

#include "config_impl.h"
#include "log_impl.h"
#include "handler_impl.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************

static void _report_err(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                        char * file, size_t filelen,
                        const char * func, size_t funclen,
                        int line,
                        const char * fmt, va_list arglist);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

void bxilog_report(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line,
                   const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _report_err(logger, level,
                err, file, filelen, func, funclen, line, fmt, ap);
    va_end(ap);
    bxierr_destroy(err);
    *err = BXIERR_OK;
}

void bxilog_report_keep(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                        char * file, size_t filelen,
                        const char * func, size_t funclen,
                        int line,
                        const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _report_err(logger, level,
                err, file, filelen, func, funclen, line, fmt, ap);
    va_end(ap);
}



//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

void _report_err(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                 char * file, size_t filelen,
                 const char * func, size_t funclen,
                 int line,
                 const char * fmt, va_list arglist) {

    if (*err == NULL) return;
    if (bxierr_isok(*err)) return;
    if (!bxilog_is_enabled_for(logger, level)) return ;

    char * msg;
    bxistr_vnew(&msg, fmt, arglist);
    char * err_str = bxierr_str(*err);;
    bxierr_p logerr = bxilog_log_nolevelcheck(logger, level,
                                              file, filelen,
                                              func, funclen, line,
                                              "%s: %s", msg, err_str);
    BXIFREE(msg);
    BXIFREE(err_str);
    bxierr_report(&logerr, STDERR_FILENO);
}

