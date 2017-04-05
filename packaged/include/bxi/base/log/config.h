/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_CONFIG_H_
#define BXILOG_CONFIG_H_

#ifndef BXICFFI
#include <stdbool.h>
#include <fcntl.h>
#endif

#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log/handler.h"

/**
 * Configuration of the High Performance Logging Module
 *
 * A bxilog configuration specifies various logging parameters, in particular
 * the set of logging handlers that must be used and with which parameters.
 *
 * @file    config.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
/**
 * Convenience macro for specifying append mode in ::bxilog_basic_config() and
 * ::bxilog_unit_test_config()
 *
 * @see bxilog_basic_config()
 * @see bxilog_unit_test_config()
 */
#define BXI_APPEND_OPEN_FLAGS O_CLOEXEC | O_CREAT | O_APPEND

/**
 * Convenience macro for specifying truncate mode in ::bxilog_basic_config() and
 * ::bxilog_unit_test_config()
 *
 * @see bxilog_basic_config()
 * @see bxilog_unit_test_config()
 */
#define BXI_TRUNC_OPEN_FLAGS O_CLOEXEC | O_CREAT | O_TRUNC

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

/**
 * The bxilog configuration structure.
 */
typedef struct {
    int data_hwm;                               //!< ZMQ High Water Mark of data zocket
    int ctrl_hwm;                               //!< ZMQ High Water Mark of control zocket
    size_t tsd_log_buf_size;                    //!< Size in bytes of the logging buffer
    size_t handlers_nb;                         //!< Number of logging handlers
    const char * progname;                      //!< Program name used by bxilog_init()
                                                //!< to set the process name (on linux
                                                //!< at least)
    bxilog_handler_p * handlers;                //!< The handlers list
    bxilog_handler_param_p * handlers_params;   //!< The handler parameters list
} bxilog_config_s;

/**
 * A bxilog configuration.
 */
typedef bxilog_config_s * bxilog_config_p;


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Create the default basic configuration consisting of a bxilog_console_handler_p and
 * and a file_handler_p.
 *
 * @note if the given filename is NULL, the bxilog_file_handler_p is not installed.
 * @note if the environment variable BXILOG_CONSOLE_HANDLER_LOGGERNAME_WIDTH is found,
 *       it is parsed as an int and given as a parameter to BXILOG_CONSOLE_HANDLER.
 *
 * @param[in] progname the program name (usually argv[0])
 * @param[in] filename if not NULL, the full file name where the bxilog_file_handler_p
 *            must produce its output
 * @param[in] open_flags the flags to give to open() (man 2 open).
 *
 * @return a new bxilog "standard" configuration
 *
 * @see BXI_APPEND_OPEN_FLAGS
 * @see BXI_TRUNC_OPEN_FLAGS
 *
 */
bxilog_config_p bxilog_basic_config(const char *progname,
                                    const char * filename,
                                    int open_flags,
                                    bxilog_filters_p filters);

/**
 * Create a configuration suitable for unit testing.
 *
 * @param[in] progname the program name (usually argv[0])
 * @param[in] filename the full file name where the bxilog_file_handler_p
 * must produce its output
 * @param[in] open_flags the flags to give to open() (man 2 open).
 *
 * @return a new bxilog configuration
 *
 * @see BXI_APPEND_OPEN_FLAGS
 * @see BXI_TRUNC_OPEN_FLAGS
 */
bxilog_config_p bxilog_unit_test_config(const char *progname,
                                        const char * filename,
                                        int open_flags);

#ifdef HAVE_LIBNETSNMP
/**
 * Create a configuration suitable for being used with the net-snmp logging library.
 *
 * @param[in] progname the program name (usually argv[0])
 *
 * @return a new bxilog configuration
 */
bxilog_config_p bxilog_netsnmp_config(const char * const progname);
#endif

/**
 * Create a new empty bxilog configuration.
 *
 * @param[in] progname the program name (usually argv[0])
 *
 * @return a new empty bxilog configuration.
 */
bxilog_config_p bxilog_config_new(const char * progname);

/**
 * Add the given handler to the given configuration
 *
 * @param[in] self a bxilog configuration
 * @param[in] handler a bxilog handler
 * @param[in] filters a set of bxilog_filter_p terminated by NULL
 *
 */
void bxilog_config_add_handler(bxilog_config_p self,
                               bxilog_handler_p handler,
                               bxilog_filters_p filters,
                               ...);


#endif /* BXILOG_H_ */
