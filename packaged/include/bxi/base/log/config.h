/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_CONFIG_H_
#define BXILOG_CONFIG_H_

#ifndef BXICFFI
#include <stdbool.h>
#endif

#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log.h"
#include "bxi/base/log/handler.h"

/**
 * @file    config.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  Configuration of the High Performance Logging Module
 *
 * A bxilog configuration specifies various logging parameters, in particular
 * the set of logging handlers that must be used and with which parameters.
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

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
    const char * progname;                      //!< Program name
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
 *
 * @param[in] progname the program name (usually argv[0])
 * @param[in] filename if not NULL, the full file name where the bxilog_file_handler_p
 *            must produce its output
 * @param[in] append if true, append to filename if it exists (ignored when filename
 *            is NULL).
 *
 * @return a new bxilog "standard" configuration
 *
 */
bxilog_config_p bxilog_basic_config(const char *progname,
                                    const char * filename,
                                    bool append);

/**
 * Create a configuration suitable for unit testing.
 *
 * @param[in] progname the program name (usually argv[0])
 * @param[in] filename the full file name where the bxilog_file_handler_p
 * must produce its output
 * @param[in] append if true, append to filename if it exists
 *
 * @return a new bxilog configuration
 */
bxilog_config_p bxilog_unit_test_config(const char *progname,
                                        const char * filename,
                                        bool append);

/**
 * Create a configuration suitable for being used with the net-snmp logging library.
 *
 * @param[in] progname the program name (usually argv[0])
 *
 * @return a new bxilog configuration
 */
bxilog_config_p bxilog_netsnmp_config(const char * const progname);

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
 *
 */
void bxilog_config_add_handler(bxilog_config_p self, bxilog_handler_p handler, ...);


#endif /* BXILOG_H_ */
