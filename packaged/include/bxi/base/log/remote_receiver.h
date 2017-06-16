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


typedef struct bxilog_remote_receiver_s * bxilog_remote_receiver_p;


//*********************************************************************************
//****************************  Global Variables  *********************************
//*********************************************************************************

//*********************************************************************************
//********************************  Interfaces  ***********************************
//*********************************************************************************

/**
 * Create a new remote receiver of bxilog
 *
 * @param[in] urls a set of urls to bind/connect to
 * @param[in] urls_nb the number of urls to bind/connect to
 * @param[in] bind whether given urls must be binded or connected to
 * @param[in] hostname of the remote server
 *
 * @return a new receiver instance
 * @note when executing on a remote handler on a different node and binding on the
 * node of the remote handler the hostname is required in other cases
 * the hostname could be NULL
 */
bxilog_remote_receiver_p bxilog_remote_receiver_new(const char ** urls, size_t urls_nb,
                                                    bool bind, char * hostname);


/**
 * Destroy a remote receiver
 *
 * @note the given pointer is nullified after this call
 *
 * @param[inout] self_p a pointer on the object to destroy
 *
 */
void bxilog_remote_receiver_destroy(bxilog_remote_receiver_p *self_p);


/**
 * The asynchronous Remote Receiver function.
 *
 * This function starts a background thread to handle the remote bxilogs. A
 * blocking version is also available as `bxilog_remote_recv`.
 *
 * @param[in] self the bxilog_remote_recv_s parameters
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
 * @param[in] self the remote receiver object to stop
 * @param[in] wait_remote_exit if true, wait until all
 *            publisher have sent their exit message
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_remote_receiver_stop(bxilog_remote_receiver_p self,
                                     bool wait_remote_exit);


/**
 * Return the urls that the internal thread has binded to or NULL if not applicable.
 *
 * @param[in] self the receiver
 * @param[out] result a pointer on the result
 * @return the number of urls that the internal thread has binded to or 0 if
 *         not applicable
 */
size_t bxilog_get_binded_urls(bxilog_remote_receiver_p self, const char*** result);


#endif
