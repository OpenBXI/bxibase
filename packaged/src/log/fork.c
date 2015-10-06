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

#include <pthread.h>
#include <errno.h>
#include <error.h>
#include <sysexits.h>

#include "bxi/base/str.h"
#include "bxi/base/log.h"

#include "log_impl.h"
#include "fork_impl.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
static void _parent_before_fork(void);
static void _parent_after_fork(void);
static void _child_after_fork(void);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The internal logger
SET_LOGGER(LOGGER, "bxi.base.log.fork");


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************

void bxilog__fork_install_handlers(void) {
    // Install fork handler
    errno = 0;
    int rc = pthread_atfork(_parent_before_fork, _parent_after_fork, _child_after_fork);
    bxiassert(0 == rc);
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************


void _parent_before_fork(void) {
    // WARNING: If you change the FSM transition,
    // comment you changes in bxilog_state_e above.
    if (INITIALIZING == BXILOG__GLOBALS->state || FINALIZING == BXILOG__GLOBALS->state) {
        error(EX_SOFTWARE, 0, "Forking while bxilog is in state %d! Aborting", BXILOG__GLOBALS->state);
    }
    if(INITIALIZED != BXILOG__GLOBALS->state) return;
    FINE(LOGGER, "Preparing for a fork() (state == %d)", BXILOG__GLOBALS->state);
    bxierr_p err = BXIERR_OK, err2;
    err2 = bxilog_flush();
    BXIERR_CHAIN(err, err2);
    err2 = bxilog__finalize();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        char * err_str = bxierr_str(err);
        char * msg = bxistr_new("Error while preparing for a fork(), "
                                "some logs might have been missed. "
                                "Detailed error is - %s",
                                err_str);
        bxilog_rawprint(msg, STDERR_FILENO);
        BXIFREE(msg);
        BXIFREE(err_str);
        bxierr_destroy(&err);
    }
    if (FINALIZING != BXILOG__GLOBALS->state) {
        error(EX_SOFTWARE, 0,
              "Forking should lead bxilog to reach state %d (current state is %d)!",
              FINALIZING, BXILOG__GLOBALS->state);
    }
    BXILOG__GLOBALS->state = FORKED;
}

void _parent_after_fork(void) {
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (FORKED != BXILOG__GLOBALS->state) return;

    BXILOG__GLOBALS->state = INITIALIZING;
    bxilog__init_globals();

    bxierr_p err = bxilog__start_handlers();
    // Can't do a log
    bxiassert(bxierr_isok(err));

    if (INITIALIZING != BXILOG__GLOBALS->state) {
        error(EX_SOFTWARE, 0,
              "Forking should leads bxilog to reach state %d (current state is %d)!",
              INITIALIZING, BXILOG__GLOBALS->state);
    }
    BXILOG__GLOBALS->state = INITIALIZED;
    // Sending a log required the log library to be initialized.
    FINE(LOGGER, "Ready after a fork()");
}

void _child_after_fork(void) {
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (FORKED != BXILOG__GLOBALS->state) return;
    BXILOG__GLOBALS->state = FINALIZED;
    // The child remain in the finalized state
    // It has to call bxilog_init() if it wants to do some log.
}

