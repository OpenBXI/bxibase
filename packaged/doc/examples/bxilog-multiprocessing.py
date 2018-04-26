#!/usr/bin/env python
import bxi.base.log as bxilog
import multiprocessing
import tempfile
import os
import sys


# [BXI Log and Python multiprocessing module]
@bxilog.multiprocessing_target
def my_function(logfilename):
    # Since a fork() has been made, the logging module must be initialized
    bxilog.basicConfig(filename=logfilename, filemode='w')
    # Using the bxilog.multiprocessing_target decorator guarantees that
    # uncaught exception will be reported by the logging system
    # and that the logging system will be cleanup properly (and therefore
    # flushed).
    bxilog.out("In subprocess")
    bxilog.flush()

    # Test 1 : simple trace message
    bxilog.trace("A simple trace message")

    # Test 2 : simple error message
    bxilog.error("A simple error message")

    # Test 3 : catch warning error
    try:
        raise ValueError("An expected exception in first subprocess")
    except:
        bxilog.exception("Handling an exception in subprocess", level=bxilog.WARNING)

    # Test 4 :
    # This will be catched thanks to the bxilog.multiprocessing_target() decorator
    # Otherwise, the exception will appear on the standard error as usual
    raise ValueError("An unexpected exception in second subprocess")
# [BXI Log and Python multiprocessing module]


if __name__ == '__main__':

    filename = tempfile.NamedTemporaryFile(prefix=os.path.basename(sys.argv[0]),
                                           suffix='.bxilog', delete=False).name

    p = multiprocessing.Process(target=my_function, args=[filename])
    p.start()
    p.join()

    with open(filename) as f:
        lines = f.readlines()

        # Test 1 : trace message not write (not good loglevel)
        assert not lines[1].startswith("T|")

        # Test 2 : error message correctly write
        assert lines[1].startswith("E|")
        assert lines[1].endswith("A simple error message\n")

        # Test 3a : exception correctly catch and stacktrace correctly write
        assert lines[2].startswith("W|")
        assert lines[2].endswith("Handling an exception in subprocess\n")
        assert lines[3].startswith("W|")
        assert lines[3].endswith("An expected exception in first subprocess\n")

        # Test 4 : stacktrace correctly write
        assert lines[-1].startswith("E|")
        assert lines[-1].endswith("raise ValueError(\"An unexpected exception in second subprocess\")\n")
