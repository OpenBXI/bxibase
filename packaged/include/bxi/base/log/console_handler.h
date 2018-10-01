/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2018 Bull S.A.S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P. 68, 78340 Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#ifndef BXILOG_CONSOLE_HANDLER_H_
#define BXILOG_CONSOLE_HANDLER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "bxi/base/err.h"
#include "bxi/base/log.h"

/**
 * @file    console_handler.h
 * @author Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The Console Logging Handler
 *
 * The console handler produces logs to the console, that is stdout and stderr
 * according to the level of a given log.
 */
//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************
/**
 * An ANSI color code.
 *
 * See: https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
 */
typedef const char * const ansi_color_code_p;

#ifndef BXICFFI
/**
 * An array of string representing ANSI color code for each level
 */
typedef const ansi_color_code_p bxilog_colors_p[BXILOG_LOWEST + 1];
#else
typedef const char* bxilog_colors_p[];
#endif


//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************
#ifndef BXICFFI
/**
 * The Console Handler.
 *
 * Parameters for the ::bxilog_handler_p.param_new() function are given below:
 *
 * @param[in] level a ::bxilog_level_e value; when a log is processed, if the log level
 *                  is less or equals to this level, the log is emitted on the standard
 *                  error, otherwise it is emitted on standard output.
 * @param[in] loggername_width an int representing the maximal size used to display the
 *                  logger name
 * @param[in] colors a ::bxilog_colors_p value representing the colors to use
 *                  according to different log levels
 *
 */
extern const bxilog_handler_p BXILOG_CONSOLE_HANDLER;

/**
 * A dark theme with 216 colors.
 */
extern const bxilog_colors_p BXILOG_COLORS_216_DARK;

/**
 * A dark theme using true color (24 bits).
 */
extern const bxilog_colors_p BXILOG_COLORS_TC_DARK;

/**
 * A dark gray-only theme using true color (24 bits).
 */
extern const bxilog_colors_p BXILOG_COLORS_TC_DARKGRAY;

/**
 * A light theme using true color (24 bits).
 */
extern const bxilog_colors_p BXILOG_COLORS_TC_LIGHT;

/**
 * A light gray-only theme using true color (24 bits).
 */
extern const bxilog_colors_p BXILOG_COLORS_TC_LIGHTGRAY;

/**
 * A theme that specifically defines no color.
 */
extern const char ** BXILOG_COLORS_NONE;
#else
extern bxilog_handler_p BXILOG_CONSOLE_HANDLER;
extern bxilog_colors_p BXILOG_COLORS_216_DARK;
extern bxilog_colors_p BXILOG_COLORS_TC_DARK;
extern bxilog_colors_p BXILOG_COLORS_TC_DARKGRAY;
extern bxilog_colors_p BXILOG_COLORS_TC_LIGHT;
extern bxilog_colors_p BXILOG_COLORS_TC_LIGHTGRAY;
extern char ** BXILOG_COLORS_NONE;
#endif


//*********************************************************************************
//********************************** Interfaces        ****************************
//*********************************************************************************


#ifdef __cplusplus
}
#endif
#endif
