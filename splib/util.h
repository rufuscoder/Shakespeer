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

#ifndef _util_h_
#define _util_h_

#include <sys/types.h> /* for size_t */
#include <stdint.h>

#define FILELIST_NONE 0
#define FILELIST_DCLST 1
#define FILELIST_XML 2

typedef void (*sp_shutdown_func_t)(void);

typedef enum share_type
{
    SHARE_TYPE_ANY = 1,
    SHARE_TYPE_AUDIO = 2,
    SHARE_TYPE_COMPRESSED = 3,
    SHARE_TYPE_DOCUMENT = 4,
    SHARE_TYPE_EXECUTABLE = 5,
    SHARE_TYPE_IMAGE = 6,
    SHARE_TYPE_MOVIE = 7,
    SHARE_TYPE_DIRECTORY = 8,
    SHARE_TYPE_TTH = 9
} share_type_t;

typedef enum share_size_restriction
{
    SHARE_SIZE_NONE,
    SHARE_SIZE_MIN,
    SHARE_SIZE_MAX,
    SHARE_SIZE_EQUAL /* special case, not supported by DC protocol */
} share_size_restriction_t;

char *str_size_human(uint64_t size);
void print_command(const char *command, const char *fmt, ...)
    __attribute__ (( format(printf, 2, 3) ));
char *tilde_expand_path(const char *path);
char *absolute_path(const char *path);
char *get_working_directory(void);
char *verify_working_directory(const char *basedir);
char *detect_external_ip(const char *address);
const char *str_find_word_start(const char *string, const char *pos, const char *delimiters);
void set_corelimit(int enabled);
pid_t sp_get_pid(const char *workdir, const char *appname);
int sp_write_pid(const char *workdir, const char *appname);
void sp_remove_pid(const char *workdir, const char *appname);
int sp_daemonize(void);
int valid_tth(const char *tth);
share_type_t share_filetype(const char *filename);
char *str_shorten_path(const char *path, int maxlen);
char *find_filelist(const char *working_directory, const char *nick);
int is_filelist(const char *filename);
int split_host_port(const char *hostport, char **host, int *port);

int mkpath(const char *path);
pid_t sp_exec(const char *path, const char *basedir);
char *get_exec_path(const char *argv0);

#endif

