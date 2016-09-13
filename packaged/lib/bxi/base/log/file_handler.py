#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
"""
@file file_handler.py bxilog file handler
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.file_handler bxilog file handler

"""

import os
import sys
import bxi.ffi as bxiffi
import bxi.base.err as bxierr
import bxi.base as bxibase
import bxi.base.log.console_handler as bxilog_consolehandler
import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()

"""
Specifies that file handler filters must be computed automatically.

@see ::basicConfig()
@see ::bxilog_handler_p
@see ::BXILOG_FILE_HANDLER
"""
FILTERS_AUTO = 'auto'
STDOUT = '-'
STDERR = '+'


def add_handler(configobj, section_name, c_config):
    """
    Add a file handler configured from the given section in configobj to the c_config

    @param[in] configobj the configobj (a dict) representing the whole configuration
    @param[in] section_name the section name in the configobj that must be used
    @param[inout] c_config the bxilog configuration where the handler must be added to 
    """
    section = configobj[section_name]
    filters_str = section['filters']
    # Use absolute path to prevent fork() problem with chdir().
    filename = section['file']
    if filename not in [STDOUT, STDERR]:
        filename = os.path.abspath(filename)
    section['file'] = filename
    append = section.as_bool('append')

    if filters_str == FILTERS_AUTO:
        # Compute file filters automatically according to console handler filters
        # Find console filters
        found = None
        for section in configobj:
            if not isinstance(configobj[section], dict):
                continue
            try:
                module = configobj[section]['module']
            except KeyError:
                continue
            if module == bxilog_consolehandler.__name__:
                found = section
                break
        if found is None:
            raise bxierr.BXIError("No console handler found, can't compute the file"
                                  " handler logging filters automatically. "
                                  "Either specify a filter or define a console handler."
                                  "Configuration used: %s" % configobj)

        filters_str = configobj[section]['filters']
        console_filters = bxilogfilter.parse_filters(filters_str)
        file_filters = bxilogfilter.new_detailed_filters(console_filters)
        #__BXIBASE_CAPI__.bxilog_filters_free(console_filters);
    else:
        file_filters = bxilogfilter.parse_filters(filters_str)

    progname = __FFI__.new('char[]', sys.argv[0])
    filename = __FFI__.new('char[]', filename)
    open_flags = __FFI__.cast('int',
                              os.O_CREAT |
                              (os.O_APPEND if append else os.O_TRUNC))
    __BXIBASE_CAPI__.bxilog_config_add_handler(c_config,
                                               __BXIBASE_CAPI__.BXILOG_FILE_HANDLER,
                                               file_filters,
                                               progname,
                                               filename,
                                               open_flags)
#    __BXIBASE_CAPI__.bxilog_filters_free(file_filters);
