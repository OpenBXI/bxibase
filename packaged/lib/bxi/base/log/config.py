#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
"""
@file config.py bxilog configuration utilities
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.config bxilog configuration utilities

"""

import os
import sys

import bxi.ffi as bxiffi
import bxi.base as bxibase

import bxi.base.log as bxilog
import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def add_console_config(console_handler_args, config):
    """
    Add a console handler to the given configuration

    @param[in] console_handler_args console handler arguments (see ::BXILOG_CONSOLE_HANDLER)
    @param[inout] config the configuration where the handler must be added to

    @return the console filters used.
    """
    console_filters = bxilogfilter.parse_filters(console_handler_args[0])
    stderr_level = __FFI__.cast('int', console_handler_args[1])
    colors = console_handler_args[2]
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_CONSOLE_HANDLER,
                                               console_filters,
                                               stderr_level, colors)
    return console_filters


def add_file_config(file_handler_args, config, console_filters):
    """
    Add a file handler to the given configuration according tho the given console filters

    @param[in] file_handler_args file handler arguments (see ::BXILOG_FILE_HANDLER)
    @param[inout] the configuration where the handler must be added to
    @param[in] the console_filters used (can be None)

    If console_filters is not None, the file filters is defined by a call to 
    ::bxi.base.log.filter.new_detailed_filters(console_filters). 
    Otherwise, ::BXILOG_FILTERS_ALL_ALL is used.
    """
    if file_handler_args[0] == bxilog.FILE_HANDLER_FILTERS_AUTO:
        # Compute file filters automatically according to console handler filters
        if console_filters is not None:
            file_filters = bxilogfilter.new_detailed_filters(console_filters)
        else:
            file_filters = __BXIBASE_CAPI__.BXILOG_FILTERS_ALL_ALL
    else:
        file_filters = bxilogfilter.parse_filters(file_handler_args[0])
    progname = __FFI__.new('char[]', sys.argv[0])
    filename = __FFI__.new('char[]', file_handler_args[1])
    open_flags = __FFI__.cast('int',
                              os.O_CREAT |
                              (os.O_APPEND if file_handler_args[2] else os.O_TRUNC))
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_FILE_HANDLER,
                                               file_filters,
                                               progname,
                                               filename,
                                               open_flags)
    return file_filters


def add_syslog_config(syslog_handler_args, config):
    """
    Add a syslog handler to the given configuration.

    @param[in] syslog_handler_args syslog handler arguments (see ::BXILOG_SYSLOG_HANDLER)
    @param[inout] config the configuration where the handler must be added to
    """
    syslog_filters = syslog_handler_args[0]
    if isinstance(syslog_filters, str):
        syslog_filters = bxilogfilter.parse_filters(syslog_filters)
    ident = __FFI__.new('char[]', sys.argv[0])
    option = __FFI__.cast('int', syslog_handler_args[1])
    facility = __FFI__.cast('int', syslog_handler_args[2])
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_SYSLOG_HANDLER,
                                               syslog_filters,
                                               ident, option, facility)
    return syslog_filters


def add_null_handler(config):
    """
    Add a null handler to the given configuration.

    @param[inout] config the configuration where the handler must be added to
    """

    null_filters = __BXIBASE_CAPI__.BXILOG_FILTERS_ALL_OFF
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_NULL_HANDLER,
                                               __BXIBASE_CAPI__.BXILOG_FILTERS_ALL_OFF)
    return null_filters