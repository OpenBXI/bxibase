#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Pierre Vignéras <pierre.vigneras@bull.net>
###############################################################################
# Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################
"""
Simple program for unit tests of BXI Log Python library.
See testparserconf.py for details.
"""


import bxi.base.log as bxilog

import bxi.base.posless as posless
import bxi.base.parserconf as bxiparserconf


if __name__ == "__main__":

    parser = posless.ArgumentParser(description='Unit Testing ParserConf',
                                    formatter_class=bxiparserconf.FilteredHelpFormatter)
    bxiparserconf.addargs(parser)
    args = parser.parse_args()

    bxilog.panic("A panic message")
    bxilog.alert("An alert message")
    bxilog.critical("A critical message")
    bxilog.error("An error message")
    bxilog.warning("A warning message")
    bxilog.notice("A notice message")
    bxilog.output("An output message")
    bxilog.info("An info message")
    bxilog.debug("A debug message")
    bxilog.fine("A fine message")
    bxilog.trace("A trace message")
    bxilog.lowest("A lowest message")
