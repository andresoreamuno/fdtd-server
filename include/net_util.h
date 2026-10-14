//
// Based on net_util.h created by Sleyter Angulo on 9/14/26.
//

#ifndef MINI_SERVER_NET_UTIL_H
#define MINI_SERVER_NET_UTIL_H

#include <stddef.h>
#include <sys/types.h>

int nu_listen(unsigned short port, int backlog);

int nu_write_all(int file_descriptor, const void *buffer, size_t size);

ssize_t nu_read_request(int file_descriptor, char *buffer, size_t size);

#endif //MINI_SERVER_NET_UTIL_H