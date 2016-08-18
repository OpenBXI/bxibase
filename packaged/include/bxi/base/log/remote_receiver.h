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

/**
 * BXILog remote reciver paramters
 */
typedef struct bxilog_remote_recv_s {
    int nb_urls;           //!< Number of urls to connect to
    bool bind;             //!< If true, bind instead of connect
    const char ** urls;    //!< The urls
} bxilog_remote_recv_s;

typedef bxilog_remote_recv_s * bxilog_remote_recv_p;


//*********************************************************************************
//****************************  Global Variables  *********************************
//*********************************************************************************

//*********************************************************************************
//********************************  Interfaces  ***********************************
//*********************************************************************************

/**
 * The Remote Receiver function.
 *
 * Note that this function is blocking. A version starting a background thread
 * is also available as `bxilog_remote_recv_async`.
 *
 * @param[in] param the bxilog_remote_recv_s parameters
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_remote_recv(bxilog_remote_recv_p param);


/**
 * The Asynchrounous Remote Receiver function.
 *
 * This function starts a background thread to handle the remote bxilogs. A
 * blocking version is also availalbe as `bxilog_remote_recv`.
 *
 * @param[in] param the bxilog_remote_recv_s parameters
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_remote_recv_async_start(bxilog_remote_recv_p param);


/**
 * Stop the Asynchrounous Remote Receiver function.
 *
 * This function aims at stopping the background thread handling the remote
 * bxilogs. If no background thread has been started it returns a BXIERROR.
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_remote_recv_async_stop(void);


#endif
