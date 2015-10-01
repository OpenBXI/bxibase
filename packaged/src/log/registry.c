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

static void _configure(bxilog_p logger);
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
static bxilog_p * REGISTERED_LOGGERS = NULL;
/**
 * Number of registered loggers.
 */
static size_t REGISTERED_LOGGERS_NB = 0;

/**
 * Number of items in the current configuration.
 */
static size_t CFG_ITEMS_NB = 0;

/**
 * Current configuration.
 */
static bxilog_cfg_item_s * CFG_ITEMS = NULL;

static pthread_mutex_t REGISTER_LOCK = PTHREAD_MUTEX_INITIALIZER;

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


void bxilog_register(bxilog_p logger) {
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

void bxilog_unregister(bxilog_p logger) {
    bxiassert(logger != NULL);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    bool found = false;
//    fprintf(stderr, "Nb Registered loggers: %zu\n", REGISTERED_LOGGERS_NB);
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        bxilog_p current = REGISTERED_LOGGERS[i];
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


bxierr_p bxilog_get(const char * logger_name, bxilog_p * result) {
    *result = NULL;

    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    if (0 != rc) return bxierr_errno("Call to pthread_mutex_lock() failed (rc=%d)", rc);

    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        bxilog_p logger = REGISTERED_LOGGERS[i];
        if (NULL == logger) continue;
        if (strncmp(logger->name, logger_name, logger->name_length) == 0) {
            *result = logger;
            break;
        }
    }

    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    if (0 != rc) return bxierr_errno("Call to pthread_mutex_unlock() failed (rc=%d)", rc);

    if (NULL == *result) { // Not found
        bxilog_p self = bximem_calloc(sizeof(*self));
        self->allocated = true;
        self->name = strdup(logger_name);
        self->name_length = strlen(logger_name) + 1;
        self->level = BXILOG_LOWEST;
        bxilog_register(self);
        *result = self;
    }
//    fprintf(stderr, "Returning %s %d\n", (*result)->name, (*result)->level);
    return BXIERR_OK;
}


size_t bxilog_get_registered(bxilog_p *loggers[]) {
    bxiassert(loggers != NULL);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    bxilog_p * result = bximem_calloc(REGISTERED_LOGGERS_NB * sizeof(*result));
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


void bxilog_reset_registry() {
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    _reset_config();
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    bxiassert(0 == rc);
}

bxierr_p bxilog_cfg_registered(size_t n, bxilog_cfg_item_s cfg[n]) {
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    _reset_config();
    CFG_ITEMS = bximem_calloc(n * sizeof(*CFG_ITEMS));
    for (size_t i = 0; i < n; i++) {
        CFG_ITEMS[i].level = cfg[i].level;
        CFG_ITEMS[i].prefix = strdup(cfg[i].prefix);
    }
    CFG_ITEMS_NB = n;

    // TODO: O(n*m) n=len(cfg) and m=len(loggers) -> be more efficient!
//    fprintf(stderr, "Number of cfg items: %zd\n", n);
    for (size_t l = 0; l < REGISTERED_LOGGERS_ARRAY_SIZE; l++) {
        bxilog_p logger = REGISTERED_LOGGERS[l];
        if (NULL != logger) _configure(logger);
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    bxiassert(0 == rc);
    return BXIERR_OK;
}

bxierr_p bxilog_cfg_parse(char * str) {
    bxiassert(NULL != str);

    str = strdup(str); // Required because str might not be allocated on the heap
    bxierr_p err = BXIERR_OK, err2 = BXIERR_OK;
    char *saveptr1 = NULL;
    char *str1 = str;
    size_t cfg_items_nb = 20;
    bxilog_cfg_item_s * cfg_items = bximem_calloc(cfg_items_nb * sizeof(*cfg_items));
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
        cfg_items[next_idx].prefix = strdup(prefix);
        cfg_items[next_idx].level = level;
        next_idx++;
        if (next_idx >= cfg_items_nb) {
            size_t new_size = cfg_items_nb * 2;
            cfg_items = bximem_realloc(cfg_items,
                                       cfg_items_nb,
                                       new_size);
            cfg_items_nb = new_size;
        }
    }

    err2 = bxilog_cfg_registered(next_idx, cfg_items);
    BXIERR_CHAIN(err, err2);
QUIT:
    for (size_t i = 0; i < next_idx; i++) {
        BXIFREE(cfg_items[i].prefix);
    }
    BXIFREE(cfg_items);
    BXIFREE(str);
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
                bxilog_destroy(&REGISTERED_LOGGERS[i]);
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
    for (size_t i = 0; i < CFG_ITEMS_NB; i++) {
        BXIFREE(CFG_ITEMS[i].prefix);
    }
    BXIFREE(CFG_ITEMS);
    CFG_ITEMS_NB = 0;
}

void _configure(bxilog_p logger) {
    bxiassert(NULL != logger);

    for (size_t i = 0; i < CFG_ITEMS_NB; i++) {
        if (NULL == CFG_ITEMS[i].prefix) continue;

        size_t len = strlen(CFG_ITEMS[i].prefix);
        if (0 == strncmp(CFG_ITEMS[i].prefix, logger->name, len)) {
            // We have a prefix, configure it
//            fprintf(stderr, "Prefix: '%s' match logger '%s': level: %d -> %d\n",
//                    CFG_ITEMS[i].prefix, logger->name,
//                    logger->level, CFG_ITEMS[i].level);
            logger->level = CFG_ITEMS[i].level;
        }
//        else {
//            fprintf(stderr, "Mismatch: %s %s\n", CFG_ITEMS[i].prefix, logger->name);
//        }
    }
}
