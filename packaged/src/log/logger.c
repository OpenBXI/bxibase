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

#include "bxi/base/log/logger.h"

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
static bxierr_p _send2handlers(const bxilog_logger_p logger, const bxilog_level_e level,
                               void * log_channel,
#ifdef __linux__
                               pid_t tid,
#endif
                               uint16_t thread_rank,
                               const char * filename, size_t filename_len,
                               const char * funcname, size_t funcname_len,
                               int line,
                               const char * rawstr, size_t rawstr_len);
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The internal logger
SET_LOGGER(LOGGER, BXILOG_LIB_PREFIX "bxilog.logger");

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


bxilog_level_e bxilog_logger_get_level(const bxilog_logger_p logger) {
    bxiassert(NULL != logger);
    return logger->level;
}

void bxilog_logger_set_level(const bxilog_logger_p logger, const bxilog_level_e level) {
    bxiassert(NULL != logger);
    bxiassert(BXILOG_LOWEST >= level);
    logger->level = level;
}

void bxilog_logger_reconfigure(const bxilog_logger_p logger) {
    if (NULL == BXILOG__GLOBALS || NULL == BXILOG__GLOBALS->config) return;
    bxilog_level_e minimum_level = BXILOG_OFF; // The minimum level required for the current logger
                                               // across all handlers
    for (size_t i = 0; i < BXILOG__GLOBALS->config->handlers_nb; i++) {
        const bxilog_filters_p filters = BXILOG__GLOBALS->config->handlers_params[i]->filters;
        size_t best_match_len = 0; // The length of the most precise matching filter
                                   // inside each handler
        bxilog_level_e best_match_level = BXILOG_LOWEST; // The best match level inside
                                                         // each handler
        // First, look after the most precise filter in the current handler
        for (size_t f = 0; f < filters->nb; f++) {
            const bxilog_filter_p filter = filters->list[f];
            const size_t filter_pre_len = strlen(filter->prefix);
            if (logger->name_length < filter_pre_len) continue;
            if (0 != strncmp(filter->prefix, logger->name, filter_pre_len)) continue;
            // This filter prefix matches the logger name
            // Let see if it is the maximum match
            if (best_match_len > filter_pre_len) continue;
            // We found a new good match, make it the last best known
            best_match_len = filter_pre_len;
            best_match_level = filter->level;
        }
        // At that stage, we know the most precise filter's level
        // However, other handlers might have other requirements
        // If another handler specifies a more detailed level, it must be set in
        // the logger, otherwise, it won't be seen by expected handlers.
        // Therefore, the minimum level is the one with the greatest details across
        // handlers
        if (best_match_level < minimum_level) continue;
        minimum_level = best_match_level;
    }
    logger->level = minimum_level;
}


// Defined inline in bxilog.h
extern bool bxilog_logger_is_enabled_for(const bxilog_logger_p logger,
                                         const bxilog_level_e level);


void bxilog_logger_free(bxilog_logger_p self) {
    if (NULL == self) return;
    if (!(self)->allocated) return;

    BXIFREE((self)->name);
    BXIFREE(self);
}

void bxilog_logger_destroy(bxilog_logger_p * self_p) {
    if (NULL == *self_p) return;
    bxilog_logger_free(*self_p);
    *self_p = NULL;
}

bxierr_p bxilog_logger_log_rawstr(const bxilog_logger_p logger, const bxilog_level_e level,
                                  const char * const filename, const size_t filename_len,
                                  const char * const funcname, const size_t funcname_len,
                                  const int line,
                                  const char * const rawstr, const size_t rawstr_len) {
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

bxierr_p bxilog_logger_vlog_nolevelcheck(const bxilog_logger_p logger,
                                         const bxilog_level_e level,
                                         const char * const fullfilename, const size_t fullfilename_len,
                                         const char * const funcname, const size_t funcname_len,
                                         const int line,
                                         const char * const fmt, va_list arglist) {

    if (INITIALIZED != BXILOG__GLOBALS->state) return BXIERR_OK;

    tsd_p tsd;
    bxierr_p err = bxilog__tsd_get(&tsd);
    if (bxierr_isko(err)) return err;

    // Start creating the logmsg so the length can be computed
    // We start in the thread local buffer

    char * logmsg = tsd->log_buf;
    size_t logmsg_len = BXILOG__GLOBALS->config->tsd_log_buf_size;
    bool logmsg_allocated = false; // When true,  means that a new special buffer has been
                                   // allocated -> it will have to be freed
    while (true) {
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

    bxiassert(bxierr_isok(err));

    const char * filename;
    size_t filename_len = bxistr_rsub(fullfilename, fullfilename_len, '/', &filename);

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

bxierr_p bxilog_logger_log_nolevelcheck(const bxilog_logger_p logger, const bxilog_level_e level,
                                        char * filename, size_t filename_len,
                                        const char * funcname, size_t funcname_len,
                                        const int line,
                                        const char * fmt, ...) {
    va_list ap;
    bxierr_p err;

    va_start(ap, fmt);
    err = bxilog_logger_vlog_nolevelcheck(logger, level,
                                          filename, filename_len,
                                          funcname, funcname_len,
                                          line,
                                          fmt, ap);
    va_end(ap);

    return err;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

bxierr_p _send2handlers(const bxilog_logger_p logger,
                        const bxilog_level_e level,
                        void * const log_channel,
#ifdef __linux__
                        const pid_t tid,
#endif
                        const uint16_t thread_rank,
                        const char * const filename, const size_t filename_len,
                        const char * const funcname, const size_t funcname_len,
                        const int line,
                        const char * const rawstr, const size_t rawstr_len) {

    bxierr_p err = BXIERR_OK, err2;
    bxilog_record_p record;
    char * data;

    size_t var_len = filename_len + funcname_len + logger->name_length;
    size_t data_len = sizeof(*record) + var_len + rawstr_len;

    // We need a mallocated buffer to prevent ZMQ from making its own copy
    // We use malloc() instead of calloc() for performance reason
    record = malloc(data_len);
    bxiassert(NULL != record);
    // Fill the buffer
    record->level = level;


    err2 = bxitime_get(CLOCK_REALTIME, &record->detail_time);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) {
        char * err_str = bxierr_str(err);
        fprintf(stderr, "[W] Calling bxitime_get() failed: %s\n", err_str);
        bxierr_destroy(&err);
        BXIFREE(err_str);
        record->detail_time.tv_sec = 0;
        record->detail_time.tv_nsec = 0;
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

    for (size_t i = 0; i< BXILOG__GLOBALS->internal_handlers_nb; i++) {
        // Send the frame
        // normal version if record comes from the stack 'buf'
        err2 = bxizmq_data_snd(record, data_len,
                               log_channel, ZMQ_DONTWAIT,
                               RETRIES_MAX, RETRY_DELAY);

        // Zero-copy version (if record has been mallocated).
//        err = bxizmq_data_snd_zc(record, data_len,
//                                 log_channel, ZMQ_DONTWAIT,
//                                 RETRIES_MAX, RETRY_DELAY,
//                                 bxizmq_data_free, NULL);
        BXIERR_CHAIN(err, err2);
    }
    BXIFREE(record);
    return err;
}
