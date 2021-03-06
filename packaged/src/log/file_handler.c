/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
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
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>


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

#define ERR_LOG_LIMIT 1061117   // Leet Speak

#define INTERNAL_LOGGER_NAME BXILOG_LIB_PREFIX "bxilog.handler.file"
#define DEFAULT_BLOCKS_NB 4

// WARNING: highly dependent on the log format
#define YEAR_SIZE 4
#define MONTH_SIZE 2
#define DAY_SIZE 2
#define HOUR_SIZE 2
#define MINUTE_SIZE 2
#define SECOND_SIZE 2
#define SUBSECOND_SIZE 9
#define PID_SIZE 7
#define TID_SIZE 7
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
                       1 + 1 + 1 + 1 + 1 + 1 + 1  // Add all remaining fixed characters
                                                 // such as ':|:@||\n' in that order
#endif

#define _ilog(level, data, ...) _internal_log_func(level, data, __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************
typedef struct bxilog_file_handler_param_s_f * bxilog_file_handler_param_p;
typedef struct bxilog_file_handler_param_s_f {
    bxilog_handler_param_s generic;
    int open_flags;
    char * filename;
    char * progname;
    size_t progname_len;
    pid_t pid;
    int fd;                         // the file descriptor where log must be produced
#ifdef __linux__
    pid_t tid;
#endif
    uintptr_t thread_rank;
    bxierr_set_p errset;
    size_t err_max;
    bool dirty;
    size_t bytes_lost;
    size_t bytes_written;
    size_t next_char;
    size_t buf_size;
    char * buf;
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
static bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                         bxilog_filters_p filters,
                                         va_list ap);
static bxierr_p _init(bxilog_file_handler_param_p data);
static bxierr_p _process_log(bxilog_record_p record,
                             char * filename,
                             char * funcname,
                             char * loggername,
                             char * logmsg,
                             bxilog_file_handler_param_p data);
static bxierr_p _process_ierr(bxierr_p * err, bxilog_file_handler_param_p data);
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

static size_t _mkmsg(const size_t n, char buf[n],
                     const char level,
                     const struct timespec * const detail_time,
                     const pid_t pid,
#ifdef __linux__
                     const pid_t tid,
#endif
                     const uintptr_t thread_rank,
                     const char * const progname,
                     const char * const filename,
                     const int line_nb,
                     const char * const funcname,
                     const char * const loggername,
                     const char * const logmsg,
                     size_t line_len);

static bxierr_p _flush(bxilog_file_handler_param_p data);
static bxierr_p _write(bxilog_file_handler_param_p data, const void * buf, size_t count);
static bxierr_p _sync(bxilog_file_handler_param_p data);
static void _tune_io(bxilog_file_handler_param_p data);
static bxierr_p _internal_log_func(bxilog_level_e level,
                                   bxilog_file_handler_param_p data,
                                   const char * funcname,
                                   size_t funclen,
                                   int line_nb,
                                   const char * fmt, ...);
static void _record_new_error(bxilog_file_handler_param_p data, bxierr_p * err);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The various log levels specific characters
const char BXILOG_FILE_HANDLER_LOG_LEVEL_STR[] = { '-', 'P', 'A', 'C', 'E', 'W', 'N', 'O',
                                                   'I', 'D', 'F', 'T', 'L'};
// The log format used by the IHT when writing
// WARNING: If you change this format, change also different #define above
// along with FIXED_LOG_SIZE
#ifdef __linux__
static const char LOG_FMT[] = "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u=%0*" PRIxPTR ":%s|%s:%d@%s|%s|";
#else
static const char LOG_FMT[] = "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u:%s|%s:%d@%s|%s|";
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
                  .process_ierr = (bxierr_p (*) (bxierr_p*, bxilog_handler_param_p)) _process_ierr,
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

bxilog_handler_param_p _param_new(bxilog_handler_p self,
                                  bxilog_filters_p filters,
                                  va_list ap) {

    bxiassert(BXILOG_FILE_HANDLER == self);

    char * progname = va_arg(ap, char *);
    char * filename = va_arg(ap, char *);
    int open_flags = va_arg(ap, int);
    va_end(ap);

    bxilog_file_handler_param_p result = bximem_calloc(sizeof(*result));
    bxilog_handler_init_param(self, filters, &result->generic);

    result->filename = strdup(filename);
    result->open_flags = open_flags;
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
    data->thread_rank = (uintptr_t) pthread_self();
    data->errset = bxierr_set_new();
    data->err_max = 10;
    data->bytes_lost = 0;
    data->bytes_written = 0;
    data->dirty = false;

    err2 = _get_file_fd(data);
    BXIERR_CHAIN(err, err2);

    errno = 0;
    struct stat st;
    int rc = fstat(data->fd, &st);
    if (0 != rc) {
        err2 = bxierr_errno("Calling fstat(%s) failed", data->filename);
        BXIERR_CHAIN(err, err2);
        data->buf_size = 4 * 1024  * sizeof(*data->buf) * DEFAULT_BLOCKS_NB;
    } else {
        data->buf_size = ((size_t) st.st_blksize) * sizeof(*data->buf) * DEFAULT_BLOCKS_NB;
    }
//    data->buf_size = 241 * sizeof(*data->buf) * DEFAULT_BLOCKS_NB;
    size_t align = (size_t) sysconf(_SC_PAGESIZE);

    errno = 0;
    rc = posix_memalign((void**) &data->buf, align, data->buf_size);
    if (0 != rc) {
        err2 = bxierr_errno("Calling posix_memalign(%ld, %zu) failed",
                            align, data->buf_size);
        BXIERR_CHAIN(err, err2);
    }

    _tune_io(data);

//    fprintf(stderr, "%d.%d: Initialization: ok\n", data->pid, data->tid);
    return err;
}

bxierr_p _process_exit(bxilog_file_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    if (0 < data->fd) {
//        err2 = _ilog(BXILOG_TRACE, data,
//                     "Total of %zu bytes written (excluding this message)",
//                     data->bytes_written);
//        BXIERR_CHAIN(err, err2);

//        if (bxierr_isko(err)) {
//            char * err_msg = bxierr_str(err);
//            bxilog_rawprint(err_msg, STDERR_FILENO);
//            BXIFREE(err_msg);
//            bxierr_destroy(&err);
//        }

        err2 = _sync(data);
        BXIERR_CHAIN(err, err2);
        errno = 0;
        if (STDOUT_FILENO != data->fd && STDERR_FILENO != data->fd) {
            int rc = close(data->fd);
            if (-1 == rc) {
                err2 = bxierr_errno("Closing logging file '%s' failed", data->filename);
                BXIERR_CHAIN(err, err2);
            }
        }
    }

    if (data->bytes_lost > 0) {
        char * str = bxistr_new("BXI Log File Handler Error Summary:\n"
                                "\tNumber of bytes written: %zu\n"
                                "\tNumber of bytes lost: %zu\n"
                                "\tNumber of reported distinct errors: %zu\n",
                                data->bytes_written,
                                data->bytes_lost,
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
    BXIFREE(data->buf);

//    fprintf(stderr, "%d.%d: process_exit: ok\n", data->pid, data->tid);
    return err;
}

inline bxierr_p _process_implicit_flush(bxilog_file_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

    err2 = _flush(data);
    BXIERR_CHAIN(err, err2);

    err2 = _sync(data);
    BXIERR_CHAIN(err, err2);

    return err;

}

inline bxierr_p _process_explicit_flush(bxilog_file_handler_param_p data) {
    bxierr_p err = BXIERR_OK, err2;

//    err2 = _ilog(BXILOG_TRACE, data, "Flushing requested");
//    BXIERR_CHAIN(err, err2);
//    fprintf(stderr, "Flushing\n");
    err2 = _flush(data);
//    fprintf(stderr, "Flushed\n");
    BXIERR_CHAIN(err, err2);

    err2 = _sync(data);
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

    log_single_line_param_s param = {
                                     .data = data,
                                     .record = record,
                                     .filename = filename,
                                     .funcname = funcname,
                                     .loggername = loggername,
                                     .logmsg = logmsg,
    };
//    fprintf(stderr, "Processing log\n");
    bxierr_p err = bxistr_apply_lines(logmsg,
                                      record->logmsg_len - 1,
                                      (bxierr_p (*)(char*, size_t, bool, void*)) _log_single_line,
                                      &param);
//    fprintf(stderr, "Processed log\n");
//    fprintf(stderr, "%d.%d: process_log of %d.%d: ok\n", data->pid, data->tid, record->pid, record->tid);
    return err;

}


bxierr_p _process_ierr(bxierr_p *err, bxilog_file_handler_param_p data) {
    bxierr_p result = BXIERR_OK;

    if (bxierr_isok(*err)) return *err;

    bool new_err = bxierr_set_add(data->errset, err);
    if (new_err) {
        char * str = bxierr_str(*err);
        bxierr_p fatal = _ilog(BXILOG_ERROR, data,
                               "A bxilog internal error occured:\n %s", str);

        if (bxierr_isko(fatal)) {
            result = bxierr_new(BXILOG_HANDLER_EXIT_CODE, fatal, NULL, NULL, NULL,
                                "Fatal: error while processing internal error: %s",
                                str);
        }
        BXIFREE(str);
    }

    if (data->errset->total_seen_nb > data->err_max) {

        result = bxierr_new(BXILOG_HANDLER_EXIT_CODE,
                            bxierr_from_set(BXILOG_TOO_MANY_IERR,
                                            data->errset,
                                            "Too many errors (%zu > %zu)",
                                            data->errset->total_seen_nb,
                                            data->err_max),
                                            NULL,
                                            NULL,
                                            NULL,
                                            "Fatal, exiting from thread %d",
                                            data->tid);

    }

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

    const size_t prefix_size = FIXED_LOG_SIZE + \
            // Exclude NULL terminating byte from preprocessed length
            data->progname_len - 1 + \
            record->filename_len -1 + \
            record->funcname_len - 1 + \
            record->logname_len - 1 + \
            bxistr_digits_nb(record->line_nb);

    size_t size = prefix_size + line_len;

    if (data->buf_size - data->next_char <= size) {
        bxierr_p err = _flush(data);
        bxierr_abort_ifko(err);
    }

    char * buf;
    if (size > data->buf_size) {
        buf = bximem_calloc(size + 1); // Include the NULL terminating byte since we
                                       // use a specific buffer.
    } else {
        buf = data->buf + data->next_char;
    }

    // Include the NULL terminating byte in the size given to
    // underlying snprintf() call, it is required.
    _mkmsg(prefix_size + 1, buf,
           BXILOG_FILE_HANDLER_LOG_LEVEL_STR[record->level],
           &record->detail_time,
           record->pid,
#ifdef __linux__
           record->tid,
#endif
           record->thread_rank,
           data->progname,
           param->filename,
           record->line_nb,
           param->funcname,
           param->loggername,
           line, line_len);

    if (size > data->buf_size) {
        bxierr_p err = _write(data, buf, size);
        BXIFREE(buf);
        return err;
    }

    data->next_char += (size_t) size;
    data->dirty = true;
    bxiassert(data->next_char <= data->buf_size);

    return BXIERR_OK;
}


size_t _mkmsg(const size_t n, char buf[n],
               const char level,
               const struct timespec * const detail_time,
               const pid_t pid,
#ifdef __linux__
               const pid_t tid,
#endif
               const uintptr_t thread_rank,
               const char * const progname,
               const char * const filename,
               const int line_nb,
               const char * const funcname,
               const char * const loggername,
               const char * const logmsg,
               size_t logmsg_len) {

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
                           loggername);
    memcpy(buf + written, logmsg, logmsg_len);
    buf[(size_t) written + logmsg_len] = '\n';


    // WARNING: Truncation can happen if the logmsg is part of a larger string.
    // Therefore the assertion below might not be true.
    // If we want to guarantee that no truncation can happen, the logmsg in the
    // original record string must be copied, and each '\n' might then be replaced by
    // an '\0'. For speed reason, we don't do that: no copy -> faster.
    //    bxiassert(written < (int)n);

    bxiassert(written >= 0);
    return (size_t) written + logmsg_len;
}

bxierr_p _get_file_fd(bxilog_file_handler_param_p data) {
    errno = 0;
    if (0 == strncmp("-", data->filename, ARRAYLEN("-"))) {
        data->fd = STDOUT_FILENO;
    } else if (0 == strncmp("+", data->filename, ARRAYLEN("+"))) {
        data->fd = STDERR_FILENO;
    } else {
        errno = 0;
        data->fd = open(data->filename,
                        O_WRONLY | data->open_flags,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (-1 == data->fd) return bxierr_errno("Can't open %s", data->filename);
    }

    return BXIERR_OK;
}

void _tune_io(bxilog_file_handler_param_p data) {
    int rc;
    // We just tune, so we don't care on error
    rc = posix_fadvise(data->fd, 0, 0, POSIX_FADV_DONTNEED);
    rc = posix_madvise(data->buf, data->buf_size, POSIX_MADV_SEQUENTIAL);
    UNUSED(rc);
}

inline bxierr_p _flush(bxilog_file_handler_param_p data) {
    if (!data->dirty) return BXIERR_OK;

    bxierr_p err = _write(data, data->buf, data->next_char);
    data->next_char = 0;
    data->dirty = false;
    return err;
}

bxierr_p _write(bxilog_file_handler_param_p data, const void * buf, size_t count) {
    // Do not write more bytes than expected.
    ssize_t written = write(data->fd, buf, count);

    if (0 >= written) {
        if (EPIPE == errno) {
            return bxierr_errno("Can't write to pipe (fd=%d, name=%s). "
                    "Exiting. Some messages will be lost.",
                    data->fd, data->filename);
        }

        bxierr_p bxierr = bxierr_errno("Calling write(fd=%d, name=%s) "
                "failed (written=%d)",
                data->fd, data->filename, written);
        data->bytes_lost += count;
        _record_new_error(data, &bxierr);
    } else {
        data->bytes_written += (size_t) written;
    }
    return BXIERR_OK;
}

bxierr_p _sync(bxilog_file_handler_param_p data) {
    errno = 0;

    UNUSED(data);
//    int rc = fdatasync(data->fd);
//    if (rc != 0) {
//        if (EROFS != errno && EINVAL != errno) {
//            return bxierr_errno("Call to fdatasync() failed");
//        }
//        // OK otherwise, it just means the given FD does not support synchronization
//        // this is the case for example with stdout, stderr...
//    }

//    fprintf(stderr, "%d.%d: Sync: ok\n", data->pid, data->tid);

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

void _record_new_error(bxilog_file_handler_param_p data, bxierr_p * err) {
    bool new = bxierr_set_add(data->errset, err);
    // Only report newly detected errors
    if (new) {
        char * err_str = bxierr_str(*err);
        char * str = bxistr_new("[W] Can't write to '%s' - cause is %s\n"
                "This means "
                "some log lines have been lost\n"
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
        bxierr_destroy(err);
    }
}

