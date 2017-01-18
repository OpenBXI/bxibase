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
#include <sys/types.h>
#include <sys/wait.h>

#include <CUnit/Basic.h>

#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/log.h"
#include "bxi/base/zmq.h"
#include "bxi/base/time.h"

SET_LOGGER(LOGGER, "test.bxizmq");


extern char * PROGNAME;
extern char * FULLFILENAME;


void test_bxizmq_generate_url() {

    bxierr_p err;

    char * url, *result;

    url = "inproc://something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "inproc:///something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc://something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc:///something";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc://something/bar";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "ipc:///something/bar";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp(url, result, strlen(url)));
    BXIFREE(result);

    url = "tcp://localhost:*";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_EQUAL(url, result);
    BXIFREE(result);

    url = "tcp://localhost:27182";
    err = bxizmq_generate_new_url_from(url, &result);
    BXILOG_REPORT_KEEP(LOGGER, BXILOG_ERROR, err, "Unexpected problem...");
    OUT(LOGGER, "url=%s, result=%s", url, result);
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_STRING_NOT_EQUAL(url, result);
    CU_ASSERT_EQUAL(0, strncmp("tcp://localhost:", result, strlen("tcp://localhost:")));
    BXIFREE(result);

    err = bxizmq_generate_new_url_from("pgm://wtf", &result);
    CU_ASSERT_TRUE(bxierr_isko(err));
    bxierr_destroy(&err);

}

static char * _get_tmp_filename(const char * const infix) {
    char * tmpdir = getenv("TMPDIR");
    if (NULL == tmpdir) tmpdir = "/tmp";
    char * _tmp_file = bxistr_new("%s/test-%s-XXXXXX", tmpdir, infix);
    int fd = mkstemp(_tmp_file);
    bxiassert(-1 != fd);
    int rc = close(fd);
    bxiassert(0 == rc);
    return _tmp_file;
}

typedef struct {
    size_t msg_nb;
    bool bind;
    size_t sync_nb;
    char * quit_url;
    size_t urls_nb;
    char ** urls;
    bool terminated;
} thread_param_s;

typedef thread_param_s * thread_param_p;

static void * pub_thread(void * data) {
    OUT(LOGGER, "Starting a publisher");
    thread_param_p param = (thread_param_p) data;

    param->terminated = false;

    TRACE(LOGGER, "Creating publisher zmq context");
    void * ctx = NULL;
    bxierr_p err = bxizmq_context_new(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Creating publisher zmq PUB socket");
    void * zocket;
    err = bxizmq_zocket_create(ctx, ZMQ_PUB, &zocket);
    BXIABORT_IFKO(LOGGER, err);

    for (size_t i = 0; i < param->urls_nb; i++) {
        if (param->bind) {
            int tcp_port = 0;
            err = bxizmq_zocket_bind(zocket, param->urls[i], &tcp_port);
            char * new_url = bxizmq_create_url_from(param->urls[i], tcp_port);
            DEBUG(LOGGER, "Binded to %s", new_url);
            param->urls[i] = new_url;
        } else {
            err = bxizmq_zocket_connect(zocket, param->urls[i]);
            DEBUG(LOGGER, "Connected to %s", param->urls[i]);
        }
    }
    BXIABORT_IFKO(LOGGER, err);

    OUT(LOGGER, "Synchronizing with %zu subscribers", param->sync_nb);
    err = bxizmq_sync_pub_many(ctx, zocket, "tcp://127.0.0.1:*", param->sync_nb, 60);
    BXIABORT_IFKO(LOGGER, err);

    OUT(LOGGER, "Sending %zu messages", param->msg_nb);
    for (size_t i = 0; i < param->msg_nb; i++) {
        char * str = bxistr_new("Message %zu", i);
        err = bxizmq_str_snd(str, zocket, 0, 0, 0);
        BXIABORT_IFKO(LOGGER, err);
        BXIFREE(str);
    }

    OUT(LOGGER, "Sending final message");
    err = bxizmq_str_snd("NO MORE MESSAGE", zocket, 0, 0, 0);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Creating quit sub socket");
    void * quit_zocket;
    err = bxizmq_zocket_create(ctx, ZMQ_SUB, &quit_zocket);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Subscribing to everything");
    err = bxizmq_zocket_setopt(quit_zocket, ZMQ_SUBSCRIBE, "", 0);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Connecting to %s", param->quit_url);
    err = bxizmq_zocket_connect(quit_zocket, param->quit_url);
    BXIABORT_IFKO(LOGGER, err);

    char * quit_str = NULL;
    err = bxizmq_str_rcv(quit_zocket, 0, 0, &quit_str);
    BXIABORT_IFKO(LOGGER, err);

    DEBUG(LOGGER, "Received: %s", quit_str);

    BXIASSERT(LOGGER, 0 == strncmp("PUB MUST QUIT", quit_str, strlen("PUB MUST QUIT")));
    BXIFREE(quit_str);

    TRACE(LOGGER, "Destroying quit zocket");
    err = bxizmq_zocket_destroy(quit_zocket);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Destroying pub zocket");
    err = bxizmq_zocket_destroy(zocket);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Destroying context");
    err = bxizmq_context_destroy(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    if (param->bind) {
        for (size_t i = 0; i < param->urls_nb; i++) {
            BXIFREE(param->urls[i]);
        }
    }

    param->terminated = true;

    return NULL;
}

static void * sub_thread(void * data) {
    OUT(LOGGER, "Starting a subscriber");

    thread_param_p param = (thread_param_p) data;

    param->terminated = false;

    BXIASSERT(LOGGER, 0 == param->msg_nb);
    TRACE(LOGGER, "Creating context");
    void * ctx = NULL;
    bxierr_p err = bxizmq_context_new(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Creating sub zocket");
    void * zocket;
    err = bxizmq_zocket_create(ctx, ZMQ_SUB, &zocket);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Subscribing to all");
    err = bxizmq_zocket_setopt(zocket, ZMQ_SUBSCRIBE, "", 0);
    BXIABORT_IFKO(LOGGER, err);

    for (size_t i = 0; i < param->urls_nb; i++) {
        if (param->bind) {
            int port;
            err = bxizmq_zocket_bind(zocket, param->urls[i], &port);
            param->urls[i] = bxizmq_create_url_from(param->urls[i], port);
            DEBUG(LOGGER, "Binded to %s", param->urls[i]);
        } else {
            err = bxizmq_zocket_connect(zocket, param->urls[i]);
            DEBUG(LOGGER, "Connected to %s", param->urls[i]);
        }
    }
    BXIABORT_IFKO(LOGGER, err);

    OUT(LOGGER, "Synchronizing with %zu publishers", param->sync_nb);
    err = bxizmq_sync_sub_many(ctx, zocket, param->sync_nb, 60);
    BXIABORT_IFKO(LOGGER, err);

    OUT(LOGGER, "Getting messages from %zu publishers", param->sync_nb);
    size_t remaining = param->sync_nb;
    while(0 < remaining) {
        char * str;
        err = bxizmq_str_rcv(zocket, 0, false, &str);
        BXIABORT_IFKO(LOGGER, err);

        if (0 == strcmp("NO MORE MESSAGE", str)) {
            OUT(LOGGER, "Sync done. Remaining: %zu", remaining);
            remaining--;
        } else {
            param->msg_nb++;
        }
        BXIFREE(str);
    }
    OUT(LOGGER,
        "Sync fully completed. Number of received messages: %zu. Destroying zocket",
        param->msg_nb);
    err = bxizmq_zocket_destroy(zocket);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Destroying context.");
    err = bxizmq_context_destroy(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    if (param->bind) {
        for (size_t i = 0; i < param->urls_nb; i++) {
            BXIFREE(param->urls[i]);
        }
    }

    param->terminated = true;

    return NULL;
}

void test_1pub_1sub_sync() {
    char * sub_tmp_file = _get_tmp_filename(__FUNCTION__);
    char * sub_url = bxistr_new("ipc://%s", sub_tmp_file);

    char * quit_tmp_file = _get_tmp_filename(__FUNCTION__);
    char * quit_url = bxistr_new("ipc://%s", quit_tmp_file);

    thread_param_p sub_param = bximem_calloc(sizeof(*sub_param));
    sub_param->msg_nb = 0;
    sub_param->urls_nb = 1;
    sub_param->urls = bximem_calloc(sub_param->urls_nb*sizeof(*sub_param->urls));
    sub_param->urls[0] = sub_url;
    sub_param->bind = true;
    sub_param->sync_nb = 1;
    sub_param->quit_url = quit_url;

    OUT(LOGGER, "Launching a sub");
    pthread_t sub;
    int rc = pthread_create(&sub, NULL, sub_thread, sub_param);
    BXIASSERT(LOGGER, 0 == rc);

    thread_param_p pub_param = bximem_calloc(sizeof(*pub_param));
    pub_param->msg_nb = 13;
    pub_param->urls_nb = 1;
    pub_param->urls = bximem_calloc(pub_param->urls_nb*sizeof(*pub_param->urls));
    pub_param->urls[0] = sub_param->urls[0];
    pub_param->bind = false;
    pub_param->sync_nb = 1;
    pub_param->quit_url = quit_url;

    OUT(LOGGER, "Launching a pub");
    pthread_t pub;
    rc = pthread_create(&pub, NULL, pub_thread, pub_param);
    BXIASSERT(LOGGER, 0 == rc);

    TRACE(LOGGER, "Creating a zmq context");
    void * ctx;
    bxierr_p err = bxizmq_context_new(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Creating and binding the quit zocket to %s", quit_url);
    void * quit_zock = NULL;
    err = bxizmq_zocket_create_binded(ctx, ZMQ_PUB, quit_url, NULL, &quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    while (true) {
        DEBUG(LOGGER, "Requesting a pub quit");
        err = bxizmq_str_snd("PUB MUST QUIT", quit_zock, 0, 0, 0);
        BXIABORT_IFKO(LOGGER, err);

        if (pub_param->terminated) break;

        FINE(LOGGER, "pub has not terminated yet, looping again");
        err = bxitime_sleep(CLOCK_MONOTONIC, 0, 5e7);
        BXIABORT_IFKO(LOGGER, err);
    }

    TRACE(LOGGER, "Destroying quit zocket");
    err = bxizmq_zocket_destroy(quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Destroying zmq context");
    err = bxizmq_context_destroy(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    while (true) {
        DEBUG(LOGGER, "Waiting for a sub quit");

        if (sub_param->terminated) {
            pthread_join(sub, NULL);
            break;
        }

        FINE(LOGGER, "sub has not terminated yet, looping again.");
    }

    size_t msg_nb = sub_param->msg_nb;

    OUT(LOGGER,
        "Nb published: %zu, Nb received: %zu",
        pub_param->msg_nb, msg_nb);
    CU_ASSERT_EQUAL(pub_param->msg_nb, msg_nb);

    BXIFREE(sub_param->urls);
    BXIFREE(sub_param);
    BXIFREE(pub_param->urls);
    BXIFREE(pub_param);

    BXIFREE(sub_url);
    unlink(sub_tmp_file);
    BXIFREE(sub_tmp_file);

    BXIFREE(quit_url);
    unlink(quit_tmp_file);
    BXIFREE(quit_tmp_file);
}

void test_2pub_1sub_sync() {
    char * sub_tmp_file = _get_tmp_filename(__FUNCTION__);
    char * sub_url = bxistr_new("ipc://%s", sub_tmp_file);

    char * quit_tmp_file = _get_tmp_filename(__FUNCTION__);
    char * quit_url = bxistr_new("ipc://%s", quit_tmp_file);

    thread_param_p sub_param = bximem_calloc(sizeof(*sub_param));
    sub_param->msg_nb = 0;

    sub_param->urls = bximem_calloc(1 * sizeof(*sub_param->urls));
    sub_param->urls[0] = sub_url;
    sub_param->urls_nb = 1;
    sub_param->bind = true;
    sub_param->sync_nb = 2;
    sub_param->quit_url = quit_url;

    OUT(LOGGER, "Launching a sub");
    pthread_t sub;
    int rc = pthread_create(&sub, NULL, sub_thread, sub_param);
    BXIASSERT(LOGGER, 0 == rc);

    char * pub_url = sub_param->urls[0];
    thread_param_s * pubs_param = bximem_calloc(2 * sizeof(*pubs_param));
    pubs_param[0].msg_nb = 7;
    pubs_param[0].urls = bximem_calloc(1 * sizeof(*pubs_param[0].urls));
    pubs_param[0].urls[0] = pub_url;
    pubs_param[0].urls_nb = 1;
    pubs_param[0].bind = false;
    pubs_param[0].sync_nb = 1;
    pubs_param[0].quit_url = quit_url;

    pubs_param[1].msg_nb = 5;
    pubs_param[1].urls = bximem_calloc(1 * sizeof(*pubs_param[1].urls));
    pubs_param[1].urls[0] = pub_url;
    pubs_param[1].urls_nb = 1;
    pubs_param[1].bind = false;
    pubs_param[1].sync_nb = 1;
    pubs_param[1].quit_url = quit_url;

    OUT(LOGGER, "Launching 2 publishers");
    pthread_t pub[2];
    rc = pthread_create(&pub[0], NULL, pub_thread, &pubs_param[0]);
    bxiassert(0 == rc);

    rc = pthread_create(&pub[1], NULL, pub_thread, &pubs_param[1]);
    bxiassert(0 == rc);

    TRACE(LOGGER, "Creating a zmq context");
    void * ctx;
    bxierr_p err = bxizmq_context_new(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Creating and binding the quit zocket to %s", quit_url);
    void * quit_zock = NULL;
    err = bxizmq_zocket_create_binded(ctx, ZMQ_PUB, quit_url, NULL, &quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    while (true) {
        DEBUG(LOGGER, "Requesting pub quit");
        err = bxizmq_str_snd("PUB MUST QUIT", quit_zock, 0, 0, 0);
        BXIABORT_IFKO(LOGGER, err);

        if (pubs_param[0].terminated && pubs_param[1].terminated) break;

        FINE(LOGGER, "Some pub have not terminated yet, looping again");
        err = bxitime_sleep(CLOCK_MONOTONIC, 0, 5e7);
        BXIABORT_IFKO(LOGGER, err);
    }

    TRACE(LOGGER, "Destroying quit zocket");
    err = bxizmq_zocket_destroy(quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Destroying zmq context");
    err = bxizmq_context_destroy(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    while (true) {
        DEBUG(LOGGER, "Waiting for a sub quit");

        if (sub_param->terminated) break;

        FINE(LOGGER, "sub has not terminated yet, looping again.");
    }

    size_t total = pubs_param[0].msg_nb + pubs_param[1].msg_nb;
    OUT(LOGGER,
        "Nb published: %zu, Nb received: %zu",
        total, sub_param->msg_nb);

    CU_ASSERT_EQUAL(total, sub_param->msg_nb);

    BXIFREE(sub_param->urls);
    BXIFREE(sub_param);
    BXIFREE(pubs_param[0].urls);
    BXIFREE(pubs_param[1].urls);
    BXIFREE(pubs_param);

    BXIFREE(sub_url);
    unlink(sub_tmp_file);
    BXIFREE(sub_tmp_file);

    BXIFREE(quit_url);
    unlink(quit_tmp_file);
    BXIFREE(quit_tmp_file);
}

void test_2pub_2sub_sync() {
    char * sub_tmp_file1 = _get_tmp_filename(__FUNCTION__);
    char * sub_url1 = bxistr_new("ipc://%s", sub_tmp_file1);

    char * sub_tmp_file2 = _get_tmp_filename(__FUNCTION__);
    char * sub_url2 = bxistr_new("ipc://%s", sub_tmp_file2);

    char * quit_tmp_file = _get_tmp_filename(__FUNCTION__);
    char * quit_url = bxistr_new("ipc://%s", quit_tmp_file);

    thread_param_s * subs_param = bximem_calloc(2 * sizeof(*subs_param));

    subs_param[0].msg_nb = 0;
    subs_param[0].urls_nb = 1;
    subs_param[0].urls = bximem_calloc(subs_param[0].urls_nb * sizeof(*subs_param[0].urls));
    subs_param[0].urls[0] = sub_url1;
    subs_param[0].bind = true;
    subs_param[0].sync_nb = 2;
    subs_param[0].quit_url = quit_url;

    subs_param[1].msg_nb = 0;
    subs_param[1].urls_nb = 1;
    subs_param[1].urls = bximem_calloc(subs_param[1].urls_nb * sizeof(*subs_param[1].urls));
    subs_param[1].urls[0] = sub_url2;
    subs_param[1].bind = true;
    subs_param[1].sync_nb = 1;
    subs_param[1].quit_url = quit_url;

    OUT(LOGGER, "Starting 2 subs");
    pthread_t subs[2];
    int rc = pthread_create(&subs[0], NULL, sub_thread, &subs_param[0]);
    bxiassert(0 == rc);

    rc = pthread_create(&subs[1], NULL, sub_thread, &subs_param[1]);
    bxiassert(0 == rc);

    thread_param_s * pubs_param = bximem_calloc(2 * sizeof(*pubs_param));
    pubs_param[0].msg_nb = 7;
    pubs_param[0].urls_nb = 1;
    pubs_param[0].urls = bximem_calloc(pubs_param[0].urls_nb * sizeof(*pubs_param[0].urls));
    pubs_param[0].urls[0] = subs_param[0].urls[0];
    pubs_param[0].bind = false;
    pubs_param[0].sync_nb = 1;
    pubs_param[0].quit_url = quit_url;

    pubs_param[1].msg_nb = 5;
    pubs_param[1].urls_nb = 2;
    pubs_param[1].urls = bximem_calloc(pubs_param[1].urls_nb * sizeof(*pubs_param[1].urls));
    pubs_param[1].urls[0] = subs_param[0].urls[0];
    pubs_param[1].urls[1] = subs_param[1].urls[0];
    pubs_param[1].bind = false;
    pubs_param[1].sync_nb = 2;
    pubs_param[1].quit_url = quit_url;

    OUT(LOGGER, "Starting 2 pubs");
    pthread_t pub[2];
    rc = pthread_create(&pub[0], NULL, pub_thread, &pubs_param[0]);
    bxiassert(0 == rc);

    rc = pthread_create(&pub[1], NULL, pub_thread, &pubs_param[1]);
    bxiassert(0 == rc);

    TRACE(LOGGER, "Creating a zmq context");
    void * ctx;
    bxierr_p err = bxizmq_context_new(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Creating and binding the quit zocket to %s", quit_url);
    void * quit_zock = NULL;
    err = bxizmq_zocket_create_binded(ctx, ZMQ_PUB, quit_url, NULL, &quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    while (true) {
        DEBUG(LOGGER, "Requesting pub quit");
        err = bxizmq_str_snd("PUB MUST QUIT", quit_zock, 0, 0, 0);
        BXIABORT_IFKO(LOGGER, err);

        if (pubs_param[0].terminated && pubs_param[1].terminated) break;

        FINE(LOGGER, "Some pub have not terminated yet, looping again");
        err = bxitime_sleep(CLOCK_MONOTONIC, 0, 5e7);
        BXIABORT_IFKO(LOGGER, err);
    }

    TRACE(LOGGER, "Destroying quit zocket");
    err = bxizmq_zocket_destroy(quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Destroying zmq context");
    err = bxizmq_context_destroy(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    while (true) {
        DEBUG(LOGGER, "Waiting for a sub quit");

        if (subs_param[0].terminated && subs_param[1].terminated) break;

        FINE(LOGGER, "sub has not terminated yet, looping again.");
    }

    size_t total = pubs_param[0].msg_nb + pubs_param[1].msg_nb;

    OUT(LOGGER,
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

    BXIFREE(sub_url1);
    unlink(sub_tmp_file1);
    BXIFREE(sub_tmp_file1);

    BXIFREE(sub_url2);
    unlink(sub_tmp_file2);
    BXIFREE(sub_tmp_file2);

    BXIFREE(quit_url);
    unlink(quit_tmp_file);
    BXIFREE(quit_tmp_file);
}


void test_1pub_1sub_sync_fork() {
    char * sub_tmp_file = _get_tmp_filename(__FUNCTION__);
    char * sub_url = bxistr_new("ipc://%s", sub_tmp_file);

    char * quit_tmp_file = _get_tmp_filename(__FUNCTION__);
    char * quit_url = bxistr_new("ipc://%s", quit_tmp_file);

    thread_param_p sub_param = bximem_calloc(sizeof(*sub_param));
    sub_param->msg_nb = 0;
    sub_param->urls_nb = 1;
    sub_param->urls = bximem_calloc(sub_param->urls_nb*sizeof(*sub_param->urls));
    sub_param->urls[0] = sub_url;
    sub_param->bind = true;
    sub_param->sync_nb = 1;
    sub_param->quit_url = quit_url;

    OUT(LOGGER, "Forking a subscriber");
    errno = 0;
    pid_t sub = fork();
    switch (sub) {
        case -1: bxierr_abort_ifko(bxierr_errno("Can't fork")); break;
        case 0: { // We are in the child
            bxilog_config_p config = bxilog_unit_test_config("sub",
                                                             FULLFILENAME,
                                                             BXI_APPEND_OPEN_FLAGS);
            bxierr_p err = bxilog_init(config);
            bxierr_report(&err, STDERR_FILENO);
            err = bxilog_install_sighandler();
            CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

            sub_thread(sub_param);

            err = bxilog_finalize();
            bxierr_report(&err, STDERR_FILENO);
            exit((int) sub_param->msg_nb);
        }
    }
    // We are in the parent

    thread_param_p pub_param = bximem_calloc(sizeof(*pub_param));
    pub_param->msg_nb = 13;
    pub_param->urls_nb = 1;
    pub_param->urls = bximem_calloc(pub_param->urls_nb*sizeof(*pub_param->urls));
    pub_param->urls[0] = sub_param->urls[0];
    pub_param->bind = false;
    pub_param->sync_nb = 1;
    pub_param->quit_url = quit_url;

    OUT(LOGGER, "Forking a publisher");
    pid_t pub = fork();
    switch (pub) {
        case -1: bxierr_abort_ifko(bxierr_errno("Can't fork")); break;
        case 0: { // We are in the child
            bxilog_config_p config = bxilog_unit_test_config("pub",
                                                             FULLFILENAME,
                                                             BXI_APPEND_OPEN_FLAGS);
            bxierr_p err = bxilog_init(config);
            bxierr_report(&err, STDERR_FILENO);
            err = bxilog_install_sighandler();
            CU_ASSERT_TRUE_FATAL(bxierr_isok(err));

            pub_thread(pub_param);

            err = bxilog_finalize();
            bxierr_report(&err, STDERR_FILENO);
            exit((int) pub_param->msg_nb);
        }
    }

    // We are in the parent
    TRACE(LOGGER, "Creating a zmq context");
    void * ctx;
    bxierr_p err = bxizmq_context_new(&ctx);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Creating and binding the quit zocket to %s", quit_url);
    void * quit_zock = NULL;
    err = bxizmq_zocket_create_binded(ctx, ZMQ_PUB, quit_url, NULL, &quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    int wstatus;
    int rc;
    while (true) {
        DEBUG(LOGGER, "Requesting a pub quit");
        err = bxizmq_str_snd("PUB MUST QUIT", quit_zock, 0, 0, 0);
        BXIABORT_IFKO(LOGGER, err);

        rc = waitpid(pub, &wstatus, WNOHANG);
        if (-1 == rc) bxierr_abort_ifko(bxierr_errno("Calling waitpid() failed"));

        if (pub == rc) {
            break;
        }
        FINE(LOGGER, "waitpid() returned %d. Looping again.", rc);
        err = bxitime_sleep(CLOCK_MONOTONIC, 0, 5e7);
        BXIABORT_IFKO(LOGGER, err);
    }

    DEBUG(LOGGER, "Raw exit code of publisher: %d", wstatus);
    BXIASSERT(LOGGER, WIFEXITED(wstatus));

    TRACE(LOGGER, "Destroying quit zocket");
    err = bxizmq_zocket_destroy(quit_zock);
    BXIABORT_IFKO(LOGGER, err);

    TRACE(LOGGER, "Destroying zmq context");
    err = bxizmq_context_destroy(&ctx);
    BXIABORT_IFKO(LOGGER, err);


    while (true) {
        DEBUG(LOGGER, "Waiting for a sub quit");
        rc = waitpid(sub, &wstatus, WNOHANG);
        if (-1 == rc) bxierr_abort_ifko(bxierr_errno("Calling waitpid() failed"));

        if (sub == rc) {
            break;
        }
        FINE(LOGGER, "waitpid() returned %d. Looping again.", rc);
    }

    DEBUG(LOGGER, "Raw exit code of subscriber: %d", wstatus);
    BXIASSERT(LOGGER, WIFEXITED(wstatus));

    int msg_nb = WEXITSTATUS(wstatus);

    OUT(LOGGER,
        "Nb published: %zu, Nb received: %d",
        pub_param->msg_nb, msg_nb);
    CU_ASSERT_EQUAL(pub_param->msg_nb, msg_nb);

    BXIFREE(sub_param->urls);
    BXIFREE(sub_param);
    BXIFREE(pub_param->urls);
    BXIFREE(pub_param);

    BXIFREE(sub_url);
    unlink(sub_tmp_file);
    BXIFREE(sub_tmp_file);

    BXIFREE(quit_url);
    unlink(quit_tmp_file);
    BXIFREE(quit_tmp_file);
}
