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

/**
 * A prefixer structure.
 */
typedef struct {
    bool allocated;                 //!< if true, the structure has been mallocated
    size_t lines_nb;                //!< number of lines
    size_t allocated_lines_nb;      //!< number of allocated lines
    size_t prefix_len;              //!< prefix length
    char * prefix;                  //!< prefix
    char ** lines;                  //!< the list of lines to be prefixed
    size_t * lines_len;             //!< the list of line length
} bxistr_prefixer_s;

/**
 * A prefixer object.
 *
 * @see bxistr_prefixer_new
 * @see bxistr_prefixer_init
 * @see bxistr_prefixer_destroy
 * @see bxistr_prefixer_cleanup
 */
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
 * The function `f()` is passed the current line in the string that must be processed,
 * its length, a boolean `last`, that when true means the line is the last one in the
 * string, and the parameter `param` passed to `bxistr_apply_lines()`.
 *
 * @note the given string `str` is not modified while parsing. If you don't want such a
 *       behavior, duplicate your string before calling this function.
 *
 * @param[inout] str the string to parse
 * @param[in] str_len the length of the string (e.g: as returned by strlen())
 * @param[in] f the function to apply for each line found
 * @param[in] param a parameter that will be given to f for each line found
 *
 *
 * @return the first error returned by `f()`.
 */
bxierr_p bxistr_apply_lines(char * str,
                            size_t str_len,
                            bxierr_p (*f)(char * line,
                                          size_t line_len,
                                          bool last,
                                          void *param),
                            void * param);

/**
 * Allocate, initialize and return a new bxistr_prefixer_p object.
 *
 * @param[in] prefix a string representing the prefix
 * @param[in] prefix_len the length of the prefix string
 *
 * @return a new bxistr_prefixer_p object
 *
 * @see bxistr_prefixer_init
 */
bxistr_prefixer_p bxistr_prefixer_new(char* prefix, size_t prefix_len);

/**
 * Initialize a bxistr_prefixer_p object.
 *
 * @param[in] self a prefixer object
 * @param[in] prefix the prefix to use
 * @param[in] prefix_len the length of the prefix string
 */
void bxistr_prefixer_init(bxistr_prefixer_p self,
                          char* prefix, size_t prefix_len);

/**
 * Prefix a single line using the given bxistr_prefixer_p as the parameter.
 *
 * @note: this function should not be used directly. Instead, use it in combination
 * with bxistr_apply_lines such as in the following code snippet:
 * @snippet bxistr_examples.c Multi-line prefixer
 *
 * @param[in] line the line to prefix
 * @param[in] line_len the length of the line
 * @param[in] last if true, line is the last one
 * @param[in] param the parameter given to bxistr_apply_lines()
 *
 * @return if not BXIERR_OK, ask the caller bxistr_apply_lines() to stop.
 *
 * @see bxistr_apply_lines()
 *
 */
bxierr_p bxistr_prefixer_line(char * line, size_t line_len, bool last, void * param);

/**
 * Destroy the given prefixer.
 *
 * @note the given pointer is nullified.
 *
 * @param[inout] self_p a pointer on the prefixer to destroy
 *
 */
void bxistr_prefixer_destroy(bxistr_prefixer_p *self_p);

/**
 * Cleanup the given prefixer so it can be reinitialized with bxistr_prefixer_init().
 *
 * @note: this not equivalent to a free(). Use bxistr_prefixer_destroy() for this purpose.
 *
 * @param[in] self the prefixer to cleanup
 *
 */
void bxistr_prefixer_cleanup(bxistr_prefixer_p self);

/**
 * Join the given lines with the given separator.
 *
 * @note: the resulting string will have to be freed. Use BXIFREE() for this purpose.
 *
 * @param[in] sep the separator string
 * @param[in] sep_len the length of the separotor string
 * @param[in] lines the lines to join together
 * @param[in] lines_len the array of lines length
 * @param[in] lines_nb the number of elements in both lines and lines_len arrays
 * @param[out] result a pointer on the resulting string
 *
 * @return the length of the resulting string
 *
 *
 */
size_t bxistr_join(char * sep, size_t sep_len,
                   char ** lines, size_t *lines_len, size_t lines_nb,
                   char** result);

/**
 * Return the substring in `str` that starts by `c` starting from the end or the
 * `str` if not found.
 *
 * @note: the returned substring share its memory with `str`. Freeing it using either
 * free() or BXIFREE() is undefined. Also, if you free `str` the returned pointer
 * becomes invalid immediately. If this is not what you want, use strdup(str) before
 * calling this function.
 *
 * @param[in] str the string
 * @param[in] str_len the length of the string
 * @param[in] c the character the substring must start after
 * @param[out] result a pointer on the result
 *
 * @return the length of the substring found
 */
size_t bxistr_rsub(const char * const str,
                   const size_t str_len,
                   const char c,
                   const char ** result);


/**
 * Return the number of digits in the given number.
 *
 * @param[in] n a number
 *
 * @return the number of digits in that number.
 */
size_t bxistr_digits_nb(int32_t n);


/**
 * @example bxistr_examples.c
 * Examples of the bxistr.h module. Compile with `-lbxibase`.
 *
 */


#endif /* BXISTR_H_ */
