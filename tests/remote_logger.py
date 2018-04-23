#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Pierre Vignéras <pierre.vigneras@bull.net>
###############################################################################
# Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################
from __future__ import print_function

import os
import sys
import configobj

import bxi.base.log as bxilog

"""
Remote logger for testing.
"""

BASENAME = os.path.basename(__file__)
SHORTNAME = os.path.splitext(BASENAME)[0]
FILENAME = "%s.bxilog" % SHORTNAME

_LOGGER = bxilog.get_logger(BASENAME)


def _do_log(start, end):
    nb = 0
    for i in range(int(start), int(end)):
        bxilog.out("Message #%d sent from the child", i)
        nb += 1
    return nb


def main(file_out, url, bind, sync_nb, logs_nb):
    config = {'handlers': ['file', 'remote'],
              'remote': {'module': 'bxi.base.log.remote_handler',
                         'filters': ':all',
                         'url': url,
                         'bind': bind,
                         },
              'file': {'module': 'bxi.base.log.file_handler',
                       'filters': ':all',
                       'path': file_out,
                       'append': True,
                       }
              }
    bxilog.set_config(config)
    nb = 0
    nb += _do_log(0, logs_nb / 2)
    bxilog.cleanup()
    bxilog.set_config(config)
    nb += _do_log(logs_nb / 2, logs_nb)

    return nb

###############################################################################

if __name__ == "__main__":
    if len(sys.argv) != 6:
        print("Usage: %s file_out remote_handler_url bind sync_nb logs_nb" % \
              os.path.basename(sys.argv[0]),
              file=sys.stderr)
        sys.exit(1)

    rc = main(file_out=sys.argv[1],
              url=sys.argv[2],
              bind=sys.argv[3] in ['True', 'true', '1', 'yes', 'Yes'],
              sync_nb=int(sys.argv[4]),
              logs_nb=int(sys.argv[5]))

    sys.exit(rc)
