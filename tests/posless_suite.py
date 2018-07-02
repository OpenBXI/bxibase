# -*- coding: utf-8 -*-
###############################################################################
# Authors: Pierre Vignéras <pierre.vigneras@atos.net>
# Contributor: Sébastien Miquée <sebastien.miquee@atos.net>
###############################################################################
# Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################
from unittest.loader import TestLoader
import unittest


def suite():
    suite = TestLoader().loadTestsFromNames(['specific_argparse_poslessed',
                                             'specific_posless'])
    return suite


def load_tests(loader, tests, pattern):
    return suite()


if __name__ == '__main__':
    runner = unittest.TextTestRunner()
    test_suite = suite()
    result = runner.run(test_suite)
    if result.wasSuccessful():
        exit(0)
    else:
        exit(1)
