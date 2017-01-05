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

#ifndef BXIERR_H_
#define BXIERR_H_

#ifndef BXICFFI
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#endif

#include "bxi/base/mem.h"

/**
 * @file    err.h
 * @brief   Error Handling Module
 *
 * ### Overview
 *
 * This module provides a convenient and efficient way to deal with errors in C.
 *
 * Basically, instead of integer error code, it relies on pointer. That is,
 * checking whether a given function returns an ok code or not should be as efficient
 * as usual (using integer returned code), but in the case of error, more information
 * should be provided (and in this case we accept to lose some time).
 *
 * ### Creating Errors
 *
 * Various macros and functions are provided for different use cases:
 *
 * 1. errno with a static message:
 *    ~~~
 *    errno = 0;
 *    int rc = f(...);
 *    if (0 != rc) return bxierr_errno("Cannot call f()");
 *    ~~~
 * 2. errno with a message:
 *    ~~~
 *    errno = 0;
 *    int rc = f(i, j);
 *    if (0 != rc) return bxierr_errno("Cannot call f(%d, %d)", i, j);
 *    ~~~
 * 3. generic error (no specific returned code, just a simple message):
 *    ~~~
 *    if (error) return bxierr_gen("An error occured");
 *    ~~~
 * 4. error message given by an array of static messages
 *    ~~~
 *    const char * errmsg[] = {"A given type of error occured",
 *                             "Another type of error occured",
 *                             "Yet another type of error occured",
 *                             };
 *    if (error) return bxierr_fromidx(idx, errmsg,
 *                                     "An error occured at %s", __FILE__);
 *    ~~~
 *
 * 5. error message given by specific error code
 *    ~~~
 *    #define MY_FIRST_ERR 1    // Data is NULL
 *    #define MY_SECOND_ERR 2   // Data is a size_t casted to a void*
 *    #define MY_THIRD_ERR 3    // Data is a mallocated buffer
 *    ...
 *    if (error1) return bxierr_new(MY_FIRST_ERR, NULL, NULL, NULL, NULL, "%s", "Error");
 *    size_t some_stuff = ...;
 *    if (error2) return bxierr_new(MY_SECOND_ERR,
 *                                  (void*) some_stuff,
 *                                  NULL,
 *                                  NULL,
 *                                  NULL,
 *                                 "%s", "Error");
 *    struct stuff_s stuff_p = bximisc_calloc(...);
 *    stuff_p->field1 = ...;
 *    ...
 *    if (error3) return bxierr_new(MY_THIRD_ERR,
 *                                  stuff_p,
 *                                  free,
 *                                  NULL,
 *                                  NULL,
 *                                  "%s", "Error");
 *    ~~~
 * ### Handling Errors
 *
 * When using `log.h` module (which is highly recommended),
 * see `BXIEXIT()` and `BXILOG_REPORT()`.
 *
 * 1. Exiting with an error message (but use `BXIEXIT()` instead):
 *    ~~~
 *    bxierr_p err = f(...);
 *    if (bxierr_isko(err)) {
 *        char * str = bxierr_str(err);
 *        // If you use bxilog, use BXIEXIT() instead in order to flush the logs.
 *        error(EX_SOFTWARE, 0, "Error calling f(): %s", str);
 *        BXIFREE(str);
 *    }
 *    ~~~
 *
 * 2. Returning to the caller:
 *    ~~~
 *    bxierr_p err = f(...);
 *    if (bxierr_isko(err)) return err;
 *    ~~~
 *
 * 3. Chain/wrap multiple errors and return only one:
 *    ~~~
 *    bxierr_p err = BXIERR_OK, err2;
 *    err2 = f(...); BXIERR_CHAIN(err, err2);
 *    err2 = g(...); BXIERR_CHAIN(err, err2);
 *    err2 = h(...); BXIERR_CHAIN(err, err2);
 *
 *    return err;
 *    ~~~
 */


// *********************************************************************************
// ********************************** Define  **************************************
// *********************************************************************************
#ifndef BXICFFI
/**
 * Use to check at compile time an assertion.
 *
 * @param[in] type the name of the generated typedef
 * @param[in] expn a compile-time boolean expression
 */
#define BXIERR_CASSERT(type, expn) typedef char __C_ASSERT__ ## type[(expn)?1:-1]
#endif


/**
 * Define the bxierr_p generic code
 *
 * Note: the value is ERROR in leet speak ;-)
 */
#define BXIERR_GENERIC_CODE 32203

/**
 * Define the bxierr_p code for failed assertion
 *
 * Note: the value is ASSERT in leet speak ;-)
 *
 * @see bxierr_p
 */
#define BXIASSERT_CODE 455327

/**
 * Define the bxierr_p code for unreachable statement
 *
 * Note: this is the devil code! ;-)
 */
#define BXIUNREACHABLE_STATEMENT_CODE 666

/**
 * Define the bxierr_p code for an error containing a bxierr_group_p in its data.
 */
#define BXIERR_GROUP_CODE 6209

/**
 * Define the standard BXI message for a bug.
 */
#define BXIBUG_STD_MSG "\nThis is a bug and should be reported as such.\n"                \
                       "In your report, do not omit the following informations:\n"      \
                       "\t- version of the product;\n"                                  \
                       "\t- full command line arguments;\n"                             \
                       "\t- the logging file at the lowest log level.\n"                \
                       "Contact Bull for bug submission.\n"                             \
                       "Thanks and Sorry."


#define BXIERR_ALL_CAUSES 64
#define ERR2STR_MAX_SIZE 1024


/**
 * Chain the new error with the current one and adapt the current error accordingly.
 *
 * Use it like this:
 * ~~~{C}
 * bxierr_p err = BXIERR_OK, err2;
 * err2 = f1(...); BXIERR_CHAIN(err, err2);
 * err2 = f2(...); BXIERR_CHAIN(err, err2);
 * err2 = f2(...); BXIERR_CHAIN(err, err2);
 *
 * // The err pointer points to the last error and is chained with all others.
 * return err;
 * ~~~
 */
#define BXIERR_CHAIN(current, new) do {     \
        bxierr_chain(&(current), &(new));\
    } while(false)

/**
 * Produce a backtrace in the given `file` (a FILE* object).
 */
#define BACKTRACE(file) do { \
    char * _trace = bxierr_backtrace_str(); \
    fprintf((file), "%s", _trace); \
    BXIFREE(_trace); \
    } while(false)

/**
 * Return a new instance according to current `errno` value and the given
 * printf-like message.
 *
 * @see bxierr_fromidx()
 * @see bxierr_new()
 */
#define bxierr_errno(...) bxierr_fromidx(errno, NULL, __VA_ARGS__)

/**
 * Return a new instance with an error code and the given printf-like message.
 *
 * @see bxierr_new()
 */
#define bxierr_simple(code, ...) bxierr_new(code, NULL, NULL, NULL, NULL, __VA_ARGS__)

/**
 * Create a generic error, with the given printf-like message.
 *
 * @note: method bxierr_simple() should be preferred as it provides a specific error code
 *        that can be used to distinguish different errors.
 *
 * @see bxierr_new()
 * @see bxierr_simple()
 */
#define bxierr_gen(...) bxierr_simple(BXIERR_GENERIC_CODE, __VA_ARGS__)

/**
 * Define an error with the given code and the given error list.
 *
 * @see bxierr_list_p
 */
#define bxierr_from_list(code, errlist, ...)            \
    bxierr_new(code, errlist,                           \
               (void (*) (void*)) bxierr_list_free,     \
               bxierr_list_add_to_report,\
               NULL, __VA_ARGS__)

/**
 * Transform a bxierr_p with a bxierr_set_p in its data field into a string.
 *
 * @note this function main purpose is to be used as an argument of bxierr_new().
 *
 * @param[in] err the error
 * @param[in] depth the number of error causes to transform to string
 *
 * @see bxierr_new
 */
#define bxierr_set_add_to_report bxierr_list_add_to_report

/**
 * Define an error with the given code and the given error set.
 *
 * @see bxierr_set_p
 */
#define bxierr_from_set(code, errset, ...)              \
    bxierr_new(code, errset,                            \
               (void (*) (void*)) bxierr_set_free,      \
               bxierr_set_add_to_report,                \
               NULL, __VA_ARGS__)

/**
 * If the given expression is false, exit with a message and a
 * stack trace.
 *
 * @note This must not be used unless bxilog is not
 * available (such as when bxilog_init() failed).
 * Use BXIASSERT() instead
 *
 * This is a replacement for standard POSIX assert().
 *
 * @see BXIASSERT
 */
#define bxiassert(expr)                                             \
        ((expr)                                                     \
          ? (void) (0)                                              \
          : bxierr_assert_fail(__STRING(expr),                      \
                               __FILE__, __LINE__,                  \
                               __func__))

/**
 * Display a message when this statement is reached with a stack
 * trace and exit.
 *
 * @note This must not be used unless bxilog is not
 * available (such as when bxilog_init() failed).
 * Use BXIUNREACHABLE_STATEMENT() instead
 *
 * @see BXIUNREACHABLE_STATEMENT
 */
#define bxiunreachable_statement                                    \
          bxierr_unreachable_statement(__FILE__, __LINE__, __func__)

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************


/**
 * An error report, used for error/exception reporting.
 */
struct bxierr_report_s {
    size_t err_nb;                  //<! Number of errors in the report
    size_t allocated;               //<! Number of allocated slots
    size_t * err_msglens;           //<! The array of err_nb message strings length
    size_t * err_btslens;           //<! The array of err_nb backtrace strings length
    char ** err_msgs;               //<! The array of err_nb message strings length
    char ** err_bts;                //<! The array of err_nb backtrace strings length
};

/**
 * Alias
 */
typedef struct bxierr_report_s bxierr_report_s;

/**
 * The main type used for error report.
 */
typedef bxierr_report_s * bxierr_report_p;

/**
 * The bxi error instance structure.
 */
typedef struct bxierr_s bxierr_s;

/** \public
 *
 * Represents a bxi error instance.
 */
typedef struct bxierr_s * bxierr_p;


/** \public
 * The bxierr data structure representing an error.
 *
 * Note: bxierr can be chained
 */
struct bxierr_s {
    int  code;                              //!< the error code
    char * backtrace;                       //!< the backtrace
    size_t backtrace_len;                   //!< the backtrace string length (including
                                            //!< the NULL terminating byte)
    void * data;                            //!< some data related to the error
    void (*add_to_report)(bxierr_p,         //!< add this error to the given report
                          bxierr_report_p,
                          size_t depth);
    void (*free_fn)(void *);                //!< the function to use to free the data
    bxierr_p cause;                         //!< the cause if any (can be NULL)
    bxierr_p last_cause;                    //!< the initial cause valid only on the first error
    char * msg;                             //!< the message of the error
    size_t msg_len;                         //!< the length of the message
};


/**
 * An error list
 *
 * @see bxierr_list_new
 * @see bxierr_list_append
 * @see bxierr_list_destroy
 *
 */
typedef struct {
    size_t errors_nb;              //!< Number of errors in the group
    size_t errors_size;            //!< Size of the errors array
    bxierr_p * errors;             //!< The list of errors
} bxierr_list_s;

/**
 * A simple list of errors.
 */
typedef bxierr_list_s * bxierr_list_p;

/**
 * A set of distinct errors.
 *
 * @see bxierr_set_new
 * @see bxierr_set_add
 * @see bxierr_set_destroy
 */
typedef struct {
    bxierr_list_s distinct_err;     //!< used for distinct errors storage
    size_t * seen_nb;               //!< for each distinct error, count the number of
                                    //!< time it has been seen.
    size_t total_seen_nb;           //!< total number of seen errors
} bxierr_set_s;

/**
 * A simple set of distinct errors.
 */
typedef bxierr_set_s * bxierr_set_p;


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

/**
 * The single instance meaning OK.
 *
 * Do not use it directly, use `bxierr_isko()` and `bxierr_isok()` instead.
 *
 * @see bxierr_isko()
 * @see bxierr_isok()
 */
#ifndef BXICFFI
extern const bxierr_p BXIERR_OK;
extern const char * BXIERR_CAUSED_BY_STR;
extern const int BXIERR_CAUSED_BY_STR_LEN;
#else
extern bxierr_p BXIERR_OK;
extern char * BXIERR_CAUSED_BY_STR;
extern int BXIERR_CAUSED_BY_STR_LEN;
#endif


// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Return a new bxi error instance.
 *
 *
 * @param[in] code the error code
 * @param[in] data an error specific data (can be NULL)
 * @param[in] free_fn the function that must be used to release the given `data` (can be NULL)
 * @param[in] add_to_report the function (can be NULL) that must be used to add the
 *            error to a report
 * @param[in] cause the error cause (can be NULL)
 * @param[in] fmt the error message in a printf like style.
 *
 * @return a new bxi error instance
 *
 * @see bxierr_destroy()
 * @see bxierr_report_p
 * @see bxierr_p
 */
bxierr_p bxierr_new(int code,
                    void * data,
                    void (*free_fn) (void *),
                    void (*add_to_report)(bxierr_p, bxierr_report_p, size_t),
                    bxierr_p cause,
                    const char * fmt,
                    ...)
#ifndef BXICFFI
__attribute__ ((format (printf, 6, 7)))
#endif
                    ;

/**
 * Release all resources in self and self itself.
 *
 * @note: the pointer is not nullified, use bxierr_destroy() instead.
 *
 * @param[in] self the error to free
 *
 * @see bxierr_destroy()
 */
void bxierr_free(bxierr_p self);


/**
 * Return the depth of the given error.
 *
 * The depth is defined as the number of causes in the whole chain starting from
 * the given error.
 *
 * @param[in] self the error
 *
 * @return the error depth
 */
size_t bxierr_get_depth(bxierr_p self);


/**
 * Return the `BXIERR_OK` error;
 *
 * Mostly used by CFFI.
 *
 * Use `bxierr_isok()` or `bxierr_isko()` instead.
 *
 * @return `BXIERR_OK`
 *
 * @see bxierr_isok()
 * @see bxierr_isko()
 * @see BXIERR_OK
 */
bxierr_p bxierr_get_ok();

/**
 * Create a new error report
 *
 * @return a new report
 */
bxierr_report_p bxierr_report_new();

/**
 * Add an error/exception into the given report
 *
 * @param[in] report a report
 * @param[in] err_msg the error message
 * @param[in] err_msglen the error message length (including the NULL terminating byte)
 * @param[in] err_bt the error backtrace
 * @param[in] err_btlen the error backtrace length (including the NULL terminating byte)
 */
void bxierr_report_add(bxierr_report_p report,
                       char * err_msg, const size_t err_msglen,
                       char * err_bt, size_t err_btlen);

/**
 * Free the given report
 *
 * @param[inout] self the report to free
 *
 * @see bxierr_report_destroy
 */
void bxierr_report_free(bxierr_report_p self);

/**
 * Return a string from the given report.
 *
 * @note the returned string will have to be freed. Use BXIFREE().
 *
 * @param[in] self a report
 * @param[out] result_p a pointer on the result string
 *
 * @return the length of the returned string
 */
size_t bxierr_report_str(bxierr_report_p self, char ** result_p);

/**
 * Add the given bxierr_p information into a report
 *
 * @param[in] self the error to add
 * @param[inout] report the report
 * @param[in] depth how many causes must be added
 *
 */
void bxierr_report_add_from_limit(bxierr_p self, bxierr_report_p report, size_t depth);

/**
 * Report the given error on the given file descriptor.
 *
 * Use only when you do not have the bxilog library initialized. Otherwise,
 * use BXILOG_REPORT().
 *
 * @note the given error will be destroyed.
 *
 * @param[inout] self the error to report
 * @param[in] fd a file descriptor where the error will be reported
 *
 * @see BXILOG_REPORT()
 */
void bxierr_report(bxierr_p *self, int fd);

/**
 * Report the given error on the given file descriptor and keep the error.
 *
 * @note normally, a reported error should be destroyed, since it has been dealt with.
 * Use bxierr_report() instead or even better BXILOG_REPORT().
 *
 * @param[in] self the error to report
 * @param[in] fd the file descriptor to use (e.g: STDERR_FILENO)
 */
void bxierr_report_keep(bxierr_p self, int fd);

/**
 * Display an assertion failed message with a stack trace and exit.
 *
 * @note this function is only useful when logging is not available, otherwise,
 *       BXIASSERT must be used instead.
 *
 * @param[in] assertion a string representing the assertion that failed
 * @param[in] file the file in which the failed assertion was seen
 * @param[in] line the line in file in which the failed assertion was seen
 * @param[in] function the function in file in which the failed assertion was seen
 *
 * @see bxierr_assert
 * @see BXIASSERT
 */
void bxierr_assert_fail(const char *assertion, const char *file,
                        unsigned int line, const char *function)
#ifndef BXICFFI
                        __attribute__ ((__noreturn__))
#endif
                        ;

/**
 * Return a human string from the given error up to the given depth.
 *
 * @note the returned string must be freed. Use BXIFREE().
 *
 * @param[in] self an error
 * @param[in] depth the maximum number of causes to display
 *
 * @return a human string representing the given error
 */
char * bxierr_str_limit(bxierr_p self, size_t depth);

#ifndef BXICFFI


/**
 * Destroy the given report.
 *
 * @note the value of the given pointer is nullified after this call.
 *
 * @param[inout] report_p a pointer on the report
 *
 */
inline void bxierr_report_destroy(bxierr_report_p * report_p) {
    bxiassert(NULL != report_p);
    bxierr_report_free(*report_p);
    *report_p = NULL;;
}

/**
 * Add an error to a report
 *
 * @param[in] self an error
 * @param[inout] report a report
 *
 * @see bxierr_report_add_from_limit
 */
inline void bxierr_report_add_from(bxierr_p self, bxierr_report_p report) {
    bxierr_report_add_from_limit(self, report, BXIERR_ALL_CAUSES);
}


/**
 * Destroy the error instance pointed to by `self_p`.
 *
 * All underlying mallocated data will be released,
 * and the pointer given by `*self_p` will be nullified.
 *
 * If `bxierr_get_cause(*self_p)` is not NULL,
 * `bxierr_destroy()` will be called on the returned cause
 * (and so on recursively).
 *
 * If `bxierr_get_data(*self_p)` is not NULL,
 * function given by `bxierr_get_free_fn()` will be called with the returned
 * `data` in parameter.
 *
 * @param self_p the pointer on the error to destroy
 */
inline void bxierr_destroy(bxierr_p * self_p) {
    bxiassert(NULL != self_p);
    
    bxierr_free(*self_p);
    *self_p = NULL;
}

/**
 * Return true if the given `bxierr` is ok. False otherwise.
 *
 * @param[in] self the error to check
 *
 * @return true if the given `bxierr` is ok. False otherwise.
 */
inline bool bxierr_isok(bxierr_p self) {
    return self == BXIERR_OK;
}

/**
 * Return true if the given `bxierr` is not ok. False otherwise.
 *
 * @param[in] self the error to check
 *
 * @return true if the given `bxierr` is not ok. False otherwise.
 */
inline bool bxierr_isko(bxierr_p self) {
    return self != BXIERR_OK;
}



/**
 * Return a human string representation of the given `bxierr`.
 *
 * @param[in] self the error to get the human string representation of
 *
 * @return a human string representation of the given `bxierr`.
 */
inline char * bxierr_str(bxierr_p self) {
    return bxierr_str_limit(self, BXIERR_ALL_CAUSES);
}

/**
 * Chain the given new error (tmp) with the current one (err) if required.
 *
 * The workflow is the following:
 *
 *   bxierr_p err = BXIERR_OK, err2;
 *
 *   err2 = f();
 *   BXIERR_CHAIN(err, err2);
 *
 *   if (bxierr_isko(err)) return err;
 *
 *   err2 = bxierr_new(...);
 *   BXIERR_CHAIN(err, err2);
 *
 *   return err;
 *
 *
 * @param err the actual variable holding the final error, it might be changed by
 *            this function
 *
 * @param tmp an error to check against, and to chain to err if required
 *
 */
inline void bxierr_chain(bxierr_p *err, const bxierr_p *tmp) {
            bxiassert(NULL != (*err));
            bxiassert(NULL != (*tmp));

            if (bxierr_isko((*tmp)) && bxierr_isko((*err))) {
                bxiassert(*err != *tmp);
                if (NULL != (*tmp)->cause) {
                    bxiassert((*tmp)->last_cause->cause == NULL);
                    (*tmp)->last_cause->cause = (*err);
                } else {
                    (*tmp)->cause = (*err);
                }
                if ((*err)->last_cause != NULL) {
                    (*tmp)->last_cause = (*err)->last_cause;
                } else {
                    (*tmp)->last_cause = (*err);
                }
            }
            (*err) = bxierr_isko((*tmp)) ? (*tmp) : (*err);
}
/**
 * Abort the program if the given fatal_err is ko.
 *
 * The given error is reported first on stderr.
 *
 * @note: This function never returns as it exit the program.
 *
 * @param[in] fatal_err the error to check
 */
inline void bxierr_abort_ifko(bxierr_p fatal_err) {
    if (bxierr_isko(fatal_err)) {
        bxierr_report(&fatal_err, STDERR_FILENO);
        abort();
    }
}
#else
void bxierr_report_add_from(bxierr_p self, bxierr_report_p report);
void bxierr_report_destroy(bxierr_report_p report);
void bxierr_destroy(bxierr_p * self_p);
bool bxierr_isok(bxierr_p self);
bool bxierr_isko(bxierr_p self);
char * bxierr_str(bxierr_p self);
void bxierr_chain(bxierr_p *err, const bxierr_p *tmp);
void bxierr_abort_ifko(bxierr_p fatal_err);

#endif



/**
 * Return a bxierr from the given errcode error code.
 *
 * The instance returned has the given `errcode` as the code,
 * and a message, given by both the `erridx2str` array and the given `fmt` printf
 * like format string.
 *
 * Most of the case, the errcode comes from errno itself, in this case, use the
 * simpler form `bxierr_errno()`.
 *
 * @param[in] erridx the error code
 * @param[in] erridx2str an array of error message
 * @param[in] fmt a printf like format string
 *
 * @return a bxierr instance
 *
 * @see bxierr_errno
 * @see bxierr_vfromidx
 */
bxierr_p bxierr_fromidx(int erridx,
                        const char * const * erridx2str,
                        const char * fmt, ...);
/**
 * Equivalent to `bxierr_fromidx()` but with a variable argument list instead
 * of an ellipse, useful for library wrapper.
 *
 * @param[in] errcode the error code
 * @param[in] erridx2str an array of error message
 * @param[in] fmt a printf-like format string
 * @param[in] ap a va_list representing the parameter for the given format
 *
 * @return BXIERR_OK on success, any other value on error
 *
 * @see bxierr_fromidx
 */
#ifndef BXICFFI
bxierr_p bxierr_vfromidx(int errcode,
                         const char * const * erridx2str,
                         const char * fmt, va_list ap);
#endif

/**
 * Fill the given buffer with a human representation of the current backtrace.
 *
 * The returned buffer is mallocated, and will have to be freed (see BXIFREE()).
 *
 * @param[out] buf a pointer on the resulting buffer
 *
 * @return the number of bytes written to buf.
 */
size_t bxierr_backtrace_str(char ** buf);



/**
 * Display an 'Unreachable statement reached' message with a stack trace and exit.
 *
 * @note this function is only useful when logging is not available, otherwise,
 *       BXIUNREACHABLE_STATEMENT must be used instead.
 *
 * @param[in] file the file in which the failed assertion was seen
 * @param[in] line the line in file in which the failed assertion was seen
 * @param[in] function the function in file in which the failed assertion was seen
 */
void bxierr_unreachable_statement(const char *file,
                                  unsigned int line, const char *function)
#ifndef BXICFFI
                                  __attribute__ ((__noreturn__))
#endif
                                  ;

/**
 * Create a new bxierr_list_p object.
 *
 * @return a new bxierr_list_p
 */
bxierr_list_p bxierr_list_new();

/**
 * Free a bxierr_list_p object.
 *
 * @note the pointer is not nullified. Use bxierr_list_destroy() instead.
 *
 * @param[inout] errlist the bxierr_list_p to free
 *
 * @see bxierr_list_destroy()
 */
void bxierr_list_free(bxierr_list_p errlist);

#ifndef BXICFFI
/**
 * Destroy a bxierr_list_p object.
 *
 * @note: the pointer is nullified.
 *
 * @param[inout] errlist_p the bxierr_list_p to destroy
 *
 */
inline void bxierr_list_destroy(bxierr_list_p * errlist_p) {
    bxiassert(NULL != errlist_p);

    bxierr_list_free(*errlist_p);

    *errlist_p = NULL;
}
#else
void bxierr_list_destroy(bxierr_list_p * group_p);
#endif


/**
 * Append the given error in the list.
 *
 * @param[inout] list an error list
 * @param[in] err an error
 *
 */
void bxierr_list_append(bxierr_list_p list, bxierr_p err);

/**
 * Transform a bxierr_p with a bxierr_list_p in its data field into a report.
 *
 * @note this function main purpose is to be used as an argument of bxierr_new().
 *
 * @param[in] err the error
 * @param[inout] report the report
 * @param[in] depth the number of error causes to transform to string
 *
 * @return a string representation of the given error
 *
 * @see bxierr_new()
 */
void bxierr_list_add_to_report(bxierr_p err, bxierr_report_p report, size_t depth);


/**
 * Create a new bxierr_set_p object.
 *
 * @return a new bxierr_set_p
 */
bxierr_set_p bxierr_set_new();

/**
 * Free a bxierr_set_p object.
 *
 * @note the pointer is not nullified. Use bxierr_set_destroy() instead.
 *
 * @param[inout] errset the bxierr_set_p to free
 *
 * @see bxierr_set_destroy()
 */
void bxierr_set_free(bxierr_set_p errset);

#ifndef BXICFFI

/**
 * Destroy a bxierr_set_p object.
 *
 * @note: the pointer is nullified.
 *
 * @param[inout] errset_p the bxierr_set_p to destroy
 */
inline void bxierr_set_destroy(bxierr_set_p * errset_p) {
    bxiassert(NULL != errset_p);

    bxierr_set_free(*errset_p);

    *errset_p = NULL;
}
#else
void bxierr_set_destroy(bxierr_set_p * errset_p);
#endif


/**
 * Add the given error in the set if no error with the same code exists.
 *
 * @note: the given error `*err` is destroyed if an other error with the same code
 * already exists in the given error set.
 *
 * @param[inout] set the error set
 * @param[inout] err a pointer on the error to store in the given set if it does not
 * exist or to destroy otherwise.
 *
 * @return true if the given error has been added, false otherwise, meaning the given
 * error has been destroyed using bxierr_destroy().
 *
 * @see bxierr_set_p
 */
bool bxierr_set_add(bxierr_set_p set, bxierr_p * err);


#endif /* BXIERR_H_ */
