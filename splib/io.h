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

#ifndef _io_h_
#define _io_h_

#include <netdb.h>

#include <sys/time.h>
#include <sys/types.h>
#include <event.h>

#include "xerr.h"

struct sockaddr_in *io_lookup(const char *hostport, xerr_t **err);
int io_connect_async(struct sockaddr_in *remote_addr,
        void (*event_func)(int fd, int error, void *user_data),
        void *user_data,
        xerr_t **err);
int io_accept_connection_addr(int fd, char **ip_address);
int io_accept_connection(int fd);
int io_connect_tcp(struct sockaddr_in *remote_addr, xerr_t **err);
int io_connect_unix(const char *filename);
int io_exec_and_connect_unix(const char *filename, const char *program_path,
        const char *basedir);
int io_bind_unix_socket(const char *filename);
int io_bind_tcp_socket(int port, xerr_t **err);
int io_set_blocking(int fd, int flag);
char *io_evbuffer_readline(struct evbuffer *buffer);

#endif

