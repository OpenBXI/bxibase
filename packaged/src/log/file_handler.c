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

#include "handler_impl.h"
#include "log_impl.h"

#include "bxi/base/log/file_handler.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

#define INTERNAL_LOGGER_NAME "bxi.base.log.handler.file"

// WARNING: highly dependent on the log format
#define YEAR_SIZE 4
#define MONTH_SIZE 2
#define DAY_SIZE 2
#define HOUR_SIZE 2
#define MINUTE_SIZE 2
#define SECOND_SIZE 2
#define SUBSECOND_SIZE 9
#define PID_SIZE 5
#define TID_SIZE 5
#define THREAD_RANK_SIZE 5

// WARNING: highly dependent on the log format
#ifdef __linux__
// LOG_FMT "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u=%0*u:%s|%s:%d@%s|%s|%s\n";
// O|20140918T090752.472145261|11297.11302=01792:unit_t|unit_t.c:308@_dummy|bxiclib.test|msg

#define FIXED_LOG_SIZE 2 + YEAR_SIZE + MONTH_SIZE + DAY_SIZE +\
                       1 + HOUR_SIZE + MINUTE_SIZE + SECOND_SIZE + 1 + SUBSECOND_SIZE +\
                       1 + PID_SIZE + 1 + TID_SIZE + 1 + THREAD_RANK_SIZE + \
                       1 + 1 + 1 + 1 + 1 + 1 + 1  // Add all remaining fixed characters
                                                  // such as ':|:@||\n' in that order
#else
#define FIXED_LOG_SIZE 2 + YEAR_SIZE + MONTH_SIZE + DAY_SIZE +\
                       1 + HOUR_SIZE + MINUTE_SIZE + SECOND_SIZE + 1 + SUBSECOND_SIZE +\
                       1 + PID_SIZE + 1 + THREAD_RANK_SIZE + \
                       1 + 1 + 1 + 1 + 1 + 1 +1  // Add all remaining fixed characters
                                                 // such as ':|:@||\n' in that order
#endif

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************
typedef struct bxilog_file_handler_param_s_f * bxilog_file_handler_param_p;
typedef struct bxilog_file_handler_param_s_f {
    bxilog_handler_param_s generic;
    bool append;
    char * filename;
    char * progname;
    size_t progname_len;
    pid_t pid;
    int fd;                         // the file descriptor where log must be produced
#ifdef __linux__
    pid_t tid;
#endif
    uint16_t thread_rank;
    bxierr_set_p errset;
    size_t lost_logs;
} bxilog_file_handler_param_s;

typedef struct {
    const bxilog_file_handler_param_p data;
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
static bxilog_handler_param_p _param_new(bxilog_handler_p self, va_list ap);
static bxierr_p _init(bxilog_file_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_file_handler_param_p data);
static bxierr_p _process_err(bxierr_p * err, bxilog_file_handler_param_p data);
static bxierr_p _process_implicit_flush(bxilog_file_handler_param_p data);
static bxierr_p _process_explicit_flush(bxilog_file_handler_param_p data);
static bxierr_p _process_exit(bxilog_file_handler_param_p data);
static bxierr_p _process_cfg(bxilog_file_handler_param_p data);
static bxierr_p _param_destroy(bxilog_file_handler_param_p *data_p);

static bxierr_p _get_file_fd(bxilog_file_handler_param_p data);

static bxierr_p _log_single_line(char * line,
                             size_t line_len,
                             bool last,
                             log_single_line_param_p param);

static ssize_t _mkmsg(const size_t n, char buf[n],
                      const char level,
                      const struct timespec * const detail_time,
                      const pid_t pid,
#ifdef __linux__
                      const pid_t tid,
#endif
                      const uint16_t thread_rank,
                      const char * const progname,
                      const char * const filename,
                      const int line_nb,
                      const char * const funcname,
                      const char * const loggername,
                      const char * const logmsg);

static bxierr_p _sync(bxilog_file_handler_param_p data);

static bxierr_p _internal_log_func(bxilog_level_e level,
                                   bxilog_file_handler_param_p data,
                                   const char * funcname,
                                   size_t funclen,
                                   int line_nb,
                                   const char * fmt, ...);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The various log levels specific characters
static const char LOG_LEVEL_STR[] = { 'P', 'A', 'C', 'E', 'W', 'N', 'O', 'I', 'D', 'F', 'T', 'L'};
// The log format used by the IHT when writing
// WARNING: If you change this format, change also different #define above
// along with FIXED_LOG_SIZE
#ifdef __linux__
static const char LOG_FMT[] = "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u=%0*u:%s|%s:%d@%s|%s|%s\n";
#else
static const char LOG_FMT[] = "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u:%s|%s:%d@%s|%s|%s\n";
#endif

static const bxilog_handler_s BXILOG_FILE_HANDLER_S = {
                  .name = "BXI Logging File Handler",
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
const bxilog_handler_p BXILOG_FILE_HANDLER = (bxilog_handler_p) &BXILOG_FILE_HANDLER_S;

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxilog_handler_param_p _param_new(bxilog_handler_p self, va_list ap) {
    bxiassert(BXILOG_FILE_HANDLER == self);

    char * progname = va_arg(ap, char *);
    char * filename = va_arg(ap, char *);
    bool append = (bool) va_arg(ap, int);
    va_end(ap);

    bxilog_file_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, &result->generic);

    result->filename = strdup(filename);
    result->append = append;
    result->progname = strdup(progname);
    result->progname_len = strlen(progname) + 1; // Include the NULL terminal byte

    return (bxilog_handler_param_p) result;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _init(bxilog_file_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    data->pid = getpid();
#ifdef __linux__
    data->tid = (pid_t) syscall(SYS_gettid);
#endif
    // TODO: find something better for the rank of the IHT
    // Maybe, define already the related string instead of
    // a rank number?
    data->thread_rank = (uint16_t) pthread_self();
    data->errset = bxierr_set_new();
    data->lost_logs = 0;

    err2 = _get_file_fd(data);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _process_exit(bxilog_file_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    err2 = _sync(data);
    BXIERR_CHAIN(err, err2);
    errno = 0;
    if (STDOUT_FILENO != data->fd && STDERR_FILENO != data->fd && 0 < data->fd) {
        int rc = close(data->fd);
        if (-1 == rc) {
            err2 = bxierr_errno("Closing logging file '%s' failed", data->filename);
            BXIERR_CHAIN(err, err2);
        }
    }

    if (data->lost_logs > 0) {
        char * str = bxistr_new("BXI Log File Handler Error Summary:\n"
                                "\tNumber of lost log lines: %zu\n"
                                "\tNumber of reported distinct errors: %zu\n",
                                data->lost_logs,
                                data->errset->distinct_err.errors_nb);
        bxilog_rawprint(str, STDERR_FILENO);
        BXIFREE(str);
    }

    if (0 < data->errset->distinct_err.errors_nb) {
        err2 = bxierr_from_set(BXIERR_GROUP_CODE, data->errset,
                                 "Error Set (%zu distinct errors)",
                                 data->errset->distinct_err.errors_nb);
        BXIERR_CHAIN(err, err2);
    } else {
        bxierr_set_destroy(&data->errset);
    }

    return err;
}

inline bxierr_p _process_implicit_flush(bxilog_file_handler_param_p data) {
    return _sync(data);
}

inline bxierr_p _process_explicit_flush(bxilog_file_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

//    err2 = _ilog(BXILOG_TRACE, data, "Flushing requested");
//    BXIERR_CHAIN(err, err2);
//    fprintf(stderr, "Flushing\n");
    err2 = _process_implicit_flush(data);
//    fprintf(stderr, "Flushed\n");
    BXIERR_CHAIN(err, err2);

//    err2 = _ilog(BXILOG_TRACE, data, "Flushed");
//    BXIERR_CHAIN(err, err2);

    return err;
}

inline bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_file_handler_param_p data) {

    const char * fn = bxistr_rsub(filename, record->filename_len, '/');

    log_single_line_param_s param = {
                                     .data = data,
                                     .record = record,
                                     .filename = fn,
                                     .funcname = funcname,
                                     .loggername = loggername,
                                     .logmsg = logmsg,
    };
//    fprintf(stderr, "Processing log\n");
    bxierr_p err = bxistr_apply_lines(logmsg,
                                      (bxierr_p (*)(char*, size_t, bool, void*)) _log_single_line,
                                      &param);
//    fprintf(stderr, "Processed log\n");
    return err;

}


bxierr_p _process_err(bxierr_p *err, bxilog_file_handler_param_p data) {
    bxierr_p result = BXIERR_OK;

    if (bxierr_isok(*err)) return *err;

    char * str = bxierr_str(*err);
    bxierr_p fatal = _ilog(BXILOG_ERROR, data, "An internal error occured: %s", str);

    if (bxierr_isko(fatal)) {
        result = bxierr_new(BXILOG_HANDLER_EXIT_CODE, fatal, NULL, NULL, NULL,
                            "Fatal: error while processing error: %s", str);
    } else {
        bxierr_destroy(err);
    }
    BXIFREE(str);

    return result;
}


bxierr_p _process_cfg(bxilog_file_handler_param_p data) {
    UNUSED(data);
    bxiunreachable_statement;
    return BXIERR_OK;
}

inline bxierr_p _param_destroy(bxilog_file_handler_param_p * data_p) {
    bxilog_file_handler_param_p data = *data_p;

    bxilog_handler_clean_param(&data->generic);

    bxierr_set_destroy(&data->errset);
    BXIFREE(data->progname);
    BXIFREE(data->filename);
    bximem_destroy((char**) data_p);
    return BXIERR_OK;
}

bxierr_p _log_single_line(char * line,
                          size_t line_len,
                          bool last,
                          log_single_line_param_p param) {

    UNUSED(last);
    bxilog_file_handler_param_p data = param->data;
    bxilog_record_p record = param->record;

    const size_t size = FIXED_LOG_SIZE + \
            data->progname_len + \
            record->variable_len + line_len + 1;
    char msg[size];
    const ssize_t loglen = _mkmsg(size, msg,
                                  LOG_LEVEL_STR[param->record->level],
                                  &param->record->detail_time,
                                  param->record->pid,
#ifdef __linux__
                                  param->record->tid,
#endif
                                  record->thread_rank,
                                  data->progname,
                                  param->filename,
                                  record->line_nb,
                                  param->funcname,
                                  param->loggername,
                                  line);
    errno = 0;
    ssize_t written = write(data->fd, msg, (size_t) loglen);

    if (0 >= written) {
        if (EPIPE == errno) return bxierr_errno("Can't write to pipe (fd=%d, name=%s). "
                                                "Exiting. Some messages will be lost.",
                                                data->fd, data->filename);

        bxierr_p bxierr = bxierr_errno("Calling write(fd=%d, name=%s) "
                "failed (written=%d)",
                data->fd, data->filename, written);
        data->lost_logs++;
        bool new = bxierr_set_add(data->errset, &bxierr);
        // Only report newly detected errors
        if (new) {
            char * err_str = bxierr_str(bxierr);
            char * str = bxistr_new("[W] Can't write to '%s' - cause is %s\n"
                    "This means "
                    "at least one log line has been lost\n"
                    "[W]This error might be caused by other errors.\n"
                    "[W]This is the first time this cause has been"
                    " reported however, and it will be the last time.\n"
                    "[W]An error reporting summary should be "
                    "available in your program if it uses the full bxi "
                    "high performance logging library.\n",
                    data->filename, err_str);
            bxilog_rawprint(str, STDERR_FILENO);
            BXIFREE(err_str);
            BXIFREE(str);
        } else {
            bxierr_destroy(&bxierr);
        }
    }
    return BXIERR_OK;
}


ssize_t _mkmsg(const size_t n, char buf[n],
               const char level,
               const struct timespec * const detail_time,
               const pid_t pid,
#ifdef __linux__
               const pid_t tid,
#endif
               const uint16_t thread_rank,
               const char * const progname,
               const char * const filename,
               const int line_nb,
               const char * const funcname,
               const char * const loggername,
               const char * const logmsg) {

    errno = 0;
    struct tm dummy, *now;
    now = localtime_r(&detail_time->tv_sec, &dummy);
    bxiassert(NULL != now);
    int written = snprintf(buf, n, LOG_FMT,
                           level,
                           YEAR_SIZE, now->tm_year + 1900,
                           MONTH_SIZE, now->tm_mon + 1,
                           DAY_SIZE, now->tm_mday,
                           HOUR_SIZE, now->tm_hour,
                           MINUTE_SIZE, now->tm_min,
                           SECOND_SIZE, now->tm_sec,
                           SUBSECOND_SIZE, detail_time->tv_nsec,
                           PID_SIZE, pid,
#ifdef __linux__
                           TID_SIZE, tid,
#endif
                           THREAD_RANK_SIZE, thread_rank,
                           progname,
                           filename,
                           line_nb,
                           funcname,
                           loggername,
                           logmsg);


    // Truncation must never occur
    // If it happens, it means the size given was just plain wrong
//    bxiassert(written < (int)n);
    // For debugging in case the previous condition does not hold,
    // comment previous line, and uncomment lines below.
    if (written >= (int) n) {
        fprintf(stderr, "******** ERROR: FIXED_LOG_SIZE=%d, "
                    "written = %d >= %zu = n\nbuf(%zu)=%s\nlogmsg(%zu)=%s\n",
                    FIXED_LOG_SIZE, written, n ,
                    strlen(buf), buf, strlen(logmsg), logmsg);
    }

    bxiassert(written >= 0);
    return written;
}

bxierr_p _get_file_fd(bxilog_file_handler_param_p data) {
    errno = 0;
    if (0 == strcmp(data->filename, "-")) {
        data->fd = STDOUT_FILENO;
    } else if (0 == strcmp(data->filename, "+")) {
        data->fd = STDERR_FILENO;
    } else {
        errno = 0;
        data->fd = open(data->filename,
                        O_WRONLY | O_CREAT | (data->append ? O_APPEND : O_TRUNC) | O_CLOEXEC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (-1 == data->fd) return bxierr_errno("Can't open %s", data->filename);
    }

    return BXIERR_OK;
}

bxierr_p _sync(bxilog_file_handler_param_p data) {
    errno = 0;

    int rc = fdatasync(data->fd);
    if (rc != 0) {
        if (EROFS != errno && EINVAL != errno) {
            return bxierr_errno("Call to fdatasync() failed");
        }
        // OK otherwise, it just means the given FD does not support synchronization
        // this is the case for example with stdout, stderr...
    }
    return BXIERR_OK;
}


bxierr_p _internal_log_func(bxilog_level_e level,
                            bxilog_file_handler_param_p data,
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

    const char * filename = bxistr_rsub(__FILE__, ARRAYLEN(__FILE__) - 1, '/');

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
    record.filename_len = strlen(filename) + 1;
    record.funcname_len = funclen;
    record.logname_len = ARRAYLEN(INTERNAL_LOGGER_NAME);
    record.logmsg_len = msg_len;
    record.variable_len = record.filename_len + \
                          record.funcname_len + \
                          record.logname_len + \
                          record.logmsg_len;

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
