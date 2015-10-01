/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_HANDLER_H_
#define BXILOG_HANDLER_H_

#ifndef BXICFFI
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#endif


#include "bxi/base/mem.h"
#include "bxi/base/err.h"


/**
 * @file    log.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief   High Performance Logging Module
 *
 * ### Overview
 *
 * This module provides a high performance logging library (>500k logs/s),
 * for heavy multi-threaded processes, while supporting fork() system call,
 * and signal handling. It provides convenient functions to deal with:
 *
 * - logging at various log levels: `::bxilog_level_e`,
 * - logging configuration: `bxilog_cfg_registered()`,
 * - error reporting: `BXILOG_REPORT()`,
 * - assertion: `BXIASSERT()` and `BXIUNREACHABLE_STATEMENT()`,
 * - exiting: `::BXIEXIT`,
 * - signal handling set-up: `bxilog_install_sighandler()`
 *
 * ### Basic 4-steps usage
 *
 * 1. use macro `SET_LOGGER()` at the beginning of each C module (in .c, not in .h)
 * 2. in the same source file, use `DEBUG()`, `INFO()`, `WARNING()`, `ERROR()`, ...
 * 3. call `bxilog_init()` (in your `main()`) to initialize the whole module
 * 4. call `bxilog_finalize()` (in your `main()`) otherwise *your program won't terminate*!
 *
 * ### Notes
 *
 * - `OUT()` should be seen as a replacement for `printf()`
 * - Loggers created with `SET_LOGGER()` are automatically registered with the library
 * - Dynamically created loggers with `bxilog_get()` (such as from higher level languages)
 *   can be registered with `bxilog_register()`
 * - Default level of each logger is `BXILOG_LOWEST`. Use `bxilog_cfg_registered()`
 *   to configure all registered loggers.
 * - Use `BXIASSERT()` instead of `assert()` and `BXIEXIT()` instead of `exit()`
 *   to guarantee that logs are flushed before exiting the process.
 * - For the same reason, you might need the installation of a signal handler in order
 *   to call `bxilog_finalize()` properly and to flush all pending messages.
 *   See `bxilog_install_sighandler()` for this purpose.
 * - For `::bxierr_p` reporting, use `::BXILOG_REPORT`.
 * - Exiting a process (with `::BXIEXIT`) normally requires at least the level
 *   `::BXILOG_CRITICAL` or above and a standard exit code (see sysexits.h).
 *
 * - All functions in this library are thread safe.
 * - Fork is supported, but note that after a `fork()`, while the parent remains in same
 *   state as before the `fork()`, the child have to call `bxilog_init()` in order to
 *   produce logs.
 *
 *
 * ### Full Running Examples
 *
 * - @link bxilog_err.c logging and error handling @endlink
 * - @link bxilog_cfg.c configuring the bxilog library @endlink
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
#define READY_CTRL_MSG_REQ "BC->H: ready?"
#define READY_CTRL_MSG_REP "H->BC: ready!"
#define FLUSH_CTRL_MSG_REQ "BC->H: flush?"
#define FLUSH_CTRL_MSG_REP "H->BC: flushed!"
#define EXIT_CTRL_MSG_REQ "BC->H: exit?"
#define EXIT_CTRL_MSG_REP "H->BC: exited!"


/**
 *  Error code used by handlers to specify they must exit
 */
#define BXILOG_HANDLER_EXIT_CODE 41322811


#define TIMESPEC_SIZE 16
#ifndef BXICFFI
BXIERR_CASSERT(timespec_size, TIMESPEC_SIZE == sizeof(struct timespec));
#endif

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

// A bxilog record
// TODO: reorganize with most-often used data first
// see cachegrind results.
typedef struct {
    bxilog_level_e level;           // log level
#ifndef BXICFFI
    struct timespec detail_time;    // log timestamp
#else
    uint8_t detail_time[TIMESPEC_SIZE];
#endif
#ifndef BXICFFI
    pid_t pid;                      // process pid
#ifdef __linux__
    pid_t tid;                      // kernel thread id
//    size_t thread_name_len        // Thread name (see pthread_getname_np())
#endif
#else
    int pid;
#ifdef __linux__
    int tid;
#endif
#endif
    uint16_t thread_rank;           // user thread rank
    int line_nb;                    // line nb
    //  size_t progname_len;            // program name length
    size_t filename_len;            // file name length
    size_t funcname_len;            // function name length
    size_t logname_len;             // logger name length
    size_t variable_len;            // filename_len + funcname_len +
                                    // logname_len
    size_t logmsg_len;              // the logmsg length
} bxilog_record_s;

typedef bxilog_record_s * bxilog_record_p;

typedef struct {
    bool mask_signals;
    void * zmq_context;
    int data_hwm;
    int ctrl_hwm;
    double connect_timeout_s;
    long poll_timeout_ms;
    size_t subscriptions_nb;
    char **subscriptions;
    char * data_url;
    char * ctrl_url;
} bxilog_handler_param_s;

typedef bxilog_handler_param_s * bxilog_handler_param_p;

typedef struct bxilog_handler_s_f bxilog_handler_s;
typedef bxilog_handler_s * bxilog_handler_p;
struct bxilog_handler_s_f {
    char * name;
    bxilog_handler_param_p (*param_new)(bxilog_handler_p,
#ifndef BXICFFI
            va_list);
#else
            void *);
#endif
    bxierr_p (*init)(bxilog_handler_param_p param);
    bxierr_p (*process_log)(bxilog_record_p record,
                            char * filename,
                            char * funcname,
                            char * loggername,
                            char * logmsg,
                            bxilog_handler_param_p param);
    bxierr_p (*process_err)(bxierr_p *err, bxilog_handler_param_p param);
    bxierr_p (*process_implicit_flush)(bxilog_handler_param_p param);
    bxierr_p (*process_explicit_flush)(bxilog_handler_param_p param);
    bxierr_p (*process_exit)(bxilog_handler_param_p param);
    bxierr_p (*process_cfg)(bxilog_handler_param_p param);
    bxierr_p (*param_destroy)(bxilog_handler_param_p* param);
};


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

void bxilog_handler_init_param(bxilog_handler_p handler, bxilog_handler_param_p param);
void bxilog_handler_clean_param(bxilog_handler_param_p param);

#endif /* BXILOG_H_ */
