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
import ctypes
"""Unit tests of BXI Log Python library.
"""

import __main__
import multiprocessing
import os, time, signal
import subprocess
import sys
import tempfile
import threading
import unittest

from bxi.base.err import BXICError
import bxi.base.log as bxilog
 
 
BASENAME = os.path.basename(__main__.__file__)
FILENAME = "%s.bxilog" % os.path.splitext(BASENAME)[0]
 
__LOOP_AGAIN__ = True
 
def do_some_logs_threading():
    global __LOOP_AGAIN__
    while __LOOP_AGAIN__:
        bxilog.out("Doing a simple log: %s", __LOOP_AGAIN__)
        time.sleep(0.1)
    bxilog.out("Termination requested. Exiting.")
 
def do_some_logs_multiprocessing(again):
    bxilog.cleanup()
    bxilog.basicConfig(console=None,
                       filename=FILENAME,
                       setsighandler=True,
                       filters=':lowest')
    while again.value:
        bxilog.out("Doing a simple log: %s", again)
        time.sleep(0.1)
    bxilog.out("Termination requested. Exiting.")
     
def threads_in_process(again):
    global __LOOP_AGAIN__
    __LOOP_AGAIN__ = True
    bxilog.cleanup()
    bxilog.basicConfig(console=None,
                       filename=FILENAME,
                       setsighandler=True,
                       filters=':lowest')
    threads = []
    for i in xrange(multiprocessing.cpu_count()):
        thread = threading.Thread(target=do_some_logs_threading)
        bxilog.out("Starting new thread")
        thread.start()
        threads.append(thread)
 
    while again.value:
        time.sleep(0.05)
 
    bxilog.out("Requesting termination of %s threads", len(threads))
    __LOOP_AGAIN__ = False
    for thread in threads:
        try:
            thread.join(5)
        except Error as e:
            bxilog.out("Exception: %s", e)
 
 
class BXILogTest(unittest.TestCase):
    """Unit tests for the BXILog
    """
 
    def _check_log_produced(self, file, log_f, *args, **kwargs):
        old_size = os.stat(file).st_size if os.path.exists(file) else 0
        log_f(*args, **kwargs)
        bxilog.flush()
        new_size = os.stat(file).st_size
        self.assertTrue(new_size > old_size, 
                        "%s: new_size=%d > %d=old_size: " % (file, new_size, old_size))
 
    def setUp(self):
        """Definition of the context.
        """
        print("Whole unit test suite file outputs: %s" % FILENAME)
        bxilog.basicConfig(console=None,
                           filename=FILENAME,
                           setsighandler=True,
                           filters=':lowest')
 
    def tearDown(self):
        """Cleaning up for each test.
        """
        bxilog.cleanup()
 
    def test_level_parser(self):
        """
        Test bxilog_level parsing function"
        """
        level_names = list(bxilog.get_all_level_names_iter())
        for i in xrange(len(level_names)):
            level = bxilog.get_level_from_str(level_names[i])
            self.assertEquals(i, level)
    
    def test_default_logger(self):
        """Test default logging functions"""
        for level in bxilog.get_all_level_names_iter():
            self._check_log_produced(FILENAME, 
                                     getattr(bxilog, level), "Some stuff with noarg")
            self._check_log_produced(FILENAME, 
                                     getattr(bxilog, level), "Some stuff with args: %d %f %s", 2, 3.14, 'a string')
            self._check_log_produced(FILENAME,
                                     getattr(bxilog, level), "Some stuff with no args but special characters: %d %f %s")
             
        self._check_log_produced(FILENAME, 
                                 bxilog.exception,
                                 "When no exception was raised - inner message: '%s'",
                                "Who care?", level=bxilog.WARNING)
        self._check_log_produced(FILENAME, 
                                 bxilog.exception,
                                 "When no exception was raised - special char"\
                                 "but no args: '%s'",
                                 level=bxilog.DEBUG)
        try:
            raise ValueError("Exception raised: inner message: '%s'" % "And so what?")
        except ValueError as ve:
            self._check_log_produced(FILENAME, 
                                     bxilog.exception,
                                     "Exception catched: inner message: '%s'",
                                     "Kilroy was here",
                                     level=bxilog.CRITICAL)
         
        try:
            raise ValueError("Exception raised: inner message: '%s'" % "And so what again?")
        except ValueError as ve:
            self._check_log_produced(FILENAME, 
                                     bxilog.exception,
                                     "Exception catched - special char but no args: '%s'",
                                     level=bxilog.CRITICAL)
         
 
    def test_basic_log(self):
        """Test normal logging"""
        def _produce_logs(logger, level):
            self._check_log_produced(FILENAME,
                                     getattr(logger, level), "Some stuff with noarg")
            self._check_log_produced(FILENAME,
                                     getattr(logger, level), "Some stuff with args: %d %f %s", 2, 3.14, 'a string')
            self._check_log_produced(FILENAME,
                                     getattr(logger, level), "Some stuff with no args but special characters: %d %f %s")
 
        logger1 = bxilog.getLogger("foo")
        for level in bxilog.get_all_level_names_iter():
            _produce_logs(logger1, level)
 
        logger2 = bxilog.getLogger("bar")
        for level in bxilog.get_all_level_names_iter():
            _produce_logs(logger2, level)
 
        logger3 = bxilog.getLogger("foo.bar")
        for level in bxilog.get_all_level_names_iter():
            _produce_logs(logger3, level)
         
        self._check_log_produced(FILENAME, 
                                 logger1.exception,
                                 "When no exception was raised - inner message: '%s'",
                                 "Who care?",
                                 level=bxilog.ERROR)
         
        self._check_log_produced(FILENAME, 
                                 logger1.exception,
                                 "When no exception was raised - special char but no args: '%s'",
                                 level=bxilog.ERROR)
             
        try:
            raise ValueError("Exception raised: inner message: '%s'" % "And so what?")
        except ValueError as ve:
            self._check_log_produced(FILENAME, 
                                     logger1.exception,
                                     "Exception catched: inner message: '%s'",
                                     "Kilroy was here",
                                     level=bxilog.ERROR)
             
        try:
            raise ValueError("Exception raised: inner message: '%s'" % "And so what again?")
        except ValueError as ve:
            self._check_log_produced(FILENAME, 
                                     logger1.exception,
                                     "Exception catched - special char but no args: '%s'",
                                     level=bxilog.CRITICAL)
         
 
 
    def test_error_log(self):
        """Test error while logging"""
        logger = bxilog.getLogger("foo")
        self.assertRaises(TypeError,
                          logger.output, "Testing not enough args: %d %d %s", 1)
        self.assertRaises(TypeError,
                          logger.output,
                           "Testing too many args: %d %d %s", 1, 2, 3, 'toto', 'tata')
        self.assertRaises(TypeError,
                          logger.output,
                           "Testing wrong types: %d %d %s", 'foo', 2.5, 3, 'toto')
 
    def test_existing_file(self):
        """Test logging into an existing file"""
        fd, name = tempfile.mkstemp(".bxilog", "test_")
 
        print("Overriding file output to "
              "%s for %s.%s()" % (name, __name__,
                                  BXILogTest.test_existing_file.__name__))
        self.assertEquals(os.stat(name).st_size, 0)
        os.close(fd)
 
        bxilog.basicConfig(console=None, filename=name)
 
        self._check_log_produced(name, bxilog.output, "One log on file: %s", name)
        os.remove(name)
        bxilog.cleanup()
 
    def test_non_existing_file(self):
        """Test logging into a non existing file"""
        fd, name = tempfile.mkstemp(".bxilog", "test_")
        print("Overriding file output to "
              "%s for %s.%s()" % (name, __name__,
                                  BXILogTest.test_non_existing_file.__name__))
        self.assertEquals(os.stat(name).st_size, 0)
        os.close(fd)
        os.remove(name)
 
        bxilog.basicConfig(console=None, filename=name)
 
        self._check_log_produced(name, bxilog.output,
                                 "One log on non-existent (deleted) file: %s", name)
        bxilog.cleanup()
 
    def test_non_existing_dir(self):
        """Test logging into a non existing tmpdir - this should raise an error"""
        tmpdir = tempfile.mkdtemp(".bxilog", "test_")
        os.rmdir(tmpdir)
        name = os.path.join(tmpdir, 'dummy.bxilog')
        bxilog.basicConfig(console=None, filename=name)
 
        self.assertRaises(BXICError, bxilog.output,
                          "One log on non-existent (deleted) directory: %s", name)
 
        bxilog.cleanup()
 
        self.assertFalse(os.path.exists(name))
 
    def test_config_file(self):
        """Test logging with configuration items"""
 
        orig_size = os.stat(FILENAME).st_size if os.path.exists(FILENAME) else 0
 
        bxilog.basicConfig(console=None, 
                           filename=FILENAME,
                           filters=':output,foo:debug,foo.bar:trace')
         
        foo = bxilog.getLogger('foo')
        bar = bxilog.getLogger('foo.bar')
        any = bxilog.getLogger('bebopalula')
         
        any.info('This message must not appear in file %s', FILENAME)
        bxilog.flush()
        newsize = os.stat(FILENAME).st_size
        self.assertEquals(newsize, orig_size)
        bar.lowest('This message must also not appear in file %s', FILENAME)
        bxilog.flush()
        newsize = os.stat(FILENAME).st_size
        self.assertEquals(newsize, orig_size)
        foo.fine('This message must not appear in file %s', FILENAME)
        bxilog.flush()
        newsize = os.stat(FILENAME).st_size
        self.assertEquals(newsize, orig_size)
         
        self._check_log_produced(FILENAME, any.output,
                                 'This message must appear in file %s', FILENAME)
        self._check_log_produced(FILENAME, bar.trace,
                                 'This message must also appear in file %s', FILENAME)
        self._check_log_produced(FILENAME, foo.debug,
                                 'This message must also appear in file %s', FILENAME)
        self._check_log_produced(FILENAME, bxilog.output,
                                 "This message must also appear in file %s", FILENAME)
 
 
 
    def test_threading(self):
        threads = []
        for i in xrange(multiprocessing.cpu_count() * 2):
            thread = threading.Thread(target=do_some_logs_threading)
            bxilog.out("Starting new thread")
            thread.start()
            threads.append(thread)
        bxilog.out("Sleeping")
        time.sleep(0.5)
        bxilog.out("Requesting termination of %s threads", len(threads))
        global __LOOP_AGAIN__
        __LOOP_AGAIN__ = False
        for thread in threads:
            try:
                thread.join(5)
            except Error as e:
                bxilog.out("Exception: %s", e)
            self.assertFalse(thread.is_alive())
 
 
    def test_multiprocessing(self):
        processes = []
        again = multiprocessing.Value(ctypes.c_bool, True, lock=False)
        for i in xrange(multiprocessing.cpu_count() * 2):
            process = multiprocessing.Process(target=do_some_logs_multiprocessing,
                                              args=(again,))
            bxilog.out("Starting new process")
            process.start()
            processes.append(process)
        bxilog.out("Sleeping")
        time.sleep(0.5)
        bxilog.out("Requesting termination of %s processes", len(processes))
        again.value = False
        for process in processes:
            try:
                process.join(5)
            except Error as e:
                bxilog.out("Exception: %s", e)
            self.assertFalse(process.is_alive())
 
 
    def test_threads_and_forks(self):
        processes = []
        again = multiprocessing.Value(ctypes.c_bool, True, lock=False)
        for i in xrange(multiprocessing.cpu_count()):
            process = multiprocessing.Process(target=threads_in_process,
                                              args=(again,))
            bxilog.out("Starting new process")
            process.start()
            processes.append(process)
        bxilog.out("Sleeping")
        time.sleep(0.5)
        bxilog.out("Requesting termination of %s processes", len(processes))
        again.value = False
        for process in processes:
            try:
                process.join(5)
            except Error as e:
                bxilog.out("Exception: %s", e)
            self.assertFalse(process.is_alive())
 
 
    def test_sighandler(self):
        """Unit test for mantis#19501"""
        fnull = open(os.devnull, 'w')
        exe = os.path.join(os.path.dirname(__file__), "simple_bxilog_user.py")
        p = subprocess.Popen([exe], stderr=fnull)
        time.sleep(0.5)
        rc = p.poll()
        self.assertIsNone(rc, None)
        p.send_signal(signal.SIGTERM)
        time.sleep(0.5)
        rc = p.wait()
        self.assertEquals(rc, -signal.SIGTERM)

###############################################################################
 
if __name__ == "__main__":
    unittest.main()
