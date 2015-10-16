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


#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

#include "log_impl.h"
#include "registry_impl.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************
#define REGISTERED_LOGGERS_DEFAULT_ARRAY_SIZE 64

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************



//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************

static void _configure(bxilog_logger_p logger);
static void _reset_config();

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

/**
 * Initial number of registered loggers array size
 */
static size_t REGISTERED_LOGGERS_ARRAY_SIZE = REGISTERED_LOGGERS_DEFAULT_ARRAY_SIZE;
/**
 * The array of registered loggers.
 */
static bxilog_logger_p * REGISTERED_LOGGERS = NULL;
/**
 * Number of registered loggers.
 */
static size_t REGISTERED_LOGGERS_NB = 0;

/**
 * Number of items in the current configuration.
 */
static size_t FILTERS_NB = 0;

/**
 * Current configuration.
 */
static bxilog_filter_s * FILTERS = NULL;

static pthread_mutex_t REGISTER_LOCK = PTHREAD_MUTEX_INITIALIZER;

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


void bxilog_registry_add(bxilog_logger_p logger) {
    bxiassert(logger != NULL);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);

    if (REGISTERED_LOGGERS == NULL) {
        size_t bytes = REGISTERED_LOGGERS_ARRAY_SIZE * sizeof(*REGISTERED_LOGGERS);
        REGISTERED_LOGGERS = bximem_calloc(bytes);
        int rc = atexit(bxilog__wipeout);
        bxiassert(0 == rc);

    } else if (REGISTERED_LOGGERS_NB >= REGISTERED_LOGGERS_ARRAY_SIZE) {
        size_t old_size = REGISTERED_LOGGERS_ARRAY_SIZE;
        REGISTERED_LOGGERS_ARRAY_SIZE *= 2;
        size_t bytes = REGISTERED_LOGGERS_ARRAY_SIZE * sizeof(*REGISTERED_LOGGERS);
        REGISTERED_LOGGERS = bximem_realloc(REGISTERED_LOGGERS,
                                            old_size * sizeof(*REGISTERED_LOGGERS),
                                            bytes);

//        fprintf(stderr, "[I] Reallocation of %zu slots for (currently) "
//                "%zu registered loggers\n", REGISTERED_LOGGERS_ARRAY_SIZE,
//                REGISTERED_LOGGERS_NB);
    }

    size_t slot = REGISTERED_LOGGERS_NB;
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        // Find next free slot
        if (NULL == REGISTERED_LOGGERS[i]) {
            if (slot == REGISTERED_LOGGERS_NB) slot = i;

        } else if (0 == strncmp(REGISTERED_LOGGERS[i]->name,
                                logger->name,
                                logger->name_length)) {
            // TODO: provide something better here!
            fprintf(stderr,
                    "[W] Logger name '%s' already registered at position %zu, "
                    "this can lead to various problems such as wrong logging level "
                    "configuration or misleading messages!\n",
                    logger->name, i);
        }
    }
//    fprintf(stderr, "Registering new logger[%zu]: %s\n", slot, logger->name);
    REGISTERED_LOGGERS[slot] = logger;
    REGISTERED_LOGGERS_NB++;
    _configure(logger);
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    bxiassert(0 == rc);
}

void bxilog_registry_del(bxilog_logger_p logger) {
    bxiassert(logger != NULL);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    bool found = false;
//    fprintf(stderr, "Nb Registered loggers: %zu\n", REGISTERED_LOGGERS_NB);
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        bxilog_logger_p current = REGISTERED_LOGGERS[i];
        if (NULL == current) continue;
//        fprintf(stderr, "Logger[%zu]: %s\n", i, current->name);
        if (current == logger) {
//            fprintf(stderr, "Unregistering loggers[%zu]: %s\n", i, current->name);
            REGISTERED_LOGGERS[i] = NULL;
            found = true;
        }
    }
    if (!found) {
//        bxierr_p bxierr = bxierr_gen("[W] Can't find registered logger: %s\n",
//                                     logger->name);
//        char * str = NULL;
//        if (err->str != NULL) {
//            err_str = err->str(err);
//        } else {
//            err_str = bxierr_str(err);
//        }
//        _display_err_msg(str);
//        BXIFREE(str);
    } else REGISTERED_LOGGERS_NB--;

    if (0 == REGISTERED_LOGGERS_NB) {
        BXIFREE(REGISTERED_LOGGERS);
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    bxiassert(0 == rc);
}


bxierr_p bxilog_registry_get(const char * logger_name, bxilog_logger_p * result) {
    *result = NULL;

    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    if (0 != rc) return bxierr_errno("Call to pthread_mutex_lock() failed (rc=%d)", rc);

    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        bxilog_logger_p logger = REGISTERED_LOGGERS[i];
        if (NULL == logger) continue;
        if (strncmp(logger->name, logger_name, logger->name_length) == 0) {
            *result = logger;
            break;
        }
    }

    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    if (0 != rc) return bxierr_errno("Call to pthread_mutex_unlock() failed (rc=%d)", rc);

    if (NULL == *result) { // Not found
        bxilog_logger_p self = bximem_calloc(sizeof(*self));
        self->allocated = true;
        self->name = strdup(logger_name);
        self->name_length = strlen(logger_name) + 1;
        self->level = BXILOG_LOWEST;
        bxilog_registry_add(self);
        *result = self;
    }
//    fprintf(stderr, "Returning %s %d\n", (*result)->name, (*result)->level);
    return BXIERR_OK;
}


size_t bxilog_registry_getall(bxilog_logger_p *loggers[]) {
    bxiassert(NULL != loggers);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    bxilog_logger_p * result = bximem_calloc(REGISTERED_LOGGERS_NB * sizeof(*result));
    size_t j = 0;
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        if (NULL == REGISTERED_LOGGERS[i]) continue;
        result[j++] = REGISTERED_LOGGERS[i];
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    *loggers = result;
    return j;
}


void bxilog_registry_reset() {
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    _reset_config();
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    bxiassert(0 == rc);
}

bxierr_p bxilog_registry_set_filters(size_t n, bxilog_filter_p filters[n]) {
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    _reset_config();
    FILTERS = bximem_calloc(n * sizeof(*FILTERS));
    for (size_t i = 0; i < n; i++) {
        FILTERS[i].level = filters[i]->level;
        FILTERS[i].prefix = strdup(filters[i]->prefix);
    }
    FILTERS_NB = n;

    // TODO: O(n*m) n=len(cfg) and m=len(loggers) -> be more efficient!
//    fprintf(stderr, "Number of cfg items: %zd\n", n);
    for (size_t l = 0; l < REGISTERED_LOGGERS_ARRAY_SIZE; l++) {
        bxilog_logger_p logger = REGISTERED_LOGGERS[l];
        if (NULL != logger) _configure(logger);
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    return BXIERR_OK;
}

bxierr_p bxilog_registry_parse_set_filters(char * str) {
    bxiassert(NULL != str);
    bxierr_p err = BXIERR_OK, err2;

    bxilog_filter_p * filters = NULL;
    size_t n = 0;
    err2 = bxilog_filters_parse(str, &n, &filters);
    BXIERR_CHAIN(err, err2);

    err2 = bxilog_registry_set_filters(n, filters);
    BXIERR_CHAIN(err, err2);

    BXIFREE(filters);

    return err;
}



void bxilog__cfg_release_loggers() {
    if (NULL != REGISTERED_LOGGERS) {
        //        fprintf(stderr, "Nb Registered loggers: %zu\n", REGISTERED_LOGGERS_NB);
        for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
            if (NULL == REGISTERED_LOGGERS[i]) {
                continue;
            }
            //            fprintf(stderr, "loggers[%zu]: %s\n", i, REGISTERED_LOGGERS[i]->name);
            if (REGISTERED_LOGGERS[i]->allocated) {
                //                fprintf(stderr, "[I] Destroying %s\n", REGISTERED_LOGGERS[i]->name);
                bxilog_logger_destroy(&REGISTERED_LOGGERS[i]);
            }
            REGISTERED_LOGGERS[i] = NULL;
            REGISTERED_LOGGERS_NB--;
        }
        bxiassert(0 == REGISTERED_LOGGERS_NB);
        //        fprintf(stderr, "[I] Removing registered loggers\n");
        BXIFREE(REGISTERED_LOGGERS);
        REGISTERED_LOGGERS_ARRAY_SIZE = 0;
    }
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

void _reset_config() {
    for (size_t i = 0; i < FILTERS_NB; i++) {
        BXIFREE(FILTERS[i].prefix);
    }
    BXIFREE(FILTERS);
    FILTERS_NB = 0;
}

void _configure(bxilog_logger_p logger) {
    bxiassert(NULL != logger);

    for (size_t i = 0; i < FILTERS_NB; i++) {
        if (NULL == FILTERS[i].prefix) continue;

        size_t len = strlen(FILTERS[i].prefix);
        if (0 == strncmp(FILTERS[i].prefix, logger->name, len)) {
            // We have a prefix, configure it
//            fprintf(stderr, "Prefix: '%s' match logger '%s': level: %d -> %d\n",
//                    CFG_ITEMS[i].prefix, logger->name,
//                    logger->level, CFG_ITEMS[i].level);
            logger->level = FILTERS[i].level;
        }
//        else {
//            fprintf(stderr, "Mismatch: %s %s\n", CFG_ITEMS[i].prefix, logger->name);
//        }
    }
}
