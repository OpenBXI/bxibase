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
#include <libgen.h>

#include <bxi/base/err.h>
#include <bxi/base/log/level.h>
#include <bxi/base/log/logger.h>
#include <bxi/base/log.h>
#include <bxi/base/mem.h>
#include <bxi/base/str.h>
#include <bxi/base/time.h>
#include <bxi/base/log/file_handler.h>

#include "common.h"

static volatile bool AGAIN = true;

SET_LOGGER(logger, "bench")

static inline void benched_log(bxilog_level_e level, char * str, struct stats_s *  stats) {

    struct timespec start;
    bxierr_p err = bxitime_get(CLOCK_MONOTONIC, &start);
    bxierr_abort_ifko(err);

    bxilog_logger_log(logger, level,
                      (char *)__FILE__, ARRAYLEN(__FILE__),
                      __func__, ARRAYLEN(__func__), __LINE__,
                      "%s", str);

    double duration;
    err = bxitime_duration(CLOCK_MONOTONIC, start, &duration);
    bxierr_abort_ifko(err);
    stats->min_duration = (duration < stats->min_duration) ? duration : stats->min_duration;
    stats->max_duration = (duration > stats->max_duration) ? duration : stats->max_duration;
    stats->total_duration += duration;
    stats->n++;
}

static void * logging_thread(void * param) {
    UNUSED(param);

    struct stats_s * stats = bximem_calloc(sizeof(*stats));
    stats->min_duration = DBL_MAX;
    stats->max_duration = DBL_MIN;
    stats->total_duration = 0;
    stats->n = 0;

    while (AGAIN) {
        if (0 == stats->n) {
            benched_log(BXILOG_LOWEST, "Logging something useless", stats);
            continue;
        }

        char * min_str = bxitime_duration_str(stats->min_duration);
        char * max_str = bxitime_duration_str(stats->max_duration);
        char * avg_str = bxitime_duration_str(stats->total_duration / stats->n);
        char * str = bxistr_new("Logging step %zu: min=%s, max=%s, average=%s",
                                stats->n,
                                min_str,
                                max_str,
                                avg_str);
        benched_log(stats->n % BXILOG_LOWEST + 1, str, stats);
        BXIFREE(min_str);
        BXIFREE(max_str);
        BXIFREE(avg_str);
        BXIFREE(str);
    }
    return stats;
}

int main(int argc, char * argv[]) {
    UNUSED(argc);

    if (argc != 3) {
        fprintf(stderr, "Usage: %s threads_nb seconds_to_run\n", basename(argv[0]));
        exit(1);
    }
    struct timespec start;
    bxitime_get(CLOCK_MONOTONIC, &start);

    char * fullprogname = strdup(argv[0]);
    char * progname = basename(fullprogname);
    char * filename = bxistr_new("/tmp/%s%s", progname, ".log");

    if( access(filename, F_OK) != -1) {
        unlink(filename);
    }

    
    bxilog_config_p config = bxilog_config_new(progname);
    bxilog_config_add_handler(config, BXILOG_FILE_HANDLER,
                              BXILOG_FILTERS_ALL_ALL,
                              progname, filename, BXI_TRUNC_OPEN_FLAGS);

//    bxilog_config_add_handler(config, BXILOG_FILE_HANDLER_STDIO,
//                              BXILOG_FILTERS_ALL_ALL,
//			      progname, filename);


    bxierr_p bxierr = bxilog_init(config);
    assert(bxierr_isok(bxierr));
//    logger->level = BXILOG_DEBUG;
    //fprintf(stderr, "Logging to file: %s\n", filename);

    int n = atoi(argv[1]);
    pthread_t threads[n];
    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, logging_thread, NULL);
    }

    sleep(atoi(argv[2]));
    AGAIN = false;
    struct stats_s * statss[n];
    for (int i = 0; i < n; i++) {
        struct stats_s * stats;
        pthread_join(threads[i], (void**)&stats);
        statss[i] = stats;
    }
    bxierr = bxilog_finalize(false);
    if (!bxierr_isok(bxierr)) {
        char * str = bxierr_str(bxierr);
        fprintf(stderr, "WARNING: bxilog finalization returned: %s", str);
        BXIFREE(str);
        bxierr_destroy(&bxierr);
    }


    display_stats(start, statss, n, filename);


    BXIFREE(fullprogname);
    BXIFREE(filename);
}
