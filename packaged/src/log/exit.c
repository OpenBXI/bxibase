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


#include <error.h>
#include <sysexits.h>

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

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


void bxilog_exit(int exit_code,
                 bxierr_p err,
                 bxilog_p logger,
                 bxilog_level_e level,
                 char * file,
                 size_t filelen,
                 const char * func,
                 size_t funclen,
                 int line) {

    bxiassert(NULL != logger);
    char * str = bxierr_str_limit(err, BXIERR_ALL_CAUSES);
    bxilog_log(logger, level,
               file, filelen,
               func, funclen,
               line,
               "Exiting with code %d, Error is: %s", exit_code, str);
    bxierr_destroy(&err);
    err = bxitime_sleep(CLOCK_MONOTONIC, 0, 50000000);
    bxierr_destroy(&err);
    err = bxilog_flush();
    bxierr_destroy(&err);
    exit((exit_code));
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

