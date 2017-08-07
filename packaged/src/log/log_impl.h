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


#ifndef BXILOG_LOG_IMPL_H
#define BXILOG_LOG_IMPL_H

#include <pthread.h>

#include "bxi/base/log.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************
/*
 * Finite State Machine.
 *
 * Normal path:
 * UNSET --> _init() -> INITIALIZING -> INITIALIZED - _finalize() -> FINALIZING -> FINALIZED
 * FINALIZED -> _finalize() -> FINALIZED
 * Support for fork() makes special transitions:
 * (UNSET, FINALIZED) --> fork() --> (UNSET, FINALIZED) // No change
 * (INITIALIZING, FINALIZING) --> fork() --> ILLEGAL    // Can't fork while initializing
 *                                                      // or finalizing
 * INITIALIZED --> fork()
 * Parent: _parent_before_fork() --> FINALIZING --> FINALIZED --> FORKED
 *         _parent_after_fork(): FORKED --> _init() --> INITIALIZING --> INITIALIZED
 * Child:  _child_after_fork(): FORKED --> FINALIZED
 */
typedef enum {
    UNSET, INITIALIZING, BROKEN, INITIALIZED, FINALIZING, FINALIZED, ILLEGAL, FORKED,
} bxilog_state_e;


//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


typedef struct {
    bxilog_config_p config;

    void * zmq_ctx;
    pid_t pid;
    const size_t RECORD_MINIMUM_SIZE;

    bxilog_state_e state;

    /* Key for the thread-specific data (tsd) */
    pthread_key_t tsd_key;
    /* Once-only initialisation of the tsd */
    pthread_once_t tsd_key_once;

    size_t internal_handlers_nb;
    pthread_t *handlers_threads;
} bxilog__core_globals_s;

typedef bxilog__core_globals_s * bxilog__core_globals_p;

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

extern bxilog__core_globals_p BXILOG__GLOBALS;

//*********************************************************************************
//********************************** Interface         ****************************
//*********************************************************************************
bxierr_p bxilog__init_globals();
void bxilog__wipeout();
bxierr_p bxilog__finalize(void);
bxierr_p bxilog__start_handlers(void);
bxierr_p bxilog__stop_handlers(void);
#endif
