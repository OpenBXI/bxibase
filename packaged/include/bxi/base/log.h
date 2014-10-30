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

#ifndef BXILOG_H_
#define BXILOG_H_

#ifndef BXICFFI
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
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
 * @brief   High Performance Logging Module
 *
 * ### Overview
 *
 * This module provides a high performance logging library (>500k logs/s),
 * for heavy multi-threaded processes, while supporting fork() system call,
 * and signal handling. It provides convenient functions to deal with:
 *
 * - logging at various log levels: `bxilog_level_e`,
 * - logging configuration: `bxilog_cfg_registered()`,
 * - error reporting: `BXILOG_REPORT()`,
 * - assertion: `BXIASSERT()` and `BXIUNREACHABLE_STATEMENT()`,
 * - exiting: `BXIEXIT`,
 * - signal handling set-up: `bxilog_install_sighandler()`
 *
 * ### Basic 4-steps usage:
 *
 * 1. use macro `SET_LOGGER()` at the beginning of each C module (in .c, not in .h)
 * 2. in the same source file, use `DEBUG()`, `INFO()`, `WARNING()`, `ERROR()`, ...
 * 3. call `bxilog_init()` (in your `main()`) to initialize the whole module
 * 4. call `bxilog_finalize()` (in your `main()`) otherwise *your program won't terminate*!
 *
 * ### Notes:
 *
 * - `OUT()` should be seen as a replacement for `printf()`
 * - Loggers created with `SET_LOGGER()` are automatically registered with the library
 * - Dynamically created loggers with `bxilog_new()` (such as from higher level languages)
 *   can be registered with `bxilog_register()`
 * - Default level of each logger is `BXILOG_LOWEST`. Use `bxilog_cfg_registered()`
 *   to configure all registered loggers.
 * - Use `BXIASSERT()` instead of `assert()` and `BXIEXIT()` instead of `exit()`
 *   to guarantee that logs are flushed before exiting the process.
 * - For the same reason, you might need the installation of a signal handler in order
 *   to call `bxilog_finalize()` properly and to flush all pending messages.
 *   See `bxilog_install_sighanler()` for this purpose.
 * - For `bxierr_p` reporting, use `BXILOG_REPORT`.
 * - Exiting a process (with `BXIEXIT`) normally requires at least the level
 *   `BXILOG_CRITICAL` or above and a standard exit code (see sysexits.h).
 *
 * - All functions in this library are thread safe.
 * - Fork is supported, but note that after a `fork()`, while the parent remains in same
 *   state as before the `fork()`, the child have to call `bxilog_init()` in order to
 *   produce logs.
 *
 * Full running example (compile with `-lbxibase`):
 *
 *      #include <sys/types.h>
 *      #include <unistd.h>
 *      #include <string.h>
 *      #include <libgen.h>
 *      #include <sysexits.h>
 *
 *      #include <bxi/base/mem.h>
 *      #include <bxi/base/str.h>
 *      #include <bxi/base/err.h>
 *      #include <bxi/base/time.h>
 *      #include <bxi/base/log.h>
 *
 *      // Create a logger for my module/main
 *      SET_LOGGER(MY_LOGGER, "my.logger");
 *
 *      // This function does not raise an error
 *      // It deals with error itself, internally
 *      // using BXILOG_REPORT()
 *      void foo_noraise(void) {
 *        struct timespec start, stop;
 *        bxierr_p err = bxitime_get(CLOCK_MONOTONIC, &start);
 *        if (bxierr_isko(err)) BXILOG_REPORT(MY_LOGGER,
 *                                            BXILOG_ERROR,
 *                                            err,
 *                                            "Calling bxitime_gettime() failed");
 *        DEBUG(MY_LOGGER, "Producing a log");
 *        double duration;
 *        err = bxitime_duration(CLOCK_MONOTONIC, start, &duration);
 *        if (bxierr_isko(err)) BXILOG_REPORT(MY_LOGGER,
 *                                            BXILOG_ERROR,
 *                                            err,
 *                                            "Calling bxitime_duration() failed");
 *        OUT(MY_LOGGER, "Duration: %lf", duration);
 *      }
 *
 *      // This function is the equivalent but it does raise errors
 *      // It uses BXIERR_CHAIN() for this purpose.
 *      bxierr_p bar_raise(void) {
 *        struct timespec start, stop;
 *        bxierr_p err = BXIERR_OK, err2;
 *        err2 = bxitime_get(CLOCK_MONOTONIC, &start);
 *        BXIERR_CHAIN(err, err2); // This makes err2->cause = err, err=err2 on error
 *        // We choose to continue here, despite the error.
 *        // We migh have returned sooner with:
 *        // if (bxierr_isko(err)) return err;
 *        DEBUG(MY_LOGGER, "Producing a log");
 *        double duration = 0;
 *        err = bxitime_duration(CLOCK_MONOTONIC, start, &duration);
 *        BXIERR_CHAIN(err, err2);
 *        OUT(MY_LOGGER, "Duration: %lf", duration);
 *        return err;
 *      }
 *
 *      int main(int argc, char** argv) {
 *        // Produce the log on stdout
 *        bxierr_p err = bxilog_init(argv[0], "-");
 *
 *        if (bxierr_isko(err)) {
 *          // Nothing can be logged, use fprintf() exceptionnaly
 *          char * str = bxierr_str(err);
 *          fprintf(stderr, "Calling bxilog_init() failed: %s", str);
 *          BXIFREE(str); // Nullify the pointer!
 *          BXIASSERT(MY_LOGGER, NULL == str);
 *          int rc = err->code;
 *          bxierr_destroy(&err);
 *          exit(rc);
 *        }
 *
 *        DEBUG(MY_LOGGER, "Calling noraise");
 *        foo_noraise();
 *        DEBUG(MY_LOGGER, "Calling raise");
 *        err = bar_raise();
 *        if (bxierr_isko(err)) {
 *          // Exit the program
 *          BXIEXIT(EX_SOFTWARE, err, MY_LOGGER, BXILOG_CRITICAL);
 *          // The following statement must never be reached
 *          BXIUNREACHABLE_STATEMENT(MY_LOGGER);
 *        }
 *
 *        err = bxilog_finalize();
 *        if (bxierr_isko(err)) {
 *          // Logging cannot be used, use fprintf() exceptionnaly
 *          char * str = bxierr_str(err);
 *          fprintf(stderr, "Calling bxilog_finalize() failed: %s",
 *                  str);
 *          BXIFREE(str);
 *          bxierr_destroy(&err);
 *        }
 *        exit(EXIT_SUCCESS);
 *      }
 *
 * *************************************************************************************
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

// WARNING: These macros should be first initialized with bxilog_init_default_logger()
// This is reentrant

/**
 * Produce a log at the `BXILOG_LOWEST` level
 */
#define LOWEST(logger, ...) bxilog_log(logger, BXILOG_LOWEST, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__);
/**
 * Produce a log at the `BXILOG_TRACE` level
 */
#define TRACE(logger, ...) bxilog_log(logger, BXILOG_TRACE, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__);
/**
 * Produce a log at the `BXILOG_FINE` level
 */
#define FINE(logger, ...) bxilog_log(logger, BXILOG_FINE, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_DEBUG` level
 */
#define DEBUG(logger, ...) bxilog_log(logger, BXILOG_DEBUG, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_INFO` level
 */
#define INFO(logger, ...)  bxilog_log(logger, BXILOG_INFO, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_OUTPUT` level
 */
#define OUT(logger, ...)   bxilog_log(logger, BXILOG_OUTPUT, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_NOTICE` level
 */
#define NOTICE(logger, ...)  bxilog_log(logger, BXILOG_NOTICE, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_WARNING` level
 */
#define WARNING(logger, ...)  bxilog_log(logger, BXILOG_WARNING, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_ERROR` level
 */
#define ERROR(logger, ...)   bxilog_log(logger, BXILOG_ERROR, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_CRITICAL` level
 */
#define CRITICAL(logger, ...)  bxilog_log(logger, BXILOG_CRITICAL, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_ALERT` level
 */
#define ALERT(logger, ...)  bxilog_log(logger, BXILOG_ALERT, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_PANIC` level
 */
#define PANIC(logger, ...)  bxilog_log(logger, BXILOG_PANIC, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)


/**
 * Define the bxierr code for `BXIASSERT`
 *
 * @see bxierr_p
 */
#define BXIASSERT_CODE 455327 // Leet speak! ;-)

/**
 * Define the bxierr code for `BXIUNREACHABLE_STATEMENT`
 */
#define BXIUNREACHABLE_STATEMENT_CODE 666 // Devil's code! ;-)

#define BXIBUG_STD_MSG "This is a bug and should be reported as such.\n"                \
                       "In your report, do not omit the following informations:\n"      \
                       "\t- version of the product;\n"                                  \
                       "\t- full command line arguments;\n"                             \
                       "\t- the logging file at the lowest log level.\n"                \
                       "Contact Bull for bug submission.\n"                             \
                       "Thanks and Sorry."

/**
 * Exit the program with given `exit_code`.
 *
 * The given `bxierr` is used to produce a log on the given `logger` at the
 * given `level`,  before flushing the log and exiting the program.
 *
 * Usage looks like (for an `errno` related error message):
 *
 *      errno = 0;
 *      int rc = function_call(...);
 *      if (rc != 0) BXIEXIT(EX_OSERR,  // See sysexits.h for other standard exit code
 *                           bxierr_errno("Can't call function_call()),
 *                           MYLOGGER, BXILOG_ERROR);
 *
 * Same stuff with a generic `bxierr`:
 *
 *      bxierr_p err = function_call(...);
 *      if (BXIERR_OK != err) BXIEXIT(EXIT_FAILURE, err, MYLOGGER, BXILOG_ERROR);
 *
 * @param exit_code the exit code to exit the program with (see `sysexits.h`)
 * @param bxierr the `bxierr` to log before exiting
 * @param logger the `logger` to produce a log with
 * @param level the `bxilevel_e` for the log (should be `BXILOG_CRITICAL` at least)
 */
#define BXIEXIT(exit_code, bxierr, logger, level) do {                              \
    bxilog_exit((exit_code), (bxierr), (logger), (level),                           \
                __FILE__, ARRAYLEN(__FILE__),                                       \
                __func__, ARRAYLEN(__func__),                                       \
                __LINE__);                                                          \
    } while(0)

/**
 * Equivalent to assert() but ensure logs are flushed before exiting.
 *
 * @param logger the logger to use for logging before exiting
 * @param expr the boolean expression to check
 * @see BXIEXIT()
 */
#define BXIASSERT(logger, expr) do {                                                \
        bxilog_assert(logger, expr,                                                 \
                      __FILE__, ARRAYLEN(__FILE__),                                 \
                      __func__, ARRAYLEN(__func__),                                 \
                      __LINE__, #expr);                                             \
    } while(0)


/**
 * Use this macro to exit with a log and a flush on unreachable statements.
 *
 * Call `BXIEXIT()` internally.
 *
 * @param logger the logger to use in the case the statement is reached
 *
 * @see BXIEXIT()
 */
#define BXIUNREACHABLE_STATEMENT(logger) do {                                       \
    BXIEXIT(EX_SOFTWARE,                                                            \
            bxierr_new(BXIUNREACHABLE_STATEMENT_CODE,                               \
                                 NULL, NULL, NULL,                                  \
                                 "Unreachable statement reached! This is a bug"     \
                                 BXIBUG_STD_MSG),                                   \
            (logger),                                                               \
            BXILOG_CRITICAL);                                                       \
    } while(0)

/**
 * Log the given error with the given message and destroys it.
 *
 * Basic usage is:
 *
 *      err = f(...);
 *      if (bxierr_isko(err)) BXILOG_REPORT(MY_LOGGER, BXILOG_ERROR, err,
 *                                          "Calling f() failed");
 *      ...
 */
#define BXILOG_REPORT(logger, level, err, ...) do {                                 \
        bxilog_report((logger), (level), (err),                                     \
                      __FILE__, ARRAYLEN(__FILE__),                                 \
                      __func__, ARRAYLEN(__func__),                                 \
                      __LINE__, __VA_ARGS__);                                       \
    } while(0)


/**
 * Create a log using the given logger at the given level.
 *
 * @see `bxilog_log_nolevel_check()`
 */
// TODO: do log the actual log also with the same format than the actual log instead
// of throwing it away
#define bxilog_log(logger, lvl, filename, filename_len, funcname, funcname_len, line, ...) do {\
        if (bxilog_is_enabled_for((logger), (lvl))) {                                   \
            bxierr_p __err__ = bxilog_log_nolevelcheck((logger), (lvl),                 \
                                                       (filename), (filename_len),      \
                                                       (funcname), (funcname_len),      \
                                                       (line), __VA_ARGS__);            \
            if (bxierr_isko(__err__)) {                                                 \
                char * str = bxierr_str(__err__);                                       \
                fprintf(stderr, "Can't produce a log: %s", str);                        \
                BXIFREE(str);                                                           \
                bxierr_destroy(&__err__);                                               \
            }                                                                           \
        }                                                                               \
    } while(0);



/**
 * Defines a new logger as a global variable
 * Should be set inside your .c file, in order to define a new logger structure.
 * Note that by default, loggers level is set to `BXILOG_LOWEST`
 */
#ifdef __cplusplus
#define SET_LOGGER(variable_name, logger_name) \
    struct bxilog_s variable_name ## _s = { \
                                        logger_name,\
                                        ARRAYLEN(logger_name),\
                                        BXILOG_LOWEST\
    };\
    bxilog_p variable_name = &variable_name ## _s;\
    __attribute__((constructor)) void __bxilog_register_log__ ## variable_name(void) {\
        bxilog_register(variable_name);\
    }\
    __attribute__((destructor)) void __bxilog_unregister_log__ ## variable_name(void) {\
        bxilog_unregister(variable_name);\
    }
#else
#ifdef BXICFFI
#define SET_LOGGER(variable_name, logger_name) \
    struct bxilog_s variable_name ## _s = { \
                                        .name = logger_name,\
                                        .name_length = ARRAYLEN(logger_name),\
                                        .level = BXILOG_LOWEST\
    };\
    bxilog_p variable_name = &variable_name ## _s;
#else
#define SET_LOGGER(variable_name, logger_name) \
    struct bxilog_s variable_name ## _s = { \
                                        .name = logger_name,\
                                        .name_length = ARRAYLEN(logger_name),\
                                        .level = BXILOG_LOWEST\
    };\
    bxilog_p variable_name = &variable_name ## _s;\
    __attribute__((constructor)) void __bxilog_register_log__ ## variable_name(void) {\
        bxilog_register(variable_name);\
    }\
    __attribute__((destructor)) void __bxilog_unregister_log__ ## variable_name(void) {\
         bxilog_unregister(variable_name);\
    }
#endif
#endif


/**
 * The error code returned when a function is called while the library is
 * in an illegal state.
 */
#define BXILOG_ILLEGAL_STATE_ERR       1

/**
 * The error code returned when a configuration error is encountered
 */
#define BXILOG_CONFIG_ERR        2

/**
 * The error code returned when an error occurred within the underlying protocol.
 */
#define BXILOG_IHT2BC_PROTO_ERR 3

/**
 * The error code returned when a signal prevented a function to complete successfully.
 */
#define BXILOG_SIGNAL_ERR     10

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

/**
 *  Superset of Syslog levels:
 *  see for a good introduction on log levels and their usage.
 *  http://www.ypass.net/blog/2012/06/introduction-to-syslog-log-levelspriorities/
 */
typedef enum {
    // Note: first letter of a log level name should be unique
    BXILOG_PANIC,                           //!< system is unusable
    BXILOG_ALERT,                           //!< action must be taken immediately
    BXILOG_CRITICAL,                        //!< critical conditions
    BXILOG_ERROR,                           //!< error conditions
    BXILOG_WARNING,                         //!< warning conditions
    BXILOG_NOTICE,                          //!< notice conditions
    BXILOG_OUTPUT,                          //!< `printf()` replacement
    BXILOG_INFO,                            //!< informational message
    BXILOG_DEBUG,                           //!< debug-level message
    BXILOG_FINE,                            //!< detailed debug-level message
    BXILOG_TRACE,                           //!< very detailed debug-level message
    BXILOG_LOWEST,                          //!< most detailed debug-level message
// Alias
#ifndef BXICFFI
    BXILOG_EMERG = BXILOG_PANIC,    //!< alias for `BXILOG_EMERG`
    BXILOG_EMERGENCY = BXILOG_PANIC,//!< alias for `BXILOG_EMERGENCY`
    BXILOG_CRIT = BXILOG_CRITICAL,  //!< alias for `BXILOG_CRIT`
    BXILOG_ERR = BXILOG_ERROR,      //!< alias for `BXILOG_ERR`
    BXILOG_WARN = BXILOG_WARNING,   //!< alias for `BXILOG_WARN`
    BXLOG_OUT = BXILOG_OUTPUT,      //!< alias for `BXILOG_OUT`
#endif
} bxilog_level_e;


/**
 * Data structure representing a logger.
 *
 * @see bxilog_p
 */
struct bxilog_s {
    const char * name;              //!< Logger name
    size_t name_length;             //!< Logger name length, including NULL ending byte.
    bxilog_level_e level;           //!< Logger level
};


/**
 * A logger "object".
 *
 * Use `SET_LOGGER()` to create one (or `bxilog_new()`
 * from a high level language).
 */
typedef struct bxilog_s * bxilog_p;

/**
 * A logger configuration item.
 *
 * @see bxilog_cfg_registered()
 */
typedef struct bxilog_cfg_item_s {
    const char * prefix;             //!< logger name prefix
    bxilog_level_e level;            //!< the level to set each matching logger to
} bxilog_cfg_item_s;

/**
 * A logger configuration item.
 *
 * @see bxilog_cfg_registered()
 */
typedef bxilog_cfg_item_s * bxilog_cfg_item_p;
// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************



/**
 * Register the current logger.
 *
 * This is used by macro `SET_LOGGER()`. Use `SET_LOGGER()` instead.
 *
 * @param logger the logger to register with.
 */
void bxilog_register(bxilog_p logger);

/**
 * Unregister the current logger.
 *
 * This is used by macro `SET_LOGGER()`. Use `SET_LOGGER()` instead.
 *
 * @param logger the logger to unregister with.
 */
void bxilog_unregister(bxilog_p logger);

/**
 * Return all registered loggers.
 *
 * @param loggers a pointer on an array of loggers where the result should be returned.
 *
 * @return the number of loggers in the returned array
 *
 * @see bxilog_register()
 * @see bxilog_unregister()
 */
size_t bxilog_get_registered(bxilog_p *loggers[]);

/**
 * Configure all registered loggers with the given array of configuration items.
 *
 * For each registered logger, a check is performed to know if its `bxilog_p.name`
 * matches one of the given configuration item `bxilog_cfg_item_p.prefix`.
 * If this is the case, its level is set to the related configuration
 * item `bxilog_cfg_item_p.level`.
 *
 * Note: the algorithm scan the configuration items in order. Therefore, the usual case
 * is to specify configuration items from the most generic one to the most specific one.
 * In particular, the first item can be the empty string "" with the default level,
 * since all registered loggers will match. Then, other configuration items can be
 * specified with other log levels.
 *
 * Full running example (compile with `-lbxibase`):
 *
 *
 *     #include <assert.h>
 *
 *     #include <bxi/base/err.h>
 *     #include <bxi/base/log.h>
 *
 *     // Create a logger for my module/main
 *     SET_LOGGER(MY_LOGGER, "my.logger");
 *     // And other loggers to play with
 *     SET_LOGGER(LOGGER_A, "a.logger");
 *     SET_LOGGER(LOGGER_AB, "a.b.logger");
 *     SET_LOGGER(LOGGER_AC, "a.c.logger");
 *
 *     char ** LEVEL_NAMES;
 *
 *     void log_stuff(bxilog_p logger) {
 *       WARNING(logger, "A message");
 *       OUT(logger, "A message");
 *       DEBUG(logger, "A message");
 *     }
 *
 *     void display_loggers(size_t n, bxilog_p loggers[n]) {
 *
 *       for (size_t i = 0; i < n; i++) {
 *         OUT(MY_LOGGER, "%s: %s",
 *         loggers[i]->name,
 *         LEVEL_NAMES[loggers[i]->level]);
 *       }
 *     }
 *
 *     int main(int argc, char** argv) {
 *       // Produce the log on stdout
 *       bxierr_p err = bxilog_init(argv[0], "-");
 *       assert(bxierr_isok(err));
 *
 *       // Fetching log level names
 *       size_t n = bxilog_get_all_level_names(&LEVEL_NAMES);
 *       BXIASSERT(MY_LOGGER, n > 0 && NULL != LEVEL_NAMES);
 *
 *
 *       // Fetching all registered loggers
 *       bxilog_p *loggers = NULL;
 *       n = bxilog_get_registered(&loggers);
 *       BXIASSERT(MY_LOGGER, n>0 && NULL != loggers);
 *
 *       OUT(MY_LOGGER, "Before configuration:");
 *       display_loggers(n, loggers);
 *       log_stuff(LOGGER_A);
 *       log_stuff(LOGGER_AB);
 *       log_stuff(LOGGER_AC);
 *
 *       bxilog_cfg_item_s cfg[] = {{.prefix="", .level=BXILOG_LOWEST},
 *                      {.prefix="a", .level=BXILOG_OUTPUT},
 *                      {.prefix="a.b", .level=BXILOG_WARNING},
 *       };
 *       bxilog_cfg_registered(3, cfg);
 *       OUT(MY_LOGGER, "After configuration:");
 *       display_loggers(n, loggers);
 *       log_stuff(LOGGER_A);
 *       log_stuff(LOGGER_AB);
 *       log_stuff(LOGGER_AC);
 *
 *       err = bxilog_finalize();
 *       assert(bxierr_isok(err));
 *       return 0;
 *     }
 *
 *
 * @param n the number of configuration items in the `cfg` array
 * @param cfg an array of configuration items
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_cfg_registered(size_t n, bxilog_cfg_item_s cfg[]);

/**
 * Returns the level corresponding to the given string representation.
 *
 * @param level_str the log level name
 * @param level a pointer where the result should be returned.
 *
 * @result BXIERR_OK on success, anything else on error.
 *
 * @see bxilog_get_all_level_names()
 */
bxierr_p bxilog_get_level_from_str(char * level_str, bxilog_level_e *level);

/**
 * Return an array of (statically allocated) strings representing all known
 * level names.
 *
 * @param names a pointer on an array of strings that will be set by this function.
 *
 * @return the size of the array
 */
size_t bxilog_get_all_level_names(char *** names);

/**
 * Initialize the BXI logging module.
 *
 * WARNING: this function should be called once only (from the `main()` usually).
 *
 * If program_name is NULL, a name will be generated somehow. ;-)
 *
 * Note: `bxilog_init()` does not set any signal handler.
 *
 * Most of the time, you want to guarantee that bxilog_finalize() will be called
 * before exiting whatever the reason is to prevent log lost.
 *
 * This library provides `bxilog_install_sighandler()` that can be used for this
 * specific purpose. It should be called just after `bxilog_init()`.
 *
 * @param progname the progname, as given by argv[0]
 * @param filename the filename to write into ('-' means standard output,
 *         '+': means stdandard error )
 * @return BXIERR_OK on success, anything else on error.
 *
 */
bxierr_p bxilog_init(const char * progname, const char * filename);

/**
 * Release all resources used by the logging system. After this call,
 * no other thread will be able to perform a log.
 *
 * A return result different than BXIERR_OK is an error
 * related to a bad state such as when
 * `bxilog_init()` has not been called.
 *
 * @return BXIERR_OK on success, anything else on error.
 *
 */
bxierr_p bxilog_finalize(void);

/**
 * Create a new logger instance.
 *
 * Note: this function is mainly for language bindings (such as Python).
 * In C, use `SET_LOGGER` macro instead.
 *
 * @param logger_name a logger name
 *
 * @return a new logger instance
 *
 * @see SET_LOGGER
 */
bxilog_p bxilog_new(const char * logger_name);

/**
 * Destroy the given logger.
 *
 * @param self_p a pointer on a logger instance
 */
void bxilog_destroy(bxilog_p * self_p);


/**
 * Request a flush of all pending logs.
 *
 * Return when all messages have been flushed.
 *
 * @return BXIERR_OK on success, anything else is an error.
 */
bxierr_p bxilog_flush(void);


/**
 * Create a log unconditionally. This is mainly used by macros defined above
 * that already check the logger level, removing the useless function call.
 *
 * If you need the check, either do it manually, or use the `bxilog_log()` macro.
 *
 * WARNING: the `filename_len` and `funcname_len` should include the NULL
 * terminated string. Use strlen(s) + 1 for dynamic strings,
 * or ARRAYLEN(a) for static strings.
 *
 * @param logger the logger to perform the log with
 * @param level the level at which the log must be emitted
 * @param filename the name of the source file the log comes from
 * @param filename_len the length of 'filename' including the NULL terminating byte
 * @param funcname the name of the function the log comes from
 * @param funcname_len the length of 'funcname' including the NULL terminating byte
 * @param line the line number in file 'filename' the log comes from
 * @param fmt the printf like format of the message
 * @return BXIERR_OK on success, any other value is an error
 *
 * @see bxilog_log()
 * @see bxierr_new()
 */
bxierr_p bxilog_log_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                 char * filename, size_t filename_len,
                                 const char * funcname, size_t funcname_len,
                                 int line,
                                 const char * fmt, ...)
#ifndef BXICFFI
                                __attribute__ ((format (printf, 8, 9)))
#endif
    ;


#ifndef BXICFFI
/**
 * Equivalent to `bxilog_log_nolevelcheck()` but with a va_list instead of
 * a variable number of arguments.
 *
 * @see bxilog_log_nolevelcheck
 */
bxierr_p bxilog_vlog_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                  char * filename, size_t filename_len,
                                  const char * funcname, size_t funcname_len,
                                  const int line,
                                  const char * fmt, va_list arglist);
#endif

/**
 * Get the log level of the given logger
 *
 * @param logger the logger instance
 *
 * @return the given logger log level
 */
bxilog_level_e bxilog_get_level(const bxilog_p logger);

/**
 * Set the log level for the given logger.
 *
 * @param logger the logger instance
 * @param level the log level
 */
void bxilog_set_level(const bxilog_p logger, const bxilog_level_e level);

/**
 * Return true if the given logger is enabled at the given log level.
 *
 * @param logger the logger instance
 * @param level the log level
 *
 * @return true if the given logger is enabled at the given log level.
 */
#ifndef BXICFFI
inline bool bxilog_is_enabled_for(const bxilog_p logger, const bxilog_level_e level) {
    assert (logger != NULL && level <= BXILOG_LOWEST);
    return level <= logger->level;
}
#else
bool bxilog_is_enabled_for(const bxilog_p logger, const bxilog_level_e level);
#endif

/**
 * Log the given `err` on the given `logger`, flush the logging system and exit
 * the program with the given `exit_code`.
 *
 * This function sole purpose is to make the macro BXIEXIT() cleaner.
 * Use BXIEXIT() instead.
 *
 * @param exit_code the exit code to exit the program with (see sysexits.h)
 * @param err the bxierr to log
 * @param logger the logger to produce a log with
 * @param level the level for the log (should be BXILOG_CRITICAL at least)
 * @param file the filename from which the exit should appear to come from
 * @param filelen the length of the filename
 * @param func the function name which the exit should appear to come from
 * @param funclen the function name length
 * @param line the line number in the function from which the exit should appear
 *        to come from
 * @see BXIEXIT
 */
void bxilog_exit(int exit_code,
                 bxierr_p err,
                 bxilog_p logger,
                 bxilog_level_e level,
                 char * file,
                 size_t filelen,
                 const char * func,
                 size_t funclen,
                 int line);

/**
 * Exit the program if the given `result` (normally coming from the given `expr`)
 * is false.
 *
 * This function sole purpose is to make the macro BXIASSERT() cleaner.
 * Use BXIASSERT() instead.
 *
 * @see BXIASSERT
 */
void bxilog_assert(bxilog_p logger, bool result,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line, char * expr);

/**
 * Report an error and destroys it.
 *
 * This function sole purpose is to make the macro BXILOG_REPORT() cleaner.
 * Use BXILOG_REPORT() instead.
 *
 * @see BXILOG_REPORT
 */
void bxilog_report(bxilog_p logger, bxilog_level_e level, bxierr_p err,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line,
                   const char * fmt, ...)
#ifndef BXICFFI
                   __attribute__ ((format (printf, 9, 10)))
#endif
;

/**
 * Set a logical rank to the current pthread.
 * This is used when displaying a log to differentiate logical threads
 * from kernel threads.
 *
 * @param rank the logical rank of the current thread
 *
 * @see bxilog_get_thread_rank
 * @return BXIERR_OK on success, any other values is an error
 */
bxierr_p bxilog_set_thread_rank(uint16_t rank);

/**
 * Return the logical rank of the current pthread.
 *
 * If no logical rank has been set by `bxilog_set_thread_rank()`,
 * a generated logical rank will be returned
 * (uniqueness is not guaranteed).
 *
 * @param rank_p a pointer on the result
 *
 * @return BXIERR_OK on success, any other values is an error
 */
bxierr_p bxilog_get_thread_rank(uint16_t * rank_p);

/**
 * Install a signal handler that call bxilog_finalize() automatically
 * on reception of the following signals:  SIGTERM, SIGINT, SIGSEGV, SIGBUS,
 * SIGFPE, SIGILL, SIGABRT.
 *
 * Note that no signal handler is set for SIGQUIT (ctrl-\ on a standard UNIX terminal)
 * (so you can bypass the sighandler if you need to).
 *
 * @return BXIERR_OK on success, any other values is an error
 */

bxierr_p bxilog_install_sighandler();

#ifndef BXICFFI
/**
 * Create a sigset structure according to the given array of signal numbers.
 *
 * @param sigset
 * @param signum
 * @param n
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_sigset_new(sigset_t *sigset, int * signum, size_t n);

/**
 * Return a string representation of the given signal number using the
 * given siginfo or sfdinfo (only one must be NULL).
 *
 * The returned string will have to be released using FREE().
 */
char * bxilog_signal_str(const int signum,
                         const siginfo_t * siginfo,
                         const struct signalfd_siginfo * sfdinfo);
#endif



#endif /* BXILOG_H_ */
