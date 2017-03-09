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
from __future__ import print_function

"""Unit tests of BXI Log Python library.
"""

import os
import sys
import tempfile
import threading
import time

import bxi.base.log as bxilog

AGAIN = True

def logging_thread():
    while AGAIN:
        bxilog.debug("Logging something...");

if __name__ == "__main__":

    if len(sys.argv) != 3:
        print("Usage: %s threads_nb timeout" % sys.argv[0])
        exit(1)

    filename = "/tmp/%s.log" % sys.argv[0]
    bxilog.basicConfig(filename=filename, append=True, level=bxilog.DEBUG)
    #print("Output file is %s" % filename)

    threads_nb = int(sys.argv[1])
    timeout = int(sys.argv[2])

    bxilog.output("Launching %d threads", threads_nb)
    threads = []
    for i in xrange(threads_nb):
        thread = threading.Thread(target=logging_thread)
        thread.start()
        threads.append(thread)

    time.sleep(timeout)

    AGAIN = False;

    for i in xrange(threads_nb):
        threads[i].join()
