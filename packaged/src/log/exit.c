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


#include <sysexits.h>

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
                 bxilog_logger_p logger,
                 bxilog_level_e level,
                 char * file,
                 size_t filelen,
                 const char * func,
                 size_t funclen,
                 int line) {

    bxiassert(NULL != logger);
    char * str = bxierr_str(err);
    if (INITIALIZED != BXILOG__GLOBALS->state) {
        char * msg = bxistr_new("Exiting with code %d, error is %s\n"
                                "(Since the BXI logging library is not "
                                "initialized: the above message is raw displayed "
                                "on stderr. Especially, it won't appear in the "
                                "expected logging file.)\n", exit_code, str);
        bxilog_rawprint(msg, STDERR_FILENO);
        BXIFREE(msg);
    } else {
        bxilog_logger_log(logger, level,
                          file, filelen,
                          func, funclen,
                          line,
                          "Exiting with code %d, error is: %s", exit_code, str);
    }
    BXIFREE(str);
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

