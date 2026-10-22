#ifndef FDTD_RWLOCK_H
#define FDTD_RWLOCK_H

#include <semaphore.h>

typedef struct {
    int readers;          // readers in critical zone
    sem_t mutex;          // readers mutex
    sem_t roomEmpty;      // binary semaphore 1=nobody inside
    sem_t turnstile;      // turnstile: waiting writer blocks new readers
} rwlock_t;

void rwlock_init(rwlock_t *lock);
void rwlock_read_lock(rwlock_t *lock);
void rwlock_read_unlock(rwlock_t *lock);
void rwlock_write_lock(rwlock_t *lock);
void rwlock_write_unlock(rwlock_t *lock);
void rwlock_destroy(rwlock_t *lock);

#endif
