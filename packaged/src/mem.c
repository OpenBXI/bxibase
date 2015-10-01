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
#include <assert.h>
#include <string.h>

#include "bxi/base/mem.h"
#include "bxi/base/err.h"

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************


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

void * bximem_calloc(const size_t n) {
    void * ptr = calloc(1, n);
    assert(ptr != NULL || n == 0);
    return(ptr);
}

/*
 * New realloc
 */
void * bximem_realloc(void* ptr, const size_t old_size, const size_t new_size) {
    char * new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL && new_size != 0) {
        bxierr_p err = bxierr_gen("Calling realloc(%p, %zu) failed!", ptr, new_size);
        char * str = bxierr_str(err);
        fprintf(stderr, "%s", str);
        BXIFREE(str);
        bxierr_destroy(&err);
    }
    if (old_size > 0) {
        bxiassert(old_size < new_size);
        // Initialize the memory.
        memset(new_ptr + old_size, 0, new_size - old_size);
    }

    return new_ptr;
}


void bximem_destroy(char ** pointer) {

    // Freeing a NULL pointer is allowed and does nothing.
    // Therefore, it is not required to check for *pointer.

    // So *DO NOT* UNCOMMENT THE FOLLOWING LINE UNLESS YOU HAVE GOOD REASONS!! ;-)
    //    if (pointer == NULL || *pointer == NULL) return;

    if (pointer == NULL) return;
    free(*pointer); // Allowed!
    *pointer = NULL;
}

// *********************************************************************************
// ********************************** Static Functions  ****************************
// *********************************************************************************

