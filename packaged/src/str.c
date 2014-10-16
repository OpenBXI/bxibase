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

#include "bxi/base/mem.h"
#include "bxi/base/str.h"

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


// *********************************************************************************
// ********************************** Static Functions  ****************************
// *********************************************************************************

