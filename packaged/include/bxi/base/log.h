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

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

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
 * @see bxilog_config_p
 */
bxierr_p bxilog_init(bxilog_config_p param);

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
 * Request a flush of all pending logs.
 *
 * Return when all messages have been flushed.
 *
 * @return BXIERR_OK on success, anything else is an error.
 */
bxierr_p bxilog_flush(void);


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
