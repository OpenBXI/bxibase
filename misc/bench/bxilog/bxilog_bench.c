/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: Jul 16, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <zmq.h>
#include <assert.h>
#include <error.h>
#include <errno.h>

#include "bxi/base/str.h"
#include "bxi/base/log.h"

static volatile bool AGAIN = true;
SET_LOGGER(logger, "bench");

void * logging_thread(void * param) {
    assert(param != NULL );
    while (AGAIN) {
        DEBUG(logger, "Logging something...");
    }
    return NULL;
}

int main(int argc, char * argv[]) {
    UNUSED(argc);
    char * fullprogname = strdup(argv[0]);
    char * progname = basename(fullprogname);
    char * filename = bxistr_new("/tmp/%s%s", progname, ".log");
    bxierr_p bxierr = bxilog_init(progname, filename);
    assert(bxierr_isok(bxierr));
    logger->level = BXILOG_DEBUG;
    //fprintf(stderr, "Logging to file: %s\n", filename);

    int n = atoi(argv[1]);
    pthread_t threads[n];
    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, logging_thread, logger);
    }

    sleep(atoi(argv[2]));
    AGAIN = false;
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    bxierr = bxilog_finalize();
    if (!bxierr_isok(bxierr)) {
        char * str = bxierr_str(bxierr);
        fprintf(stderr, "WARNING: bxilog finalization returned: %s", str);
        BXIFREE(str);
        bxierr_destroy(&bxierr);
    }

    BXIFREE(fullprogname);
    BXIFREE(filename);
}
