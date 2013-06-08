/*
 * Copyright 2004-2005 Martin Hedenfalk <martin@bzero.se>
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
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "spclient.h"
#include "util.h"
#include "io.h"
#include "rx.h"
#include "xerr.h"
#include "log.h"

/* FIXME: must filter out or quote $ and | characters (or perhaps better: use
 * shell-like syntax with space delimiting arguments and real quoting)
 */

static int sp_attach_io_channel(sp_t *sp, int fd)
{
    return_val_if_fail(sp, -1);
    return_val_if_fail(fd != -1, -1);

    sp->fd = fd;
    sp->input = evbuffer_new();
    io_set_blocking(fd, 0);

    return 0;
}

int sp_connect(sp_t *sp, const char *working_directory, const char *executable_path)
{
    return_val_if_fail(sp, -1);
    return_val_if_fail(working_directory, -1);

    char *socket_filename;
    int num_returned_bytes = asprintf(&socket_filename, "%s/sphubd", working_directory);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    int fd = io_exec_and_connect_unix(socket_filename, executable_path, working_directory);
    free(socket_filename);

    return sp_attach_io_channel(sp, fd);
}

int sp_connect_remote(sp_t *sp, const char *remote_address)
{
    xerr_t *err = NULL;
    struct sockaddr_in *addr = io_lookup(remote_address, &err);
    if(addr == NULL)
    {
        WARNING("unable to lookup %s: %s\n", remote_address, xerr_msg(err));
        xerr_free(err);
        return -1;
    }
    int fd = io_connect_tcp(addr, &err);
    if(fd == -1)
    {
        WARNING("%s", xerr_msg(err));
        xerr_free(err);
        return -1;
    }
    return sp_attach_io_channel(sp, fd);
}

int sp_disconnect(sp_t *sp)
{
    return_val_if_fail(sp, -1);

    if(sp->fd != -1)
    {
        DEBUG("closing fd %d", sp->fd);
        close(sp->fd);
        sp->fd = -1;
    }

    return 0;
}

sp_t *sp_create(void *user_data)
{
    sp_t *sp = sp_init();
    sp->user_data = user_data;

    return sp;
}

void sp_free(sp_t *sp)
{
    if(sp)
    {
        sp_disconnect(sp);
        evbuffer_free(sp->input);
        free(sp);
    }
}

int sp_in_event(int fd, short why, void *data)
{
    sp_t *sp = data;

    int rc = evbuffer_read(sp->input, fd, 4096);
    if(rc == -1 || rc == 0 /* EOF */)
    {
        return -1;
    }

    while(1)
    {
        char *cmd = io_evbuffer_readline(sp->input);
        if(cmd == NULL)
        {
            break;
        }
        print_command(cmd, "<- (fd %d)", fd);
        sp_dispatch_command(cmd, "$", 1, sp);
        free(cmd);
    }

    return 0;
}

int sp_send_command(sp_t *sp, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    char *cmd;
    int num_returned_bytes = vasprintf(&cmd, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    rc = sp_send_string(sp, cmd);
    free(cmd);
    va_end(ap);

    return rc;
}

