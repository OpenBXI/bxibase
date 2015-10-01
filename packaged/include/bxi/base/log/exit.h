/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_EXIT_H_
#define BXILOG_EXIT_H_

#ifndef BXICFFI
#endif


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

/**
 * Exit the program with given `exit_code`.
 *
 * The given `bxierr` is used to produce a log on the given `logger` at the
 * given `level`,  before flushing the log and exiting the program.
 *
 * Usage looks like (for an `errno` related error message):
 * ~~~{C}
 * errno = 0;
 * int rc = function_call(...);
 * if (rc != 0) BXIEXIT(EX_OSERR,  // See sysexits.h for other standard exit code
 *                      bxierr_errno("Can't call function_call()),
 *                      MYLOGGER, BXILOG_ERROR);
 * ~~~
 *
 * Same stuff with a generic `bxierr`:
 * ~~~{C}
 * bxierr_p err = function_call(...);
 * if (BXIERR_OK != err) BXIEXIT(EXIT_FAILURE, err, MYLOGGER, BXILOG_ERROR);
 * ~~~
 *
 * @param[in] exit_code the exit code to exit the program with (see `sysexits.h`)
 * @param[in] bxierr the `bxierr` to log before exiting
 * @param[in] logger the `logger` to produce a log with
 * @param[in] level the `bxilevel_e` for the log (should be `BXILOG_CRITICAL` at least)
 */
#define BXIEXIT(exit_code, bxierr, logger, level) do {                              \
    bxilog_exit((exit_code), (bxierr), (logger), (level),                           \
                (char *)__FILE__, ARRAYLEN(__FILE__),                               \
                __func__, ARRAYLEN(__func__),                                       \
                __LINE__);                                                          \
    } while(false)

/**
 * Use this macro to exit with a log and a flush on unreachable statements.
 *
 * Call `BXIEXIT()` internally.
 *
 * @param[in] logger the logger to use in the case the statement is reached
 *
 * @see BXIEXIT()
 */
#define BXIUNREACHABLE_STATEMENT(logger) do {                                       \
    BXIEXIT(EX_SOFTWARE,                                                            \
            bxierr_new(BXIUNREACHABLE_STATEMENT_CODE,                               \
                                 NULL, NULL, NULL, NULL,                                  \
                                 "Unreachable statement reached! This is a bug"     \
                                 BXIBUG_STD_MSG),                                   \
            (logger),                                                               \
            BXILOG_CRITICAL);                                                       \
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
 * Log the given `err` on the given `logger`, flush the logging system and exit
 * the program with the given `exit_code`.
 *
 * This function sole purpose is to make the macro BXIEXIT() cleaner.
 * Use BXIEXIT() instead.
 *
 * @param[in] exit_code the exit code to exit the program with (see sysexits.h)
 * @param[in] err the bxierr to log
 * @param[in] logger the logger to produce a log with
 * @param[in] level the level for the log (should be BXILOG_CRITICAL at least)
 * @param[in] file the filename from which the exit should appear to come from
 * @param[in] filelen the length of the filename
 * @param[in] func the function name which the exit should appear to come from
 * @param[in] funclen the function name length
 * @param[in] line the line number in the function from which the exit should appear
 *        to come from
 * @see BXIEXIT
 */
void bxilog_exit(int exit_code,
                 bxierr_p err,
                 bxilog_p logger,
                 bxilog_level_e level,
                 char * file,
                 size_t filelen,
                 const char * func,
                 size_t funclen,
                 int line);

#endif
