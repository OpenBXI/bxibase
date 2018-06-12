#!/usr/bin/env python
import bxi.base.log as bxilog
import multiprocessing
import tempfile
import os
import sys


# [BXI Log and Python multiprocessing module]
@bxilog.multiprocessing_target
def my_function(logfilename, logfilter):
    # Since a fork() has been made, the logging module must be initialized
    bxilog.basicConfig(filename=logfilename, filemode='w', level=logfilter)
    # Using the bxilog.multiprocessing_target decorator guarantees that
    # uncaught exception will be reported by the logging system
    # and that the logging system will be cleanup properly (and therefore
    # flushed).

    # Test 0 : simple out message
    bxilog.out("In subprocess")
    bxilog.flush()

    # Test 1 : simple lowest message
    bxilog.lowest("A simple lowest message")

    # Test 2 : simple error message
    bxilog.error("A simple error message")

    # Test 3 : catch warning error
    try:
        raise ValueError("An expected exception in first subprocess")
    except ValueError:
        bxilog.exception("Handling an exception in subprocess", level=bxilog.WARNING)

    # Test 4 :
    # This will be catched thanks to the bxilog.multiprocessing_target() decorator
    # Otherwise, the exception will appear on the standard error as usual
    raise ValueError("An unexpected exception in second subprocess")
# [BXI Log and Python multiprocessing module]


if __name__ == '__main__':

    # Test with TRACE loglevel (print all backtrace)
    filename = tempfile.NamedTemporaryFile(prefix=os.path.basename(sys.argv[0]),
                                           suffix='.bxilog', delete=False).name
    logfilter = "%s,~bxilog:off" % bxilog.TRACE
    p = multiprocessing.Process(target=my_function, args=[filename, logfilter])
    p.start()
    p.join()

    idx = 0
    with open(filename) as f:
        lines = f.readlines()
        print lines

        # Test 0 : simple out message
        assert lines[0].startswith("O|")
        assert lines[0].endswith("In subprocess\n")

        # Test 1 : lowest message not write (not good loglevel)
        assert not lines[0].startswith("L|")

        # Test 2 : error message correctly write
        assert lines[1].startswith("E|")
        assert lines[1].endswith("A simple error message\n")

        # Test 3 : exception correctly catch and stacktrace correctly write in trace level
        assert lines[2].startswith("W|")
        assert lines[2].endswith("Handling an exception in subprocess\n")
        assert lines[3].startswith("W|")
        assert lines[3].endswith("ValueError: An expected exception in first subprocess\n")
        assert lines[4].startswith("T|")
        assert lines[5].startswith("T|")
        assert lines[5].endswith("raise ValueError(\"An expected exception in first subprocess\")\n")

        # Test 4 : stacktrace correctly write in error level (Uncaught Exception)
        assert lines[6].startswith("E|")
        assert lines[6].endswith("Uncaught Exception: ValueError\n")
        assert lines[7].startswith("E|")
        assert lines[7].endswith("An unexpected exception in second subprocess\n")
        assert lines[-1].startswith("E|")
        assert lines[-1].endswith("raise ValueError(\"An unexpected exception in second subprocess\")\n")

    # Test with OUTPUT loglevel (not print backtrace except for Uncaught Exception)
    filename = tempfile.NamedTemporaryFile(prefix=os.path.basename(sys.argv[0]),
                                           suffix='.bxilog', delete=True).name
    logfilter = bxilog.OUTPUT
    p = multiprocessing.Process(target=my_function, args=[filename, logfilter])
    p.start()
    p.join()

    idx = 0
    with open(filename) as f:
        lines = f.readlines()

        # Test 0 : simple out message
        assert lines[0].startswith("O|")
        assert lines[0].endswith("In subprocess\n")

        # Test 1 : lowest message not write (not good loglevel)
        assert not lines[0].startswith("L|")

        # Test 2 : error message correctly write
        assert lines[1].startswith("E|")
        assert lines[1].endswith("A simple error message\n")

        # Test 3 : exception correctly catch and stacktrace not write (not good loglevel)
        assert lines[2].startswith("W|")
        assert lines[2].endswith("Handling an exception in subprocess\n")
        assert lines[3].startswith("W|")
        assert lines[3].endswith("ValueError: An expected exception in first subprocess\n")

        # Test 4 : stacktrace correctly write in error level (Uncaught Exception)
        assert lines[4].startswith("E|")
        assert lines[4].endswith("Uncaught Exception: ValueError\n")
        assert lines[5].startswith("E|")
        assert lines[5].endswith("An unexpected exception in second subprocess\n")
        assert lines[-1].startswith("E|")
        assert lines[-1].endswith("raise ValueError(\"An unexpected exception in second subprocess\")\n")
