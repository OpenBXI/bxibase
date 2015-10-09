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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/syscall.h>
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
#define ERR_BT_PREFIX   "##trce## "
#define ERR_CODE_PREFIX "##code## "
#define ERR_MSG_PREFIX  "##mesg## "

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************
static void _err_list_init(bxierr_list_p errlist);
static int _bt_full_cb(void *data, uintptr_t pc,
                       const char *filename, int lineno, const char *function);
static void _bt_error_cb(void *data, const char *msg, int errnum);
static char** _pretty_backtrace(void* addresses[], int array_size);
static void __bt_init__(void);
// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

bxierr_define(BXIERR_OK, 0, OK_MSG);

struct backtrace_state * BT_STATE = NULL;

// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************
// Inline functions defined in the .h
extern void bxierr_list_destroy(bxierr_list_p * group_p);
extern void bxierr_set_destroy(bxierr_set_p * group_p);
extern void bxierr_destroy(bxierr_p * self_p);
extern void bxierr_abort_ifko(bxierr_p fatal_err);
extern bool bxierr_isok(bxierr_p self);
extern bool bxierr_isko(bxierr_p self);
extern char* bxierr_str(bxierr_p self);
extern void bxierr_chain(bxierr_p *err, const bxierr_p *tmp);

bxierr_p bxierr_new(int code,
                    void * data,
                    void (*free_fn) (void *),
                    char * (*str)(bxierr_p, uint64_t),
                    bxierr_p cause,
                    const char * fmt,
                    ...) {

    bxierr_p self = bximem_calloc(sizeof(*self));
    self->allocated = true;
    self->code = code;
    self->backtrace = bxierr_backtrace_str();
    self->data = data;
    self->free_fn = free_fn;
    self->str = str;
    if (self->str == NULL) {
        self->str = bxierr_str_limit;
    }
    self->cause = cause;
    if (self->cause != NULL) {
        if (self->cause->last_cause != NULL) {
            self->last_cause = self->cause->last_cause;
        } else {
            self->last_cause = self->cause;
        }
    }

    va_list ap;
    va_start(ap, fmt);
    self->msg_len = bxistr_vnew(&self->msg, fmt, ap);
    va_end(ap);

    return self;
}

void bxierr_free(bxierr_p self) {
    if (NULL == self) return;
    if (!self->allocated) return;
    if (NULL != self->cause) bxierr_destroy(&(self->cause));
    if (NULL != self->free_fn) {
        self->free_fn(self->data);
        self->data = NULL;
    }
    BXIFREE(self->msg);
    BXIFREE(self->backtrace);
    BXIFREE(self);
}

char * bxierr_str_limit(bxierr_p self, uint64_t depth) {
    bxiassert(NULL != self);
    const char * cause_str;
    if (NULL == self->cause) {
        cause_str = bxistr_new("%s", "");
    } else if (2 > depth) {
        size_t remaining = bxierr_get_depth(self->cause);
        cause_str = bxistr_new("...<%zu more causes>", remaining);
    } else {
        if (self->cause->str != NULL) {
            cause_str = self->cause->str(self->cause, depth-1);
        } else {
            cause_str = bxierr_str_limit(self->cause, depth-1);
        }
    }

    bxistr_prefixer_s data;
    bxistr_prefixer_init(&data, ERR_MSG_PREFIX, ARRAYLEN(ERR_MSG_PREFIX) - 1);
    bxierr_p tmp = bxistr_apply_lines(self->msg, bxistr_prefixer_line, &data);
    assert(bxierr_isok(tmp));
    char * final_msg;
    bxistr_join("\n", ARRAYLEN("\n") - 1,
                data.lines, data.lines_len, data.lines_nb,
                &final_msg);

    char * result = bxistr_new(ERR_CODE_PREFIX"%d\n%s\n%s\n%s",
                               self->code, final_msg, self->backtrace,
                               cause_str);

    bxistr_prefixer_cleanup(&data);
    BXIFREE(cause_str);
    BXIFREE(final_msg);

    return result;
}

void bxierr_report(bxierr_p * self, int fd) {
    bxierr_report_keep(*self, fd);
    bxierr_destroy(self);
}

void bxierr_report_keep(bxierr_p self, int fd) {
    if (bxierr_isok(self)) return;

    char * errstr = bxierr_str(self);
    ssize_t count = write(fd, errstr, strlen(errstr));
    bxiassert(0 < count);
    BXIFREE(errstr);
}

bxierr_p bxierr_get_ok() { return BXIERR_OK; }

size_t bxierr_get_depth(bxierr_p self) {
//    bxiassert(NULL != self);

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
    bxiassert(NULL != fmt);

    va_list ap;
    va_start(ap, fmt);
    bxierr_p result = bxierr_vfromidx(erridx, erridx2str, fmt, ap);
    va_end(ap);

    return result;
}

bxierr_p bxierr_vfromidx(const int erridx,
                         const char * const * const erridx2str,
                         const char * const fmt, va_list ap) {
    bxiassert(NULL != fmt);

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
                     "<can't get related error message for errno=%d,"
                     " strerror_r() failed with rc = %d>", erridx, rc);
        }
        errmsg = buf;
#endif
    }

    char * msg;
    bxistr_vnew(&msg, fmt, ap);

    bxierr_p result = bxierr_new(erridx, NULL, NULL, NULL, NULL, "%s: %s", msg, errmsg);
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
    char **symbols = backtrace_symbols(addresses, c);
    char **strings = _pretty_backtrace(addresses, c);

#ifdef __linux__
    pid_t tid = (pid_t) syscall(SYS_gettid);
#else
    int tid;
    bxierr_p err = bxilog_get_thread_rank(&tid);
    if (BXIERR_OK != err) tid = -1;
#endif
    const char * const truncated = (c == BACKTRACE_MAX) ? "(truncated) " : "";

    fprintf(faked_file,
            ERR_BT_PREFIX"Backtrace of tid %d: %d function calls %s\n",
            tid, c, truncated);
    for(int i = 0; i < c; i++) {
        fprintf(faked_file, ERR_BT_PREFIX"[%02d] %s\n", i,
                NULL == strings[i] ? symbols[i] : strings[i]);
        BXIFREE(strings[i]);
    }
    fprintf(faked_file,ERR_BT_PREFIX"Backtrace end\n");
    fclose(faked_file);
    BXIFREE(symbols);
    BXIFREE(strings);
    return buf;
}

void bxierr_assert_fail(const char *assertion, const char *file,
                        unsigned int line, const char *function) {

    bxierr_p err = bxierr_new(BXIASSERT_CODE,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              "%s:%d - %s(): wrong assertion: %s"\
                              BXIBUG_STD_MSG,
                              file, line, function, assertion);
    bxierr_report(&err, STDERR_FILENO);
    abort();
}

void bxierr_unreachable_statement(const char *file,
                                  unsigned int line,
                                  const char * function) {
    bxierr_p err = bxierr_new(BXIUNREACHABLE_STATEMENT_CODE,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              "Unreachable statement reached at %s:%d in %s()."\
                              BXIBUG_STD_MSG,
                              file, line, function);
    bxierr_report(&err, STDERR_FILENO);
    abort();
}


bxierr_list_p bxierr_list_new() {
    bxierr_list_p result = bximem_calloc(sizeof(*result));

    _err_list_init(result);

    return result;
}

void bxierr_list_free(bxierr_list_p errlist) {
    if (NULL == errlist) return;

    for (size_t i = 0; i < errlist->errors_size; i++) {
        bxierr_p err = errlist->errors[i];
        if (NULL == err) continue;
        bxierr_destroy(&err);
    }
    BXIFREE(errlist->errors);
    BXIFREE(errlist);
}

void bxierr_list_append(bxierr_list_p list, bxierr_p err) {
    if (list->errors_nb >= list->errors_size) {
        // Not enough space, allocate some new
        // Add one to prevent product with zero!
        size_t new_len = (list->errors_size + 1) * 2;
        list->errors = bximem_realloc(list->errors,
                                      list->errors_size*sizeof(*list->errors),
                                      new_len*sizeof(*list->errors));
        list->errors_size = new_len;
    }
    list->errors[list->errors_nb] = err;
    list->errors_nb++;
}

char * bxierr_list_str(bxierr_p err, uint64_t depth) {
    char * str = bxierr_str_limit(err, depth);
    bxierr_list_p group = err->data;
    char ** ierr_lines = bximem_calloc(group->errors_nb * sizeof(*ierr_lines));
    size_t * ierr_lines_len = bximem_calloc(group->errors_nb * sizeof(*ierr_lines_len));
    size_t current = 0;
    bxistr_prefixer_s prefixer;
    char * final_s;
    // Create the string for each internal error in the group
    for (size_t i = 0; i < group->errors_size; i++) {
        bxierr_p ierr = group->errors[i];
        if (NULL == ierr) continue;
        char * s = bxierr_str_limit(ierr, depth);
        // Prefix all line
        char * prefix = bxistr_new("##egrp %zu", current);
        bxistr_prefixer_init(&prefixer, prefix, strlen(prefix));
        bxistr_apply_lines(s, bxistr_prefixer_line, &prefixer);
        // Join all lines
        ierr_lines_len[current] = bxistr_join("\n", ARRAYLEN("\n") - 1,
                                         prefixer.lines, prefixer.lines_len,
                                         prefixer.lines_nb,
                                         &final_s);
        ierr_lines[current] = final_s;
        bxistr_prefixer_cleanup(&prefixer);
        current++;
    }
    // Join all lines from each internal error into one big string
    bxistr_join("\n", ARRAYLEN("\n") - 1,
                ierr_lines, ierr_lines_len,
                current,
                &final_s);
    char * result = bxistr_new("%s%s", str, final_s);
    BXIFREE(str);
    BXIFREE(final_s);
    return result;
}

bxierr_set_p bxierr_set_new() {
    bxierr_set_p result = bximem_calloc(sizeof(*result));

    _err_list_init(&result->distinct_err);

    result->seen_nb = bximem_calloc(result->distinct_err.errors_size * sizeof(*result->seen_nb));

    return result;
}

void bxierr_set_free(bxierr_set_p errset) {
    if (NULL == errset) return;

    BXIFREE(errset->seen_nb);
    bxierr_list_free(&errset->distinct_err);
}

bool bxierr_set_add(bxierr_set_p set, bxierr_p * err) {
    bxiassert(NULL != set);
    bxiassert(NULL != err);
    bxiassert(NULL != *err);

    set->total_seen_nb++;
    size_t i = 0;
    bxierr_p slot = NULL;
    for (; i < set->distinct_err.errors_size; i++) {
        slot = set->distinct_err.errors[i];
        if (NULL == slot) break;
        if (slot->code == (*err)->code) {
            // An error with same code is already recorded, delete the new one.
            bxierr_destroy(err);
            set->seen_nb[i]++;
            return false;
        }
    }
    if (i >= set->distinct_err.errors_size) {
        // Not enough space, allocate some new
        size_t old_len = set->distinct_err.errors_size;
        size_t new_len = set->distinct_err.errors_size * 2;
        set->distinct_err.errors = bximem_realloc(set->distinct_err.errors,
                                                  old_len * sizeof(*set->distinct_err.errors),
                                                  new_len*sizeof(*set->distinct_err.errors));
        set->seen_nb = bximem_realloc(set->seen_nb,
                                              old_len * sizeof(*set->seen_nb),
                                              new_len*sizeof(*set->seen_nb));
        set->distinct_err.errors_size = new_len;
    }
    bxiassert(NULL == slot);
    set->distinct_err.errors[i] = *err;
    set->distinct_err.errors_nb++;
    set->seen_nb[i]++;
    return true;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************
void _err_list_init(bxierr_list_p errlist) {

    errlist->errors_size = 16;
    errlist->errors = bximem_calloc(errlist->errors_size * sizeof(*errlist->errors));
}


char** _pretty_backtrace(void* addresses[], int array_size) {
    // Used to return the strings generated from the addresses
    char** backtrace_strings = bximem_calloc(sizeof(*backtrace_strings) * (unsigned)array_size);
    for(int i = 0; i < array_size; i++) {
        int rc = backtrace_pcinfo(BT_STATE,
                                  (uintptr_t) addresses[i],
                                  _bt_full_cb,
                                  _bt_error_cb,
                                  &backtrace_strings[i]);
        bxiassert(0 == rc);
    }
    return backtrace_strings;
}

__attribute__((constructor)) void __bt_init__(void) {
    BT_STATE = backtrace_create_state(NULL,
                                      BACKTRACE_SUPPORTS_THREADS,
                                      _bt_error_cb, NULL);
    bxiassert(NULL != BT_STATE);
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

