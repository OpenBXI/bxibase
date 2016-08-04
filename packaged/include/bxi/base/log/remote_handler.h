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


#ifndef BXILOG_REMOTE_HANDLER_H_
#define BXILOG_REMOTE_HANDLER_H_

#include "bxi/base/err.h"
#include "bxi/base/log.h"


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

extern const bxilog_handler_p BXILOG_REMOTE_HANDLER;
#else
extern bxilog_handler_p BXILOG_REMOTE_HANDLER;
#endif


//*********************************************************************************
//********************************  Interfaces  ***********************************
//*********************************************************************************


#endif
