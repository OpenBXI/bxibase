# PosLess unitesting: extending the original argparse test suite
# Writen by Steven J. Bethard <steven.bethard@gmail.com>.

import six
import codecs
import inspect
import os
import shutil
import stat
import sys
import textwrap
import tempfile
import unittest
import bxi.base.posless as posless

from io import BytesIO

class StdIOBuffer(BytesIO):
    pass

try:
    from test import test_support
    test_support.EnvironmentVarGuard()
except:
    from test import support as test_support


class TestCase(unittest.TestCase):

    def assertEqual(self, obj1, obj2):
        if obj1 != obj2:
            print('')
            print(repr(obj1))
            print(repr(obj2))
            print(obj1)
            print(obj2)
        super(TestCase, self).assertEqual(obj1, obj2)

    def setUp(self):
        # The tests assume that line wrapping occurs at 80 columns, but this
        # behaviour can be overridden by setting the COLUMNS environment
        # variable.  To ensure that this assumption is true, unset COLUMNS.
        env = test_support.EnvironmentVarGuard()
        env.unset("COLUMNS")
        self.addCleanup(env.__exit__)


class TempDirMixin(object):

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.old_dir = os.getcwd()
        os.chdir(self.temp_dir)

    def tearDown(self):
        os.chdir(self.old_dir)
        shutil.rmtree(self.temp_dir, True)

    def create_readonly_file(self, filename):
        file_path = os.path.join(self.temp_dir, filename)
        with open(file_path, 'wb') as file:
            file.write(filename)
        os.chmod(file_path, stat.S_IREAD)

class Sig(object):

    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs


class NS(object):

    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def __repr__(self):
        sorted_items = sorted(self.__dict__.items())
        kwarg_str = ', '.join(['%s=%r' % tup for tup in sorted_items])
        return '%s(%s)' % (type(self).__name__, kwarg_str)

    __hash__ = None

    def __eq__(self, other):
        return vars(self) == vars(other)

    def __ne__(self, other):
        return not (self == other)


class ArgumentParserError(Exception):

    def __init__(self, message, stdout=None, stderr=None, error_code=None):
        Exception.__init__(self, message, stdout, stderr)
        self.message = message
        self.stdout = stdout
        self.stderr = stderr
        self.error_code = error_code


def stderr_to_parser_error(parse_args, *args, **kwargs):
    # if this is being called recursively and stderr or stdout is already being
    # redirected, simply call the function and let the enclosing function
    # catch the exception
    if isinstance(sys.stderr, StdIOBuffer) and isinstance(sys.stdout, StdIOBuffer):
        return parse_args(*args, **kwargs)

    # if this is not being called recursively, redirect stderr and
    # use it as the ArgumentParserError message
    old_stdout = sys.stdout
    old_stderr = sys.stderr
    sys.stdout = StdIOBuffer()
    sys.stderr = StdIOBuffer()
    try:
        try:
            result = parse_args(*args, **kwargs)
            for key in list(vars(result)):
                if getattr(result, key) is sys.stdout:
                    setattr(result, key, old_stdout)
                if getattr(result, key) is sys.stderr:
                    setattr(result, key, old_stderr)
            return result
        except SystemExit:
            code = sys.exc_info()[1].code
            stdout = sys.stdout.getvalue()
            stderr = sys.stderr.getvalue()
            raise ArgumentParserError("SystemExit", stdout, stderr, code)
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr


class ErrorRaisingArgumentParser(posless.ArgumentParser):

    def parse_args(self, *args, **kwargs):
        parse_args = super(ErrorRaisingArgumentParser, self).parse_args
        return stderr_to_parser_error(parse_args, *args, **kwargs)

    def exit(self, *args, **kwargs):
        exit = super(ErrorRaisingArgumentParser, self).exit
        return stderr_to_parser_error(exit, *args, **kwargs)

    def error(self, *args, **kwargs):
        error = super(ErrorRaisingArgumentParser, self).error
        return stderr_to_parser_error(error, *args, **kwargs)


class ParserTesterMetaclass(type):
    """Adds parser tests using the class attributes.

    Classes of this type should specify the following attributes:

    argument_signatures -- a list of Sig objects which specify
        the signatures of Argument objects to be created
    failures -- a list of args lists that should cause the parser
        to fail
    successes -- a list of (initial_args, options, remaining_args) tuples
        where initial_args specifies the string args to be parsed,
        options is a dict that should match the vars() of the options
        parsed out of initial_args, and remaining_args should be any
        remaining unparsed arguments
    """

    def __init__(cls, name, bases, bodydict):
        if name == 'ParserTestCase':
            return

        # default parser signature is empty
        if not hasattr(cls, 'parser_signature'):
            cls.parser_signature = Sig()
        if not hasattr(cls, 'parser_class'):
            cls.parser_class = ErrorRaisingArgumentParser

        # ---------------------------------------
        # functions for adding optional arguments
        # ---------------------------------------
        def no_groups(parser, argument_signatures):
            """Add all arguments directly to the parser"""
            for sig in argument_signatures:
                parser.add_argument(*sig.args, **sig.kwargs)

        def one_group(parser, argument_signatures):
            """Add all arguments under a single group in the parser"""
            group = parser.add_argument_group('foo')
            for sig in argument_signatures:
                group.add_argument(*sig.args, **sig.kwargs)

        def many_groups(parser, argument_signatures):
            """Add each argument in its own group to the parser"""
            for i, sig in enumerate(argument_signatures):
                group = parser.add_argument_group('foo:%i' % i)
                group.add_argument(*sig.args, **sig.kwargs)

        # --------------------------
        # functions for parsing args
        # --------------------------
        def listargs(parser, args):
            """Parse the args by passing in a list"""
            return parser.parse_args(args)

        def sysargs(parser, args):
            """Parse the args by defaulting to sys.argv"""
            old_sys_argv = sys.argv
            sys.argv = [old_sys_argv[0]] + args
            try:
                return parser.parse_args()
            finally:
                sys.argv = old_sys_argv

        # class that holds the combination of one optional argument
        # addition method and one arg parsing method
        class AddTests(object):

            def __init__(self, tester_cls, add_arguments, parse_args):
                self._add_arguments = add_arguments
                self._parse_args = parse_args

                add_arguments_name = self._add_arguments.__name__
                parse_args_name = self._parse_args.__name__
                for test_func in [self.test_failures, self.test_successes]:
                    func_name = test_func.__name__
                    names = func_name, add_arguments_name, parse_args_name
                    test_name = '_'.join(names)

                    def wrapper(self, test_func=test_func):
                        test_func(self)
                    try:
                        wrapper.__name__ = test_name
                    except TypeError:
                        pass
                    setattr(tester_cls, test_name, wrapper)

            def _get_parser(self, tester):
                args = tester.parser_signature.args
                kwargs = tester.parser_signature.kwargs
                parser = tester.parser_class(*args, **kwargs)
                self._add_arguments(parser, tester.argument_signatures)
                return parser

            def test_failures(self, tester):
                parser = self._get_parser(tester)
                for args_str in tester.failures:
                    args = args_str.split()
                    raises = tester.assertRaises
                    raises(ArgumentParserError, parser.parse_args, args)

            def test_successes(self, tester):
                parser = self._get_parser(tester)
                for args, expected_ns in tester.successes:
                    if isinstance(args, six.string_types):
                        args = args.split()
                    result_ns = self._parse_args(parser, args)
                    tester.assertEqual(expected_ns, result_ns)

        # add tests for each combination of an optionals adding method
        # and an arg parsing method
        for add_arguments in [no_groups, one_group, many_groups]:
            for parse_args in [listargs, sysargs]:
                AddTests(cls, add_arguments, parse_args)

bases = TestCase,
ParserTestCase = ParserTesterMetaclass('ParserTestCase', bases, {})

# ===============
# Optionals tests
# ===============

class TestOnePositionnal(ParserTestCase):
    """Test an argument with a equal sign"""

    argument_signatures = [
            Sig('x')
    ]
    failures = ['--foo', '-x --foo', '-x -y']
    successes = [
        ('a', NS(x='a')),
        ('-1', NS(x='-1')),
        ('x=a', NS(x='a')),
        ('x=-1', NS(x='-1')),
    ]

class TestTwoPositionnal(ParserTestCase):
    """Test an argument with a equal sign"""

    argument_signatures = [
            Sig('foo'),
            Sig('bar'),
    ]
    failures = ['--foo', '-bar --foo', '-x --foo', '-x -y']
    successes = [
        ('foo=x bar=y', NS(foo='x', bar='y')),
        ('bar=x foo=y', NS(foo='y', bar='x')),
        ('foo=1 bar=-1', NS(foo='1', bar='-1')),
        ('foo=-1 bar=1', NS(foo='-1', bar='1')),
    ]

class TestFooBarBob(ParserTestCase):
    """Test thre arguments with a equal sign"""

    argument_signatures = [
            Sig('foo'),
            Sig('bar'),
            Sig('bob'),
    ]
    failures = ['--foo', '-x --foo', '-x -y']
    successes = [
        ('1 2 3', NS(foo='1', bar='2', bob='3')),
        ('foo=1 2 3', NS(foo='1', bar='2', bob='3')),
        ('1 foo=2 3', NS(foo='2', bar='1', bob='3')),
        ('1 2 foo=3', NS(foo='3', bar='1', bob='2')),
        ('bar=1 bob=2 foo=3', NS(foo='3', bar='1', bob='2')),
    ]

class TestOverride(ParserTestCase):
    """Test an argument with a equal sign"""

    argument_signatures = [
            Sig('x')
    ]
    failures = ['--foo', '-x --foo', '-x -y', '1 x=a']
    successes = [
        ('x=-1', NS(x='-1')),
        ('x=a x=b', NS(x='b')),
    ]


# ============================
# from bxi.base.posless import * tests
# ============================

class TestImportStar(TestCase):

    def test(self):
        for name in posless.__all__:
            self.assertTrue(hasattr(posless, name))

    def test_all_exports_everything_but_modules(self):
        items = [
            name
            for name, value in vars(posless).items()
            if not name.startswith("_")
            if not inspect.ismodule(value)
        ]
        self.assertEqual(sorted(items), sorted(posless.__all__))

def test_main():
#    # silence warnings about version argument - these are expected
#    with test_support.check_warnings(
#            ('The "version" argument to ArgumentParser is deprecated.',
#             DeprecationWarning),
#            ('The (format|print)_version method is deprecated',
#             DeprecationWarning)):
    test_support.run_unittest(__name__)
#    # Remove global references to avoid looking like we have refleaks.
#    RFile.seen = {}
#    WFile.seen = set()



if __name__ == '__main__':
    test_main()

