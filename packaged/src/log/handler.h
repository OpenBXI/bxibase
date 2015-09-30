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

#ifndef BXILOG_HANDLER_H
#define BXILOG_HANDLER_H

#include "bxi/base/err.h"
#include "bxi/base/log.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

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
