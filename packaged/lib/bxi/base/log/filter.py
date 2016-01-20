#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
"""
@file filters.py bxilog filtering utilities
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.filters bxilog filtering utilities

"""

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def parse_filters(filter_str):
    """
    Parse the given string and return the corresponding set of filters as
    a bxilog_filter_p

    @param[in] filters_str a string representing filters as 
                           defined in ::bxilog_filters_parse()

    @return a ::bxilog_filter_p object
    """
    filters_p = __FFI__.new('bxilog_filters_p[1]')
    err = __BXIBASE_CAPI__.bxilog_filters_parse(filter_str, filters_p)
    bxierr.BXICError.raise_if_ko(err)
    return filters_p[0]


def new_detailed_filters(filters, delta=2, min_level=bxilog.ERROR):
    """
    Create a copy of the given filters with a level increase of delta.
    """
    result_p = __FFI__.new('bxilog_filters_p[1]')
    result_p[0] = __BXIBASE_CAPI__.bxilog_filters_new()
    for i in xrange(filters.nb):
        new_level = min(filters.list[i].level + delta, bxilog.LOWEST)
        # At least error messages
        new_level = max(new_level, min_level)
        __BXIBASE_CAPI__.bxilog_filters_add(result_p, filters.list[i].prefix,
                                            new_level)
    return result_p[0]


def merge_filters(filters_set):
    """
    Merge the given set of filters.
    """
    prefixes = dict()
    for filters in filters_set:
        if isinstance(filters, str):
            filters
        for i in xrange(filters.nb):
            prefix = __FFI__.string(filters.list[i].prefix)
            level = filters.list[i].level
            if prefix in prefixes:
                prefixes[prefix] = max(level, prefixes[prefix])
            else:
                prefixes[prefix] = level

    result_p = __FFI__.new('bxilog_filters_p[1]')
    result_p[0] = __BXIBASE_CAPI__.bxilog_filters_new()
    for prefix in sorted(prefixes):
        __BXIBASE_CAPI__.bxilog_filters_add(result_p, prefix, prefixes[prefix])

    return result_p[0]
