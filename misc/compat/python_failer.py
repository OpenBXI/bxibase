#!/usr/bin/env python

import sys

import multiprocessing
import ctypes
from threading_and_logging import startWorkers as startThreadWorkers
from multiprocessing_and_logging import startWorkers as startProcessWorkers
from threading_and_logging import waitingLoop

MODULES = ['logging', 'bxilog']


def main(logging):
    print("Write stop to stop the whole process")

    # logging.basicConfig(level=logging.DEBUG, filename='/dev/null')
    logging.basicConfig(level=logging.DEBUG)
    again = multiprocessing.Value(ctypes.c_bool, True, lock=False)
    # (1)
    # if multiprocessing.Process are created first, before threads, then both
    # threads as well as processes write logging all right.

    # (2)
    # if multiprocessing.Process is created & started second, it will never
    # print anything and sleep (S) all the time, even after the threads are
    # terminated, processes impossible to terminate gracefully, need to kill

    workers = []
    workers.extend(startThreadWorkers(2, again, logging, iterations=10000))
    #    iterations - make the threads stop after some time - processes
    #    in the case (2) will never resume anyway
    workers.extend(startProcessWorkers(2, again, logging))  # problem
    # workers.extend(startThreadWorkers(2, iterations = 10000))

    logging.debug("workers created: %s" % workers)
    waitingLoop()
    again.value = False
    runningFlags = [w.is_alive() for id, w in workers]
    logging.debug("terminating workers, current status: %s" % runningFlags)

    for id, w in workers[:2]:
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
