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

#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/err.h"

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
#define OK_MSG "No problem found - everything is ok"

#define BACKTRACE_MAX 1024
// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

bxierr_define(BXIERR_OK, 0, OK_MSG);

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

// Inline functions defined in the .h
extern bool bxierr_isok(bxierr_p self);
extern bool bxierr_isko(bxierr_p self);
extern char* bxierr_str(bxierr_p self);

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
    char **strings = backtrace_symbols(addresses,c);
    if (NULL == strings) {
        error(0, errno, "Calling backtrace_symbols() failed");
        return strdup("Unavailable backtrace (backtrace_symbols() failed)");
    }
#ifdef __linux__
    pid_t tid = (pid_t) syscall(SYS_gettid);
#else
    int tid;
    bxierr_p err = bxilog_get_thread_rank(&tid);
    if (BXIERR_OK != err) tid = -1;
#endif
    fprintf(faked_file, "#### Backtrace of tid %u: %d function calls ####\n", tid, c);
    for(int i = 0; i < c; i++) {
        fprintf(faked_file, "#### [%2d] %s\n", i, strings[i]);
    }
    fprintf(faked_file,"#### Backtrace end ####\n");
    fclose(faked_file);
    BXIFREE(strings);
    return buf;
}

