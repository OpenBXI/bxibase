/* -*- coding: utf-8 -*-
 ###############################################################################
 # Authors: Sébastien Miquée <sebastien.miquee@atos.net>
 #          Pierre Vignéras <pierre.vigneras@atos.net>
 # Created on: 2013/11/21
 # Contributors:
 ###############################################################################
 # Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#ifndef BXIZMQ_H_
#define BXIZMQ_H_
#ifdef __cplusplus
extern "C" {
#endif

#ifndef BXICFFI
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <zmq.h>
#endif

#include "bxi/base/err.h"

#ifdef __DBG__
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif


/**
 * @file    zmq.h
 * @brief   ZeroMQ Handling Module
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

#define BXIZMQ_RETRIES_MAX_ERR      202372135              // Leet code: Z.QRETRIES
#define BXIZMQ_FSM_ERR              205322                 // Leet code: Z.Q.S.ERR
#define BXIZMQ_MISSING_FRAME_ERR    2015516                // Leet code: Z.Q.ISSI.G
#define BXIZMQ_UNEXPECTED_MSG       203893730              // Leet code: Z.Q..EXPE.TED

/**
 * The bxierr code for protocol error
 *
 * @note This is leet code for PROTO
 */
#define BXIZMQ_PROTOCOL_ERR 92070
/**
 * The bxierr code for timeout error
 *
 * @note This is leet code for TIEOT
 */
#define BXIZMQ_TIMEOUT_ERR 71307

// Protocol
#define BXIZMQ_PUBSUB_SYNC_HEADER   ".bxizmq/sync/"
#define BXIZMQ_PUBSUB_SYNC_PING     BXIZMQ_PUBSUB_SYNC_HEADER "pub->sub: ping"
#define BXIZMQ_PUBSUB_SYNC_PONG     BXIZMQ_PUBSUB_SYNC_HEADER "sub->pub: pong"
#define BXIZMQ_PUBSUB_SYNC_READY    BXIZMQ_PUBSUB_SYNC_HEADER "pub->sub: ready?"
#define BXIZMQ_PUBSUB_SYNC_ALMOST   BXIZMQ_PUBSUB_SYNC_HEADER "sub->pub: almost!"
#define BXIZMQ_PUBSUB_SYNC_LAST     BXIZMQ_PUBSUB_SYNC_HEADER "pub->sub: last"
#define BXIZMQ_PUBSUB_SYNC_GO       BXIZMQ_PUBSUB_SYNC_HEADER "sub->pub: go!"

/**
 * Default zeromq linger period used at zocket creation time
 */
#define BXIZMQ_DEFAULT_LINGER 1000u
//#define BXIZMQ_DEFAULT_LINGER -1
// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

#ifndef BXICFFI

/**
 * Create a zmq context.
 *
 * Any error encountered leads to a NULL pointer returned.
 *
 * @param ctx the newly created context (NULL on error)
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_context_new(void ** ctx);


/**
 * Close and destroy a zmq context. It cleans all the underlying sockets and
 * liberate the threads. At the end, the context variable is NULL.
 *
 * @param ctx the context to destroy
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_context_destroy(void ** ctx);

/**
 * Create a zeromq socket.
 *
 * @note: the created zocket have a default linger period set
 * to BXIZMQ_DEFAULT_LINGER.
 *
 * @param ctx the zeromq context
 * @param type the zeromq socket type (ZMQ_REQ, ZMQ_REP, ZMQ_PUB, ...)
 * @param zocket a pointer on the result
 *
 * @return BXIERR_OK on success, any other on failure.
 *
 * @see BXIZMQ_DEFAULT_LINGER
 */
bxierr_p bxizmq_zocket_create(void * const ctx, const int type, void ** zocket);

/**
 * Cleanup the given zeromq socket, releasing all underlying resources and nullifying
 * the given pointer.
 *
 * @param[inout] zocket_p the pointer on the socket to cleanup.
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_destroy(void ** const zocket_p);

/**
 * Cleanup the given zeromq socket, releasing all underlying resources and nullifying
 * the given pointer. This version sets a linger at 0 to not wait for closing zocket.
 *
 * @param[inout] zocket_p the pointer on the socket to cleanup.
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_destroy_no_wait(void ** const zocket_p);


/**
 * Bind the socket to the given url and set the specified option on it. If the
 * socket given as a parameter is NULL, then it will be created.
 *
 * Any error encountered leads to a NULL pointer for the socket.
 *
 * @param zocket the socket to bind
 * @param url the url to bind to
 * @param affected_port is the port selected by zmq for addresses like 'tcp://localhost:!'

 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_bind(void * const zocket,
                            const char * const url,
                            int  * affected_port);


/**
 * Connect the socket to the given url and set the hwm (high water mark) to 0. If the
 * socket given as a parameter is NULL, then it will be created.
 *
 * Any error encountered leads to a NULL pointer for the socket.
 *
 * @param zocket the socket to connect
 * @param url the url to connect to
 *
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_connect(void * zocket,
                               const char * const url);

/**
 * Disconnect a zocket from an url.
 *
 * @param[inout] zocket the socket to disconnect
 * @param[in] url the url to disconnect from
 *
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_disconnect(void * zocket, const char * url);

/**
 * Get the specified option on the zmq socket
 *
 * @param socket socket from which the option should be get
 * @param option_name the name of the option
 * @param option_value the value of the option
 * @param option_len the length of the option value
 *
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_getopt(void * socket,
                              int option_name,
                              void * option_value,
                              size_t * option_len);

/**
 * Set the specified option on the zmq socket
 *
 * @param self socket on which the option should be apply
 * @param option_name the name of the option
 * @param option_value the value of the option
 * @param option_len the length of the option value
 *
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_setopt(void * self,
                              const int option_name,
                              const void * const option_value,
                              const size_t option_len
                             );

/**
 * Short cut for zocket creation and connection to a single url.
 *
 * @param ctx a zeromq context
 * @param type the zeromq socket type to create
 * @param url the url to connect the socket to
 * @param zocket the resulting socket (NULL on error)
 *
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_create_connected(void *const ctx,
                                        const int type,
                                        const char *const url,
                                        void **zocket);


/**
 * Short cut for zocket creation and binding to a single url.
 *
 * @param ctx a zeromq context
 * @param type the zeromq socket type to create
 * @param url the url to bind the socket to
 * @param affected_port the port selected by zmq for addresses like 'tcp://localhost:!'
 * @param zocket the resulting socket (NULL on error)
 *
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_zocket_create_binded(void *const ctx,
                                     const int type,
                                     const char *const url,
                                     int * affected_port,
                                     void **zocket);



/**
 * Initialize the given zeromq message.
 *
 * @param zmsg the zeromq message to initialize
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_msg_init(zmq_msg_t * zmsg);


/**
 * Close the given msg and exit on errors.
 *
 * @param zmsg the zeromq message to close
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_msg_close(zmq_msg_t * zmsg);

/**
 * Try asynchronous sending of the given zmq message `zmsg`
 * through the given zmq socket `zocket`.
 *
 * After each failure (EAGAIN) sleep for `delay` seconds.
 * If after `retries_max` retries, it still fails (EAGAIN), retry
 * synchronously and return a bxierr with error code
 * `BXIZMQ_RETRIES_MAX_ERR` and the number of retries
 * as the error data (casted from a `size_t`, it is not a pointer).
 *
 * Note: if `retries_max == 0`, this call is equivalent to a synchronous
 * send.
 *
 * @param zmsg the zeromq message to send
 * @param zocket the zeromq socket the message must be sent from
 * @param flags a zeromq flag
 * @param retries_max the number of retries before switching to synchronous sending
 * @param delay_ns the maximum number of nanoseconds to sleep before a retry
 * @return BXIERR_OK on succes,
 *         `bxierr(code=BXIZMQ_RETRIES_MAX_ERR, data=(size_t) retries_nb)`
 *         among others.
 */
bxierr_p bxizmq_msg_snd(zmq_msg_t * zmsg,
                        void * zocket, int flags,
                        const size_t retries_max, long delay_ns);


/**
 * Copy a ZMQ message.
 * @param zmsg_src the zeromq message source
 * @param zmsg_dst the zeromq message destination
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_msg_copy(zmq_msg_t * zmsg_src, zmq_msg_t * zmsg_dst);


/**
 * Receive a message.
 *
 * @param zocket the zeromq socket
 * @param zmsg the zeromq message
 * @param flags the zeromq flag
 * @return BXIERR_OK on success, any other on failure.
 */
bxierr_p bxizmq_msg_rcv(void * zocket, zmq_msg_t * zmsg, int flags);


/**
 * Try to receive a message asynchronously a given maximum of `retries_max` times.
 *
 * After each failure (EAGAIN), the calling thread sleep for `delay_ns` nanoseconds
 * and tries again, up to `retries_max` attempt.
 *
 * If after `retries_max` attempt, the message cannot be received without blocking,
 * the error with error code BXIERR_RETRIES_MAX_ERR is returned with the number of
 * retries as the data (casted from a `size_t`, it is not a pointer).
 *
 * @param zocket the zeromq socket
 * @param msg the zeromq message
 * @param retries_max the maximum number of retries
 * @param delay_ns the maximum number of nanoseconds to sleep between two retries
 *
 * @return BXIERR_OK on success,
 *         `bxierr(code=BXIZMQ_RETRIES_MAX_ERR, data=(size_t) retries_nb)`
 *         among others
 */
bxierr_p bxizmq_msg_rcv_async(void *zocket, zmq_msg_t *msg,
                              size_t retries_max, long delay_ns);


/**
 * Return true if the next frame the given zocket will received comes
 * from a multipart message.
 *
 * @param zocket the zeromq socket
 * @param result a pointer on the result
 *
 * @return BXIERR_OK on success
 */
bxierr_p bxizmq_msg_has_more(void *zocket, bool* result);


/**
 * Send the given data through the given zeromq socket.
 *
 * See bxizmq_msg_snd() for details on parameters and returned code.
 *
 * Usage:
 *
 *      int i = 12345;
 *      // Synchronous send
 *      bxierr_p err = bxizmq_msg_snd(&i, sizeof(int), socket, 0, 0, 0);
 *      bxiassert(BXIERR_OK == retries);
 *      // Asynchronous send, 4 retries, sleep 1us
 *      err = bxizmq_msg_snd(&i, sizeof(int), socket, ZMQ_DONTWAIT, 4, 1000);
 *      bxiassert(BXIERR_OK || 4 >= (size_t) err.data);
 *
 * @param data the data to send
 * @param size the size of the data
 * @param zocket the zeromq to send the data with
 * @param flags the zeromq flags
 * @param retries_max the number of maximum retries
 * @param delay_ns the maximum number of nanoseconds to wait between two retries
 *
 * @return BXIERR_OK on success
 *
 * @see bxizmq_msg_snd()
 */
bxierr_p bxizmq_data_snd(const void * data, size_t size, void * zocket,
                         int flags, size_t retries_max,
                         long delay_ns);



/**
 * Equivalent to bxizmq_data_snd() but with zero copy.
 *
 * This means that the given *data pointer should not be used afterwards!
 *
 * The given data will be freed using the given 'ffn' function with the data
 * and the given hint in parameter. If you need a simple free(data)
 * without the hint, use bxizmq_data_free() as in the following:
 *
 *      bxizmq_snd_msg_zc(data, size, zocket, flags, retries_max, delay_ns,
 *                        bxizmq_data_free, NULL);
 *
 * @param data the data to send
 * @param size the size of the data
 * @param zocket the zeromq to send the data with
 * @param flags the zeromq flags
 * @param retries_max the number of maximum retries
 * @param delay_ns the maximum number of nanoseconds to wait between two retries
 * @param ffn the function to use to free the data after it has been sent
 * @param hint any data useful for the given function `ffn`
 *
 * @return BXIERR_OK on success
 * @see bxizmq_msg_snd()
 */
bxierr_p bxizmq_data_snd_zc(const void * data, size_t size, void * zocket,
                            int flags, size_t retries_max,
                            long delay_ns,
                            zmq_free_fn *ffn, void *hint);


/**
 * Receive a zmq message through the given zocket with the given ZMQ flags,
 * fills the given `*result` pointer with the received content and return the number of
 * bytes received in the `received_size` pointer if it is not NULL.
 *
 * If `*result` is given NULL, then it is set to a new allocated area filled with
 * the message content. This area will have to be freed using BXIFREE(). The size of
 * the allocated region is of the specified expected size unless it is 0. In this case
 * the allocated region is of the received size.
 *
 * If `*result` is not NULL, it is filled with the message content unless the
 * received message size is strictly greater than the given `expected_size`.
 * In this case the program will exit with an error message (since it is
 * dangerous to fill `*result` with more bytes than expected).
 *
 * Note that if the received message size is less than `expected_size` the
 * `*result` will be filled up to the received message size and no more.
 * Therefore, `*result` might holds inconsistent data: the beginning of the region
 * contains a copy of the message content up to the received size, but the rest
 * (expected_size - received_size) will still contains its previous content.
 *
 * If this is a problem, check that `*received_size == expected_size`.
 *
 * if `check_more` is `true`, a check is made to know if another zeromq frame is expected
 * from the given zocket (using zmq_getsockopt()) that is related to a
 * zeromq multipart message. If it is not the case,
 * a bxierr(code=BXIZMQ_MISSING_FRAME_ERR) is returned.
 *
 * If `flags` contains ZMQ_DONTWAIT and EAGAIN was returned while receiving (meaning
 * no message was received at that time), then `*result` will be NULL and
 * `*received_size` will be 0 if received_size is not NULL.
 *
 * If `expected_size` is 0 and `*received_size` is 0, the given `*result` pointer is left
 * untouched and BXIERR_OK is returned. This can be used for signaling purpose
 * (if you send something with a zero size), you will receive a zero sized message.
 *
 * @param result a pointer on the storage location where the received data must be
 *               stored (if *result is NULL the area will be mallocated)
 * @param expected_size the expected size of the received data
 * @param zocket the zeromq socket the data should be received from
 * @param flags a zeromq flag
 * @param check_more tells if the next data must be part of a zeromq message or not
 * @param received_size the actual size of the received data (can be NULL)
 * @return BXIERR_OK on success, bxierr(code=BXIZMQ_MISSING_FRAME_ERR) among others.
 */
bxierr_p bxizmq_data_rcv(void ** result, size_t expected_size,
                         void * zocket, int flags,
                         bool check_more, size_t *received_size);



/**
 * Send the given string through the given zocket with the given ZMQ flags.
 *
 *
 * @param str the NULL terminated string to send
 * @param zocket the zeromq socket to send the string with
 * @param flags some zeromq flags
 * @param retries_max the maximum number of retries
 * @param delay_ns the maximum number of nanoseconds to wait between two retries
 *
 * @return BXIERR_OK on success
 *
 * @see bxizmq_msg_snd()
 */
bxierr_p bxizmq_str_snd(const char * const str, void * zocket, int flags,
                        size_t retries_max, long delay_ns);


/**
 * Send the given string through the given zocket with the given ZMQ flags without
 * performing a copy. The given string should not be modified after this call.
 *
 * @param[in] str the NULL terminated string to send
 * @param[inout] zocket the zeromq socket to send the string with
 * @param[in] flags some zeromq flags
 * @param[in] retries_max the maximum number of retries
 * @param[in] delay_ns the maximum number of nanoseconds to wait between two retries
 * @param[in] freestr if true, release the given string using BXIFREE
 *
 * @return BXIERR_OK on success
 *
 * @see bxizmq_msg_snd()
 */
bxierr_p bxizmq_str_snd_zc(const char * const str, void * zocket, int flags,
                           size_t retries_max, long delay_ns, bool freestr);


/**
 * Receive a string through the given zocket with the given ZMQ flags.
 * Note that the returned string has been allocated. Therefore, it should
 * be freed using BXIFREE().
 *
 * if ZMQ_DONTWAIT has been set in flags, this function can return NULL,
 * meaning that nothing was (fully) received at the time this call was made,
 * and suggesting that retrying might work.
 *
 * if `check_more` is `true`, a check is made to know if another zeromq frame is expected
 * from the given zocket (using zmq_getsockopt()) that is related to a
 * zeromq multipart message. If it is not the case, the program exit with an
 * error message.
 *
 * Receiving a multipart message looks like the following sample code:
 *
 *      char * frame0, frame1, frame2;
 *      bxierr_p err0, err1, err2;
 *      err0 = bxizmq_str_rcv(zocket, 0, false, &frame0);
 *      err1 = bxizmq_str_rcv(zocket, 0, true, &frame1);
 *      err2 = bxizmq_str_rcv(zocket, 0, true, &frame2);
 *      ...
 *      BXIFREE(frame0);
 *      BXIFREE(frame1);
 *      BXIFREE(frame2);
 *
 * @param zocket the socket to receive a message from
 * @param flags some zeromq flags
 * @param check_more if true, check that the received frame is part of
 *        a multi-part message
 * @param result where the received data should be stored in
 *
 * @return BXIERR_OK on success, any other bxierr otherwise
 */
bxierr_p bxizmq_str_rcv(void * zocket, int flags, bool check_more, char ** result);


/**
 * Function utility to be used with bximisc_snd_data_zc()
 * and bxizmq_snd_str_zc() functions.
 *
 * @param data the data to free
 * @param hint unused
 */
void bxizmq_data_free(void * data, void * hint);
#endif

/**
 * Synchronize a PUB zocket with a SUB zocket using a sync REP zocket.
 *
 * This guarantees no message published is lost by the SUB zocket after a
 * successful synchronization (classical problem of PUB/SUB zeromq
 * synchronization, refer to the zeromq guide for details:
 * http://zguide.zeromq.org/page:all#toc47)
 *
 * @param pub_zocket the PUB zocket to be synchronized
 * @param sync_zocket REP zocket use to communicate with the SUB zocket
 * @param sync_url used to bind the PUB zocket
 * @param sync_url_len length of the url
 * @param timeout_s in seconds before aborting
 *
 * @return BXIERR_OK if the synchronization completes successfully.
 */
bxierr_p bxizmq_sync_pub(void * pub_zocket,
                         void * sync_zocket,
                         char * const sync_url,
                         const size_t sync_url_len,
                         const double timeout_s);


/**
 * Synchronize a SUB zocket.
 *
 * This guarantees no message published is lost by the SUB zocket after a
 * successful synchronization (classical problem of PUB/SUB zeromq
 * synchronization, refer to the zeromq guide for details:
 * http://zguide.zeromq.org/page:all#toc47).
 *
 * @param zmq_ctx required for socket creation
 * @param sub_zocket to be synchronized
 * @param timeout_s in seconds before aborting.
 *
 * @return BXIERR_OK if the synchronization is done.
 */
bxierr_p bxizmq_sync_sub(void * zmq_ctx,
                         void * sub_zocket,
                         const double timeout_s);

/**
 * Deal with a SYNC message received by a SUB socket when the publisher uses
 * bxizmq_sync_pub().
 *
 * @note this function must be called only after a first zeromq frame containing
 * BXIZMQ_PUBSUB_SYNC_HEADER has been received.
 *
 * @param[in] zmq_ctx the zmq context to use for the creation of an internal
 *                    control zocket
 * @param[in] sub_zocket the SUB socket to use for receiving next zmq frame
 *
 * @return BXIERR_OK if successful, anything else otherwise.
 */
bxierr_p bxizmq_sub_sync_manage(void * zmq_ctx,
                                void * sub_zocket);

/**
 * Synchronize a PUB zocket with many SUB zockets.
 *
 * This guarantees no message published is lost by SUB zockets after a
 * successful synchronization (classical problem of PUB/SUB zeromq
 * synchronization, refer to the zeromq guide for details:
 * http://zguide.zeromq.org/page:all#toc47)
 *
 * @param[inout] zmq_ctx the zeromq context to use for internal zocket creation
 * @param[inout] pub_zocket the zocket to synchronize, it must be a PUB
 * @param[in] url the url used for publication
 * @param[in] sub_nb the number of subscribers to wait for
 * @param[in] timeout_s the maximal number of seconds to wait for subscribers
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxizmq_sync_pub_many(void * zmq_ctx,
                              void * pub_zocket,
                              const char * url,
                              size_t sub_nb,
                              const double timeout_s);

/**
 * Synchronize a SUB zocket with many PUB zockets.
 *
 * This guarantees no message published is lost by SUB zocket after a
 * successful synchronization (classical problem of PUB/SUB zeromq
 * synchronization, refer to the zeromq guide for details:
 * http://zguide.zeromq.org/page:all#toc47)
 *
 * @param[inout] zmq_ctx the zeromq context to use for internal zocket creation
 * @param[inout] sub_zocket the zocket to synchronize
 * @param[in] pub_nb the number of publishers to wait for
 * @param[in] timeout_s the maximal number of seconds to wait for publishers
 *
 * @return BXIERR_OK on success, anything else on error.
 */
bxierr_p bxizmq_sync_sub_many(void * zmq_ctx,
                         void * sub_zocket,
                         size_t pub_nb,
                         const double timeout_s);

/**
 * Generate a fresh new zeromq URL from the given one.
 *
 * The returned URL will share the same protocol than the one given.
 *
 * If the protocol is of the form 'inproc://something', the new URL will look like
 * 'inproc://something-ID' where ID is a string unique to the whole calling process.
 *
 * If the protocol is of the form 'ipc://path/to/somewhere', the new URL will look like
 * 'ipc://path/to/somewhere-ID' where ID is a string unique to the whole system.
 *
 * If the protocol is of the form 'tcp://host:port', the new URL will look like
 * 'tcp://host:*'.
 *
 * @param url the URL to generate a new one from
 * @param result a pointer on the result
 *
 * @return BXIERR_OK on success, anything else on error
 *
 *
 */
bxierr_p bxizmq_generate_new_url_from(const char * const url, char ** result);

char * bxizmq_create_url_from(const char * const url, const int tcp_port);

bxierr_p bxizmq_split_url(const char * const url, char * elements[3]);

bxierr_p bxizmq_err(int errnum, const char * fmt, ...);
#ifdef __cplusplus
}
#endif
#endif /* BXIZMQ_H_ */
