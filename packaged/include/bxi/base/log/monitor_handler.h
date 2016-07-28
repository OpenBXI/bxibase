/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Sébastien Miquée <sebastien.miquee@atos.net>
 # Created on: 2016/07/27
 # Contributors:
 ###############################################################################
 # Copyright (C) 2016  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */


#ifndef BXILOG_MONITOR_HANDLER_H_
#define BXILOG_MONITOR_HANDLER_H_

#include "bxi/base/err.h"
#include "bxi/base/log.h"

/**
 * @file    monitor_handler.h
 * @authors Sébastien Miquée <sebastien.miquee@atos.net>
 * @copyright 2016  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The Monitor Logging Handler
 *
 * The monitor handler sends logs through a ZMQ socket.
 */

//*********************************************************************************
//********************************  Defines  **************************************
//*********************************************************************************

//*********************************************************************************
//*********************************  Types  ***************************************
//*********************************************************************************

//*********************************************************************************
//****************************  Global Variables  *********************************
//*********************************************************************************


#ifndef BXICFFI
/**
 * The Monitor Handler.
 *
 * Parameters for the ::bxilog_handler_p.param_new() function are given below:
 *
 * @param[in] progname a `char *` string; the program name (argv[0])
 * @param[in] url a `char *` string; the url to bind to
 */

extern const bxilog_handler_p BXILOG_MONITOR_HANDLER;
#else
extern bxilog_handler_p BXILOG_MONITOR_HANDLER;
#endif


//*********************************************************************************
//********************************  Interfaces  ***********************************
//*********************************************************************************


#endif
