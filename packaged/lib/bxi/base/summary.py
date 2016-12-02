#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
@file summary.py Defines an issues summary class
@authors Jean-Noël Quintin <jean-noel.quintin@atos.net>
@copyright 2016  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.summary Python BXI Issues Summary

This module provides a summary of multiple issues.
The summary is made by dispatching issues on different logger
and ordering those issues.
"""

import bxi.base.log as bxilog
import bxi.base.parserconf as bxiparserconf

DEFAULT_BXI_REPORT_ORDER = 'BXI_REPORT_ORDER'

_LOGGER = bxilog.getLogger(bxilog.LIB_PREFIX + 'bxibasesummary')


def addargs(parser, config=None):
    """
    Configure the arg parser with summary options.

    @param parser of the command line

    @return
    """

    default_value = bxiparserconf.getdefaultvalue(parser, ['Defaults'],
                                                  'reporting_order', _LOGGER,
                                                  'level', config)

    parser.add_argument('--report-order',
                        default=default_value,
                        envvar=DEFAULT_BXI_REPORT_ORDER,
                        choices=['level', 'object'],
                        help='define the issue display order.' +
                        ' Default is taken from %(envvar)s environment variable' +
                        ' if defined, %s otherwise.' % 'level' +
                        ' Default: "%(default)s".')


class Issue(object):
    '''
    Represent an encounter error.
    '''
    def __init__(self,
                 name,
                 msg,
                 details='',):
        '''
        Build an issue.

        @param name of the component in error.
        @param msg describing the error
        @param details about the error
        @return
        '''
        self.name = name
        self.details = details
        self.msg = msg

    def get_name(self):
        '''
        Return the name of the issue

        @return The name associated to the issue
        '''
        return self.name

    def get_error(self):
        """
        Return the error message
        """
        return self.msg


class Issues(object):
    """
    Container of issues to be reported
    """
    def __init__(self, resolution, prefix):
        """
        Build a container of errors.
        @param resolution how the errors are generally fixed.
        @param prefix of the logger use the report the issues.

        @return
        """
        self.logger_prefix = prefix
        self.errors = [[] for _ in bxilog.get_all_loggers_iter()]
        self.resolution = resolution

    def append(self, level, error):
        """
        Store the error to display it on request

        @param level at which the issue is reported
        @param error add to the container
        """
        self.errors[level].append(error)

    def off(self, error):
        """
        Do not store the given message!

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.OFF, error)

    def panic(self, error):
        """
        Store at the ::PANIC level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.PANIC, error)

    def alert(self, error):
        """
        Store at the ::ALERT level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.ALERT, error)

    def critical(self, error):
        """
        Store at the ::CRITICAL level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.CRITICAL, error)

    def error(self, error):
        """
        Store at the ::ERROR level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.ERROR, error)

    def warning(self, error):
        """
        Store at the ::WARNING level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.WARNING, error)

    def notice(self, error):
        """
        Store at the ::NOTICE level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.NOTICE, error)

    def output(self, error):
        """
        Store at the ::OUT level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.OUTPUT, error)

    def info(self, error):
        """
        Store at the ::INFO level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.INFO, error)

    def debug(self, error):
        """
        Store at the ::DEBUG level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.DEBUG, error)

    def fine(self, error):
        """
        Store at the ::FINE level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.FINE, error)

    def trace(self, error):
        """
        Store at the ::TRACE level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.TRACE, error)

    def lowest(self, error):
        """
        Store at the ::LOWEST level the given error.

        @param[in] error the message to log
        @return

        """
        self.append(bxilog.LOWEST, error)

    def fulldisplay(self):
        """
        Display all the errors
        """
        for level in xrange(len(self.errors)):
            self.display(level)

    def display(self, level):
        """
        Display the errors at the provided level
        """
        for issue in self.errors[int(level)]:
            logger = bxilog.getLogger('bxi.report.%s' % self.logger_prefix)
            logger.log(level, "%s\t%s\t%s", issue.name,
                       issue.msg, issue.details)


class Summary(object):
    """
    Handle Reporting of issues
    """
    def __init__(self, order='level'):
        """
        Build a reporter of errors
        @param order in which the issues are reported
        @return
        """
        self.order = order
        self.issues = dict()

    def set_order(self, order):
        """
        Change the order the issues are displayed
        @param order in which the issues are reported
        @return
        """
        self.order = order

    def display(self):
        """
        Display all issues in the current order
        @return
        """
        for key_issues in self.issues:
            for issues in self.issues[key_issues]:
                issues.fulldisplay()

    def add_issues(self, issues):
        """
        Add issues for the report
        @param issues add the container of errors
        @return
        """
        try:
            self.issues[issues.resolution].append(issues)
        except KeyError:
            self.issues[issues.resolution] = []
            self.issues[issues.resolution].append(issues)
