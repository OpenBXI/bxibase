/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_REPORT_H_
#define BXILOG_REPORT_H_

#ifndef BXICFFI
#endif


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
void bxilog_report(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
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
void bxilog_report_keep(bxilog_p logger, bxilog_level_e level, bxierr_p * err,
                        char * file, size_t filelen,
                        const char * func, size_t funclen,
                        int line,
                        const char * fmt, ...)
#ifndef BXICFFI
                       __attribute__ ((format (printf, 9, 10)))
#endif
;



#endif
