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
#define BUF_SIZE 100
#define MAX_RETRIES 3

void * thread_routine(void * context) {

    void *worker = zmq_socket(context, ZMQ_PULL);
    int rc = zmq_bind(worker, URL);
    assert(0 == rc);

    size_t n = 0;
    while(true) {
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
        rc = zmq_msg_close(&mzg);
        assert(0 == rc);
        if (size == 0) break;
        n++;
//        if (0 == (n % 1000)) {
//            printf("Received %zd messages\n", n);
//        }
//        usleep(1);
    }
    printf("Terminated message received\n");
    rc = zmq_close(worker);
    assert(0 == rc);

    return (void *) n;
}

int main (int argc, char ** argv) {

  if (argc != 2) {
      fprintf(stderr, "Usage: %s loop_nb\n", argv[0]);
      exit(1);
  }

  void *context = zmq_ctx_new();
  assert(NULL != context);

  pthread_t thread;
  int rc = pthread_create(&thread, NULL, &thread_routine, context);
  assert(0 == rc);

  void * master = zmq_socket(context, ZMQ_PUSH);
  assert(NULL != master);
  rc = zmq_connect(master, URL);
  assert(0 == rc);

  size_t loop_nb = strtoul(argv[1], NULL, 10);

  for (size_t i = 0; i < loop_nb; i++) {
      zmq_msg_t mzg;
      rc = zmq_msg_init_size(&mzg, BUF_SIZE);
      assert(0 == rc);
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
  }

  zmq_msg_t mzg;
  zmq_msg_init_size(&mzg, 0);
  rc = zmq_msg_send(&mzg, master, 0);

  printf("Terminating message sent\n");
  size_t result;
  pthread_join(thread, (void*) &result);
  printf("Thread joined\n");
  rc = zmq_close(master);
  assert(0 == rc);
  rc = zmq_ctx_destroy(context);
  assert(0 == rc);



  return 0;
}
