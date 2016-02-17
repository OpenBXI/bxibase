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

#include <bxi/base/log/netsnmp_handler.h>
#include <unistd.h>
#include <syscall.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"

#include "bxi/base/log.h"


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define INTERNAL_LOGGER_NAME BXILOG_LIB_PREFIX "bxilog.handler.snmplog"

SET_LOGGER(NETSNMP_LOGGER, BXILOG_LIB_PREFIX "bxilog.netsnmp");

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************
typedef struct bxilog_snmplog_handler_param_s_f * bxilog_snmplog_handler_param_p;
typedef struct bxilog_snmplog_handler_param_s_f {
    bxilog_handler_param_s generic;

    pid_t pid, tid;
    uint16_t thread_rank;

    bxierr_set_p errset;
    size_t error_limit;

} bxilog_snmplog_handler_param_s;

typedef struct {
    const bxilog_snmplog_handler_param_p data;
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
                                         bxilog_filters_p filters,
                                         va_list ap);
static bxierr_p _init(bxilog_snmplog_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_snmplog_handler_param_p data);
static bxierr_p _process_ierr(bxierr_p * err, bxilog_snmplog_handler_param_p data);
static bxierr_p _process_implicit_flush(bxilog_snmplog_handler_param_p data);
static bxierr_p _process_explicit_flush(bxilog_snmplog_handler_param_p data);
static bxierr_p _process_exit(bxilog_snmplog_handler_param_p data);
static bxierr_p _process_cfg(bxilog_snmplog_handler_param_p data);
static bxierr_p _param_destroy(bxilog_snmplog_handler_param_p *data_p);

static bxierr_p _sync(bxilog_snmplog_handler_param_p data);

static bxierr_p _internal_log_func(bxilog_level_e level,
                                   bxilog_snmplog_handler_param_p data,
                                   const char * funcname,
                                   size_t funclen,
                                   int line_nb,
                                   const char * fmt, ...);
static bxierr_p _log_single_line(char * line,
                          size_t line_len,
                          bool last,
                          log_single_line_param_p param);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

static const bxilog_handler_s BXILOG_SNMPLOG_HANDLER_S = {
                  .name = "BXI Logging NetSNMP Handler",
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
const bxilog_handler_p BXILOG_NETSNMP_HANDLER = (bxilog_handler_p) &BXILOG_SNMPLOG_HANDLER_S;

static const bxilog_level_e SNMP2BXILOG_LEVELS[] = {
    BXILOG_EMERG,       // #define LOG_EMERG       0       /* system is unusable */
    BXILOG_ALERT,       // #define LOG_ALERT       1       /* action must be taken immediately */
    BXILOG_CRIT,        // #define LOG_CRIT        2       /* critical conditions */
    BXILOG_ERR,         // #define LOG_ERR         3       /* error conditions */
    BXILOG_WARN,        // #define LOG_WARNING     4       /* warning conditions */
    BXILOG_NOTICE,      // #define LOG_NOTICE      5       /* normal but significant condition */
    BXILOG_INFO,        // #define LOG_INFO        6       /* informational */
    BXILOG_DEBUG,       // #define LOG_DEBUG       7       /* debug-level messages */
};
//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filters_p filters,
                                  va_list ap) {

    bxiassert(BXILOG_NETSNMP_HANDLER == self);

    va_end(ap);

    bxilog_snmplog_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    return (bxilog_handler_param_p) result;
}

int bxilog_netsnmp_callback(int majorID, int minorID,
                            void *serverarg, void * clientarg) {
    UNUSED(majorID);
    UNUSED(minorID);

    BXIASSERT(NETSNMP_LOGGER, NULL != serverarg);
    BXIASSERT(NETSNMP_LOGGER, NULL != clientarg);

    bxilog_logger_p* logger_p = (bxilog_logger_p*) clientarg;
    struct snmp_log_message * slm = (struct snmp_log_message *) serverarg;

    if (NULL == slm->msg) return 0;

    // SNMP message always contains the '\n'
    // We will remove it and replace it by '\0'
    size_t len = strlen(slm->msg); // Does not include the NULL terminating byte
    char buf[len];
    memcpy(buf, slm->msg, len);

    BXIASSERT(*logger_p, '\n' == buf[len-1]);
    buf[len - 1] = '\0';

    bxilog_logger_log_rawstr(*logger_p,
                             SNMP2BXILOG_LEVELS[slm->priority],
                             __FILE__, ARRAYLEN(__FILE__),
                             __FUNCTION__, ARRAYLEN(__FUNCTION__),
                             __LINE__,
                             buf, len);

    return 0;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _init(bxilog_snmplog_handler_param_p data) {

    data->pid = getpid();
#ifdef __linux__
    data->tid = (pid_t) syscall(SYS_gettid);
#endif
    // TODO: find something better for the rank of the IHT
    // Maybe, define already the related string instead of
    // a rank number?
    data->thread_rank = (uint16_t) pthread_self();
    data->errset = bxierr_set_new();
    data->error_limit = 10;

    return BXIERR_OK;
}

bxierr_p _process_exit(bxilog_snmplog_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    err2 = _sync(data);
    BXIERR_CHAIN(err, err2);

    bxierr_set_destroy(&data->errset);

    return err;
}

inline bxierr_p _process_implicit_flush(bxilog_snmplog_handler_param_p data) {
    UNUSED(data);
    return _sync(data);
}

inline bxierr_p _process_explicit_flush(bxilog_snmplog_handler_param_p data) {
    UNUSED(data);
    return _sync(data);
}

inline bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_snmplog_handler_param_p data) {

//    const char * fn = bxistr_rsub(filename, record->filename_len, '/');

    log_single_line_param_s param = {
                                     .data = data,
                                     .record = record,
                                     .filename = filename,
                                     .funcname = funcname,
                                     .loggername = loggername,
                                     .logmsg = logmsg,
    };

    bxierr_p err = bxistr_apply_lines(logmsg,
                                      record->logmsg_len - 1,
                                      (bxierr_p (*)(char*, size_t, bool, void*)) _log_single_line,
                                      &param);

    return err;
}


bxierr_p _process_ierr(bxierr_p *err, bxilog_snmplog_handler_param_p data) {
    bxierr_p result = BXIERR_OK;

    if (bxierr_isok(*err)) return *err;

    bool new = bxierr_set_add(data->errset, err);
    // Only report newly detected errors
    char * err_str = NULL;
    if (new) {
        char * err_str = bxierr_str(*err);
        result  = _ilog(BXILOG_ERROR, data, "An error occured.\n%s\n"
                        "This is the first time an error with code %d is reported "
                        "and it will be the last time.\n"
                        "An error reporting summary should be "
                        "available in your program if it uses the full BXI "
                        "high performance logging library.\n",
                        err_str, (*err)->code);
        BXIFREE(err_str);
    }

    if (bxierr_isko(result)) {
        result = bxierr_new(BXILOG_HANDLER_EXIT_CODE, result, NULL, NULL, NULL,
                            "Fatal: error while processing error: %s", err_str);
        BXIFREE(err_str);
    } else if (data->errset->total_seen_nb >= data->error_limit) {
        result = bxierr_new(BXILOG_HANDLER_EXIT_CODE, NULL, NULL, NULL, NULL,
                            "Fatal: too many errors (%zu distinct errors/%zu total errors)",
                            data->errset->distinct_err.errors_nb,
                            data->errset->total_seen_nb);
    }

    return result;
}


bxierr_p _process_cfg(bxilog_snmplog_handler_param_p data) {
    UNUSED(data);
    bxiunreachable_statement;
    return BXIERR_OK;
}

inline bxierr_p _param_destroy(bxilog_snmplog_handler_param_p * data_p) {
    bxilog_snmplog_handler_param_p data = *data_p;

    bxilog_handler_clean_param(&data->generic);

    bxierr_set_destroy(&data->errset);
    bximem_destroy((char**) data_p);
    return BXIERR_OK;
}


bxierr_p _sync(bxilog_snmplog_handler_param_p data) {
    UNUSED(data);

    return BXIERR_OK;
}


bxierr_p _internal_log_func(bxilog_level_e level,
                            bxilog_snmplog_handler_param_p data,
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

bxierr_p _log_single_line(char * line,
                          size_t line_len,
                          bool last,
                          log_single_line_param_p param) {

    UNUSED(last);
    UNUSED(line_len);
//    bxilog_snmplog_handler_param_p data = param->data;
    bxilog_record_p record = param->record;

    snmp_log(record->level, "%s\n", line);

    return BXIERR_OK;
}
