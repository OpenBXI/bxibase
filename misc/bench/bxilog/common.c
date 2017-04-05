/*
 * common.c
 *
 *  Created on: Dec 20, 2016
 *      Author: vigneras
 */

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <float.h>
#include <sys/stat.h>

#include <bxi/base/time.h>

#include "common.h"


void display_stats(const struct timespec start,
                   struct stats_s ** statss, size_t n,
                   const char * filename) {

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
      int rc = stat(filename, &sb);
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

}
