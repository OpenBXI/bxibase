/**
 * @file    remote_receiver.h
 * @authors Sébastien Miquée <sebastien.miquee@atos.net>
 * @copyright 2016  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The Monitor Logging Handler
 *
 * The remote receiverver receives logs through a or more ZMQ sockets.
 */


#ifndef BXILOG_REMOTE_RECEIVER_H_
#define BXILOG_REMOTE_RECEIVER_H_

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

//*********************************************************************************
//********************************  Interfaces  ***********************************
//*********************************************************************************

/**
 * The Remote Receiver function.
 *
 * Note that this function blocking.
 *
 * @param[in] nb_urls an integer indicating how many urls it should connect to
 * @param[in] urls the urls list
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_remote_recv(int nb_urls, const char ** urls);


#endif
