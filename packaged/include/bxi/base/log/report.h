/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_REPORT_H_
#define BXILOG_REPORT_H_

#ifndef BXICFFI
#endif


/**
 * @file    report.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  BXI Error Reporting Module
 *
 * This module provides several macros and functions that deals with bxierr_p
 * reporting. They must be used in replacement of printf(), fprintf() and also
 * bxierr_report() function.
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

/**
 * Log the given error with the given message and destroys it.
 *
 * Note the given err is assigned ::BXIERR_OK.
 *
 * Basic usage is:
 * ~~~{C}
 * err = f(...);
 * if (bxierr_isko(err)) BXILOG_REPORT(MY_LOGGER, BXILOG_ERROR, &err,
 *                                     "Calling f() failed");
 * assert(bxierr_isok(err)); // Always true, since BXILOG_REPORT
 *                           // destroys err and assign it ::BXIERR_OK
 * ...
 * ~~~
 *
 * @see bxilog_report
 * @see BXILOG_REPORT_KEEP
 * @see bxilog_report_keep
 */
#define BXILOG_REPORT(logger, level, err, ...) do {                                 \
        bxilog_report((logger), (level), (&err),                                     \
                      (char *)__FILE__, ARRAYLEN(__FILE__),                         \
                      __func__, ARRAYLEN(__func__),                                 \
                      __LINE__, __VA_ARGS__);                                     \
    } while(false)


/**
 * Log the given error with the given message.
 *
 * This is a convenience macro, used mainly in test. Most of the time,
 * an error reported, has no good reason from being kept in the process.
 * Therefore, one might prefer ::BXILOG_REPORT instead which destroys
 * the error after reporting.
 *
 * Basic usage is:
 * ~~~{C}
 * err = f(...);
 * if (bxierr_isko(err)) BXILOG_REPORT_KEEP(MY_LOGGER, BXILOG_ERROR, &err,
 *                                          "Calling f() failed");
 * ...
 * ~~~
 *
 * @see bxilog_report_keep
 * @see BXILOG_REPORT
 * @see bxilog_report
 */
#define BXILOG_REPORT_KEEP(logger, level, err, ...) do {                             \
        bxilog_report_keep((logger), (level), (&err),                                     \
                           (char *)__FILE__, ARRAYLEN(__FILE__),                         \
                           __func__, ARRAYLEN(__func__),                                 \
                           __LINE__, __VA_ARGS__);                                     \
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
 * Report an error and destroy it.
 *
 * Note: after this call bxierr_isko(*err) is always true.
 *
 * This function sole purpose is to make the macro BXILOG_REPORT() cleaner.
 * Use BXILOG_REPORT() instead.
 *
 * @param[in] logger a logger instance
 * @param[in] level the logger level to use while reporting the given error
 * @param[in] err pointer on the error to report
 * @param[in] file the file name
 * @param[in] filelen the file name length (including the terminal NULL byte)
 * @param[in] func the function name
 * @param[in] funclen the function name length (including the terminal NULL byte)
 * @param[in] line the line number
 * @param[in] fmt a printf-like format string
 *
 * @see BXILOG_REPORT
 */
void bxilog_report(bxilog_logger_p logger, bxilog_level_e level, bxierr_p * err,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line,
                   const char * fmt, ...)
#ifndef BXICFFI
                   __attribute__ ((format (printf, 9, 10)))
#endif
;

/**
 * Report an error but do not destroy it.
 *
 * This function sole purpose is to make the macro BXILOG_REPORT_KEEP() cleaner.
 * Use BXILOG_REPORT_KEEP() instead.
 *
 * @param[in] logger a logger instance
 * @param[in] level the logger level to use while reporting the given error
 * @param[in] err pointer on the error to report
 * @param[in] file the file name
 * @param[in] filelen the file name length (including the terminal NULL byte)
 * @param[in] func the function name
 * @param[in] funclen the function name length (including the terminal NULL byte)
 * @param[in] line the line number
 * @param[in] fmt a printf-like format string
 *
 * @see BXILOG_REPORT_KEEP
 */
void bxilog_report_keep(bxilog_logger_p logger, bxilog_level_e level, bxierr_p * err,
                        char * file, size_t filelen,
                        const char * func, size_t funclen,
                        int line,
                        const char * fmt, ...)
#ifndef BXICFFI
                       __attribute__ ((format (printf, 9, 10)))
#endif
;


/**
 * Log a report at a logging level with a message
 *
 * @note the report is destroyed after this call
 *
 * @param[inout] self a report
 * @param[in] logger the logger to emit the report with
 * @param[in] level the level at which the report must be produced
 * @param[in] filename the file name from which the log is emitted from
 * @param[in] filename_len the file name length (including the NULL terminating byte)
 * @param[in] func the function name from which the log is emitted from
 * @param[in] funclen the function name length (including the NULL terminating byte)
 * @param[in] line the line number in the function
 * @param[in] msg the message to produce along with the report
 * @param[in] msglen the message length (including the NULL terminating byte)
 */
void bxilog_report_raw(bxierr_report_p self,
                       bxilog_logger_p logger, bxilog_level_e level,
                       const char * filename, size_t filename_len,
                       const char * func, size_t funclen,
                       int line,
                       const char * msg, size_t msglen);


#endif
