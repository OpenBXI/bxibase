/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_FILTER_H_
#define BXILOG_FILTER_H_

#ifndef BXICFFI
#include <stdbool.h>
#endif

#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log.h"

/**
 * @file    filter.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  BXI Logging Filters.
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

/**
 * A filter.
 *
 * @see bxilog_cfg_registered()
 */
typedef struct bxilog_filter_s {
    const char * prefix;             //!< logger name prefix
    bxilog_level_e level;            //!< the level to set each matching logger to
} bxilog_filter_s;

/**
 * A logger configuration item.
 *
 * @see bxilog_cfg_registered()
 */
typedef bxilog_filter_s * bxilog_filter_p;

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************


extern const bxilog_filter_p BXILOG_FILTER_ALL_OUTPUT;
extern const bxilog_filter_p BXILOG_FILTER_ALL_LOWEST;

extern bxilog_filter_p BXILOG_FILTERS_ALL_OFF[];
extern bxilog_filter_p BXILOG_FILTERS_ALL_OUTPUT[];
extern bxilog_filter_p BXILOG_FILTERS_ALL_LOWEST[];

// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Parse the filters format and return the corresponding list of filters.
 *
 * The string must be in the following format:
 *      lvl_str     <-  [emergency, alert, critical, crit, error, err, warning, warn,
 *                       notice, output, info, debug, fine, trace, lowest]
 *      lvl_num     <-  [0-9]*
 *      lvl         <-  [lvl_num, level_str]
 *      prefix      <-  [A-z]+
 *      format      <-  (lvl:prefix,)*lvl:prefix
 *
 *
 * @param[in] format is a string containing the configuration for the loggers.
 * @param[out] n the number of filters found
 * @param[out] result the corresponding filters list (terminated by a NULL filter pointer)
 *
 * @return BXIERR_OK on success, anything else on error.
 *
 */
bxierr_p bxilog_filters_parse(char * format, size_t *n, bxilog_filter_p ** result);

#endif /* BXILOG_H_ */
