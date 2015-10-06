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
#include "bxi/base/log/snmplog_handler.h"

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
SET_LOGGER(LOGGER, "bxi.base.log.cfg");


static char * _LOG_LEVEL_NAMES[] = {
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

//*********************************************************************************
//********************************** Implementation    ****************************
//*********************************************************************************


bxilog_config_p bxilog_basic_config(const char * const progname,
                                    const char * const filename,
                                    bool append) {

    const char * basename = bxistr_rfind(progname, strlen(progname), '/');
    bxilog_config_p config = bxilog_config_new(progname);
    // TODO: default configuration for the moment is compatible with old
    // behaviour. This will change soon.
//    bxilog_config_add_handler(config, BXILOG_CONSOLE_HANDLER, BXILOG_WARNING);
    bxilog_config_add_handler(config, BXILOG_FILE_HANDLER, basename, filename, append);
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
                                        bool append) {

    const char * basename = bxistr_rfind(progname, strlen(progname), '/');
    bxilog_config_p config = bxilog_config_new(basename);
    // Use 2 loggers to ensure multiple handlers works
    bxilog_config_add_handler(config, BXILOG_FILE_HANDLER, basename, filename, append);
    bxilog_config_add_handler(config, BXILOG_FILE_HANDLER, basename, "/dev/null", append);
//    bxilog_config_add_handler(config, BXILOG_SYSLOG_HANDLER,
//                                  basename,
//                                  LOG_CONS | LOG_PID,
//                                  LOG_LOCAL0,
//                                  BXILOG_WARNING);


    return config;
}

bxilog_config_p bxilog_netsnmp_config(const char * const progname) {

    const char * basename = bxistr_rfind(progname, strlen(progname), '/');
    bxilog_config_p config = bxilog_config_new(basename);
    // Use 2 loggers to ensure multiple handlers works
    bxilog_config_add_handler(config, BXILOG_SNMPLOG_HANDLER, basename);

    return config;
}


bxilog_config_p bxilog_config_new(const char * const progname) {
    bxilog_config_p param = bximem_calloc(sizeof(*param));
    param->progname = progname;
    param->tsd_log_buf_size = 128;
    param->handlers_nb = 0;

    return param;
}

void bxilog_config_add_handler(bxilog_config_p self, bxilog_handler_p handler, ...) {
    size_t new_size = self->handlers_nb + 1;
    self->handlers = bximem_realloc(self->handlers,
                                    self->handlers_nb * sizeof(*self->handlers),
                                    new_size * sizeof(*self->handlers));
    self->handlers_params = bximem_realloc(self->handlers_params,
                                           self->handlers_nb * sizeof(*self->handlers_params),
                                           new_size * sizeof(*self->handlers_params));
    self->handlers[self->handlers_nb] = handler;

    va_list ap;
    va_start(ap, handler);
    self->handlers_params[self->handlers_nb] = handler->param_new(handler, ap);
    va_end(ap);

    self->handlers_nb++;
}

bxierr_p bxilog__config_destroy(bxilog_config_p * config_p) {
    bxierr_group_p errlist = bxierr_group_new();
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
    bximem_destroy((char**) config_p);
    if (errlist->errors_nb > 0) {
        return bxierr_from_group(BXIERR_GROUP_CODE, errlist,
                                 "Errors encountered while destroying logging parameters");
    } else {
        bxierr_group_destroy(&errlist);
    }
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


//*********************************************************************************
//********************************** Static Helpers Implementation ****************
//*********************************************************************************

