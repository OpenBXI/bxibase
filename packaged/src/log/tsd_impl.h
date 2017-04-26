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

#ifndef BXILOG_TSD_IMPL_H
#define BXILOG_TSD_IMPL_H

#include <unistd.h>
#include <stdint.h>

#include "bxi/base/err.h"

//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

struct tsd_s {
    size_t log_nb;
    size_t rsz_log_nb;
    size_t max_log_size;
    size_t min_log_size;
    size_t sum_log_size;

    char * log_buf;                 // The per-thread log buffer
    void * data_channel;             // The thread-specific zmq logging socket;
    void * ctrl_channel;             // The thread-specific zmq controlling socket;
#ifdef __linux__
    pid_t tid;                      // Cache the tid on Linux since we assume NPTL
                                    // and therefore a 1:1 thread implementation.
#endif
    uintptr_t thread_rank;          // user thread rank
};

typedef struct tsd_s * tsd_p;

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

//*********************************************************************************
//********************************** Interface         ****************************
//*********************************************************************************

/* Free the thread-specific data */
void bxilog__tsd_free(void * const data);

/* Allocate the tsd key */
void bxilog__tsd_key_new();

/* Return the thread-specific data.*/
bxierr_p bxilog__tsd_get(tsd_p * result);


#endif
