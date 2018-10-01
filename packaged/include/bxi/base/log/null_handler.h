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

#ifndef BXILOG_NULL_HANDLER_H_
#define BXILOG_NULL_HANDLER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "bxi/base/err.h"
#include "bxi/base/log.h"


/**
 * @file    null_handler.h
 * @author Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The NULL Logging Handler
 *
 * The null handler does not produce any logs. It is mainly used for testing purpose.
 * It does not take any parameter.
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
extern const bxilog_handler_p BXILOG_NULL_HANDLER;
#else
extern bxilog_handler_p BXILOG_NULL_HANDLER;
#endif

//*********************************************************************************
//********************************** Interfaces        ****************************
//*********************************************************************************


#ifdef __cplusplus
}
#endif
#endif
