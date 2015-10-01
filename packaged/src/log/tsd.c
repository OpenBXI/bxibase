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


#include <pthread.h>
#include <sys/syscall.h>

#include "bxi/base/zmq.h"

#include "log_impl.h"
#include "tsd_impl.h"






//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

/*
 * Since zeromq implies that zeromq sockets should not be shared,
 * we use thread-specific data to holds thread specific sockets,
 */

//*********************************************************************************
//********************************** Interface         ****************************
//*********************************************************************************

/* Free the thread-specific data */
void bxilog__tsd_free(void * const data) {
    const tsd_p tsd = (tsd_p) data;

    if (NULL != tsd->data_channel) {
        bxierr_p err = BXIERR_OK, err2;
        err2 = bxizmq_zocket_destroy(tsd->data_channel);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_zocket_destroy(tsd->ctrl_channel);
        BXIERR_CHAIN(err, err2);
        if (bxierr_isko(err)) bxierr_report(err, STDERR_FILENO);
    }
    BXIFREE(tsd->log_buf);
    BXIFREE(tsd);
}


/* Allocate the tsd key */
void bxilog__tsd_key_new() {
    int rc = pthread_key_create(&BXILOG__GLOBALS->tsd_key, bxilog__tsd_free);
    bxiassert(0 == rc);
}


/* Return the thread-specific data.*/
bxierr_p bxilog__tsd_get(tsd_p * result) {
    bxiassert(result != NULL);
    // Initialize the tsdlog_key
    tsd_p tsd = pthread_getspecific(BXILOG__GLOBALS->tsd_key);
    if (NULL != tsd) {
        *result = tsd;
        return BXIERR_OK;
    }
    errno = 0;
    tsd = bximem_calloc(sizeof(*tsd));
    tsd->min_log_size = SIZE_MAX;

    bxiassert(NULL != BXILOG__GLOBALS->param->handlers);
    bxiassert(0 < BXILOG__GLOBALS->param->tsd_log_buf_size);
    tsd->log_buf = bximem_calloc(BXILOG__GLOBALS->param->tsd_log_buf_size);
    bxierr_group_p errlist = bxierr_group_new();
    for (size_t i = 0; i < BXILOG__GLOBALS->param->handlers_nb; i++) {
        char * url = BXILOG__GLOBALS->param->handlers_params[i]->data_url;
        bxiassert(NULL != url);
        bxierr_p err = BXIERR_OK, err2;

        err2 = bxizmq_zocket_connect(BXILOG__GLOBALS->zmq_ctx,
                                     ZMQ_PUB,
                                     url,
                                     &tsd->data_channel);
        BXIERR_CHAIN(err, err2);

//        fprintf(stderr, "Connecting %p to %s\n", tsd->data_channel, url);

        err2 = bxizmq_zocket_setopt(tsd->data_channel,
                                    ZMQ_SNDHWM,
                                    &BXILOG__GLOBALS->param->data_hwm,
                                    sizeof(int));
        BXIERR_CHAIN(err, err2);

        url = BXILOG__GLOBALS->param->handlers_params[i]->ctrl_url,
        err2 = bxizmq_zocket_connect(BXILOG__GLOBALS->zmq_ctx,
                                     ZMQ_REQ,
                                     url,
                                     &tsd->ctrl_channel);
        BXIERR_CHAIN(err, err2);

//        fprintf(stderr, "Connecting %p to %s\n", tsd->ctrl_channel, url);

        err2 = bxizmq_zocket_setopt(tsd->ctrl_channel,
                                    ZMQ_SNDHWM,
                                    &BXILOG__GLOBALS->param->ctrl_hwm,
                                    sizeof(int));
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err)) bxierr_list_append(errlist, err);
    }
    if (0 < errlist->errors_nb) {
        *result = tsd;
        return bxierr_from_group(BXIERR_GROUP_CODE,
                                 errlist,
                                 "At least one error occured "
                                 "while connecting to one of %zu handlers",
                                 BXILOG__GLOBALS->param->handlers_nb);
    }
    bxierr_group_destroy(&errlist);

#ifdef __linux__
    tsd->tid = (pid_t) syscall(SYS_gettid);
#endif
    // Do not try to return the kernel TID here, they might not be the same
    // Linux relies on a 1:1 mapping between kernel threads and user threads
    // but this is an NTPL implementation choice. Let's assume it might change
    // in the future.
    tsd->thread_rank = (uint16_t) pthread_self();
    int rc = pthread_setspecific(BXILOG__GLOBALS->tsd_key, tsd);
    // Nothing to do otherwise. Using a log will add a recursive call... Bad.
    // And the man page specified that there is
    bxiassert(0 == rc);

    *result = tsd;
    return BXIERR_OK;
}


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

