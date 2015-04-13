/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <unistd.h>
#include <execinfo.h>
#include <error.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/syscall.h>
//#include <sys/types.h>
//#include <regex.h>
//#include <dlfcn.h>
#include <backtrace.h>
#include <backtrace-supported.h>

#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/err.h"

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
#define OK_MSG "No problem found - everything is ok"

#define BACKTRACE_MAX 64 // Number of maximum depth of a backtrace

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************
static int _bt_full_cb(void *data, uintptr_t pc,
                       const char *filename, int lineno, const char *function);
static void _bt_error_cb(void *data, const char *msg, int errnum);
static char** _pretty_backtrace(void* addresses[], size_t array_size);
static void __bt_init__(void);
// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

bxierr_define(BXIERR_OK, 0, OK_MSG);

struct backtrace_state * BT_STATE = NULL;

// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************

bxierr_p bxierr_new(int code,
                    void * data,
                    void (*free_fn) (void *),
                    bxierr_p cause,
                    const char * fmt,
                    ...) {

    bxierr_p self = bximem_calloc(sizeof(*self));
    self->allocated = true;
    self->code = code;
    self->backtrace = bxierr_backtrace_str();
    self->data = data;
    self->free_fn = free_fn;
    self->cause = cause;

    va_list ap;
    va_start(ap, fmt);
    self->msg_len = bxistr_vnew(&self->msg, fmt, ap);
    va_end(ap);

    return self;
}

void bxierr_destroy(bxierr_p * self_p) {
    assert(NULL != self_p);

    bxierr_p self = *self_p;
    if (NULL == self) return;
    if (!self->allocated) return;
    if (NULL != self->cause) bxierr_destroy(&(self->cause));
    if (NULL != self->free_fn) self->free_fn(self->data);
    BXIFREE(self->msg);
    BXIFREE(self->backtrace);
    BXIFREE(*self_p);
}

char * bxierr_str_limit(bxierr_p self, uint64_t depth) {
    assert(NULL != self);
    const char * cause_str;
    if (NULL == self->cause) {
        cause_str = bxistr_new("%s", "");
    } else if (2 > depth) {
        size_t remaining = bxierr_get_depth(self->cause);
        cause_str = bxistr_new("...<%zu more causes>", remaining);
    } else {
        cause_str = bxierr_str_limit(self->cause, depth-1);
    }
    char * result = bxistr_new("msg='%s' code=%d backtrace=%s\n%s",
                               self->msg, self->code, self->backtrace,
                               cause_str);
    BXIFREE(cause_str);
    return result;
}

void bxierr_report(bxierr_p self, int fd) {
    if (bxierr_isok(self)) return;

    char * errstr = bxierr_str(self);
    char * msg = bxistr_new("Reporting error: %s\n", errstr);
    ssize_t count = write(fd, msg, strlen(msg) + 1);
    assert(0 < count);
    BXIFREE(msg);
    BXIFREE(errstr);
    bxierr_destroy(&self);
}

// Inline functions defined in the .h
extern bool bxierr_isok(bxierr_p self);
extern bool bxierr_isko(bxierr_p self);
extern char* bxierr_str(bxierr_p self);
extern void bxierr_chain(bxierr_p *err, const bxierr_p *tmp);

bxierr_p bxierr_get_ok() { return BXIERR_OK; }

size_t bxierr_get_depth(bxierr_p self) {
//    assert(NULL != self);

    bxierr_p current = self;
    size_t result = 0;
    while (current != NULL) {
        result++;
        current = current->cause;
    }
    return result;
}

bxierr_p bxierr_fromidx(const int erridx,
                        const char * const * const erridx2str,
                        const char * const fmt, ...) {
    assert(NULL != fmt);

    va_list ap;
    va_start(ap, fmt);
    bxierr_p result = bxierr_vfromidx(erridx, erridx2str, fmt, ap);
    va_end(ap);

    return result;
}

bxierr_p bxierr_vfromidx(const int erridx,
                         const char * const * const erridx2str,
                         const char * const fmt, va_list ap) {
    assert(NULL != fmt);

    char * errmsg;
    char buf[ERR2STR_MAX_SIZE];
    if (erridx2str != NULL) {
        strncpy(buf, erridx2str[erridx], ERR2STR_MAX_SIZE);
        errmsg = buf;
    } else {
#ifdef _GNU_SOURCE
// WARNING: we use the GNU version of strerror_r(), not the POSIX ONE
        errmsg = strerror_r(erridx, buf, ERR2STR_MAX_SIZE);
#else
        int rc = strerror_r(erridx, buf, ERR2STR_MAX_SIZE);
        if (rc != 0) {
            snprintf(buf, ERR2STR_MAX_SIZE,
                     "<can't get related error message,"
                     " strerror_r() failed with rc = %d>", rc);
        }
        errmsg = buf;
#endif
    }

    char * msg;
    bxistr_vnew(&msg, fmt, ap);

    bxierr_p result = bxierr_new(erridx, NULL, NULL, NULL, "%s: %s", msg, errmsg);
    BXIFREE(msg);

    return result;
}

char** _pretty_backtrace(void* addresses[], size_t array_size) {
    // Used to return the strings generated from the addresses
    char** backtrace_strings = bximem_calloc(sizeof(*backtrace_strings) * array_size);
    for(size_t i = 0; i < array_size; i++) {
        int rc = backtrace_pcinfo(BT_STATE,
                                  (uintptr_t) addresses[i],
                                  _bt_full_cb,
                                  _bt_error_cb,
                                  backtrace_strings + i);
        assert(0 == rc);
    }
    return backtrace_strings;
}

char * bxierr_backtrace_str(void) {
    char * buf = NULL;
    size_t size;
    errno = 0;
    FILE * faked_file = open_memstream(&buf, &size);
    if (NULL == faked_file) {
        error(0, errno, "Calling open_memstream() failed");
        return strdup("Unavailable backtrace (open_memstream() failed)");
    }
    void *addresses[BACKTRACE_MAX];
    int c = backtrace(addresses, BACKTRACE_MAX);
    errno = 0;
    char **symbols = backtrace_symbols(addresses,c);
    char **strings = _pretty_backtrace(addresses, (unsigned long) c);

#ifdef __linux__
    pid_t tid = (pid_t) syscall(SYS_gettid);
#else
    int tid;
    bxierr_p err = bxilog_get_thread_rank(&tid);
    if (BXIERR_OK != err) tid = -1;
#endif
    const char * const truncated = (c == BACKTRACE_MAX) ? "(truncated) " : "";

    fprintf(faked_file,
            "##bt## Backtrace of tid %u: %d function calls %s\n",
            tid, c, truncated);
    for(int i = 0; i < c; i++) {
        fprintf(faked_file, "##bt## [%02d] %s\n", i,
                NULL == strings[i] ? symbols[i] : strings[i]);
        BXIFREE(strings[i]);
    }
    fprintf(faked_file,"##bt## Backtrace end\n");
    fclose(faked_file);
    BXIFREE(symbols);
    BXIFREE(strings);
    return buf;
}

__attribute__((constructor)) void __bt_init__(void) {
    BT_STATE = backtrace_create_state(NULL,
                                      BACKTRACE_SUPPORTS_THREADS,
                                      _bt_error_cb, NULL);
    assert(NULL != BT_STATE);
}

void _bt_error_cb(void *data, const char *msg, int errnum) {
    UNUSED(data);
    UNUSED(errnum);
    // errnum == -1 means debug info are not available, so we don't care,
    // we will just display the classical information
    if (-1 != errnum) {
        // Otherwise, display what the problem is
        fprintf(stderr, "Warning: %s\n", msg);
    }
}

int _bt_full_cb(void *data, uintptr_t pc,
                const char *filename, int lineno, const char *function) {
    UNUSED(pc);
    if (NULL != filename && 0 != lineno && NULL != function) {
        char * str = bxistr_new("%s:%d - %s()",
                                filename, lineno, function);
        (* (char **) data) = str;
    }
    return 0;
}
