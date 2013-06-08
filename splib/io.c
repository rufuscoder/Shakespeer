/*
 * Copyright 2004-2006 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <sys/socket.h> /* for getsockname */
#include <sys/un.h>

#include <netinet/in.h> /* for inet_ntoa */
#include <arpa/inet.h>
#include <netdb.h> /* for gethostbyname */

#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "io.h"
#include "util.h"
#include "log.h"
#include "xstr.h"

/* hostport assumed to be "host-or-ip[:port]". If no port specified, uses port
 * 411.
 *
 * returns null on error
 */
struct sockaddr_in *io_lookup(const char *hostport, xerr_t **err)
{
    struct hostent *he;
    char *host = 0;
    int port = 0;

    if(split_host_port(hostport, &host, &port) != 0)
    {
	xerr_set(err, -1, "%s: invalid address", hostport);
	return NULL;
    }

    if(port < 0)
    {
        xerr_set(err, -1, "Invalid port: %i", port);
        free(host);
        return NULL;
    }
    else if(port == 0)
    {
        port = 411;
    }

    struct sockaddr_in *addr = calloc(1, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    DEBUG("resolving host [%s]", host);
    if(inet_aton(host, &addr->sin_addr) == 0)
    {
        he = gethostbyname(host);
        if(!he)
        {
            xerr_set(err, -1, "%s", hstrerror(h_errno));
            free(host);
            return NULL;
        }
        memcpy(&addr->sin_addr, he->h_addr, he->h_length);
    }

    free(host);
    return addr;
}

typedef struct io_async_connect_data io_async_connect_data_t;
struct io_async_connect_data
{
    void (*event_func)(int fd, int error, void *user_data);
    void *data;
    struct event connect_event;
};

static void io_connect_event(int fd, short condition, void *data)
{
    int error = 0;
    io_async_connect_data_t *acd = data;

    /* get the real error from the socket */
    unsigned int len = sizeof(error);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if(error == 0)
    {
        DEBUG("got connection event on file descriptor %d", fd);

        io_set_blocking(fd, 1);
    }
    else
    {
        INFO("error in connection event on fd %d: %s", fd, strerror(error));
    }

    acd->event_func(fd, error, acd->data);
    event_del(&acd->connect_event);
    free(acd);
}

int io_connect_async(struct sockaddr_in *remote_addr,
        void (*event_func)(int fd, int error, void *user_data),
        void *user_data,
        xerr_t **err)
{
    int fd;

    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd == -1)
    {
        WARNING("%s", strerror(errno));
        xerr_set(err, -1, "Can't create socket: %s", strerror(errno));
        return -1;
    }

    INFO("connecting to %s:%d...",
            inet_ntoa(remote_addr->sin_addr), ntohs(remote_addr->sin_port));

    /* set socket non-blocking */
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof(on));

    int rc = connect(fd, (struct sockaddr *)remote_addr,
            sizeof(struct sockaddr_in)); 
    if(rc != 0 && errno != EINPROGRESS)
    {
        DEBUG("rc = %d, errno = %d, strerror = %s", rc, errno, strerror(errno));

        xerr_set(err, -1, "Can't connect to %s:%d: %s",
                inet_ntoa(remote_addr->sin_addr), ntohs(remote_addr->sin_port),
                strerror(errno));
        WARNING("%s", (*err)->message);
        return -1;
    }

    io_async_connect_data_t *acd = calloc(1, sizeof(io_async_connect_data_t));
    acd->event_func = event_func;
    acd->data = user_data;

    event_set(&acd->connect_event, fd, EV_WRITE, io_connect_event, acd);
    event_add(&acd->connect_event, NULL);

    return 0;
}

int io_connect_tcp(struct sockaddr_in *remote_addr, xerr_t **err)
{
    int fd;

    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd == -1)
    {
        WARNING("%s", strerror(errno));
        xerr_set(err, -1, "Can't create socket: %s",
                strerror(errno));
        return -1;
    }

    INFO("connecting to %s:%d...",
            inet_ntoa(remote_addr->sin_addr), ntohs(remote_addr->sin_port));

    int rc = connect(fd, (struct sockaddr *)remote_addr,
            sizeof(struct sockaddr_in)); 
    if(rc != 0)
    {
        xerr_set(err, -1, "Can't connect to %s:%d: %s",
                inet_ntoa(remote_addr->sin_addr), ntohs(remote_addr->sin_port),
                strerror(errno));
        WARNING("can't connect to %s:%u: %s",
                inet_ntoa(remote_addr->sin_addr),
                ntohs(remote_addr->sin_port), strerror(errno));
        return -1;
    }

    return fd;
}

int io_bind_unix_socket(const char *filename)
{
    struct sockaddr_un addr;
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(fd == -1)
    {
        WARNING("%s", strerror(errno));
        return -1;
    }

    if(unlink(filename) != 0 && errno != ENOENT)
    {
        WARNING("%s: %s", filename, strerror(errno));
        close(fd);
        return -1;
    }
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, filename, sizeof(addr.sun_path));
    if(bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) != 0)
    {
        WARNING("%s", strerror(errno));
        close(fd);
        return -1;
    }

    if(listen(fd, 17) != 0)
    {
        WARNING("%s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int io_bind_tcp_socket(int port, xerr_t **err)
{
    int on = 1;
    struct sockaddr_in local_addr;

    int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd == -1)
    {
        xerr_set(err, -1, "%s", strerror(errno));
        return -1;
    }

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY; /* bind to all addresses */

    /* enable local address reuse */
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
    {
        WARNING("unable to enable local address reuse (ignored)");
    }

    INFO("Binding TCP port %u", port);
    if(bind(fd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in)) != 0)
    {
        xerr_set(err, -1, "%s", strerror(errno));
        close(fd);
        return -1;
    }

    if(listen(fd, 20) != 0)
    {
        xerr_set(err, -1, "%s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int io_accept_connection_addr(int fd, char **ip_address)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    socklen_t addrlen = sizeof(struct sockaddr_in);
    int afd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if(afd == -1)
    {
        WARNING("accept failed: %s", strerror(errno));
    }
    else
    {
        INFO("accepted connection from %s:%d",
                inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        if(ip_address)
        {
            *ip_address = strdup(inet_ntoa(addr.sin_addr));
        }
    }
    
    return afd;
}

int io_accept_connection(int fd)
{
    return io_accept_connection_addr(fd, NULL);
}

int io_connect_unix(const char *filename)
{
    struct sockaddr_un addr;
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(fd == -1)
    {
        perror("socket");
        return -1;
    }
    INFO("connecting to socket '%s'...", filename);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, filename, sizeof(addr.sun_path));
    if(connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) != 0)
    {
        if(errno == ENOENT)
            INFO("%s: %s", filename, strerror(errno));
        else
            INFO("%s: %s", filename, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int io_exec_and_connect_unix(const char *filename, const char *program_path,
        const char *basedir)
{
    char *expanded_filename = tilde_expand_path(filename);
    int fd = io_connect_unix(expanded_filename);
    if(fd == -1)
    {
        if(program_path)
	    sp_exec(program_path, basedir);

        int try;
        for(try = 0; try < 40; try++)
        {
            fd = io_connect_unix(expanded_filename);
            if(fd != -1)
                break;
            usleep(500000);
        }
    }
    free(expanded_filename);

    if(fd == -1)
    {
        WARNING("Failed to connect to '%s' for program '%s'",
                filename, program_path);
    }

    return fd;
}

int io_set_blocking(int fd, int flag)
{
    int flags = fcntl(fd, F_GETFL, NULL);
    if(flag)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    if(fcntl(fd, F_SETFL, flags) == -1)
    {
        WARNING("failed to set channel blocking mode: %s", strerror(errno));
        return -1;
    }

    return 0;
}

char *io_evbuffer_readline(struct evbuffer *buffer)
{
    char *data = (char *)EVBUFFER_DATA(buffer);
    char *start;
    size_t len = EVBUFFER_LENGTH(buffer);
    char *line;
    int i, n;

    /* FIXME: '|' as end-of-line character is nmdc-specific */
    for(i = n = 0; data[i] == '|' && i < len; i++)
        /* skip leading '|'s */ n++;
    start = data + i;
    for(; i < len; i++)
    {
        if(data[i] == '|')
            break;
    }

    if (i == len)
        return NULL;

    if((line = malloc(i - n + 1)) == NULL)
    {
        WARNING("%s: out of memory\n", __func__);
        evbuffer_drain(buffer, i);
        return (NULL);
    }

    memcpy(line, start, i - n);
    line[i - n] = '\0';

    evbuffer_drain(buffer, i + 1);

    return line;
}

#ifdef TEST

#include "unit_test.h"

int main(void)
{
	sp_log_set_level("debug");

	xerr_t *err = NULL;
	struct sockaddr_in *addr = io_lookup(":28589", &err);
	fail_unless(addr == NULL);

	return 0;
}

#endif

