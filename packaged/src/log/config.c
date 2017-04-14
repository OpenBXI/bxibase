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



#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>

#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/log/file_handler.h"
#include "bxi/base/log/console_handler.h"
#include "bxi/base/log/syslog_handler.h"
#include "bxi/base/log/netsnmp_handler.h"
#include "bxi/base/log/config.h"

#include "config_impl.h"
#include "log_impl.h"


//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Static Functions  ****************************
//*********************************************************************************
//--------------------------------- Generic Helpers --------------------------------

//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************

// The internal logger
SET_LOGGER(LOGGER, BXILOG_LIB_PREFIX "bxilog.cfg");


//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


bxilog_config_p bxilog_basic_config(const char * const progname,
                                    const char * const filename,
                                    const int open_flags,
                                    const bxilog_filters_p filters) {

    const char * basename;
    bxistr_rsub(progname, strlen(progname), '/', &basename);
    bxilog_config_p config = bxilog_config_new(basename);
    const int loggername_width = 12; // Default
    bxilog_config_add_handler(config,
                              BXILOG_CONSOLE_HANDLER,
                              filters,
                              BXILOG_WARNING,
                              loggername_width,
                              BXILOG_COLORS_TC_DARK);
    if (NULL != filename) {
        bxilog_filters_p file_filters = bxilog_filters_dup(filters);
        for (size_t i = 0; i < filters->nb; i++) {
            file_filters->list[i]->level += 2;
        }
        bxilog_config_add_handler(config, BXILOG_FILE_HANDLER,
                                  file_filters,
                                  basename, filename, open_flags);
    }
    // Bull default to LOG_LOCAL0
//    bxilog_config_add_handler(config, BXILOG_SYSLOG_HANDLER,
//                              basename,
//                              LOG_CONS | LOG_PID,
//                              LOG_LOCAL0,
//                              BXILOG_WARNING);

    return config;
}

bxilog_config_p bxilog_unit_test_config(const char * const progname,
                                        const char * const filename,
                                        int open_flags) {

    bxiassert(NULL != filename);
    const char * basename;
    bxistr_rsub(progname, strlen(progname), '/', &basename);
    bxilog_config_p config = bxilog_config_new(basename);
    bxilog_config_add_handler(config,
                              BXILOG_FILE_HANDLER,
                              BXILOG_FILTERS_ALL_ALL,
                              basename, filename, open_flags);
//    bxilog_config_add_handler(config, BXILOG_FILE_HANDLER,
//                              BXILOG_FILTERS_ALL_OFF,
//                              basename, "/dev/null", O_APPEND);
//    bxilog_config_add_handler(config, BXILOG_SYSLOG_HANDLER,
//                                  basename,
//                                  LOG_CONS | LOG_PID,
//                                  LOG_LOCAL0,
//                                  BXILOG_WARNING);
    return config;
}

#ifdef HAVE_LIBNETSNMP
bxilog_config_p bxilog_netsnmp_config(const char * const progname) {

    const char * basename;
    bxistr_rsub(progname, strlen(progname), '/', &basename);
    bxilog_config_p config = bxilog_config_new(basename);
    // Use 2 loggers to ensure multiple handlers works
    bxilog_config_add_handler(config,
                              BXILOG_SNMPLOG_HANDLER,
                              BXILOG_FILTERS_ALL_OUTPUT,
                              basename);

    return config;
}
#endif


bxilog_config_p bxilog_config_new(const char * const progname) {
    bxilog_config_p config = bximem_calloc(sizeof(*config));
    config->progname = strdup(progname);
    config->tsd_log_buf_size = 128;
    config->handlers_nb = 0;
    config->ctrl_hwm = 1000;
    config->data_hwm = 1000;

    return config;
}

void bxilog_config_add_handler(bxilog_config_p self,
                               bxilog_handler_p handler,
                               bxilog_filters_p filters,
                               ...) {

    size_t new_size = self->handlers_nb + 1;
    self->handlers = bximem_realloc(self->handlers,
                                    self->handlers_nb * sizeof(*self->handlers),
                                    new_size * sizeof(*self->handlers));
    self->handlers_params = bximem_realloc(self->handlers_params,
                                           self->handlers_nb * sizeof(*self->handlers_params),
                                           new_size * sizeof(*self->handlers_params));
    self->handlers[self->handlers_nb] = handler;

    va_list ap;
    va_start(ap, filters);
    self->handlers_params[self->handlers_nb] = handler->param_new(handler, filters, ap);
    va_end(ap);

    self->handlers_nb++;
}

bxierr_p bxilog__config_destroy(bxilog_config_p * config_p) {
    bxierr_list_p errlist = bxierr_list_new();
    bxilog_config_p config = *config_p;

    for (size_t i = 0; i < config->handlers_nb; i++) {
        bxilog_handler_param_p handler_param = config->handlers_params[i];

        if (NULL != config->handlers[i]->param_destroy) {
            bxierr_p err = config->handlers[i]->param_destroy(&handler_param);
            if (bxierr_isko(err)) bxierr_list_append(errlist, err);
        }
    }
    BXIFREE(config->handlers);
    BXIFREE(config->handlers_params);
    BXIFREE(config->progname);
    bximem_destroy((char**) config_p);
    if (errlist->errors_nb > 0) {
        return bxierr_from_list(BXIERR_GROUP_CODE, errlist,
                                "Errors encountered while destroying logging parameters");
    } else {
        bxierr_list_destroy(&errlist);
    }
    return BXIERR_OK;
}

bxierr_p bxilog__config_loggers() {
    bxilog_logger_p *loggers;
    static char ** level_names;
    bxilog_level_names(&level_names);
    size_t loggers_nb = bxilog_registry_getall(&loggers);
    for (size_t i = 0; i < loggers_nb; i++) {
//        bxilog_level_e old_level = loggers[i]->level;
        bxilog_logger_reconfigure(loggers[i]);
//        bxilog_level_e new_level = loggers[i]->level;
        //TODO: use DBG macro when available
//        fprintf(stderr, "Configuration of %s: %s -> %s\n",
//                loggers[i]->name,
//                level_names[old_level],
//                level_names[new_level]);
    }
    BXIFREE(loggers);

    return BXIERR_OK;
}

bxierr_p bxilog_get_level_from_str(char * level_str, bxilog_level_e *level) {
    if (0 == strcasecmp("off", level_str)) {
        *level = BXILOG_OFF;
        return BXIERR_OK;
    }
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


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

