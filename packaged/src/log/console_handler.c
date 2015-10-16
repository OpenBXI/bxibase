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

#include <unistd.h>
#include <syscall.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"

#include "bxi/base/log.h"

#include "bxi/base/log/console_handler.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define INTERNAL_LOGGER_NAME "bxi.base.log.handler.console"

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************
typedef struct bxilog_console_handler_param_s_f * bxilog_console_handler_param_p;
typedef struct bxilog_console_handler_param_s_f {
    bxilog_handler_param_s generic;

    pid_t pid, tid;
    uint16_t thread_rank;

    bxilog_level_e stderr_level;
    bxierr_list_p errset;
    size_t lost_logs;
} bxilog_console_handler_param_s;

typedef struct {
    const bxilog_console_handler_param_p data;
    const bxilog_record_p record;
    const char * filename;
    const char *funcname;
    const char * loggername;
    const char *logmsg;
} log_single_line_param_s;

typedef log_single_line_param_s * log_single_line_param_p;

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                         bxilog_filter_p * filter,
                                         va_list ap);
static bxierr_p _init(bxilog_console_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_console_handler_param_p data);
static bxierr_p _process_err(bxierr_p * err, bxilog_console_handler_param_p data);
static bxierr_p _process_implicit_flush(bxilog_console_handler_param_p data);
static bxierr_p _process_explicit_flush(bxilog_console_handler_param_p data);
static bxierr_p _process_exit(bxilog_console_handler_param_p data);
static bxierr_p _process_cfg(bxilog_console_handler_param_p data);
static bxierr_p _param_destroy(bxilog_console_handler_param_p *data_p);

static bxierr_p _sync(bxilog_console_handler_param_p data);

static bxierr_p _internal_log_func(bxilog_level_e level,
                                   bxilog_console_handler_param_p data,
                                   const char * funcname,
                                   size_t funclen,
                                   int line_nb,
                                   const char * fmt, ...);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

static const bxilog_handler_s BXILOG_CONSOLE_HANDLER_S = {
                  .name = "BXI Logging Console Handler",
                  .param_new = _param_new,
                  .init = (bxierr_p (*) (bxilog_handler_param_p)) _init,
                  .process_log = (bxierr_p (*)(bxilog_record_p record,
                                               char * filename,
                                               char * funcname,
                                               char * loggername,
                                               char * logmsg,
                                               bxilog_handler_param_p param)) _process_log,
                  .process_err = (bxierr_p (*) (bxierr_p*, bxilog_handler_param_p)) _process_err,
                  .process_implicit_flush = (bxierr_p (*) (bxilog_handler_param_p)) _process_implicit_flush,
                  .process_explicit_flush = (bxierr_p (*) (bxilog_handler_param_p)) _process_explicit_flush,
                  .process_exit = (bxierr_p (*) (bxilog_handler_param_p)) _process_exit,
                  .process_cfg = (bxierr_p (*) (bxilog_handler_param_p)) _process_cfg,
                  .param_destroy = (bxierr_p (*) (bxilog_handler_param_p*)) _param_destroy,
};
const bxilog_handler_p BXILOG_CONSOLE_HANDLER = (bxilog_handler_p) &BXILOG_CONSOLE_HANDLER_S;

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filter_p * filters,
                                  va_list ap) {

    bxiassert(BXILOG_CONSOLE_HANDLER == self);

    bxilog_level_e level = (bxilog_level_e) va_arg(ap, int);
    va_end(ap);

    bxilog_console_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    result->stderr_level = level;

    return (bxilog_handler_param_p) result;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _init(bxilog_console_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    data->pid = getpid();
 #ifdef __linux__
     data->tid = (pid_t) syscall(SYS_gettid);
 #endif
     // TODO: find something better for the rank of the IHT
     // Maybe, define already the related string instead of
     // a rank number?
     data->thread_rank = (uint16_t) pthread_self();

    if (data->stderr_level > BXILOG_LOWEST) {
        err2 = bxierr_gen("Bad stderr level value '%d', must be between [%d, %d]",
                          data->stderr_level, BXILOG_PANIC, BXILOG_LOWEST);
        BXIERR_CHAIN(err, err2);
    }
    data->errset = bxierr_list_new();
    data->lost_logs = 0;

    return err;
}

bxierr_p _process_exit(bxilog_console_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    err2 = _sync(data);
    BXIERR_CHAIN(err, err2);

    if (data->lost_logs > 0) {
        char * str = bxistr_new("BXI Log File Handler Error Summary:\n"
                                "\tNumber of lost log lines: %zu\n"
                                "\tNumber of reported distinct errors: %zu\n",
                                data->lost_logs,
                                data->errset->errors_nb);
        bxilog_rawprint(str, STDERR_FILENO);
        BXIFREE(str);
    }

    return err;
}

inline bxierr_p _process_implicit_flush(bxilog_console_handler_param_p data) {
    UNUSED(data);
    return _sync(data);
}

inline bxierr_p _process_explicit_flush(bxilog_console_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

//    err2 = _ilog(BXILOG_TRACE, data, "Flushing requested");
//    BXIERR_CHAIN(err, err2);

    err2 = _process_implicit_flush(data);
    BXIERR_CHAIN(err, err2);

//    err2 = _ilog(BXILOG_TRACE, data, "Flushed");
    BXIERR_CHAIN(err, err2);

    return err;
}

inline bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_console_handler_param_p data) {

    UNUSED(filename);
    UNUSED(funcname);
    UNUSED(loggername);

    FILE * out = (record->level > data->stderr_level) ? stdout : stderr;

    errno = 0;
    int rc = fprintf(out, "%s\n", logmsg);
    if (rc < 0) {
        return bxierr_errno("Calling fprintf() failed");
    }

    return BXIERR_OK;
}


bxierr_p _process_err(bxierr_p *err, bxilog_console_handler_param_p data) {
    bxierr_p result = BXIERR_OK;

    if (bxierr_isok(*err)) return *err;

    char * str = bxierr_str(*err);
    bxierr_p fatal = _ilog(BXILOG_ERROR, data, "An error occured:\n %s", str);

    if (bxierr_isko(fatal)) {
        result = bxierr_new(BXILOG_HANDLER_EXIT_CODE, fatal, NULL, NULL, NULL,
                            "Fatal: error while processing error: %s", str);
    } else {
        bxierr_destroy(err);
    }
    BXIFREE(str);

    return result;
}


bxierr_p _process_cfg(bxilog_console_handler_param_p data) {
    UNUSED(data);
    bxiunreachable_statement;
    return BXIERR_OK;
}

inline bxierr_p _param_destroy(bxilog_console_handler_param_p * data_p) {
    bxilog_console_handler_param_p data = *data_p;

    bxilog_handler_clean_param(&data->generic);

    bxierr_list_destroy(&data->errset);
    bximem_destroy((char**) data_p);
    return BXIERR_OK;
}


bxierr_p _sync(bxilog_console_handler_param_p data) {
    UNUSED(data);

    bxierr_p err = BXIERR_OK, err2;

    errno = 0;
    int rc = fflush(stderr);
    if (0 != rc) {
        err2 = bxierr_errno("Calling fflush(stderr) failed");
        BXIERR_CHAIN(err, err2);
    }

    errno = 0;
    rc = fflush(stdout);
    if (0 != rc) {
        err2 = bxierr_errno("Calling fflush(stdout) failed");
        BXIERR_CHAIN(err, err2);
    }

    return err;
}


bxierr_p _internal_log_func(bxilog_level_e level,
                            bxilog_console_handler_param_p data,
                            const char * funcname,
                            size_t funclen,
                            int line_nb,
                            const char * fmt, ...) {

    bxierr_p err = BXIERR_OK, err2;

    va_list ap;
    char *msg;

    va_start(ap, fmt);
    size_t msg_len = bxistr_vnew(&msg, fmt, ap);
    va_end(ap);

    const char * filename;
    size_t filename_len = bxistr_rsub(__FILE__, ARRAYLEN(__FILE__) - 1, '/', &filename);

    bxilog_record_s record;
    record.level = level;

    err2 = bxitime_get(CLOCK_REALTIME, &record.detail_time);
    BXIERR_CHAIN(err, err2);

    record.pid = data->pid;
#ifdef __linux__
    record.tid = data->tid;
//    size_t thread_name_len
#endif
    record.thread_rank = data->thread_rank;
    record.line_nb = line_nb;
    //  size_t progname_len;            // program name length
    record.filename_len = filename_len + 1;
    record.funcname_len = funclen;
    record.logname_len = ARRAYLEN(INTERNAL_LOGGER_NAME);
    record.logmsg_len = msg_len;

    err2 = _process_log(&record,
                        (char *) filename,
                        (char*) funcname,
                        INTERNAL_LOGGER_NAME,
                        msg,
                        data);
    BXIERR_CHAIN(err, err2);

    BXIFREE(msg);

    return err;
}
