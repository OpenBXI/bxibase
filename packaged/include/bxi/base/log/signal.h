/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_SIGNAL_H_
#define BXILOG_SIGNAL_H_
#ifdef __cplusplus
extern "C" {
#endif

#ifndef BXICFFI
#include <signal.h>
#endif


#include "bxi/base/mem.h"
#include "bxi/base/err.h"


/**
 * @file    signal.h
 * @author Pierre Vignéras <pierre.vigneras@atos.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  BXI Signal Handling Module
 *
 * This module provides several macros and functions that deals with POSIX signal
 * management. They must be used to ensure logs are actually flushed before aborting.
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************


/**
 * The error code returned when a signal prevented a function to complete successfully.
 */
#define BXILOG_SIGNAL_ERR     10


// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************


/**
 * Install a signal handler that call bxilog_finalize() automatically
 * on reception of the following signals:  SIGTERM, SIGINT, SIGSEGV, SIGBUS,
 * SIGFPE, SIGILL, SIGABRT.
 *
 * Note that no signal handler is set for SIGQUIT (ctrl-\ on a standard UNIX terminal)
 * (so you can bypass the sighandler if you need to).
 *
 * @return BXIERR_OK on success, any other values is an error
 */
bxierr_p bxilog_install_sighandler(void);

#ifndef BXICFFI
/**
 * Create a sigset structure according to the given array of signal numbers.
 *
 * @param[out] sigset
 * @param[in] signum
 * @param[in] n
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxilog_sigset_new(sigset_t *sigset, int * signum, size_t n);

#endif


#ifdef __cplusplus
}
#endif
#endif /* BXILOG_H_ */
