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
"""
Simple program for unit tests of BXI Log Python library.
See testbxilog.py for details.
"""


import os

import bxi.base.log as bxilog

import __main__

basename = os.path.basename(__main__.__file__)
FILENAME = "%s.bxilog" % os.path.splitext(basename)[0]

if __name__ == "__main__":
    bxilog.basicConfig(filename=FILENAME, level=bxilog.TRACE)

    bxilog.output("Will raise an exception without any try/except, log must catch it")
    raise ValueError("Normal: this must appears in the file")
