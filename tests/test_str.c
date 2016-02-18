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


#include <CUnit/Basic.h>

#include "bxi/base/str.h"


static bxierr_p count_lines(char * line, size_t line_len, bool last, void * param) {
    UNUSED(line);
    UNUSED(line_len);

    if (last && 0 == line_len) return BXIERR_OK;

    size_t * n = (size_t *) param;

    (*n)++;

    return (*n > 18) ? bxierr_gen("Too many lines!") : BXIERR_OK;
}

void test_bxistr_apply_lines(void) {
    size_t n;
    char * s;
    bxierr_p err;

    n = 0;
    s = strdup("");
    err = bxistr_apply_lines(s, strlen(s), count_lines, &n);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_EQUAL(0, n);
    BXIFREE(s);

    n = 0;
    s = strdup("\n");
    err = bxistr_apply_lines(s, strlen(s), count_lines, &n);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_EQUAL(1, n);
    BXIFREE(s);

    n = 0;
    s = strdup("Line one");
    err = bxistr_apply_lines(s, strlen(s), count_lines, &n);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_EQUAL(1, n);
    BXIFREE(s);

    n = 0;
    s = strdup("Line one\n");
    err = bxistr_apply_lines(s, strlen(s), count_lines, &n);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_EQUAL(1, n);
    BXIFREE(s);

    n = 0;
    s = strdup("Line one\nLine two\nLine three");
    err = bxistr_apply_lines(s, strlen(s), count_lines, &n);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_EQUAL(3, n);
    BXIFREE(s);

    n = 0;
    s = strdup("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17");
    err = bxistr_apply_lines(s, strlen(s), count_lines, &n);
    CU_ASSERT_TRUE(bxierr_isok(err));
    CU_ASSERT_EQUAL(17, n);
    BXIFREE(s);

    n = 0;
    s = strdup("\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n");
    err = bxistr_apply_lines(s, strlen(s), count_lines, &n);
    CU_ASSERT_TRUE(bxierr_isko(err));
    CU_ASSERT_EQUAL(19, n);
    BXIFREE(s);
    bxierr_destroy(&err);
}


void test_bxistr_prefix_lines(void) {

    char * s = strdup("\n");
    bxistr_prefixer_s prefixer;
    bxistr_prefixer_init(&prefixer, "**prefix**", ARRAYLEN("**prefix**") - 1);
    bxistr_apply_lines(s, strlen(s), bxistr_prefixer_line, &prefixer);
    CU_ASSERT_EQUAL(prefixer.lines_nb, 1);
    CU_ASSERT_STRING_EQUAL(prefixer.lines[0], "**prefix**");
    BXIFREE(s);
    bxistr_prefixer_cleanup(&prefixer);

    s = strdup("foo\nbar\ntoto");

    bxistr_prefixer_init(&prefixer, "**prefix**", ARRAYLEN("**prefix**") - 1);
    bxistr_apply_lines(s, strlen(s), bxistr_prefixer_line, &prefixer);
    CU_ASSERT_EQUAL(prefixer.lines_nb, 3);
    CU_ASSERT_STRING_EQUAL(prefixer.lines[0], "**prefix**foo");
    CU_ASSERT_STRING_EQUAL(prefixer.lines[1], "**prefix**bar");
    CU_ASSERT_STRING_EQUAL(prefixer.lines[2], "**prefix**toto");
    BXIFREE(s);
    bxistr_prefixer_cleanup(&prefixer);
}

void test_bxistr_join(void) {

    size_t lines_nb = 1;
    char * lines[] = {"foo"};
    size_t lines_len[] = { ARRAYLEN("foo") - 1 };
    char * result;
    size_t len = bxistr_join("\n", strlen("\n"), lines, lines_len, lines_nb, &result);
    char * expected = "foo";
    CU_ASSERT_EQUAL(len, strlen(expected));
    CU_ASSERT_STRING_EQUAL(result, expected);
    BXIFREE(result);


    lines_nb = 3;
    char * lines2[] = {"foo", "bar", "baz" };
    size_t lines_len2[] = {ARRAYLEN("foo") - 1,
                           ARRAYLEN("bar") - 1,
                           ARRAYLEN("baz") - 1,
    };

    len = bxistr_join(", ", strlen(", "), lines2, lines_len2, lines_nb, &result);
    expected = "foo, bar, baz";
    CU_ASSERT_EQUAL(len, strlen(expected));
    CU_ASSERT_STRING_EQUAL(result, expected);
    BXIFREE(result);

}

void test_bxistr_rfind(void) {

    char * path = "foo";
    const char * s;
    const char * t;

    size_t len = bxistr_rsub(path, strlen(path), '/', &s);
    CU_ASSERT_STRING_EQUAL(s, path);
    CU_ASSERT_EQUAL(len, strlen(s));

    len = bxistr_rsub(s, strlen(s), '/', &t);
    CU_ASSERT_STRING_EQUAL(t, s);
    CU_ASSERT_EQUAL(len, strlen(t));

    len = bxistr_rsub(t, strlen(t), '/', &t);
    CU_ASSERT_STRING_EQUAL(s, t);
    CU_ASSERT_EQUAL(len, strlen(t));

    path = "foo/";
    len = bxistr_rsub(path, strlen(path), '/', &s);
    CU_ASSERT_STRING_EQUAL(s, "")
    CU_ASSERT_EQUAL(len, strlen(s));

    len = bxistr_rsub(s, strlen(s), '/', &t);
    CU_ASSERT_PTR_NULL(t);

    len = bxistr_rsub(t, 0, '/', &t);
    CU_ASSERT_PTR_NULL(t);

    path = "bar/foo";
    len = bxistr_rsub(path, strlen(path), '/', &s);
    CU_ASSERT_STRING_EQUAL(s, "foo");
    CU_ASSERT_EQUAL(len, strlen(s));

    len = bxistr_rsub(s, strlen(s), '/', &t);
    CU_ASSERT_STRING_EQUAL(t, s);
    CU_ASSERT_EQUAL(len, strlen(t));

    len = bxistr_rsub(t, strlen(t), '/', &t);
    CU_ASSERT_STRING_EQUAL(s, t);
    CU_ASSERT_EQUAL(len, strlen(t));

    path = "/boo/bar/foo";
    size_t size = strlen(path);
    len = bxistr_rsub(path, strlen(path), '/', &s);
    CU_ASSERT_STRING_EQUAL(s, "foo");
    CU_ASSERT_EQUAL(len, strlen(s));

    len = bxistr_rsub(s, strlen(s), '/', &t);
    CU_ASSERT_STRING_EQUAL(t, s);
    CU_ASSERT_EQUAL(len, strlen(t));

    len = bxistr_rsub(t, strlen(t), '/', &t);
    CU_ASSERT_STRING_EQUAL(s, t);
    CU_ASSERT_EQUAL(len, strlen(t));

    CU_ASSERT_EQUAL(strlen(path), size);
}

void test_bxistr_count(void) {
    char * s = "foo";
    size_t n = bxistr_count(s, '.');
    CU_ASSERT_EQUAL(n, 0);

    n = bxistr_count(s, 'f');
    CU_ASSERT_EQUAL(n, 1);

    n = bxistr_count(s, 'o');
    CU_ASSERT_EQUAL(n, 2);
}

void test_bxistr_mkshorter(void) {
    char * s = "foo";
    char * r = bxistr_mkshorter(s, 1, '.');

    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "f");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 2, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "fo");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 3, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "foo");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 4, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "foo");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 4, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "foo");
    BXIFREE(r);

    s = "foo.bar";
    r = bxistr_mkshorter(s, 1, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "f");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 2, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "fo");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 3, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "fob");
    BXIFREE(r);

    s = "bxi.base.log.config";
    r = bxistr_mkshorter(s, 1, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "b");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 2, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "bx");
    BXIFREE(r);

    r = bxistr_mkshorter(s, 8, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "bxibaslo");
    BXIFREE(r);

    s = "a.b.c.d.e";
    r = bxistr_mkshorter(s, 5, '.');
    CU_ASSERT_PTR_NOT_NULL_FATAL(r);
    CU_ASSERT_STRING_EQUAL(r, "abcde");
    BXIFREE(r);


}
