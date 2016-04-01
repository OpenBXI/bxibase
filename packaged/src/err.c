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
#define CAUSED_BY_STR "... caused by:"

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
const char * BXIERR_CAUSED_BY_STR = CAUSED_BY_STR;
const int BXIERR_CAUSED_BY_STR_LEN = ARRAYLEN(CAUSED_BY_STR);

struct backtrace_state * BT_STATE = NULL;

// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************
// Inline functions defined in the .h
extern void bxierr_report_add_from(bxierr_p self, bxierr_report_p report);
extern void bxierr_report_destroy(bxierr_report_p *);
extern void bxierr_destroy(bxierr_p * self_p);
extern void bxierr_abort_ifko(bxierr_p fatal_err);
extern bool bxierr_isok(bxierr_p report);
extern bool bxierr_isko(bxierr_p report);
extern char* bxierr_str(bxierr_p report);
extern void bxierr_chain(bxierr_p *err, const bxierr_p *tmp);
extern void bxierr_list_destroy(bxierr_list_p * group_p);
extern void bxierr_set_destroy(bxierr_set_p * group_p);

bxierr_p bxierr_new(int code,
                    void * data,
                    void (*free_fn) (void *),
                    void (*add_to_report)(bxierr_p, bxierr_report_p, size_t),
                    bxierr_p cause,
                    const char * fmt,
                    ...) {

    bxierr_p self = bximem_calloc(sizeof(*self));
    self->allocated = true;
    self->code = code;
    char * tmp = NULL;
    self->backtrace_len = bxierr_backtrace_str(&tmp);
    self->backtrace = tmp;
    self->data = data;
    self->free_fn = free_fn;
    self->add_to_report = (NULL == add_to_report) ? bxierr_report_add_from_limit : add_to_report;
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


bxierr_report_p bxierr_report_new() {
    bxierr_report_p self = bximem_calloc(sizeof(*self));
    self->err_nb = 0;
    self->allocated = 1;
    self->err_msglens = bximem_calloc(self->allocated * sizeof(*self->err_msglens));
    self->err_btslens = bximem_calloc(self->allocated * sizeof(*self->err_btslens));
    self->err_msgs = bximem_calloc(self->allocated * sizeof(*self->err_msgs));
    self->err_bts = bximem_calloc(self->allocated * sizeof(*self->err_bts));

    return self;
}

void bxierr_report_add(bxierr_report_p report,
                       char * err_msg, const size_t err_msglen,
                       char * err_bt, size_t err_btlen) {

    if (report->err_nb >= report->allocated) {
        size_t new_size = 2 * report->allocated;
        report->err_msglens = bximem_realloc(report->err_msglens,
                                             report->allocated * sizeof(*report->err_msglens),
                                             new_size * sizeof(*report->err_msglens));
        report->err_btslens = bximem_realloc(report->err_btslens,
                                           report->allocated * sizeof(*report->err_btslens),
                                           new_size * sizeof(*report->err_btslens));

        report->err_msgs = bximem_realloc(report->err_msgs,
                                        report->allocated * sizeof(*report->err_msgs),
                                        new_size * sizeof(*report->err_msgs));
        report->err_bts = bximem_realloc(report->err_bts,
                                       report->allocated * sizeof(*report->err_btslens),
                                       new_size * sizeof(*report->err_btslens));
        report->allocated = new_size;
    }

    report->err_msgs[report->err_nb] = strdup(err_msg);
    report->err_msglens[report->err_nb] = err_msglen;

    report->err_bts[report->err_nb] = strdup(err_bt);
    report->err_btslens[report->err_nb] = err_btlen;

    report->err_nb++;
}

void bxierr_report_free(bxierr_report_p report) {
    if (NULL == report) return;

    for (size_t i = 0; i < report->err_nb; i++) {
        BXIFREE(report->err_bts[i]);
        BXIFREE(report->err_msgs[i]);
    }
    BXIFREE(report->err_msglens);
    BXIFREE(report->err_btslens);
    BXIFREE(report->err_msgs);
    BXIFREE(report->err_bts);
}


void bxierr_report_add_from_limit(bxierr_p self, bxierr_report_p report, size_t depth) {
    bxiassert(NULL != self);
    bxiassert(NULL != report);

    bxistr_prefixer_s data;
    bxistr_prefixer_init(&data, ERR_MSG_PREFIX, ARRAYLEN(ERR_MSG_PREFIX) - 1);
    bxierr_p tmp = bxistr_apply_lines(self->msg,
                                      self->msg_len,
                                      bxistr_prefixer_line, &data);
    assert(bxierr_isok(tmp));
    char * final_msg;
    bxistr_join("\n", ARRAYLEN("\n") - 1,
                data.lines, data.lines_len, data.lines_nb,
                &final_msg);

    char * result = bxistr_new(ERR_CODE_PREFIX"%d\n%s", self->code, final_msg);

    char * bt = (NULL == self->backtrace) ? "" : self->backtrace;
    size_t bt_len = (NULL == self->backtrace) ? 1 : (self->backtrace_len + 1);

    bxierr_report_add(report, result, strlen(result) + 1, bt, bt_len);
    BXIFREE(result);

    if (NULL == self->cause) {
        // Nothing to do!
    } else if (2 > depth) {
        size_t remaining = bxierr_get_depth(self->cause);
        char * cause_str = bxistr_new("...<%zu more causes>", remaining);
        bxierr_report_add(report, cause_str, strlen(cause_str) + 1, "", 1);
        BXIFREE(cause_str);
    } else {
        bxierr_report_add(report, CAUSED_BY_STR, ARRAYLEN(CAUSED_BY_STR), "", 1);
        if (self->cause->add_to_report != NULL) {
            self->cause->add_to_report(self->cause, report, depth-1);
        } else {
            bxierr_report_add_from_limit(self->cause, report, depth-1);
        }
    }

    bxistr_prefixer_cleanup(&data);
    BXIFREE(final_msg);
}

size_t bxierr_report_str(bxierr_report_p report, char** result_p) {
    bxiassert(NULL != report);
    bxiassert(NULL != result_p);

    char * lines[report->err_nb];
    size_t lines_len[report->err_nb];
    for (size_t i = 0; i < report->err_nb; i++) {
        char * tmp[2] = {report->err_msgs[i], report->err_bts[i]};
        // Remove the NULL terminating byte from the length
        size_t tmp_len[2] = {report->err_msglens[i] - 1, report->err_btslens[i] - 1};
        char * result = NULL;
        lines_len[i] = bxistr_join("\n", ARRAYLEN("\n") - 1,
                                   tmp, tmp_len, 2,
                                   &result);
        lines[i] = result;
    }
    size_t len = bxistr_join("\n", ARRAYLEN("\n") - 1, lines, lines_len, report->err_nb,
                             result_p);
    for (size_t i = 0; i < report->err_nb; i++) {
        BXIFREE(lines[i]);
    }

    return len;
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

char * bxierr_str_limit(bxierr_p self, size_t depth) {
    bxiassert(NULL != self);

    bxierr_report_p report = bxierr_report_new();
    // Ensure that even when the str function is NULL, we can still create the message
    // in one way or another
    if (NULL != self->add_to_report) {
        self->add_to_report(self, report, depth);
    } else {
        bxierr_report_add_from_limit(self, report, depth);
    }
    char * result = NULL;
    bxierr_report_str(report, &result);
    bxierr_report_destroy(&report);
    return result;
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
        buf[ERR2STR_MAX_SIZE - 1] = '\0';
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

size_t bxierr_backtrace_str(char ** result) {
    *result = NULL;
    size_t size;
    errno = 0;
    FILE * faked_file = open_memstream(result, &size);
    if (NULL == faked_file) {
        error(0, errno, "Calling open_memstream() failed");
        *result = strdup("Unavailable backtrace (open_memstream() failed)");
        return strlen(*result) + 1;
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
    return size;
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

void bxierr_list_add_to_report(bxierr_p err, bxierr_report_p report, size_t depth) {
    bxierr_report_add_from_limit(err, report, depth);
    bxierr_list_p group = err->data;
    size_t current = 0;
    // Create the string for each internal error in the group
    for (size_t i = 0; i < group->errors_size; i++) {
        bxierr_p ierr = group->errors[i];
        if (NULL == ierr) continue;
        char * s = bxistr_new("Error n°%zu", current);
        bxierr_report_add(report, s, strlen(s) + 1, "", 1);
        bxierr_report_add_from_limit(ierr, report, depth);
        current++;
    }
}

bxierr_set_p bxierr_set_new() {
    bxierr_set_p result = bximem_calloc(sizeof(*result));

    _err_list_init(&result->distinct_err);

    result->seen_nb = bximem_calloc(result->distinct_err.errors_size * sizeof(*result->seen_nb));
    result->total_seen_nb = 0;

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
        // Add one to prevent product with zero!
        size_t new_len = (set->distinct_err.errors_size + 1) * 2;
        set->seen_nb = bximem_realloc(set->seen_nb,
                                      old_len * sizeof(*set->seen_nb),
                                      new_len*sizeof(*set->seen_nb));
    }

    bxiassert(NULL == slot);
    bxierr_list_append(&set->distinct_err, *err);
    set->seen_nb[i]++;

    return true;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************
void _err_list_init(bxierr_list_p errlist) {

    errlist->errors_nb = 0;
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
        char * str = bxistr_new("%s at %s:%d",
                                function, filename, lineno);
        (* (char **) data) = str;
    }
    return 0;
}

