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

#include <string.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************


//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

const bxilog_filter_s BXILOG_FILTER_ALL_LOWEST_S = {.prefix = "", .level = BXILOG_LOWEST};
const bxilog_filter_p BXILOG_FILTER_ALL_LOWEST = (const bxilog_filter_p) &BXILOG_FILTER_ALL_LOWEST_S;
const bxilog_filter_s BXILOG_FILTER_ALL_OUTPUT_S = {.prefix = "", .level = BXILOG_OUTPUT};
const bxilog_filter_p BXILOG_FILTER_ALL_OUTPUT = (const bxilog_filter_p) &BXILOG_FILTER_ALL_OUTPUT_S;

bxilog_filter_p BXILOG_FILTERS_ALL_OFF[] = {NULL};
bxilog_filter_p BXILOG_FILTERS_ALL_OUTPUT[] = {(bxilog_filter_p) &BXILOG_FILTER_ALL_OUTPUT_S, NULL};
bxilog_filter_p BXILOG_FILTERS_ALL_LOWEST[] = {(bxilog_filter_p) &BXILOG_FILTER_ALL_LOWEST_S, NULL};


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

bxierr_p bxilog_filters_parse(char * str, size_t * n, bxilog_filter_p ** result) {
    bxiassert(NULL != str && NULL != n && NULL != result);

    str = strdup(str); // Required because str might not be allocated on the heap
    bxierr_p err = BXIERR_OK, err2 = BXIERR_OK;

    *n = 0;
    *result = NULL;
    char *saveptr1 = NULL;
    char *str1 = str;

    size_t filters_nb = 20;
    bxilog_filter_p * filters = bximem_calloc(filters_nb * sizeof(*filters));

    size_t next_idx = 0;

    for (size_t j = 0; ; j++, str1 = NULL) {
        char * token = strtok_r(str1, ",", &saveptr1);
        if (NULL == token) break;
        char * sep = strchr(token, ':');
        if (sep == NULL) {
            err2 = bxierr_gen("Expected ':' in log level configuration: %s", token);
            BXIERR_CHAIN(err, err2);
            goto QUIT;
        }

        *sep = '\0';
        char * prefix = token;
        char * level_str = sep + 1;
        bxilog_level_e level;
        unsigned long tmp;
        errno = 0;

        char * endptr;
        tmp = strtoul(level_str, &endptr, 10);
        if (0 != errno) return bxierr_errno("Error while parsing number: '%s'", level_str);
        if (endptr == level_str) {
            err2 = bxilog_get_level_from_str(level_str, &level);
            BXIERR_CHAIN(err, err2);
            if (bxierr_isko(err)) {
                goto QUIT;
            }
        } else if (*endptr != '\0') {
            err = bxierr_errno("Error while parsing number: '%s' doesn't contain only "
                               "a number or a level", level_str);
            BXIERR_CHAIN(err, err2);
            goto QUIT;
        } else {
            if (tmp > BXILOG_LOWEST) {
                err2 = bxierr_new(BXILOG_BADLVL_ERR,
                                  (void*) tmp, NULL, NULL, NULL,
                                  "Bad log level: %lu, using %d instead",
                                  tmp, BXILOG_LOWEST);
                BXIERR_CHAIN(err, err2);
                tmp = BXILOG_LOWEST;
            }
            level = (bxilog_level_e) tmp;
        }
        filters[next_idx] = bximem_calloc(sizeof(*filters[next_idx]));
        filters[next_idx]->prefix = strdup(prefix);
        filters[next_idx]->level = level;
        next_idx++;
        if (next_idx >= filters_nb) {
            size_t new_size = filters_nb * 2;
            filters = bximem_realloc(filters, filters_nb, new_size);
            filters_nb = new_size;
        }
    }

    *n = next_idx;
    *result = filters;

QUIT:
    BXIFREE(str);
    return err;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

