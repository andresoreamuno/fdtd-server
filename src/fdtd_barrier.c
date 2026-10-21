#define _POSIX_C_SOURCE 200809L

#include "fdtd_barrier.h"

void barrier_init(barrier_t *barrier, int threads)
{
    barrier->count = 0;
    barrier->threads = threads;
    pthread_mutex_init(&barrier->mutex, NULL);
    sem_init(&barrier->turnstile1, 0, 0);
    sem_init(&barrier->turnstile2, 0, 0);
}

void barrier_wait(barrier_t *barrier)
{
#ifdef SIN_BARRERA
    (void)barrier;
    return;
#else
    //Phase 1
    pthread_mutex_lock(&barrier->mutex);
    barrier->count++;
    if (barrier->count == barrier->threads)
    {
        for (int i = 0; i < barrier->threads; ++i)
            sem_post(&barrier->turnstile1);
    }
    pthread_mutex_unlock(&barrier->mutex);

    sem_wait(&barrier->turnstile1);

    //Phase 2
    pthread_mutex_lock(&barrier->mutex);
    barrier->count--;
    if (barrier->count == 0)
    {
        for (int i = 0; i < barrier->threads; ++i)
            sem_post(&barrier->turnstile2);
    }
    pthread_mutex_unlock(&barrier->mutex);

    sem_wait(&barrier->turnstile2);
#endif
}

void barrier_destroy(barrier_t *barrier)
{
    pthread_mutex_destroy(&barrier->mutex);
    sem_destroy(&barrier->turnstile1);
    sem_destroy(&barrier->turnstile2);
}