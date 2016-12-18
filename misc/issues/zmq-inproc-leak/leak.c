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

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <float.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <bxi/base/err.h>
#include <bxi/base/log/level.h>
#include <bxi/base/log/logger.h>
#include <bxi/base/log.h>
#include <bxi/base/mem.h>
#include <bxi/base/str.h>
#include <bxi/base/time.h>
#include <bxi/base/log/file_handler.h>
#include <bxi/base/log/null_handler.h>

static volatile bool AGAIN = true;
SET_LOGGER(logger, "bench");


static void * logging_thread(void * param) {
    size_t loop_nb = (size_t) param;

    printf("looping: %zu times\n", loop_nb);
    while (0 < loop_nb--) {
        OUT(logger, "Logging something");
    }
    printf("Exiting\n");
    return NULL;
}

int main(int argc, char * argv[]) {
    UNUSED(argc);

    if (argc != 3) {
        fprintf(stderr, "Usage: %s loop_nb logger_nb\n", basename(argv[0]));
        exit(1);
    }
    bxilog_config_p config = bxilog_config_new(basename(argv[0]));

    bxilog_config_add_handler(config, BXILOG_NULL_HANDLER, BXILOG_FILTERS_ALL_ALL);

    bxierr_p bxierr = bxilog_init(config);
    assert(bxierr_isok(bxierr));
    size_t loop_nb = atoll(argv[1]);
    printf("Looping: %zu times\n", loop_nb);
    int n = atoi(argv[2]);
    printf("Starting %d threads\n", n);

    pthread_t threads[n];
    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, logging_thread, (void*) loop_nb);
    }

    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }
    bxierr = bxilog_finalize(false);
    if (!bxierr_isok(bxierr)) {
        char * str = bxierr_str(bxierr);
        fprintf(stderr, "WARNING: bxilog finalization returned: %s", str);
        BXIFREE(str);
        bxierr_destroy(&bxierr);
    }
}
