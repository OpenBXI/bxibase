/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: May 24, 2013
 # Contributors:
 ###############################################################################
 # Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */

#ifndef BXISTR_H_
#define BXISTR_H_

#include <stdlib.h>
#include <stdarg.h>

/**
 * @file    str.h
 * @brief   String Handling Module
 *
 */

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************
/**
 * Return the number of bytes taken by the given string `str`
 * @param str the string
 * @return the number of bytes
 */
#define BXISTR_BYTES_NB(str) strlen((str)) * sizeof(*(str))

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************


/**
 * Create a message from the given printf style `fmt` parameter.
 *
 * The string returned has been allocated. It must be released
 * (e.g. using BXIFREE()).
 *
 * @param fmt the printf-like format string and their arguments.
 *
 * @return the new allocated string
 *
 * @see bxistr_vnew()
 */
char * bxistr_new(const char * fmt, ...)
#ifndef BXICFFI
    __attribute__ ((format (printf, 1, 2)))
#endif
                             ;
/**
 * Create a message from the given printf style `fmt` parameter.
 *
 * The string returned in the pointer *str_p has been allocated. It must be released
 * (e.g. using BXIFREE()).
 *
 * @param str_p the pointer on the result string
 * @param fmt the printf-like format string and their arguments.
 * @param ap the list of printf-like format
 *
 * @return the length of the allocated string
 * @see bxistr_new()

 * @return
 */
size_t bxistr_vnew(char ** str_p, const char * fmt, va_list ap);


#endif /* BXISTR_H_ */
