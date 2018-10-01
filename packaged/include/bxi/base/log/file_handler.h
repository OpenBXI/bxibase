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

#ifndef BXILOG_FILE_HANDLER_H_
#define BXILOG_FILE_HANDLER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "bxi/base/err.h"
#include "bxi/base/log.h"


/**
 * @file    file_handler.h
 * @author Pierre Vignéras <pierre.vigneras@bull.net>
 * @copyright 2018 Bull S.A.S.  -  All rights reserved.\n
 *         This is not Free or Open Source software.\n
 *         Please contact Bull SAS for details about its license.\n
 *         Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
 * @brief  The File Logging Handler
 *
 * The file handler writes logs to a file.
 */
//*********************************************************************************
//********************************** Defines **************************************
//*********************************************************************************

//*********************************************************************************
//********************************** Types ****************************************
//*********************************************************************************


//*********************************************************************************
//********************************** Global Variables  ****************************
//*********************************************************************************
#ifndef BXICFFI
/**
 * The File Handler.
 *
 * Parameters for the ::bxilog_handler_p.param_new() function are given below:
 *
 * @param[in] progname a `char *` string; the program name (argv[0])
 * @param[in] filename a `char *` string; where logs must be must be written
 * @param[in] open_flags an `int` value; as defined by open() (man 2 open)
 *
 * @note for your convenience macros ::BXI_APPEND_OPEN_FLAGS/::BXI_TRUNC_OPEN_FLAGS
 *       are specified for appending/truncating the file respectively.
 */
extern const bxilog_handler_p BXILOG_FILE_HANDLER;
extern const bxilog_handler_p BXILOG_FILE_HANDLER_STDIO;
extern const char BXILOG_FILE_HANDLER_LOG_LEVEL_STR[];
#else
extern bxilog_handler_p BXILOG_FILE_HANDLER;
extern bxilog_handler_p BXILOG_FILE_HANDLER_STDIO;
extern char BXILOG_FILE_HANDLER_LOG_LEVEL_STR[];
#endif

//*********************************************************************************
//********************************** Interfaces        ****************************
//*********************************************************************************


#ifdef __cplusplus
}
#endif
#endif
