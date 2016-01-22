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
import importlib

import bxi.ffi as bxiffi
import bxi.base as bxibase

import bxi.base.log as bxilog
import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def add_handler(configobj, section_name, c_config):
    """
    Add a bxilog handler to the given configuration.

    @param[in] configobj the configobj (a dict) representing the whole configuration
    @param[in] section the section in the configobj that must be used
    @param[inout] c_config the bxilog configuration where the handler must be added to 
    """
    section = configobj[section_name]
    module_name = section['module']
    module = importlib.import_module(module_name)
    module.add_handler(configobj, section_name, c_config)


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