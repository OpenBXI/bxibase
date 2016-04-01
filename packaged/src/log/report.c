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
#include <string.h>

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

static void _report_err(bxilog_logger_p logger, bxilog_level_e level, bxierr_p * err_p,
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

void bxilog_report(bxilog_logger_p logger, bxilog_level_e level, bxierr_p * err,
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

void bxilog_report_keep(bxilog_logger_p logger, bxilog_level_e level, bxierr_p * err,
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


void bxilog_report_raw(bxierr_report_p self,
                       bxilog_logger_p logger, bxilog_level_e level,
                       char * file, size_t filelen,
                       const char * func, size_t funclen,
                       int line,
                       const char * msg, size_t msglen) {

    bxierr_p logerr = bxilog_logger_log_rawstr(logger, level,
                                               file, filelen,
                                               func, funclen, line,
                                               msg, msglen);
    bxierr_report(&logerr, STDERR_FILENO);

    for (size_t i = 0; i < self->err_nb; i++) {
        bxierr_p logerr = bxilog_logger_log_rawstr(logger, level,
                                                   file, filelen,
                                                   func, funclen, line,
                                                   self->err_msgs[i],
                                                   self->err_msglens[i]);
        bxierr_report(&logerr, STDERR_FILENO);

        logerr = bxilog_logger_log_rawstr(logger, BXILOG_TRACE,
                                          file, filelen,
                                          func, funclen, line,
                                          self->err_bts[i],
                                          self->err_btslens[i]);
        bxierr_report(&logerr, STDERR_FILENO);
    }
}




//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

void _report_err(bxilog_logger_p logger, bxilog_level_e level, bxierr_p * err_p,
                 char * file, size_t filelen,
                 const char * func, size_t funclen,
                 int line,
                 const char * fmt, va_list arglist) {

    if (*err_p == NULL) return;
    if (bxierr_isok(*err_p)) return;
    if (!bxilog_logger_is_enabled_for(logger, level)) return;

    char * msg;
    size_t len = bxistr_vnew(&msg, fmt, arglist);

    if (INITIALIZED != BXILOG__GLOBALS->state) {
        char * err_str = bxierr_str(*err_p);
        char * out = bxistr_new("%s\n%s\n"
                                "(The BXI logging library is not initialized: "
                                "the above message is raw displayed "
                                "on stderr. Especially, it won't appear in the "
                                "expected logging file.)\n", msg, err_str);
        bxilog_rawprint(out, STDERR_FILENO);
        BXIFREE(out);
        BXIFREE(err_str);
    } else {
        bxierr_report_p report = bxierr_report_new();
        bxierr_report_add_from_limit(*err_p, report, BXIERR_ALL_CAUSES);
        bxilog_report_raw(report, logger, level,
                          file, filelen,
                          func, funclen, line,
                          msg, len+1);
        bxierr_report_destroy(&report);

    }
    BXIFREE(msg);
}

