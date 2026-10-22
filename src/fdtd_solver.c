#define _POSIX_C_SOURCE 200809L

#include "fdtd_solver.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846 //Pi constant
#define VACUUM_IMPEDANCE 376.730313668 //Vacuum impedance constant
#define RELATIVE_PERMITIVITY 1.0 //Relative permitivity constant
#define POINT_PER_WAVELENGTH 20.0 //PPW constant
#define COURANT_NUMBER 0.70710678118654752440 //Courant number (1/square(2))

static double **create_grid(int rows, int cols)
{
    double **grid = malloc(rows * sizeof(double *));
    if (grid == NULL)
        return NULL;

    for (int m = 0; m < rows; ++m)
    {
        grid[m] = calloc(cols, sizeof(double));
        if (grid[m] == NULL)
        {
            for (int i = 0; i < m; ++i)
                free(grid[i]);
            free(grid);
            return NULL;
        }
    }

    return grid;
}

static void destroy_grid(double **grid, int rows)
{
    if (grid == NULL)
        return;

    for (int m = 0; m < rows; ++m)
        free(grid[m]);
    free(grid);
}

static double ricker(int q)
{
    double sc = COURANT_NUMBER;
    double x = PI * ((sc * q) / POINT_PER_WAVELENGTH - 1.0);
    double arg = x * x;
    return (1.0 - 2.0 * arg) * exp(-arg);
}

static void update_h(fdtd_sim_t *sim, int first_row, int last_row)
{
    double sc = COURANT_NUMBER;
    double coef = sc / VACUUM_IMPEDANCE;

    for (int m = first_row; m <= last_row; ++m)
    {
        for (int n = 0; n < sim->ny - 1; ++n)
            sim->Hx[m][n] = sim->Hx[m][n] - coef * (sim->Ez[m][n + 1] - sim->Ez[m][n]);
    }

    for (int m = first_row; m <= last_row; ++m)
    {
        if (m > sim->nx - 2)
            continue;

        for (int n = 0; n < sim->ny; ++n)
            sim->Hy[m][n] = sim->Hy[m][n] + coef * (sim->Ez[m + 1][n] - sim->Ez[m][n]);
    }
}

static void update_e(fdtd_sim_t *sim, int first_row, int last_row)
{
    double sc = COURANT_NUMBER;
    double coef = sc * VACUUM_IMPEDANCE / RELATIVE_PERMITIVITY;

    for (int m = first_row; m <= last_row; ++m)
    {
        if (m < 1 || m > sim->nx - 2)
            continue;

        for (int n = 1; n < sim->ny - 1; ++n)
            sim->Ez[m][n] = sim->Ez[m][n]
                + coef * ((sim->Hy[m][n] - sim->Hy[m - 1][n]) - (sim->Hx[m][n] - sim->Hx[m][n - 1]));
    }
}

static void *solver_thread(void *arg)
{
    fdtd_thread_t *data = arg;
    fdtd_sim_t *sim = data->sim;

    for (int q = 0; q < sim->steps; ++q)
    {
        update_h(sim, data->first_row, data->last_row);
        barrier_wait(&sim->barrier);

        update_e(sim, data->first_row, data->last_row);

        if (sim->sx >= data->first_row && sim->sx <= data->last_row)
            sim->Ez[sim->sx][sim->sy] = sim->Ez[sim->sx][sim->sy] + ricker(q);

        barrier_wait(&sim->barrier);

        if (data->id==0 && sim->published != NULL)
        {
            rwlock_write_lock(&sim->published->lock);
            sim->published->step = q+1;
            rwlock_write_unlock(&sim->published->lock);
        }
    }

    return NULL;
}

int fdtd_init(fdtd_sim_t *sim, int nx, int ny, int steps, int threads, int k, int sx, int sy)
{
    sim->nx = nx;
    sim->ny = ny;
    sim->steps = steps;
    sim->threads = threads;
    sim->k = k;
    sim->sx = sx;
    sim->sy = sy;
    sim->published = NULL;

    //Init arrays
    sim->Ez = create_grid(nx, ny);
    sim->Hx = create_grid(nx, ny - 1);
    sim->Hy = create_grid(nx - 1, ny);

    if (sim->Ez == NULL || sim->Hx == NULL || sim->Hy == NULL)
    {
        destroy_grid(sim->Ez, nx);
        destroy_grid(sim->Hx, nx);
        destroy_grid(sim->Hy, nx - 1);
        return -1;
    }

    //Init thread barrier
    barrier_init(&sim->barrier, threads);
    return 0;
}

int fdtd_run(fdtd_sim_t *sim)
{
    pthread_t *thread_ids = malloc(sim->threads * sizeof(pthread_t));
    fdtd_thread_t *data = malloc(sim->threads * sizeof(fdtd_thread_t));

    if (thread_ids == NULL || data == NULL)
    {
        free(thread_ids);
        free(data);
        return -1;
    }

    //Rows distribution
    for (int i = 0; i < sim->threads; ++i)
    {
        data[i].id = i;
        data[i].first_row = i * sim->nx / sim->threads;
        data[i].last_row = (i + 1) * sim->nx / sim->threads - 1;
        data[i].sim = sim;
    }

    for (int i = 0; i < sim->threads; ++i)
    {
        int rc = pthread_create(&thread_ids[i], NULL, solver_thread, &data[i]);
        if (rc != 0)
        {
            fprintf(stderr, "pthread_create failed %s\n", strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < sim->threads; ++i)
        pthread_join(thread_ids[i], NULL);

    free(thread_ids);
    free(data);
    return 0;
}

double fdtd_sum_abs_ez(fdtd_sim_t *sim)
{
    double sum = 0.0;

    for (int m = 0; m < sim->nx; ++m)
    {
        for (int n = 0; n < sim->ny; ++n)
            sum = sum + fabs(sim->Ez[m][n]);
    }

    return sum;
}

void fdtd_destroy(fdtd_sim_t *sim)
{
    destroy_grid(sim->Ez, sim->nx);
    destroy_grid(sim->Hx, sim->nx);
    destroy_grid(sim->Hy, sim->nx - 1);
    barrier_destroy(&sim->barrier);
}