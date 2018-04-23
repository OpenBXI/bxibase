/**
 * @file    remote_handler.h
 * @authors Sébastien Miquée <sebastien.miquee@atos.net>
 * @authors Pierre Vignéras  <pierre.vigneras@atos.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The Monitor Logging Handler
 *
 * The remote handler sends logs through a ZMQ socket.
 */


#ifndef BXILOG_REMOTE_HANDLER_H_
#define BXILOG_REMOTE_HANDLER_H_

#include "bxi/base/err.h"
#include "bxi/base/log.h"


//*********************************************************************************
//********************************  Defines  **************************************
//*********************************************************************************

#define BXILOG_REMOTE_HANDLER_RECORD_HEADER "level/"
#define BXILOG_REMOTE_HANDLER_EXITING_HEADER ".ctrl/exit"
#define BXILOG_REMOTE_HANDLER_CFG_CMD "get-config"

#define BXILOG_REMOTE_HANDLER_URLS "URLs?"
/**
 * Timeout in seconds for PUB/SUB synchronization.
 */
#define BXILOG_REMOTE_HANDLER_SYNC_DEFAULT_TIMEOUT 1.0
//*********************************************************************************
//*********************************  Types  ***************************************
//*********************************************************************************

//*********************************************************************************
//****************************  Global Variables  *********************************
//*********************************************************************************


#ifndef BXICFFI
/**
 * The Remote Handler.
 *
 * Parameters for the ::bxilog_handler_p.param_new() function are given below:
 *
 * @param[in] progname a `char *` string; the program name (argv[0])
 * @param[in] url a `char *` string; the url to bind/connect to
 * @param[in] bind if true, tells that the url must be binded to, otherwise connect()
 *                 is called
 * @param[in] sub_nb an int representing the number of subscribers to synchronize with
 *                   before starting (0 means: do not synchronize)
 */

extern const bxilog_handler_p BXILOG_REMOTE_HANDLER;
#else
extern bxilog_handler_p BXILOG_REMOTE_HANDLER;
#endif


//*********************************************************************************
//********************************  Interfaces  ***********************************
//*********************************************************************************


#endif
