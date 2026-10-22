//
// Bench - Server FDTD / CI-0117 
//
#define _POSIX_C_SOURCE 200809L

#include "fdtd_solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

static long read_number(const char *text)
{
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0)
        return -1;
    return value;
}

static double duration(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        fprintf(stderr, "usage: %s nx ny steps threads k\n", argv[0]);
        return EXIT_FAILURE;
    }

    long nx = read_number(argv[1]);
    long ny = read_number(argv[2]);
    long steps = read_number(argv[3]);
    long threads = read_number(argv[4]);
    long k = read_number(argv[5]);

    if (nx < 3 || ny < 3 || steps < 1 || threads < 1 || threads > nx - 2 || k < 0)
    {
        fprintf(stderr, "invalid parameters\n");
        return EXIT_FAILURE;
    }

    //Calculate center for the source
    int sx = (int)nx / 2;
    int sy = (int)ny / 2;

    struct timespec t0, t1, t2, t3;
    fdtd_sim_t sim;

    //Init Stage
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (fdtd_init(&sim, (int)nx, (int)ny, (int)steps, (int)threads, (int)k, sx, sy) != 0)
    {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    // Compute stage
    if (fdtd_run(&sim) != 0)
    {
        fprintf(stderr, "out of memory\n");
        fdtd_destroy(&sim);
        return EXIT_FAILURE;
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    double sum = fdtd_sum_abs_ez(&sim);
    fdtd_destroy(&sim);
    clock_gettime(CLOCK_MONOTONIC, &t3);


    double init_time = duration(t0, t1);
    double compute_time = duration(t1, t2);

    //TODO
    double barrier_time = 0.0;
    double io_time = 0.0;

    double total_time = duration(t0, t3);

    printf("nx,ny,steps,threads,k,init,compute,barrier,io,total,sum_abs_ez\n");
    printf("%ld,%ld,%ld,%ld,%ld,%.6f,%.6f,%.6f,%.6f,%.6f,%.10e\n",
           nx, ny, steps, threads, k,
           init_time, compute_time, barrier_time, io_time, total_time, sum);

    return EXIT_SUCCESS;
}