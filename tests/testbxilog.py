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


"""Unit tests of BXI Log Python library.
"""

import ctypes
import multiprocessing
import os
import time
import signal
import subprocess
import tempfile
import threading
import unittest
import re
import configobj
import random

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog


BASENAME = os.path.basename(__file__)
FILENAME = "%s.bxilog" % os.path.splitext(BASENAME)[0]

__FFI__ = bxiffi.get_ffi()

__LOOP_AGAIN__ = True


def do_some_logs_threading():
    global __LOOP_AGAIN__
    while __LOOP_AGAIN__:
        bxilog.out("Doing a simple log: %s", __LOOP_AGAIN__)
        time.sleep(0.1)
    bxilog.out("Termination requested. Exiting.")


def do_some_logs_multiprocessing(again):
    bxilog.cleanup()
    bxilog.basicConfig(filename=FILENAME,
                       filemode='a',
                       level=bxilog.LOWEST)
    while again.value:
        bxilog.out("Doing a simple log: %s", again)
        time.sleep(0.1)
    bxilog.out("Termination requested. Exiting.")


def threads_in_process(again):
    global __LOOP_AGAIN__
    __LOOP_AGAIN__ = True
    bxilog.cleanup()
    bxilog.basicConfig(filename=FILENAME,
                       filemode='a',
                       level=bxilog.LOWEST)
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


class TestException(Exception):
    pass


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
        bxilog.basicConfig(filename=FILENAME,
                           filemode='a',
                           level=bxilog.LOWEST)

    def tearDown(self):
        """Cleaning up for each test.
        """
        bxilog.cleanup()

    def test_level_parser(self):
        """
        Test bxilog_level parsing function"
        """
        for i in xrange(len(bxilog.LEVEL_NAMES)):
            level = bxilog.get_level_from_str(bxilog.LEVEL_NAMES[i])
            self.assertEquals(i, level)

    def test_filtering(self):
        # Warning: lexical order is important for the test
        filters = bxilog.filter.parse_filters(':out,~a.bar:debug,~a.foo:info')
        self.assertIsNotNone(filters)
        self.assertEqual(filters[0].prefix, '')
        self.assertEqual(filters[0].level, bxilog.OUTPUT)
        self.assertEqual(filters[1].prefix, '~a.bar')
        self.assertEqual(filters[1].level, bxilog.DEBUG)
        self.assertEqual(filters[2].prefix, '~a.foo')
        self.assertEqual(filters[2].level, bxilog.INFO)
        delta = 2
        detailed_filters = bxilog.filter.new_detailed_filters(filters, delta, bxilog.OUTPUT)
        self.assertEqual(detailed_filters[0].prefix, '')
        self.assertEqual(detailed_filters[0].level, bxilog.OUTPUT + delta)
        self.assertEqual(detailed_filters[1].prefix, '~a.bar')
        self.assertEqual(detailed_filters[1].level, bxilog.DEBUG + delta)
        self.assertEqual(detailed_filters[2].prefix, '~a.foo')
        self.assertEqual(detailed_filters[2].level, bxilog.INFO + delta)
        other_filters = bxilog.filter.parse_filters(':fine,~a.bar:panic,~a.foo:lowest')
        merged_filters = bxilog.filter.merge_filters((filters, detailed_filters, other_filters))
        self.assertEqual(merged_filters[0].prefix, '')
        self.assertEqual(merged_filters[0].level, bxilog.FINE)
        self.assertEqual(merged_filters[1].prefix, '~a.bar')
        self.assertEqual(merged_filters[1].level, bxilog.DEBUG + delta)
        self.assertEqual(merged_filters[2].prefix, '~a.foo')
        self.assertEqual(merged_filters[2].level, bxilog.LOWEST)

    def test_default_logger(self):
        """Test default logging functions"""
        for level in bxilog.LEVEL_NAMES:
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
                                 "When no exception was raised - special char "\
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
        for level in bxilog.LEVEL_NAMES:
            _produce_logs(logger1, level)

        logger2 = bxilog.getLogger("bar")
        for level in bxilog.LEVEL_NAMES:
            _produce_logs(logger2, level)

        logger3 = bxilog.getLogger("foo.bar")
        for level in bxilog.LEVEL_NAMES:
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

    def test_strange_char(self):
        """Test logging with non-printable character, and especially, NULL char"""
        for i in xrange(256):
            bxilog.output("A message with ascii character %d just between "
                          "the two following quotes '%s'", i, chr(i))


    def test_existing_file(self):
        """Test logging into an existing file"""
        fd, name = tempfile.mkstemp(".bxilog", "test_")

        print("Overriding file output to "
              "%s for %s.%s()" % (name, __name__,
                                  BXILogTest.test_existing_file.__name__))
        self.assertEquals(os.stat(name).st_size, 0)
        os.close(fd)

        bxilog.basicConfig(filename=name)

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

        bxilog.basicConfig(filename=name)

        self._check_log_produced(name, bxilog.output,
                                 "One log on non-existent (deleted) file: %s", name)
        bxilog.cleanup()
        os.remove(name)

    def test_non_existing_dir(self):
        """Test logging into a non existing tmpdir - this should raise an error"""
        tmpdir = tempfile.mkdtemp(".bxilog", "test_")
        os.rmdir(tmpdir)
        name = os.path.join(tmpdir, 'dummy.bxilog')
        bxilog.basicConfig(filename=name)

        # Raise an error because filename provided to basicConfig doesn't exist
        self.assertRaises(bxierr.BXICError, bxilog.output,
                          "One log on non-existent (deleted) directory: %s", name)

        bxilog.cleanup()

        self.assertFalse(os.path.exists(name))

    def test_config_file(self):
        """Test logging with configuration items"""

        orig_size = os.stat(FILENAME).st_size if os.path.exists(FILENAME) else 0

        conf = {'handlers': ['out', 'null'],
                'setsighandler': True,
                'out':{'module': 'bxi.base.log.file_handler',
                       'filters': ':output,foo:debug,foo.bar:trace',
                       'path': FILENAME,
                       'append': True},
                'null': {'module': 'bxi.base.log.null_handler',
                         'filters': ':off,foo:fine,foo.bar:debug',
                         },
                }
        bxilog.set_config(configobj.ConfigObj(conf))
        # It is required to initialize the library for the configuration to be used
        bxilog.init()

        foo = bxilog.getLogger('foo')
        self.assertEqual(foo.level, bxilog.FINE)
        bar = bxilog.getLogger('foo.bar')
        self.assertEqual(bar.level, bxilog.TRACE)
        any = bxilog.getLogger('bebopalula')
        self.assertEqual(any.level, bxilog.OUTPUT)

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

    def test_exception(self):
        try:
            raise ValueError("Exception 1 raised for testing purpose, don't worry")
        except ValueError:
            bxilog.exception('Handling exception 1 with no arguments')

        try:
            raise ValueError("Exception 2 raised for testing purpose, don't worry")
        except ValueError as ve:
            bxilog.exception('Handling exception 2 with 1 argument: %s',
                             'argument')

        try:
            raise ValueError("Exception 3 raised for testing purpose, don't worry")
        except ValueError as ve:
            bxilog.exception('Handling exception 3 with 2 argument: %s - %d',
                             'argument', 3)

        try:
            raise ValueError("Exception 4 raised for testing purpose, don't worry")
        except ValueError as ve:
            bxilog.exception('Handling exception 4 with level only',
                             level=bxilog.CRITICAL)
        try:
            raise ValueError("Exception 5 raised for testing purpose, don't worry")
        except ValueError as ve:
            bxilog.exception('Handling exception 5 with 1 argument and the level: %s',
                             'argument', level=bxilog.CRITICAL)
        try:
            raise ValueError("Exception 6 raised for testing purpose, don't worry")
        except ValueError as ve:
            bxilog.exception('Handling exception 6 with 2 argument and the level: %s - %d',
                             'argument', 3, level=bxilog.CRITICAL)

        try:
            raise TestException("Custom made exception, don't worry")
        except TestException:
            bxilog.exception('Custom made exception thrown, good!')

        try:
            te = TestException("Another custom made exception, don't worry")
            te.cause = TestException("Cause of the exception")
            te.cause.cause = TestException("Root cause of the exception")
            raise te
        except TestException:
            bxilog.exception('Custom made exception with causes thrown, good!')

        try:
            root = bxibase.__CAPI__.bxierr_new(101, bxiffi.__FFI__.NULL, __FFI__.NULL,
                                               __FFI__.NULL, __FFI__.NULL,
                                               "The root cause")
            other = bxibase.__CAPI__.bxierr_new(102, __FFI__.NULL, __FFI__.NULL,
                                               __FFI__.NULL, root,
                                               "Consequence")
            bce = bxierr.BXICError(other)
            be = bxierr.BXIError("Another one", cause=bce)
            te = TestException("An exception, don't worry")
            bxilog.report_bxierr(bce, msg="test report error", level=bxilog.ALERT)
            te.cause = be
            raise te
        except TestException:
            bxilog.exception('Mix of C and Python exceptions, good!')

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

    def test_uncaught(self):
        """Unit test for uncaught exception"""
        exe = os.path.join(os.path.dirname(__file__), "uncaught.py")
        filename = os.path.splitext(os.path.basename(exe))[0] + '.bxilog'
        try:
            bxilog.out("Invoking %s. It must create file: %s", exe, filename)
            subprocess.check_call([exe])
        except subprocess.CalledProcessError as cpe:
            self.assertEqual(cpe.returncode, 1)
        filename = os.path.splitext(os.path.basename(exe))[0] + '.bxilog'
        with open(filename) as logfile:
            found = False
            pattern = '.*Uncaught Exception - exiting thread.*'
            regexp = re.compile(pattern)
            for line in logfile:
                if regexp.match(line):
                    found = True
        self.assertTrue(found, "Pattern %s not found in %s" % (pattern, filename))
        os.unlink(filename)

###############################################################################

if __name__ == "__main__":
    unittest.main()
