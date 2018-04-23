/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#ifndef BXILOG_HANDLER_IMPL_H
#define BXILOG_HANDLER_IMPL_H

#include "bxi/base/err.h"
#include "bxi/base/log.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************
#define READY_CTRL_MSG_REQ "BC->H: ready?"
#define READY_CTRL_MSG_REP "H->BC: ready!"
#define FLUSH_CTRL_MSG_REQ "BC->H: flush?"
#define FLUSH_CTRL_MSG_REP "H->BC: flushed!"
#define EXIT_CTRL_MSG_REQ "BC->H: exit?"
#define EXIT_CTRL_MSG_REP "H->BC: exited!"


//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

typedef struct {
    bxilog_handler_p handler;
    bxilog_handler_param_p param;
} bxilog__handler_thread_bundle_s;

typedef bxilog__handler_thread_bundle_s * bxilog__handler_thread_bundle_p;
//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

//*********************************************************************************
//********************************** Interfaces        ****************************
//*********************************************************************************

// Can be used directly with pthread_create()
bxierr_p bxilog__handler_start(bxilog__handler_thread_bundle_p bundle);

#endif
