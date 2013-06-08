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

#ifndef _bloom_h_
#define _bloom_h_

#define BLOOM_MINLENGTH 4

typedef struct bloom bloom_t;
struct bloom
{
    unsigned length; /* length in bytes of the filter */
    unsigned char *filter;
    unsigned collisions;
};

bloom_t *bloom_create(unsigned length);
void bloom_free(bloom_t *bloom);
void bloom_add_key(bloom_t *bloom, const char *key);
void bloom_add_filename(bloom_t *bloom, const char *filename);
int bloom_check_key(bloom_t *bloom, const char *key);
int bloom_check_filename(bloom_t *bloom, const char *filename);
void bloom_reset(bloom_t *bloom);
void bloom_merge(bloom_t *dest, bloom_t *src);
float bloom_filled_percent(bloom_t *bloom);
unsigned bloom_filled_bits(bloom_t *bloom);

#endif

