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
                            size_t str_len,
                            bxierr_p (*f)(char * line,
                                          size_t line_len,
                                          bool last,
                                          void *param),
                            void * param) {

        char * s = str;
        char * eol = str + str_len;
        while (true) {
            char * next = strchr(s, '\n');
            next = (NULL == next) ? eol : next;
            size_t len = (size_t) (next - s);
            bool last = next == eol;
            bxierr_p err = f(s, len, last, param);
            if (bxierr_isko(err)) return err;
            if (last) break;
            s = next + 1;
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

size_t bxistr_rsub(const char * const str,
                   const size_t str_len,
                   const char c,
                   const char ** result) {
    bxiassert(NULL != result);

    if (NULL == str || 0 == str_len) {
        *result = NULL;
        return 0;
    }

    const char * r = strrchr(str, c);

    if (NULL == r) {
        *result = str;
    } else {
        *result = r + 1;
    }

    size_t pos = (size_t) (*result - str);

    return str_len - pos;
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

size_t bxistr_count(const char * s, const char c) {
   bxiassert(NULL != s);

   // Trick: increment either i or s. s+i is the current character to check.
   size_t i = 0;
   while (s[i] != '\0') {
       if (s[i] == c) {
           i++; // Character found, increment i
       } else {
           s++; // Character not found, increment s, so s+i is the next character
       }
   }
   return i;
}

char * bxistr_mkshorter(char * s, size_t max_len, char sep) {
    // TODO: unfinished yet!
    // When there are very few sep, but max_len is wide enough,
    // still the process will produce strings way too short.
    // We must be smarter.
    bxiassert(NULL != s);
    bxiassert(0 < max_len);

    char * const result = bximem_calloc((max_len + 1) * sizeof(*result));

    size_t n = bxistr_count(s, sep) + 1;
    if (1 == n) n = max_len;
    // We have n subnames. We want to allocate
    // at least 1 char for the last level
    // at least 2 char for the last - 1 level
    // ...
    // at least n chars for the first level
    size_t max_in_slot[n];
    for (size_t i = 0; i < n; i++) max_in_slot[i] = n - i;

    const char * next = s;
    size_t slot = 0;
    for (size_t i = 0; i < max_len; i++) {
        result[i] = *next;              // Allocate the character
        next++;
        max_in_slot[slot]--;            // Count the allocation

        if (0 == max_in_slot[slot]) { // We allocated all characters for this level
            next = strchr(next, sep); // Find next separator
            if (NULL == next) break; // This is the end
        }
        if (sep == *next) { // We reached the next separator
            next++;                     // Next character is just after
            if (NULL == next) break;    // This is the end
            // Distribute the remaining char in slot to all others
            size_t remaining = max_in_slot[slot];
            for (size_t k = slot + 1; k < n; k++) {
                max_in_slot[k] += remaining / (n - slot);
            }
            slot++;
        }

    }
    return result;
}

bxierr_p bxistr_hex2bytes(char * s, size_t len, uint8_t ** pbuf) {
    // Adapted from:
    // http://stackoverflow.com/questions/3408706/hexadecimal-string-to-byte-array-in-c
    if (NULL == s) return bxierr_gen("Null pointer given for s parameter");

    if (0 == len || 0 != len % 2) return bxierr_gen("Wrong string length: %zu must be "
                                                    "a strictly positive even number",
                                                    len);

    size_t dlength = len / 2;

    if (NULL == *pbuf) *pbuf = bximem_calloc(dlength * sizeof(**pbuf));
    uint8_t * buf = *pbuf;

    size_t index = 0;

    while (index < len) {
        const char c = s[index];

        int value;
        if (c >= '0' && c <= '9') value = (c - '0');
        else if (c >= 'A' && c <= 'F') value = (10 + (c - 'A'));
        else if (c >= 'a' && c <= 'f') value = (10 + (c - 'a'));
        else return bxierr_gen("Non hexadecimal digit: %c in %s", c, s);

        // Multiple lines required to remove GCC warning conversion removals

        // (index + 1) % 2: We want odd numbers to result to 1 and even to 0
        // since the first digit of a hex string is the most significant and
        // needs to be multiplied by 16. so for index 0 => 0 + 1 % 2 = 1,
        // index 1 => 1 + 1 % 2 = 0 etc.
        // << 4: Shift by 4 is multiplying by 16. example: b00000001 << 4 = b00010000
        uint8_t add = (uint8_t) (value << (((index + 1) % 2) * 4));
        // index / 2 :
        // Division between integers will round down the value,
        // so 0/2 = 0, 1/2 = 0, 2/2 = 1, 3/2 = 0 etc.
        // So, for every 2 string characters we add the value to 1 data byte.
        uint8_t cur = buf[(index/2)];
        buf[(index/2)] = (uint8_t) (cur + add);
        index++;
    }

    return BXIERR_OK;
}

bxierr_p bxistr_bytes2hex(uint8_t * buf, size_t len, char ** ps) {
    // Modified from:
    // http://stackoverflow.com/questions/6357031/how-do-you-convert-buffer-byte-array-to-hex-string-in-c

    static const char hex_str[]= "0123456789abcdef";

    if (NULL == buf) return bxierr_gen("Null pointer given for buf parameter.");
    if (NULL == ps) return bxierr_gen("Null pointer given for ps parameter");

    if (NULL == *ps) *ps = bximem_calloc((len * 2 + 1) * sizeof(**ps));

    char * out = *ps;
    out[len * 2] = '\0';

    for (size_t i = 0; i < len; i++) {
        out[i * 2 + 0] = hex_str[(buf[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_str[(buf[i]     ) & 0x0F];
    }

    return BXIERR_OK;
}


// *********************************************************************************
// ********************************** Static Functions  ****************************
// *********************************************************************************

