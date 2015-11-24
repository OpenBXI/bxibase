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
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)


#define TC_FG_RGB(r, g, b) "\033[38;2;" #r ";" #g ";" #b "m"
#define TC_BG_RGB(r, g, b) "\033[48;2;" #r ";" #g ";" #b "m"

#define RESET_COLORS "\033[0m"

#define INTERNAL_LOGGER_NAME "bxi.base.log.handler.console"

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************
typedef struct bxilog_console_handler_param_s_f * bxilog_console_handler_param_p;
typedef struct log_single_line_param_s_f * log_single_line_param_p;

typedef struct bxilog_console_handler_param_s_f {
    bxilog_handler_param_s generic;

    pid_t pid, tid;
    uint16_t thread_rank;

    bxilog_level_e stderr_level;
    bxierr_list_p errset;
    size_t lost_logs;
    char ** colors;
    bxierr_p (*display_out) (char * line,
                             size_t line_len,
                             bool last,
                             log_single_line_param_p param);
    bxierr_p (*display_err) (char * line,
                             size_t line_len,
                             bool last,
                             log_single_line_param_p param);
} bxilog_console_handler_param_s;

typedef struct log_single_line_param_s_f {
    const bxilog_console_handler_param_p data;
    const bxilog_record_p record;
    const char * filename;
    const char *funcname;
    const char * loggername;
    const char *logmsg;
    FILE* out;
} log_single_line_param_s;


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                         bxilog_filters_p filters,
                                         va_list ap);
static bxierr_p _init(bxilog_console_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_console_handler_param_p data);
static bxierr_p _display_color(char * line,
                               size_t line_len,
                               bool last,
                               log_single_line_param_p param);
static bxierr_p _display_nocolor(char * line,
                                 size_t line_len,
                                 bool last,
                                 log_single_line_param_p param);
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

//************************************************************************************
//******************************** COLORS ********************************************
//************************************************************************************
// Creating new color theme is very simple:
// Just create a new array with the corresponding ANSI color code
// See: https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
// Note that some terminals does not support all color code (namely rxvt and the like).


// 6 × 6 × 6 = 216 colors: 16 + 36 × r + 6 × g + b (0 ≤ r, g, b ≤ 5)
const bxilog_colors_p BXILOG_COLORS_216_DARK = {
                             "\033[5m",                                     // OFF: Blink
                             "\033[1m" "\033[38;5;207m",                                // PANIC
                             "\033[1m" "\033[38;5;200m",                                // ALERT
                             "\033[1m" "\033[38;5;198m",                                // CRITICAL
                             "\033[22m" "\033[38;5;196m",                                // ERROR
                             "\033[22m" "\033[38;5;226m",                                // WARNING
                             "\033[22m" "\033[38;5;229m",                                // NOTIFY
                             "\033[22m" "\033[38;5;231m",                                // OUTPUT
                             "\033[2m" "\033[38;5;46m",                                 // INFO
                             "\033[2m" "\033[38;5;83m",                                 // DEBUG
                             "\033[2m" "\033[38;5;77m",                                 // FINE
                             "\033[2m" "\033[38;5;71m",                                 // TRACE
                             "\033[2m" "\033[38;5;65m",                                 // LOWEST
};

const bxilog_colors_p BXILOG_COLORS_TC_DARK = {
                             "\033[5m",                                     // OFF: Blink
                             "\033[1m" TC_FG_RGB(255,51,255),                               // PANIC
                             "\033[1m" TC_FG_RGB(255,0,175),                                // ALERT
                             "\033[1m" TC_FG_RGB(255,0,150),                                // CRITICAL
                             "\033[22m" TC_FG_RGB(255,0,0),                                  // ERROR
                             "\033[22m" TC_FG_RGB(255,255,0),                                // WARNING
                             "\033[22m" TC_FG_RGB(255,255,153),                              // NOTIFY
                             "\033[22m" TC_FG_RGB(255,255,255),                              // OUTPUT
                             "\033[2m" TC_FG_RGB(0,255,0),                                  // INFO
                             "\033[2m" TC_FG_RGB(51,255,51),                                // DEBUG
                             "\033[2m" TC_FG_RGB(51,204,51),                                // FINE
                             "\033[2m" TC_FG_RGB(51,153,51),                                // TRACE
                             "\033[2m" TC_FG_RGB(51,102,51),                                // LOWEST
};

const bxilog_colors_p BXILOG_COLORS_TC_LIGHT = {
                             "\033[5m",                                     // OFF: Blink
                             "\033[1m" TC_FG_RGB(255,25,70),                                // PANIC
                             "\033[1m" TC_FG_RGB(200,0,50),                                 // ALERT
                             "\033[1m" TC_FG_RGB(175,0,0),                                  // CRITICAL
                             "\033[22m" TC_FG_RGB(128,25,0),                                 // ERROR
                             "\033[22m" TC_FG_RGB(128,75,0),                                 // WARNING
                             "\033[22m" TC_FG_RGB(75,50,0),                                  // NOTIFY
                             "\033[22m" TC_FG_RGB(0,0,0),                                    // OUTPUT
                             "\033[2m" TC_FG_RGB(0,50,75),                                  // INFO
                             "\033[2m" TC_FG_RGB(0,50,100),                                 // DEBUG
                             "\033[2m" TC_FG_RGB(0,75,125),                                 // FINE
                             "\033[2m" TC_FG_RGB(0,75,150),                                 // TRACE
                             "\033[2m" TC_FG_RGB(0,100,175),                                // LOWEST
};

const bxilog_colors_p BXILOG_COLORS_TC_DARKGRAY = {
                             "\033[5m",                                     // OFF: Blink
                             "\033[1m" TC_FG_RGB(255,255,255),                                // PANIC
                             "\033[1m" TC_FG_RGB(233,233,233),                                 // ALERT
                             "\033[1m" TC_FG_RGB(212,212,212),                                  // CRITICAL
                             "\033[22m" TC_FG_RGB(191,191,191),                                 // ERROR
                             "\033[22m" TC_FG_RGB(170,170,170),                                 // WARNING
                             "\033[22m" TC_FG_RGB(149,149,149),                                  // NOTIFY
                             "\033[22m" TC_FG_RGB(128,128,128),                              // OUTPUT
                             "\033[2m" TC_FG_RGB(113,113,113),                                  // INFO
                             "\033[2m" TC_FG_RGB(98,98,98),                                 // DEBUG
                             "\033[2m" TC_FG_RGB(83,83,83),                                 // FINE
                             "\033[2m" TC_FG_RGB(68,68,68),                                 // TRACE
                             "\033[2m" TC_FG_RGB(53,53,53),                                // LOWEST
};

const bxilog_colors_p BXILOG_COLORS_TC_LIGHTGRAY = {
                             "\033[5m",                                     // OFF: Blink
                             "\033[1m" TC_FG_RGB(38,38,38),                                // PANIC
                             "\033[1m" TC_FG_RGB(53,53,53),                                 // ALERT
                             "\033[1m" TC_FG_RGB(68,38,68),                                  // CRITICAL
                             "\033[22m" TC_FG_RGB(83,83,83),                                 // ERROR
                             "\033[22m" TC_FG_RGB(98,98,98),                                 // WARNING
                             "\033[22m" TC_FG_RGB(113,113,113),                              // NOTIFY
                             "\033[22m" TC_FG_RGB(128,128,128),                              // OUTPUT
                             "\033[2m" TC_FG_RGB(149,149,149),                                  // INFO
                             "\033[2m" TC_FG_RGB(170,170,170),                                 // DEBUG
                             "\033[2m"TC_FG_RGB(191,191,191),                                 // FINE
                             "\033[2m" TC_FG_RGB(202,202,202),                                 // TRACE
                             "\033[2m" TC_FG_RGB(210,210,210),                                // LOWEST
};

const char ** BXILOG_COLORS_NONE = NULL;

static const char LOG_LEVEL_STR[] = { '-', 'P', 'A', 'C', 'E', 'W', 'N', 'O',
                                      'I', 'D', 'F', 'T', 'L'};


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filters_p filters,
                                  va_list ap) {

    bxiassert(BXILOG_CONSOLE_HANDLER == self);

    bxilog_level_e level = (bxilog_level_e) va_arg(ap, int);
    char ** colors = va_arg(ap, char **);
    va_end(ap);

    bxilog_console_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    result->stderr_level = level;
    result->colors = colors;

    if (NULL != result->colors) {
//        result->display_out = isatty(STDOUT_FILENO) ? _display_color : _display_nocolor;
//        result->display_err = isatty(STDERR_FILENO) ? _display_color : _display_nocolor;
        result->display_out = _display_color;
        result->display_err = _display_color;
    } else {
        result->display_out = _display_nocolor;
        result->display_err = _display_nocolor;
    }

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

    log_single_line_param_s param = {
                                     .data = data,
                                     .record = record,
                                     .filename = filename,
                                     .funcname = funcname,
                                     .loggername = loggername,
                                     .logmsg = logmsg,
    };


    bxierr_p err;
    if (record->level > data->stderr_level) {
        param.out = stdout;
        err = bxistr_apply_lines(logmsg,
                                 (bxierr_p (*)(char*, size_t, bool, void*)) data->display_out,
                                 &param);
    } else {
        param.out = stderr;
        err = bxistr_apply_lines(logmsg,
                                 (bxierr_p (*)(char*, size_t, bool, void*)) data->display_err,
                                 &param);
    }

    return err;
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
    // We just don't care!
//    if (0 != rc) {
//        err2 = bxierr_errno("Calling fflush(stderr) failed");
//        BXIERR_CHAIN(err, err2);
//    }

    errno = 0;
    rc = fflush(stdout);
    // We just don't care!
//    if (0 != rc) {
//        err2 = bxierr_errno("Calling fflush(stdout) failed");
//        BXIERR_CHAIN(err, err2);
//    }

    UNUSED(rc);
    UNUSED(err2);

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


inline bxierr_p _display_nocolor(char * line,
                                 size_t line_len,
                                 bool last,
                                 log_single_line_param_p param) {

    UNUSED(last);
    bxilog_record_p record = param->record;

    char buf[line_len + 1];
    memcpy(buf, line, line_len);
    buf[line_len] = '\0';

    int rc;
    if (BXILOG_OUTPUT != record->level) {
        rc = fprintf(param->out, "[%c] %s\n",
                     LOG_LEVEL_STR[record->level],
                     buf);
    } else {
        rc = fprintf(param->out, "%s\n", buf);
    }

    UNUSED(rc);
    // We just don't care!
//    if (rc < 0) {
//        return bxierr_errno("Calling fprintf() failed");
//    }

    return BXIERR_OK;
}

inline bxierr_p _display_color(char * line,
                               size_t line_len,
                               bool last,
                               log_single_line_param_p param) {
    UNUSED(last);
    bxilog_console_handler_param_p data = param->data;
    bxilog_record_p record = param->record;

    char buf[line_len + 1];
    memcpy(buf, line, line_len);
    buf[line_len] = '\0';

    errno = 0;
    int rc;
    if (BXILOG_OUTPUT != record->level) {
        rc = fprintf(param->out, "%s[%c] %s\n" RESET_COLORS,
                     data->colors[record->level],
                     LOG_LEVEL_STR[record->level],
                     buf);
    } else {
        rc = fprintf(param->out, "%s%s\n" RESET_COLORS,
                     data->colors[record->level],
                     buf);
    }
    UNUSED(rc);
    // We just don't care!
//    if (rc < 0) {
//        return bxierr_errno("Calling fprintf() failed");
//    }

    return BXIERR_OK;
}
