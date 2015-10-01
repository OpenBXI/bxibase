/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_SIGNAL_H_
#define BXILOG_SIGNAL_H_

#ifndef BXICFFI
#include <signal.h>
#include <sys/signalfd.h>
#endif


#include "bxi/base/mem.h"
#include "bxi/base/err.h"



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

/**
 * Return a string representation of the given signal number using the
 * given siginfo or sfdinfo (only one must be NULL).
 *
 * Note: the returned string will have to be released using FREE().
 *
 * @param[in] siginfo the signal information
 * @param[in] sfdinfo the signal information
 *
 * @return a string representation of the given signal number
 *
 */
char * bxilog_signal_str(const siginfo_t * siginfo,
                         const struct signalfd_siginfo * sfdinfo);
#endif


#endif /* BXILOG_H_ */
