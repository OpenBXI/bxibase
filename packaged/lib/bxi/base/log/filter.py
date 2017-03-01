#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
@file filters.py bxilog filtering utilities
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.filters bxilog filtering utilities

"""
from __future__ import print_function
import collections

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog


# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


class Filter(bxibase.Wrapper):
    """
    Wraps the ::bxilog_filter_p object
    """
    def __init__(self, filter_p):
        super(Filter, self).__init__(filter_p)

    def __str__(self):
        return self.prefix + ':' + bxilog.LEVEL_NAMES[self.level]


class Filters(bxibase.SequenceSliceImplMixin,
              collections.Sequence,
              collections.Iterable):
    """
    Wraps the ::bxilog_filters_p object
    """
    def __init__(self, filters_p):
        super(Filters, self).__init__()
        self._cstruct = filters_p
        self._wrapped = [None] * self._cstruct.nb

    def __str__(self):
        return ",".join(str(f) for f in self)

    def __len__(self):
        return self._cstruct.nb

    def __wrap__(self, index):
        return Filter(self._cstruct.list[index])

    def __raw_getitem__(self, index):
        if self._cstruct == __FFI__.NULL or self._cstruct[index] == __FFI__.NULL:
            return None
        if self._wrapped[index] is None:
            self._wrapped[index] = self.__wrap__(index)

        return self._wrapped[index]


def parse_filters(filter_str):
    """
    Parse the given string and return the corresponding set of filters as
    a bxilog_filter_p

    @param[in] filters_str a string representing filters as
                           defined in ::bxilog_filters_parse()

    @return an instance of the Filters class
    """
    filters_p = __FFI__.new('bxilog_filters_p[1]')
    err = __BXIBASE_CAPI__.bxilog_filters_parse(filter_str, filters_p)
    bxierr.BXICError.raise_if_ko(err)
    return Filters(filters_p[0])


def new_detailed_filters(filters, delta=2, min_level=bxilog.ERROR):
    """
    Create a copy of the given filters with a level increase of delta.
    """
    result_p = __FFI__.new('bxilog_filters_p[1]')
    result_p[0] = __BXIBASE_CAPI__.bxilog_filters_new()
    for filter_ in filters:
        new_level = min(filter_.level + delta, bxilog.LOWEST)
        # At least min_level messages
        new_level = max(new_level, min_level)
        __BXIBASE_CAPI__.bxilog_filters_add(result_p, filter_.prefix, new_level)
    return Filters(result_p[0])


def merge_filters(filters_set):
    """
    Merge the given set of filters.
    """
    c_filters_set = __FFI__.new('bxilog_filters_p[%d]' % len(filters_set))
    for i in xrange(len(filters_set)):
        c_filters_set[i] = filters_set[i]._cstruct  # pylint: disable=protected-access
    c_result = __BXIBASE_CAPI__.bxilog_filters_merge(c_filters_set, len(filters_set))
    return Filters(c_result)
