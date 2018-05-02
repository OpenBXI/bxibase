#!/usr/bin/env python

from multiprocessing import Process
import multiprocessing
import random
import sys
import time
import ctypes


MODULES = ['bxilog', 'logging']


def worker(id, again, logging):
    while again.value:
        t = time.time()
        msg = "worker '%s', time: %s" % (id, t)
        logging.debug(msg)


def startWorkers(numWorkers, again, logging):
    workers = []
    for i in range(numWorkers):
        id = "process %02i" % i
        w = Process(target=worker, args=(id, again, logging))
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
    # log on /dev/null
    logging.basicConfig(level=logging.DEBUG, filename='/dev/null')
    again = multiprocessing.Value(ctypes.c_bool, True, lock=False)
    workers = []
    workers = startWorkers(4, again, logging)
    logging.debug("workers created: %s" % workers)
    waitingLoop()

    runningFlags = [w.is_alive() for id, w in workers]
    logging.debug("terminating workers, current status: %s" % runningFlags)

    again.value = False
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
