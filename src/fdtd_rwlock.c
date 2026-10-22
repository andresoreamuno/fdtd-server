//
// Reades-Writers Lock - Server FDTD / CI-0117 
//

#define _POSIX_C_SOURCE 200809L

#include "fdtd_rwlock.h"

void rwlock_init(rwlock_t *lock)
{
    lock->readers = 0;
    sem_init(&lock->mutex, 0, 1);
    sem_init(&lock->roomEmpty, 0, 1);
    sem_init(&lock->turnstile, 0, 1);
}

void rwlock_read_lock(rwlock_t *lock)
{
    sem_wait(&lock->turnstile);
    sem_post(&lock->turnstile);

    sem_wait(&lock->mutex);
    lock->readers += 1;
    if (lock->readers == 1)
        sem_wait(&lock->roomEmpty);
    sem_post(&lock->mutex);
}

void rwlock_read_unlock(rwlock_t *lock)
{
    sem_wait(&lock->mutex);
    lock->readers -= 1;
    if (lock->readers == 0)
        sem_post(&lock->roomEmpty);
    sem_post(&lock->mutex);
}

void rwlock_write_lock(rwlock_t *lock)
{
    //applies turnstile and wait for room to empty
    sem_wait(&lock->turnstile);
    sem_wait(&lock->roomEmpty);
}

void rwlock_write_unlock(rwlock_t *lock)
{
    sem_post(&lock->turnstile);
    sem_post(&lock->roomEmpty);
}

void rwlock_destroy(rwlock_t *lock)
{
    sem_destroy(&lock->mutex);
    sem_destroy(&lock->roomEmpty);
    sem_destroy(&lock->turnstile);
}
