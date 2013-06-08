/*
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
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

#ifndef _log_h_
#define _log_h_

#include <sys/types.h>
#include <stdarg.h>
#include <unistd.h>

enum
{
  LOG_LEVEL_ERROR             = 1 << 2,
  LOG_LEVEL_CRITICAL          = 1 << 3,
  LOG_LEVEL_WARNING           = 1 << 4,
  LOG_LEVEL_INFO              = 1 << 5,
  LOG_LEVEL_DEBUG             = 1 << 6,
};

int sp_log_init(const char *workdir, const char *prefix);
void sp_log_set_level(const char *level);
const char *sp_log_get_level(void);
void sp_log_close(void);
void sp_vlog(int level, const char *fmt, va_list ap);
void sp_log(int level, const char *fmt, ...);

#undef g_debug
#define g_debug(fmt, ...)  sp_log(LOG_LEVEL_DEBUG, "[%d] (%s:%i) " fmt, getpid(), __func__, __LINE__, ## __VA_ARGS__)

#undef g_info
#define g_info(fmt, ...)  sp_log(LOG_LEVEL_INFO, "[%d] (%s:%i) " fmt, getpid(), __func__, __LINE__, ## __VA_ARGS__)

#undef g_warning
#define g_warning(fmt, ...)  sp_log(LOG_LEVEL_WARNING, "[%d] (%s:%i) " fmt, getpid(), __func__, __LINE__, ## __VA_ARGS__)

#undef g_error
#define g_error(fmt, ...)  sp_log(LOG_LEVEL_ERROR, "[%d] (%s:%i) " fmt, getpid(), __func__, __LINE__, ## __VA_ARGS__)

#define DEBUG g_debug
#define INFO g_info
#define WARNING g_warning
#define ERROR g_error
#define g_message g_info

#define return_val_if_fail(cond, retval) do {           \
        if(!(cond)) { WARNING("assert failed: " #cond); \
        return (retval); }                                \
    } while(0)

#define return_if_fail(cond) do {                       \
        if(!(cond)) { WARNING("assert failed: " #cond); \
        return; }                                        \
    } while(0)

#endif

