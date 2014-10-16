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
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#endif
#include <bxi/base/mem.h>


/**
 * HOWTO USE
 * ==========
 *
 * This module provides a convenient and efficient way to deal with error in C.
 *
 * Basically, instead of integer error code, it relies on pointer. That is,
 * checking whether a given function returns an ok code or not should be as efficient
 * as usual (using integer returned code), but in the case of error, more information
 * should be provided (and in this case we accept to lose some time).
 *
 * Error Production
 * -----------------
 *
 * Various macros and functions are provided for different use cases.
 *
 * 1. errno with a static message:
 *
 *    errno = 0;
 *    int rc = f(...);
 *    if (0 != rc) return bxierr_perror("Cannot call f()");
 *
 *
 * 2. errno with a message:
 *
 *    errno = 0;
 *    int rc = f(i, j);
 *    if (0 != rc) return bxierr_error("Cannot call f(%d, %d)", i, j);
 *
 * 3. generic error (no specific returned code, just a simple message):
 *
 *    if (error) return bxierr_pgen("An error occured");
 *
 * 4. error message given by an array of static messages
 *
 *    if (error) return bxierr_fromidx(code, "An error occured at %s", __FILE__);
 *
 * 5. error message given by specific error code
 *
 *    #define MY_FIRST_ERR 1    // Data is NULL
 *    #define MY_SECOND_ERR 2   // Data is a size_t casted to a void*
 *    #define MY_THIRD_ERR 3    // Data is a mallocated buffer
 *    ...
 *    if (error1) return bxierr_new(MY_FIRST_ERR, NULL, NULL, NULL, "%s", "Error");
 *    size_t some_stuff = ...;
 *    if (error2) return bxierr_new(MY_SECOND_ERR,
 *                                  (void*) some_stuff, NULL,
 *                                  NULL,
 *                                  "%s", "Error");
 *    struct stuff_s stuff_p = bximisc_calloc(...);
 *    stuff_p->field1 = ...;
 *    ...
 *    if (error3) return bxierr_new(MY_THIRD_ERR, stuff_p, free, NULL, "%s", "Error");
 *
 * Error Handling
 * ---------------
 *
 * 1. Exiting with an error message:
 *
 *      bxierr_p err = f(...);
 *      if (bxierr_isko(err)) {
 *          char * str = bxierr_str(err);
 *          error(EX_SOFTWARE, 0, "Error calling f(): %s", str);
 *          BXIFREE(str);
 *      }
 *
 * 2. Returning to the caller:
 *
 *      bxierr_p err = f(...);
 *      if (bxierr_isko(err)) return err;
 *
 * 3. Chain/wrap multiple errors and return only one:
 *
 *      bxierr_p err = BXIERR_OK, err2;
 *      err2 = f(...); BXIERR_CHAIN(err, err2);
 *      err2 = g(...); BXIERR_CHAIN(err, err2);
 *      err2 = h(...); BXIERR_CHAIN(err, err2);
 *
 *      return err;
 */


// *********************************************************************************
// ********************************** Define  **************************************
// *********************************************************************************

#define BXIERR_GENERIC_CODE 32203  // Leet speak ;-)
#define BXIERR_ALL_CAUSES UINT64_MAX
#define ERR2STR_MAX_SIZE 1024

/**
 * Chain the new error with the current one and adapt the current error accordingly.
 *
 * Use it like this:
 *
 * bxierr_p err = BXIERR_OK, err2;
 * err2 = f1(...); BXIERR_CHAIN(err, err2);
 * err2 = f2(...); BXIERR_CHAIN(err, err2);
 * err2 = f2(...); BXIERR_CHAIN(err, err2);
 *
 * // The err pointer points to the last error and is chained with all others.
 * return err;
 */
#define BXIERR_CHAIN(current, new) do {\
            assert(NULL != (current));\
            assert(NULL != (new));\
            if (bxierr_isko((current)) && bxierr_isko((new))) {\
                if (NULL != new->cause) {\
                    char * __str__ = bxierr_str((new));\
                    fprintf(stderr, \
                            "WARNING: Cannot chain two chains of errors. "\
                            "Making current the data of new. Current is: %s", __str__);\
                    BXIFREE(__str__);\
                    new->data = &(current);\
                    new->free_fn = (void (*) (void*)) bxierr_destroy;\
                } else (new)->cause = (current);\
            }\
            (current) = bxierr_isko((new)) ? (new) : (current);\
        } while(0)

/**
 * Produce a backtrace in the given `file` (a FILE* object).
 */
#define BACKTRACE(file) do { \
    char * _trace = bxierr_backtrace_str(); \
    fprintf((file), "%s", _trace); \
    BXIFREE(_trace); \
    } while(0)

/**
 * Return a new instance according to current errno value and the given
 * printf-like message.
 *
 * @see bxierr_error
 */
#define bxierr_error(fmt, ...) bxierr_fromidx(errno, NULL, (fmt), __VA_ARGS__)
/**
 * Equivalent to bxierr_error() but with a single string as the parameter.
 */
#define bxierr_perror(msg) bxierr_fromidx(errno, NULL, "%s", (msg))

/**
 * Create a generic error, with the given printf-like message.
 */
#define bxierr_gen(fmt, ...) bxierr_new(BXIERR_GENERIC_CODE, NULL, NULL, NULL, (fmt), __VA_ARGS__)
/**
 * Equivalent to bxierr_gen() but with a single string as the parameter.
 */
#define bxierr_pgen(msg) bxierr_new(BXIERR_GENERIC_CODE, NULL, NULL, NULL, "%s", (msg))

/**
 * Define a new static error.
 */
#define bxierr_define(name, code_number, user_msg)                  \
    bxierr_s name ## _S = {                                         \
                                .allocated = false,                 \
                                .code = code_number,                \
                                .backtrace = NULL,                  \
                                .data = NULL,                       \
                                .free_fn = NULL,                    \
                                .cause = NULL,                      \
                                .msg = user_msg,                    \
                                .msg_len = ARRAYLEN(user_msg),      \
    };                                                              \
    const bxierr_p name = (const bxierr_p) &name ## _S;

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************
typedef struct bxierr_s bxierr_s;
typedef struct bxierr_s * bxierr_p;

/**
 * The bxierr data structure representing an error.
 *
 * Note: bxierr can be chained
 */
struct bxierr_s {
    bool allocated;                         //< true if this error has been mallocated
    int  code;                              //< the error code
    char * backtrace;                       //< the backtrace
    void * data;                            //< some data related to the error
    void (*free_fn)(void *);                //< the function to use to free the data
    bxierr_p cause;                         //< the cause if any (can be NULL)
    char * msg;                             //< the message of the error
    size_t msg_len;                         //< the length of the message
};

extern const bxierr_p BXIERR_OK;            //< the single instance meaning OK

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Return a new bxi error instance.
 *
 * @param code the error code
 * @param data an error specific data (can be NULL)
 * @param free the function that must be used to release the given `data` (can be NULL)
 * @param cause the error cause (can be NULL)
 * @param fmt the error message in a printf like style.
 *
 * @return a new bxi error instance
 *
 * @see bxierr_destroy()
 * @see bxierr_p
 */
bxierr_p bxierr_new(int code,
                    void * data,
                    void (*free_fn) (void *),
                    bxierr_p cause,
                    const char * fmt,
                    ...)
#ifndef BXICFFI
__attribute__ ((format (printf, 5, 6)))
#endif
                    ;


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
void bxierr_destroy(bxierr_p * self_p);

/**
 * Return a human string representation of the given error up
 * to the given depth.
 *
 * Since errors can be chained (see `bxierr_get_cause()`),
 * one might want to stop the string generation up to a given
 * depth of causes. Use `BXIERR_ALL_CAUSES` to include all
 * causes in the returned string.
 *
 * The returned string *must* be freed. Use `BXIFREE()`.
 *
 * @param self the error
 * @return a human string representation of the given error
 *
 * @see bxierr_get_depth()
 * @see bxierr_get_cause()
 */
char * bxierr_str_limit(bxierr_p self, size_t depth);

/**
 * Return the depth of the given error.
 *
 * The depth is defined as the number of causes in the whole chain starting from
 * the given error.
 *
 * @param self the error
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

#ifndef BXICFFI
/**
 * Return true if the given `bxierr` is ok. False otherwise.
 */
inline bool bxierr_isok(bxierr_p self) {
    return self == BXIERR_OK;
}

/**
 * Return true if the given `bxierr` is not ok. False otherwise.
 */
inline bool bxierr_isko(bxierr_p self) {
    return self != BXIERR_OK;
}

/**
 * Return a human string representation of the given `bxierr`.
 */
inline char * bxierr_str(bxierr_p self) {
    return bxierr_str_limit(self, BXIERR_ALL_CAUSES);
}
#else
bool bxierr_isok(bxierr_p self);
bool bxierr_isko(bxierr_p self);
char * bxierr_str(bxierr_p self);
#endif

/**
 * Return a bxierr from the given errcode error code.
 *
 * The instance returned has the given `errcode` as the code,
 * and a message, given by both the `erridx2str` array and the given `fmt` printf
 * like format string.
 *
 * Most of the case, the errcode comes from errno itself, in this case, use the
 * simpler form `bxierr_perror()`.
 *
 * @param errcode the error code
 * @param errcode2str an array of error message
 * @param fmt a printf like format string
 * @return a bxierr instance
 *
 * @see bxierr_perror
 * @see bxierr_vstrerror
 */
bxierr_p bxierr_fromidx(int errcode,
                         const char * const * erridx2str,
                         const char * fmt, ...);
/**
 * Equivalent to `bxierr_strerror()` but with a variable argument list instead
 * of an ellipse, useful for library wrapper.
 *
 * @see bxierr_strerror
 * @see bxierr_perror
 */
#ifndef BXICFFI
bxierr_p bxierr_vfromidx(int errcode,
                          const char * const * erridx2str,
                          const char * fmt, va_list ap);
#endif

/**
 * Return a human representation of the current backtrace
 *
 * @return a string representing the backtrace
 */
char * bxierr_backtrace_str(void);

#endif /* BXIERR_H_ */
