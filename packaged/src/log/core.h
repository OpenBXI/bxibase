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


#ifndef BXILOG_CORE_H
#define BXILOG_CORE_H

#include "bxi/base/log.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************
#define DEFAULT_LOG_BUF_SIZE 128   // We use a 128 bytes per thread log buffer
                                   // by default expecting it is sufficient
                                   // for the vast majority of cases
                                   // If this is not the case, a specific
                                   // mallocated buffer will be used instead
#define DEFAULT_POLL_TIMEOUT 500   // Awake after this amount of milliseconds
                                   // if nothing has been received.

#define MAX_DEPTH_ERR 5            // Maximum number of chained errors before quiting

#define IH_RCVHWM 1500000

// How many retries before falling back to SYNC sending.
#define RETRIES_MAX 3u
// Used when a sending failed with EAGAIN, we sleep for that amount of time (in ns).
#define RETRY_DELAY 500000l

#define IHT_LOGGER_NAME "bxilog.iht"

#define READY_CTRL_MSG_REQ "BC->H: ready?"
#define READY_CTRL_MSG_REP "H->BC: ready!"
#define FLUSH_CTRL_MSG_REQ "BC->H: flush?"
#define FLUSH_CTRL_MSG_REP "H->BC: flushed!"
#define EXIT_CTRL_MSG_REQ "BC->H: exit?"
#define EXIT_CTRL_MSG_REP "H->BC: exited!"




//*********************************************************************************
//********************************** Types ****************************************
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

typedef struct {
    bxilog_param_p param;

    void * zmq_ctx;
    pid_t pid;
    const size_t RECORD_MINIMUM_SIZE;

    bxilog_state_e state;

    /* Key for the thread-specific data (tsd) */
    pthread_key_t tsd_key;
    /* Once-only initialisation of the tsd */
    pthread_once_t tsd_key_once;

    size_t internal_handlers_nb;
    pthread_t *internal_handlers;
} bxilog__core_globals_s;

typedef bxilog__core_globals_s * bxilog__core_globals_p;

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

extern bxilog__core_globals_p BXILOG__GLOBALS;

//*********************************************************************************
//********************************** Interface         ****************************
//*********************************************************************************
bxierr_p bxilog__core_init_globals();
void bxilog__core_display_err_msg(char* msg);
void bxilog__core_wipeout();
bxierr_p bxilog__core_finalize(void);
bxierr_p bxilog__core_start_handlers(void);
bxierr_p bxilog__core_stop_handlers(void);
#endif
