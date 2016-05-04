// Hello World server

#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int
main (int argc, char** argv)
{
// Socket to talk to clients
  void *context = zmq_ctx_new ();
  void *responder = zmq_socket (context, ZMQ_REP);
  int rc = zmq_bind (responder, argv[1]);
  assert (rc == 0);

  while (1)
    {
      char buffer[10];
      zmq_recv (responder, buffer, 10, 0);
      printf ("Received Hello %s\n", argv[1]);
      sleep (1);                // Do some 'work'
      zmq_send (responder, argv[1], strlen(argv[1]), 0);
    }
  return 0;
}
