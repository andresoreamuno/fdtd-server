//
// Based on server_unsafe.c created by Sleyter Angulo on 9/17/26.
//

#define _POSIX_C_SOURCE 200809L

#include "net_util.h"
#include "fdtd_server.h"
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

#define V_SUBMIT 1
#define V_STATUS 2
#define V_PEEK 3
#define V_RESULT 4
#define V_SHUTDOWN 5

#define DEFAULT_PORT 8080
#define LISTEN_BACKLOG 64
#define BUFFER_SIZE 100 //Socket buffer max size
#define QUEUE_CAPACITY 8 
#define MAX_SIZE 100000 //Max value for SUBMIT params

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

static job_queue_t g_job_queue = {
    .front = 0,
    .back = 0,
    .next_id = 1,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};


static unsigned long enqueue_job(job_queue_t *queue, job_t *job)
{
    if (sem_trywait(&queue->free) != 0)
        return 0;//no space taken => full queue, return code 0 so that server can inform is BUSY

    pthread_mutex_lock(&queue->mutex);

    job->id = queue->next_id++;
    queue->jobs[queue->back] = *job;
    queue->back = (queue->back  + 1) % QUEUE_CAPACITY;

    pthread_mutex_unlock(&queue->mutex);

    sem_post(&queue->busy);
    return job->id;
}

static void dequeue_job(job_queue_t *queue, job_t *job)
{
    //wait for queued jobs (in a while to handle Ctrl-C interruption)
    while (sem_wait(&queue->busy) && errno == EINTR);

    pthread_mutex_lock(&queue->mutex);

    //pulls job and update queue values
    *job = queue->jobs[queue->front];
    queue->front = (queue->front + 1) % QUEUE_CAPACITY;

    pthread_mutex_unlock(&queue->mutex);

    sem_post(&queue->free);
}

static void enqueue_pill(job_queue_t *queue)
{
    job_t pill = {.id = 0};

    //waits for space to enqueue the pill
    while (sem_wait(&queue->free) && errno == EINTR);

    pthread_mutex_lock(&queue->mutex);

    queue->jobs[queue->back] = pill;
    queue->back = (queue->back + 1) % QUEUE_CAPACITY;

    pthread_mutex_unlock(&queue->mutex);

    sem_post(&queue->busy);
}

static void *dispatcher(void *arg)
{
    (void)arg;
    job_t job;

    
    while(1){
        dequeue_job(&g_job_queue, &job);

        //id = 0 => pill, end cycle
        if (job.id == 0)
            break;

        //TODO: replace print and sleep with solver threads call     
        printf("Dispatcher: job %lu: %dx%d, %d steps, %d threads, k=%d, source (%d,%d)\n",
                                job.id, job.nx,job.ny,job.steps,job.threads, job.k, job.sx, job.sy);
        fflush(stdout);

        sleep(1);
    }
        
    return NULL;
}

static const char *parse_submit(char **rest, job_t *job)
{
    long values[7];

    for (int i = 0; i < 7; ++i)
    {
        char *param = strtok_r(NULL, " ", rest);
        if (param == NULL)
            return "missing params";

        char *end = NULL;
        errno = 0;
        values[i] = strtol(param, &end, 10);
        if (errno != 0 || end == param || *end != '\0' || values[i] < 0 || values[i] > MAX_SIZE)
            return "invalid input";
    }

    if (strtok_r(NULL, " ", rest) != NULL)
        return "too many params";

    job->nx = (int)values[0];
    job->ny = (int)values[1];
    job->steps = (int)values[2];
    job->threads = (int)values[3];
    job->k = (int)values[4];
    job->sx = (int)values[5];
    job->sy = (int)values[6];
    
    if (job->nx < 3 || job->ny < 3)
        return "nx/ny too small";

    if (job->steps <= 0)
        return "negative steps not permitted";

    if (job->threads <= 0 || job->threads > job->nx -2)
        return "invalid thread count";

    if (job->sx >= job->nx || job->sy >= job->ny)
        return "invalid source address";
    
    return NULL;    

}

static void on_sigint(int signum)
{
    (void)signum;
    g_running = 0;
}

static int install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;

    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;//Ignores broken pipe so it doesn't stop server

    if (sigaction(SIGPIPE, &sa, NULL) < 0)
    {
        perror("sigaction");
        return -1;
    }

    return 0;
}

static int numericValue(const char *command)
{
    if (strcmp(command, "SUBMIT") == 0) return V_SUBMIT;
    if (strcmp(command, "STATUS") == 0) return V_STATUS;
    if (strcmp(command, "PEEK") == 0) return V_PEEK;
    if (strcmp(command, "RESULT") == 0) return V_RESULT;
    if (strcmp(command, "SHUTDOWN") == 0) return V_SHUTDOWN;
    return 0;
}

static void *handle_connection(void *arg)
{
    connection_t *conn = arg;

    printf("[Handling connection %lu] accepted\n", conn->connection_id);
    fflush(stdout);

    char line[BUFFER_SIZE];
    ssize_t n = nu_read_request(conn->file_descriptor,line, sizeof line);
    
    if (n>0)//Message received
    {
        printf("[Handling connection %lu] received %s\n", conn->connection_id, line);
        fflush(stdout);

        char reply[BUFFER_SIZE];

        char *rest = NULL;
        char *command = strtok_r(line, " ",&rest);

        if (command == NULL)
            sprintf(reply, "ERROR empty request\n");
        else
        {
            switch (numericValue(command))
            {
                case V_SUBMIT:
                {
                    job_t job;
                    const char *error = parse_submit(&rest, &job);

                    if (error != NULL)
                    {
                        sprintf(reply, "ERROR %s\n", error);
                    }
                    else
                    {
                        unsigned long id = enqueue_job(&g_job_queue, &job);
                        if (id == 0)
                            sprintf(reply, "BUSY\n");
                        else
                            sprintf(reply, "OK %lu\n", id);
                    }
                    break;
                }
                //TODO...
                case V_STATUS:
                    sprintf(reply, "pending...\n");
                    break;   
                case V_PEEK:
                    sprintf(reply, "pending...\n");
                    break;
                case V_RESULT:
                    sprintf(reply, "pending...\n");
                    break;
                case V_SHUTDOWN:
                    sprintf(reply, "pending...\n");
                    break;
                default:
                    sprintf(reply, "ERROR wrong command\n");
                    break;
            }
        }

        nu_write_all(conn->file_descriptor, reply, strlen(reply));
    }

    if (close(conn->file_descriptor) < 0)
        perror("close(file_descriptor)");

    free(conn);
    return NULL;
}

static unsigned short parse_port(int argc, char **argv)
{
    if (argc < 2)
    {
        return DEFAULT_PORT;
    }

    char *end = NULL;
    errno = 0;
    long value = strtol(argv[1], &end, 10);

    if (errno != 0 || end == argv[1] || *end != '\0' ||
        value <= 0 || value > 65535) {
        fprintf(stderr, "invalid port '%s', using %d\n", argv[1], DEFAULT_PORT);
        return DEFAULT_PORT;
        }

    return (unsigned short)value;
}

void* fdtd_solver(void *arg_data){
    fdtd_data_t *data = (fdtd_data_t *)arg_data;
    int sx = data->energy_pixel[0]; //to have easy access to the cell
    int sy = data->energy_pixel[1];

    for(int i = 0; i < data->steps; i++){
        //Main Loop

        //Magentic field changes
        for(int m = 0; m < data->nx_ny[0]; m++){
            for(int n = 0; n < data->nx_ny[1]; n++){
                data->Hx[m][n] -= (SC/IMPENDENT_VOID) * (data->Ez[m][n+1] - data->Ez[m][n]); 
            }
        }
    }

    return NULL;

}



int main(int argc, char **argv)
{
    if (install_signal_handlers() < 0)
    {
        return EXIT_FAILURE;
    }
    unsigned short port = parse_port(argc, argv);

    int listen_file_descriptor = nu_listen(port, LISTEN_BACKLOG);

    if (listen_file_descriptor < 0)
    {
        return EXIT_FAILURE;
    }

    //Init semaphores
    sem_init(&g_job_queue.free,0,QUEUE_CAPACITY);
    sem_init(&g_job_queue.busy,0,0);

    pthread_t dispatcher_thread;
    int dispatcher_created = pthread_create(&dispatcher_thread, NULL, dispatcher, NULL);
    if (dispatcher_created != 0)
    {
        fprintf(stderr, "pthread_create failed %s\n", strerror(dispatcher_created));
        close(listen_file_descriptor);   
        return EXIT_FAILURE;
    }

    printf("listening on port %u — Ctrl-C to stop\n", port);
    fflush(stdout);

    unsigned long accepted = 0;

    while (g_running)
    {
        int client_file_descriptor = accept(listen_file_descriptor, NULL, NULL);
        if (client_file_descriptor < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("accept");
            break;
        }
        connection_t *conn = malloc(sizeof(connection_t));

        if (conn == NULL)
        {
            fprintf(stderr, "out of memory, dropping connection\n");
            close(client_file_descriptor);
            continue;
        }

        conn->file_descriptor = client_file_descriptor;
        conn->connection_id = ++accepted;

        pthread_t thread_id;

        int pthread_created = pthread_create(&thread_id, NULL, handle_connection, conn);

        if (pthread_created != 0)
        {
            fprintf(stderr, "pthread_create failed %s\n", strerror(pthread_created));
            close(client_file_descriptor);
            free(conn);
            --accepted;
            continue;
        }
        pthread_created = pthread_detach(thread_id);
        if (pthread_created != 0)
        {
            fprintf(stderr, "pthread_detach failed %s\n", strerror(pthread_created));
        }

    }

    if (close(listen_file_descriptor))
    {
        perror("close(listen_file_descriptor)");
    }

    sleep(1);//temp, until shutdown

    //enqueue end pills
    enqueue_pill(&g_job_queue);
    int rc = pthread_join(dispatcher_thread,NULL);
    if (rc != 0)
        fprintf(stderr, "pthread_join: %s\n", strerror(rc));   

    //free queue resources
    sem_destroy(&g_job_queue.free);
    sem_destroy(&g_job_queue.busy);
    pthread_mutex_destroy(&g_job_queue.mutex);

    printf("\naccepted: %lu\n", accepted);

    return EXIT_SUCCESS;
}