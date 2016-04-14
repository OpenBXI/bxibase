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


#include <zmq.h>

#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/err.h"
#include "bxi/base/time.h"
#include "bxi/base/zmq.h"


// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

#define DEFAULT_CLOSING_LINGER 1000u
#define MAX_CONNECTION_TIMEOUT 1.0     // seconds
#define INPROC_PROTO "inproc"
#define TCP_PROTO "tcp"


// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************

bxierr_p _zocket_create(void * const ctx, const int type, void ** result);
bxierr_p _split_url(const char * const url, char * elements[3]);
bxierr_p _zmqerr(int errnum, const char * fmt, ...);

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************

/*************************************** Zocket ***********************************/
bxierr_p bxizmq_zocket_bind(void * const ctx,
                            const int type,
                            const char * const url,
                            int  * affected_port,
                            void ** result) {
    bxiassert(NULL != ctx);
    bxiassert(NULL != url);
    bxiassert(NULL != result);
    bxierr_p err = BXIERR_OK, err2;

    unsigned long port = 0;

    void * socket = *result;
    if (NULL == socket) {
        err2 = _zocket_create(ctx, type, &socket);
        BXIERR_CHAIN(err, err2);
        if (bxierr_isko(err)) return err;
    }


    char * translate_url = (char *)url;

    int rc = zmq_bind(socket, translate_url);
    if (rc == -1) {
        err2 = _zmqerr(errno, "Can't bind zmq socket on %s", translate_url);
        BXIERR_CHAIN(err, err2);

        //Try by translating hostname into IP
        if (strncmp(translate_url, TCP_PROTO, ARRAYLEN(TCP_PROTO) - 1) != 0) {
            err2 = bxizmq_zocket_destroy(socket);
            BXIERR_CHAIN(err, err2);
            return err;
        }

        char * elements[3];
        //Get the protocol the hostname and the port
        err2 = _split_url(url, elements);
        BXIERR_CHAIN(err, err2);

        if (bxierr_isko(err2)) {
            err2 = bxizmq_zocket_destroy(socket);
            BXIERR_CHAIN(err, err2);
            return err;
        }

        errno = 0;
        struct addrinfo hints, *servinfo;
        int rv;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // use either IPV4 or IPV6
        hints.ai_socktype = SOCK_STREAM;

        errno = 0;
        if ( (rv = getaddrinfo(elements[1] , NULL , &hints , &servinfo)) != 0) 
        {
            err2 = bxierr_gen("Translation address error: getaddrinfo: %s", gai_strerror(rv));
            BXIERR_CHAIN(err, err2);
        }

        if (bxierr_isko(err2)) {
            err2 = bxizmq_zocket_destroy(socket);
            BXIERR_CHAIN(err, err2);
            return err;
        }

        errno = 0;
        char * ip = NULL;
        struct addrinfo *p;
        struct sockaddr_in *h;
        for(p = servinfo; p != NULL && ip == NULL; p = p->ai_next) {
            h = (struct sockaddr_in *) p->ai_addr;
            if (h == NULL) {
                continue;
            }
            ip = bxistr_new("%s", inet_ntoa( h->sin_addr ) );
        }
        if (ip != NULL) {
            translate_url = bxistr_new("%s://%s:%s", elements[0], ip, elements[2]);
            BXIFREE(ip);
            int rc = zmq_bind(socket, translate_url);
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

    if (bxierr_isko(err)) {
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    char endpoint[512];
    endpoint[0] = '\0';

    if (NULL != affected_port && strncmp(translate_url,
                                         INPROC_PROTO,
                                         ARRAYLEN(INPROC_PROTO) - 1) != 0) {
        bxiassert(NULL != socket);
        size_t endpoint_len = 512;
        errno = 0 ;

        rc = zmq_getsockopt(socket, ZMQ_LAST_ENDPOINT, &endpoint, &endpoint_len);
        if (rc == -1) {
            if (errno == EINVAL || errno == ETERM || errno == ENOTSOCK || errno == EINTR) {
                err2 = _zmqerr(errno, "Can't retrieve the zocket endpoint option");
                BXIERR_CHAIN(err, err2);
            }
        }
    }

    if (strlen(endpoint) > 0) {
        char * ptr = strrchr(endpoint, ':');
        if (NULL == ptr) {
            err2 = bxierr_errno("Unable to retrieve the binded port number");
            BXIERR_CHAIN(err, err2);
            return err;
        }
        char * endptr = NULL;
        errno = 0;
        port = strtoul(ptr+1, &endptr, 10);
        if (errno == ERANGE) {
            err2 = bxizmq_zocket_destroy(socket);
            BXIERR_CHAIN(err, err2);
            err2 = bxierr_errno("Unable to parse integer in: '%s'", ptr+1);
            BXIERR_CHAIN(err, err2);
            return err;
        }
    }

    if (0 != port) {
        *affected_port = (int) port;
    }
    //if (NULL != affected_port)  *affected_port = rc;
    *result = socket;
    return err;
}

bxierr_p bxizmq_zocket_connect(void * const ctx,
                               const int type,
                               const char * const url,
                               void ** result) {
    bxiassert(NULL != ctx);
    bxiassert(NULL != url);
    bxiassert(NULL != result);
    bxierr_p err = BXIERR_OK, err2;

    void * socket = *result;
    if (NULL == socket) {
        err2 = _zocket_create(ctx, type, &socket);
        BXIERR_CHAIN(err, err2);
        if (bxierr_isko(err)) return err;
    }

    long sleep = 128; // nanoseconds
    struct timespec start;
    err2 = bxitime_get(CLOCK_MONOTONIC, &start);
    BXIERR_CHAIN(err, err2);
    double delay = 0.0;
    int tries = 0;
    while(delay < MAX_CONNECTION_TIMEOUT) {
        errno = 0;
        int rc = zmq_connect(socket, url);
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
    if (bxierr_isko(err)) {
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    *result = socket;
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
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }
    return err;
}


/*
 * Called for closing a zmq socket.
 * Returns 0, if ok.
 */
bxierr_p bxizmq_zocket_destroy(void * const zocket) {
    if (NULL == zocket) return BXIERR_OK;
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    int linger = DEFAULT_CLOSING_LINGER;
    int rc = zmq_setsockopt(zocket, ZMQ_LINGER, &linger, sizeof(linger));
    if (rc != 0) {
        err2 = _zmqerr(errno, "Can't set linger for socket cleanup");
        BXIERR_CHAIN(err, err2);
    }

    errno = 0;
    rc = zmq_close(zocket);
    if (rc != 0) {
        while (rc == -1 && errno == EINTR) rc = zmq_close(zocket);
        if (rc != 0) {
            err2 = _zmqerr(errno, "Can't close socket");
            BXIERR_CHAIN(err, err2);
        }
    }
    return err;
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

    while(n-- > 0) {
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
        delay_ns *= 2;
    }

    bxierr_unreachable_statement(__FILE__, __LINE__, __FUNCTION__);
}


/********************************* END Msg ****************************************/
/*********************************  DATA   ****************************************/


bxierr_p bxizmq_data_snd(void * const data, const size_t size,
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

inline bxierr_p bxizmq_str_snd(char * const str, void * zsocket, int flags,
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
            err2 = bxizmq_str_snd(BXIZMQ_PUBSUB_SYNC_HEADER, pub_zocket, ZMQ_SNDMORE, 0, 0);
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
    } while(delay < timeout_s);

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
                                BXIZMQ_PUBSUB_SYNC_HEADER, ARRAYLEN(BXIZMQ_PUBSUB_SYNC_HEADER) - 1);
    BXIERR_CHAIN(err, err2);

    char * sync_url = NULL;
    char * key = NULL;
    while(true) {

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
    err2 = bxizmq_zocket_connect(zmq_ctx, ZMQ_REQ, sync_url, &sync_zocket);
    BXIERR_CHAIN(err, err2);

    err2 = bxizmq_str_snd(sync_url, sync_zocket, ZMQ_DONTWAIT, 0, 0);
    BXIERR_CHAIN(err, err2);
    BXIFREE(sync_url);

    err2 = bxizmq_zocket_setopt(sub_zocket, ZMQ_UNSUBSCRIBE,
                                BXIZMQ_PUBSUB_SYNC_HEADER, ARRAYLEN(BXIZMQ_PUBSUB_SYNC_HEADER) - 1);

    zmq_pollitem_t poll_set[] = {
        { sync_zocket, 0, ZMQ_POLLIN, 0 },};

    while(true) {
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

    while(key != NULL) {
        BXIFREE(key);
        key = NULL;
        err2 = bxizmq_str_rcv(sub_zocket, ZMQ_DONTWAIT, false, &key);
        BXIERR_CHAIN(err, err2);
    }

    err2 = bxizmq_zocket_destroy(sync_zocket);
    BXIERR_CHAIN(err, err2);

    return err;
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

bxierr_p _zocket_create(void * const ctx, const int type, void ** result) {
    bxiassert(NULL != ctx);
    bxiassert(NULL != result);
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;

    void * socket = zmq_socket(ctx, type);
    if (socket == NULL) return _zmqerr(errno, "Can't create a zmq socket of type %d", type);

    int linger = 500;

    int rc = zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger));
    if (rc != 0) {
        err2 = _zmqerr(errno,
                       "Can't set option ZMQ_LINGER=%d "
                       "on zmq socket: %s", linger);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    int _hwm = 0 ;
    errno = 0;
    rc = zmq_setsockopt(socket, ZMQ_RCVHWM, &_hwm, sizeof(_hwm));
    if (rc != 0) {
        err2 = _zmqerr(errno, "Can't set option ZMQ_RCVHWM=%d on zmq socket", _hwm);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    _hwm = 0;
    errno = 0;
    rc = zmq_setsockopt(socket, ZMQ_SNDHWM, &_hwm, sizeof(_hwm));
    if (rc != 0) {
        err2 = _zmqerr(errno, "Can't set option ZMQ_SNDHWM=%d on zmq socket", _hwm);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }
    *result = socket;
    return BXIERR_OK;

}

bxierr_p _split_url(const char * const url, char * elements[3]) {
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
