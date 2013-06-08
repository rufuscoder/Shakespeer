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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "xstr.h"
#include "extra_slots.h"
#include "rx.h"
#include "log.h"
#include "user.h"
#include "hub.h"

/* All strings assumed to be in UTF-8 already
 */
user_t *user_new(const char *nick, const char *tag,
        const char *speed, const char *description,
        const char *email, uint64_t shared_size, hub_t *hub)
{
    return_val_if_fail(nick, NULL);

    user_t *user = calloc(1, sizeof(user_t));

    user->nick = strdup(nick);
    if(user->nick == NULL)
    {
        free(user);
        return NULL;
    }

    user->tag = xstrdup(tag);

    user->speed = xstrdup(speed);
    user->description = xstrdup(description);

    if(email && email[0])
    {
        user->email = strdup(email);
    }

    user->shared_size = shared_size;
    user->hub = hub;
    user->passive = false;
    user->is_operator = false;

    user->extra_slots = extra_slots_get_for_user(nick);

    return user;
}

user_t *user_new_from_myinfo(const char *myinfo, hub_t *hub)
{
    if(str_has_prefix(myinfo, "$MyINFO "))
        myinfo += 8;

    if(strncmp(myinfo, "$ALL ", 5) != 0)
        return NULL;
    myinfo += 5;

    char *nick = NULL;
    char *speed = NULL;
    char *email = NULL;
    char *tag = NULL;
    char *desc = NULL;
    char *size_str = NULL;
    uint64_t size = 0ULL;

    size_t n = strcspn(myinfo, " ");
    if(n == 0 || myinfo[n] != ' ')
        goto error;
    nick = xstrndup(myinfo, n);
    myinfo += n + 1;

    n = strcspn(myinfo, "$");
    if(myinfo[n] != '$')
        goto error;
    desc = xstrndup(myinfo, n);
    myinfo += n + 1;
    myinfo += strspn(myinfo, "AP \x05");
    if(*myinfo != '$')
        goto error;
    myinfo++;

    char *e = desc ? strrchr(desc, '<') : NULL;
    if(e)
    {
        if(e == desc)
        {
            tag = desc;
            desc = NULL;
        }
        else
        {
            tag = strdup(e);
            *e = 0; /* truncate description: remove tag */
        }
    }

    n = strcspn(myinfo, "$\x01");
    speed = xstrndup(myinfo, n);
    str_trim_end_inplace(speed, " ");
    myinfo += n;
    myinfo += strspn(myinfo, " \x01");

    if(*myinfo != '$')
        goto error;
    myinfo++;

    n = strcspn(myinfo, "$");
    email = xstrndup(myinfo, n);
    myinfo += n + 1;

    n = strcspn(myinfo, "$");
    size_str = xstrndup(myinfo, n);
    size = strtoull(size_str, NULL, 10);

    user_t *user = user_new(nick, tag, speed, desc, email, size, hub);
    free(nick);
    free(speed);
    free(email);
    free(tag);
    free(desc);
    free(size_str);

    return user;

error:
    free(nick);
    free(speed);
    free(email);
    free(tag);
    free(desc);
    free(size_str);
    return NULL;
}

void user_free(void *data)
{
    user_t *user = data;
    if(user)
    {
        free(user->nick);
        free(user->tag);
        free(user->speed);
        free(user->description);
        free(user->email);
        free(user->ip);
        free(user);
    }
}

void user_set_ip(user_t *user, const char *ip)
{
    return_if_fail(user);
    free(user->ip);
    user->ip = xstrdup(ip);
}

void user_set_speed(user_t *user, const char *speed)
{
    return_if_fail(user);
    free(user->speed);
    user->speed = xstrdup(speed);
}

void user_set_description(user_t *user, const char *description)
{
    return_if_fail(user);
    free(user->description);
    user->description = xstrdup(description);
}

void user_set_email(user_t *user, const char *email)
{
    return_if_fail(user);
    free(user->email);
    user->email = xstrdup(email);
}

