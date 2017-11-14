#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Pierre Vignéras <pierre.vigneras@bull.net>
###############################################################################
# Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################
import os
import unittest

import configobj
import sys

import bxi.base.log as bxilog
import bxi.base.log.remote_receiver as remote_receiver
import sys
import tempfile
import subprocess
import time


"""
Unit tests for bxilog remote handler
"""


BASENAME = os.path.basename(__file__)
FILENAME = "%s.bxilog" % os.path.splitext(BASENAME)[0]

LOGGER_CMD = 'remote_logger.py'

class BXIRemoteLoggerTest(unittest.TestCase):
    """
    Unit tests for the bxilog remote handler
    """
    def tearDown(self):
        """Cleaning up for each test.
        """
        bxilog.cleanup()
    
    def test_remote_logging_bind_simple(self):
        """
        Process Parent receives logs from child process
        """
        # Configure the log in the parent so that all logs received from the child
        # goes to a dedicated file from which we can count the number of messages
        # produced by the child
        tmpdir = tempfile.mkdtemp(suffix="tmp",
                                  prefix=BXIRemoteLoggerTest.__name__)
        all = os.path.join(tmpdir, 'all.bxilog')
        child = os.path.join(tmpdir, 'child.bxilog')
        parent_config = {'handlers': ['all', 'child'],
                        'all': {'module': 'bxi.base.log.file_handler',
                                'filters': ':lowest',
                                'path': all,
                                'append': True,
                               },
                        'child': {'module': 'bxi.base.log.file_handler',
                                'filters': ':off,%s:lowest' % LOGGER_CMD,
                                'path': child,
                                'append': True,
                               }
                        }
        bxilog.set_config(configobj.ConfigObj(parent_config))
        print("Logging output: all: %s, child: %s" % (all, child))
        url = 'ipc://%s/rh-cfg.zock' % tmpdir
        logs_nb = 25
        full_cmd_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     LOGGER_CMD)
        logger_output_file = os.path.join(tmpdir,
                                          os.path.splitext(LOGGER_CMD)[0] + '.bxilog')

        args = [sys.executable, full_cmd_path, logger_output_file, url, 'False', '1', str(logs_nb)]
        bxilog.out("Executing '%s': it must produce %d logs", ' '.join(args), logs_nb)
        popen = subprocess.Popen(args)
        bxilog.out("Starting logs reception thread on %s", url)
        receiver = remote_receiver.RemoteReceiver([url], bind=True)
        receiver.start()
        bxilog.out("Waiting for the child termination")
        popen.wait()
        rc = popen.returncode
        bxilog.out("Child exited with return code %s", rc)
        self.assertEquals(rc, logs_nb)
        # Wait a bit for the logs to be processed
        time.sleep(1)
        bxilog.out("Stopping the receiver")
        receiver.stop(True)
        bxilog.out("Flushing bxilog")
        bxilog.flush()
        with open(child) as file_:
            lines = file_.readlines()
        self.assertEquals(len(lines), logs_nb)

    def test_remote_logging_connect(self):
        pass
    

###############################################################################

if __name__ == "__main__":
    unittest.main()
