/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_FILTER_H_
#define BXILOG_FILTER_H_

#ifndef BXICFFI
#include <stdbool.h>
#endif

#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log/level.h"

/**
 * @brief BXI Logging Filters.
 * @file    filter.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 *
 *
 * Filters are used to remove logging messages at various steps in the logging process.
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************


/**
 * The filter structure.
 */
typedef struct bxilog_filter_s bxilog_filter_s;

/**
 * A filter instance.
 */
typedef bxilog_filter_s * bxilog_filter_p;

/**
 * The filters structure.
 */
typedef struct bxilog_filters_s bxilog_filters_s;

/**
 * A Filters instance.
 */
typedef bxilog_filters_s * bxilog_filters_p;


/**
 * A filter.
 *
 * @see bxilog_cfg_registered()
 */
struct bxilog_filter_s {
    const char * prefix;             //!< logger name prefix
    bxilog_level_e level;            //!< the level to set each matching logger to
    void * reserved;
};

/**
 * A set of filters.
 */
struct bxilog_filters_s {
    bool allocated;                 //!< If true, means it must be deallocated
    size_t nb;                      //!< Number of filters in the list
    size_t allocated_slots;         //!< Number of allocated slots in the list
    bxilog_filter_p list[];         //!< The list of filter.
};



// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

/**
 * Filters all messages to level ::BXILOG_OFF
 */
extern bxilog_filters_p BXILOG_FILTERS_ALL_OFF;

/**
 * Filters all messages to level ::BXILOG_OUTPUT
 */
extern bxilog_filters_p BXILOG_FILTERS_ALL_OUTPUT;


/**
 * Filters all messages to level ::BXILOG_ALL
 */
extern bxilog_filters_p BXILOG_FILTERS_ALL_ALL;

// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Create a new empty set of filters.
 *
 * @return an array of filter.
 */
bxilog_filters_p  bxilog_filters_new();

/**
 * Free a set of filters. Use bxilog_filters_destroy() instead.
 *
 * @param[inout] filters a pointer on filters
 */
void bxilog_filters_free(bxilog_filters_p filters);

/**
 * Destroy a set of filters.
 *
 * @param[inout] filters_p a pointer on filters
 */
void bxilog_filters_destroy(bxilog_filters_p * filters_p);

/**
 * Duplicate the given set of filters.
 *
 * @param[in] filters a set of filters
 * @return a mallocated set of filters that contains a copy of all elements
 *         found in filters.
 */
bxilog_filters_p bxilog_filters_dup(bxilog_filters_p filters);
/**
 * Add a new filter to the given set of filters.
 *
 * @param[inout] filters the filters to add new filter to
 * @param[in] prefix the prefix to use
 * @param[in] level the level if a match is found
 *
 *
 *
 */
void bxilog_filters_add(bxilog_filters_p * filters,
                        const char * prefix, bxilog_level_e level);


/**
 * Parse the filters format and return the corresponding list of filters.
 *
 * The string must be in the following format:
 *      - lvl_str     <-  [emergency, alert, critical, crit, error, err, warning, warn,
 *                         notice, perf, output, info, debug, fine, trace, lowest]
 *      - lvl_num     <-  [0-9]*
 *      - lvl         <-  [lvl_num, level_str]
 *      - prefix      <-  [A-z]+
 *      - format      <-  (lvl:prefix,)*lvl:prefix
 *
 *
 * @param[in] format is a string containing the configuration for the loggers.
 * @param[out] result the corresponding filters
 *
 * @return BXIERR_OK on success, anything else on error.
 *
 */
bxierr_p bxilog_filters_parse(char * format, bxilog_filters_p * result);

#endif /* BXILOG_H_ */
