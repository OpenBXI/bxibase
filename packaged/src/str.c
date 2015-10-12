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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "bxi/base/mem.h"
#include "bxi/base/str.h"
#include "bxi/base/err.h"

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

#define STR_INIT_SIZE 128

// *********************************************************************************
// ********************************** Types ****************************************
// *********************************************************************************


// *********************************************************************************
// **************************** Static function declaration ************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Implementation   *****************************
// *********************************************************************************

/*
 * Taken from the GCC manual and cleaned up a bit.
 */
size_t bxistr_vnew(char ** str, const char * const fmt, va_list ap) {
    /* Start with 100 bytes. */
    int size = STR_INIT_SIZE;
    *str = (char *) malloc((size_t) size);
    if (*str == NULL) return 0;

    while(1) {
        char *np;
        va_list apc;

        /* Try to print in the allocated space. */
        va_copy(apc, ap);
        int n = vsnprintf(*str, (size_t) size, fmt, apc);
        va_end(apc);

        /* Check error code */
        if (n < 0) {
            free(*str);
            *str = NULL;
            return 0;
        }

        /* If that worked, return the string. */
        if (n < size) {
            return (size_t) n;
        }

        /* Else try again with more space. */
        size = n + 1; /* Exactly what is needed */

        np = (char *) realloc(*str, (size_t) size);
        if (!np) {
            free(*str);
            *str = NULL;
            return 0;
        }
        *str = np;
    }

    return 0;
}


/*
 * Equivalent to make_message but using an ellipsis instead of a va_list.
 */
char* bxistr_new(const char * const fmt, ...) {
    va_list ap;
    char *msg;

    va_start(ap, fmt);
    bxistr_vnew(&msg, fmt, ap);
    va_end(ap);

    return (msg);
}


bxierr_p bxistr_apply_lines(char * str,
                            bxierr_p (*f)(char * line,
                                          size_t line_len,
                                          bool last,
                                          void *param),
                            void * param) {

        char * s = str;
        char * next = s;
        size_t len = 0;
        while(true) {
            if ('\0' == *next) return f(s, len, true, param);
            if ('\n' == *next) {
                bxierr_p err = f(s, len, false, param);
                if (bxierr_isko(err)) return err;
                len = 0;
                s = next + 1;
            } else {
                len++;
            }
            next++;
        }
        return BXIERR_OK;
}

void bxistr_prefixer_init(bxistr_prefixer_p self,
                          char * prefix, size_t prefix_len) {
    assert(NULL != self);
    assert(NULL != prefix);
    assert(0 < prefix_len);

    self->allocated = false;
    self->prefix = prefix;
    self->prefix_len = prefix_len;
    self->lines_nb = 0;
    self->allocated_lines_nb = 16;

    self->lines = bximem_calloc(self->allocated_lines_nb * sizeof(*self->lines));
    self->lines_len = bximem_calloc(self->allocated_lines_nb * sizeof(*self->lines_len));
}

bxistr_prefixer_p bxistr_prefixer_new(char * prefix, size_t prefix_len) {
    bxistr_prefixer_p result = bximem_calloc(sizeof(*result));
    bxistr_prefixer_init(result, prefix, prefix_len);

    result->allocated = true;
    return result;
}

void bxistr_prefixer_destroy(bxistr_prefixer_p * self_p) {
    bxistr_prefixer_cleanup(*self_p);
    if ((*self_p)->allocated) bximem_destroy((char**) self_p);
}

void bxistr_prefixer_cleanup(bxistr_prefixer_p self) {
    for (size_t i = 0; i < self->lines_nb; i++) {
        BXIFREE(self->lines[i]);
    }
    BXIFREE(self->lines);
    BXIFREE(self->lines_len);
}

bxierr_p bxistr_prefixer_line(char * line, size_t line_len, bool last, void * param) {
    bxistr_prefixer_p self = (bxistr_prefixer_p) param;
    assert(NULL != self);

    if (last && 0 == line_len) return BXIERR_OK;

    size_t len = line_len + self->prefix_len;
    self->lines_len[self->lines_nb] = len;
    char * result = bximem_calloc((len +1) * sizeof(*line));
    strncpy(result, self->prefix, self->prefix_len);
    strncpy(result + self->prefix_len, line, line_len);
    self->lines[self->lines_nb] = result;
    // We did a calloc, no need to initialize last terminating byte to NULL

    self->lines_nb++;

    if (self->lines_nb >= self->allocated_lines_nb) {
        size_t new_size = self->allocated_lines_nb * 2;
        self->lines = bximem_realloc(self->lines,
                                     self->allocated_lines_nb * sizeof(*self->lines),
                                     new_size * sizeof(*self->lines));
        self->lines_len = bximem_realloc(self->lines_len,
                                         self->allocated_lines_nb * sizeof(*self->lines_len),
                                         new_size * sizeof(*self->lines_len));
        self->allocated_lines_nb = new_size;
    }
    return BXIERR_OK;
}


size_t bxistr_join(char * sep, size_t sep_len,
                   char ** lines, size_t *lines_len, size_t lines_nb,
                   char** result) {

    size_t len = 0;

    for (size_t i = 0; i < lines_nb; i++) {
        len += lines_len[i];
        if (i < lines_nb - 1) {
            len += sep_len;
        }
    }

    char * s = bximem_calloc((len + 1) * sizeof(*s));
    len = 0;
    for (size_t i = 0; i < lines_nb; i++) {
        strncpy(s + len, lines[i], lines_len[i]);
        len += lines_len[i];
        if (i < lines_nb - 1) {
            strncpy(s + len, sep, sep_len);
            len += sep_len;
        }
    }

    *result = s;
    return len;
}

const char * bxistr_rsub(const char * const str,
                          const size_t str_len,
                          const char c) {

    if (NULL == str || 0 == str_len) return NULL;

    // start from last character
    size_t i = str_len - 1;
    while(i > 0) {
        if (str[i] == c) {
            i++;
            break;
        }
        i--;
    }

    return str + i;
}


size_t bxistr_digits_nb(int32_t n) {
    // This implementation has been proven to be very fast.
    // See: http://stackoverflow.com/questions/1068849/how-do-i-determine-the-number-of-digits-of-an-integer-in-c
    // Note that since this function target number of digits of line numbers,
    // the chance line numbers are small is high, therefore we start from
    // small numbers.
    if (n < 0) n = (n == INT32_MIN) ? INT32_MAX : -n;
    if (n < 10) return 1;
    if (n < 100) return 2;
    if (n < 1000) return 3;
    if (n < 10000) return 4;
    if (n < 100000) return 5;
    if (n < 1000000) return 6;
    if (n < 10000000) return 7;
    if (n < 100000000) return 8;
    if (n < 1000000000) return 9;
    /*      2147483647 is 2^31-1 - add more ifs as needed
        and adjust this final return as well. */
    return 10;
}

// *********************************************************************************
// ********************************** Static Functions  ****************************
// *********************************************************************************

