#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>

#define numof(a) (sizeof(a) / sizeof(a[0]))

/* Random numbers and accuracy checks. */

static int rndnum[10000];
static int rt[numof(rndnum)];

/* All digit counting functions here. */

static int count_recur (int n) {
    if (n < 0) return count_recur ((n == INT_MIN) ? INT_MAX : -n);
    if (n < 10) return 1;
    return 1 + count_recur (n / 10);
}

static int count_diviter (int n) {
    int r = 1;
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
    while (n > 9) {
        n /= 10;
        r++;
    }
    return r;
}

static int count_multiter (int n) {
    unsigned int num = abs(n);
    unsigned int x, i;
    for (x=10, i=1; ; x*=10, i++) {
        if (num < x)
            return i;
  if (x > INT_MAX/10)
            return i+1;
    }
}

static int count_ifs (int n) {
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
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

static int count_revifs (int n) {
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
    if (n > 999999999) return 10;
    if (n > 99999999) return 9;
    if (n > 9999999) return 8;
    if (n > 999999) return 7;
    if (n > 99999) return 6;
    if (n > 9999) return 5;
    if (n > 999) return 4;
    if (n > 99) return 3;
    if (n > 9) return 2;
 return 1;
}

static int count_log10 (int n) {
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
    if (n == 0) return 1;
    return floor (log10 (n)) + 1;
}

static int count_bchop (int n) {
    int r = 1;
    if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
    if (n >= 100000000) {
        r += 8;
        n /= 100000000;
    }
    if (n >= 10000) {
        r += 4;
        n /= 10000;
    }
    if (n >= 100) {
        r += 2;
        n /= 100;
    }
    if (n >= 10)
        r++;

    return r;
}


/* Return the number of digits in the decimal representation of n. */
static int digits(int n) {
    static uint32_t powers[10] = {
        0, 10, 100, 1000, 10000, 100000, 1000000,
        10000000, 100000000, 1000000000,
    };
    static unsigned maxdigits[33] = {
        1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5,
        5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 
    };
    unsigned bits = sizeof(n) * CHAR_BIT - __builtin_clz(n);
    unsigned digits = maxdigits[bits];
    if (n < powers[digits - 1]) {
        -- digits;
    }
    return (int) digits;
}

/* Structure to control calling of functions. */
typedef struct {
    int (*fnptr)(int);
    char *desc;
} tFn;

static tFn fn[] = {
    NULL,                              NULL,
    count_recur,    "            recursive",
    count_diviter,  "     divide-iterative",
    count_multiter, "   multiply-iterative",
    count_ifs,      "        if-statements",
    count_revifs,   "reverse-if-statements",
    count_log10,    "               log-10",
    count_bchop,    "          binary chop",
    digits,         "              builtin",
};
static clock_t clk[numof (fn)];

int main (int c, char *v[]) {
    int i, j, k, r;
    int s = 1;

    /* Test code:
        printf ("%11d %d\n", INT_MIN, count_recur(INT_MIN));
        for (i = -1000000000; i != 0; i /= 10)
            printf ("%11d %d\n", i, count_recur(i));
        printf ("%11d %d\n", 0, count_recur(0));
        for (i = 1; i != 1000000000; i *= 10)
            printf ("%11d %d\n", i, count_recur(i));
        printf ("%11d %d\n", 1000000000, count_recur(1000000000));
        printf ("%11d %d\n", INT_MAX, count_recur(INT_MAX));
    /* */

    /* Randomize and create random pool of numbers. */

 srand (time (NULL));
    for (j = 0; j < numof (rndnum); j++) {
        rndnum[j] = s * rand();
        s = -s;
    }
    rndnum[0] = INT_MAX;
    rndnum[1] = INT_MIN;

    /* For testing. */
    for (k = 0; k < numof (rndnum); k++) {
        rt[k] = (fn[1].fnptr)(rndnum[k]);
    }

    /* Test each of the functions in turn. */

    clk[0] = clock();
    for (i = 1; i < numof (fn); i++) {
        for (j = 0; j < 10000; j++) {
            for (k = 0; k < numof (rndnum); k++) {
                r = (fn[i].fnptr)(rndnum[k]);
                /* Test code:
                    if (r != rt[k]) {
                        printf ("Mismatch error [%s] %d %d %d %d\n",
                            fn[i].desc, k, rndnum[k], rt[k], r);
                        return 1;
                    }
                /* */
            }
        }
        clk[i] = clock();
    }

    /* Print out results. */
  for (i = 1; i < numof (fn); i++) {
        printf ("Time for %s: %10d\n", fn[i].desc, (int)(clk[i] - clk[i-1]));
    }

    return 0;
}
