/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_LOGGER_H_
#define BXILOG_LOGGER_H_

#ifndef BXICFFI
#include <stdbool.h>
#endif


#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log/level.h"


/**
 * @file    logger.h
 * @author Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The logger object
 *
 * Business code threads use loggers to produce logs in the system.
 * A logger is defined by at least a name and a logging level bxilog_level_e.
 *
 */


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
/**
 * Produce a log at the `BXILOG_LOWEST` level
 */
#define LOWEST(logger, ...) bxilog_logger_log(logger, BXILOG_LOWEST, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__);
/**
 * Produce a log at the `BXILOG_TRACE` level
 */
#define TRACE(logger, ...) bxilog_logger_log(logger, BXILOG_TRACE, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__);
/**
 * Produce a log at the `BXILOG_FINE` level
 */
#define FINE(logger, ...) bxilog_logger_log(logger, BXILOG_FINE, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_DEBUG` level
 */
#define DEBUG(logger, ...) bxilog_logger_log(logger, BXILOG_DEBUG, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_INFO` level
 */
#define INFO(logger, ...)  bxilog_logger_log(logger, BXILOG_INFO, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_OUTPUT` level
 */
#define OUT(logger, ...)   bxilog_logger_log(logger, BXILOG_OUTPUT, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_NOTICE` level
 */
#define NOTICE(logger, ...)  bxilog_logger_log(logger, BXILOG_NOTICE, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_WARNING` level
 */
#define WARNING(logger, ...)  bxilog_logger_log(logger, BXILOG_WARNING, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_ERROR` level
 */
#define ERROR(logger, ...)   bxilog_logger_log(logger, BXILOG_ERROR, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_CRITICAL` level
 */
#define CRITICAL(logger, ...)  bxilog_logger_log(logger, BXILOG_CRITICAL, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_ALERT` level
 */
#define ALERT(logger, ...)  bxilog_logger_log(logger, BXILOG_ALERT, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)
/**
 * Produce a log at the `BXILOG_PANIC` level
 */
#define PANIC(logger, ...)  bxilog_logger_log(logger, BXILOG_PANIC, (char *)__FILE__, ARRAYLEN(__FILE__), __func__, ARRAYLEN(__func__), __LINE__, __VA_ARGS__)



/**
 * Create a log using the given logger at the given level.
 *
 * @see `bxilog_log_nolevel_check()`
 */
// TODO: do log the actual log also with the same format than the actual log instead
// of throwing it away
#define bxilog_logger_log(logger, lvl, filename, filename_len, funcname, funcname_len, line, ...) do {\
        if (bxilog_logger_is_enabled_for((logger), (lvl))) {                            \
            bxierr_p __err__ = bxilog_logger_log_nolevelcheck((logger), (lvl),          \
                                                       (filename), (filename_len),      \
                                                       (funcname), (funcname_len),      \
                                                       (line), __VA_ARGS__);            \
            if (bxierr_isko(__err__)) {                                                 \
                bxierr_report(&__err__, STDOUT_FILENO);                                 \
            }                                                                           \
        }                                                                               \
    } while(false);


/**
 * Defines a new logger as a global variable
 * Should be set inside your .c file, in order to define a new logger structure.
 * Note that by default, loggers level is set to `BXILOG_LOWEST`
 */
#ifdef __cplusplus
#define SET_LOGGER(variable_name, logger_name) \
    static struct bxilog_logger_s variable_name ## _s = { \
                                        false, \
                                        logger_name,\
                                        ARRAYLEN(logger_name),\
                                        BXILOG_LOWEST\
    };\
    static bxilog_logger_p const variable_name = &variable_name ## _s;\
    static __attribute__((constructor)) void __bxilog_register_log__ ## variable_name(void) {\
        bxilog_registry_add(variable_name);\
    }\
    static __attribute__((destructor)) void __bxilog_unregister_log__ ## variable_name(void) {\
        bxilog_registry_del(variable_name);\
    }
#else
#ifdef BXICFFI
#define SET_LOGGER(variable_name, logger_name) \
    static struct bxilog_logger_ variable_name ## _s = { \
                                        .allocated = false, \
                                        .name = logger_name,\
                                        .name_length = ARRAYLEN(logger_name),\
                                        .level = BXILOG_LOWEST\
    };\
    static bxilog_logger_p const variable_name = &variable_name ## _s;
#else
#define SET_LOGGER(variable_name, logger_name) \
    static struct bxilog_logger_s variable_name ## _s = { \
                                        .allocated = false, \
                                        .name = logger_name,\
                                        .name_length = ARRAYLEN(logger_name),\
                                        .level = BXILOG_LOWEST\
    };\
    static bxilog_logger_p const variable_name = &variable_name ## _s;\
    static __attribute__((constructor)) void __bxilog_register_log__ ## variable_name(void) {\
        bxilog_registry_add(variable_name);\
    }\
    static __attribute__((destructor)) void __bxilog_unregister_log__ ## variable_name(void) {\
         bxilog_registry_del(variable_name);\
    }
#endif
#endif


// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

/**
 * Data structure representing a logger.
 *
 * @see bxilog_logger_p
 */
struct bxilog_logger_s {
    bool allocated;                 //!< true if allocated on the heap, false otherwise
    const char * name;              //!< Logger name
    size_t name_length;             //!< Logger name length, including NULL ending byte
    bxilog_level_e level;           //!< Logger level
};


/**
 * A logger "object".
 *
 * Use `SET_LOGGER()` to create one (or `bxilog_get()`
 * from a high level language).
 */
typedef struct bxilog_logger_s * bxilog_logger_p;


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Free the given logger. Use destroy() instead.
 *
 * @note: this is mainly for internal purpose, do not use unless you understand the
 * relationship with logging registry.
 *
 * @param[in] self a pointer on a logger instance
 */
void bxilog_logger_free(bxilog_logger_p self);

/**
 * Destroy the given logger.
 *
 * @note: this is mainly for internal purpose, do not use unless you understand the
 * relationship with logging registry.
 *
 * @param[inout] self_p a pointer on a logger instance
 */
void bxilog_logger_destroy(bxilog_logger_p * self_p);

/**
 * Create a log unconditionally without formatting. This is mainly used for
 * high level languages binding.
 *
 *
 * WARNING: the `filename_len` and `funcname_len` should include the NULL
 * terminated string. Use strlen(s) + 1 for dynamic strings,
 * or ARRAYLEN(a) for static strings.
 *
 * @param[in] logger the logger to perform the log with
 * @param[in] level the level at which the log must be emitted
 * @param[in] filename the name of the source file the log comes from
 * @param[in] filename_len the length of 'filename' including the NULL terminating byte
 * @param[in] funcname the name of the function the log comes from
 * @param[in] funcname_len the length of 'funcname' including the NULL terminating byte
 * @param[in] line the line number in file 'filename' the log comes from
 * @param[in] rawstr the raw string
 * @param[in] rawstr_len the length of the string including the NULL terminating byte
 *
 * @return BXIERR_OK on success, any other value is an error
 *
 * @see bxilog_logger_log()
 * @see bxierr_p
 */
bxierr_p bxilog_logger_log_rawstr(const bxilog_logger_p logger, const bxilog_level_e level,
                                  const char * filename, size_t filename_len,
                                  const char * funcname, size_t funcname_len,
                                  int line,
                                  const char * rawstr, size_t rawstr_len);


/**
 * Create a log unconditionally. This is mainly used by macros defined above
 * that already check the logger level, removing the useless function call.
 *
 * If you need the check, either do it manually, or use the `bxilog_logger_log()` macro.
 *
 * WARNING: the `filename_len` and `funcname_len` should include the NULL
 * terminated string. Use strlen(s) + 1 for dynamic strings,
 * or ARRAYLEN(a) for static strings.
 *
 * @param[in] logger the logger to perform the log with
 * @param[in] level the level at which the log must be emitted
 * @param[in] fullfilename the name of the source file the log comes from
 * @param[in] fullfilename_len the length of 'filename' including the NULL terminating byte
 * @param[in] funcname the name of the function the log comes from
 * @param[in] funcname_len the length of 'funcname' including the NULL terminating byte
 * @param[in] line the line number in file 'filename' the log comes from
 * @param[in] fmt the printf like format of the message
 *
 * @return BXIERR_OK on success, any other value is an error
 *
 * @see bxilog_logger_log()
 * @see bxierr_p
 */
bxierr_p bxilog_logger_log_nolevelcheck(const bxilog_logger_p logger, const bxilog_level_e level,
                                        char * fullfilename, size_t fullfilename_len,
                                        const char * funcname, size_t funcname_len,
                                        int line,
                                        const char * fmt, ...)
#ifndef BXICFFI
                                        __attribute__ ((format (printf, 8, 9)))
#endif
                                        ;


#ifndef BXICFFI
/**
 * Equivalent to `bxilog_log_nolevelcheck()` but with a va_list instead of
 * a variable number of arguments.
 *
 * @param[in] logger the logger to perform the log with
 * @param[in] level the level at which the log must be emitted
 * @param[in] fullfilename the name of the source file the log comes from
 * @param[in] fullfilename_len the length of 'filename' including the NULL terminating byte
 * @param[in] funcname the name of the function the log comes from
 * @param[in] funcname_len the length of 'funcname' including the NULL terminating byte
 * @param[in] line the line number in file 'filename' the log comes from
 * @param[in] fmt the printf like format of the message
 * @param[in] arglist the va_list of all parameters for the given format string 'fmt'
 *
 * @return BXIERR_OK on success, any other value is an error
 * @see bxilog_logger_log_nolevelcheck
 */
bxierr_p bxilog_logger_vlog_nolevelcheck(const bxilog_logger_p logger, const bxilog_level_e level,
                                         const char * fullfilename, size_t fullfilename_len,
                                         const char * funcname, size_t funcname_len,
                                         const int line,
                                         const char * fmt, va_list arglist);
#endif

/**
 * Get the log level of the given logger
 *
 * @param[in] logger the logger instance
 *
 * @return the given logger log level
 */
bxilog_level_e bxilog_logger_get_level(const bxilog_logger_p logger);

/**
 * Set the log level for the given logger.
 *
 * @param[in] logger the logger instance
 * @param[in] level the log level
 */
void bxilog_logger_set_level(const bxilog_logger_p logger, const bxilog_level_e level);

/**
 * Reconfigure the given logger according to the current bxilog configuration.
 *
 * @param[inout] logger the logger instance to reconfigure
 */
void bxilog_logger_reconfigure(const bxilog_logger_p logger);

/**
 * Return true if the given logger is enabled at the given log level.
 *
 * @param[in] logger the logger instance
 * @param[in] level the log level
 *
 * @return true if the given logger is enabled at the given log level.
 */
#ifndef BXICFFI
inline bool bxilog_logger_is_enabled_for(const bxilog_logger_p logger,
                                         const bxilog_level_e level) {

    bxiassert(logger != NULL && level <= BXILOG_LOWEST);
    return level <= logger->level && level != BXILOG_OFF;
}
#else
bool bxilog_logger_is_enabled_for(const bxilog_logger_p logger, const bxilog_level_e level);
#endif



#endif /* BXILOG_H_ */
