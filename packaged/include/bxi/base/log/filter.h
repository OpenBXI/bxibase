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



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************


#endif /* BXILOG_H_ */
