#ifndef FDTD_SOLVER_H
#define FDTD_SOLVER_H

#include "fdtd_barrier.h"

typedef struct {
    int nx; //Ez rows
    int ny; //Ez cols
    int steps; //Number of steps
    int threads; //Number of solver threads used
    int k; //steps to snapshot
    int sx; //source row
    int sy; //source col
    double **Ez; //electric field z
    double **Hx; //electric field x
    double **Hy; //electric field y
    barrier_t barrier; //solver threads barrier
} fdtd_sim_t;

typedef struct {
    int id;
    int first_row;
    int last_row;
    fdtd_sim_t *sim;
} fdtd_thread_t;

int fdtd_init(fdtd_sim_t *sim, int nx, int ny, int steps, int threads, int k, int sx, int sy);
int fdtd_run(fdtd_sim_t *sim);
double fdtd_sum_abs_ez(fdtd_sim_t *sim);
void fdtd_destroy(fdtd_sim_t *sim);

#endif