#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base Python BXI Base module

"""
# Try to find other BXI packages in other folders
from pkgutil import extend_path
__path__ = extend_path(__path__, __name__)

from bxi.base.cffi_h import C_DEF

import bxi.ffi as bxiffi

import collections  # noqa
from functools import total_ordering  # noqa

bxiffi.add_cdef_for_type('bxilog_p', C_DEF)

__FFI__ = bxiffi.get_ffi()
__CAPI__ = bxiffi.get_ffi().dlopen('libbxibase.so')


def get_capi():
    """
    Return the CFFI wrapped C library.

    @return the CFFI wrapped C library.
    """
    return __CAPI__


class SequenceSliceImplMixin(object):
    """
    Implement the python array access for c array
    """
    def __getitem__(self, val):
        size = len(self)
        if isinstance(val, slice):
            # Get the start, stop, and step from the slice
            return [self.__raw_getitem__(i) for i in xrange(*val.indices(size))]
        elif isinstance(val, int):
            if val < 0:  # Handle negative indices
                val += size
            if val >= size:
                raise IndexError("Index (%d) out of range [0, %d[" % (val, size))
            return self.__raw_getitem__(val)
        else:
            raise TypeError("Invalid argument type: %s, expecting slice or int" % val)


#pylint: disable=R0921
@total_ordering
class Uint8CTuple(SequenceSliceImplMixin, collections.MutableSequence):
    """
    Wrapper for C tuple of uint8_t objects
    """
    def __init__(self, ctab, n):
        super(Uint8CTuple, self).__init__()
        self._ctab = ctab
        self._len = n
        self._buf = __FFI__.buffer(ctab, n)

    def __raw_getitem__(self, index):
        return ord(self._buf[index])

    def __setitem__(self, index, value):
        if index < self._len and index >= 0:
            self._buf[index] = chr(value)
        elif index >= -1 * self._len and index < 0:
            self._buf[self._len + index] = chr(value)
        else:
            raise IndexError("tuple %s index %s out of range(%s)" %
                             (self, index, self._len))

    def __delitem__(self, index):
        raise NotImplementedError("Can't delete from a C array")

    def insert(self, value):
        raise NotImplementedError("Can't insert from a C array")

    def __len__(self):
        return self._len

    def __hash__(self):
        result = 0
        for i in xrange(len(self)):
            result += self[i]
        return result

    def set(self, seq):
        """
        Set the Uint8CTupletuple with the provide tuple.
        @param seq tuple to copy
        @return
        """
        for i in xrange(len(seq)):
            self[i] = seq[i]

    def __str__(self):
        return "[%s]" % ",".join("%02d" % x for x in self)

    def __eq__(self, other):
        if other is None:
            return False

        if len(self) != len(other):
            return False
        for i in xrange(len(other)):
            if self[i] != other[i]:
                return False
        return True

    def __ne__(self, other):
        return not self == other

    def __lt__(self, other):
        for i in xrange(len(other)):
            if self[i] >= other[i]:
                return False
        return True


class Wrapper(object):
    """
    Generic Python Wrapper of  C structure
    """
    # _STRING_TYPES = (__FFI__.typeof("char *"), __FFI__.typeof("char[64]"))
    _CHAR_KINDS = (__FFI__.typeof('char [0]').kind, __FFI__.typeof('char *').kind)
    _CHAR_ITEM_CNAME = __FFI__.typeof('char [0]').item.cname
    _UINT8_TUPLE_KIND = __FFI__.typeof('uint8_t [0]').kind
    _UINT8_TUPLE_ITEM_CNAME = __FFI__.typeof('uint8_t [0]').item.cname

    def __init__(self, cstruct):
        super(Wrapper, self).__init__()
#        self._cstruct = cstruct
#        _LOGGER.lowest("Wrapping: %s", cstruct)
        super(Wrapper, self).__setattr__('_cstruct', cstruct)

    def __getattr__(self, name):
        cstruct = object.__getattribute__(self, '_cstruct')
        field = getattr(cstruct, name)

#        _LOGGER.lowest("Wrapped retrieval: %r.%s -> %s", self, name, field)

        if field == __FFI__.NULL:
            return None

        if isinstance(field, __FFI__.CData):
            type_ = __FFI__.typeof(field)
            if type_.kind in Wrapper._CHAR_KINDS and \
               type_.item.cname == Wrapper._CHAR_ITEM_CNAME:
                return __FFI__.string(field)
            if type_.kind == Wrapper._UINT8_TUPLE_KIND and \
               type_.item.cname == Wrapper._UINT8_TUPLE_ITEM_CNAME:
                return Uint8CTuple(field, type_.length)

        return field

    def __setattr__(self, name, value):
        try:

            cstruct = getattr(self, '_cstruct')
            if isinstance(value, Uint8CTuple):
                setattr(cstruct, name, tuple(value))
            else:
                setattr(cstruct, name, value)
#            Wrapper._LOGGER.lowest("Wrapped assignment: %r.%s = %r", self, name, value)
        except AttributeError:
            # LOGGER.lowest("Raw assignment: %r.%s = %r", self, name, value)
            super(Wrapper, self).__setattr__(name, value)
