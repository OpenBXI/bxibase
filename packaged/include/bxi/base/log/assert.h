/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_ASSERT_H_
#define BXILOG_ASSERT_H_
#ifdef __cplusplus
extern "C" {
#endif

#ifndef BXICFFI
#endif


/**
 * @file    assert.h
 * @author Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  BXI Assertion Module
 *
 * This module provides assertion macros and functions that guarantees the flushing of
 * logs before aborting. They should be seen as replacement of the standard assert()
 * function.
 *
 */


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

/**
 * Equivalent to assert() but ensure logs are flushed before exiting.
 *
 * @param[in] logger the logger to use for logging before exiting
 * @param[in] expr the boolean expression to check
 * @see BXIEXIT()
 * @see BXIABORT_IFKO()
 */
#define BXIASSERT(logger, expr) do {                                                \
            bxilog_assert(logger, expr,                                             \
                          (char *)__FILE__, ARRAYLEN(__FILE__),                     \
                          __func__, ARRAYLEN(__func__),                             \
                          __LINE__, (char*)#expr);                                         \
    } while(false)

/**
 * Abort the program if the given bxierr_p  is not BXIERR_OK.
 *
 * @param[in] logger the logger to use for logging before exiting
 * @param[in] err a bxierr_p
 *
 * @see BXIASSERT()
 * @see BXIEXIT()
 */
#define BXIABORT_IFKO(logger, err) do {                                             \
            bxilog_abort_ifko(logger, &err,                                         \
                          (char *)__FILE__, ARRAYLEN(__FILE__),                     \
                          __func__, ARRAYLEN(__func__),                             \
                          __LINE__);                                                \
    } while(false)


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
 * Exit the program if the given `result` (normally coming from the given `expr`)
 * is false.
 *
 * This function sole purpose is to make the macro BXIASSERT() cleaner.
 * Use BXIASSERT() instead.
 *
 * @param[in] logger a logger instance
 * @param[in] result a boolean value (usually a check)
 * @param[in] file the file name
 * @param[in] filelen the file name length (including the terminal NULL byte)
 * @param[in] func the function name
 * @param[in] funclen the function name length (including the terminal NULL byte)
 * @param[in] line the line number
 * @param[in] expr a string representing the expression that returned the given result
 *
 * @see BXIASSERT()
 * @see BXIEXIT()
 * @see BXIABORT_IFKO()
 */
void bxilog_assert(bxilog_logger_p logger, bool result,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line, char * expr);


/**
 * Exit the program if the given `bxierr_p` is not BXIERR_OK.
 *
 * This function sole purpose is to make the macro BXIABORT_IFKO() cleaner.
 * Use BXIABORT_IFKO() instead.
 *
 * @param[in] logger a logger instance
 * @param[in] err_p a pointer on a bxierr_p
 * @param[in] file the file name
 * @param[in] filelen the file name length (including the terminal NULL byte)
 * @param[in] func the function name
 * @param[in] funclen the function name length (including the terminal NULL byte)
 * @param[in] line the line number
 *
 * @see BXIASSERT()
 * @see BXIEXIT()
 * @see BXIABORT_IFKO()
 */
void bxilog_abort_ifko(bxilog_logger_p logger, bxierr_p * err_p,
                       char * file, size_t filelen,
                       const char * func, size_t funclen,
                       int line);

#ifdef __cplusplus
}
#endif
#endif
