//
// Based on net_util.c created by Sleyter Angulo on 9/14/26.
//

#include "net_util.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int nu_listen(unsigned short port, int backlog)
{
    int file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (file_descriptor < 0)
    {
        perror("socket");
        return -1;
    }

    int yes = 1;
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        perror("setsockopt");
        close(file_descriptor);
        return -1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(file_descriptor, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)
    {
        perror("bind");
        close(file_descriptor);
        return -1;
    }

    if (listen(file_descriptor, backlog) < 0)
    {
        perror("listen");
        close(file_descriptor);
        return -1;
    }

    return file_descriptor;
}

int nu_write_all(int file_descriptor, const void* buffer, size_t size)
{
    const char *p = buffer;
    size_t sent = 0;

    while (sent < size) {
        ssize_t n = write(file_descriptor, p + sent, size - sent);
        if (n < 0) {
            if (errno == EINTR)
                continue;          /* interrupted by a signal: retry */
            perror("write");
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

ssize_t nu_read_request(int file_descriptor, char *buffer, size_t size)
{
    if (size == 0)
        return -1;
    
    size_t read_bytes = 0;

    while (read_bytes < (size-1))
    {
        ssize_t n = read(file_descriptor, buffer + read_bytes, (size-1)-read_bytes);    

        if (n < 0)
        {
            perror("read");
            return -1;
        }

        //EOF
        if (n == 0)
        {
            //if something already read return error code, else 0
            if (read_bytes == 0) 
                return 0;
            else 
                return -1;
        }
        
        char *line = memchr(buffer + read_bytes, '\n', (size_t)n);
        read_bytes += (size_t)n;
        //if line not null => line end found, replace line end with string end
        if (line != NULL)
        {
            *line = '\0';
            return (ssize_t)(line - buffer);

        }
    }

    return -1;
}
