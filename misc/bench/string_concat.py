# -*- coding: utf-8 -*-

##
## requires Benchmarker 3.0
##   http://pypi.python.org/pypi/Benchmarker/
##

from __future__ import with_statement

from benchmarker import Benchmarker
from cStringIO import StringIO

LOOP = 10

s1, s2, s3, s4, s5 = "Haruhi", "Mikuru", "Yuki", "Itsuki", "Kyon"
with Benchmarker(width=20, loop=100) as bm:
#for bm in Benchmarker(width=20, loop=1000*1000, cycle=5, extra=1):

    for _ in bm.empty():   ## empty loop
        pass

    for _ in bm('plus op'):
        sos = ""
        for i in xrange(LOOP):
            sos += s1 + s2 + s3 + s4 + s5

    for _ in bm('format'):
        sos = ""
        for i in xrange(LOOP):
            sos = "%s%s%s%s%s%s" % (sos, s1, s2, s3, s4, s5)

    for _ in bm('join'):
        sos = ""
        for i in xrange(LOOP):
            sos = "".join((sos, s1, s2, s3, s4, s5))

    for _ in bm('StringIO'):
        io = StringIO()
        for i in xrange(LOOP):
            io.write(s1)
            io.write(s2)
            io.write(s3)
            io.write(s4)
            io.write(s5)
        sos = io.getvalue()

###
### output example
###
# $ py ex01.py
# ## benchmarker:       release 0.0.0 (for python)
# ## python platform:   darwin [GCC 4.2.1 (Apple Inc. build 5664)]
# ## python version:    2.7.1
# ## python executable: /usr/local/python/2.7.1/bin/python
#
# ##                       user       sys     total      real
# (Empty)                0.1600    0.0000    0.1600    0.1631
# plus op                0.5700    0.0000    0.5700    0.5768
# format                 0.7500    0.0000    0.7500    0.7499
# join                   0.6500    0.0000    0.6500    0.6472
#
# ## Ranking               real
# plus op                0.5768 (100.0%) *************************
# join                   0.6472 ( 89.1%) **********************
# format                 0.7499 ( 76.9%) *******************
#
# ## Ratio Matrix          real    [01]    [02]    [03]
# [01] plus op           0.5768  100.0%  112.2%  130.0%
# [02] join              0.6472   89.1%  100.0%  115.9%
# [03] format            0.7499   76.9%   86.3%  100.0%

