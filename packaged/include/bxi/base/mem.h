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

#ifndef BXIMEM_H_
#define BXIMEM_H_

#ifndef BXICFFI
#include <stdlib.h>
#endif

// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

/**
 * Return the size of the given array in bytes.
 * @param a the array
 * @return the size in bytes of the given array
 */
#define ARRAYLEN(a) (sizeof((a))/sizeof((a)[0]))

/**
 * Used to free() the memory pointed to by the given `pointer` and also
 * nullify the pointer value.
 *
 * @see bximem_destroy()
 */
#define BXIFREE(pointer) bximem_destroy((char**) &(pointer))

/**
 * Used to prevent warning = error at compile time
 */
#define UNUSED(x) (void) (x)

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
 * Replaces malloc().
 *
 * Allocate the given number of bytes, and clear all the memory with 0.
 * A check is also performed on the returned value to ensure the returned pointer
 * is usable.
 *
 * @param n the number of bytes to allocate
 * @return a usable pointer on a memory area of the given size
 *
 * @see bximem_destroy()
 */
void * bximem_calloc(size_t n);

/**
 * Replaces realloc().
 *
 * Same usage as realloc(). A check is performed to ensure the returned pointer
 * is usable.
 *
 * @param ptr
 * @param n
 * @return a usable pointer
 */
void * bximem_realloc(void *ptr, size_t n);

/**
 * Free and nullify the given pointer.
 *
 * Note here, we take a char** pointer type but this is just
 * to prevent GCC strict aliasing from being disturbed and
 * raise error or warning. Any pointer can be aliased to char*
 * according to C standard, and therefore, feel free to pass
 * any pointer casted to (char**) to this function.
 *
 * @param pointer the address of a pointer on a memory area allocated using malloc()
 *                or bximem_calloc() or bximem_realloc().
 * @see bximem_alloc()
 */
void bximem_destroy(char ** pointer);

#endif /* BXIMEM_H_ */
