/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_HANDLER_H_
#define BXILOG_HANDLER_H_

#ifndef BXICFFI
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <zmq.h>
#endif


#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log/filter.h"


/**
 * @file    handler.h
 * @authors Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2013  Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief   Log Handler
 *
 *
 * A log handler is basically a thread (or many...) that processes logs as they are
 * produced by business code threads.
 *
 * Implementing a new log handler, is quite simple as it only requires the
 * implementation of few functions defined in bxilog_handler_p.
 *
 */


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

/**
 *  Error code used by handlers to specify they must exit.
 */
#define BXILOG_HANDLER_EXIT_CODE 41323817  // Leet code for .A.LER EXIT

/**
 * Error code used by handlers to  specify too many internal errors was detected
 */
#define BXILOG_TOO_MANY_IERR 700471322     // Leet code  for TOO .A.7 IERR

#if defined(__x86_64__)  || defined(__aarch64__)
#define TIMESPEC_SIZE 16
#elif defined(__i386__) || defined(__arm__)
#define TIMESPEC_SIZE 8
#endif

#ifndef BXICFFI
BXIERR_CASSERT(timespec_size, TIMESPEC_SIZE == sizeof(struct timespec));
#endif

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

/**
 * A bxilog record is received for each log produced by business code threads.
 */
typedef struct {
// TODO: reorganize with most-often used data first
// see cachegrind results.
    bxilog_level_e level;               //!< log level
#ifndef BXICFFI
    struct timespec detail_time;        //!< log timestamp
#else
    uint8_t detail_time[TIMESPEC_SIZE]; // fake for CFFI
#endif
#ifndef BXICFFI
    pid_t pid;                          //!< process pid
#ifdef __linux__
    pid_t tid;                          //!< kernel thread id
//    size_t thread_name_len            //!< Thread name (see pthread_getname_np())
#endif
#else
    int pid;                            // fake for CFFI
#ifdef __linux__
    int tid;                            // fake for CFFI
#endif
#endif
    uint16_t thread_rank;               //!< user thread rank
    int line_nb;                        //!< line nb
    size_t filename_len;                //!< file name length
    size_t funcname_len;                //!< function name length
    size_t logname_len;                 //!< logger name length
    size_t logmsg_len;                  //!< the logmsg length
} bxilog_record_s;

/**
 * A bxilog record object.
 */
typedef bxilog_record_s * bxilog_record_p;

typedef enum {
    BXI_LOG_HANDLER_NOT_READY=0,
    BXI_LOG_HANDLER_READY=1,
    BXI_LOG_HANDLER_ERROR=2,
} bxilog_handler_state_e;


// Log handler parameter forward reference.
typedef struct bxilog_handler_param_s bxilog_handler_param_s;

/**
 * The log handler parameter object.
 */
typedef bxilog_handler_param_s * bxilog_handler_param_p;


#ifndef BXICFFI
typedef bxierr_p (*bxilog_handler_cbs)(bxilog_handler_param_p, int);
#endif

/**
 * Log handler structure definition.
 */
struct bxilog_handler_param_s {
    int data_hwm;                       //!< ZMQ High Water Mark for the data socket
    int ctrl_hwm;                       //!< ZMQ High Water Mark for the control socket
    size_t ierr_max;                    //!< Maximal number of internal errors before
                                        //!< exiting
    long flush_freq_ms;                 //!< Implicit flush frequency
    char * data_url;                    //!< The data zocket URL
    char * ctrl_url;                    //!< The control zocket URL
    bxilog_filters_p filters;           //!< The filters
    size_t rank;                        //!< identifier of the handler
    bxilog_handler_state_e status;      //!< handler status
    size_t private_items_nb;            //!< Number of private items
#ifndef BXICFFI
    zmq_pollitem_t * private_items;     //!< Private items (zmq/standard sockets or
                                        //!< file descriptors) used by the handler
                                        //!< zmq_poll(). See zmq_poll(3) for details.
    bxilog_handler_cbs *cbs;            //!< Callbacks to use for each private item in
                                        //!< case of ZMQ_POLLIN, ZMQ_POLLOUT and
                                        //!< ZMQ_POLLERR
#else
    void * private_items;               //!< Fake for CFFI
    void ** cbs;                        //!< Fake for CFFI
#endif

};

/**
 * A log handler.
 */
typedef struct bxilog_handler_s_f bxilog_handler_s;

/**
 * A log handler.
 */
typedef bxilog_handler_s * bxilog_handler_p;

/** A log handler.
 *
 */
struct bxilog_handler_s_f {
    char * name;                                            //!< Log handler name

    /**
     * This function is called automatically by bxilog_config_add_handler(). The
     * arguments passed to this last function are passed into the va_list structure.
     *
     * @param[in] handler the log handler
     * @param[in] filters the list of filters to use
     * @param[in] param_list a list of parameters as given in the
     * bxilog_config_add_handler()
     *
     * @return the parameters to be used by the logging handler.
     */
    bxilog_handler_param_p (*param_new)(bxilog_handler_p handler,
                                        bxilog_filters_p filters,
#ifndef BXICFFI
                                        va_list param_list);
#else
                                        void *);
#endif
    /**
     * This function is called during initialization of the log handler.
     *
     * @param[in] param the log handler parameter as returned by param_new()
     *
     * @return if not BXIERR_OK, the log handler thread will exit.
     */
    bxierr_p (*init)(bxilog_handler_param_p param);

    /**
     * This function is called each time a log is produced.
     *
     * This is the main purpose of a log handler: this function defines what the log
     * handler does with a log. It can discard it, displays on the console, in a file,
     * send it to another process such as syslog, or even send it to another logging
     * backend such as netsnmp-log.
     *
     * @param[in] record a logging record
     * @param[in] filename the filename
     * @param[in] funcname the function name
     * @param[in] loggername the logger name
     * @param[in] logmsg the actual log message
     * @param[in] param the log handler parameter as returned by param_new()
     */
    bxierr_p (*process_log)(bxilog_record_p record,
                            char * filename,
                            char * funcname,
                            char * loggername,
                            char * logmsg,
                            bxilog_handler_param_p param);

    /**
     * Process an internal error, that is an error raised in the handler code itself.
     *
     * @param[in] err the internal error to process
     * @param[in] param the log handler parameter as returned by param_new()
     */
    bxierr_p (*process_ierr)(bxierr_p *err, bxilog_handler_param_p param);

    /**
     * @param[in] param the log handler parameter as returned by param_new()
     */
    bxierr_p (*process_implicit_flush)(bxilog_handler_param_p param);
    /**
     * @param[in] param the log handler parameter as returned by param_new()
     */
    bxierr_p (*process_explicit_flush)(bxilog_handler_param_p param);
    /**
     * @param[in] param the log handler parameter as returned by param_new()
     */
    bxierr_p (*process_exit)(bxilog_handler_param_p param);
    /**
     * @param[in] param the log handler parameter as returned by param_new()
     */
    bxierr_p (*process_cfg)(bxilog_handler_param_p param);

    /**
     * @param[in] param the log handler parameter as returned by param_new()
     */
    bxierr_p (*param_destroy)(bxilog_handler_param_p* param);
};


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

/**
 * Initialize the given handler parameters with the given filters.
 *
 * This function must be called by handler implementation in order to initialize
 * correctly various generic handler implementation parameter.
 *
 * @param[in] handler a handler
 * @param[in] filters a set of filters
 * @param[in] param the parameters to initialize
 */
void bxilog_handler_init_param(bxilog_handler_p handler,
                               bxilog_filters_p filters,
                               bxilog_handler_param_p param);

/**
 * Cleanup the set of parameters initialized with ::bxilog_handler_init_param().
 *
 * @param[inout] param the set of parameters to cleanup
 */
void bxilog_handler_clean_param(bxilog_handler_param_p param);

#endif /* BXILOG_H_ */
