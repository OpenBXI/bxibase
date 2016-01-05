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

const bxilog_filter_s BXILOG_FILTER_ALL_ALL_S = {.prefix = "", .level = BXILOG_ALL};
const bxilog_filter_s BXILOG_FILTER_ALL_OUTPUT_S = {.prefix = "", .level = BXILOG_OUTPUT};

const bxilog_filter_s * BXILOG_FILTER_ALL_LOWEST = &BXILOG_FILTER_ALL_ALL_S;
const bxilog_filter_s * BXILOG_FILTER_ALL_OUTPUT = &BXILOG_FILTER_ALL_OUTPUT_S;

const bxilog_filters_s BXILOG_FILTERS_ALL_OFF_S = {.allocated=false,
                                                   .nb=0,
                                                   .allocated_slots=0,
                                                   .list = {NULL},
                                                  };
const bxilog_filters_s BXILOG_FILTERS_ALL_OUTPUT_S = {.allocated=false,
                                                      .nb=1,
                                                      .allocated_slots=1,
                                                      .list = {(bxilog_filter_p)
                                                               &BXILOG_FILTER_ALL_OUTPUT_S},
                                                     };
const bxilog_filters_s BXILOG_FILTERS_ALL_ALL_S = { .allocated=false,
                                                    .nb = 1,
                                                    .allocated_slots = 1,
                                                    .list = {(bxilog_filter_p)
                                                             &BXILOG_FILTER_ALL_ALL_S},
                                                  };

bxilog_filters_p BXILOG_FILTERS_ALL_OFF = (bxilog_filters_p) &BXILOG_FILTERS_ALL_OFF_S;
bxilog_filters_p BXILOG_FILTERS_ALL_OUTPUT = (bxilog_filters_p) &BXILOG_FILTERS_ALL_OUTPUT_S;
bxilog_filters_p BXILOG_FILTERS_ALL_ALL = (bxilog_filters_p) &BXILOG_FILTERS_ALL_ALL_S;

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


bxilog_filters_p  bxilog_filters_new() {
    size_t slots = 2;
    bxilog_filters_p filters = bximem_calloc(sizeof(*filters) +\
                                             slots*sizeof(filters->list[0]));
    filters->allocated = true;
    filters->nb = 0;
    filters->allocated_slots = slots;

    return filters;
}

void bxilog_filters_destroy(bxilog_filters_p * filters_p) {
    bxiassert(NULL != filters_p);
    bxilog_filters_p filters = *filters_p;
    if (NULL == filters) return;
    if (!filters->allocated) {
        *filters_p = NULL;
        return;
    }

    for (size_t i = 0; i < filters->nb; i++) {
        BXIFREE(filters->list[i]->prefix);
        BXIFREE(filters->list[i]);
    }
    bximem_destroy((char**)filters_p);
}

void bxilog_filters_add(bxilog_filters_p * filters_p,
                        char * prefix, bxilog_level_e level) {

    bxiassert(NULL != filters_p);
    bxiassert(NULL != *filters_p);
    bxiassert(NULL != prefix);

    bxilog_filter_p filter = bximem_calloc(sizeof(*filter));
    filter->prefix = strdup(prefix);
    filter->level = level;

    bxilog_filters_p filters = *filters_p;
    filters->list[filters->nb] = filter;
    filters->nb++;

    if (filters->nb >= filters->allocated_slots) {
        size_t old = filters->allocated_slots;
        size_t new = old * 2;
        filters = bximem_realloc(filters,
                                 sizeof(*filters) + old*sizeof(filters->list[0]),
                                 sizeof(*filters) + new*sizeof(filters->list[0]));
        filters->allocated_slots = new;
        *filters_p = filters;
    }
}


bxierr_p bxilog_filters_parse(char * str, bxilog_filters_p * result) {
    bxiassert(NULL != str);
    bxiassert(NULL != result);

    str = strdup(str); // Required because str might not be allocated on the heap
    bxierr_p err = BXIERR_OK, err2 = BXIERR_OK;

    *result = NULL;
    char *saveptr1 = NULL;
    char *str1 = str;

    bxilog_filters_p filters = bxilog_filters_new();

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
        if (0 != errno) {
            err2 = bxierr_errno("Error while parsing number: '%s'",
                                level_str);
            BXIERR_CHAIN(err, err2);
            goto QUIT;
        }
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
        bxilog_filters_add(&filters, prefix, level);
    }

    *result = filters;

QUIT:
    BXIFREE(str);
    if (bxierr_isko(err)) bxilog_filters_destroy(&filters);
    return err;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

