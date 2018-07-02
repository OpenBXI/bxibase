#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Pierre Vignéras <pierre.vigneras@atos.net>
###############################################################################
# Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################
"""Unit tests of BXI Log Python library.
"""

from __future__ import print_function

import multiprocessing
import os
import sys
import tempfile
import time
import ctypes

import bxi.base.log as bxilog


AGAIN = multiprocessing.Value('b', True, lock=False)
FILENAME = multiprocessing.Value(ctypes.c_char_p, lock=False)
TOTAL = None
N = None
MIN = None
MAX = None


def logging_thread(id):

    n = 0
    total = 0
    min_ = 1.0
    max_ = 0.0
    while AGAIN.value:
        start = time.clock()
        bxilog.debug("Logging something...")
        duration = time.clock() - start
        n += 1
        total += duration
        min_ = min((min_, duration))
        max_ = max((max_, duration))

    global TOTAL, N, MIN, MAX
    TOTAL[id] = total
    N[id] = n
    MIN[id] = min_
    MAX[id] = max_


if __name__ == "__main__":

    if len(sys.argv) != 3:
        print("Usage: %s threads_nb timeout" % sys.argv[0])
        exit(1)

    FILENAME.value = "/tmp/%s.log" % sys.argv[0]
#    print("Output file is %s" % FILENAME.value)

    bxilog.basicConfig(filename=FILENAME.value, append=True, level=bxilog.DEBUG)

    threads_nb = int(sys.argv[1])
    timeout = int(sys.argv[2])

    TOTAL = multiprocessing.Array('d', threads_nb, lock=False)
    N = multiprocessing.Array('I', threads_nb, lock=False)
    MIN = multiprocessing.Array('d', threads_nb, lock=False)
    MAX = multiprocessing.Array('d', threads_nb, lock=False)

    threads = []
    for i in xrange(threads_nb):
        thread = multiprocessing.Process(target=logging_thread, args=(i,))
        thread.start()
        threads.append(thread)

    time.sleep(timeout)

    AGAIN.value = False

    for i in xrange(threads_nb):
        threads[i].join()

    total = 0
    n = 0
    min_ = min(MIN)
    max_ = max(MAX)
    for i in xrange(threads_nb):
        total += TOTAL[i]
        n += N[i]

    print("Average: %s us, min: %s us, max: %s us" % (total * 1e6 / n, min_ * 1e6, max_ * 1e6))
