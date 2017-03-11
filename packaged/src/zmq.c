/* -*- coding: utf-8 -*-
 ###############################################################################
 # Authors: Sébastien Miquée <sebastien.miquee@bull.net>
 #          Pierre Vignéras <pierre.vigneras@bull.net>
 # Created on: 2013/11/21
 # Contributors:
 ###############################################################################
 # Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <search.h>

#include <zmq.h>

#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

#define MAX_CONNECTION_TIMEOUT 1.0     // seconds
#define INPROC_PROTO "inproc"
#define TCP_PROTO "tcp"

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************
static bxierr_p _create_pub_sync_socket(void * zmq_ctx, void ** sync_zocket,
                                        const char * const pub_url,
                                        char ** const sync_url,
                                        int *tcp_port);
static bxierr_p _process_pub_snd(void * pub_zocket, const char * key, const char * url,
                                 struct timespec * last_send_date, double max_snd_delay);
static bxierr_p _process_pub_sync_msg(void * pub_zocket,
                                      void * sync_zocket,
                                      size_t * missing_almost,
                                      size_t * missing_go,
                                      const char * url);

static bxierr_p _sync_sub_send_pong(void * sub_zocket, void * sync_zocket);
static bxierr_p _process_sub_ping_msg(void * sub_zocket,
                                      void * sync_zocket,
                                      char * header,
                                      void ** already_pinged_root);
static bxierr_p _process_sub_ready_msg(void * const sync_zocket,
                                       size_t * const missing_ready_msg_nb,
                                       size_t pub_nb);
static bxierr_p _process_sub_last_msg(void * sync_zocket,
                                      size_t * missing_last_msg_nb,
                                      size_t pub_nb);

static bxierr_p _zmqerr(int errnum, const char * fmt, ...);

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************

/************************************** Context ***********************************/
bxierr_p bxizmq_context_new(void ** ctx) {
    void * context = NULL;

    context = zmq_ctx_new();
    if (NULL == context) {
      return bxierr_errno("ZMQ context creation failed");
    }

    *ctx = context;

    return BXIERR_OK;
}


bxierr_p bxizmq_context_destroy(void ** ctx) {
    int rc;

    if (NULL != *ctx) {
        errno = 0;
        rc = zmq_ctx_shutdown(*ctx);
        if (-1 == rc) {
            return bxierr_errno("Unable to shutdown ZMQ context");
        }
        errno = 0;
        do {
            rc = zmq_ctx_term(*ctx);
        } while (-1 == rc && EINTR == errno);

        if (-1 == rc) {
            return bxierr_errno("Unable to terminate ZMQ context");
        }
    }

    // NULLify the pointer
    *ctx = NULL;

    return BXIERR_OK;
}

/********************************** END Context ***********************************/

/*************************************** Zocket ***********************************/

bxierr_p bxizmq_zocket_create(void * const ctx, const int type, void ** result) {
    bxiassert(NULL != ctx);
    bxiassert(NULL != result);

    bxierr_p err = BXIERR_OK, err2;

    errno = 0;
    void * zocket = zmq_socket(ctx, type);
    if (NULL == zocket) {
        err2 = _zmqerr(errno,
                       "Can't create a zmq socket of type %d",
                       type);
        BXIERR_CHAIN(err, err2);
    } else {
        DBG("Zocket %p created\n", zocket);
    }

    // Yes, make all zeromq socket have a non-infinite linger period.
    int linger = BXIZMQ_DEFAULT_LINGER;
    int rc = zmq_setsockopt(zocket, ZMQ_LINGER, &linger, sizeof(linger));
    if (rc != 0) {
        err2 = _zmqerr(errno, "Can't set linger for socket %p", zocket);
        BXIERR_CHAIN(err, err2);
    }


    *result = zocket;
    return err;

}

bxierr_p bxizmq_zocket_destroy(void ** const zocket_p) {
    bxiassert(NULL != zocket_p);

    void * zocket = *zocket_p;

    if (NULL == zocket) return BXIERR_OK;

    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    int rc;
    do {
        rc = zmq_close(zocket);
    } while (rc == -1 && errno == EINTR);

    if (rc != 0) {
        err2 = _zmqerr(errno, "Can't close socket");
        BXIERR_CHAIN(err, err2);
    } else {
        DBG("Zocket %p destroyed\n", zocket);
    }
    *zocket_p = NULL;

    return err;
}

bxierr_p bxizmq_zocket_bind(void * const zocket,
                            const char * const url,
                            int  * affected_port) {
    bxiassert(NULL != url);
    bxierr_p err = BXIERR_OK, err2;

    unsigned long port = 0;

    char * translate_url = (char *)url;

    int rc = zmq_bind(zocket, translate_url);

    if (-1 == rc) {
        err2 = _zmqerr(errno, "Can't bind zmq socket on %s", translate_url);
        BXIERR_CHAIN(err, err2);

        //Try by translating hostname into IP
        if (0 != strncmp(translate_url, TCP_PROTO, ARRAYLEN(TCP_PROTO) - 1)) {
            return err;
        }

        char * elements[3];
        //Get the protocol the hostname and the port
        err2 = bxizmq_split_url(url, elements);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err)) {
            return err;
        }

        errno = 0;
        struct addrinfo hints, *servinfo;
        int rv;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // use either IPV4 or IPV6
        hints.ai_socktype = SOCK_STREAM;

        errno = 0;
        if (0 != (rv = getaddrinfo(elements[1] , NULL , &hints , &servinfo))) {
            err2 = bxierr_gen("Translation address error: getaddrinfo: %s",
                              gai_strerror(rv));
            BXIERR_CHAIN(err, err2);
        }

        if (bxierr_isko(err)) {
            return err;
        }

        errno = 0;
        char * ip = NULL;
        struct addrinfo *p;
        struct sockaddr_in *h;
        for(p = servinfo; p != NULL && ip == NULL; p = p->ai_next) {
            h = (struct sockaddr_in *) p->ai_addr;
            if (NULL == h) {
                continue;
            }
            ip = bxistr_new("%s", inet_ntoa(h->sin_addr));
        }
        if (NULL != ip) {
            translate_url = bxistr_new("%s://%s:%s", elements[0], ip, elements[2]);
            BXIFREE(ip);
            int rc = zmq_bind(zocket, translate_url);
            if (rc == -1) {
                //Neither url works
                err2 = _zmqerr(errno,
                               "Can't bind zmq socket on translated url %s",
                               translate_url);
                BXIERR_CHAIN(err, err2);
            } else {
                //It finally works
                bxierr_destroy(&err);
                err = BXIERR_OK;
            }
        }
    }

    char endpoint[512];
    endpoint[0] = '\0';

    if (NULL != affected_port && 0 != strncmp(translate_url,
                                              INPROC_PROTO,
                                              ARRAYLEN(INPROC_PROTO) - 1)) {
        bxiassert(NULL != zocket);
        size_t endpoint_len = 512;
        errno = 0 ;

        rc = zmq_getsockopt(zocket, ZMQ_LAST_ENDPOINT, &endpoint, &endpoint_len);
        if (-1 == rc) {
            if (errno == EINVAL || errno == ETERM || errno == ENOTSOCK || errno == EINTR) {
                err2 = _zmqerr(errno, "Can't retrieve the zocket endpoint option");
                BXIERR_CHAIN(err, err2);
            }
        }
    }

    if (0 < strlen(endpoint)) {
        char * ptr = strrchr(endpoint, ':');
        if (NULL == ptr) {
            err2 = bxierr_errno("Unable to retrieve the binded port number");
            BXIERR_CHAIN(err, err2);
            return err;
        }
        char * endptr = NULL;
        errno = 0;
        port = strtoul(ptr+1, &endptr, 10);
        if (ERANGE == errno) {
            err2 = bxierr_errno("Unable to parse integer in: '%s'", ptr+1);
            BXIERR_CHAIN(err, err2);
            return err;
        }
    }

    if (0 != port) {
        *affected_port = (int) port;
    }

    if (bxierr_isok(err)) {
        DBG("Zocket %p binded to %s\n", zocket, translate_url);
    }
    //if (NULL != affected_port)  *affected_port = rc;
    return err;
}

bxierr_p bxizmq_zocket_connect(void * zocket,
                               const char * const url) {
    bxiassert(NULL != zocket);
    bxiassert(NULL != url);
    bxierr_p err = BXIERR_OK, err2;

    struct timespec start;
    err2 = bxitime_get(CLOCK_MONOTONIC, &start);
    BXIERR_CHAIN(err, err2);

    long sleep = 128; // nanoseconds
    double delay = 0.0;
    int tries = 0;

    while (MAX_CONNECTION_TIMEOUT > delay) {
        errno = 0;
        int rc = zmq_connect(zocket, url);
        if (0 == rc) break;
        if (ECONNREFUSED != errno) {
            err2 = _zmqerr(errno, "Can't connect zmq socket on %s", url);
            BXIERR_CHAIN(err, err2);
            break;
        }
        err2 = bxitime_sleep(CLOCK_MONOTONIC, sleep / 1000000000,
                             sleep % 1000000000);
        BXIERR_CHAIN(err, err2);
        err2 = bxitime_duration(CLOCK_MONOTONIC, start, &delay);
        BXIERR_CHAIN(err, err2);
        sleep *= 2;
        tries++;
    }

    if (bxierr_isok(err)) {
        DBG("Zocket %p connected to %s\n", zocket, url);
    }
    return err;
}

bxierr_p bxizmq_disconnect(void * zocket, const char * url) {
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;

    int rc = zmq_disconnect(zocket, url);
    if (0 != rc) {
        err2 = _zmqerr(errno, "Calling disconnect on zocket %s failed", url);
        BXIERR_CHAIN(err, err2);
    }

    return err;
}

bxierr_p bxizmq_zocket_setopt(void * socket,
                              const int option_name,
                              const void * const option_value,
                              const size_t option_len
                             ) {
    bxiassert(NULL != socket);
    bxiassert(NULL != option_value);
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    const int rc = zmq_setsockopt(socket, option_name, option_value, option_len);
    if (rc != 0) {
        err2 = _zmqerr(errno, "Can't set option %d on zmq socket", option_name);
        BXIERR_CHAIN(err, err2);
        return err;
    }
#ifdef __DBG__
    if (ZMQ_SUBSCRIBE == option_name) DBG("subscribing: '%s'\n", (char*) option_value);
    if (ZMQ_UNSUBSCRIBE == option_name) DBG("unsubscribing: '%s'\n", (char*) option_value);
#endif
    return err;
}

bxierr_p bxizmq_zocket_getopt(void * socket,
                              const int option_name,
                              void * const option_value,
                              size_t * const option_len) {
    bxiassert(NULL != socket);
    bxiassert(NULL != option_value);

    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    const int rc = zmq_getsockopt(socket, option_name, option_value, option_len);
    if (rc != 0) {
        err2 = _zmqerr(errno, "Can't get option %d on zmq socket", option_name);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    return err;
}

bxierr_p bxizmq_zocket_create_connected(void *const ctx,
                                        const int type,
                                        const char *const url,
                                        void **zocket_p) {

    if (NULL == *zocket_p) {
        bxierr_p err = bxizmq_zocket_create(ctx, type, zocket_p);
        if (bxierr_isko(err)) return err;
    }

    return bxizmq_zocket_connect(*zocket_p, url);
}

bxierr_p bxizmq_zocket_create_binded(void *const ctx,
                                     const int type,
                                     const char *const url,
                                     int * affected_port,
                                     void **zocket_p) {

    if (NULL == *zocket_p) {
        bxierr_p err = bxizmq_zocket_create(ctx, type, zocket_p);
        if (bxierr_isko(err)) return err;
    }

    return bxizmq_zocket_bind(*zocket_p, url, affected_port);
}
/*********************************** END Zocket ***********************************/

/************************************* Msg ****************************************/


// Initialize a ZMQ message
bxierr_p bxizmq_msg_init(zmq_msg_t * zmsg) {
    errno = 0;
    int rc = zmq_msg_init(zmsg);
    if (rc != 0) {
        return _zmqerr(errno, "Problem during ZMQ message initialization");
    }
    return BXIERR_OK;
}


// Receive a ZMQ message
bxierr_p bxizmq_msg_rcv(void * const zocket,
                        zmq_msg_t * mzg,
                        int const flags) {
    errno = 0;
    int rc = zmq_msg_recv(mzg, zocket, flags);
    if (-1 == rc) {
        while (-1 == rc && EINTR == errno) rc = zmq_msg_recv(mzg, zocket, flags);
        if (-1 == rc) {
            if (EFSM == errno) return bxierr_new(BXIZMQ_FSM_ERR,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 "Can't receive a msg "
                                                 "through zsocket: %p:"
                                                 " ZMQ EFSM (man zmq_msg_recv)",
                                                 zocket);

            return _zmqerr(errno, "Can't receive a msg through zsocket %p", zocket);
        }
    }

    return BXIERR_OK;
}


/****************************************************************************************/
/*
 * Function that is used to retrieve the subtrees that are requested by a client
 * for a snapshot. The most important thing is that it allows the server to not wait
 * too much time for a subtree from a client (it may have died, or no subtree is
 * given -- so all has to be sent); thus it liberates the server to serve the other
 * clients.
 */
bxierr_p bxizmq_msg_rcv_async(void * const zocket, zmq_msg_t * const msg,
                              const size_t retries_max, const long delay_ns) {

    bxierr_p err = BXIERR_OK, err2;
    size_t n = retries_max;

    while (n-- > 0) {
        errno = 0;
        bxierr_p tmp = bxizmq_msg_rcv(zocket, msg, ZMQ_DONTWAIT);
        if (bxierr_isok(tmp)) return tmp;
        if (EAGAIN != tmp->code) return tmp;
        bxierr_destroy(&tmp);
        err2 = bxitime_sleep(CLOCK_MONOTONIC, 0, delay_ns);
        BXIERR_CHAIN(err, err2);
    }

    return bxierr_new(4941, NULL, NULL, NULL, err,
                      "No receipt after %zu retries. Giving up.",
                      retries_max);
}


// Close a ZMQ message
bxierr_p bxizmq_msg_close(zmq_msg_t * const zmsg) {
    errno = 0;
    int rc = zmq_msg_close(zmsg);
    if (rc != 0) return _zmqerr(errno, "Calling zmq_msg_close() failed");

    return BXIERR_OK;
}


// Copy a ZMQ message
bxierr_p bxizmq_msg_copy(zmq_msg_t * const zmsg_src, zmq_msg_t * const zmsg_dst){
    int rc = zmq_msg_copy(zmsg_dst, zmsg_src);
    if (rc != 0) return _zmqerr(errno, "Problem while copying a zmsg");

    return BXIERR_OK;
}

inline bxierr_p bxizmq_msg_has_more(void * const zsocket, bool * const result) {
    int64_t more = 0;
    size_t more_size = sizeof(more);
    int rc = zmq_getsockopt(zsocket, ZMQ_RCVMORE, &more, &more_size);
    if (-1 == rc) return _zmqerr(errno, "Can't call zmq_getsockopt()");

    *result = more;
    return BXIERR_OK;
}


bxierr_p bxizmq_msg_snd(zmq_msg_t * const zmsg,
                        void * const zocket, int flags,
                        const size_t retries_max, long delay_ns) {
    bxiassert(NULL != zmsg);
    bxiassert(NULL != zocket);

    size_t retries = 0;
    delay_ns /= (retries_max == 0) ? 1 : (long)retries_max;
    bxierr_p current = BXIERR_OK;
    while (true) {
        errno = 0;
        const int n = zmq_msg_send(zmsg, zocket, flags);
        if (n >= 0) {
            if (0 == retries) return BXIERR_OK;
            bxierr_p new = bxierr_new(BXIZMQ_RETRIES_MAX_ERR,
                                      (void *) retries,
                                      NULL,
                                      NULL,
                                      NULL,
                                      "Sending a message "
                                      "needed %zu retries", retries);
            BXIERR_CHAIN(current, new);
            return current;
        }
        bxiassert(n == -1);
        if (errno != EAGAIN && errno != EINTR) {
            bxierr_p new;
            if (errno == EFSM) {
                new = bxierr_new(BXIZMQ_FSM_ERR,
                                          NULL, NULL, NULL, NULL,
                                          "Invalid state for sending through zsocket %p" \
                                          " (error code is zeromq EFSM)", zocket);
            } else {
                new = _zmqerr(errno, "Can't send msg through zsocket %p", zocket);
            }
            BXIERR_CHAIN(current, new);
            return current;
        }
        retries++;
        if (retries >= retries_max) {
            // Now we will wait -> cancel the ZMQ_DONTWAIT bits
            flags = flags ^ ZMQ_DONTWAIT;
        }

        if (0 == delay_ns) continue;
        // We try to fetch a pseudo random sleeping time.
        // We cannot use bxirng here, since this function is used by log which itself
        // is used by rng, leading to an infinite recursive loop with some patterns.
        // Since random quality is not that essential here, we use a simple trick
        // to find a pseudo-random number.
        // See mantis#0018767: LOG during the seed generation
        // https://novahpc.frec.bull.fr/mantis-default/mantis/view.php?id=18767&cur_project_id=32
        struct timespec now;
        bxierr_p new = bxitime_get(CLOCK_MONOTONIC, &now);
        BXIERR_CHAIN(current, new);
        uint32_t sleep =  (uint32_t) (now.tv_nsec % delay_ns);
        new = bxitime_sleep(CLOCK_MONOTONIC, 0, (long)sleep);
        BXIERR_CHAIN(current, new);
//        delay_ns *= 2;
    }

    bxierr_unreachable_statement(__FILE__, __LINE__, __FUNCTION__);
}


/********************************* END Msg ****************************************/
/*********************************  DATA   ****************************************/


bxierr_p bxizmq_data_snd(const void * const data, const size_t size,
                         void * const zocket, const int flags,
                         const size_t retries_max, long delay_ns) {
    bxiassert(NULL != data);
    bxiassert(NULL != zocket);
    zmq_msg_t msg;
    errno = 0;
    int rc = zmq_msg_init_size(&msg, size);
    if (rc != 0) return _zmqerr(errno, "Calling zmq_msg_init_size() failed");

    memcpy(zmq_msg_data(&msg), data, size);
    bxierr_p current = bxizmq_msg_snd(&msg, zocket, flags, retries_max, delay_ns);
    bxierr_p new = bxizmq_msg_close(&msg);
    BXIERR_CHAIN(current, new);
    return current;
}


bxierr_p bxizmq_data_snd_zc(const void * const data, const size_t size,
                            void * const zocket, int flags,
                            const size_t retries_max, long delay_ns,
                            zmq_free_fn * const ffn, void * const hint) {
    bxiassert(NULL != data);
    bxiassert(NULL != zocket);
    zmq_msg_t msg;
    errno = 0;
    int rc = zmq_msg_init_data(&msg, (void *)data, size, ffn, hint);
    if (0 != rc) return _zmqerr(errno, "Calling zmq_msg_init_data() failed");

    bxierr_p current = bxizmq_msg_snd(&msg, zocket, flags, retries_max, delay_ns);
    bxierr_p new = bxizmq_msg_close(&msg);
    BXIERR_CHAIN(current, new);

    return current;

}

bxierr_p bxizmq_data_rcv(void ** result, const size_t expected_size,
                         void * const zocket, const int flags,
                         const bool check_more, size_t * received_size) {
    bxierr_p current = BXIERR_OK, new;
    if (check_more) {
        bool more = false;
        new = bxizmq_msg_has_more(zocket, &more);
        if (BXIERR_OK != new) return new;
        if (!more) return bxierr_new(BXIZMQ_MISSING_FRAME_ERR,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     "Missing zeromq frame on socket %p", zocket);
    }

    zmq_msg_t mzg;
    errno = 0;
    int rc = zmq_msg_init(&mzg);
    if (-1 == rc) return _zmqerr(errno, "Calling zmq_msg_init() failed");

    new = bxizmq_msg_rcv(zocket, &mzg, flags);
    if (bxierr_isko(new)) {
        if (EAGAIN == new->code) {
            bxiassert(ZMQ_DONTWAIT == (flags & ZMQ_DONTWAIT));
            bxierr_destroy(&new);
            *result = NULL;
            *received_size = 0;
            return current;
        }
        BXIERR_CHAIN(current, new);
        return current;
    }
    size_t rcv_size = zmq_msg_size(&mzg);
    if (NULL != *result && rcv_size > expected_size) {
        new = bxizmq_msg_close(&mzg);
        BXIERR_CHAIN(current, new);
        new = bxierr_gen("Received more bytes than expected on zsocket %p:"
                         " expected %zu, got: %zu",
                           zocket, expected_size, rcv_size);
        BXIERR_CHAIN(current, new);
        return current;
    }
    // Special case for signaling
    if (0 == rcv_size && 0 == expected_size) goto END;

    if (NULL == *result) {
        // Malloc either the received size if expected size was not specified or
        // the expected size otherwise.
        *result = bximem_calloc(expected_size == 0 ? rcv_size : expected_size);
    }
    memcpy(*result, zmq_msg_data(&mzg), rcv_size);

END:
    new = bxizmq_msg_close(&mzg);
    BXIERR_CHAIN(current, new);
    if (received_size != NULL) *received_size = rcv_size;
    return current;
}
/********************************* END DATA ****************************************/
/*********************************   STR    ****************************************/

inline bxierr_p bxizmq_str_snd(const char * const str, void * zsocket, int flags,
                               size_t retries_max, long delay_ns) {
    // Since we use strlen() it means the last '\0' is not sent!
    // Therefore the corresponding rcv() function cannot rely on the
    // received size to allocate the memory, it should allocate one byte more.
    // See bxizmq_rcv_str() for details.
    return bxizmq_data_snd(str, strlen(str), zsocket, flags, retries_max, delay_ns);
}


inline bxierr_p bxizmq_str_snd_zc(const char * const str, void * zsocket, int flags,
                                  size_t retries_max, long delay_ns, bool freestr) {
    // Since we use strlen() it means the last '\0' is not sent!
    // Therefore the corresponding rcv() function cannot rely on the
    // received size to allocate the memory, it should allocate one byte more.
    // See bxizmq_rcv_str() for details.
    return bxizmq_data_snd_zc(str, strlen(str), zsocket, flags,
                              retries_max, delay_ns,
                              freestr ? bxizmq_data_free : NULL, NULL);
}


bxierr_p bxizmq_str_rcv(void * const zocket, const int flags, const bool check_more,
                        char ** result) {
    // This function cannot use the bximisc_rcv_msg() since the amount of data sent
    // for a string is strlen(): the last '\0' is not sent therefore!
    // When we receive such a message the expected size should therefore be the
    // strlen(), but the allocated memory should strlen() + 1!!
    bxierr_p current = BXIERR_OK, new;
    if (check_more) {
        bool more = false;
        new = bxizmq_msg_has_more(zocket, &more);
        BXIERR_CHAIN(current, new);
        if (bxierr_isko(current)) return current;
        if (!more) return bxierr_new(BXIZMQ_MISSING_FRAME_ERR,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     "Missing zeromq frame on socket %p", zocket);
    }
    zmq_msg_t mzg;
    int rc = zmq_msg_init(&mzg);
    if (rc != 0) return _zmqerr(errno, "Can't initialize msg");

    new = bxizmq_msg_rcv(zocket, &mzg, flags);
    if (bxierr_isko(new)) {
        if (EAGAIN == new->code) {
            bxiassert(ZMQ_DONTWAIT == (flags & ZMQ_DONTWAIT));
            bxierr_destroy(&new);
            *result = NULL;
            return current;
        }
        BXIERR_CHAIN(current, new);
        return current;
    }
    size_t len = zmq_msg_size(&mzg);
    // calloc, fills with 0, no need to add the NULL terminating byte
    *result = bximem_calloc(len + 1);
    memcpy(*result, zmq_msg_data(&mzg), len);
    new = bxizmq_msg_close(&mzg);
    BXIERR_CHAIN(current, new);

    return current;
}
/********************************* END STR  ****************************************/

// Used by bxizmq_snd_str_zc() for freeing a simple mallocated string
void bxizmq_data_free(void * const data, void * const hint) {
    UNUSED(hint);
    BXIFREE(data);
}

bxierr_p bxizmq_sync_pub(void * pub_zocket,
                         void * sync_zocket,
                         char * const sync_url,
                         const size_t sync_url_len,
                         const double timeout_s) {
    bxiassert(NULL != pub_zocket);
    bxiassert(NULL != sync_zocket);
    bxiassert(NULL != sync_url);
    bxiassert(0 < sync_url_len);
    bxiassert(0 <= timeout_s);
    bxierr_p err = BXIERR_OK, err2;

    struct timespec start, last_send = {0,0};
    err2 = bxitime_get(CLOCK_MONOTONIC, &start);
    BXIERR_CHAIN(err, err2);

    double delay = 0.0;
    double send_delay = timeout_s;
    double nb_msg = 1000;
    do {

        err2 = bxitime_duration(CLOCK_MONOTONIC, last_send, &send_delay);
        BXIERR_CHAIN(err, err2);
        if (send_delay > (timeout_s / nb_msg)) {
            // First frame: the SYNC header
            err2 = bxitime_get(CLOCK_MONOTONIC, &last_send);
            BXIERR_CHAIN(err, err2);
            err2 = bxizmq_str_snd(BXIZMQ_PUBSUB_SYNC_HEADER,
                                  pub_zocket, ZMQ_SNDMORE, 0, 0);
            BXIERR_CHAIN(err, err2);

            // Second frame: the sync socket URL
            err2 = bxizmq_str_snd(sync_url, pub_zocket, 0, 0, 0);
            BXIERR_CHAIN(err, err2);
        }

        char * synced = NULL;
        err2 = bxizmq_str_rcv(sync_zocket, ZMQ_DONTWAIT, false, &synced);
        BXIERR_CHAIN(err, err2);

        // Ok we received the synced message
        if (NULL != synced) {
            err2 = bxizmq_str_snd(synced, sync_zocket, 0, 0, 0);
            BXIERR_CHAIN(err, err2);
            // Check it is the expected one
            if (strncmp(sync_url, synced, sync_url_len) == 0) {
                BXIFREE(synced);
                return err;
            }
            return bxierr_new(BXIZMQ_PROTOCOL_ERR, NULL, NULL, NULL, err,
                              "Expected PUB/SUB synced message: '%s', received: '%s'",
                              sync_url, synced);
        }

        // Check the timeout did not expire
        err2 = bxitime_duration(CLOCK_MONOTONIC, start, &delay);
        BXIERR_CHAIN(err, err2);
    } while (delay < timeout_s);

    return bxierr_new(BXIZMQ_TIMEOUT_ERR, NULL, NULL, NULL,
                      err,
                      "Timeout %f reached (%f) "
                      "while syncing %s",
                      timeout_s, delay, sync_url);
}

bxierr_p bxizmq_sync_sub(void * zmq_ctx,
                         void * sub_zocket,
                         const double timeout_s) {
    bxiassert(NULL != sub_zocket);
    bxiassert(0 <= timeout_s);
    bxierr_p err = BXIERR_OK, err2;

    struct timespec now;
    err2 = bxitime_get(CLOCK_MONOTONIC, &now);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_zocket_setopt(sub_zocket, ZMQ_SUBSCRIBE,
                                BXIZMQ_PUBSUB_SYNC_HEADER,
                                ARRAYLEN(BXIZMQ_PUBSUB_SYNC_HEADER) - 1);
    BXIERR_CHAIN(err, err2);

    char * sync_url = NULL;
    char * key = NULL;
    while (true) {

        err2 = bxizmq_str_rcv(sub_zocket, ZMQ_DONTWAIT, false, &key);
        BXIERR_CHAIN(err, err2);

        if (NULL != key && strcmp(key, BXIZMQ_PUBSUB_SYNC_HEADER) == 0) {

            err2 = bxizmq_str_rcv(sub_zocket, ZMQ_DONTWAIT, false, &sync_url);
            BXIERR_CHAIN(err, err2);

            BXIFREE(key);
            // We received something
            if (NULL != sync_url) {
                break;
            }
        }

        // Check the timeout did not expire
        double delay;
        err2 = bxitime_duration(CLOCK_MONOTONIC, now, &delay);
        BXIERR_CHAIN(err, err2);
        if (delay > timeout_s) return bxierr_new(BXIZMQ_TIMEOUT_ERR, NULL, NULL, NULL,
                                                 err,
                                                 "Timeout %f reached (%f) "
                                                 "while syncing",
                                                 timeout_s, delay);
    }

    void * sync_zocket = NULL;
    err2 = bxizmq_zocket_create_connected(zmq_ctx, ZMQ_REQ, sync_url, &sync_zocket);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_str_snd(sync_url, sync_zocket, ZMQ_DONTWAIT, 0, 0);
    BXIERR_CHAIN(err, err2);
    BXIFREE(sync_url);

    err2 = bxizmq_zocket_setopt(sub_zocket, ZMQ_UNSUBSCRIBE,
                                BXIZMQ_PUBSUB_SYNC_HEADER,
                                ARRAYLEN(BXIZMQ_PUBSUB_SYNC_HEADER) - 1);

    zmq_pollitem_t poll_set[] = {
        { sync_zocket, 0, ZMQ_POLLIN, 0 },};

    while (true) {
        int rc = zmq_poll(poll_set, 1, (long)timeout_s/10);
        if (-1 == rc) {
            err2 = _zmqerr(errno, "Calling zmq_poll() failed");
            BXIERR_CHAIN(err, err2);
            break;
        }

        if (!(poll_set[0].revents & ZMQ_POLLIN)) break;
        key = NULL;
        err2 = bxizmq_str_rcv(sync_zocket, 0, false, &key);
        BXIERR_CHAIN(err, err2);
        break;
    }

    while (key != NULL) {
        BXIFREE(key);
        key = NULL;
        err2 = bxizmq_str_rcv(sub_zocket, ZMQ_DONTWAIT, false, &key);
        BXIERR_CHAIN(err, err2);
    }

    err2 = bxizmq_zocket_destroy(&sync_zocket);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p bxizmq_sync_pub_many(void * const zmq_ctx,
                              void * const pub_zocket,
                              const char * const pub_url,
                              size_t sub_nb,
                              const double timeout_s) {
    bxiassert(NULL != zmq_ctx);
    bxiassert(NULL != pub_zocket);
    bxiassert(0 <= timeout_s);
    bxiassert(NULL != pub_url);

    bxierr_p err = BXIERR_OK, err2;

    struct timespec start, last_send_date = {0, 0};
    err2 = bxitime_get(CLOCK_MONOTONIC, &start);
    BXIERR_CHAIN(err, err2);

    void * sync_zocket = NULL;
    char * sync_url = NULL;
    int tcp_port;
    err2 = _create_pub_sync_socket(zmq_ctx, &sync_zocket, pub_url, &sync_url, &tcp_port);
    BXIERR_CHAIN(err, err2);

    if (bxierr_isko(err)) return err;

    const char * const url = bxizmq_create_url_from(sync_url, tcp_port);
    BXIFREE(sync_url);
    // We generate a key of the format: .bxizmq/sync|url
    // This key will be unique (hopefully) among multiple publishers
    // so subscribers can drops message only of those
    // publishers for which they have already subscribed.
    const char * const key = bxistr_new("%s|%s", BXIZMQ_PUBSUB_SYNC_PING, url);

    double time_spent = 0.0;
    const long nb_msg = 1000; // We will send at most that amount of messages
    long poll_timeout = ((long)(timeout_s * 1000)) / nb_msg;

    zmq_pollitem_t poll_set[] = {
                                 { pub_zocket, 0, ZMQ_POLLOUT, 0 },
                                 { sync_zocket, 0, ZMQ_POLLIN, 0} ,
    };
    int nitems = 2;
    size_t missing_almost = sub_nb;
    size_t missing_go = sub_nb;

    while (0 < missing_go) {
        errno = 0;
        int rc = zmq_poll(poll_set, nitems, poll_timeout);
        if (-1 == rc) {
            err2 = _zmqerr(errno, "Calling zmq_poll() failed");
            BXIERR_CHAIN(err, err2);
            break;
        }

        if (time_spent >= timeout_s) {
            err2 = bxierr_new(BXIZMQ_TIMEOUT_ERR, NULL, NULL, NULL,
                              err,
                              "Timeout %f reached (%f) "
                              "while syncing %s",
                              timeout_s, time_spent, url);
            BXIERR_CHAIN(err, err2);
            break; // Timeout reached
        }

        if (0 == rc) {
            // We can neither send on pub_zocket neither receive on sync_zocket
            // Let's try again...
            continue;
        }

        if (10 < bxierr_get_depth(err)) break;

        bool something_changed = false;

        if (poll_set[0].revents & ZMQ_POLLOUT) { // We can send on the pub_zocket
            // We send until all subscribers have seen the last message
            if (0 != missing_almost) {
                struct timespec tmp = last_send_date;
                err2 = _process_pub_snd(pub_zocket, key, url,
                                        &last_send_date, ((double) poll_timeout) / 1000);
                BXIERR_CHAIN(err, err2);
                if (tmp.tv_nsec != last_send_date.tv_nsec) something_changed = true;
            }
        }

        if (poll_set[1].revents & ZMQ_POLLIN) { // We received something on the sync_zocket

            err2 = _process_pub_sync_msg(pub_zocket,
                                         sync_zocket,
                                         &missing_almost,
                                         &missing_go,
                                         url);
            BXIERR_CHAIN(err, err2);
            something_changed = true;
        }

        if (something_changed) {
            DBG("PUB[%s]: missing almost msg: %zu, missing go msg: %zu\n",
                url, missing_almost, missing_go);
        }

        // Check the timeout did not expire
        err2 = bxitime_duration(CLOCK_MONOTONIC, start, &time_spent);
        BXIERR_CHAIN(err, err2);
    }

    BXIFREE(url);
    BXIFREE(key);
    if (NULL != sync_zocket) {
        // Don't care here
        DBG("PUB[%s]: destroying socket %p\n", url, sync_zocket);
        bxierr_p tmp = bxizmq_zocket_destroy(&sync_zocket);
        bxierr_report(&tmp, STDERR_FILENO);
    }

    return err;

}

bxierr_p bxizmq_sync_sub_many(void * const zmq_ctx,
                              void * const sub_zocket,
                              const size_t pub_nb,
                              const double timeout_s) {

    bxiassert(NULL != sub_zocket);
    bxiassert(0 <= timeout_s);
    bxierr_p err = BXIERR_OK, err2;

    // Subscribes to SYNC messages
    err2 = bxizmq_zocket_setopt(sub_zocket, ZMQ_SUBSCRIBE,
                                BXIZMQ_PUBSUB_SYNC_HEADER,
                                ARRAYLEN(BXIZMQ_PUBSUB_SYNC_HEADER) - 1);
    BXIERR_CHAIN(err, err2);

    // Create the DEALER socket
    void * sync_zocket = NULL;
    err2 = bxizmq_zocket_create(zmq_ctx, ZMQ_DEALER, &sync_zocket);
    BXIERR_CHAIN(err, err2);
    if (NULL == sync_zocket) return err;

    struct timespec now;
    err2 = bxitime_get(CLOCK_MONOTONIC, &now);
    BXIERR_CHAIN(err, err2);
    long remaining_time = (long) (timeout_s * 1000);

    zmq_pollitem_t poll_set[] = {
        { sub_zocket, 0, ZMQ_POLLIN, 0 },
        { sync_zocket, 0, ZMQ_POLLIN, 0} ,
    };

    void * already_pinged_root = NULL;
    size_t missing_ready_msg_nb = pub_nb;
    size_t missing_last_msg_nb = pub_nb;

    while(0 < missing_last_msg_nb) {
        errno = 0;
        int rc = zmq_poll(poll_set, 2, remaining_time);
        if (-1 == rc) {
            err2 = _zmqerr(errno, "Calling zmq_poll() failed");
            BXIERR_CHAIN(err, err2);
            break;
        }

        if (0 == rc) {
            err2 = bxierr_new(BXIZMQ_TIMEOUT_ERR, NULL, NULL, NULL,
                              err,
                              "Timeout %f reached "
                              "while syncing",
                              timeout_s);
            BXIERR_CHAIN(err, err2);
            break;
        }

        if (10 < bxierr_get_depth(err)) break;

        DBG("SUB: missing ready msg: %zu, missing last msg: %zu\n",
            missing_ready_msg_nb, missing_last_msg_nb);

        if (poll_set[0].revents & ZMQ_POLLIN) {
            // We received something from the SUB socket
            char * header;
            // First frame: the header
            err2 = bxizmq_str_rcv(sub_zocket, ZMQ_DONTWAIT, false, &header);
            BXIERR_CHAIN(err, err2);

            if (0 == strncmp(BXIZMQ_PUBSUB_SYNC_PING, header,
                             ARRAYLEN(BXIZMQ_PUBSUB_SYNC_PING) - 1)) {
                err2 = _process_sub_ping_msg(sub_zocket, sync_zocket,
                                             header, &already_pinged_root);
                BXIERR_CHAIN(err, err2);
                // Do not free header here, it is inserted in the binary tree already_pinged_root
                // BXIFREE(header);
            } else if (0 == strncmp(BXIZMQ_PUBSUB_SYNC_LAST, header,
                             ARRAYLEN(BXIZMQ_PUBSUB_SYNC_LAST) - 1)) {
                // We received a 'last' message
                err2 = _process_sub_last_msg(sync_zocket, &missing_last_msg_nb, pub_nb);
                BXIERR_CHAIN(err, err2);
                BXIFREE(header);
            } else {
                err2 = bxierr_simple(BXIZMQ_PROTOCOL_ERR,
                                     "Wrong pub/sub sync header message received: '%s'",
                                     header);
                BXIERR_CHAIN(err, err2);
                BXIFREE(header);
                break;
            }
            // Do not free header here, it is inserted in the binary tree already_pinged_root
            // BXIFREE(header);
        }

        if (poll_set[1].revents & ZMQ_POLLIN) {
            // We received something from the DEALER socket
            char * msg;
            err2 = bxizmq_str_rcv(sync_zocket, 0, false, &msg);
            BXIERR_CHAIN(err, err2);
            DBG("DEALER: rcv '%s'\n", msg);

            if (0 == strncmp(BXIZMQ_PUBSUB_SYNC_READY, msg,
                             ARRAYLEN(BXIZMQ_PUBSUB_SYNC_READY) - 1)) {
                err2 = _process_sub_ready_msg(sync_zocket,
                                              &missing_ready_msg_nb, pub_nb);
                BXIERR_CHAIN(err, err2);
            } else {
                err2 = bxierr_simple(BXIZMQ_PROTOCOL_ERR,
                                     "Wrong header message received: '%s'", msg);
                BXIERR_CHAIN(err, err2);
                BXIFREE(msg);
                break;
            }
            BXIFREE(msg);
        }
    }

    bxierr_p tmp = bxizmq_zocket_destroy(&sync_zocket);
    // We don't care here!
    bxierr_report(&tmp, STDERR_FILENO);

    // Destroy the binary tree
#ifdef _GNU_SOURCE
    tdestroy(already_pinged_root, NULL);
#else
    while (NULL != already_pinged_root) {
        char * header = *(char **) already_pinged_root;
        tdelete(header, &already_pinged_root, (__compar_fn_t) strcmp);
        BXIFREE(header);
    }
#endif

    // Unsubscribe from SYNC messages
    err2 = bxizmq_zocket_setopt(sub_zocket, ZMQ_UNSUBSCRIBE,
                                BXIZMQ_PUBSUB_SYNC_HEADER,
                                ARRAYLEN(BXIZMQ_PUBSUB_SYNC_HEADER) - 1);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p bxizmq_generate_new_url_from(const char * const url, char ** result) {
    bxiassert(NULL != url);
    bxiassert(NULL != result);

    *result = NULL;

    if ((0 == strncmp("inproc", url, ARRAYLEN("inproc") - 1)) ||
        (0 == strncmp("ipc", url, ARRAYLEN("ipc") - 1))) {

        pthread_t thread = pthread_self();
        struct timespec time;

        bxierr_p tmp = bxitime_get(CLOCK_MONOTONIC, &time);
        if (bxierr_isko(tmp)) {
            bxierr_report(&tmp, STDERR_FILENO);
            *result = bxistr_new("%s-%x", url, (unsigned int) thread);
        } else {
            *result = bxistr_new("%s-%x.%x", url, (unsigned int) thread,
                                 (unsigned int) (time.tv_sec * 1000000000 + time.tv_nsec));
        }
        return BXIERR_OK;
    }
    if (0 == strncmp("tcp", url, ARRAYLEN("tcp") - 1)) {
        char * colon = strrchr(url + ARRAYLEN("tcp://") - 1, ':');
        size_t len = (size_t) (colon - url);
        *result = bximem_calloc((len + ARRAYLEN(":*")) * sizeof(**result));
        memcpy(*result, url, len);
        memcpy(*result + len, ":*", strlen(":*"));
        return BXIERR_OK;
    }

    return bxierr_gen("Bad or non-supported zeromq URL: %s", url);
}


char * bxizmq_create_url_from(const char * const url, const int tcp_port) {
    if (0 != strncmp("tcp://", url, ARRAYLEN("tcp://") - 1)) return strdup(url);

    const char * port;
    bxistr_rsub(url, strlen(url), ':', &port);
    if (port[0] == '*') {
        return bxistr_new("%.*s%d", (int) (port - url), url, tcp_port);
    } else return strdup(url);
}


// *********************************************************************************
// **************************** Static function ************************************
// *********************************************************************************

bxierr_p _zmqerr(int errnum, const char * fmt, ...) {
    bxiassert(NULL != fmt);

    const char * errmsg = zmq_strerror(errnum);

    va_list ap;
    va_start(ap, fmt);

    char * msg;
    bxistr_vnew(&msg, fmt, ap);

    bxierr_p result = bxierr_new(errnum, NULL, NULL, NULL, NULL, "%s: %s", msg, errmsg);
    BXIFREE(msg);

    va_end(ap);

    return result;
}


bxierr_p bxizmq_split_url(const char * const url, char * elements[3]) {
    if (url == NULL) {
        return bxierr_gen("Url should be defined");
    }
    elements[0] = strdup(url);
    char * hostname_pos = strchr(elements[0], ':');

    if (hostname_pos == NULL) {
        elements[0] = NULL;
        return bxierr_gen("Url doesn't contain seperator ':'");
    }

    if (hostname_pos[1] == '/' && hostname_pos[2] == '/') {
        hostname_pos[0] = '\0';
        hostname_pos[1] = '\0';
        hostname_pos[2] = '\0';
        elements[1] = &hostname_pos[3];
        char * port = strchr(elements[1], ':');

        if (port == NULL) {
            bxierr_p err = bxierr_gen("Url doesn't contain port number after"
                                      " hostname %s ':.*'", elements[1]);
            elements[0] = NULL;
            elements[1] = NULL;
            return err;
        }

        port[0] = '\0';
        elements[2] = &port[1];
        return BXIERR_OK;
    } else {
        elements[0] = NULL;
        return bxierr_gen("Url doesn't contain '://' after protocol name");
    }
}

bxierr_p _create_pub_sync_socket(void * zmq_ctx, void ** sync_zocket,
                                 const char * const pub_url,
                                 char ** const sync_url,
                                 int * const tcp_port) {
    bxiassert(NULL != zmq_ctx);
    bxiassert(NULL != sync_zocket);
    bxiassert(NULL != pub_url);
    bxiassert(NULL != sync_url);
    bxiassert(NULL != tcp_port);

    bxierr_p err = BXIERR_OK, err2;
    // Create the ROUTER zocket
    err2 = bxizmq_zocket_create(zmq_ctx, ZMQ_ROUTER, sync_zocket);
    BXIERR_CHAIN(err, err2);

    bxierr_abort_ifko(err);

    // Generate a new URL from the given pub_url
    err2 = bxizmq_generate_new_url_from(pub_url, sync_url);
    BXIERR_CHAIN(err, err2);

    // Bind this socket to the URL
    err2 = bxizmq_zocket_bind(*sync_zocket, *sync_url, tcp_port);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _process_pub_snd(void * pub_zocket, const char * key, const char * url,
                          struct timespec * last_send_date, double max_snd_delay) {

    bxierr_p err = BXIERR_OK, err2;

    double last_send_duration;
    err2 = bxitime_duration(CLOCK_MONOTONIC, *last_send_date, &last_send_duration);
    BXIERR_CHAIN(err, err2);

    if (last_send_duration < max_snd_delay) {
//        DBG("PUB[%s]: last send was %lf s (< %lf s) ago, skipping\n",
//            url, last_send_duration, max_snd_delay);
        return BXIERR_OK;
    }

    // First frame: the SYNC header
    err2 = bxizmq_str_snd(key, pub_zocket, ZMQ_SNDMORE, 0, 0);
    BXIERR_CHAIN(err, err2);
    DBG("PUB[%s]: snd '%s'\n", url, key);

    // Second frame: the ROUTER socket URL
    err2 = bxizmq_str_snd(url, pub_zocket, 0, 0, 0);
    BXIERR_CHAIN(err, err2);
    DBG("PUB[%s]: snd '%s'\n", url, url);


    err2 = bxitime_get(CLOCK_MONOTONIC, last_send_date);
    BXIERR_CHAIN(err, err2);

    return err;
}

bxierr_p _process_pub_sync_msg(void * const pub_zocket,
                               void * const sync_zocket,
                               size_t * const missing_almost,
                               size_t * const missing_go,
                               const char * const url) {

    bxierr_p err = BXIERR_OK, err2;

    // Since we use a ROUTER socket, it comes with its own protocol
    // First frame is the ID
    zmq_msg_t id;

    err2 = bxizmq_msg_init(&id);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_msg_rcv(sync_zocket, &id, 0);
    BXIERR_CHAIN(err, err2);

    char * msg = NULL;
    // Then the message itself
    err2 = bxizmq_str_rcv(sync_zocket, 0, true, &msg);
    BXIERR_CHAIN(err, err2);

    DBG("ROUTER[%s]: rcv '%s'\n", url, msg);
    // Check what we received
    if (0 == strncmp(BXIZMQ_PUBSUB_SYNC_PONG,
                     msg,
                     ARRAYLEN(BXIZMQ_PUBSUB_SYNC_PONG) - 1)) {

        // Pong message received, reply with READY? message
        // First frame is the id
        err2 = bxizmq_msg_snd(&id, sync_zocket, ZMQ_SNDMORE, 0, 0);
        BXIERR_CHAIN(err, err2);
        DBG("ROUTER[%s]: snd id\n", url);
        // Then the message
        err2 = bxizmq_str_snd(BXIZMQ_PUBSUB_SYNC_READY, sync_zocket, 0, 0, 0);
        BXIERR_CHAIN(err, err2);
        DBG("ROUTER[%s]: snd '%s'\n", url, BXIZMQ_PUBSUB_SYNC_READY);
    } else if (0 == strncmp(BXIZMQ_PUBSUB_SYNC_ALMOST,
                            msg,
                            ARRAYLEN(BXIZMQ_PUBSUB_SYNC_ALMOST) - 1)) {

        // ALMOST! message received, we expect *sub_almost_ready_nb such messages
        (*missing_almost)--;
        if (0 == *missing_almost) {
            // All subscribers send their 'almost' message -> send the last message
            const char * const last = bxistr_new("%s|%s", BXIZMQ_PUBSUB_SYNC_LAST, url);
            err2 = bxizmq_str_snd(last, pub_zocket, 0, 0, 0);
            BXIERR_CHAIN(err, err2);
            DBG("PUB[%s]: snd '%s'\n", url, last);
            BXIFREE(last);
        }

    } else if (0 == strncmp(BXIZMQ_PUBSUB_SYNC_GO,
                            msg,
                            ARRAYLEN(BXIZMQ_PUBSUB_SYNC_GO) - 1)) {

        // GO! message received, we expect sub_ready_nb such messages
        (*missing_go)--;
    } else {
        err2 = bxierr_new(BXIZMQ_PROTOCOL_ERR, NULL, NULL, NULL, err,
                          "Unexpected PUB/SUB synced message: '%s' from '%s'",
                          msg, url);
        BXIERR_CHAIN(err, err2);
    }

    BXIFREE(msg);
    err2 = bxizmq_msg_close(&id);
    BXIERR_CHAIN(err, err2);
    return err;
}

bxierr_p _process_sub_ping_msg(void* sub_zocket, void * sync_zocket,
                               char * header, void ** already_pinged_root) {
    bxierr_p err = BXIERR_OK, err2;

    // Search if we have already pinged this publisher
    char ** found = tsearch(header,
                            already_pinged_root,
                            (__compar_fn_t) strcmp);
    bxiassert(NULL != found);

    if (*found != header) { // Already seen
        // Drop next frame
        char * tmp;
        err2 = bxizmq_str_rcv(sub_zocket, 0, false, &tmp);
        BXIERR_CHAIN(err, err2);
        DBG("SUB: dropping '%s'\n", tmp);
        BXIFREE(tmp);
        BXIFREE(header);
    } else {
        // First time seen, it has been automatically added to the tree
        // by tsearch() -> launch the sync process
        err2  = _sync_sub_send_pong(sub_zocket, sync_zocket);
        BXIERR_CHAIN(err, err2);
        // Do not free header, it is in the tree!
        // BXIFREE(header);
    }

    return err;
}


bxierr_p _sync_sub_send_pong(void * sub_zocket, void * sync_zocket) {
    bxierr_p err = BXIERR_OK, err2;

    // Second frame: the sync url
    char * sync_url;
    err2 = bxizmq_str_rcv(sub_zocket, ZMQ_DONTWAIT, true, &sync_url);
    BXIERR_CHAIN(err, err2);

    DBG("SUB: rcv '%s'\n", sync_url);

    // Connecting
    err2 = bxizmq_zocket_connect(sync_zocket, sync_url);
    BXIERR_CHAIN(err, err2);

    // Send pong message
    err2 = bxizmq_str_snd(BXIZMQ_PUBSUB_SYNC_PONG, sync_zocket, 0, 0, 0);
    BXIERR_CHAIN(err, err2);
    DBG("DEALER: snd '%s'\n", BXIZMQ_PUBSUB_SYNC_PONG);

    BXIFREE(sync_url);

    return err;
}

bxierr_p _process_sub_last_msg(void * sync_zocket,
                               size_t * missing_last_msg_nb,
                               size_t pub_nb) {

    bxierr_p err = BXIERR_OK, err2;

    (*missing_last_msg_nb)--;

    if (0 == *missing_last_msg_nb) {
        // Send GO message to all publishers
        for (size_t i = 0; i < pub_nb; i++) {
            err2 = bxizmq_str_snd(BXIZMQ_PUBSUB_SYNC_GO, sync_zocket, 0, 0, 0);
            BXIERR_CHAIN(err, err2);
            DBG("DEALER: snd %s\n", BXIZMQ_PUBSUB_SYNC_GO);
        }
    }

    return err;
}

bxierr_p _process_sub_ready_msg(void * const sync_zocket,
                                size_t * const missing_ready_msg_nb,
                                size_t pub_nb) {
    bxierr_p err = BXIERR_OK, err2;

    (*missing_ready_msg_nb)--;

    if (0 == *missing_ready_msg_nb) { // All publisher synced,
        // Sending the ALMOST message to all publishers
        for (size_t i = 0; i < pub_nb; i++) {
            err2 = bxizmq_str_snd(BXIZMQ_PUBSUB_SYNC_ALMOST, sync_zocket,
                                  0, 0, 0);
            BXIERR_CHAIN(err, err2);
            DBG("DEALER: snd %s\n", BXIZMQ_PUBSUB_SYNC_ALMOST);
        }
    }
    return err;
}
