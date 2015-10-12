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

#ifndef BXILOG_SNMPLOG_HANDLER_H_
#define BXILOG_SNMPLOG_HANDLER_H_

#include "bxi/base/log.h"

/**
 * @file    snmplog_handler.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The net-snmp Logging Handler
 *
 * The net-snmp logging handler redirect all logs produced by the bxilog library
 * to the net-snmp logging library.
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
 * The Net-SNMP Handler.
 *
 * Parameters for the ::bxilog_handler_p.param_new() function are given below:
 *
 * @note the net-snmp logging library must be initialized. See net-snmp documentation
 * for details.
 */
extern const bxilog_handler_p BXILOG_SNMPLOG_HANDLER;
#else
extern bxilog_handler_p BXILOG_SNMPLOG_HANDLER;
#endif

//*********************************************************************************
//********************************** Interfaces        ****************************
//*********************************************************************************


#endif




