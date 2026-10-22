#ifndef FDTD_SOLVER_H
#define FDTD_SOLVER_H

#include "fdtd_barrier.h"
#include "fdtd_rwlock.h"

typedef struct {
    int step;
    unsigned long job_id; //snapshot job id
    int nx; //snapshot size x
    int ny; //snapshot size y
    int snapshot_step; //snapshot step
    double **ez;
    rwlock_t lock;//protects fields
} fdtd_published_t;

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
    fdtd_published_t *published;
} fdtd_sim_t;

typedef struct {
    int id;
    int first_row;
    int last_row;
    fdtd_sim_t *sim;
} fdtd_thread_t;

double **create_grid(int rows, int cols);
void destroy_grid(double **grid, int rows);

int fdtd_init(fdtd_sim_t *sim, int nx, int ny, int steps, int threads, int k, int sx, int sy);
int fdtd_run(fdtd_sim_t *sim);
double fdtd_sum_abs_ez(fdtd_sim_t *sim);
void fdtd_destroy(fdtd_sim_t *sim);

#endif