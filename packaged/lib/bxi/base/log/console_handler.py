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

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog

import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


""" 
Colors Theme.

Defining new color themes is really very simple.

See ::bxilog_colors_p for details.
"""
COLORS = {'none': __BXIBASE_CAPI__.BXILOG_COLORS_NONE,
          '216_dark': __BXIBASE_CAPI__.BXILOG_COLORS_216_DARK,
          'truecolor_dark': __BXIBASE_CAPI__.BXILOG_COLORS_TC_DARK,
          'truecolor_light': __BXIBASE_CAPI__.BXILOG_COLORS_TC_LIGHT,
          'truecolor_darkgray': __BXIBASE_CAPI__.BXILOG_COLORS_TC_DARKGRAY,
          'truecolor_lightgray': __BXIBASE_CAPI__.BXILOG_COLORS_TC_LIGHTGRAY,
          }


def add_handler(configobj, section_name, c_config):
    """
    Add a console handler configured from the given section in configobj to the c_config

    @param[in] configobj the configobj (a dict) representing the whole configuration
    @param[in] section_name the section name in the configobj that must be used
    @param[inout] c_config the bxilog configuration where the handler must be added to 
    """

    section = configobj[section_name]
    filters_str = section['filters']
    stderr_level = bxilog.get_level_from_str(section.get('stderr_level', 'WARNING'))
    colors = COLORS[section.get('colors', '216_dark')]

    filters = bxilogfilter.parse_filters(filters_str)
    __BXIBASE_CAPI__.bxilog_config_add_handler(c_config,
                                               __BXIBASE_CAPI__.BXILOG_CONSOLE_HANDLER,
                                               filters,
                                               __FFI__.cast('int', stderr_level),
                                               colors)
