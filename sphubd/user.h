/*
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
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

#ifndef _user_h_
#define _user_h_

#include "sys_queue.h"
#include <stdint.h>
#include <stdbool.h>

struct hub;

typedef struct user user_t;
struct user
{
    LIST_ENTRY(user) link;

    char *nick;
    char *tag;
    char *speed;
    char *description;
    char *email;
    uint64_t shared_size;
    bool is_operator;
    bool passive;
    struct hub *hub;
    char *ip;
    unsigned int extra_slots;
};

user_t *user_new(const char *nick, const char *tag, const char *speed, const char *description,
        const char *email, uint64_t shared_size, struct hub *hub);
user_t *user_new_from_myinfo(const char *myinfo, struct hub *hub);
void user_free(void *data);
void user_set_ip(user_t *user, const char *ip);
void user_set_speed(user_t *user, const char *speed);
void user_set_description(user_t *user, const char *description);
void user_set_email(user_t *user, const char *email);

#endif

