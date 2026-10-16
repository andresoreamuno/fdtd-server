//
// Client Server FDTD / CI-0117 
// Based on load_client.c created by Sleyter Angulo on 9/17/26.
//

#define _POSIX_C_SOURCE 200809L

#include "net_util.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define V_SUBMIT 1
#define V_STATUS 2
#define V_PEEK 3
#define V_RESULT 4
#define V_SHUTDOWN 5

#define BUFFER_SIZE 100

static int send_one_request(const struct sockaddr_in *server, const char *request, char *response, size_t size)
{
    int file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (file_descriptor < 0)
    {
        perror("socket");
        return -1;
    }
    if (connect(file_descriptor, (const struct sockaddr *)server, sizeof *server) < 0) {
        perror("connect");
        close(file_descriptor);
        return -1;
    }

    //send request
    if (nu_write_all(file_descriptor,request,strlen(request)) < 0){
        close(file_descriptor);
        return -1;
    }

    //read response
    ssize_t n = nu_read_request(file_descriptor, response, size);

    close(file_descriptor);

    //invalid responses
    if (n <= 0){
        fprintf(stderr, "no response from server\n");
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

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <host> <port> <command>\n",
                argv[0]);
        return EXIT_FAILURE;
    }
 
    switch (numericValue(argv[3])) {
        case V_SUBMIT:  
            //SUBMIT nx ny steps threads k sx sy 
            //OK <id>  BUSY  ERROR <reason>         
            if (argc != 11) {
                fprintf(stderr,"usage: %s <host> <port> SUBMIT nx ny steps threads k sx sy\n",
                        argv[0]);
                return EXIT_FAILURE;
            }
            break;
        case V_STATUS:
            //STATUS <id> 
            //QUEUED <position> RUNNING <step>/<total>  DONE <seconds>  ERROR <reason>
            if (argc != 5) {
                fprintf(stderr,"usage: %s <host> <port> STATUS <id>\n",
                        argv[0]);
                return EXIT_FAILURE;
            }
            break;
        case V_PEEK:
            //PEEK <id> <i> <j> 
            //VALUE <step> <ez>: last published snapshot value
            if (argc != 7) {
                fprintf(stderr,"usage: %s <host> <port> PEEK <id> <i> <j>\n",
                        argv[0]);
                return EXIT_FAILURE;
            }
            break;
        case V_RESULT:
            //RESULT <id> 
            //RESULT <add_abs_ez> <file> when job ends
            if (argc != 5) {
                fprintf(stderr,"usage: %s <host> <port> RESULT <id>\n",
                        argv[0]);
                return EXIT_FAILURE;
            }
            break;
        case V_SHUTDOWN:
            //SHUTDOWN 
            //BYE: Server stop taking jobs, end current job, empty buffer, free resources
            if (argc != 4) {
                fprintf(stderr,"usage: %s <host> <port> SHUTDOWN\n",
                        argv[0]);
                return EXIT_FAILURE;
            }
            break;
        default:
            fprintf(stderr,"Unknown command: %s. Valid commands:\n",argv[3]);
            fprintf(stderr,"SUBMIT nx ny steps threads k sx sy\n");
            fprintf(stderr,"STATUS <id>\n");
            fprintf(stderr,"PEEK <id> <i> <j> \n");
            fprintf(stderr,"RESULT <id> \n");
            fprintf(stderr,"SHUTDOWN \n");
            return EXIT_FAILURE;            
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &server.sin_addr) != 1) {
        fprintf(stderr, "invalid host '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    //prepare message string
    char request[BUFFER_SIZE] = "";
    for (int i = 3; i < argc; ++i) {
        if (strlen(request) + strlen(argv[i] + 2 > sizeof request)) {
            fprintf(stderr,"Buffer size exceeded\n");
            return EXIT_FAILURE;
        }
        strcat(request, argv[i]);
        strcat(request, (i < argc - 1)? "":"\n");
    }

    //send request
    char response[BUFFER_SIZE];
    if (send_one_request(&server, request, response, sizeof response) != 0)
        return EXIT_FAILURE;
    
    printf("Response %s\n", response);


    return EXIT_SUCCESS;
}