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

#ifndef _ui_h_
#define _ui_h_

#include <stdarg.h>

#include "hub.h"
#include "ui_cmd.h"
#include "ui_send.h"

void ui_list_init(void);

void ui_add(ui_t *ui);
ui_t *ui_find_by_fd(int fd);
void ui_free(ui_t *ui);
void ui_remove(ui_t *ui);
void ui_close_connection(ui_t *ui);
void ui_close_all_connections(void);
void ui_send_share_stats_for_root(ui_t *ui, const char *local_root);
void ui_foreach(void (*func)(ui_t *, void *), void *user_data);
void ui_accept_connection(int fd, short why, void *data);
int ui_send_string(ui_t *ui, const char *string);
int ui_send_command(ui_t *ui, const char *fmt, ...)
    __attribute__ (( format(printf, 2, 3) ));

void ui_schedule_share_stats_update(void);

#endif

