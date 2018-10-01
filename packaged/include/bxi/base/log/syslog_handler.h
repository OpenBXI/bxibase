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

#ifndef BXILOG_SYSLOG_HANDLER_H_
#define BXILOG_SYSLOG_HANDLER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "bxi/base/log.h"

/**
 * @file    syslog_handler.h
 * @author Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The Syslog Logging Handler
 *
 * The syslog handler writes logs to syslog (see: man 3 syslog).
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
 * The Syslog Handler.
 *
 * Parameters for the ::bxilog_handler_p.param_new() function are given below:
 *
 * @param[in] ident a `char *` string; the identity
 * @param[in] option an `int`; options of openlog()
 * @param[in] facility an `int`; default facility
 *
 * @note See syslog(3) (e.g. man 3 syslog) for details on those parameters
 */
extern const bxilog_handler_p BXILOG_SYSLOG_HANDLER;
#else
extern bxilog_handler_p BXILOG_SYSLOG_HANDLER;
#endif

//*********************************************************************************
//********************************** Interfaces        ****************************
//*********************************************************************************


#ifdef __cplusplus
}
#endif
#endif
