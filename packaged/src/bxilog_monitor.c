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

#include "bxi/base/err.h"
#include "bxi/base/mem.h"
#include "bxi/base/log.h"
#include "bxi/base/log/remote_receiver.h"

#include <argp.h>


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
#define PROGNAME "bxilog_monitor"

SET_LOGGER(MAIN_LOGGER, "bxilog_monitor");

#define OPT_LOGFILE_KEY 10            // The key should not be a printable character

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

/* Used by main to communicate with parse_opt. */
struct arguments_s {
    int nb_urls; // How many urls to connect to
    const char ** urls; // The urls
    char *logfilters;
    char *logfile;
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
    arguments.urls = bximem_calloc(sizeof(*arguments.urls) * (size_t)argc);

    /* Parse our arguments; every option seen by parse_opt will
       be reflected in arguments. */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    char * const fullprogname = strdup(argv[0]);
    BXIASSERT(MAIN_LOGGER, fullprogname != NULL);
    char * const progname = basename(fullprogname);
    bxierr_p err = bxilog_registry_parse_set_filters(arguments.logfilters);
    bxierr_abort_ifko(err);
    err = bxilog_init(bxilog_basic_config(progname,
                                          arguments.logfile,
                                          BXI_APPEND_OPEN_FLAGS));
    bxierr_abort_ifko(err);
    err = bxilog_install_sighandler();
    bxierr_abort_ifko(err);
    DEBUG(MAIN_LOGGER, "fullprogname: %s", fullprogname);

    bxilog_remote_recv_p param = malloc(sizeof(*param));
    param->nb_urls = arguments.nb_urls;
    param->urls = arguments.urls;
    err = bxilog_remote_recv(param);

    int rc = err->code;
    BXILOG_REPORT(MAIN_LOGGER, BXILOG_CRITICAL, err, "An error occured, exiting");
    bxilog_finalize(true);
    BXIFREE(fullprogname);

    for (int i = 0; i < argc; i++) {
        BXIFREE(arguments.urls[i]);
    }
    BXIFREE(arguments.urls);

    BXIFREE(param);

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
//        case 'v':
//            printf("%s\n",get_bxiquickfix_version());
//            exit(1);
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
