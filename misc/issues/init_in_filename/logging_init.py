#!/usr/bin/env python
# encoding: utf-8
'''

@author: Pierre Vignéras <pierre.vigneras@atos.net>
'''
import logging
import sys

import mymodule


if __name__ == '__main__':
    logging.basicConfig(format='%(levelname)s:%(module)s:%(filename)s %(message)s', level=logging.DEBUG)
    mymodule.a_logging_function()
