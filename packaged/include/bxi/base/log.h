/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_H_
#define BXILOG_H_

#ifndef BXICFFI
#include <stdbool.h>
#endif


#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log/level.h"
#include "bxi/base/log/logger.h"
#include "bxi/base/log/config.h"
#include "bxi/base/log/report.h"
#include "bxi/base/log/assert.h"
#include "bxi/base/log/exit.h"
#include "bxi/base/log/signal.h"
#include "bxi/base/log/thread.h"
#include "bxi/base/log/filter.h"
#include "bxi/base/log/registry.h"


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
 * This module provides a high performance logging library,
 * for heavy multi-threaded processes, while supporting fork() system call,
 * and signal handling. It provides:
 *
 * - loggers: `::bxilog_logger_p`
 * - logging levels: `::bxilog_level_e`,
 * - filtering: `::bxilog_filter_p`
 * - configuration: `::bxilog_config_p`,
 * - error reporting functions: `::BXILOG_REPORT()`,
 * - assertion function: `::BXIASSERT()` and `::BXIUNREACHABLE_STATEMENT()`,
 * - exiting function: `::BXIEXIT()`,
 * - signal handling set-up: `bxilog_install_sighandler()`
 * - log handlers specification: `::bxilog_handler_p`
 * - several logging handlers:
 *      - `::BXILOG_CONSOLE_HANDLER`: log to standard output and standard error;
 *      - `::BXILOG_FILE_HANDLER`: log to a file;
 *      - `::BXILOG_SYSLOG_HANDLER`: log to syslog;
 *
 * ### Basic 5-steps usage
 *
 * 1. use `SET_LOGGER()` at the beginning of each C module (in .c, not in .h);
 * 2. use `DEBUG()`, `INFO()`, `WARNING()`, `ERROR()`, ..., in your source file at will;
 * 3. use bxilog_basic_config() to create a new basic configuration or
 *    use bxilog_config_new() and bxilog_config_add_handler() to create your own
 *    configuration;
 * 4. call `bxilog_init()` (in your `main()`) to initialize the whole module;
 * 5. call `bxilog_finalize()` (in your `main()`) otherwise *your program won't terminate*!
 *
 * ### Notes
 *
 * - `OUT()` should be seen as a replacement for `printf()`;
 * - Loggers statically created with `SET_LOGGER()` or dynamically created
 *   with `bxilog_registry_get()` (such as from higher level languages)
 *   are automatically registered.
 * - Remember to use `bxilog_registry_set_filters()` or
 *   `bxilog_registry_parse_set_filters()` to define the logging level of all loggers.
 *   Default is `BXILOG_LOWEST`.
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
 * The error code returned when a function is called while the library is
 * in an illegal state.
 */
#define BXILOG_ILLEGAL_STATE_ERR       1

/**
 * The error code returned when an error occurred within the underlying protocol.
 */
#define BXILOG_IHT2BC_PROTO_ERR 3

/**
 * Define the error code when an error occurred while flushing
 *
 * @note the bxierr_p.data holds a bxierr_group data containing all errors
 * encountered while flushing. This data will be freed when the error is freed.
 */
#define BXILOG_FLUSH_ERR 51054

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
 * The character used for separation in logger names.
 */
#define BXILOG_NAME_SEP '.'

#define BXILOG_LIB_PREFIX "~"
#define BXILOG_HB_PREFIX "?"

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

/**
 * BXI Logging Constants Type (mainly for higher level languages binding)
 */
typedef struct {
    char * LIB_PREFIX;      //!< The library prefix used for logger names
    char * HB_PREFIX;       //!< The heartbeat prefix used for logger names
} bxilog_const_s;

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

#ifndef BXICFFI
/**
 * BXI Logging Constants (mainly for higher level languages binding)
 */
extern const bxilog_const_s bxilog_const;
#else
extern bxilog_const_s bxilog_const;
#endif


// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Initialize the BXI logging module.
 *
 * WARNING: this function should be called once only (from the `main()` usually).
 *
 * Note: `bxilog_init()` does not set any signal handler.
 * Most of the time, you want to guarantee that bxilog_finalize() is called
 * before exiting to prevent log lost.
 * This library provides `bxilog_install_sighandler()` that can be used for this
 * specific purpose. It should be called just after `bxilog_init()`.
 *
 * @param[in] config the configuration to use
 *
 * @return BXIERR_OK on success, anything else on error. On error, do not use
 *         BXIREPORT, BXIEXIT or any other such functions that produce logs: since
 *         the library has not been initialized correctly, result is undefined.
 *         Use bxierr_report() in such a case.
 *
 * @see bxilog_config_p
 * @see bxilog_basic_config()
 * @see bxilog_config_new()
 * @see bxierr_report()
 *
 */
bxierr_p bxilog_init(bxilog_config_p config);

/**
 * Release all resources used by the logging system. After this call,
 * no other thread will be able to perform a log.
 *
 * A return result different than BXIERR_OK is an error
 * related to a bad state such as when
 * `bxilog_init()` has not been called.
 *
 * @param[in] flush flush before exiting if true
 * @return BXIERR_OK on success, anything else on error. On error, do not use
 *         BXIREPORT, BXIEXIT or any other such functions that produce logs: since
 *         the library has not been initialized correctly, result is undefined.
 *         Use bxierr_report() in such a case.
 *
 */
bxierr_p bxilog_finalize(bool flush);


/**
 * Request a flush of all pending logs.
 *
 * Return when all messages have been flushed.
 *
 * @return BXIERR_OK on success, anything else is an error.
 */
bxierr_p bxilog_flush(void);


/**
 * Write the set of registered loggers along with the list of bxilog_level_e to
 * the given file descriptor.
 *
 * @param[in] fd a file descriptor (e.g: STDERR_FILENO)
 */
void bxilog_display_loggers(int fd);

/**
 * Display a message on a file descriptor.
 *
 * @note: do not use unless you implement a new bxilog handler or implement a new
 * bxilog feature.
 *
 * @param[in] msg a message
 * @param[in] fd an open file descriptor
 *
 */
void bxilog_rawprint(char* msg, int fd);

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
