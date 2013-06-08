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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "nfkc.h"

#include "bloom.h"
#include "args.h"
#include "tiger.h"

/* Implementation of a Bloom Filter. See
 * http://www.perl.com/pub/a/2004/04/08/bloom_filters.html for a good
 * explanation.
 */

/* Create a new bloom filter of length length (in bytes).
 */
bloom_t *bloom_create(unsigned length)
{
    bloom_t *bloom;

    assert(length > 0);

    bloom = malloc(sizeof(bloom_t));
    bloom->length = length;
    bloom->filter = (unsigned char *)malloc(bloom->length);

    bloom_reset(bloom);

    return bloom;
}

void bloom_free(bloom_t *bloom)
{
    if(bloom)
    {
        free(bloom->filter);
        free(bloom);
    }
}

void bloom_set_bit(bloom_t *bloom, unsigned bit)
{
    assert(bloom);
    unsigned offset = bit >> 3;
    unsigned mask = 1 << (bit & 7);
    assert(offset < bloom->length);
    bloom->collisions += ((bloom->filter[offset] & mask) == mask);
    bloom->filter[offset] |= mask;
}

int bloom_get_bit(bloom_t *bloom, unsigned bit)
{
    assert(bloom);
    unsigned offset = bit >> 3;
    unsigned mask = 1 << (bit & 7);
    assert(offset < bloom->length);
    return (bloom->filter[offset] & mask) == mask;
}

/* Return an array of 5 different hashes of the same key. Uses tiger hashing
 * from tiger.c. A tiger hash is 24 bytes; we split it in 6 parts, each one
 * consisting of 32 bits. Each 32 bits part is treated as an integer, which is
 * wrapped around the FILTER_LENGTH, and finally used as index into the filter
 * array.
 */
static void bloom_hash_key(bloom_t *bloom, const char *key,
        unsigned *h1, unsigned *h2, unsigned *h3, unsigned *h4, unsigned *h5)
{
    assert(bloom);
    assert(key);

    word64 res[3];
    tiger((word64 *)key, strlen(key), res);

    if(h1) *h1 = *(unsigned *)((unsigned char *)res+0) % (bloom->length * 8);
    if(h2) *h2 = *(unsigned *)((unsigned char *)res+4) % (bloom->length * 8);
    if(h3) *h3 = *(unsigned *)((unsigned char *)res+8) % (bloom->length * 8);
    if(h4) *h4 = *(unsigned *)((unsigned char *)res+12) % (bloom->length * 8);
    if(h5) *h5 = *(unsigned *)((unsigned char *)res+16) % (bloom->length * 8);
}

typedef int (*subkey_function_t)(bloom_t *bloom,
        unsigned h1, unsigned h2, unsigned h3, unsigned h4, unsigned h5);

/*
 * Calls the subkey_function for each overlapped segment of BLOOM_MINLENGTH
 * characters in length. Breaks the loop and returns if the subkey_function
 * returns non-zero.
 */
static int bloom_iterate_key(bloom_t *bloom, const char *key,
        subkey_function_t func)
{
    assert(bloom);
    assert(key);
    assert(func);

    int len = g_utf8_strlen(key, -1); /* length in characters, not bytes */
    if(len >= BLOOM_MINLENGTH)
    {
        len -= BLOOM_MINLENGTH;

        int i;
        const char *p = key;
        for(i = 0; i <= len; i++)
        {
            char *np = g_utf8_offset_to_pointer(p, BLOOM_MINLENGTH);
            char *xkey = g_utf8_casefold(p, np - p);

            unsigned h1, h2, h3, h4, h5;
            bloom_hash_key(bloom, xkey, &h1, &h2, &h3, &h4, &h5);

            int rc = func(bloom, h1, h2, h3, h4, h5);
            free(xkey);
            if(rc != 0)
                return rc;

            p = g_utf8_next_char(p);
        }
    }

    return 0;
}

static int bloom_add_key_callback(bloom_t *bloom,
        unsigned h1, unsigned h2, unsigned h3, unsigned h4, unsigned h5)
{
    bloom_set_bit(bloom, h1);
    bloom_set_bit(bloom, h2);
    bloom_set_bit(bloom, h3);
    bloom_set_bit(bloom, h4);
    bloom_set_bit(bloom, h5);

    return 0;
}

/*
 * Add a key (minimum BLOOM_MINLENGTH characters) to the filter array. For
 * each (overlapped) BLOOM_MINLENGTH characters part of the filename, we
 * calculate all 5 different indexes/hashes and sets the corresponding bits in
 * the filter array.
 */
void bloom_add_key(bloom_t *bloom, const char *key)
{
    bloom_iterate_key(bloom, key, bloom_add_key_callback);
}

void bloom_add_filename(bloom_t *bloom, const char *filename)
{
    assert(bloom);
    assert(filename);

    arg_t *subkeys;
    int i;

    subkeys = arg_create(filename, "$.-_()[]{} ", 0);
    
    for(i = 0; i < subkeys->argc; i++)
        bloom_add_key(bloom, subkeys->argv[i]);

    arg_free(subkeys);
}

static int bloom_check_key_callback(bloom_t *bloom,
        unsigned h1, unsigned h2, unsigned h3, unsigned h4, unsigned h5)
{
    if(bloom_get_bit(bloom, h1) == 1 &&
       bloom_get_bit(bloom, h2) == 1 &&
       bloom_get_bit(bloom, h3) == 1 &&
       bloom_get_bit(bloom, h4) == 1 &&
       bloom_get_bit(bloom, h5) == 1)
    {
        return 0;
    }
    return -1;
}

/* Same as bloom_add_key, but checks the bits instead of setting them.
 */
int bloom_check_key(bloom_t *bloom, const char *key)
{
    return bloom_iterate_key(bloom, key, bloom_check_key_callback);
}

int bloom_check_filename(bloom_t *bloom, const char *filename)
{
    assert(bloom);
    assert(filename);

    arg_t *subkeys;
    int i;

    subkeys = arg_create(filename, "$.-_()[]{} ", 0);

    for(i = 0; i < subkeys->argc; i++)
    {
        if(bloom_check_key(bloom, subkeys->argv[i]) != 0)
            break;
    }

    int rc = (i == subkeys->argc ? 0 : -1);
    arg_free(subkeys);

    return rc;
}

void bloom_reset(bloom_t *bloom)
{
    assert(bloom);

    memset(bloom->filter, 0, bloom->length);
}

unsigned bloom_filled_bits(bloom_t *bloom)
{
    assert(bloom);

    unsigned tot = 0;
    unsigned i, j;

    for(i = 0; i < bloom->length; i++)
    {
        for(j = 0; j < 8; j++)
        {
            if((bloom->filter[i] & (1 << j)) == (1 << j))
                tot++;
        }
    }

    return tot;
}

float bloom_filled_percent(bloom_t *bloom)
{
    assert(bloom);

    unsigned tot = bloom_filled_bits(bloom);
    float percent = ((float)tot * 100) / (bloom->length * 8);
    return percent;
}

/* Merges two bloom filters. dest->lenght and src->length must be equal.
 */
void bloom_merge(bloom_t *dest, bloom_t *src)
{
    assert(dest);
    assert(src);

    int i, j;

    assert(dest->length == src->length);

    for(i = 0; i < src->length; i++)
    {
        for(j = 0; j < 8; j++)
        {
            if((src->filter[i] & (1 << j)) == (1 << j))
                dest->filter[i] |= (1 << j);
        }
    }
}

