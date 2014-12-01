#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Sébastien Miquée <sebastien.miquee@bull.net>
###############################################################################
# Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
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

import bxilog


AGAIN = multiprocessing.Value('b', True, lock=False)
FILENAME = multiprocessing.Value(ctypes.c_char_p, lock=False)

def logging_thread():

    while AGAIN.value:
        bxilog.debug("Logging something...");

if __name__ == "__main__":

    if len(sys.argv) != 3:
        print("Usage: %s threads_nb timeout" % sys.argv[0])
        exit(1)

    FILENAME.value = "/tmp/%s.log" % sys.argv[0]
    print("Output file is %s" % FILENAME.value)

    bxilog.basicConfig(filename=FILENAME.value)

    threads_nb = int(sys.argv[1])
    timeout = int(sys.argv[2])

    threads = []
    for i in xrange(threads_nb):
        thread = multiprocessing.Process(target=logging_thread)
        thread.start()
        threads.append(thread)

    time.sleep(timeout)

    AGAIN.value = False;

    for i in xrange(threads_nb):
        threads[i].join()
