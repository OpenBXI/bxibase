/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_THREAD_H_
#define BXILOG_THREAD_H_

#ifndef BXICFFI

#endif


#include "bxi/base/mem.h"
#include "bxi/base/err.h"


/**
 * @file    thread.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  BXI Thread Module
 *
 * This module provides macros and functions that deals with threading in general.
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************


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
 * Set a logical rank to the current pthread.
 * This is used when displaying a log to differentiate logical threads
 * from kernel threads.
 *
 * @param[in] rank the logical rank of the current thread
 *
 * @see bxilog_get_thread_rank
 * @return BXIERR_OK on success, any other values is an error
 */
bxierr_p bxilog_set_thread_rank(uintptr_t rank);

/**
 * Return the logical rank of the current pthread.
 *
 * If no logical rank has been set by `bxilog_set_thread_rank()`,
 * a generated logical rank will be returned
 * (uniqueness is not guaranteed).
 *
 * @param[out] rank_p a pointer on the result
 *
 * @return BXIERR_OK on success, any other values is an error
 */
bxierr_p bxilog_get_thread_rank(uintptr_t * rank_p);


#endif /* BXILOG_H_ */
