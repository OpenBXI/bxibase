/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_LEVEL_H_
#define BXILOG_LEVEL_H_

#ifndef BXICFFI

#endif


/**
 * @file    level.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  Logging levels
 *
 *
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

/**
 *  Superset of Syslog levels:
 *  see for a good introduction on log levels and their usage.
 *  http://www.ypass.net/blog/2012/06/introduction-to-syslog-log-levelspriorities/
 */
typedef enum {
    // Note: first letter of a log level name should be unique
    BXILOG_OFF = 0,                             //!< no log
    BXILOG_PANIC,                           //!< system is unusable
    BXILOG_ALERT,                           //!< action must be taken immediately
    BXILOG_CRITICAL,                        //!< critical conditions
    BXILOG_ERROR,                           //!< error conditions
    BXILOG_WARNING,                         //!< warning conditions
    BXILOG_NOTICE,                          //!< notice conditions
    BXILOG_OUTPUT,                          //!< `printf()` replacement
    BXILOG_INFO,                            //!< informational message
    BXILOG_DEBUG,                           //!< debug-level message
    BXILOG_FINE,                            //!< detailed debug-level message
    BXILOG_TRACE,                           //!< very detailed debug-level message
    BXILOG_LOWEST,                          //!< most detailed debug-level message
// Alias
#ifndef BXICFFI
    BXILOG_EMERG = BXILOG_PANIC,    //!< alias for `BXILOG_PANIC`
    BXILOG_EMERGENCY = BXILOG_PANIC,//!< alias for `BXILOG_PANIC`
    BXILOG_CRIT = BXILOG_CRITICAL,  //!< alias for `BXILOG_CRITICAL`
    BXILOG_ERR = BXILOG_ERROR,      //!< alias for `BXILOG_ERROR`
    BXILOG_WARN = BXILOG_WARNING,   //!< alias for `BXILOG_WARNING`
    BXLOG_OUT = BXILOG_OUTPUT,      //!< alias for `BXILOG_OUTPUT`
    // If you change BXILOG_ALL, remember to also change the Python part,
    // see log.py
    BXILOG_ALL = BXILOG_LOWEST,     //!< convenience
#endif
} bxilog_level_e;


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************


/**
 * Returns the level corresponding to the given string representation.
 *
 * @param[in] level_str the log level name
 * @param[out] level a pointer where the result should be returned.
 *
 * @result BXIERR_OK on success, anything else on error.
 *
 * @see bxilog_level_names()
 */
bxierr_p bxilog_level_from_str(char * level_str, bxilog_level_e *level);

/**
 * Return an array of (statically allocated) strings representing all known
 * level names.
 *
 * @param[out] names a pointer on an array of strings that will be set by this function.
 *
 * @return the size of the array
 */
size_t bxilog_level_names(char *** names);

#endif /* BXILOG_H_ */
