/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_REGISTRY_H_
#define BXILOG_REGISTRY_H_

#ifndef BXICFFI
#include <stdbool.h>
#endif

#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log.h"

/**
 * @file    registry.h
 * @author Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  BXI Logging Registry and filters.
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

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
 * Register the current logger.
 *
 * This is used by macro `SET_LOGGER()`. Use `SET_LOGGER()` instead.
 *
 * @param[in] logger the logger to register with.
 */
void bxilog_registry_add(bxilog_logger_p logger);

/**
 * Unregister the current logger.
 *
 * This is used by macro `SET_LOGGER()`. Use `SET_LOGGER()` instead.
 *
 * @param[in] logger the logger to unregister with.
 */
void bxilog_registry_del(bxilog_logger_p logger);

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
bxierr_p bxilog_registry_get(const char * logger_name, bxilog_logger_p * result);


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
size_t bxilog_registry_getall(bxilog_logger_p *loggers[]);

/**
 * Reset the bxilog registry.
 */
void bxilog_registry_reset();


#endif /* BXILOG_H_ */
