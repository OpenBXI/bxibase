/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_H_
#define BXILOG_H_

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
 * Define the error code when a bad level configuration is given.
 *
 * @note: BAD LVL in Leet Speak
 *
 * @note: The bxierr_p.data holds the incorrect level as a casted (void*).
 *        It must not be freed since it is not a memory address.
 *
 */
#define BXILOG_BADLVL_ERR 840111

/**
 * Define the error code when an error occurred while flushing
 *
 * @note the bxierr_p.data holds a bxierr_group data containing all errors
 * encountered while flushing. This data will be freed when the error is freed.
 */
#define BXILOG_FLUSH_ERR 51054


/**
 *  Error code used by handlers to specify they must exit
 */
#define BXILOG_HANDLER_EXIT_CODE 41322811
/**
 * Exit the program with given `exit_code`.
 *
 * The given `bxierr` is used to produce a log on the given `logger` at the
 * given `level`,  before flushing the log and exiting the program.
 *
 * Usage looks like (for an `errno` related error message):
 * ~~~{C}
 * errno = 0;
 * int rc = function_call(...);
 * if (rc != 0) BXIEXIT(EX_OSERR,  // See sysexits.h for other standard exit code
 *                      bxierr_errno("Can't call function_call()),
 *                      MYLOGGER, BXILOG_ERROR);
 * ~~~
 *
 * Same stuff with a generic `bxierr`:
 * ~~~{C}
 * bxierr_p err = function_call(...);
 * if (BXIERR_OK != err) BXIEXIT(EXIT_FAILURE, err, MYLOGGER, BXILOG_ERROR);
 * ~~~
 *
 * @param[in] exit_code the exit code to exit the program with (see `sysexits.h`)
 * @param[in] bxierr the `bxierr` to log before exiting
 * @param[in] logger the `logger` to produce a log with
 * @param[in] level the `bxilevel_e` for the log (should be `BXILOG_CRITICAL` at least)
 */
#define BXIEXIT(exit_code, bxierr, logger, level) do {                              \
    bxilog_exit((exit_code), (bxierr), (logger), (level),                           \
                (char *)__FILE__, ARRAYLEN(__FILE__),                               \
                __func__, ARRAYLEN(__func__),                                       \
                __LINE__);                                                          \
    } while(false)

/**
 * Equivalent to assert() but ensure logs are flushed before exiting.
 *
 * @param[in] logger the logger to use for logging before exiting
 * @param[in] expr the boolean expression to check
 * @see BXIEXIT()
 */
#define BXIASSERT(logger, expr) do {                                                \
            bxilog_assert(logger, expr,                                             \
                          (char *)__FILE__, ARRAYLEN(__FILE__),                     \
                          __func__, ARRAYLEN(__func__),                             \
                          __LINE__, #expr);                                         \
    } while(false)


/**
 * Use this macro to exit with a log and a flush on unreachable statements.
 *
 * Call `BXIEXIT()` internally.
 *
 * @param[in] logger the logger to use in the case the statement is reached
 *
 * @see BXIEXIT()
 */
#define BXIUNREACHABLE_STATEMENT(logger) do {                                       \
    BXIEXIT(EX_SOFTWARE,                                                            \
            bxierr_new(BXIUNREACHABLE_STATEMENT_CODE,                               \
                                 NULL, NULL, NULL, NULL,                                  \
                                 "Unreachable statement reached! This is a bug"     \
                                 BXIBUG_STD_MSG),                                   \
            (logger),                                                               \
            BXILOG_CRITICAL);                                                       \
    } while(false)



/**
 * Log the given error with the given message and destroys it.
 *
 * Note the given err is assigned ::BXIERR_OK.
 *
 * Basic usage is:
 * ~~~{C}
 * err = f(...);
 * if (bxierr_isko(err)) BXILOG_REPORT(MY_LOGGER, BXILOG_ERROR, &err,
 *                                     "Calling f() failed");
 * assert(bxierr_isok(err)); // Always true, since BXILOG_REPORT
 *                           // destroys err and assign it ::BXIERR_OK
 * ...
 * ~~~
 *
 * @see bxilog_report
 * @see BXILOG_REPORT_KEEP
 * @see bxilog_report_keep
 */
#define BXILOG_REPORT(logger, level, err, ...) do {                                 \
        bxilog_report((logger), (level), (&err),                                     \
                      (char *)__FILE__, ARRAYLEN(__FILE__),                         \
                      __func__, ARRAYLEN(__func__),                                 \
                      __LINE__, __VA_ARGS__);                                     \
    } while(false)


/**
 * Log the given error with the given message.
 *
 * This is a convenience macro, used mainly in test. Most of the time,
 * an error reported, has no good reason from being kept in the process.
 * Therefore, one might prefer ::BXILOG_REPORT instead which destroys
 * the error after reporting.
 *
 * Basic usage is:
 * ~~~{C}
 * err = f(...);
 * if (bxierr_isko(err)) BXILOG_REPORT_KEEP(MY_LOGGER, BXILOG_ERROR, &err,
 *                                          "Calling f() failed");
 * ...
 * ~~~
 *
 * @see bxilog_report_keep
 * @see BXILOG_REPORT
 * @see bxilog_report
 */
#define BXILOG_REPORT_KEEP(logger, level, err, ...) do {                             \
        bxilog_report_keep((logger), (level), (&err),                                     \
                           (char *)__FILE__, ARRAYLEN(__FILE__),                         \
                           __func__, ARRAYLEN(__func__),                                 \
                           __LINE__, __VA_ARGS__);                                     \
    } while(false)

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
                bxierr_report(__err__, STDOUT_FILENO);                                  \
            }                                                                           \
        }                                                                               \
    } while(false);


/**
 * Defines a new logger as a global variable
 * Should be set inside your .c file, in order to define a new logger structure.
 * Note that by default, loggers level is set to `BXILOG_LOWEST`
 */
#ifdef __cplusplus
#define SET_LOGGER(variable_name, logger_name) \
    static struct bxilog_s variable_name ## _s = { \
                                        false, \
                                        logger_name,\
                                        ARRAYLEN(logger_name),\
                                        BXILOG_LOWEST\
    };\
    static bxilog_p const variable_name = &variable_name ## _s;\
    static __attribute__((constructor)) void __bxilog_register_log__ ## variable_name(void) {\
        bxilog_register(variable_name);\
    }\
    static __attribute__((destructor)) void __bxilog_unregister_log__ ## variable_name(void) {\
        bxilog_unregister(variable_name);\
    }
#else
#ifdef BXICFFI
#define SET_LOGGER(variable_name, logger_name) \
    static struct bxilog_s variable_name ## _s = { \
                                        .allocated = false, \
                                        .name = logger_name,\
                                        .name_length = ARRAYLEN(logger_name),\
                                        .level = BXILOG_LOWEST\
    };\
    static bxilog_p const variable_name = &variable_name ## _s;
#else
#define SET_LOGGER(variable_name, logger_name) \
    static struct bxilog_s variable_name ## _s = { \
                                        .allocated = false, \
                                        .name = logger_name,\
                                        .name_length = ARRAYLEN(logger_name),\
                                        .level = BXILOG_LOWEST\
    };\
    static bxilog_p const variable_name = &variable_name ## _s;\
    static __attribute__((constructor)) void __bxilog_register_log__ ## variable_name(void) {\
        bxilog_register(variable_name);\
    }\
    static __attribute__((destructor)) void __bxilog_unregister_log__ ## variable_name(void) {\
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
    bool allocated;                 //!< true if allocated on the heap, false otherwise
    const char * name;              //!< Logger name
    size_t name_length;             //!< Logger name length, including NULL ending byte
    bxilog_level_e level;           //!< Logger level
};


/**
 * A logger "object".
 *
 * Use `SET_LOGGER()` to create one (or `bxilog_get()`
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

#define TIMESPEC_SIZE 16
#ifndef BXICFFI
BXIERR_CASSERT(timespec_size, TIMESPEC_SIZE == sizeof(struct timespec));
#endif

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


typedef struct {
    size_t handlers_nb;
    int data_hwm;
    int ctrl_hwm;
    size_t tsd_log_buf_size;
    const char * progname;
    bxilog_handler_p * handlers;
    bxilog_handler_param_p * handlers_params;
} bxilog_param_s;

typedef bxilog_param_s * bxilog_param_p;


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
 * @param[in] logger the logger to register with.
 */
void bxilog_register(bxilog_p logger);

/**
 * Unregister the current logger.
 *
 * This is used by macro `SET_LOGGER()`. Use `SET_LOGGER()` instead.
 *
 * @param[in] logger the logger to unregister with.
 */
void bxilog_unregister(bxilog_p logger);

/**
 * Return  a copy of all registered loggers.
 *
 * Note the returned array is actually a copy of the
 * currently registered loggers since the latter can be modified concurrently by
 * other threads (however each element of the array -- logger instance -- are actual
 * pointers on effective loggers, not a copy of them).
 *
 * The array will have to be freed using BXIFREE().
 *
 * @param[out] loggers a pointer on an array of loggers where the result
 *             should be returned.
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
 * See @link bxilog_cfg.c bxilog_cfg.c for a full running example@endlink
 *
 *
 * @param[in] n the number of configuration items in the `cfg` array
 * @param[in] cfg an array of configuration items
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_cfg_registered(size_t n, bxilog_cfg_item_s cfg[]);

/**
 * Returns the level corresponding to the given string representation.
 *
 * @param[in] level_str the log level name
 * @param[out] level a pointer where the result should be returned.
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
 * @param[out] names a pointer on an array of strings that will be set by this function.
 *
 * @return the size of the array
 */
size_t bxilog_get_all_level_names(char *** names);

/**
 * Reset the bxilog configuration.
 */
void bxilog_reset_config();

bxierr_p bxilog_basic_config(const char *progname,
                             const char * filename,
                             bool append,
                             bxilog_param_p *result);

bxierr_p bxilog_unit_test_config(const char *progname,
                                 const char * filename,
                                 bool append,
                                 bxilog_param_p *result);

bxilog_param_p bxilog_param_new(const char * progname);
void bxilog_param_add(bxilog_param_p self, bxilog_handler_p handler, ...);

bxierr_p bxilog_param_destroy(bxilog_param_p * param_p);
void bxilog_handler_init_param(bxilog_handler_p handler, bxilog_handler_param_p param);
void bxilog_handler_clean_param(bxilog_handler_param_p param);

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
 * @param[in] param bxilog parameters
 *
 * @return BXIERR_OK on success, anything else on error.
 *
 * @see bxilog_param_p
 */
bxierr_p bxilog_init(bxilog_param_p param);

/**
 * Release all resources used by the logging system. After this call,
 * no other thread will be able to perform a log.
 *
 * A return result different than BXIERR_OK is an error
 * related to a bad state such as when
 * `bxilog_init()` has not been called.
 *
 * @param[in] flush flush before exiting if true
 * @return BXIERR_OK on success, anything else on error.
 *
 */
bxierr_p bxilog_finalize(bool flush);

/**
 * Get the logger with the given name.
 *
 * A new logger with the given name is created and registered
 * if it does not already exist.
 *
 * Note: this function is mainly for language bindings (such as Python).
 * In C, use `SET_LOGGER` macro instead.
 *
 * @param[in] logger_name a logger name
 * @param[out] result the logger instance with the given name
 *
 * @return BXIERR_OK if ok, anything else on error.
 *
 * @see SET_LOGGER
 */
bxierr_p bxilog_get(const char * logger_name, bxilog_p * result);

/**
 * Destroy the given logger.
 *
 * @param[in] self_p a pointer on a logger instance
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
 * Create a log unconditionally without formatting. This is mainly used for
 * high level languages binding.
 *
 *
 * WARNING: the `filename_len` and `funcname_len` should include the NULL
 * terminated string. Use strlen(s) + 1 for dynamic strings,
 * or ARRAYLEN(a) for static strings.
 *
 * @param[in] logger the logger to perform the log with
 * @param[in] level the level at which the log must be emitted
 * @param[in] filename the name of the source file the log comes from
 * @param[in] filename_len the length of 'filename' including the NULL terminating byte
 * @param[in] funcname the name of the function the log comes from
 * @param[in] funcname_len the length of 'funcname' including the NULL terminating byte
 * @param[in] line the line number in file 'filename' the log comes from
 * @param[in] rawstr the raw string
 * @param[in] rawstr_len the length of the string including the NULL terminating byte
 *
 * @return BXIERR_OK on success, any other value is an error
 *
 * @see bxilog_log()
 * @see bxierr_p
 */
bxierr_p bxilog_log_rawstr(const bxilog_p logger, const bxilog_level_e level,
                           char * filename, size_t filename_len,
                           const char * funcname, size_t funcname_len,
                           int line,
                           const char * rawstr, size_t rawstr_len);


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
 * @param[in] logger the logger to perform the log with
 * @param[in] level the level at which the log must be emitted
 * @param[in] filename the name of the source file the log comes from
 * @param[in] filename_len the length of 'filename' including the NULL terminating byte
 * @param[in] funcname the name of the function the log comes from
 * @param[in] funcname_len the length of 'funcname' including the NULL terminating byte
 * @param[in] line the line number in file 'filename' the log comes from
 * @param[in] fmt the printf like format of the message
 *
 * @return BXIERR_OK on success, any other value is an error
 *
 * @see bxilog_log()
 * @see bxierr_p
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
 * @param[in] logger the logger to perform the log with
 * @param[in] level the level at which the log must be emitted
 * @param[in] filename the name of the source file the log comes from
 * @param[in] filename_len the length of 'filename' including the NULL terminating byte
 * @param[in] funcname the name of the function the log comes from
 * @param[in] funcname_len the length of 'funcname' including the NULL terminating byte
 * @param[in] line the line number in file 'filename' the log comes from
 * @param[in] fmt the printf like format of the message
 * @param[in] arglist the va_list of all parameters for the given format string 'fmt'
 *
 * @return BXIERR_OK on success, any other value is an error
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
 * @param[in] logger the logger instance
 *
 * @return the given logger log level
 */
bxilog_level_e bxilog_get_level(const bxilog_p logger);

/**
 * Set the log level for the given logger.
 *
 * @param[in] logger the logger instance
 * @param[in] level the log level
 */
void bxilog_set_level(const bxilog_p logger, const bxilog_level_e level);

/**
 * Return true if the given logger is enabled at the given log level.
 *
 * @param[in] logger the logger instance
 * @param[in] level the log level
 *
 * @return true if the given logger is enabled at the given log level.
 */
#ifndef BXICFFI
inline bool bxilog_is_enabled_for(const bxilog_p logger, const bxilog_level_e level) {
    bxiassert(logger != NULL && level <= BXILOG_LOWEST);
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
 * @param[in] exit_code the exit code to exit the program with (see sysexits.h)
 * @param[in] err the bxierr to log
 * @param[in] logger the logger to produce a log with
 * @param[in] level the level for the log (should be BXILOG_CRITICAL at least)
 * @param[in] file the filename from which the exit should appear to come from
 * @param[in] filelen the length of the filename
 * @param[in] func the function name which the exit should appear to come from
 * @param[in] funclen the function name length
 * @param[in] line the line number in the function from which the exit should appear
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
 * @param[in] logger a logger instance
 * @param[in] result a boolean value (usually a check)
 * @param[in] file the file name
 * @param[in] filelen the file name length (including the terminal NULL byte)
 * @param[in] func the function name
 * @param[in] funclen the function name length (including the terminal NULL byte)
 * @param[in] line the line number
 * @param[in] expr a string representing the expression that returned the given result
 *
 * @see BXIASSERT
 */
void bxilog_assert(bxilog_p logger, bool result,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line, char * expr);

/**
 * Report an error and destroy it.
 *
 * Note: after this call bxierr_isko(*err) is always true.
 *
 * This function sole purpose is to make the macro BXILOG_REPORT() cleaner.
 * Use BXILOG_REPORT() instead.
 *
 * @param[in] logger a logger instance
 * @param[in] level the logger level to use while reporting the given error
 * @param[in] err pointer on the error to report
 * @param[in] file the file name
 * @param[in] filelen the file name length (including the terminal NULL byte)
 * @param[in] func the function name
 * @param[in] funclen the function name length (including the terminal NULL byte)
 * @param[in] line the line number
 * @param[in] fmt a printf-like format string
 *
 * @see BXILOG_REPORT
 */
void bxilog_report(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line,
                   const char * fmt, ...)
#ifndef BXICFFI
                   __attribute__ ((format (printf, 9, 10)))
#endif
;

/**
 * Report an error but do not destroy it.
 *
 * This function sole purpose is to make the macro BXILOG_REPORT_KEEP() cleaner.
 * Use BXILOG_REPORT_KEEP() instead.
 *
 * @param[in] logger a logger instance
 * @param[in] level the logger level to use while reporting the given error
 * @param[in] err pointer on the error to report
 * @param[in] file the file name
 * @param[in] filelen the file name length (including the terminal NULL byte)
 * @param[in] func the function name
 * @param[in] funclen the function name length (including the terminal NULL byte)
 * @param[in] line the line number
 * @param[in] fmt a printf-like format string
 *
 * @see BXILOG_REPORT_KEEP
 */
void bxilog_report_keep(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
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
 * @param[in] rank the logical rank of the current thread
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
 * @param[out] rank_p a pointer on the result
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
bxierr_p bxilog_install_sighandler(void);

#ifndef BXICFFI
/**
 * Create a sigset structure according to the given array of signal numbers.
 *
 * @param[out] sigset
 * @param[in] signum
 * @param[in] n
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_sigset_new(sigset_t *sigset, int * signum, size_t n);

/**
 * Return a string representation of the given signal number using the
 * given siginfo or sfdinfo (only one must be NULL).
 *
 * Note: the returned string will have to be released using FREE().
 *
 * @param[in] siginfo the signal information
 * @param[in] sfdinfo the signal information
 *
 * @return a string representation of the given signal number
 *
 */
char * bxilog_signal_str(const siginfo_t * siginfo,
                         const struct signalfd_siginfo * sfdinfo);
#endif

/**
 * Configure registered loggers using a string.
 *
 * The string must be in the following format:
 *      lvl_str     <-  [emergency, alert, critical, crit, error, err, warning, warn,
 *                       notice, output, info, debug, fine, trace, lowest]
 *      lvl_num     <-  [0-9]*
 *      lvl         <-  [lvl_num, level_str]
 *      prefix      <-  [A-z]+
 *      format      <-  (lvl:prefix,)*lvl:prefix
 *
 *  Each logger sharing same prefix as the parsed one will be set to the define level.
 *
 * @param[in] str is a string containing the configuration for the loggers.
 *
 * @return BXIERR_OK on success, anything else on error.
 *
 */
bxierr_p bxilog_parse_levels(char * str);

/**
 * Display on stderr the loggers which can be configured.
 */
void bxilog_display_loggers();


/**
 * @example bxilog_err.c
 * An example on how to use the BXI Logging API with high-level error
 * management. Compile with `-lbxibase`.
 *
 */

/**
 * @example bxilog_cfg.c
 * An example on how to configure the BXI Logging API. Compile with `-lbxibase`.
 *
 */

#endif /* BXILOG_H_ */
