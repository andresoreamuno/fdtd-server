#ifndef FDTD_BARRIER_H
#define FDTD_BARRIER_H

#include <pthread.h>
#include <semaphore.h>

// two-phase barrier
typedef struct {
    int count;              // Arrived thread count
    int threads;            // Expected thread count
    pthread_mutex_t mutex;  // mutex
    sem_t turnstile1;       // turnstile 1: opens when all threads arrive
    sem_t turnstile2;       // turnstile 2: opens where all thread leave
} barrier_t;

void barrier_init(barrier_t *barrier, int threads);
void barrier_wait(barrier_t *barrier);
void barrier_destroy(barrier_t *barrier);

#endif