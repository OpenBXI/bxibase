#!/usr/bin/env python

#import bxi.base.log as bxilog
import logging as bxilog

import sys

# bxilog.set_config({'handlers':'file',
#                    'file':{'module': 'bxi.base.log.file_handler',
#                            'path': '/tmp/bxilog-profile.bxilog',
#                            'append': False,
#                            'filters': ':out',
#                           },
#                   })
bxilog.basicConfig(level=bxilog.WARNING)

logger = bxilog.getLogger('foo.bar')

for i in xrange(int(sys.argv[1])):

    logger.warning("Something to say?")
