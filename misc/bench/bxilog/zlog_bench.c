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
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <libgen.h>

#include <pthread.h>
#include <stdbool.h>
#include <zmq.h>
#include <time.h>
#include <assert.h>
#include <error.h>
#include <errno.h>
#include <float.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "zlog.h"

#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/time.h"


static volatile bool AGAIN = true;

struct stats_s {
    double min_duration;
    double max_duration;
    double total_duration;
    size_t n;
};

static int levels[6] = {ZLOG_LEVEL_DEBUG,
                        ZLOG_LEVEL_INFO,
                        ZLOG_LEVEL_NOTICE,
                        ZLOG_LEVEL_WARN,
                        ZLOG_LEVEL_ERROR,
                        ZLOG_LEVEL_FATAL,
};

static inline void benched_log(zlog_category_t * category,
                               int level,
                               char * str, struct stats_s *  stats) {

    struct timespec start;
    bxierr_p err = bxitime_get(CLOCK_MONOTONIC, &start);
    bxierr_abort_ifko(err);

    zlog(category,
         (char *)__FILE__, ARRAYLEN(__FILE__),
         __func__, ARRAYLEN(__func__), __LINE__,
         level,
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

    zlog_category_t * c = zlog_get_category("my_cat");

    while (AGAIN) {
        if (0 == stats->n) {
            benched_log(c, ZLOG_LEVEL_DEBUG, "Logging something useless", stats);
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
        benched_log(c, levels[stats->n % 6], str, stats);
        BXIFREE(min_str);
        BXIFREE(max_str);
        BXIFREE(avg_str);
        BXIFREE(str);
    }
    return stats;
}


int main(int argc, char * argv[]) {

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

    int rc;
    zlog_category_t *c;

    rc = zlog_init("zlog.conf");
    if (rc) {
        fprintf(stderr, "init failed\n");
        return -1;
    }

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

    zlog_fini();

    struct stats_s global_stats = {.min_duration = DBL_MAX,
                                   .max_duration = DBL_MIN,
                                   .total_duration = 0,
                                   .n = 0,
    };
    bxitime_duration(CLOCK_MONOTONIC, start, &global_stats.total_duration);
    for (size_t i = 0; i < n; i++) {
        global_stats.min_duration = (statss[i]->min_duration < global_stats.min_duration) ?
                statss[i]->min_duration :
                global_stats.min_duration;

        global_stats.max_duration = (statss[i]->max_duration > global_stats.max_duration) ?
                statss[i]->max_duration :
                global_stats.max_duration;
        global_stats.n += statss[i]->n;
        BXIFREE(statss[i]);
    }


    char * min_str = bxitime_duration_str(global_stats.min_duration);
    char * max_str = bxitime_duration_str(global_stats.max_duration);
    char * avg_str = bxitime_duration_str(global_stats.total_duration / global_stats.n);
    char * total_str = bxitime_duration_str(global_stats.total_duration);

    struct stat sb;
    errno = 0;
    rc = stat(filename, &sb);
    size_t size;
    if (-1 == rc) {
        bxierr_p err = bxierr_errno("Calling stat(%s) failed", filename);
        bxierr_report(&err, STDERR_FILENO);
        size = 0;
    } else {
        size = sb.st_size;
    }

    printf("Total Time: %zu logs in %s - %g logs/s, min=%s/log, max=%s/log, average=%s/log\n",
           global_stats.n, total_str, global_stats.n / global_stats.total_duration,
           min_str, max_str, avg_str);
    printf("Total Size: %zu bytes in (overall) %s: %.1f MB/s\n",
           size, total_str, size/global_stats.total_duration/1024/1024);

    fprintf(stderr, "%zu\t%lf\t%.9lf\t%.9lf\t%.9zu\n",
               global_stats.n,
               global_stats.total_duration,
               global_stats.min_duration,
               global_stats.max_duration,
               size);

    BXIFREE(min_str);
    BXIFREE(max_str);
    BXIFREE(avg_str);
    BXIFREE(total_str);

    BXIFREE(fullprogname);
    BXIFREE(filename);


}
