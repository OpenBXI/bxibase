#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>


void display_res(clockid_t id, char * clock_name) {
    int rc;
    struct timespec res;

    rc = clock_getres(id, &res);
    if (!rc) printf("%s: %ldns\n", clock_name, res.tv_nsec);
}

void bench_clock(clockid_t id, char * clock_name, size_t loop) {
    int rc;
    struct timespec start;
    rc = clock_gettime(CLOCK_MONOTONIC, &start);

    errno = 0;
    for (size_t i = 0; i < loop; i++) {
        struct timespec res;
        int rc = clock_gettime(id, &res);
        if (rc) perror("Calling clock_gettime() failed\n");
    }

    struct timespec end;
    rc = clock_gettime(CLOCK_MONOTONIC, &end);
    if (rc) perror("Calling clock_gettime() failed\n");

    uint64_t sec = end.tv_sec - start.tv_sec;
    uint64_t nsec = end.tv_nsec - start.tv_nsec;
    uint64_t result = (sec * 1e9 + nsec);
    double calls_per_ns = (double) loop / (double) result/1e-9;
    double ns_per_call = (double) result / (double) loop;

    printf("%zu ns\t%g calls/s\t%g ns/call\t %zu calls to clock_gettime(%s, ...)\n",
           result, calls_per_ns, ns_per_call, loop, clock_name);
}


void main(int argc, char** argv) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s calls_nb\n", argv[0]);
        exit(1);
    }

    int calls_nb = atoi(argv[1]);
    printf("Resolution:\n");
    display_res(CLOCK_REALTIME, "CLOCK_REALTIME");
    display_res(CLOCK_REALTIME_COARSE, "CLOCK_REALTIME_COARSE");
    display_res(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");
    display_res(CLOCK_MONOTONIC_COARSE, "CLOCK_MONOTONIC_COARSE");
    display_res(CLOCK_MONOTONIC_RAW, "CLOCK_MONOTONIC_RAW");

    printf("Calling cost:\n");
    bench_clock(CLOCK_REALTIME, "CLOCK_REALTIME", calls_nb);
    bench_clock(CLOCK_REALTIME_COARSE, "CLOCK_REALTIME_COARSE", calls_nb);
    bench_clock(CLOCK_MONOTONIC, "CLOCK_MONOTONIC", calls_nb);
    bench_clock(CLOCK_MONOTONIC_COARSE, "CLOCK_MONOTONIC_COARSE", calls_nb);
    bench_clock(CLOCK_MONOTONIC_RAW, "CLOCK_MONOTONIC_RAW", calls_nb);

}


