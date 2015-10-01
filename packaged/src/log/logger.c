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
#include <error.h>
#include <sysexits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <libgen.h>
#include <fcntl.h>
#include <execinfo.h>
#include <time.h>
#include <errno.h>


#include <limits.h>

#include <pthread.h>

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <sys/ioctl.h>

#include <zmq.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

#include "log_impl.h"
#include "tsd_impl.h"
#include "fork_impl.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************
#define RETRIES_MAX 3u

#define RETRY_DELAY 500000l

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************



//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static bxierr_p _send2handlers(const bxilog_p logger, const bxilog_level_e level,
                            void * log_channel,
#ifdef __linux__
                            pid_t tid,
#endif
                            uint16_t thread_rank,
                            char * filename, size_t filename_len,
                            const char * funcname, size_t funcname_len,
                            int line,
                            const char * rawstr, size_t rawstr_len);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The internal logger
SET_LOGGER(LOGGER, "bxi.base.log.logger");

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


bxilog_level_e bxilog_get_level(const bxilog_p logger) {
    bxiassert(NULL != logger);
    return logger->level;
}

void bxilog_set_level(const bxilog_p logger, const bxilog_level_e level) {
    bxiassert(NULL != logger);
    bxiassert(BXILOG_LOWEST >= level);
    logger->level = level;
}

// Defined inline in bxilog.h
extern bool bxilog_is_enabled_for(const bxilog_p logger, const bxilog_level_e level);

void bxilog_destroy(bxilog_p * self_p) {
    if (NULL == *self_p) return;
    if (!(*self_p)->allocated) return;

    BXIFREE((*self_p)->name);
    BXIFREE(*self_p);
}


bxierr_p bxilog_log_rawstr(const bxilog_p logger, const bxilog_level_e level,
                           char * filename, size_t filename_len,
                           const char * funcname, size_t funcname_len,
                           int line,
                           const char * rawstr, size_t rawstr_len) {
    if (INITIALIZED != BXILOG__GLOBALS->state) return BXIERR_OK;
    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;

    err = _send2handlers(logger, level, tsd->data_channel,
#ifdef __linux__
                    tsd->tid,
#endif
                    tsd->thread_rank,
                    filename, filename_len,
                    funcname, funcname_len,
                    line,
                    rawstr, rawstr_len);
    return err;
}

bxierr_p bxilog_vlog_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                  char * filename, size_t filename_len,
                                  const char * funcname, size_t funcname_len,
                                  const int line,
                                  const char * fmt, va_list arglist) {

    if (INITIALIZED != BXILOG__GLOBALS->state) return BXIERR_OK;

    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;

    // Start creating the logmsg so the length can be computed
    // We start in the thread local buffer

    char * logmsg = tsd->log_buf;
    size_t logmsg_len = BXILOG__GLOBALS->param->tsd_log_buf_size;
    bool logmsg_allocated = false; // When true,  means that a new special buffer has been
                                   // allocated -> it will have to be freed
    while(true) {
        va_list arglist_copy;
        va_copy(arglist_copy, arglist);
        // Does not include the null terminated byte
        int n = vsnprintf(logmsg, logmsg_len, fmt, arglist_copy);
        va_end(arglist_copy);

        // Check error
        bxiassert(n >= 0);

        // Ok
        if ((size_t) n < logmsg_len) {
            logmsg_len = (size_t) n + 1; // Record the actual size of the message
            break;
        }

        if (logmsg_allocated) BXIFREE(logmsg);
        // Not enough space, mallocate a new special buffer of the precise size
        logmsg_len = (size_t) (n + 1);
        logmsg = malloc(logmsg_len); // Include the null terminated byte
        bxiassert(NULL != logmsg);
        logmsg_allocated = true;

        tsd->rsz_log_nb++;
    }

    tsd->max_log_size = tsd->max_log_size > logmsg_len ? tsd->max_log_size : logmsg_len;
    tsd->min_log_size = tsd->min_log_size < logmsg_len ? tsd->min_log_size : logmsg_len;
    tsd->sum_log_size += logmsg_len;
    tsd->log_nb++;

    err = _send2handlers(logger, level, tsd->data_channel,
#ifdef __linux__
                    tsd->tid,
#endif
                    tsd->thread_rank,
                    filename, filename_len,
                    funcname, funcname_len,
                    line,
                    logmsg, logmsg_len);

    if (logmsg_allocated) BXIFREE(logmsg);
    // Either record comes from the stack
    // or it comes from a mallocated region that will be freed
    // by bxizmq_snd_data_zc itself
    // Therefore it should not be freed here
    // FREE(record);
    return err;
}

bxierr_p bxilog_log_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                 char * filename, size_t filename_len,
                                 const char * funcname, size_t funcname_len,
                                 const int line,
                                 const char * fmt, ...) {
    va_list ap;
    bxierr_p err;

    va_start(ap, fmt);
    err = bxilog_vlog_nolevelcheck(logger, level,
                                   filename, filename_len, funcname, funcname_len, line,
                                   fmt, ap);
    va_end(ap);

    return err;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _send2handlers(const bxilog_p logger,
                        const bxilog_level_e level,
                        void * log_channel,
#ifdef __linux__
                        pid_t tid,
#endif
                        uint16_t thread_rank,
                        char * filename, size_t filename_len,
                        const char * funcname, size_t funcname_len,
                        int line,
                        const char * rawstr, size_t rawstr_len) {

    bxilog_record_p record;
    char * data;

    size_t var_len = filename_len\
            + funcname_len\
            + logger->name_length;

    size_t data_len = sizeof(*record) + var_len + rawstr_len;

    // We need a mallocated buffer to prevent ZMQ from making its own copy
    // We use malloc() instead of calloc() for performance reason
    record = malloc(data_len);
    bxiassert(NULL != record);
    // Fill the buffer
    record->level = level;
    bxierr_p err = bxitime_get(CLOCK_REALTIME, &record->detail_time);
    // TODO: be smarter here, if the time can't be fetched, just fake it!
    if (bxierr_isko(err)) {
        BXIFREE(record);
        return err;
    }
    record->pid = BXILOG__GLOBALS->pid;
#ifdef __linux__
    record->tid = tid;
#endif
    record->thread_rank = thread_rank;
    record->line_nb = line;
    record->filename_len = filename_len;
    record->funcname_len = funcname_len;
    record->logname_len = logger->name_length;
    record->variable_len = var_len;
    record->logmsg_len = rawstr_len;

    // Now copy the rest after the record
    data = (char *) record + sizeof(*record);
    memcpy(data, filename, filename_len);
    data += filename_len;
    memcpy(data, funcname, funcname_len);
    data += funcname_len;
    memcpy(data, logger->name, logger->name_length);
    data += logger->name_length;
    memcpy(data, rawstr, rawstr_len);

    // Send the frame
    // normal version if record comes from the stack 'buf'
    //    size_t retries = bxizmq_snd_data(record, data_len,
    //                                      _get_tsd()->zocket, ZMQ_DONTWAIT,
    //                                      RETRIES_MAX, RETRY_DELAY);

    // Zero-copy version (if record has been mallocated).
    err = bxizmq_data_snd_zc(record, data_len,
                             log_channel, ZMQ_DONTWAIT,
                             RETRIES_MAX, RETRY_DELAY,
                             bxizmq_data_free, NULL);
    if (bxierr_isko(err)) {
        if (BXIZMQ_RETRIES_MAX_ERR != err->code) return err;
        // Recursive call!
        WARNING(LOGGER,
                "Sending last log required %lu retries", (long) err->data);
        bxierr_destroy(&err);
    }
    return BXIERR_OK;
}
