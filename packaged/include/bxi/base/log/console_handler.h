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

#ifndef BXILOG_CONSOLE_HANDLER_H_
#define BXILOG_CONSOLE_HANDLER_H_

#include "bxi/base/err.h"
#include "bxi/base/log.h"

/**
 * @file    console_handler.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The Console Logging Handler
 *
 * The console handler produces logs to the console, that is stdout and stderr
 * according to the level of a given log.
 */
//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************
#ifndef BXICFFI
/**
 * The Console Handler.
 *
 * Parameters for the ::bxilog_handler_p.param_new() function are given below:
 *
 * @param[in] level a ::bxilog_level_e value; when a log is processed, if the log level
 *                  is less or equals to this level, the log is emitted on the standard
 *                  error, otherwise it is emitted on standard output.
 */
extern const bxilog_handler_p BXILOG_CONSOLE_HANDLER;
#else
extern bxilog_handler_p BXILOG_CONSOLE_HANDLER;
#endif

//*********************************************************************************
//********************************** Interfaces        ****************************
//*********************************************************************************


#endif




