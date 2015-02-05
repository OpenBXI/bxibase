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
#include <assert.h>
#include <stdlib.h>

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

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************

// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************

bxierr_p _zmq_context_creation(void * const ctx, const int type, void ** result);

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************
static volatile int counter = 0;


// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************

/*************************************** Zocket ***********************************/
bxierr_p bxizmq_zocket_bind(void * const ctx,
                           const int type,
                           const char * const url,
                           int  * affected_port,
                           void ** result) {
    assert(NULL != ctx);
    assert(NULL != url);
    assert(NULL != result);
    bxierr_p err = BXIERR_OK, err2, err3;

    unsigned long port = 0;

    void * socket = NULL;
    err2 = _zmq_context_creation(ctx, type, &socket);
    if (bxierr_isko(err2)) {
        return err2;
    }

    int rc = zmq_bind(socket, url);
    if (rc == -1) {
        err2 = bxierr_errno("Can't bind zmq socket on %s", url);
        BXIERR_CHAIN(err, err2);
    }
    if (bxierr_isko(err)) {
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    char endpoint[512];
    endpoint[0] = '\0';

    if (NULL != affected_port && strncmp(url, INPROC_PROTO, ARRAYLEN(INPROC_PROTO) - 1) != 0)
    {
        assert(NULL != socket);
        size_t endpoint_len = 512;
        errno = 0 ;

        rc = zmq_getsockopt(socket, ZMQ_LAST_ENDPOINT, &endpoint, &endpoint_len);
        if (rc == -1) {
            if (errno == EINVAL || errno == ETERM || errno == ENOTSOCK || errno == EINTR)
            {
                err2 = bxierr_errno("Can't retrieve the zocket endpoint option");
                BXIERR_CHAIN(err, err2);
            }
        }
    }

    if (NULL != endpoint && strlen(endpoint) > 0)
    {
        char * ptr = strrchr(endpoint, ':');
        if (NULL == ptr)
        {
            err2 = bxierr_errno("Unable to retrieve the binded port number");
            BXIERR_CHAIN(err, err2);
        }
        char * endptr = NULL;
        errno = 0;
        port = strtoul(ptr+1, &endptr, 10);
        if (errno == ERANGE) {
            err2 = bxizmq_zocket_destroy(socket);
            err3 = bxierr_errno("Unable to parse integer in: '%s'", ptr+1);
            BXIERR_CHAIN(err3, err2);
            BXIERR_CHAIN(err, err2);
            return err;
        }
    }

    counter++;
    if (0 != port)
    {
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
    assert(NULL != ctx);
    assert(NULL != url);
    assert(NULL != result);
    bxierr_p err = BXIERR_OK, err2;

    void * socket = NULL;
    err2 = _zmq_context_creation(ctx, type, &socket);
    if (bxierr_isko(err2)) {
        return err2;
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
        if (rc == 0) break;
        if (errno != ECONNREFUSED) {
            err2 = bxierr_errno("Can't connect zmq socket on %s", url);
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

    counter++;
    *result = socket;
    return err;
}


bxierr_p bxizmq_zocket_setopt(void * socket,
                              const int option_name,
                              const void * const option_value,
                              const size_t option_len
                             ) {
    assert(NULL != socket);
    assert(NULL != option_value);
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    const int rc = zmq_setsockopt(socket, option_name, option_value, option_len);
    if (rc != 0) {
        err2 = bxierr_errno("Can't set option %d on zmq socket",
                            option_name);
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
    errno = 0;
    int linger = DEFAULT_CLOSING_LINGER;
    int rc = zmq_setsockopt(zocket, ZMQ_LINGER, &linger, sizeof(linger));
    if (rc != 0) return bxierr_errno("Can't set linger for socket cleanup");
    errno = 0;
    rc = zmq_close(zocket);
    if (rc != 0) {
        while (rc == -1 && errno == EINTR) rc = zmq_close(zocket);
        if (rc != 0) return bxierr_errno("Can't close socket");
    }
    counter--;
    return BXIERR_OK;
}
/*********************************** END Zocket ***********************************/

/************************************* Msg ****************************************/


// Initialize a ZMQ message
bxierr_p bxizmq_msg_init(zmq_msg_t * zmsg) {
    errno = 0;
    int rc = zmq_msg_init(zmsg);
    if (rc != 0) return bxierr_errno("Problem during ZMQ message initialization");
    return BXIERR_OK;
}


// Receive a ZMQ message
bxierr_p bxizmq_msg_rcv(void * const zocket,
                        zmq_msg_t * zmsg,
                        int const flags) {
    errno = 0;
    int rc = zmq_msg_recv(zmsg, zocket, flags);
    if (rc == -1) {
        while (rc == -1 && errno == EINTR) rc = zmq_msg_recv(zmsg, zocket, flags);
        if (0 != rc) return bxierr_errno("Problem while receiving ZMQ message");
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
                           size_t retries_max, const long delay_ns) {

    bxierr_p current = BXIERR_OK;

    while(retries_max-- > 0) {
        errno = 0;
        bxierr_p tmp = bxizmq_msg_rcv(zocket, msg, ZMQ_DONTWAIT);
        if (BXIERR_OK == tmp) return BXIERR_OK;
        BXIERR_CHAIN(current, tmp);
        if (current->code != EAGAIN) return current;
        bxierr_p new = bxitime_sleep(CLOCK_MONOTONIC, 0, delay_ns);
        BXIERR_CHAIN(current, new);
    }

    return bxierr_gen("Waiting on socket tooks too much time!");
}


// Close a ZMQ message
bxierr_p bxizmq_msg_close(zmq_msg_t * const zmsg) {
    errno = 0;
    int rc = zmq_msg_close(zmsg);
    if (rc != 0) return bxierr_errno("Calling zmq_msg_close() failed");

    return BXIERR_OK;
}


// Copy a ZMQ message
bxierr_p bxizmq_msg_copy(zmq_msg_t * const zmsg_src, zmq_msg_t * const zmsg_dst){
    int rc = zmq_msg_copy(zmsg_dst, zmsg_src);
    if (rc != 0) return bxierr_errno("Problem while copying a zmsg");

    return BXIERR_OK;
}

inline bxierr_p bxizmq_msg_has_more(void * const zsocket, bool * const result) {
    int64_t more = 0;
    size_t more_size = sizeof(more);
    int rc = zmq_getsockopt(zsocket, ZMQ_RCVMORE, &more, &more_size);
    if (-1 == rc) return bxierr_errno("Can't call zmq_getsockopt()");
    *result = more;
    return BXIERR_OK;
}


bxierr_p bxizmq_msg_snd(zmq_msg_t * const zmsg,
                        void * const zocket, int flags,
                        const size_t retries_max, long delay_ns) {
    assert(NULL != zmsg);
    assert(NULL != zocket);

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
                                      "Sending a message "
                                      "needed %zu retries", retries);
            BXIERR_CHAIN(current, new);
            return current;
        }
        assert(n == -1);
        if (errno != EAGAIN && errno != EINTR) {
            bxierr_p new;
            if (errno == EFSM) {
                new = bxierr_new(BXIZMQ_FSM_ERR,
                                          NULL, NULL, NULL,
                                          "Invalid state for sending through zsocket %p" \
                                          " (error code is zeromq EFSM)", zocket);
            } else {
                new = bxierr_errno("Can't send msg through zsocket %p", zocket);
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


    return bxierr_gen("Should never reach this statement!");
}


/********************************* END Msg ****************************************/
/*********************************  DATA   ****************************************/


bxierr_p bxizmq_data_snd(void * const data, const size_t size,
                         void * const zocket, const int flags,
                         const size_t retries_max, long delay_ns) {
    assert(NULL != data);
    assert(NULL != zocket);
    zmq_msg_t msg;
    errno = 0;
    int rc = zmq_msg_init_size(&msg, size);
    if (rc != 0) return bxierr_errno("Calling zmq_msg_init_size() failed");
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
    assert(NULL != data);
    assert(NULL != zocket);
    zmq_msg_t msg;
    errno = 0;
    int rc = zmq_msg_init_data(&msg, (void *)data, size, ffn, hint);
    if (0 != rc) return bxierr_errno("Calling zmq_msg_init_data() failed");

    bxierr_p current = bxizmq_msg_snd(&msg, zocket, flags, retries_max, delay_ns);
    bxierr_p new = bxizmq_msg_close(&msg);
    BXIERR_CHAIN(current, new);

    return current;

}

bxierr_p bxizmq_data_rcv(void ** result, const size_t expected_size,
                         void * const zsocket, const int flags,
                         const bool check_more, size_t * received_size) {
    bxierr_p current = BXIERR_OK, new;
    if (check_more) {
        bool more = false;
        new = bxizmq_msg_has_more(zsocket, &more);
        if (BXIERR_OK != new) return new;
        if (!more) return bxierr_new(BXIZMQ_MISSING_FRAME_ERR,
                                     NULL,
                                     NULL,
                                     NULL,
                                     "Missing zeromq frame on socket %p", zsocket);
    }
    zmq_msg_t zmsg;
    errno = 0;
    int rc = zmq_msg_init(&zmsg);
    if (-1 == rc) return bxierr_errno("Calling zmq_msg_init() failed");
    errno = 0;
    rc = zmq_msg_recv(&zmsg, zsocket, flags);
    if (-1 == rc) {
        while (-1 == rc && EINTR == errno) {
            errno = 0;
            rc = zmq_msg_recv(&zmsg, zsocket, flags);
        }
        if (-1 == rc) {
            if (errno != EAGAIN) {
                return bxierr_errno("Can't receive a msg through zsocket: %p", zsocket);
            }
            assert((flags & ZMQ_DONTWAIT) == ZMQ_DONTWAIT);
            *result = NULL;
            *received_size = 0;
            return current;
        }
    }

    size_t rcv_size = zmq_msg_size(&zmsg);
    if (NULL != *result && rcv_size > expected_size) {
        new = bxizmq_msg_close(&zmsg);
        BXIERR_CHAIN(current, new);
        new = bxierr_gen("Received more bytes than expected on zsocket %p:"
                         " expected %zu, got: %zu",
                           zsocket, expected_size, rcv_size);
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
    memcpy(*result, zmq_msg_data(&zmsg), rcv_size);

END:
    new = bxizmq_msg_close(&zmsg);
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
                                  size_t retries_max, long delay_ns) {
    // Since we use strlen() it means the last '\0' is not sent!
    // Therefore the corresponding rcv() function cannot rely on the
    // received size to allocate the memory, it should allocate one byte more.
    // See bxizmq_rcv_str() for details.
    return bxizmq_data_snd_zc(str, strlen(str), zsocket, flags,
                              retries_max, delay_ns,
                              bxizmq_data_free, NULL);
}






bxierr_p bxizmq_str_rcv(void * const zsocket, const int flags, const bool check_more,
                        char ** result) {
    // This function cannot use the bximisc_rcv_msg() since the amount of data sent
    // for a string is strlen(): the last '\0' is not sent therefore!
    // When we receive such a message the expected size should therefore be the
    // strlen(), but the allocated memory should strlen() + 1!!
    bxierr_p current = BXIERR_OK;
    if (check_more) {
        bool more = false;
        current = bxizmq_msg_has_more(zsocket, &more);
        if (BXIERR_OK != current) return current;
        if (!more) return bxierr_new(BXIZMQ_MISSING_FRAME_ERR,
                                     NULL,
                                     NULL,
                                     NULL,
                                     "Missing zeromq frame on socket %p", zsocket);
    }
    zmq_msg_t msg;
    int rc = zmq_msg_init(&msg);
    if (rc != 0) return bxierr_errno("Can't initialize msg");
    errno = 0;
    rc = zmq_msg_recv(&msg, zsocket, flags);
    if (-1 == rc) {
        while (-1 == rc && EINTR == errno){
            errno = 0;
            rc = zmq_msg_recv(&msg, zsocket, flags);
        }
        if (-1 == rc) {
            if (EAGAIN != errno) {
                if (EFSM == errno) return bxierr_new(BXIZMQ_FSM_ERR,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     "Can't receive a msg "
                                                     "through zsocket: %p:"
                                                     " ZMQ EFSM (man zmq_recv)",
                                                     zsocket);
                return bxierr_errno("Can't receive a msg through zsocket: %p", zsocket);
            }
            assert(ZMQ_DONTWAIT == (flags & ZMQ_DONTWAIT));
            *result = NULL;
            return BXIERR_OK;
        }
    }
    size_t len = zmq_msg_size(&msg);
    // calloc, fills with 0, no need to add the NULL terminating byte
    *result = bximem_calloc(len + 1);
    memcpy(*result, zmq_msg_data(&msg), len);
    bxierr_p new = bxizmq_msg_close(&msg);
    BXIERR_CHAIN(current, new);

    return current;
}
/********************************* END STR  ****************************************/

// *********************************************************************************
// **************************** Static function ************************************
// *********************************************************************************
// 
bxierr_p _zmq_context_creation(void * const ctx, const int type, void ** result) {
    assert(NULL != ctx);
    assert(NULL != result);
    bxierr_p err = BXIERR_OK, err2;
    errno = 0;
    void * socket = zmq_socket(ctx, type);
    if (socket == NULL) return bxierr_errno("Can't create a zmq socket of type %d", type);
    int _hwm = 0 ;
    errno = 0;
    int rc = zmq_setsockopt(socket, ZMQ_RCVHWM, &_hwm, sizeof(_hwm));
    if (rc != 0) {
        err2 = bxierr_errno("Can't set option ZMQ_RCVHWM=%d "
                            "on zmq socket", _hwm);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }

    _hwm = 0;
    errno = 0;
    rc = zmq_setsockopt(socket, ZMQ_SNDHWM, &_hwm, sizeof(_hwm));
    if (rc != 0) {
        err2 = bxierr_errno("Can't set option ZMQ_SNDHWM=%d "
                            "on zmq socket", _hwm);
        BXIERR_CHAIN(err, err2);
        err2 = bxizmq_zocket_destroy(socket);
        BXIERR_CHAIN(err, err2);
        return err;
    }
    *result = socket;
    return BXIERR_OK;

}

// Used by bxizmq_snd_str_zc() for freeing a simple mallocated string
void bxizmq_data_free(void * const data, void * const hint) {
    UNUSED(hint);
    BXIFREE(data);
}
