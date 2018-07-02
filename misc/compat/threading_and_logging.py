#!/usr/bin/env python

import threading
import sys
import time
import random

MODULES = ['bxilog', 'logging']


class Worker(threading.Thread):
    def __init__(self, id, again, logging, iterations=None):
        threading.Thread.__init__(self)
        self.id = id
        self.iterations = iterations
        self.logging = logging
        self.again = again
        self._stopFlag = False

    def run(self):
        logging = self.logging
        counter = self.iterations
        while not self._stopFlag and self.again:
            t = time.time()
            msg = "worker '%s', time: %s, again: %s" % (self.id, t, self.again)
            logging.debug(msg)
            if self.iterations:
                counter -= 1
                if not counter:
                    break

    def terminate(self):
        self._stopFlag = True


def startWorkers(numWorkers, again, logging, iterations=None):
    workers = []
    for i in range(numWorkers):
        id = "thread %02i" % i
        w = Worker(id, again, logging, iterations=iterations)
        w.start()
        workers.append((id, w))
    return workers


def waitingLoop():
    try:
        while True:
            line = raw_input()
            print "read: %s" % line
            if line == "stop":
                break
    except KeyboardInterrupt:
        print "keyboard interrupt"


def main(logging):
    print("Write stop to stop the whole process")
    # log on the console
    logging.basicConfig(level=logging.DEBUG, filename='/dev/null')
    again = True
    workers = []
    workers = startWorkers(4, again, logging)
    logging.debug("workers created: %s" % workers)
    waitingLoop()
    again = False
    runningFlags = [w.is_alive() for id, w in workers]
    logging.debug("terminating workers, current status: %s" % runningFlags)

    for id, w in workers:
        w.join(0.5)
        if w.is_alive():
            w.terminate()

    logging.info("testing workers running")
    for id, w in workers:
        logging.info("worker '%s' running: %s" % (id, w.is_alive()))


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in MODULES:
        print >> sys.stderr, "Usage: %s [%s]" % (sys.argv[0], "|".join(MODULES))
        sys.exit(1)

    if sys.argv[1] == 'bxilog':
        import bxi.base.log as logging
    elif sys.argv[1] == 'logging':
        import logging
    main(logging)
