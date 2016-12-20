/*
 * common.h
 *
 *  Created on: Dec 20, 2016
 *      Author: vigneras
 */

#ifndef COMMON_H_
#define COMMON_H_


struct stats_s {
    double min_duration;
    double max_duration;
    double total_duration;
    size_t n;
};



void display_stats(const struct timespec start,
                   struct stats_s ** statss, size_t n,
                   const char * filename);

#endif /* COMMON_H_ */
