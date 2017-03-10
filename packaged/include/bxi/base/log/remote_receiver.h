/**
 * @file    remote_receiver.h
 * @authors Sébastien Miquée <sebastien.miquee@atos.net>
 * @authors Pierre Vignéras <pierre.vigneras@atos.net>
 * @copyright 2016  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The Monitor Logging Handler
 *
 * The remote receiver receives logs through a or more ZMQ sockets.
 */


#ifndef BXILOG_REMOTE_RECV_H_
#define BXILOG_REMOTE_RECEIVER_H_

#include "bxi/base/err.h"
#include "bxi/base/log.h"


//*********************************************************************************
//********************************  Defines  **************************************
//*********************************************************************************

//*********************************************************************************
//*********************************  Types  ***************************************
//*********************************************************************************


typedef struct bxilog_remote_receiver_s * bxilog_remote_receiver_p;


//*********************************************************************************
//****************************  Global Variables  *********************************
//*********************************************************************************

//*********************************************************************************
//********************************  Interfaces  ***********************************
//*********************************************************************************

bxilog_remote_receiver_p bxilog_remote_receiver_new(const char ** urls, size_t urls_nb,
                                                    size_t sync_nb, bool bind);


void bxilog_remote_receiver_destroy(bxilog_remote_receiver_p *self_p);


/**
 * The asynchronous Remote Receiver function.
 *
 * This function starts a background thread to handle the remote bxilogs. A
 * blocking version is also available as `bxilog_remote_recv`.
 *
 * @param[in] param the bxilog_remote_recv_s parameters
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_remote_receiver_start(bxilog_remote_receiver_p self);


/**
 * Stop the asynchronous Remote Receiver function.
 *
 * This function aims at stopping the background thread handling the remote
 * bxilogs. If no background thread has been started it returns a BXIERROR.
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_remote_receiver_stop(bxilog_remote_receiver_p self);


#endif
