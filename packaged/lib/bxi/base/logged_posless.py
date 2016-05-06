#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Alain Cady <alain.cady@atos.net>
# Contributors: Sébastien Miquée <sebastien.miquee@atos.net>
###############################################################################
# Copyright (C) 2012 - 2016  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################


from configobj import ConfigObjError
from configobj import ConfigObj
from gettext import gettext as _
import bxi.base.log as logging
import bxi.base.log.console_handler as bxilog_consolehandler
import bxi.base.log.file_handler as bxilog_filehandler
import configobj
import posless as argparse
import tempfile
import os.path
import sys

ArgumentError = argparse.ArgumentError
ArgumentTypeError = argparse.ArgumentTypeError
FileType = argparse.FileType
HelpFormatter = argparse.HelpFormatter
ArgumentDefaultsHelpFormatter = argparse.ArgumentDefaultsHelpFormatter
RawDescriptionHelpFormatter = argparse.RawDescriptionHelpFormatter
RawTextHelpFormatter = argparse.RawTextHelpFormatter
Namespace = argparse.Namespace
Action = argparse.Action
ONE_OR_MORE = argparse.ONE_OR_MORE
OPTIONAL = argparse.OPTIONAL
PARSER = argparse.PARSER
REMAINDER = argparse.REMAINDER
SUPPRESS = argparse.SUPPRESS
ZERO_OR_MORE = argparse.ZERO_OR_MORE
DEFAULT_CONFIG_FILE = '/etc/bxi/common.conf'


class LogLevelsAction(argparse.Action):

    def __init__(self,
                 option_strings,
                 dest,
                 default=None,
                 GroupTitle=None,
                 mustbeprinted=True,
                 required=False,
                 help=None,
                 nargs=0,
                 setsighandler=True,
                 metavar=None):

        super(LogLevelsAction, self).__init__(option_strings=option_strings,
                                              dest=dest,
                                              nargs=0,
                                              GroupTitle=mustbeprinted,
                                              mustbeprinted=mustbeprinted,
                                              const=True,
                                              default=default,
                                              required=required,
                                              help=help)

    def __call__(self, parser, namespace, values, option_string=None):
        names = list(logging.get_all_level_names_iter())
        for i in xrange(len(names)):
            print("%s = %s" % (i, names[i]))
        sys.exit(0)


@staticmethod
def _defaultfiltering(action):
    if action.mustbeprinted:
        return True
    return False


class FilteredHelpFormatter(argparse.HelpFormatter):

    getfilterfunction = _defaultfiltering

    def __init__(self,
                 prog,
                 indent_increment=2,
                 max_help_position=24,
                 width=None):
        super(FilteredHelpFormatter, self).__init__(prog,
                                                    indent_increment=indent_increment,
                                                    max_help_position=max_help_position,
                                                    width=width)

    def add_argument(self, action, namespace):
        isactionprinted = FilteredHelpFormatter.getfilterfunction
        if isactionprinted(action):
            super(FilteredHelpFormatter, self).add_argument(action, namespace)


class _HelpActionFormatted(argparse._HelpAction):

    def __call__(self, parser, namespace, values,
                 option_string=None, filter_function=_defaultfiltering):
        parser.formatter_class = FilteredHelpFormatter
        parser.formatter_class.getfilterfunction = filter_function
        parser.print_help(namespace=namespace)
        parser.exit()


@staticmethod
def _logfiltering(action):
    if action.GroupTitle is None or action.GroupTitle == 'BXI Log options':
        return True
    return False


class _LoggedHelpAction(_HelpActionFormatted):
    def __call__(self, parser, namespace, values, option_string=None):
        super(_LoggedHelpAction, self).__call__(parser, namespace, values,
                                                option_string,
                                                filter_function=_logfiltering)


@staticmethod
def _fullfiltering(action):
    return True


class _FullHelpAction(_HelpActionFormatted):
    def __call__(self, parser, namespace, values, option_string=None):
        super(_FullHelpAction, self).__call__(parser, namespace, values,
                                              option_string,
                                              filter_function=_fullfiltering)


class Config_ArgumentParser(argparse.ArgumentParser):

    def __init__(self,
                 prog=None,
                 usage=None,
                 description=None,
                 epilog=None,
                 version=None,
                 parents=[],
                 formatter_class=FilteredHelpFormatter,
                 prefix_chars='-',
                 argument_default=None,
                 conflict_handler='error',
                 add_help=True,
                 add_configfile=True,
                 config_file=DEFAULT_CONFIG_FILE,
                 ):

        super(Config_ArgumentParser, self).__init__(
                 prog=prog,
                 usage=usage,
                 description=description,
                 epilog=epilog,
                 version=version,
                 parents=parents,
                 formatter_class=formatter_class,
                 prefix_chars=prefix_chars,
                 argument_default=argument_default,
                 conflict_handler=conflict_handler,
                 add_help=False)

        if add_configfile:
            glog = self.add_argument_group('BXI Config options')
            glog.add_argument("-C", "--common-config-file", default=config_file,
                            help="configuration file %(default)s"
                            ", environment variable: %(envvar)s",
                            envvar="BXICONFIG", metavar="FILE")
            known_args = self.parse_known_args()[0]

            if os.path.isfile(known_args.common_config_file):
                try:
                    with open(known_args.common_config_file, 'r') as f:
                        self.config = ConfigObj(f, interpolation=False)
                except IOError as err:
                    sys.stderr.write('Configuration file error: %s' % err)
                    logging.cleanup()
                    sys.exit(1)
                except ConfigObjError as err:
                    sys.stderr.write('Configuration file parsing error: %s' % err)
                    logging.cleanup()
                    sys.exit(2)
            else:
                self.config = ConfigObj()

        if add_help:
            self.add_argument(
                '-h', '--help',
                action='help', default=argparse.SUPPRESS,
                help=_('show this help message and exit'))


class ArgumentParser(Config_ArgumentParser):

    def __init__(self,
                 prog=None,
                 usage=None,
                 description=None,
                 epilog=None,
                 version=None,
                 parents=[],
                 formatter_class=FilteredHelpFormatter,
                 prefix_chars='-',
                 argument_default=None,
                 conflict_handler='error',
                 add_help=True,
                 config_file=DEFAULT_CONFIG_FILE,
                 setsighandler=True,
                 add_log_options=True,
                 add_configfile=True,
                 ):

        super(ArgumentParser, self).__init__(
                 prog=prog,
                 usage=usage,
                 description=description,
                 epilog=epilog,
                 version=version,
                 parents=parents,
                 formatter_class=formatter_class,
                 prefix_chars=prefix_chars,
                 argument_default=argument_default,
                 conflict_handler=conflict_handler,
                 add_help=False,
                 add_configfile=add_configfile,
                 config_file=config_file,)

        if add_log_options:
            glog = self.add_argument_group('BXI Log options')

            glog.add_argument("--loglevels",
                            mustbeprinted=False,
                            action=LogLevelsAction,
                            help="displays all log levels and exit.")

            try:
                default = self.config.get('Defaults')
                default_value = default['logfile']
            except:
                default_value = '%s%s%s.bxilog' % (tempfile.gettempdir(),
                                                os.path.sep,
                                                os.path.basename(sys.argv[0]))

            glog.add_argument("--logfile",
                            metavar='FILE',
                            envvar='BXILOGFILE',
                            mustbeprinted=False,
                            default=default_value,
                            help="the file where logs should be output "
                                "(in addition to stdout/stderr). "
                                "Default: %(default)s")

            try:
                default = self.config.get('Defaults')
                default_value = default['console_filters']
            except:
                default_value = ':output'

            glog.add_argument("-l", "--console_filters",
                            metavar='console_filters',
                            envvar='BXILOG_CONSOLE_FILTERS',
                            default=default_value,
                            help="define the logging filters for the console output. "
                            "Default: '%(default)s'. "
                            "Logging filters are defined by the following format: "
                            "logger_name_prefix:level[,prefix:level]*")

            try:
                default = self.config.get('Defaults')
                default_value = default['file_filters']
            except:
                default_value = bxilog_filehandler.FILE_HANDLER_FILTERS_AUTO

            glog.add_argument("--file_filters",
                            metavar='file_filters',
                            mustbeprinted=False,
                            envvar='BXILOG_FILE_FILTERS',
                            default=default_value,
                            help="define the logging filters of the file handler. "
                            "If set to '%s', " % bxilog_filehandler.FILE_HANDLER_FILTERS_AUTO +
                            "filters are automatically computed to "
                            "provide two levels more details than console_filters and "
                            "at least error levels and above. "
                            "The format is the one defined by console_filters option. "
                            "Default: '%(default)s'. ")

            try:
                default = self.config.get('Defaults')
                default_value = default['bxilogcolors']
            except:
                default_value = '216_dark'
            colors = bxilog_consolehandler.COLORS.keys()
            glog.add_argument("--colors", type=str,
                            choices=colors,
                            metavar='colors',
                            mustbeprinted=False,
                            envvar='BXILOG_COLORS',
                            default=default_value,
                            help="define the output colors: "
                            " %s." % ", ".join(colors) + "Default: '%(default)s'.")

            glog.add_argument("--logcfgfile", type=argparse.FileType('r'),
                            mustbeprinted=False,
                            metavar='logcfgfile',
                            help="logging configuration file. If set, other bxilog "
                            "options are ignored.")

            known_args = self.parse_known_args()[0]

            logging.captureWarnings(True)

            if known_args.logcfgfile is not None:
                config = configobj.ConfigObj(infile=known_args.logcfgfile)
            else:
                baseconf = {'handlers': ['console', 'file'],
                            'setsighandler': True,
                            'console': {'module': bxilog_consolehandler.__name__,
                                        'filters': known_args.console_filters,
                                        'stderr_level': 'WARNING',
                                        'colors': known_args.colors,
                                        },
                            'file': {'module': bxilog_filehandler.__name__,
                                    'filters': known_args.file_filters,
                                    'file': known_args.logfile,
                                    'append': True,
                                     }
                            }
                config = configobj.ConfigObj(infile=baseconf)

            logging.set_config(config)

        if add_help:
            self.add_argument(
                '-h', '--help',
                action='help', default=argparse.SUPPRESS,
                help=_('show this help message and exit'))
            self.add_argument('--help-logs',
                              action=_LoggedHelpAction,
                              default=argparse.SUPPRESS,
                              help=_('show detailed logging options and exit'))
            self.add_argument(
                '--help-full',
                action=_FullHelpAction, default=argparse.SUPPRESS,
                help=_('show all options and exit'))
