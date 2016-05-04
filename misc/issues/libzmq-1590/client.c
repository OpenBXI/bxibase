// Hello World client
#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int
main (int argc, char ** argv)
{
  printf ("Connecting to hello world server…\n");
  void *context = zmq_ctx_new ();
  void *requester = zmq_socket (context, ZMQ_REQ);
  int i;
  for (i = 1; i < argc; i++) {
        zmq_connect (requester, argv[i]);
  }

  int request_nbr;
  for (request_nbr = 0; request_nbr != 1000; request_nbr++)
    {
      char buffer[100];
      memset(buffer, 0, 100);
      printf ("Sending Hello %d…\n", request_nbr);
      zmq_send (requester, "Hello", 5, 0);
      zmq_recv (requester, buffer, 100, 0);
      printf ("Received World %d: %s\n", request_nbr, buffer);
      if (request_nbr == 10) {
        printf("Disconnecting from %s\n", argv[2]);
        zmq_disconnect(requester, argv[2]);
      }
    }
  zmq_close (requester);
  zmq_ctx_destroy (context);
  return 0;
}
