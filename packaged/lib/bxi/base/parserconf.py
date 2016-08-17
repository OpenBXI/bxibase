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
import bxi.base.posless as posless
import tempfile
import os.path
import sys
import bxi.base.err as bxierr

ArgumentError = posless.ArgumentError
ArgumentTypeError = posless.ArgumentTypeError
FileType = posless.FileType
HelpFormatter = posless.HelpFormatter
ArgumentDefaultsHelpFormatter = posless.ArgumentDefaultsHelpFormatter
RawDescriptionHelpFormatter = posless.RawDescriptionHelpFormatter
RawTextHelpFormatter = posless.RawTextHelpFormatter
Namespace = posless.Namespace
Action = posless.Action
ONE_OR_MORE = posless.ONE_OR_MORE
OPTIONAL = posless.OPTIONAL
PARSER = posless.PARSER
REMAINDER = posless.REMAINDER
SUPPRESS = posless.SUPPRESS
ZERO_OR_MORE = posless.ZERO_OR_MORE
DEFAULT_CONFIG_FILE = '/etc/bxi/common.conf'


class LogLevelsAction(posless.Action):

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
                                              GroupTitle=GroupTitle,
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


class FilteredHelpFormatter(posless.HelpFormatter):

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


class HelpActionFormatted(posless._HelpAction):

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


class _LoggedHelpAction(HelpActionFormatted):
    def __call__(self, parser, namespace, values, option_string=None):
        super(_LoggedHelpAction, self).__call__(parser, namespace, values,
                                                option_string,
                                                filter_function=_logfiltering)


@staticmethod
def _fullfiltering(action):
    return True


class _FullHelpAction(HelpActionFormatted):
    def __call__(self, parser, namespace, values, option_string=None):
        super(_FullHelpAction, self).__call__(parser, namespace, values,
                                              option_string,
                                              filter_function=_fullfiltering)


def _add_config(parser, config_file):

    def _add_config_file_arg(target_parser):
        group = target_parser.add_argument_group('BXI Config options')
        group.add_argument("-C", "--common-config-file", default=config_file,
                           help="configuration file %(default)s"
                           ", environment variable: %(envvar)s",
                           envvar="BXICONFIG", metavar="FILE")

    dummy = posless.ArgumentParser(add_help=False)
    _add_config_file_arg(dummy)
    _add_config_file_arg(parser)

    known_args = dummy.parse_known_args()[0]

    if os.path.isfile(known_args.common_config_file):
        try:
            with open(known_args.common_config_file, 'r') as f:
                parser.config = ConfigObj(f, interpolation=False)
        except IOError as err:
            sys.stderr.write('Configuration file error: %s' % err)
            logging.cleanup()
            sys.exit(1)
        except ConfigObjError as err:
            sys.stderr.write('Configuration file parsing error: %s' % err)
            logging.cleanup()
            sys.exit(2)
    else:
        parser.config = ConfigObj()


def _configure_log(parser):
    def _add_log(target_parser):
        group = target_parser.add_argument_group('BXI Log options')
        group.add_argument("--loglevels",
                           mustbeprinted=False,
                           action=LogLevelsAction,
                           help="displays all log levels and exit.")

        try:
            default = parser.config.get('Defaults')
            default_value = default['logfile']
        except:
            default_value = '%s%s%s.bxilog' % (tempfile.gettempdir(),
                                               os.path.sep,
                                               os.path.basename(sys.argv[0]))

        group.add_argument("--logfile",
                           metavar='FILE',
                           envvar='BXILOGFILE',
                           mustbeprinted=False,
                           default=default_value,
                           help="the file where logs should be output "
                                "(in addition to stdout/stderr). "
                                "Default: %(default)s")

        try:
            default = parser.config.get('Defaults')
            default_value = default['console_filters']
        except:
            default_value = ':output'

        group.add_argument("-l", "--console_filters",
                           metavar='console_filters',
                           envvar='BXILOG_CONSOLE_FILTERS',
                           default=default_value,
                           help="define the logging filters for the console output. "
                                "Default: '%(default)s'. "
                                "Logging filters are defined by the following format: "
                                "logger_name_prefix:level[,prefix:level]*")

        try:
            default = parser.config.get('Defaults')
            default_value = default['file_filters']
        except:
            default_value = bxilog_filehandler.FILE_HANDLER_FILTERS_AUTO

        group.add_argument("--file_filters",
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
            default = parser.config.get('Defaults')
            default_value = default['bxilogcolors']
        except:
            default_value = '216_dark'
        colors = bxilog_consolehandler.COLORS.keys()
        group.add_argument("--colors", type=str,
                           choices=colors,
                           metavar='colors',
                           mustbeprinted=False,
                           envvar='BXILOG_COLORS',
                           default=default_value,
                           help="define the output colors: "
                           " %s." % ", ".join(colors) + "Default: '%(default)s'.")

        group.add_argument("--logcfgfile", type=posless.FileType('r'),
                           mustbeprinted=False,
                           metavar='logcfgfile',
                           help="logging configuration file. If set, other bxilog "
                           "options are ignored.")

    dummy = posless.ArgumentParser(add_help=False)
    _add_log(dummy)
    _add_log(parser)

    known_args = dummy.parse_known_args()[0]

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

    parser.add_argument('--help-logs',
                        action=_LoggedHelpAction,
                        default=posless.SUPPRESS,
                        help=_('show detailed logging options and exit'))


def addargs(parser, config_file=DEFAULT_CONFIG_FILE, setsighandler=True):
    _add_config(parser, config_file)
    _configure_log(parser)

    parser.add_argument('--help-full',
                        action=_FullHelpAction, default=posless.SUPPRESS,
                        help=_('show all options and exit'))

def getdefaultvalue(parser, Sections, value, _LOGGER, default=None, config=None):
    def _return_dict_value(config, Sections):
        try:
            if len(Sections) > 1:
                return _return_dict_value(config.get(Sections[0]), Sections[1:])
            else:
                return config.get(Sections[0])
        except bxierr.BXIError as e:
            raise bxierr.BXIError("[%s]%s" % (Sections[0], e.msg))
        except:
            _LOGGER.exception("Initial error:", level=logging.DEBUG)
            raise bxierr.BXIError("[%s]" % Sections[0])
    try:
        try:
            if config is None:
                config = parser.config
        except AttributeError:
            _LOGGER.exception("No configuration provided."
                            " Using %s as default for value %s from section %s",
                            default, value, Sections, level=logging.DEBUG)
            return default

        try:
            dictonary = _return_dict_value(config, Sections)
        except bxierr.BXIError as e:
            _LOGGER.debug("Provided configuration (config) doesn't include the (sub)section config%s."
                        " using %s as default for value %s from (sub)section [%s]", e.msg,
                            default, value, ']['.join(Sections))
            return default

        try:
            _LOGGER.debug("Return %s for value %s in section %s",
                          dictonary[value], value, Sections)
            return dictonary[value]
        except (KeyError, TypeError, AttributeError):
            _LOGGER.exception("No value %s in the (sub)sections [%s]."
                            " using %s as default for value %s from (sub)section [%s]",
                            value, ']['.join(Sections),
                            default, value, ']['.join(Sections), level=logging.DEBUG)
            return default
    except:
        _LOGGER.exception("Log erro")
