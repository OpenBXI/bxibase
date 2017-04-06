# -*- coding: utf-8 -*-

"""
@file remote_receiver.py client side of the bxilog remote handler
@authors Pierre Vignéras <pierre.vigneras@atos.net>
@copyright 2016  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.remote_receiver client side of the bxilog remote handler

"""

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


class RemoteReceiver(object):

    def __init__(self, urls, bind):
        """
        Create a new instance connected or binded to given urls.

        if bind is True, the underlying SUB socket will bind to the given URLs
        instead of connecting.

        @param[in] urls the urls to bind/connect to
        @param[in] pub_nb the number of publishers to synchronize with
        @param[in] bind if true, bind instead of connecting

        """
        tmpref = []
        for url in urls:
            tmpref.append(__FFI__.new('char[]', str(url)))
        c_urls = __FFI__.new('char *[]', tmpref)
        self.urls = urls
        self.c_receiver = __BXIBASE_CAPI__.bxilog_remote_receiver_new(c_urls, len(urls),
                                                                      bind)

    def start(self):
        """
        Start receiving bxilogs.

        @note this function immediately returns. Use stop() to
              stop the underlying thread.

        @see stop_receiving()
        """
        err = __BXIBASE_CAPI__.bxilog_remote_receiver_start(self.c_receiver)
        bxierr.BXICError.raise_if_ko(err)

    def stop(self, wait_remote_exit=False):
        """
        Stop receiving bxilogs.

        @param[in] wait_remote_exit if True, wait until all remote publisher have
                                    sent their EXIT message

        @see start_receiving()
        """
        err = __BXIBASE_CAPI__.bxilog_remote_receiver_stop(self.c_receiver,
                                                           wait_remote_exit)
        bxierr.BXICError.raise_if_ko(err)

    def get_binded_urls(self):
        """
        Return the urls the internal thread has binded to or None if not applicable

        @return the urls the internal thread has binded to or None if not applicable
        """
        urls_c = __FFI__.new("char**[1]")
        nb = __BXIBASE_CAPI__.bxilog_get_binded_urls(self.c_receiver, urls_c)
        if nb == 0:
            return None
        return [__FFI__.string(urls_c[0][i]) for i in xrange(nb)]
