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

#include "zlog.h"

static volatile bool AGAIN = true;

void * logging_thread(void * param) {
    assert(param != NULL );
    zlog_category_t * c = zlog_get_category("my_cat");
    /*     *c = (zlog_category_t *) param;*/
    while (AGAIN) {
        zlog_error(c, "Logging something...");
    }
    return NULL;
}

int main(int argc, char * argv[]) {
    int rc;
    zlog_category_t *c;

    rc = zlog_init("zlog.conf");
    if (rc) {
      printf("init failed\n");
      return -1;
    }
    
    /* c = zlog_get_category("my_cat"); */
    /* if (!c) { */
    /*   printf("get cat fail\n"); */
    /*   zlog_fini(); */
    /*   return -2; */
    /* } */
    int n = atoi(argv[1]);
    pthread_t threads[n];
    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, logging_thread, &c);
    }

    sleep(atoi(argv[2]));
    AGAIN = false;
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }
    zlog_fini();
}
