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

#ifndef BXILOG_CONFIG_IMPL_H
#define BXILOG_CONFIG_IMPL_H

#include <bxi/base/err.h>
#include <bxi/base/log.h>

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

//*********************************************************************************
//********************************** Interface         ****************************
//*********************************************************************************

bxierr_p bxilog__config_destroy(bxilog_config_p * config_p);
bxierr_p bxilog__config_loggers();

#endif
