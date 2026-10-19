#ifndef FDTD_SERVER
#define FDTD_SERVER

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>

#define SC (1/sqrt(2))
#define IMPENDENT_VOID 376.730313668 
#define RELATIVITY 1
#define PPW 20
#define M_PI 3.14159265358979323846



#define _POSIX_C_SOURCE 200809L

#define V_SUBMIT 1
#define V_STATUS 2
#define V_PEEK 3
#define V_RESULT 4
#define V_SHUTDOWN 5

#define V_JOB_QUEUED 0
#define V_JOB_RUNNING 1
#define V_JOB_DONE 2

#define DEFAULT_PORT 8080 //Default port used if not other specified
#define LISTEN_BACKLOG 64 //Max socket connection waiting for accept 
#define BUFFER_SIZE 100 //Socket buffer max size
#define QUEUE_CAPACITY 8 //Job queue capacity
#define MAX_SIZE 100000 //Max value for SUBMIT params
#define MAX_JOBS 10000 //Server job limit per session

static volatile sig_atomic_t g_running = 1;

typedef struct {
    int file_descriptor;
    unsigned long connection_id;
} connection_t;

typedef struct {
    unsigned long id;
    int nx, ny, steps, threads, k, sx, sy;
} job_t;

typedef struct {
    job_t jobs[QUEUE_CAPACITY];
    int front;
    int back;
    unsigned long next_id; //Id Counter
    sem_t free; //Counter semaphore for free spaces
    sem_t busy; //Counter semaphore for busy spaces
    pthread_mutex_t mutex; //Critical zone mutex
} job_queue_t;

typedef struct {
    job_t job;  
    int state; //V_JOB_QUEUED,V_JOB_RUNNING,V_JOB_DONE
    struct timespec start;
    struct timespec end;
} job_info_t;

typedef struct {
    job_info_t job[MAX_JOBS+1]; 
    pthread_mutex_t mutex; 
} job_table_t;

static job_queue_t g_job_queue = {
    .front = 0,
    .back = 0,
    .next_id = 1,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

static job_table_t g_job_table = {
    .mutex = PTHREAD_MUTEX_INITIALIZER
};



typedef struct { //struct should be made beffore entering buffer
    int id; //use only for thread identification
    int nx_ny [2]; //size of the energy array
    int **Ez; //multidimensional arrays of electric and magnetic fields, shoudl be initialized on 0s
    int **Hx;
    int **Hy;
    int steps; //nuember of steps to be perform
    int k_step; //number of steps each time data goes to the instants buffer 
    int energy_pixel[2]; //direction of the cell that represent the energy added to the electric field, sx and sy
} fdtd_data_t;

void* fdtd_solver(void *arg_data);

double ricker(int q);

#endif