/* -*- coding: utf-8 -*-
###############################################################################
# Author: Sébastien Miquée <sebastien.miquee@atos.net>
# Created on: 2016-08-08
# Contributors:
###############################################################################
# Copyright (C) 2016  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################
*/

#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/log.h"
#include "bxi/base/log/remote_receiver.h"

#include <argp.h>


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
#define PROGNAME "bxilog-monitor"

SET_LOGGER(MAIN_LOGGER, "bxilog-monitor");

#define OPT_LOGFILE_KEY 10            // The key should not be a printable character
#define OPT_BIND_KEY 11               // The key should not be a printable character

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

/* Used by main to communicate with parse_opt. */
struct arguments_s {
    int nb_urls; // How many urls to connect to
    const char ** urls; // The urls
    char *logfilters;
    char *logfile;
    bool bind;
};

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************
//----------------------------- Option Parser -----------------------------------

static error_t _parse_opt(int key, char *arg, struct argp_state * const state);

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************
//

// -- Option parser (argp) variables.
static char DOC[] = PROGNAME" -- remotely monitor bxilog enabled programs";
static char ARGS_DOC[] = "bxilog_socket_url [bxilog_socket_url [...]]";

static struct argp_option OPTIONS[] = {
    { .name = "version", .key = 'v',
        .doc = "Print the program version" },
    { .name = "logfilters", .key = 'l', .arg = "prefix:level[,prefix:level]*",
        .doc = "Defines the logging level. Default: ':output'." },
    { .name = "bind", .key=OPT_BIND_KEY,
        .doc = "If set, bind to the given urls, otherwise, connect." },
    { .name = "logfile", .key=OPT_LOGFILE_KEY, .arg = "FILE",
        .doc = "Defines the file where logging should be output. "
            "Character '-' represents standard output. Default: '-'." },
    { .name = 0 },
};
static const struct argp argp = { .options = OPTIONS,
    .parser = _parse_opt,
    .args_doc = ARGS_DOC,
    .doc = DOC };


// *********************************************************************************
// ********************************** MAIN *****************************************
// *********************************************************************************

int main(int argc, char **argv) {
    struct arguments_s arguments;
    /* Default values. */
    arguments.logfilters = ":output";
    arguments.logfile = NULL;
    arguments.nb_urls = 0;
    arguments.bind = false;
    arguments.urls = bximem_calloc(sizeof(*arguments.urls) * (size_t)argc);

    /* Parse our arguments; every option seen by parse_opt will
       be reflected in arguments. */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    char * const fullprogname = strdup(argv[0]);
    BXIASSERT(MAIN_LOGGER, fullprogname != NULL);
    char * const progname = basename(fullprogname);
    bxilog_filters_p filters;
    bxierr_p err = bxilog_filters_parse(arguments.logfilters, &filters);
    bxierr_abort_ifko(err);
    bxilog_config_p config = bxilog_basic_config(progname,
                                                 arguments.logfile,
                                                 BXI_APPEND_OPEN_FLAGS,
                                                 filters);
    err = bxilog_init(config);
    bxierr_abort_ifko(err);
    err = bxilog_install_sighandler();
    bxierr_abort_ifko(err);
    DEBUG(MAIN_LOGGER, "fullprogname: %s", fullprogname);

    bxilog_remote_recv_p param = malloc(sizeof(*param));
    param->nb_urls = arguments.nb_urls;
    param->urls = arguments.urls;
    param->bind = arguments.bind;

    err = bxilog_remote_recv(param);

    /*
    // *** In the following using the asynchronous function
    err = bxilog_remote_recv_async(param);

    if (bxierr_isko(err)) {
        BXILOG_REPORT(MAIN_LOGGER, BXILOG_CRITICAL, err, "An error occured, exiting");
        exit(1);
    }

    sleep(10);

    err = bxilog_remote_recv_async_stop();
    */

    int rc = err->code;
    BXILOG_REPORT(MAIN_LOGGER, BXILOG_CRITICAL, err, "An error occured, exiting");

    BXIFREE(fullprogname);

    BXIFREE(arguments.urls);

    BXIFREE(param);

    bxilog_finalize(true);

    return rc;
}


// *************************************************************************************
// *********************************** Static functions ********************************
// *************************************************************************************
//------------------------------------ Option Parser ----------------------------------

/* Parse a single option. */
error_t _parse_opt(int key, char *arg, struct argp_state * const state) {
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct arguments_s * const arguments = state->input;
    switch (key) {
        case OPT_BIND_KEY:
            arguments->bind = true;
            break;
        case 'l':
            arguments->logfilters = arg;
            break;
        case OPT_LOGFILE_KEY:
            arguments->logfile = arg;
            break;
        case 'h':
            argp_state_help(state, stdout, ARGP_HELP_USAGE | ARGP_HELP_LONG | ARGP_HELP_EXIT_OK);
            return ARGP_HELP_EXIT_ERR;
        case ARGP_KEY_ARG:
            arguments->urls[arguments->nb_urls] = arg;
            arguments->nb_urls += 1;
            break;
        case ARGP_KEY_END:
            if (0 == arguments->nb_urls) {
                /* Bad number of arguments. */
                argp_state_help(state, stdout, ARGP_HELP_USAGE | ARGP_HELP_LONG | ARGP_HELP_EXIT_ERR);
                return ARGP_HELP_EXIT_ERR;
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
