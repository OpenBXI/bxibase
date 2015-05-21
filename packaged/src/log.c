/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <unistd.h>
#include <error.h>
#include <sysexits.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <libgen.h>
#include <fcntl.h>
#include <execinfo.h>
#include <sys/syscall.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <pthread.h>

#include <zmq.h>

#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"

#include "bxi/base/log.h"

/**************************************************************************************
 * ******************************* OVERALL ARCHITECTURE *******************************
 *
 * 1. bxilog_init() sets a global zmq context (hidden from client side), creates the
 *    thread INTERNAL_HANDLER_THREAD (IHT), creates the CONTROLLER_CHANNEL zmq socket
 *    for communication with the IHT and waits for the reception of a ready msg from
 *    the IHT.
 * 2. The IHT creates a data_channel zmq socket, binds it, and sends the ready msg into
 *    the controller channel. Then it polls both controller channel and data channel
 *    for data. If it receives an exit message on the controller channel, it ends.
 *    If it receives a msg on the data channel it processes it.
 *
 * 3. When the business code -- client side of the bxilog API -- calls bxilog (usually
 *    from one of the macro DEBUG, INFO, ..., CRITICAL, defined in the .h), it does the
 *    following:
 *
 *    - it fetch the zmq socket from a thread specific data: each business code
 *    thread might log, and since sharing zmq socket is plain wrong, each thread should
 *    have its own zmq socket.
 *    - then it performs the log by sending data over the data channel.
 *
 * 4. bxilog_finalize() just send the exit msg through the controller channel to the IHT.
 *
 *
 * TRICKY Stuff: Forking
 *
 *      Support for the fork() is tricky as in any multi-threaded applications. Since
 *      threads are not forked within the child, and since zeromq does not like
 *      fork() either, the best stuff is to:
 *      1. prepare for the fork (in the parent therefore) by cleaning the bxilog
 *          -> we call _finalize() for this purpose.
 *
 *      2. After the fork, we have 2 cases
 *          a. in the parent: restart in the same state as we were before
 *              (calling _init() if required).
 *          b. in the child: the lib is in the FINALIZED state and remains as such
 *             unless bxilog_init() is called.
 *
 *
 * TRICKY Stuff: Signals
 *
 *      Handling UNIX signals is tricky as in any multi-threaded applications. Two cases:
 *
 *      - Inside the IHT: first, the IHT should not receive asynchronous signals
 *        (such as SIGINT, SIGTERM, SIGQUIT and so on). For other signals (SIGSEGV, ...)
 *        it includes the signals in its poll loops. If such a signal is received,
 *        it prints a log, flush and reraise the signal with the default signal handler.
 *
 *      - Outside the IHT: We want a backtrace
 *      to be in the log, therefore we set a signal handler that
 *      performs such a log. However, since after a SIGSEGV the best thing to do is
 *      to quit, and since exiting without calling bxilog_finalize(true) will lost messages,
 *      the end result will be that the log produced by the SIGSEGV handling won't be
 *      seen at all in the log. The issue is to make sure the IHT flushes all logs before
 *      it exits. Therefore, the sighandler, ask for the termination of the IHT
 *      (which flushes the logs as far as it can), it waits some time and then
 *      exit (raising the original signal with the default signal handler).
 *
 * *************************************************************************************
 */


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************
#define REGISTERED_LOGGERS_DEFAULT_ARRAY_SIZE 64
#define DEFAULT_LOG_BUF_SIZE 128   // We use a 128 bytes per thread log buffer
                                   // by default expecting it is sufficient
                                   // for the vast majority of cases
                                   // If this is not the case, a specific
                                   // mallocated buffer will be used instead
#define DEFAULT_POLL_TIMEOUT 500   // Awake after this amount of milliseconds
                                   // if nothing has been received.

#define MAX_DEPTH_ERR 5            // Maximum number of chained errors before quiting

#define DATA_CHANNEL_URL "inproc://%d_data"
char * data_url = NULL;
#define CONTROL_CHANNEL_URL "inproc://%d_control"
char * control_url = NULL;
#define INTERNAL_LOGGER_NAME "bxilog"
#define IH_RCVHWM 1500000

#define READY_CTRL_MSG_REQ "BC->IH: ready?"
#define READY_CTRL_MSG_REP "IH->BC: ready!"
#define EXIT_CTRL_MSG_REQ "BC->IH: exit?"
//#define EXIT_CTRL_MSG_REP "BC->IH: exited!"  // No reply
#define FLUSH_CTRL_MSG_REQ "BC->IH: flush?"
#define FLUSH_CTRL_MSG_REP "IH->BC: flushed!"

// How many retries before falling back to SYNC sending.
#define RETRIES_MAX 3u
// Used when a sending failed with EAGAIN, we sleep for that amount of time (in ns).
#define RETRY_DELAY 500000l

# define IHT_LOGGER_NAME "bxilog.iht"
// WARNING: highly dependent on the log format
#define YEAR_SIZE 4
#define MONTH_SIZE 2
#define DAY_SIZE 2
#define HOUR_SIZE 2
#define MINUTE_SIZE 2
#define SECOND_SIZE 2
#define SUBSECOND_SIZE 9
#define PID_SIZE 5
#define TID_SIZE 5
#define THREAD_RANK_SIZE 5

// WARNING: highly dependent on the log format
#ifdef __linux__
// LOG_FMT "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u=%0*u:%s|%s:%d@%s|%s|%s\n";
// O|20140918T090752.472145261|11297.11302=01792:unit_t|unit_t.c:308@_dummy|bxiclib.test|msg

#define FIXED_LOG_SIZE 2 + YEAR_SIZE + MONTH_SIZE + DAY_SIZE +\
                       1 + HOUR_SIZE + MINUTE_SIZE + SECOND_SIZE + 1 + SUBSECOND_SIZE +\
                       1 + PID_SIZE + 1 + TID_SIZE + 1 + THREAD_RANK_SIZE + \
                       1 + 1 + 1 + 1 + 1 + 1 + 1  // Add all remaining fixed characters
                                                  // such as ':|:@||\n' in that order
#else
#define FIXED_LOG_SIZE 2 + YEAR_SIZE + MONTH_SIZE + DAY_SIZE +\
                       1 + HOUR_SIZE + MINUTE_SIZE + SECOND_SIZE + 1 + SUBSECOND_SIZE +\
                       1 + PID_SIZE + 1 + THREAD_RANK_SIZE + \
                       1 + 1 + 1 + 1 + 1 + 1 +1  // Add all remaining fixed characters
                                                 // such as ':|:@||\n' in that order
#endif

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

struct tsd_s {
    char * log_buf;                 // The per-thread log buffer
    void * log_channel;             // The thread-specific zmq logging socket;
    void * ctl_channel;             // The thread-specific zmq controlling socket;
#ifdef __linux__
    pid_t tid;                      // Cache the tid on Linux since we assume NPTL
                                    // and therefore a 1:1 thread implementation.
#endif
    uint16_t thread_rank;           // user thread rank
};

typedef struct tsd_s * tsd_p;

// The header of the log msg
// TODO: reorganize with most-often used data first
// see cachegrind results.
struct log_header_s {
    bxilog_level_e level;           // log level
    struct timespec detail_time;    // log timestamp
#ifdef __linux__
    pid_t tid;                      // kernel thread id
#endif
    uint16_t thread_rank;           // user thread rank
    int line_nb;                    // line nb
    size_t filename_len;            // file name length
    size_t funcname_len;            // function name length
    size_t logname_len;             // logger name length
    size_t variable_len;            // filename_len + funcname_len +
                                    // logname_len
    size_t logmsg_len;              // the logmsg length
};


/*
 * Finite State Machine.
 *
 * Normal path:
 * UNSET --> _init() -> INITIALIZING -> INITIALIZED - _finalize() -> FINALIZING -> FINALIZED
 * FINALIZED -> _finalize() -> FINALIZED
 * Support for fork() makes special transitions:
 * (UNSET, FINALIZED) --> fork() --> (UNSET, FINALIZED) // No change
 * (INITIALIZING, FINALIZING) --> fork() --> ILLEGAL    // Can't fork while initializing
 *                                                      // or finalizing
 * INITIALIZED --> fork()
 * Parent: _parent_before_fork() --> FINALIZING --> FINALIZED --> FORKED
 *         _parent_after_fork(): FORKED --> _init() --> INITIALIZING --> INITIALIZED
 * Child:  _child_after_fork(): FORKED --> FINALIZED
 */
typedef enum {
    UNSET, INITIALIZING, INITIALIZED, FINALIZING, FINALIZED, ILLEGAL, FORKED,
} bxilog_state_e;

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
//--------------------------------- Generic Helpers --------------------------------
static void _display_err_msg(char* msg);
static void _configure(bxilog_p logger);
static void _wipeout();
static bxierr_p _send2iht(const bxilog_p logger, const bxilog_level_e level,
                          void * log_channel,
#ifdef __linux__
                          pid_t tid,
#endif
                          uint16_t thread_rank,
                          char * filename, size_t filename_len,
                          const char * funcname, size_t funcname_len,
                          int line,
                          const char * rawstr, size_t rawstr_len);
//--------------------------------- Thread Specific Data helpers -------------------
static void _tsd_free(void * const data);
static void _tsd_key_new();
static bxierr_p _get_tsd(tsd_p * tsd_p);

//--------------------------------- Initializer/Finalizer helpers -------------------
static bxierr_p _init(void);
static bxierr_p _finalize(void);
static bxierr_p _end_iht(void);
static bxierr_p _join_iht(bxierr_p * iht_err_p);
static bxierr_p _cleanup(void);
//--------------------------------- IHT Helpers ------------------------------------
static void * _iht_main(void * param);
static bool _should_quit(bxierr_p * err);
static bxierr_p _create_iht();
static void _log_single_line(const struct log_header_s * header,
                             const char * filename,
                             const char * funcname,
                             const char *loggername,
                             const char * line,
                             size_t line_len);
static bxierr_p _process_data(void *);
static bxierr_p _process_ctrl_msg(void * ctrl_channel, void * data_channel);
static const char * _basename(const char *, size_t);
static bxierr_p _flush_iht(void * data_channel);
static bxierr_p _get_file_fd(int * fd);
static ssize_t _mkmsg(size_t n, char buf[n],
                      char level,
                      const struct timespec * detail_time,
#ifdef __linux__
                      pid_t tid,
#endif
                      uint16_t thread_rank,
                      const char * filename,
                      int line_nb,
                      const char * funcname,
                      const char * loggername,
                      const char * logmsg);
static bxierr_p _sync();

//---------------------------------- Signal handlers --------------------------------
static void _sig_handler(int signum, siginfo_t * siginfo, void * dummy);

//---------------------------------- Fork handlers ----------------------------------
static void _install_fork_handlers(void);
static void _parent_before_fork(void);
static void _parent_after_fork(void);
static void _child_after_fork(void);

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The internal logger
SET_LOGGER(BXILOG_INTERNAL_LOGGER, "bxibase.log");

char * _LOG_LEVEL_NAMES[] = {
                             "panic",
                             "alert",
                             "critical",
                             "error",
                             "warning",
                             "notice",
                             "output",
                             "info",
                             "debug",
                             "fine",
                             "trace",
                             "lowest"
};

/**
 * Initial number of registered loggers array size
 */
static size_t REGISTERED_LOGGERS_ARRAY_SIZE = REGISTERED_LOGGERS_DEFAULT_ARRAY_SIZE;
/**
 * The array of registered loggers.
 */
static bxilog_p * REGISTERED_LOGGERS = NULL;
/**
 * Number of registered loggers.
 */
static size_t REGISTERED_LOGGERS_NB = 0;

/**
 * Number of items in the current configuration.
 */
static size_t BXILOG_CFG_ITEMS_NB = 0;

/**
 * Current configuration.
 */
static bxilog_cfg_item_s * BXILOG_CFG_ITEMS = NULL;

// The various log levels specific characters
static const char LOG_LEVEL_STR[] = { 'P', 'A', 'C', 'E', 'W', 'N', 'O', 'I', 'D', 'F', 'T', 'L'};
// The log format used by the IHT when writing
// WARNING: If you change this format, change also different #define above
// along with FIXED_LOG_SIZE
#ifdef __linux__
static const char LOG_FMT[] = "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u=%0*u:%s|%s:%d@%s|%s|%s\n";
#else
static const char LOG_FMT[] = "%c|%0*d%0*d%0*dT%0*d%0*d%0*d.%0*ld|%0*u.%0*u:%s|%s:%d@%s|%s|%s\n";
#endif

// The zmq context used for socket creation.
static void * BXILOG_CONTEXT = NULL;

static pthread_mutex_t BXILOG_INITIALIZED_MUTEX = PTHREAD_MUTEX_INITIALIZER;

static const char * PROGNAME = NULL;
static size_t PROGNAME_LEN = ARRAYLEN(PROGNAME);
static const char * FILENAME = NULL;
static size_t FILENAME_LEN = ARRAYLEN(FILENAME);

static pthread_t INTERNAL_HANDLER_THREAD;
static bxilog_state_e STATE = UNSET;
static pid_t PID;
static struct {
    int fd;                         // the file descriptor where log must be produced
#ifdef __linux__
    pid_t tid;                      // the IHT pid
#endif
    uint16_t rank;                  // the IHT rank
    size_t lost_logs;               // Number of lost logs
    size_t reported_errors_len;     // Number of errors reported
    bxierr_p * reported_errors;     // The list of reported errors
} IHT_DATA;


/*
 * Since zeromq implies that zeromq sockets should not be shared,
 * we use thread-specific data to holds thread specific sockets,
 */

/* Key for the thread-specific data (tsd) */
static pthread_key_t TSD_KEY;

/* Once-only initialisation of the tsd */
static pthread_once_t TSD_KEY_ONCE = PTHREAD_ONCE_INIT;

/* For signals handler */
volatile sig_atomic_t FATAL_ERROR_IN_PROGRESS = 0;

/* Once-only initialisation of the pthread_atfork() */
static pthread_once_t ATFORK_ONCE = PTHREAD_ONCE_INIT;

static pthread_mutex_t REGISTER_LOCK = PTHREAD_MUTEX_INITIALIZER;

bxierr_define(_IHT_EXIT_ERR_, 333, "Special error message");
//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


void bxilog_register(bxilog_p logger) {
    assert(logger != NULL);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    assert(0 == rc);
    if (REGISTERED_LOGGERS == NULL) {
        size_t bytes = REGISTERED_LOGGERS_ARRAY_SIZE * sizeof(*REGISTERED_LOGGERS);
        REGISTERED_LOGGERS = bximem_calloc(bytes);
        int rc = atexit(_wipeout);
        assert(0 == rc);
    } else if (REGISTERED_LOGGERS_NB >= REGISTERED_LOGGERS_ARRAY_SIZE) {
        size_t old_size = REGISTERED_LOGGERS_ARRAY_SIZE;
        REGISTERED_LOGGERS_ARRAY_SIZE *= 2;
        size_t bytes = REGISTERED_LOGGERS_ARRAY_SIZE * sizeof(*REGISTERED_LOGGERS);
        REGISTERED_LOGGERS = bximem_realloc(REGISTERED_LOGGERS, bytes);
        // Set to NULL all new slots.
        for (size_t i = old_size; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
            REGISTERED_LOGGERS[i] = NULL;
        }
//        fprintf(stderr, "[I] Reallocation of %zu slots for (currently) "
//                "%zu registered loggers\n", REGISTERED_LOGGERS_ARRAY_SIZE,
//                REGISTERED_LOGGERS_NB);
    }
    size_t slot = REGISTERED_LOGGERS_NB;
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        // Find next free slot
        if (NULL == REGISTERED_LOGGERS[i]) {
            if (slot == REGISTERED_LOGGERS_NB) slot = i;
        } else if (0 == strcmp(REGISTERED_LOGGERS[i]->name, logger->name)) {
//            fprintf(stderr,
//                    "[W] Registered loggers[%zu] share same "
//                    "logger name than loggers[%zu]: %s\n",
//                    i, slot, logger->name);
        }
    }
//    fprintf(stderr, "Registering new logger[%zu]: %s\n", slot, logger->name);
    REGISTERED_LOGGERS[slot] = logger;
    REGISTERED_LOGGERS_NB++;
    _configure(logger);
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    assert(0 == rc);
}

void bxilog_unregister(bxilog_p logger) {
    assert(logger != NULL);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    assert(0 == rc);
    bool found = false;
//    fprintf(stderr, "Nb Registered loggers: %zu\n", REGISTERED_LOGGERS_NB);
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        bxilog_p current = REGISTERED_LOGGERS[i];
        if (NULL == current) continue;
//        fprintf(stderr, "Logger[%zu]: %s\n", i, current->name);
        if (current == logger) {
//            fprintf(stderr, "Unregistering loggers[%zu]: %s\n", i, current->name);
            REGISTERED_LOGGERS[i] = NULL;
            found = true;
        }
    }
    if (!found) {
//        bxierr_p bxierr = bxierr_gen("[W] Can't find registered logger: %s\n",
//                                     logger->name);
//        char * str = bxierr_str(bxierr);
//        _display_err_msg(str);
//        BXIFREE(str);
    } else REGISTERED_LOGGERS_NB--;

    if (0 == REGISTERED_LOGGERS_NB) {
        BXIFREE(REGISTERED_LOGGERS);
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    assert(0 == rc);
}

size_t bxilog_get_registered(bxilog_p *loggers[]) {
    assert(loggers != NULL);
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    assert(0 == rc);
    bxilog_p * result = bximem_calloc(REGISTERED_LOGGERS_NB * sizeof(*result));
    size_t j = 0;
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        if (NULL == REGISTERED_LOGGERS[i]) continue;
        result[j++] = REGISTERED_LOGGERS[i];
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    assert(0 == rc);
    *loggers = result;
    return j;
}

bxierr_p bxilog_cfg_registered(size_t n, bxilog_cfg_item_s cfg[n]) {
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    assert(0 == rc);
    if (NULL != BXILOG_CFG_ITEMS) {
        BXIFREE(BXILOG_CFG_ITEMS);
    }
    BXILOG_CFG_ITEMS = bximem_calloc(n * sizeof(*BXILOG_CFG_ITEMS));
    for (size_t i = 0; i < n; i++) {
        BXILOG_CFG_ITEMS[i].level = cfg[i].level;
        BXILOG_CFG_ITEMS[i].prefix = strdup(cfg[i].prefix);
    }
    BXILOG_CFG_ITEMS_NB = n;

    // TODO: O(n*m) n=len(cfg) and m=len(loggers) -> be more efficient!
//    fprintf(stderr, "Number of cfg items: %zd\n", n);
    for (size_t l = 0; l < REGISTERED_LOGGERS_ARRAY_SIZE; l++) {
        bxilog_p logger = REGISTERED_LOGGERS[l];
        if (NULL != logger) _configure(logger);
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    assert(0 == rc);
    return BXIERR_OK;
}

bxierr_p bxilog_get_level_from_str(char * level_str, bxilog_level_e *level) {
    if (0 == strcasecmp("panic", level_str)
            || 0 == strcasecmp("emergency", level_str)) {
        *level = BXILOG_PANIC;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("alert", level_str)) {
        *level = BXILOG_ALERT;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("critical", level_str)
            || 0 == strcasecmp("crit", level_str)) {
        *level = BXILOG_CRIT;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("error", level_str)
            || 0 == strcasecmp("err", level_str)) {
        *level = BXILOG_ERR;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("warning", level_str)
            || 0 == strcasecmp("warn", level_str)) {
        *level = BXILOG_WARN;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("notice", level_str)) {
        *level = BXILOG_NOTICE;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("output", level_str)
            || 0 == strcasecmp("out", level_str)) {
        *level = BXILOG_OUTPUT;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("info", level_str)) {
        *level = BXILOG_INFO;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("debug", level_str)) {
        *level = BXILOG_DEBUG;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("fine", level_str)) {
        *level = BXILOG_FINE;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("trace", level_str)) {
        *level = BXILOG_TRACE;
        return BXIERR_OK;
    }
    if (0 == strcasecmp("lowest", level_str)) {
        *level = BXILOG_LOWEST;
        return BXIERR_OK;
    }
    *level = BXILOG_LOWEST;
    return bxierr_gen("Bad log level name: %s", level_str);
}

size_t bxilog_get_all_level_names(char *** names) {
    *names = _LOG_LEVEL_NAMES;
    return ARRAYLEN(_LOG_LEVEL_NAMES);
}


bxierr_p bxilog_init(const char * const progname, const char * const fn) {
    bxierr_p err = BXIERR_OK;
    int rc = pthread_mutex_lock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) return bxierr_fromidx(rc, NULL,
                                       "Calling pthread_mutex_lock() failed (rc=%d)", rc);

    if (INITIALIZED == STATE) {
        DEBUG(BXILOG_INTERNAL_LOGGER, "Initialization already done. Skipping.");
        goto UNLOCK;
    }
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if(UNSET != STATE && FINALIZED != STATE) {
        err = bxierr_new(BXILOG_ILLEGAL_STATE_ERR,
                         NULL, NULL, NULL,
                         "Illegal state: %d",
                         STATE);
        goto UNLOCK;
    }

    if (NULL == fn) {
        err = bxierr_new(BXILOG_CONFIG_ERR,
                         NULL, NULL, NULL,
                         "Bad configuration");
        goto UNLOCK;
    }

    // Copy parameters.
    FILENAME = strdup(fn);
    FILENAME_LEN = strlen(FILENAME);
    PROGNAME = strdup(progname);
    PROGNAME_LEN = strlen(PROGNAME) + 1; // Include the NULL byte

    err = _init();
    if (bxierr_isko(err)) {
        goto UNLOCK;
    }
    assert(INITIALIZING == STATE);

    // Install the fork handler once only
    rc = pthread_once(&ATFORK_ONCE, _install_fork_handlers);
    if (0 != rc) {
        err = bxierr_fromidx(rc, NULL,
                             "Calling pthread_once() failed (rc = %d)", rc);
        goto UNLOCK;
    }

    /* now the log library is initialized */
    STATE = INITIALIZED;
    DEBUG(BXILOG_INTERNAL_LOGGER, "Initialization done");
UNLOCK:
    rc = pthread_mutex_unlock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        STATE = ILLEGAL;
        bxierr_p err2 =  bxierr_fromidx(rc, NULL,
                                        "Calling pthread_mutex_unlock() failed (rc=%d)",
                                        rc);
        BXIERR_CHAIN(err, err2);
    }

    return err;
}

bxierr_p bxilog_finalize(bool flush) {
    bxierr_p err = BXIERR_OK;
    int rc = pthread_mutex_lock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        err = bxierr_fromidx(rc, NULL,
                             "Calling pthread_mutex_lock() failed (rc=%d)", rc);
        goto UNLOCK;
    }

    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (FINALIZED == STATE) {
        err = BXIERR_OK;
        goto UNLOCK;
    }
    if (STATE != INITIALIZED) {
        err = bxierr_new(BXILOG_ILLEGAL_STATE_ERR,
                         NULL, NULL, NULL,
                         "Illegal state: %d", STATE);
        goto UNLOCK;
    }
    DEBUG(BXILOG_INTERNAL_LOGGER, "Exiting bxilog");
    bxierr_p err2;
    if (flush) {
        err2 = bxilog_flush();
        BXIERR_CHAIN(err, err2);
    }

    err2 = _finalize();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        goto UNLOCK;
    }
    assert(FINALIZING == STATE);
    STATE = FINALIZED;
UNLOCK:
    rc = pthread_mutex_unlock(&BXILOG_INITIALIZED_MUTEX);
    if (0 != rc) {
        STATE = ILLEGAL;
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_mutex_unlock() failed (rc=%d)",
                              rc);
        BXIERR_CHAIN(err, err2);
    }
    BXIFREE(FILENAME);
    BXIFREE(PROGNAME);

    return err;
}

bxierr_p bxilog_get(const char * logger_name, bxilog_p * result) {
    *result = NULL;
    int rc = pthread_mutex_lock(&REGISTER_LOCK);
    if (0 != rc) return bxierr_errno("Call to pthread_mutex_lock() failed (rc=%d)", rc);
    for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
        bxilog_p logger = REGISTERED_LOGGERS[i];
        if (NULL == logger) continue;
        if (strcmp(logger->name, logger_name) == 0) {
            *result = logger;
        }
    }
    rc = pthread_mutex_unlock(&REGISTER_LOCK);
    if (0 != rc) return bxierr_errno("Call to pthread_mutex_unlock() failed (rc=%d)", rc);
    if (NULL == *result) { // Not found
        bxilog_p self = bximem_calloc(sizeof(*self));
        self->allocated = true;
        self->name = strdup(logger_name);
        self->name_length = strlen(logger_name) + 1;
        self->level = BXILOG_LOWEST;
        bxilog_register(self);
        *result = self;
    }
//    fprintf(stderr, "Returning %s %d\n", (*result)->name, (*result)->level);
    return BXIERR_OK;
}

void bxilog_destroy(bxilog_p * self_p) {
    if (NULL == *self_p) return;
    if (!(*self_p)->allocated) return;

    BXIFREE((*self_p)->name);
    BXIFREE(*self_p);
}


bxierr_p bxilog_flush(void) {
    if (INITIALIZED != STATE) return BXIERR_OK;
    DEBUG(BXILOG_INTERNAL_LOGGER, "Requesting a flush()");
    tsd_p tsd = NULL;
    bxierr_p err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;
    void * ctl_channel = tsd->ctl_channel;
    err = bxizmq_str_snd(FLUSH_CTRL_MSG_REQ, ctl_channel, 0, 0, 0);
    if (bxierr_isko(err)) return err;
    char * reply;
    err = bxizmq_str_rcv(ctl_channel, 0, false, &reply);
    if (bxierr_isko(err)) return err;
    // Warning, no not introduce recursive call here (using ERROR() for example
    // or any log message: we are currently flushing!
    if (0 != strcmp(FLUSH_CTRL_MSG_REP, reply)) {
        return bxierr_new(BXILOG_IHT2BC_PROTO_ERR, NULL, NULL, NULL,
                          "Wrong message received in reply to %s: %s. Expecting: %s",
                          FLUSH_CTRL_MSG_REQ, reply, FLUSH_CTRL_MSG_REP);
    }
    DEBUG(BXILOG_INTERNAL_LOGGER, "flush() done");
    BXIFREE(reply);
    return BXIERR_OK;
}

bxierr_p bxilog_log_rawstr(const bxilog_p logger, const bxilog_level_e level,
                           char * filename, size_t filename_len,
                           const char * funcname, size_t funcname_len,
                           int line,
                           const char * rawstr, size_t rawstr_len) {
    if (INITIALIZED != STATE) return BXIERR_OK;
    tsd_p tsd;
    bxierr_p err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;

    err = _send2iht(logger, level, tsd->log_channel,
#ifdef __linux__
                    tsd->tid,
#endif
                    tsd->thread_rank,
                    filename, filename_len,
                    funcname, funcname_len,
                    line,
                    rawstr, rawstr_len);
    return err;
}

bxierr_p bxilog_vlog_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                  char * filename, size_t filename_len,
                                  const char * funcname, size_t funcname_len,
                                  const int line,
                                  const char * fmt, va_list arglist) {

    if (INITIALIZED != STATE) return BXIERR_OK;

    tsd_p tsd;
    bxierr_p err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;

    // Start creating the logmsg so the length can be computed
    // We start in the thread local buffer

    char * logmsg = tsd->log_buf;
    size_t logmsg_len = DEFAULT_LOG_BUF_SIZE;
    bool logmsg_allocated = false; // When true,  means that a new special buffer has been
                                   // allocated -> it will have to be freed
    while(true) {
        va_list arglist_copy;
        va_copy(arglist_copy, arglist);
        // Does not include the null terminated byte
        int n = vsnprintf(logmsg, logmsg_len, fmt, arglist_copy);
        va_end(arglist_copy);

        // Check error
        assert(n >= 0);

        // Ok
        if ((size_t) n < logmsg_len) {
            logmsg_len = (size_t) n + 1; // Record the actual size of the message
            break;
        }

        // Not enough space, mallocate a new special buffer of the precise size
        logmsg_len = (size_t) (n + 1);
        logmsg = malloc(logmsg_len); // Include the null terminated byte
        assert(NULL != logmsg);
        logmsg_allocated = true;

        // Warning: recursive call!
        LOWEST(BXILOG_INTERNAL_LOGGER,
              "Not enough space to log inside tsd log_buf (%d < %zu),"
              " mallocating a new one",
              DEFAULT_LOG_BUF_SIZE, logmsg_len);
    }

    err = _send2iht(logger, level, tsd->log_channel,
#ifdef __linux__
                    tsd->tid,
#endif
                    tsd->thread_rank,
                    filename, filename_len,
                    funcname, funcname_len,
                    line,
                    logmsg, logmsg_len);

    if (logmsg_allocated) BXIFREE(logmsg);
    // Either header comes from the stack
    // or it comes from a mallocated region that will be freed
    // by bxizmq_snd_data_zc itself
    // Therefore it should not be freed here
    // FREE(header);
    return err;
}

bxierr_p bxilog_log_nolevelcheck(const bxilog_p logger, const bxilog_level_e level,
                                 char * filename, size_t filename_len,
                                 const char * funcname, size_t funcname_len,
                                 const int line,
                                 const char * fmt, ...) {
    va_list ap;
    bxierr_p err;

    va_start(ap, fmt);
    err = bxilog_vlog_nolevelcheck(logger, level,
                                   filename, filename_len, funcname, funcname_len, line,
                                   fmt, ap);
    va_end(ap);

    return err;
}


bxilog_level_e bxilog_get_level(const bxilog_p logger) {
    assert(NULL != logger);
    return logger->level;
}

void bxilog_set_level(const bxilog_p logger, const bxilog_level_e level) {
    assert(NULL != logger);
    assert(BXILOG_LOWEST >= level);
    logger->level = level;
}

// Defined inline in bxilog.h
extern bool bxilog_is_enabled_for(const bxilog_p logger, const bxilog_level_e level);

void bxilog_exit(int exit_code,
                 bxierr_p err,
                 bxilog_p logger,
                 bxilog_level_e level,
                 char * file,
                 size_t filelen,
                 const char * func,
                 size_t funclen,
                 int line) {

    assert(NULL != logger);
    char * str = bxierr_str_limit(err, BXIERR_ALL_CAUSES);
    bxilog_log(logger, level,
               file, filelen,
               func, funclen,
               line,
               "Exiting with code %d, Error is: %s", exit_code, str);
    bxierr_destroy(&err);
    err = bxitime_sleep(CLOCK_MONOTONIC, 0, 50000000);
    bxierr_destroy(&err);
    err = bxilog_flush();
    bxierr_destroy(&err);
    exit((exit_code));
}

void bxilog_assert(bxilog_p logger, bool result,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line, char * expr) {
    if (!result) {
        bxierr_p err = bxierr_new(BXIASSERT_CODE,
                                  NULL,
                                  NULL,
                                  NULL,
                                  "From file %s:%d: assertion %s is false"\
                                  BXIBUG_STD_MSG,
                                  file, line, expr);
        bxilog_exit(EX_SOFTWARE, err, logger, BXILOG_CRITICAL,
                file, filelen, func, funclen, line);
        bxierr_destroy(&err);
    }
}

void bxilog_report(bxilog_p logger, bxilog_level_e level, bxierr_p err,
                   char * file, size_t filelen,
                   const char * func, size_t funclen,
                   int line,
                   const char * fmt, ...) {

    if (bxilog_is_enabled_for(logger, level) && bxierr_isko(err)) {
        va_list ap;
        va_start(ap, fmt);
        char * msg;
        bxistr_vnew(&msg, fmt, ap);
        va_end(ap);
        char * err_str = bxierr_str(err);
        bxierr_p logerr = bxilog_log_nolevelcheck(logger, level,
                                                  file, filelen,
                                                  func, funclen, line,
                                                  "%s: %s", msg, err_str);
        BXIFREE(msg);
        BXIFREE(err_str);
        bxierr_destroy(&err);
        bxierr_report(logerr, STDERR_FILENO);
    }
}

// ----------------------------------- Signals -----------------------------------------
// Synchronous signals produces a log and kill the initializer thread
// Asynchronous signals should be handled by the initializer thread
// and this thread is the only one allowed to call bxilog_finalize(true)
bxierr_p bxilog_install_sighandler(void) {
    // Allocate a special signal stack for SIGSEGV and the like
    stack_t sigstack;
    sigstack.ss_sp = bximem_calloc(SIGSTKSZ);
    sigstack.ss_size = SIGSTKSZ;
    sigstack.ss_flags = 0;
    errno = 0;
    int rc = sigaltstack(&sigstack, NULL);
    if (-1 == rc) return bxierr_errno("Calling sigaltstack() failed");
    FINE(BXILOG_INTERNAL_LOGGER,
         "Alternate signal stack set at %p (%zu B)",
         sigstack.ss_sp,
         sigstack.ss_size);

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));

    DEBUG(BXILOG_INTERNAL_LOGGER, "Setting signal handler process wide");
    // Keep default action for SIGQUIT so we can bypass bxilog specific
    // signal handling
    int allsig_num[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGINT, SIGTERM};
    // Mask all signals during the execution of a signal handler.
    sigset_t allsig_blocked;
    bxilog_sigset_new(&allsig_blocked, allsig_num, ARRAYLEN(allsig_num));

    sa.sa_sigaction = _sig_handler;
    sa.sa_mask = allsig_blocked;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    // Install signal handlers
    for (size_t i = 0; i < ARRAYLEN(allsig_num); i++) {
        errno = 0;
        int rc = sigaction(allsig_num[i], &sa, NULL);
        if (0 != rc) return bxierr_errno("Calling sigaction() failed for signum %d",
                                         allsig_num[i]);
        char * str = strsignal(allsig_num[i]);
        DEBUG(BXILOG_INTERNAL_LOGGER,
              "Signal handler set for %d: %s", allsig_num[i], str);
        // Do not BXIFREE(str) since it is statically allocated as specified in the manual.
        // BXIFREE(str);
    }
    INFO(BXILOG_INTERNAL_LOGGER, "Signal handlers set");
    return BXIERR_OK;
}

bxierr_p bxilog_sigset_new(sigset_t *sigset, int * signum, size_t n) {
    assert(NULL != sigset && NULL != signum);
    bxierr_p err = BXIERR_OK, err2;
    int rc = sigemptyset(sigset);
    if (0 != rc) {
        err2 = bxierr_errno("Calling sigemptyset(%p) failed", sigset);
        BXIERR_CHAIN(err, err2);
    }
    for (size_t i = 0; i < n; i++) {
       rc = sigaddset(sigset, signum[i]);
       if (0 != rc) {
           err2 = bxierr_errno("Calling sigaddset() with signum='%d' failed", signum[i]);
           BXIERR_CHAIN(err, err2);
       }
    }
    return err;
}

char * bxilog_signal_str(const siginfo_t * siginfo,
                         const struct signalfd_siginfo * sfdinfo) {

    assert(siginfo != NULL || sfdinfo != NULL);

    int signum = (NULL == siginfo) ? (int)sfdinfo->ssi_signo: siginfo->si_signo;
    char * sigstr = strsignal(signum);
    assert(NULL != sigstr);
    int code = (NULL == siginfo) ? (int) sfdinfo->ssi_code: siginfo->si_code;
    pid_t pid = (NULL == siginfo) ? (pid_t) sfdinfo->ssi_pid : siginfo->si_pid;
    uid_t uid = (NULL == siginfo) ? (uid_t) sfdinfo->ssi_uid : siginfo->si_uid;

    switch(signum) {
    case SIGTERM:
    case SIGINT:
        if (SI_USER == code) {
            return bxistr_new("Signal=%d ('%s'), Sender PID:UID='%d:%u'",
                              signum, sigstr, pid, uid);
        }
        if (SI_KERNEL == code) {
            return bxistr_new("Signal=%d ('%s'), Sender=KERNEL",
                                        signum, sigstr);
        }
        return bxistr_new("Signal=%d ('%s'), Sender=Unknown",
                                    signum, sigstr);
    case SIGABRT:
    case SIGILL:
    case SIGFPE:
    case SIGSEGV:
    case SIGBUS:
        return bxistr_new("Signal=%d ('%s'), Signal Code=%d (man 2 sigaction)",
                          signum, sigstr, code);
    default:
        return bxistr_new("Signal=%d ('%s'), This should not happen!",
                          signum, sigstr);
    }
}


bxierr_p bxilog_set_thread_rank(uint16_t rank) {
    tsd_p tsd;
    bxierr_p err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;
    tsd->thread_rank = rank;
    return BXIERR_OK;
}

bxierr_p bxilog_get_thread_rank(uint16_t * rank_p) {
    tsd_p tsd;
    bxierr_p err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;
    *rank_p = tsd->thread_rank;
    return BXIERR_OK;
}

void bxilog_display_loggers() {
    char ** level_names;
    size_t n = bxilog_get_all_level_names(&level_names);
    fprintf(stderr, "Log level names:\n");
    while(n-- > 0) {
        fprintf(stderr, "\t%zu\t = %s\n", n, level_names[n]);
    }
    bxilog_p * loggers;
    n = bxilog_get_registered(&loggers);
    fprintf(stderr, "Loggers name:\n");
    while(n-- > 0) {
        fprintf(stderr, "\t%s\n", loggers[n]->name);
    }
}

bxierr_p bxilog_parse_levels(char * str) {
    BXIASSERT(BXILOG_INTERNAL_LOGGER, NULL != str);
    str = strdup(str); // Required because str might not be allocated on the heap
    char *saveptr1 = NULL;
    char *str1 = str;
    size_t cfg_items_nb = 20;
    bxilog_cfg_item_s * cfg_items = bximem_calloc(cfg_items_nb * sizeof(*cfg_items));
    size_t next_idx = 0;
    for (size_t j = 0; ; j++, str1 = NULL) {
        char * token = strtok_r(str1, ",", &saveptr1);
        if (NULL == token) break;
        char * sep = strchr(token, ':');
        if (sep == NULL) {
            return bxierr_errno("Expected ':' in log level configuration: %s", token);
        }

        *sep = '\0';
        char * prefix = token;
        char * level_str = sep + 1;
        bxilog_level_e level;
        unsigned long tmp;
        errno = 0;

        char * endptr;
        tmp = strtoul(level_str, &endptr, 10);
        if (0 != errno) return bxierr_errno("Error while parsing number: '%s'", level_str);
        if (endptr == level_str) {
            bxierr_p err = bxilog_get_level_from_str(level_str, &level);
            if (bxierr_isko(err)) return err;
        } else if (*endptr != '\0') {
            return bxierr_errno("Error while parsing number: '%s' doesn't contain only a number or a level", level_str);
        } else {
            if (tmp > BXILOG_LOWEST) {
                DEBUG(BXILOG_INTERNAL_LOGGER,
                      "Bad log level specified: %lu, using %d instead",
                      tmp, BXILOG_LOWEST);
                tmp = BXILOG_LOWEST;
            }
            level = (bxilog_level_e) tmp;
        }
        cfg_items[next_idx].prefix = strdup(prefix);
        cfg_items[next_idx].level = level;
        next_idx++;
        if (next_idx >= cfg_items_nb) {
            cfg_items_nb *= 2;
            cfg_items = bximem_realloc(cfg_items, cfg_items_nb);
        }
    }

    bxierr_p err = bxilog_cfg_registered(next_idx, cfg_items);
    BXIASSERT(BXILOG_INTERNAL_LOGGER, bxierr_isok(err));
//    for (size_t i = 0; i < next_idx; i++) BXIFREE(cfg_items[i].prefix);
//    BXIFREE(cfg_items);
    BXIFREE(str);
    return BXIERR_OK;
}

//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

void _configure(bxilog_p logger) {
    assert(NULL != logger);

    for (size_t i = 0; i < BXILOG_CFG_ITEMS_NB; i++) {
        if (NULL == BXILOG_CFG_ITEMS[i].prefix) continue;

        size_t len = strlen(BXILOG_CFG_ITEMS[i].prefix);
        if (0 == strncmp(BXILOG_CFG_ITEMS[i].prefix, logger->name, len)) {
            // We have a prefix, configure it
//            fprintf(stderr, "Prefix: '%s' match logger '%s': level: %d -> %d\n",
//                    BXILOG_CFG_ITEMS[i].prefix, logger->name,
//                    logger->level, BXILOG_CFG_ITEMS[i].level);
            logger->level = BXILOG_CFG_ITEMS[i].level;
        }
//        else {
//            fprintf(stderr, "Mismatch: %s %s\n", BXILOG_CFG_ITEMS[i].prefix, logger->name);
//        }
    }
}

void _wipeout() {
    // Remove allocated configuration
    if (NULL != BXILOG_CFG_ITEMS) {
//        fprintf(stderr, "[I] Removing config\n");
        BXIFREE(BXILOG_CFG_ITEMS);
    }

    // Remove allocated loggers
    if (NULL != REGISTERED_LOGGERS) {
//        fprintf(stderr, "Nb Registered loggers: %zu\n", REGISTERED_LOGGERS_NB);
        for (size_t i = 0; i < REGISTERED_LOGGERS_ARRAY_SIZE; i++) {
            if (NULL == REGISTERED_LOGGERS[i]) {
                continue;
            }
//            fprintf(stderr, "loggers[%zu]: %s\n", i, REGISTERED_LOGGERS[i]->name);
            if (REGISTERED_LOGGERS[i]->allocated) {
//                fprintf(stderr, "[I] Destroying %s\n", REGISTERED_LOGGERS[i]->name);
                bxilog_destroy(&REGISTERED_LOGGERS[i]);
            }
            REGISTERED_LOGGERS[i] = NULL;
            REGISTERED_LOGGERS_NB--;
        }
        assert(0 == REGISTERED_LOGGERS_NB);
//        fprintf(stderr, "[I] Removing registered loggers\n");
        BXIFREE(REGISTERED_LOGGERS);
        REGISTERED_LOGGERS_ARRAY_SIZE = 0;
    }

    // Remove mallocated signal stack
    stack_t sigstack;
    errno = 0;
    int rc = sigaltstack(NULL, &sigstack);
    assert(0 == rc);
    if (SS_ONSTACK == sigstack.ss_flags && NULL != sigstack.ss_sp) {
//        fprintf(stderr, "[I] Removing sigstack\n");
        BXIFREE(sigstack.ss_sp);
    }
}

bxierr_p _send2iht(const bxilog_p logger, const bxilog_level_e level,
                   void * log_channel,
#ifdef __linux__
               pid_t tid,
#endif
               uint16_t thread_rank,
               char * filename, size_t filename_len,
               const char * funcname, size_t funcname_len,
               int line,
               const char * rawstr, size_t rawstr_len) {

    struct log_header_s * header;
    char * data;

    size_t var_len = filename_len\
            + funcname_len\
            + logger->name_length;

    size_t data_len = sizeof(*header) + var_len + rawstr_len;

    // We need a mallocated buffer to prevent ZMQ from making its own copy
    // We use malloc() instead of calloc() for performance reason
    header = malloc(data_len);
    assert(NULL != header);
    // Fill the buffer
    header->level = level;
    bxierr_p err = bxitime_get(CLOCK_REALTIME, &header->detail_time);
    // TODO: be smarter here, if the time can't be fetched, just fake it!
    if (bxierr_isko(err)) return err;

#ifdef __linux__
    header->tid = tid;
#endif
    header->thread_rank = thread_rank;
    header->line_nb = line;
    header->filename_len = filename_len;
    header->funcname_len = funcname_len;
    header->logname_len = logger->name_length;
    header->variable_len = var_len;
    header->logmsg_len = rawstr_len;

    // Now copy the rest after the header
    data = (char *) header + sizeof(*header);
    memcpy(data, filename, filename_len);
    data += filename_len;
    memcpy(data, funcname, funcname_len);
    data += funcname_len;
    memcpy(data, logger->name, logger->name_length);
    data += logger->name_length;
    memcpy(data, rawstr, rawstr_len);

    // Send the frame
    // normal version if header comes from the stack 'buf'
    //    size_t retries = bxizmq_snd_data(header, data_len,
    //                                      _get_tsd()->zocket, ZMQ_DONTWAIT,
    //                                      RETRIES_MAX, RETRY_DELAY);

    // Zero-copy version (if header has been mallocated).
    err = bxizmq_data_snd_zc(header, data_len,
                             log_channel, ZMQ_DONTWAIT,
                             RETRIES_MAX, RETRY_DELAY,
                             bxizmq_data_free, NULL);
    if (bxierr_isko(err)) {
        if (BXIZMQ_RETRIES_MAX_ERR != err->code) return err;
        // Recursive call!
        WARNING(BXILOG_INTERNAL_LOGGER,
                "Sending last log required %lu retries", (long) err->data);
        bxierr_destroy(&err);
    }
    return BXIERR_OK;
}

// ---------------------------------- Thread Specific Data Helpers ---------------------

/* Free the thread-specific data */
void _tsd_free(void * const data) {
    const tsd_p tsd = (tsd_p) data;

    if (NULL != tsd->log_channel) {
        bxierr_p err = BXIERR_OK, err2;
        err2 = bxizmq_zocket_destroy(tsd->log_channel);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_zocket_destroy(tsd->ctl_channel);
        BXIERR_CHAIN(err, err2);
        if (bxierr_isko(err)) bxierr_report(err, STDERR_FILENO);
    }
    BXIFREE(tsd->log_buf);
    BXIFREE(tsd);
}

/* Allocate the tsd key */
void _tsd_key_new() {
    int rc = pthread_key_create(&TSD_KEY, _tsd_free);
    assert(0 == rc);
}


/* Return the thread-specific data.*/
inline bxierr_p _get_tsd(tsd_p * result) {
    assert(result != NULL);
    // Initialize the tsdlog_key
    tsd_p tsd = pthread_getspecific(TSD_KEY);
    if (NULL != tsd) {
        *result = tsd;
        return BXIERR_OK;
    }
    errno = 0;
    tsd = bximem_calloc(sizeof(*tsd));
    assert(NULL != data_url);
    tsd->log_buf = bximem_calloc(DEFAULT_LOG_BUF_SIZE);
    bxierr_p err = bxizmq_zocket_connect(BXILOG_CONTEXT, ZMQ_PUSH, data_url, &tsd->log_channel);
    if (bxierr_isko(err)) {
        *result = tsd;
        return err;
    }
    err = bxizmq_zocket_connect(BXILOG_CONTEXT, ZMQ_REQ, control_url, &tsd->ctl_channel);
    if (bxierr_isko(err)) {
        *result = tsd;
        return err;
    }
#ifdef __linux__
    tsd->tid = (pid_t) syscall(SYS_gettid);
#endif
    // Do not try to return the kernel TID here, they might not be the same
    // Linux relies on a 1:1 mapping between kernel threads and user threads
    // but this is an NTPL implementation choice. Let's assume it might change
    // in the future.
    tsd->thread_rank = (uint16_t) pthread_self();
    int rc = pthread_setspecific(TSD_KEY, tsd);
    // Nothing to do otherwise. Using a log will add a recursive call... Bad.
    // And the man page specified that there is
    assert(0 == rc);

    *result = tsd;
    return BXIERR_OK;
}

//--------------------------------- IHT Helpers ------------------------------------
// The internal thread is responsible for processing messages.
// It has two zmq channels:
// DATA channel
// CTRL channel
void * _iht_main(void * param) {
    //TODO: remove
    (void) (param); // No use currently, prevent warning
    bxierr_p err = BXIERR_OK, err2;
    // Constants for the IHT
#ifdef __linux__
    IHT_DATA.tid = (pid_t) syscall(SYS_gettid);
#endif
    // TODO: find something better for the rank of the IHT
    // Maybe, define already the related string instead of
    // a rank number?
    IHT_DATA.rank = (uint16_t) pthread_self();
    IHT_DATA.lost_logs = 0;
    IHT_DATA.reported_errors_len = 4; // Maximum 4 different reported errors
    IHT_DATA.reported_errors = bximem_calloc(IHT_DATA.reported_errors_len * sizeof(*IHT_DATA.reported_errors));

    /******************** Signal handling *******************/
    // All signals are blocked in this thread, they must be dealt with
    // by other threads. Note: this is only true for asynchronous signals
    // such as SIGINT, SIGQUIT and so on...
    // In particular synchronous signals such as SIGSEGV, SIGBUS, and the like
    // are always sent to the thread that generated them. Therefore,
    // there is no real thing to do unless implementing a complex sigsetjmp/siglongjmp
    // in order to produce a log, flush the log and close the file.
    // However, this is complex too, since signal handlers is per process, not per-thread
    // Therefore, all threads will share same signal handler. Obviously, flushing and
    // closing the file is only relevant for the internal thread...
    // In the signal handler we therefore will have to distinguish between two cases:
    //     1. is it a business-code thread? -> use the normal signal handler
    //     2. is it an internal thread? -> use a specific signal handler
    // This is quite hard to do right. We currently prefer to avoid this complexities
    // and to ensure as far as possible that no SIGSEGV nor SIGBUS will be raised
    // in this thread. Careful programming is required therefore. ;-)
    sigset_t mask;
    int rc = sigfillset(&mask);
    assert(0 == rc);
    // block all signals.
    // Note: undefined behaviour for SIGBUS, SIGFPE, SIGILL and SIGSEGV
    // See sigprocmask(2)
    rc = pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if (-1 == rc) {
        err2 = bxierr_errno("Calling pthread_sigmask() failed");
        BXIERR_CHAIN(err, err2);
    }

    tzset(); // Should be called before invocation to localtime.
    int hwm = IH_RCVHWM;
    void * CONTROL_CHANNEL = NULL;
    void * DATA_CHANNEL = NULL;
    bxierr_p fatal_err = bxizmq_zocket_bind(BXILOG_CONTEXT,
                                            ZMQ_REP, control_url,
                                            NULL,
                                            &CONTROL_CHANNEL);
    if (bxierr_isko(fatal_err)) {
        // We can't communicate with main thread, there is nothing else to do
        // than exiting
        char * fatal_err_str = bxierr_str(fatal_err);
        error(EX_SOFTWARE, 0,
              "Can't create control socket - exiting. Reason: %s", fatal_err_str);
        BXIFREE(fatal_err_str);
        bxierr_destroy(&fatal_err);
        assert(false); // Unreachable statement!
    }

    err2 = _get_file_fd(&IHT_DATA.fd);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) {
        // Wait for the READY message and inform back
        // that we have an error on the given output file
        char * cmd;
        bxierr_p last_err = bxizmq_str_rcv(CONTROL_CHANNEL, 0, false, &cmd);
        assert(bxierr_isok(last_err) && NULL != cmd);
        assert(0 == strcmp(READY_CTRL_MSG_REQ, cmd));
        BXIFREE(cmd);
        // Send the error message
        char * err_str = bxierr_str(err);
        last_err = bxizmq_str_snd(err_str, CONTROL_CHANNEL, 0, 0, 0);
        BXIFREE(err_str);
        assert(bxierr_isok(last_err));
        bxierr_destroy(&last_err);
        goto QUIT;
    }


    errno = 0;
    err2 = bxizmq_zocket_bind(BXILOG_CONTEXT,
                             ZMQ_PULL, data_url,
                             NULL,
                             &DATA_CHANNEL);
    BXIERR_CHAIN(err, err2);
    err2 = bxizmq_zocket_setopt(DATA_CHANNEL,
                                ZMQ_RCVHWM,
                                &hwm, sizeof(hwm));
    BXIERR_CHAIN(err, err2);

    zmq_pollitem_t items[] = { { CONTROL_CHANNEL, 0, ZMQ_POLLIN, 0 },
                               { DATA_CHANNEL, 0, ZMQ_POLLIN, 0 },
    };
    while (true) {
        errno = 0;
        rc = zmq_poll(items, 2, DEFAULT_POLL_TIMEOUT); // -1 -> wait infinitely
        if (-1 == rc) {
            if (EINTR == errno) continue; // One interruption happens
                                          //(e.g. with profiling)
            err2 = _sync();
            BXIERR_CHAIN(err, err2);
            if (bxierr_isko(err)) {
                err2 = bxierr_gen("Calling zmq_poll() failed");
                BXIERR_CHAIN(err, err2);
                goto QUIT;
            }
        }
        if (0 == rc) {
            // Nothing to poll -> do a flush() and start again
            err2 = _flush_iht(DATA_CHANNEL);
            BXIERR_CHAIN(err, err2);
            if (_should_quit(&err)) goto QUIT;
            continue;
        }
        if (items[0].revents & ZMQ_POLLIN) {
            // Process control message
           err2 = _process_ctrl_msg(CONTROL_CHANNEL, DATA_CHANNEL);
           BXIERR_CHAIN(err, err2);
           if (err == _IHT_EXIT_ERR_) {
               err = (NULL == err->cause) ? BXIERR_OK : err->cause;
               goto QUIT;
           }
           if(_should_quit(&err)) goto QUIT;
        }
        if (items[1].revents & ZMQ_POLLIN) {
            // Process data, this is the normal case
            err2 = _process_data(DATA_CHANNEL);
            BXIERR_CHAIN(err, err2);
            if (_should_quit(&err)) goto QUIT;
        }
    }
    QUIT:
    err2 =  bxizmq_zocket_destroy(DATA_CHANNEL);
    BXIERR_CHAIN(err, err2);
    err2 = bxizmq_zocket_destroy(CONTROL_CHANNEL);
    BXIERR_CHAIN(err, err2);
    err2 = _sync();
    BXIERR_CHAIN(err, err2);
    errno = 0;
    if (STDOUT_FILENO != IHT_DATA.fd && STDERR_FILENO != IHT_DATA.fd) {
        rc = close(IHT_DATA.fd);
        if (-1 == rc) {
            err2 = bxierr_errno("Closing logging file '%s' failed", FILENAME);
            BXIERR_CHAIN(err, err2);
        }
    }
    if (IHT_DATA.lost_logs != 0) {
        size_t reported_error_numbers = 0;
        for (size_t i = 0; i < IHT_DATA.reported_errors_len; i++) {
            if (NULL == IHT_DATA.reported_errors[i]) continue;
            bxierr_p err_iht = IHT_DATA.reported_errors[i];
            bxierr_destroy(&err_iht);
            reported_error_numbers++;
        }
        char * str = bxistr_new("BXI Logging Error Summary:\n"
                                "\tNumber of lost log lines: %zu\n"
                                "\tNumber of reported distinct errors: %zu\n",
                                IHT_DATA.lost_logs,
                                reported_error_numbers);
        _display_err_msg(str);
        BXIFREE(str);
    }
    BXIFREE(IHT_DATA.reported_errors);

    return err;
}

bxierr_p _flush_iht(void * data_channel) {
    bxierr_p err = BXIERR_OK, err2;
    // Check that no new messages arrived in between
    zmq_pollitem_t data_items[] = {{data_channel, 0, ZMQ_POLLIN,0}};
    size_t nb = 0;
    while(true) {
        errno = 0;
        int rc = zmq_poll(data_items, 1, 0); // 0 -> do not wait
        if (-1 == rc) {
            if(EINTR == errno) continue; // One interruption happens
                                         // (e.g. when profiling)
            err2 = bxierr_errno("Calling zmq_poll() failed");
            BXIERR_CHAIN(err, err2);
            break;
        }
        if (!(data_items[0].revents & ZMQ_POLLIN)) {
            // No message are pending
            break;
        }
        // Some messages are still pending
        err2 = _process_data(data_channel);
        BXIERR_CHAIN(err, err2);
        nb++;
    }
    err2 = _sync();
    BXIERR_CHAIN(err, err2);
    return err;
}

bxierr_p _create_iht() {
    bxierr_p err = BXIERR_OK, err2;
    pthread_attr_t attr;
    int rc = pthread_attr_init(&attr);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }
//    rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
//    assert(rc == 0);
    rc = pthread_create(&INTERNAL_HANDLER_THREAD, &attr, _iht_main, NULL);
    if (0 != rc) {
        err2 = bxierr_fromidx(rc, NULL,
                              "Calling pthread_attr_init() failed (rc=%d)", rc);
        BXIERR_CHAIN(err, err2);
    }
    return err;
}

void _log_single_line(const struct log_header_s * const header,
                      const char * const filename,
                      const char * const funcname,
                      const char * const loggername,
                      const char * const line,
                      const size_t line_len) {

    size_t size = FIXED_LOG_SIZE + PROGNAME_LEN + header->variable_len + line_len + 1;
    char msg[size];
    ssize_t loglen = _mkmsg(size, msg,
                            LOG_LEVEL_STR[header->level],
                            &header->detail_time,
#ifdef __linux__
                            header->tid,
#endif
                            header->thread_rank,
                            filename,
                            header->line_nb,
                            funcname,
                            loggername,
                            line);
    errno = 0;
    ssize_t written = write(IHT_DATA.fd, msg, (size_t) loglen);

    if (0 >= written) {
        IHT_DATA.lost_logs++;
        bxierr_p bxierr = bxierr_errno("Calling write(fd=%d,name=%s) failed (written=%d)",
                                       IHT_DATA.fd, FILENAME, written);
        // Only report newly detected errors
        size_t i = 0;
        bxierr_p reported_error = NULL;
        for (; i < IHT_DATA.reported_errors_len; i++) {
            reported_error = IHT_DATA.reported_errors[i];
            if (NULL == reported_error) break;
            if (reported_error->code == bxierr->code) break;
        }
        if (i >= IHT_DATA.reported_errors_len) {
            size_t new_len = IHT_DATA.reported_errors_len * 2;
            IHT_DATA.reported_errors = bximem_realloc(IHT_DATA.reported_errors,
                                                      new_len*sizeof(*IHT_DATA.reported_errors));
            IHT_DATA.reported_errors_len = new_len;
        }
        if (reported_error == NULL) {
            char * err_str = bxierr_str(bxierr);
            char * str = bxistr_new("[W] Can't write to '%s' - cause is %s\n"
                    "This means "
                    "at least one log line has been lost\n"
                    "[W]This error might be caused by other errors.\n"
                    "[W]This is the first time this cause has been"
                    " reported however, and it will be the last time.\n"
                    "[W]An error reporting summary should be "
                    "available in your program if it uses the full bxi "
                    "high performance logging library.\n",
                    FILENAME, err_str);
            _display_err_msg(str);
            IHT_DATA.reported_errors[i] = bxierr;
            BXIFREE(err_str);
            BXIFREE(str);
        }
    }
}

bxierr_p _process_data(void * const data_channel) {
    // Process data message
    zmq_msg_t zmsg;
    errno = 0;
    int rc = zmq_msg_init(&zmsg);
    if (0 != rc) return bxierr_errno("Can't initialize message content");

    // Fetch the first frame, it should be the header
    errno = 0;
    rc = zmq_msg_recv(&zmsg, data_channel, ZMQ_DONTWAIT);
    if (-1 == rc) {
        // Nothing to process, this might happened if a flush has been asked
        // and processed before this function call
        if (EAGAIN == errno) return BXIERR_OK;
        while (-1 == rc && EINTR == errno){
            errno = 0;
            rc = zmq_msg_recv(&zmsg, data_channel, ZMQ_DONTWAIT);
        }
        if (-1 == rc) {
            if (ETERM == errno) {
                bxierr_p err = bxierr_errno("Calling zmq_msg_recv() failed");
                bxierr_p err2 = bxizmq_msg_close(&zmsg);
                BXIERR_CHAIN(err, err2);
                return err;
            }
            return bxierr_errno("Problem while receiving header from %s", data_url);
        }
    }
    // Assert we received enough data
    // TODO: make minimum_size a global variable that is constant and private to
    // the IHT
    const size_t minimum_size = sizeof(struct log_header_s);
    const size_t received_size = zmq_msg_size(&zmsg);
    assert(received_size >= minimum_size);

    struct log_header_s * const header = zmq_msg_data(&zmsg);
    // Fetch other strings: filename, funcname, loggername, logmsg
    const char * filename = (char *) header + sizeof(*header);
    const char * const funcname = filename + header->filename_len;
    const char * const loggername = funcname + header->funcname_len;
    // logmsg is no more constant: we will cut it into separate lines
    char * const logmsg = (char *) loggername + header->logname_len;

    filename = _basename(filename, header->filename_len);

    // Cut the logmsg into separate lines
    errno = 0;
    char * s = logmsg;
    char * next = s;
    size_t len = 0;
    while(true) {
        if ('\0' == *next) {
            _log_single_line(header, filename, funcname, loggername, s, len);
            break;
        }
        if ('\n' == *next) {
            *next = '\0';
            _log_single_line(header, filename, funcname, loggername, s, len);
            len = 0;
            s = next + 1;
        }
        next++;
        len++;
    }

    /* Release */
    bxierr_p err = bxizmq_msg_close(&zmsg);
    if (bxierr_isko(err)) _display_err_msg("[W] Can't close zmg msg");

    return BXIERR_OK;
}

// TODO: change the signature so ctrl_channel, data_channel and fd
// becomes global variables initialized during the iht_main()
bxierr_p _process_ctrl_msg(void * ctrl_channel, void * data_channel) {
    char * cmd;
    bxierr_p err = BXIERR_OK, err2;
    err2 = bxizmq_str_rcv(ctrl_channel, ZMQ_DONTWAIT, false, &cmd);
    BXIERR_CHAIN(err, err2);
    assert(bxierr_isok(err) && NULL != cmd);
    if (0 == strcmp(FLUSH_CTRL_MSG_REQ, cmd)) {
        err2 = _flush_iht(data_channel);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_str_snd(FLUSH_CTRL_MSG_REP, ctrl_channel, 0, 0, 0);
        BXIERR_CHAIN(err, err2);
        BXIFREE(cmd);
        return err;
    }
    if (0 == strcmp(READY_CTRL_MSG_REQ, cmd)) {
        // Reply "ready!"
        err2 = bxizmq_str_snd(READY_CTRL_MSG_REP,
                              ctrl_channel, 0, RETRIES_MAX, RETRY_DELAY);
        BXIERR_CHAIN(err, err2);
        BXIFREE(cmd);
        return err;
    }
    if (0 == strcmp(EXIT_CTRL_MSG_REQ, cmd)) {
        // Flushing must be asked explicitely by the BC.
        // Therefore, there is nothing to flush on exit.
        // Exit must exit as fast as possible.
//        err2 = _flush_iht(data_channel);
//        BXIERR_CHAIN(err, err2);
        //                size_t retries = bxizmq_str_snd(EXIT_CTRL_MSG_REP, control_channel,
        //                                                0, RETRIES_MAX, RETRY_DELAY);
        //                assert(0 == retries);
        BXIFREE(cmd);
        BXIERR_CHAIN(err, _IHT_EXIT_ERR_);
        return err;
    }
    BXIFREE(cmd);
    return bxierr_gen("bxilog.iht: Unknown control command: %s", cmd);
}

ssize_t _mkmsg(const size_t n, char buf[n],
               const char level,
               const struct timespec * const detail_time,
#ifdef __linux__
               const pid_t tid,
#endif
               const uint16_t thread_rank,
               const char * const filename,
               const int line_nb,
               const char * const funcname,
               const char * const loggername,
               const char * const logmsg) {

    errno = 0;
    struct tm dummy, *now;
    now = localtime_r(&detail_time->tv_sec, &dummy);
    assert(NULL != now);
    int written = snprintf(buf, n, LOG_FMT,
                           level,
                           YEAR_SIZE, now->tm_year + 1900,
                           MONTH_SIZE, now->tm_mon + 1,
                           DAY_SIZE, now->tm_mday,
                           HOUR_SIZE, now->tm_hour,
                           MINUTE_SIZE, now->tm_min,
                           SECOND_SIZE, now->tm_sec,
                           SUBSECOND_SIZE, detail_time->tv_nsec,
                           PID_SIZE, PID,
#ifdef __linux__
                           TID_SIZE, tid,
#endif
                           THREAD_RANK_SIZE, thread_rank,
                           PROGNAME,
                           filename,
                           line_nb,
                           funcname,
                           loggername,
                           logmsg);


    // Truncation must never occur
    // If it happens, it means the size given was just plain wrong
//    assert(written < (int)n);
    // For debugging in case the previous condition does not hold,
    // comment previous line, and uncomment lines below.
    if (written >= (int) n) {
        fprintf(stderr, "******** ERROR: FIXED_LOG_SIZE=%d, "
                    "written = %d >= %zu = n\nbuf(%zu)=%s\nlogmsg(%zu)=%s\n",
                    FIXED_LOG_SIZE, written, n ,
                    strlen(buf), buf, strlen(logmsg), logmsg);
    }

    assert(written >= 0);
    return written;
}

bxierr_p _get_file_fd(int * fd) {
    errno = 0;
    if (0 == strcmp(FILENAME, "-")) {
        *fd = STDOUT_FILENO;
    } else if (0 == strcmp(FILENAME, "+")) {
        *fd = STDERR_FILENO;
    } else {
        errno = 0;
        *fd = open(FILENAME,
//                   O_WRONLY | O_CREAT | O_APPEND | O_DSYNC,
                   O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (-1 == *fd) return bxierr_errno("Can't open %s", FILENAME);
    }
    return BXIERR_OK;
}

bxierr_p _sync() {
    // char * nb_str = bximake_message("Messages flushed: %zu, syncing data", nb);
    // _iht_log(BXILOG_DEBUG, TID, IHT_RANK, FD, str);
    // FREE(nb_str);
    errno = 0;
    int rc = fdatasync(IHT_DATA.fd);
    if (rc != 0) {
        if (EROFS != errno && EINVAL != errno) {
            return bxierr_errno("Call to fdatasync() failed");
        }
        // OK otherwise, it just means the given FD does not support synchronisation
        // this is the case for example with stdout, stderr...
    }
    return BXIERR_OK;
}

bool _should_quit(bxierr_p * err) {
    if (bxierr_isko(*err)) {
        size_t depth = bxierr_get_depth(*err);
        if (MAX_DEPTH_ERR < depth) {
            bxierr_p err2 = bxierr_gen("Too many errors (%zu), aborting", depth);
            BXIERR_CHAIN(*err, err2);
            return true;
        }
        char * str = bxierr_str(*err);
        char * msg = bxistr_new("Warning: errors encountered: %s\n", str);
        _display_err_msg(msg);
        BXIFREE(str);
        BXIFREE(msg);
        return false;
    }
    return false;
}

//--------------------------------- Initializer/Finalizer helpers -------------------
bxierr_p _init(void) {
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.

    PID = getpid();

    if (UNSET != STATE && FINALIZED != STATE) {
        bxierr_p err =  bxierr_new(BXILOG_ILLEGAL_STATE_ERR,
                                   NULL, NULL, NULL,
                                   "Illegal state: %d", STATE);
        STATE = ILLEGAL;
        return err;
    }
    STATE = INITIALIZING;

    assert(NULL == BXILOG_CONTEXT);
    errno = 0;
    BXILOG_CONTEXT = zmq_ctx_new();
    if (NULL == BXILOG_CONTEXT) {
        STATE = ILLEGAL;
        return bxierr_errno("Can't create a zmq context");
    }

    // We use 'inproc', we might be tempted to remove io threads.
    // However, since the internal handler might push to other remote
    // workers, we keep those io threads.
    /*
        int rc = zmq_ctx_set(BXILOG_CONTEXT, ZMQ_IO_THREADS, 0);
        assert(rc == 0);
     */
    data_url = bxistr_new(DATA_CHANNEL_URL, getpid());
    control_url = bxistr_new(CONTROL_CHANNEL_URL, getpid());

    bxierr_p err = _create_iht();
    if (bxierr_isko(err)) return err;
    int rc = pthread_once(&TSD_KEY_ONCE, _tsd_key_new);
    if (0 != rc) {
        STATE = ILLEGAL;
        return bxierr_fromidx(rc, NULL,
                              "Calling pthread_once() failed (rc=%d)", rc);
    }

    tsd_p tsd;
    err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;
    err = bxizmq_str_snd(READY_CTRL_MSG_REQ, tsd->ctl_channel, 0, 0, 0);
    if (bxierr_isko(err)) return err;

    char * ready;
    err = bxizmq_str_rcv(tsd->ctl_channel, 0, false, &ready);
    if (bxierr_isko(err)) return err;
    assert(NULL != ready);

    if (0 != strcmp(ready, READY_CTRL_MSG_REP)) {
        // Ok, the IHT sends us an error msg.
        // We expect it to die and the actual error will be returned
        bxierr_p iht_err;
        bxierr_p err = _join_iht(&iht_err);
        assert(bxierr_isok(err));
        BXIFREE(ready);
        err = _cleanup();
        assert(bxierr_isok(err));
        BXIFREE(FILENAME);
        BXIFREE(PROGNAME);
        STATE = FINALIZED;
        return iht_err;
    }
    BXIFREE(ready);
    return err;
}

bxierr_p _end_iht(void) {
    errno = 0;
    tsd_p tsd;
    bxierr_p err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;
    err = bxizmq_str_snd(EXIT_CTRL_MSG_REQ, tsd->ctl_channel,
                         0, RETRIES_MAX, RETRY_DELAY);
    if (BXIZMQ_RETRIES_MAX_ERR == err->code) {
        char * str = bxistr_new("Sending %s required %zu retries",
                                EXIT_CTRL_MSG_REQ, (size_t) err->data);
        _display_err_msg(str);
        BXIFREE(str);
        bxierr_destroy(&err);
        err = BXIERR_OK;
    }
    return err;
}

bxierr_p _join_iht(bxierr_p * iht_err_p) {
    int rc = pthread_join(INTERNAL_HANDLER_THREAD, (void**) iht_err_p);
    if (0 != rc) return bxierr_fromidx(rc, NULL,
                                       "Can't join the internal handler thread"
                                       " Calling pthread_join() failed (rc=%d)", rc);
    return BXIERR_OK;
}

bxierr_p _cleanup(void) {
    tsd_p tsd;
    bxierr_p err = _get_tsd(&tsd);
    if (bxierr_isko(err)) return err;
    _tsd_free(tsd);
    int rc = zmq_ctx_destroy(BXILOG_CONTEXT);
    if (-1 == rc) {
        while (-1 == rc && EINTR == errno) {
            rc = zmq_ctx_destroy(BXILOG_CONTEXT);
        }
        if (-1 == rc) {
            bxierr_p err2 = bxierr_fromidx(rc, NULL,
                                           "Can't destroy context (rc=%d)", rc);
            BXIERR_CHAIN(err, err2);
        }
    }
    BXILOG_CONTEXT = NULL;
    rc = pthread_key_delete(TSD_KEY);
    assert(0 == rc); // Nothing to do on pthread_key_delete() see man page
    TSD_KEY_ONCE = PTHREAD_ONCE_INIT;

    BXIFREE(data_url);
    BXIFREE(control_url);
    return err;
}

bxierr_p _finalize(void) {
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (INITIALIZED != STATE) {
        bxierr_p err = bxierr_new(BXILOG_ILLEGAL_STATE_ERR, NULL, NULL, NULL,
                                  "Illegal state: %d", STATE);
        STATE = ILLEGAL;
        return err;
    }
    STATE = FINALIZING;
    bxierr_p err = BXIERR_OK, err2;
    err2 = _end_iht();
    BXIERR_CHAIN(err, err2);

    bxierr_p iht_err = BXIERR_OK;
    err2 = _join_iht(&iht_err);
    BXIERR_CHAIN(err, err2);
    BXIERR_CHAIN(err, iht_err);

    err2 = _cleanup();
    return err;
}


void _install_fork_handlers(void) {
    // Install fork handler
    errno = 0;
    int rc = pthread_atfork(_parent_before_fork, _parent_after_fork, _child_after_fork);
    assert(0 == rc);
}


void _parent_before_fork(void) {
    // WARNING: If you change the FSM transition,
    // comment you changes in bxilog_state_e above.
    if (INITIALIZING == STATE || FINALIZING == STATE) {
        error(EX_SOFTWARE, 0, "Forking while bxilog is in state %d! Aborting", STATE);
    }
    if(INITIALIZED != STATE) return;
    FINE(BXILOG_INTERNAL_LOGGER, "Preparing for a fork() (state == %d)", STATE);
    bxierr_p err = BXIERR_OK, err2;
    err2 = bxilog_flush();
    BXIERR_CHAIN(err, err2);
    err2 = _finalize();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        char * err_str = bxierr_str(err);
        char * msg = bxistr_new("Error while preparing for a fork(), "
                                "some logs might have been missed. "
                                "Detailed error is - %s",
                                err_str);
        _display_err_msg(msg);
        BXIFREE(msg);
        BXIFREE(err_str);
        bxierr_destroy(&err);
    }
    if (FINALIZING != STATE) {
        error(EX_SOFTWARE, 0,
              "Forking should lead bxilog to reach state %d (current state is %d)!",
              FINALIZING, STATE);
    }
    STATE = FORKED;
}

void _parent_after_fork(void) {
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (FORKED != STATE) return;
    STATE = FINALIZED;
    bxierr_p err = _init();
    // Can't do a log
    assert(bxierr_isok(err));

    if (INITIALIZING != STATE) {
        error(EX_SOFTWARE, 0,
              "Forking should leads bxilog to reach state %d (current state is %d)!",
              INITIALIZING, STATE);
    }
    STATE = INITIALIZED;
    // Sending a log required the log library to be initialized.
    FINE(BXILOG_INTERNAL_LOGGER, "Ready after a fork()");
}

void _child_after_fork(void) {
    // WARNING: If you change the FSM transition,
    // comment your changes in bxilog_state_e above.
    if (FORKED != STATE) return;
    BXIFREE(FILENAME);
    BXIFREE(PROGNAME);
    STATE = FINALIZED;
    // The child remain in the finalized state
    // It has to call bxilog_init() if it wants to do some log.
}

//------------------------- Generic Helpers ---------------------------------------
inline const char * _basename(const char * const path, const size_t path_len) {
    assert(path != NULL && path_len > 0);

    size_t c = path_len - 2; // path_len includes the NULL terminating byte. Therefore last char is at -2
    while(c > 0) {
        if (path[c] == '/') {
            c++;
            break;
        }
        c--;
    }
    return path + c;
}

void _display_err_msg(char* msg) {
    // TODO: use a better format?
    ssize_t n = write(STDERR_FILENO, msg, strlen(msg) + 1);
    // Just don't care on error, the end-user might have redirected
    // the stderr stream to a non-writable place (such as a broken pipe after a
    // command such as 'cmd 2>&1 |head')...
    // assert(n > 0);
    UNUSED(n);
    const char details_msg[] = "\nSee bxilog for details.\n";
    n = write(STDERR_FILENO, details_msg, ARRAYLEN(details_msg));
}

// Handler of Signals (such as SIGSEGV, ...)
void _sig_handler(int signum, siginfo_t * siginfo, void * dummy) {
    (void) (dummy); // Unused, prevent warning == error at compilation time
    char * sigstr = bxilog_signal_str(siginfo, NULL);
#ifdef __linux__
    pid_t tid = (pid_t) syscall(SYS_gettid);
#else
    uint16_t tid = bxilog_get_thread_rank(pthread_self());
#endif
    /* Since this handler is established for more than one kind of signal,
       it might still get invoked recursively by delivery of some other kind
       of signal.  Use a static variable to keep track of that. */
    if (FATAL_ERROR_IN_PROGRESS) {
        error(signum, 0, "(%s#tid-%u) %s. Already handling a signal... Exiting",
              PROGNAME, tid, sigstr);
    }
    FATAL_ERROR_IN_PROGRESS = 1;
    char * str;
    if (SIGINT == signum || SIGTERM == signum) {
        str = bxistr_new("%s: %s", PROGNAME, sigstr);
    } else {
        char * trace = bxierr_backtrace_str();
        str = bxistr_new("%s: %s - %s", PROGNAME, sigstr, trace);
        BXIFREE(trace);
    }
    BXIFREE(sigstr);

    _display_err_msg(str);

    CRITICAL(BXILOG_INTERNAL_LOGGER, "%s", str);
    BXIFREE(str);
    // Flush all logs before terminating -> ask the iht to stop.
    bxierr_p err = BXIERR_OK, err2;
    err2 = bxilog_flush();
    BXIERR_CHAIN(err, err2);
    err2 = _end_iht();
    BXIERR_CHAIN(err, err2);
    if (bxierr_isko(err)) {
        char * err_str = bxierr_str(err);
        char * str = bxistr_new("Error while processing signal - %s\n",
                                err_str);
        _display_err_msg(str);
        BXIFREE(str);
    }

    // Wait some time before exiting
    struct timespec rem;
    struct timespec delay = {.tv_sec = 1, .tv_nsec=0};
    errno = 0;

    while(true) {
        int rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, &rem);
        if (0 == rc) break;
        if (-1 == rc && errno != EINTR) break; // An error occured...
        // Else we have been interrupted, start again
        delay = rem;
    }

    // Use default sighandler to end.
    struct sigaction dft_action;
    memset(&dft_action, 0, sizeof(struct sigaction));
    dft_action.sa_handler = SIG_DFL;
    errno = 0;
    int rc = sigaction(signum, &dft_action, NULL);
    if (-1 == rc) {
        error(128 + signum, errno, "Calling sigaction(%d, ...) failed", signum);
    }
    errno = 0;
    // Unblock all signals
    sigset_t mask;
    rc = sigfillset(&mask);
    assert(0 == rc);
    rc = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    if (-1 == rc) _display_err_msg("Calling pthread_sigmask() failed\n");
    rc = pthread_kill(pthread_self(), signum);
    if (0 != rc) {
        error(128 + signum, errno, "Calling pthread_kill(self, %d) failed", signum);
    }
    _exit(128 + signum);
}
