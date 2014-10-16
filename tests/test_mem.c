/* -*- coding: utf-8 -*-
 ###############################################################################
 # Author: Pierre Vigneras <pierre.vigneras@bull.net>
 # Created on: Oct 2, 2014
 # Contributors:
 ###############################################################################
 # Copyright (C) 2012  Bull S. A. S.  -  All rights reserved
 # Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
 # This is not Free or Open Source software.
 # Please contact Bull S. A. S. for details about its license.
 ###############################################################################
 */


void test_free(){
    int * double_free = bximem_calloc(sizeof(*double_free));
    *double_free = 1;
    CU_ASSERT_PTR_NOT_NULL_FATAL(double_free);
    CU_ASSERT_EQUAL(*double_free, 1);

    bximem_destroy((char **) &double_free);
    CU_ASSERT_PTR_NULL_FATAL(double_free);
    bximem_destroy((char **) &double_free);
    CU_ASSERT_PTR_NULL_FATAL(double_free);

    double_free = bximem_calloc(sizeof(*double_free));
    *double_free = 2;
    CU_ASSERT_PTR_NOT_NULL_FATAL(double_free);
    CU_ASSERT_EQUAL(*double_free, 2);

    BXIFREE(double_free);
    CU_ASSERT_PTR_NULL_FATAL(double_free);
    BXIFREE(double_free);
    CU_ASSERT_PTR_NULL_FATAL(double_free);

    double_free = bximem_calloc(sizeof(*double_free));
    *double_free = 3;
    int ** df_p = &double_free;
    CU_ASSERT_PTR_NOT_NULL_FATAL(double_free);
    CU_ASSERT_EQUAL(*double_free, 3);
    CU_ASSERT_PTR_NOT_NULL_FATAL(df_p);
    CU_ASSERT_PTR_EQUAL(*df_p, double_free);

    BXIFREE(*df_p);
    CU_ASSERT_PTR_NULL_FATAL(double_free);
}
