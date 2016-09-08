/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: Oct 2, 2014
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include <CUnit/Basic.h>

#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/log.h"
#include "bxi/base/zmq.h"
#include "bxi/base/time.h"

SET_LOGGER(TEST_LOGGER, "test.bxizmq");

void test_bxizmq_generate_url() {

    bxierr_p err;

    char * url, *result;

    url = "inproc://something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "inproc:///something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc://something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc:///something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc://something/bar";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc:///something/bar";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "tcp://localhost:*";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_EQUAL(url, result);
    BXIFREE(result);

    url = "tcp://localhost:27182";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(TEST_LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(TEST_LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp("tcp://localhost:", result, strlen("tcp://localhost:")));
    BXIFREE(result);

    err = bxizmq_generate_new_url_from("pgm://wtf", &result);
    CU_ASSERT_TRUE(bxierr_isko(err));
    bxierr_destroy(&err);

}

typedef struct {
    size_t msg_nb;
    bool bind;
    size_t sync_nb;
    volatile bool quit;
    size_t urls_nb;
    char ** urls;
} thread_param_s;

typedef thread_param_s * thread_param_p;

static void * pub_thread(void * data) {

    thread_param_p param = (thread_param_p) data;

    void * ctx = NULL;
    bxierr_p err = bxizmq_context_new(&ctx);
    bxierr_abort_ifko(err);

    void * zocket;
    err = bxizmq_zocket_create(ctx, ZMQ_PUB, &zocket);
    bxierr_abort_ifko(err);

    for (size_t i = 0; i < param->urls_nb; i++) {
        if (param->bind) {
            int tcp_port = 0;
            err = bxizmq_zocket_bind(zocket, param->urls[i], &tcp_port);
            char * new_url = bxizmq_create_url_from(param->urls[i], tcp_port);
            param->urls[i] = new_url;
        } else {
            err = bxizmq_zocket_connect(zocket, param->urls[i]);
        }
    }
    bxierr_abort_ifko(err);

    err = bxizmq_sync_pub(ctx, zocket, "tcp://127.0.0.1:*", param->sync_nb, 60);
    bxierr_abort_ifko(err);

    for (size_t i = 0; i < param->msg_nb; i++) {
        char * str = bxistr_new("Message %zu", i);
        err = bxizmq_str_snd(str, zocket, 0, 0, 0);
        bxierr_abort_ifko(err);
        BXIFREE(str);
    }

    err = bxizmq_str_snd("NO MORE MESSAGE", zocket, 0, 0, 0);
    bxierr_abort_ifko(err);

    while(!param->quit) {
        bxierr_p tmp = bxitime_sleep(CLOCK_MONOTONIC, 0, 5e7);
        bxierr_report(&tmp, STDERR_FILENO);
    }

    err = bxizmq_zocket_destroy(zocket);
    bxierr_abort_ifko(err);

    err = bxizmq_context_destroy(&ctx);
    bxierr_abort_ifko(err);

    if (param->bind) {
        for (size_t i = 0; i < param->urls_nb; i++) {
            BXIFREE(param->urls[i]);
        }
    }
    return NULL;
}

static void * sub_thread(void * data) {
    thread_param_p param = (thread_param_p) data;

    BXIASSERT(TEST_LOGGER, 0 == param->msg_nb);

    void * ctx = NULL;
    bxierr_p err = bxizmq_context_new(&ctx);
    bxierr_abort_ifko(err);

    void * zocket;
    err = bxizmq_zocket_create(ctx, ZMQ_SUB, &zocket);
    bxierr_abort_ifko(err);

    err = bxizmq_zocket_setopt(zocket, ZMQ_SUBSCRIBE, "", 0);
    bxierr_abort_ifko(err);

    for (size_t i = 0; i < param->urls_nb; i++) {
        if (param->bind) {
            int port;
            err = bxizmq_zocket_bind(zocket, param->urls[i], &port);
            param->urls[i] = bxizmq_create_url_from(param->urls[i], port);
        } else {
            err = bxizmq_zocket_connect(zocket, param->urls[i]);
        }
    }
    bxierr_abort_ifko(err);

    err = bxizmq_sync_sub(ctx, zocket, param->sync_nb, 60);
    bxierr_abort_ifko(err);

    size_t remaining = param->sync_nb;
    while(0 < remaining) {
        char * str;
        err = bxizmq_str_rcv(zocket, 0, false, &str);
        bxierr_abort_ifko(err);

        if (0 == strcmp("NO MORE MESSAGE", str)) {
            remaining--;
        } else {
            param->msg_nb++;
        }
        BXIFREE(str);
    }

    err = bxizmq_zocket_destroy(zocket);
    bxierr_abort_ifko(err);

    err = bxizmq_context_destroy(&ctx);
    bxierr_abort_ifko(err);

    if (param->bind) {
        for (size_t i = 0; i < param->urls_nb; i++) {
            BXIFREE(param->urls[i]);
        }
    }

    return NULL;
}

void test_1pub_1sub_sync() {
    char * sub_url = "tcp://127.0.0.1:*";
    thread_param_p sub_param = bximem_calloc(sizeof(*sub_param));
    sub_param->msg_nb = 0;
    sub_param->urls_nb = 1;
    sub_param->urls = bximem_calloc(sub_param->urls_nb*sizeof(*sub_param->urls));
    sub_param->urls[0] = sub_url;
    sub_param->bind = true;
    sub_param->sync_nb = 1;
    sub_param->quit = false;

    // The urls will be changed while binding
    char * old_url = sub_param->urls[0];

    pthread_t sub;
    int rc = pthread_create(&sub, NULL, sub_thread, sub_param);
    bxiassert(0 == rc);

    while (old_url == sub_param->urls[0]) {
        ; // Just wait
    }


    thread_param_p pub_param = bximem_calloc(sizeof(*pub_param));
    pub_param->msg_nb = 13;
    pub_param->urls_nb = 1;
    pub_param->urls = bximem_calloc(pub_param->urls_nb*sizeof(*pub_param->urls));
    pub_param->urls[0] = sub_param->urls[0];
    pub_param->bind = false;
    pub_param->sync_nb = 1;
    pub_param->quit = false;

    pthread_t pub;
    rc = pthread_create(&pub, NULL, pub_thread, pub_param);
    bxiassert(0 == rc);

    pthread_join(sub, NULL);
    bxiassert(0 == rc);

    pub_param->quit = true;
    rc = pthread_join(pub, NULL);
    bxiassert(0 == rc);

    OUT(TEST_LOGGER,
        "Nb published: %zu, Nb received: %zu",
        pub_param->msg_nb, sub_param->msg_nb);
    CU_ASSERT_EQUAL(pub_param->msg_nb, sub_param->msg_nb);

    BXIFREE(sub_param->urls);
    BXIFREE(sub_param);
    BXIFREE(pub_param->urls);
    BXIFREE(pub_param);
}

void test_2pub_1sub_sync() {
    char * sub_url = "tcp://127.0.0.1:*";
    thread_param_p sub_param = bximem_calloc(sizeof(*sub_param));
    sub_param->msg_nb = 0;

    sub_param->urls = bximem_calloc(1 * sizeof(*sub_param->urls));
    sub_param->urls[0] = sub_url;
    sub_param->urls_nb = 1;
    sub_param->bind = true;
    sub_param->sync_nb = 2;
    sub_param->quit = false;

    // The urls will be changed while binding
    char * old_url = sub_param->urls[0];

    pthread_t sub;
    int rc = pthread_create(&sub, NULL, sub_thread, sub_param);
    bxiassert(0 == rc);

    while (old_url == sub_param->urls[0]) {
        ; // Just wait
    }

    char * pub_url = sub_param->urls[0];
    thread_param_s * pubs_param = bximem_calloc(2 * sizeof(*pubs_param));
    pubs_param[0].msg_nb = 7;
    pubs_param[0].urls = bximem_calloc(1 * sizeof(*pubs_param[0].urls));
    pubs_param[0].urls[0] = pub_url;
    pubs_param[0].urls_nb = 1;
    pubs_param[0].bind = false;
    pubs_param[0].sync_nb = 1;
    pubs_param[0].quit = false;

    pubs_param[1].msg_nb = 5;
    pubs_param[1].urls = bximem_calloc(1 * sizeof(*pubs_param[1].urls));
    pubs_param[1].urls[0] = pub_url;
    pubs_param[1].urls_nb = 1;
    pubs_param[1].bind = false;
    pubs_param[1].sync_nb = 1;
    pubs_param[1].quit = false;

    pthread_t pub[2];
    rc = pthread_create(&pub[0], NULL, pub_thread, &pubs_param[0]);
    bxiassert(0 == rc);

    rc = pthread_create(&pub[1], NULL, pub_thread, &pubs_param[1]);
    bxiassert(0 == rc);


    pthread_join(sub, NULL);
    bxiassert(0 == rc);

    pubs_param[0].quit = true;
    pubs_param[1].quit = true;

    rc = pthread_join(pub[0], NULL);
    bxiassert(0 == rc);
    rc = pthread_join(pub[1], NULL);
    bxiassert(0 == rc);

    size_t total = pubs_param[0].msg_nb + pubs_param[1].msg_nb;
    OUT(TEST_LOGGER,
        "Nb published: %zu, Nb received: %zu",
        total, sub_param->msg_nb);

    CU_ASSERT_EQUAL(total, sub_param->msg_nb);

    BXIFREE(sub_param->urls);
    BXIFREE(sub_param);
    BXIFREE(pubs_param[0].urls);
    BXIFREE(pubs_param[1].urls);
    BXIFREE(pubs_param);
}

void test_2pub_2sub_sync() {
    char * sub_url = "tcp://127.0.0.1:*";
    thread_param_s * subs_param = bximem_calloc(2 * sizeof(*subs_param));

    subs_param[0].msg_nb = 0;
    subs_param[0].urls_nb = 1;
    subs_param[0].urls = bximem_calloc(subs_param[0].urls_nb * sizeof(*subs_param[0].urls));
    subs_param[0].urls[0] = sub_url;
    subs_param[0].bind = true;
    subs_param[0].sync_nb = 2;
    subs_param[0].quit = false;

    subs_param[1].msg_nb = 0;
    subs_param[1].urls_nb = 1;
    subs_param[1].urls = bximem_calloc(subs_param[1].urls_nb * sizeof(*subs_param[1].urls));
    subs_param[1].urls[0] = sub_url;
    subs_param[1].bind = true;
    subs_param[1].sync_nb = 1;
    subs_param[1].quit = false;


    // The urls will be changed while binding
    char * old_urls[] = {subs_param[0].urls[0], subs_param[1].urls[0]};

    pthread_t subs[2];
    int rc = pthread_create(&subs[0], NULL, sub_thread, &subs_param[0]);
    bxiassert(0 == rc);

    rc = pthread_create(&subs[1], NULL, sub_thread, &subs_param[1]);
    bxiassert(0 == rc);

    while (old_urls[0] == subs_param[0].urls[0] || old_urls[1] == subs_param[1].urls[0]) {
        ; // Just wait
    }

    thread_param_s * pubs_param = bximem_calloc(2 * sizeof(*pubs_param));
    pubs_param[0].msg_nb = 7;
    pubs_param[0].urls_nb = 1;
    pubs_param[0].urls = bximem_calloc(pubs_param[0].urls_nb * sizeof(*pubs_param[0].urls));
    pubs_param[0].urls[0] = subs_param[0].urls[0];
    pubs_param[0].bind = false;
    pubs_param[0].sync_nb = 1;
    pubs_param[0].quit = false;

    pubs_param[1].msg_nb = 5;
    pubs_param[1].urls_nb = 2;
    pubs_param[1].urls = bximem_calloc(pubs_param[1].urls_nb * sizeof(*pubs_param[1].urls));
    pubs_param[1].urls[0] = subs_param[0].urls[0];
    pubs_param[1].urls[1] = subs_param[1].urls[0];
    pubs_param[1].bind = false;
    pubs_param[1].sync_nb = 2;
    pubs_param[1].quit = false;

    pthread_t pub[2];
    rc = pthread_create(&pub[0], NULL, pub_thread, &pubs_param[0]);
    bxiassert(0 == rc);

    rc = pthread_create(&pub[1], NULL, pub_thread, &pubs_param[1]);
    bxiassert(0 == rc);


    pthread_join(subs[0], NULL);
    bxiassert(0 == rc);

    pthread_join(subs[1], NULL);
    bxiassert(0 == rc);

    pubs_param[0].quit = true;
    pubs_param[1].quit = true;

    rc = pthread_join(pub[0], NULL);
    bxiassert(0 == rc);
    rc = pthread_join(pub[1], NULL);
    bxiassert(0 == rc);

    size_t total = pubs_param[0].msg_nb + pubs_param[1].msg_nb;
    OUT(TEST_LOGGER,
        "pubs[0]=%zu, pubs[1]=%zu, total=%zu, subs[0]=%zu, subs[1]=%zu",
        pubs_param[0].msg_nb, pubs_param[1].msg_nb, total,
        subs_param[0].msg_nb, subs_param[1].msg_nb);

    CU_ASSERT_EQUAL(total, subs_param[0].msg_nb);
    CU_ASSERT_EQUAL(pubs_param[1].msg_nb, subs_param[1].msg_nb);

    for (size_t i = 0; i < subs_param[0].urls_nb; i++) {
        BXIFREE(subs_param[0].urls);
    }
    for (size_t i = 0; i < subs_param[1].urls_nb; i++) {
        BXIFREE(subs_param[1].urls);
    }

    for (size_t i = 0; i < pubs_param[0].urls_nb; i++) {
        BXIFREE(pubs_param[0].urls);
    }
    for (size_t i = 0; i < pubs_param[1].urls_nb; i++) {
        BXIFREE(pubs_param[1].urls);
    }
    BXIFREE(subs_param);
    BXIFREE(pubs_param);
}

