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

#ifndef BXICFFI
#include <stdlib.h>
#include <stdarg.h>
#endif


#include "bxi/base/err.h"

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

typedef struct {
    bool allocated;
    size_t lines_nb;
    size_t allocated_lines_nb;
    size_t prefix_len;
    char * prefix;
    char ** lines;
    size_t * lines_len;
} bxistr_prefixer_s;

typedef bxistr_prefixer_s * bxistr_prefixer_p;

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

#ifndef BXICFFI
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
#endif

/**
 * Apply the given function to each line of the given string.
 *
 * The string parsing stops as soon as the given function returns an error.
 *
 * @note the given string is modified while parsing. If you don't want such a behavior,
 *       duplicate your string before calling this function.
 *
 * @param[inout] str the string to parse
 * @param[in] f the function to apply for each line found
 * @param[in] param a parameter that will be given to f for each line found
 *
 *
 * @return the first error returned by f.
 */
bxierr_p bxistr_apply_lines(char * str,
                            bxierr_p (*f)(char * line,
                                          size_t line_len,
                                          bool last,
                                          void *param),
                            void * param);


bxistr_prefixer_p bxistr_prefixer_new(char* prefix, size_t prefix_len);

void bxistr_prefixer_init(bxistr_prefixer_p self,
                          char* prefix, size_t prefix_len);

/**
 * Prefix a single line using the given bxistr_prefixer_p as the parameter.
 *
 * @note: this function should not be used directly. Instead, use it in combination
 * with bxistr_apply_lines such as in the following code snippet:
 *
 * ~~~~~~~{.c}
 * bxistr_prefixer_s prefixer;
 * bxistr_prefixer_init(&prefixer, "*prefix*", ARRAYLEN("*prefix*") - 1);
 * char * s = strdup("One\nTwo\nThree");
 * bxistr_apply_lines(s, bxistr_prefixer_line, &prefixer);
 * for(size_t i = 0; i < prefixer->lines_nb; i++) {
 *      printf("%s", prefixer->lines[i]);
 * }
 * bxistr_prefixer_cleanup(&prefixer);
 * ~~~~~~~
 */
bxierr_p bxistr_prefixer_line(char * line, size_t line_len, bool last, void * param);

void bxistr_prefixer_destroy(bxistr_prefixer_p *self);

void bxistr_prefixer_cleanup(bxistr_prefixer_p self);

/**
 * Join
 */
size_t bxistr_join(char * sep, size_t sep_len,
                   char ** lines, size_t *lines_len, size_t lines_nb,
                   char** result);

const char * bxistr_rfind(const char * const str,
                          const size_t str_len,
                          const char c);

#endif /* BXISTR_H_ */
