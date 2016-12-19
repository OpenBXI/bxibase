// Hello World client

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <zmq.h>
#include <pthread.h>
#include <errno.h>
#include <error.h>


#define URL "inproc://test"
#define BUF_SIZE 1000
#define MAX_RETRIES 3

size_t SENDER_NB; // Initialized in the main
size_t LOOP_NB;   // Initialized in the main
void * CONTEXT;   // Initialized in the main

struct blob_t {
    size_t id;
    uint8_t buf[BUF_SIZE];
};

void * thread_receiver(void * dummy) {
    void *worker = zmq_socket(CONTEXT, ZMQ_PULL);
    int hwm = 0;
    size_t hwm_size = sizeof(hwm);
//    zmq_setsockopt(worker, ZMQ_RCVHWM, &hwm, hwm_size);

    int rc = zmq_bind(worker, URL);
    assert(0 == rc);
    rc = zmq_getsockopt(worker, ZMQ_RCVHWM, &hwm, &hwm_size);
    assert(-1 != rc);

    printf("RCV_HWM: %d\n", hwm);

    size_t nb[SENDER_NB];
    bool end[SENDER_NB];

    for(size_t i = 0; i < SENDER_NB; i++) {
        nb[i] = 0;
        end[i] = false;
    }

    zmq_pollitem_t items[] = { { worker, 0, ZMQ_POLLIN, 0 } };

    long actual_timeout = 1000;
    struct timespec last_time;
    rc = clock_gettime(CLOCK_MONOTONIC_RAW, &last_time);

    bool again = true;

    while (again) {
        errno = 0;
        int n = zmq_poll(items, 1, actual_timeout);

        if (-1 == n) {
            if (EINTR == errno) continue; // One interruption happened
                                          // (e.g. with profiling)

            int code = zmq_errno();
            const char * msg = zmq_strerror(code);
            fprintf(stderr, "Error while polling: %s", msg);
        }

        struct timespec now;
        int rc = clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        assert(0 == rc);
        double sec =   (double) now.tv_sec  - (double) last_time.tv_sec;
        double nsec =  (double) now.tv_nsec - (double) last_time.tv_nsec;
        double tmp = (sec + nsec * 1e-9);
        long duration_since_last = (long) (tmp * 1e3);

        actual_timeout = 1000 - duration_since_last;

//        fprintf(stderr, "Actual timeout: %ld\n", actual_timeout);
        if (0 == n || 0 >= actual_timeout) {
            fprintf(stderr, "Implicit flush - %lf - timeout: %ld\n", tmp, actual_timeout);

            clock_gettime(CLOCK_MONOTONIC_RAW, &last_time);
            actual_timeout = 1000;
            continue;
        }
        if (items[0].revents & ZMQ_POLLIN) {
            // Process data, this is the normal case
            zmq_msg_t mzg;
            errno = 0;
            int rc = zmq_msg_init(&mzg);
            assert(0 == rc);
            errno = 0;
            rc = zmq_msg_recv(&mzg, worker, 0);
            if (-1 == rc) {
                perror("Can't receive a message");
                exit(1);
            }
            size_t size = zmq_msg_size(&mzg);
            struct blob_t * blob;
            size_t sender;
//          printf("Received %zu bytes\n", size);
            if (size < BUF_SIZE) {
                // This is the end message
                memcpy(&sender, zmq_msg_data(&mzg), size);
                assert(sender < SENDER_NB);
                end[sender] = true;
            } else {
                // This is the normal message
                blob = zmq_msg_data(&mzg);
                sender = blob->id;
                assert(sender < SENDER_NB);
            }

//            printf("Received msg from %zu\n", sender);
            nb[sender]++;
            zmq_msg_close(&mzg);

            bool exit = true;
            for (size_t i = 0; i < SENDER_NB; i++) {
                if (!end[i]) {
                    exit = false;
                    break;
                }
            }
            if (exit) again = false;
        }
    }
    printf("All terminated messages received\n");
    rc = zmq_close(worker);
    assert(0 == rc);

    size_t result = 0;
    for (size_t i = 0; i < SENDER_NB; i++) {
        result += nb[i];
    }
    return (void *) result;
}

void * thread_sender(void * vid) {
    size_t id = (size_t) vid;
    void * master = zmq_socket(CONTEXT, ZMQ_PUSH);
    assert(NULL != master);


    int hwm = 0;
    size_t hwm_size = sizeof(hwm);
    int rc = zmq_setsockopt(master, ZMQ_SNDHWM, &hwm, hwm_size);

    rc = zmq_connect(master, URL);
    assert(0 == rc);


    rc = zmq_getsockopt(master, ZMQ_SNDHWM, &hwm, &hwm_size);
    assert(-1 != rc);

    printf("SND_HWM: %d\n", hwm);

    struct blob_t * blob_p = NULL;

    for (size_t i = 0; i < LOOP_NB; i++) {
        blob_p = malloc(sizeof(*blob_p));
        assert(NULL != blob_p);
        blob_p->id = id;

        zmq_msg_t mzg;
        rc = zmq_msg_init_size(&mzg, sizeof(*blob_p));
        memcpy(zmq_msg_data(&mzg), blob_p, sizeof(*blob_p));
//        printf("Sender %zu: sending %zu bytes\n", id, sizeof(blob));
        size_t tries = 1;
        int flags = ZMQ_DONTWAIT;
        while (true) {
            errno = 0;
            const int n = zmq_msg_send(&mzg, master, flags);
            if (n >= 0) break;

            if (errno == EAGAIN) {
                if (tries < MAX_RETRIES) {
                    //                  printf("%zu: retrying %zu\n", i, tries++);
                } else {
                    flags = 0;
                }
            } else {
                fprintf(stderr, "%zu: error while sending, exiting\n", i);
                exit(1);
            }
        }
        rc = zmq_msg_close(&mzg);
        assert(0 == rc);
        //      if (0 == (i %  1000)) printf("Nb of messages sent: %zu\n", i);
        free(blob_p);
    }

    zmq_msg_t mzg;
    zmq_msg_init_size(&mzg, sizeof(id));
    memcpy(zmq_msg_data(&mzg), &id, sizeof(id));
    rc = zmq_msg_send(&mzg, master, 0);
    assert(-1 != rc);
    rc = zmq_msg_close(&mzg);
    assert(0 == rc);

    printf("Terminating message sent\n");

    rc = zmq_close(master);
    assert(0 == rc);

    return NULL;
}

int main (int argc, char ** argv) {

  if (argc != 3) {
      fprintf(stderr, "Usage: %s loop_nb sender_nb\n", argv[0]);
      exit(1);
  }

  LOOP_NB = strtoul(argv[1], NULL, 10);
  SENDER_NB = strtoul(argv[2], NULL, 10);

  CONTEXT = zmq_ctx_new();
  assert(NULL != CONTEXT);

  pthread_t receiver;
  int rc = pthread_create(&receiver, NULL, &thread_receiver, NULL);
  assert(0 == rc);

  pthread_t sender[SENDER_NB];
  for (size_t i = 0; i < SENDER_NB; i++) {
      pthread_t thread;
      rc = pthread_create(&thread, NULL, &thread_sender, (void*) i);
      sender[i] = thread;
      assert(0 == rc);
  }
  for (size_t i = 0; i < SENDER_NB; i++) {
      pthread_join(sender[i], NULL);
  }
  size_t result = 0;
  pthread_join(receiver, (void**) &result);
  printf("Thread joined: %zu messages received\n", result);


  rc = zmq_ctx_destroy(CONTEXT);
  assert(0 == rc);

  return 0;
}
