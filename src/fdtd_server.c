//
// Server - Server FDTD / CI-0117 
// Based on server_unsafe.c created by Sleyter Angulo on 9/17/26.
//
#include "net_util.h"
#include "fdtd_server.h"


static unsigned long enqueue_job(job_queue_t *queue, job_t *job)
{
    if (sem_trywait(&queue->free) != 0)
        return 0;//no space taken => full queue, return code 0 so that server can inform is BUSY

    pthread_mutex_lock(&queue->mutex);

    //server maximum jobs exceeded
    if (queue->next_id > MAX_JOBS)
    {
        pthread_mutex_unlock(&queue->mutex);
        sem_post(&queue->free);
        return 0;
    }

    job->id = queue->next_id++;
    queue->jobs[queue->back] = *job;
    queue->back = (queue->back  + 1) % QUEUE_CAPACITY;

    //change job status to queued
    pthread_mutex_lock(&g_job_table.mutex);
    g_job_table.job[job->id].job = *job;
    g_job_table.job[job->id].state = V_JOB_QUEUED;
    pthread_mutex_unlock(&g_job_table.mutex);

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
    
        printf("Dispatcher: job %lu: %dx%d, %d steps, %d threads, k=%d, source (%d,%d)\n",
                                job.id, job.nx,job.ny,job.steps,job.threads, job.k, job.sx, job.sy);
        fflush(stdout);

        //change job status to running
        pthread_mutex_lock(&g_job_table.mutex);
        g_job_table.job[job.id].state = V_JOB_RUNNING;
        clock_gettime(CLOCK_MONOTONIC, &g_job_table.job[job.id].start);
        pthread_mutex_unlock(&g_job_table.mutex);


        //TODO: replace with solver call
        sleep(1);


        //change job status to done
        pthread_mutex_lock(&g_job_table.mutex);
        g_job_table.job[job.id].state = V_JOB_RUNNING;
        clock_gettime(CLOCK_MONOTONIC, &g_job_table.job[job.id].start);
        pthread_mutex_unlock(&g_job_table.mutex);
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

static const char *parse_id(char **rest, unsigned long *id)
{
    char *param = strtok_r(NULL, " ", rest);
    if (param == NULL)
        return "missing id";

    char *end = NULL;
    errno = 0;
    long value = strtol(param, &end, 10);
    if (errno != 0 || end == param || *end != '\0' || value < 1 || value > MAX_JOBS)
            return "invalid id";

    if (strtok_r(NULL," ", rest) != NULL)
        return "too many params";

    *id = (unsigned long)value;
    return NULL;   
}

static double duration(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

static void get_status(unsigned long id, char *reply)
{
    pthread_mutex_lock(&g_job_table.mutex);

    job_info_t *info = &g_job_table.job[id];

    if (info->job.id == 0)
    {
        sprintf(reply,"ERROR unknown job\n");
        pthread_mutex_unlock(&g_job_table.mutex);
        return;
    }

    switch (info->state)
    {
        case V_JOB_QUEUED:
        {
            unsigned long position = 1;
            for (unsigned long i = 1; 1 < id; ++i)
                if (g_job_table.job[i].state == V_JOB_QUEUED)
                    ++position;

            sprintf(reply, "QUEUED %lu\n", position);
            break;
        }
        case V_JOB_RUNNING:
        {
            //TODO
            sprintf(reply, "RUNNING\n");
            break;
        }
        case V_JOB_DONE:
        {
            sprintf(reply, "DONE %f\n", duration(info->start,info->end));
            break;
        }
        default:
        {
            sprintf(reply, "ERROR unknown job\n");
            break;
        }
    }

    pthread_mutex_unlock(&g_job_table.mutex);
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
                
                case V_STATUS:
                {
                    unsigned long id;
                    const char *error = parse_id(&rest, &id);

                    if (error != NULL)
                        sprintf(reply, "ERROR %s\n", error);
                    else
                        get_status(id, reply);
                    break; 
                }  
                //TODO
                case V_PEEK:
                    sprintf(reply, "PEEK pending...\n");
                    break;
                case V_RESULT:
                    sprintf(reply, "RESULT pending...\n");
                    break;
                case V_SHUTDOWN:
                    sprintf(reply, "SHUTDOWN pending...\n");
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

double ricker(int q){
    double arg = pow((M_PI * ((SC * q))/ PPW - 1.0), 2);
    double rick =  (1,0 - 2,0 * arg) * exp(-arg); 
    return rick;
}

void* fdtd_solver(void *arg_data){
    fdtd_data_t *data = (fdtd_data_t *)arg_data;
    int sx = data->energy_pixel[0]; //to have easy access to the cell
    int sy = data->energy_pixel[1];

    for(int i = 0; i < data->steps; i++){
        //Main Loop

        //Magentic field changes
        for(int m = 0; m < data->nx_ny[0]-1; m++){
            for(int n = 0; n < data->nx_ny[1]-2; n++){
                data->Hx[m][n] -= (SC/IMPENDENT_VOID) * (data->Ez[m][n+1] - data->Ez[m][n]); 
            }
        }
        for(int m = 0; m < data->nx_ny[0]-2; m++){
            for(int n = 0; n < data->nx_ny[1]-1; n++){
                data->Hy[m][n] -= (SC/IMPENDENT_VOID) * (data->Ez[m+1][n] - data->Ez[m][n]); 
            }
        }

        for(int m = 0; m < data->nx_ny[0]-2; m++){ //changes the electric field
            for(int n = 0; n < data->nx_ny[1]-2; n++){
                data->Ez[m][n] += (SC*IMPENDENT_VOID/RELATIVITY) * (data->Ez[m][n] - data->Ez[m-1][n]) - (data->Ez[m][n] - data->Ez[m][n-1]); 
            }
        }

        //adds wave soruce to the electric field
        data->Ez[sx][sy] = data->Ez[sx][sy] + ricker(i);
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
    pthread_mutex_destroy(&g_job_table.mutex); 

    printf("\naccepted: %lu\n", accepted);

    return EXIT_SUCCESS;
}