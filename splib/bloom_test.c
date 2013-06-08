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

#include "bloom.h"

#include "unit_test.h"

/* These aren't declared in bloom.h because they should really be static
 * within bloom.c, but put here so we don't have to re-compile bloom.c for the
 * tests.
 */
void bloom_set_bit(bloom_t *bloom, unsigned bit);
int bloom_get_bit(bloom_t *bloom, unsigned bit);

int main(void)
{
    bloom_t *b = bloom_create(2);
    fail_unless(b->length == 2);

    bloom_set_bit(b, 0);
    bloom_set_bit(b, 3);
    bloom_set_bit(b, 7);
    bloom_set_bit(b, 8);
    bloom_set_bit(b, 15);

    fail_unless(b->filter[0] == 137);
    fail_unless(b->filter[1] == 129);

    fail_unless(bloom_filled_bits(b) == 5);
    fail_unless(bloom_filled_percent(b) == (100*5.0)/16);

    fail_unless(bloom_get_bit(b, 0) == 1);
    fail_unless(bloom_get_bit(b, 1) == 0);
    fail_unless(bloom_get_bit(b, 2) == 0);
    fail_unless(bloom_get_bit(b, 3) == 1);
    fail_unless(bloom_get_bit(b, 4) == 0);
    fail_unless(bloom_get_bit(b, 5) == 0);
    fail_unless(bloom_get_bit(b, 6) == 0);
    fail_unless(bloom_get_bit(b, 7) == 1);

    fail_unless(bloom_get_bit(b, 8) == 1);
    fail_unless(bloom_get_bit(b, 9) == 0);
    fail_unless(bloom_get_bit(b, 10) == 0);
    fail_unless(bloom_get_bit(b, 11) == 0);
    fail_unless(bloom_get_bit(b, 12) == 0);
    fail_unless(bloom_get_bit(b, 13) == 0);
    fail_unless(bloom_get_bit(b, 14) == 0);
    fail_unless(bloom_get_bit(b, 15) == 1);

    bloom_free(b);

    b = bloom_create(128);
    fail_unless(b->length == 128);

    fail_unless(bloom_filled_percent(b) == 0.0);
    fail_unless(bloom_filled_bits(b) == 0);

    /* this assumes that BLOOM_MINLENGTH > 3 */
    bloom_add_filename(b, "foo");
    int i;
    for(i = 0; i < b->length; i++)
        fail_unless(b->filter[i] == 0);
    fail_unless(bloom_filled_percent(b) == 0.0);
    fail_unless(bloom_filled_bits(b) == 0);

    /* this assumes that BLOOM_MINLENGTH > 3 */
    bloom_add_filename(b, "foo.bar(baz) [gaz]onk");
    for(i = 0; i < b->length; i++)
        fail_unless(b->filter[i] == 0);
    fail_unless(bloom_filled_percent(b) == 0.0);
    fail_unless(bloom_filled_bits(b) == 0);

    char *filename = "foobarbaz";
    bloom_add_filename(b, filename);
    fail_unless(bloom_filled_bits(b) > 0);
    unsigned min_bits_set = (strlen(filename) - BLOOM_MINLENGTH + 1) * 5;
    fail_unless(bloom_filled_bits(b) <= min_bits_set);

    fail_unless(bloom_check_key(b, "foob") == 0);
    fail_unless(bloom_check_key(b, "ooba") == 0);
    fail_unless(bloom_check_key(b, "obar") == 0);
    fail_unless(bloom_check_key(b, "barb") == 0);
    fail_unless(bloom_check_key(b, "arba") == 0);
    fail_unless(bloom_check_key(b, "rbaz") == 0);

    fail_unless(bloom_check_key(b, "fooba") == 0);
    fail_unless(bloom_check_key(b, "oobar") == 0);
    fail_unless(bloom_check_key(b, "obarb") == 0);
    fail_unless(bloom_check_key(b, "barba") == 0);
    fail_unless(bloom_check_key(b, "arbaz") == 0);

    fail_unless(bloom_check_key(b, "foobar") == 0);
    fail_unless(bloom_check_key(b, "oobarb") == 0);
    fail_unless(bloom_check_key(b, "obarba") == 0);
    fail_unless(bloom_check_key(b, "barbaz") == 0);

    fail_unless(bloom_check_key(b, "foobarb") == 0);
    fail_unless(bloom_check_key(b, "oobarba") == 0);
    fail_unless(bloom_check_key(b, "obarbaz") == 0);

    fail_unless(bloom_check_key(b, "foobarba") == 0);
    fail_unless(bloom_check_key(b, "oobarbaz") == 0);

    fail_unless(bloom_check_key(b, "foobarbaz") == 0);

    bloom_free(b);

    /* test with filename in UTF-8 */
    b = bloom_create(128);
    filename = "fööbär";
    bloom_add_filename(b, filename);
    fail_unless(bloom_filled_bits(b) > 0);
    min_bits_set = (strlen(filename) - BLOOM_MINLENGTH + 1) * 5;
    fail_unless(bloom_filled_bits(b) <= min_bits_set);

    fail_unless(bloom_check_key(b, "fööb") == 0);
    fail_unless(bloom_check_key(b, "ööbä") == 0);
    fail_unless(bloom_check_key(b, "öbär") == 0);

    fail_unless(bloom_check_key(b, "fööbä") == 0);
    fail_unless(bloom_check_key(b, "ööbär") == 0);

    fail_unless(bloom_check_key(b, "fööbär") == 0);
    fail_unless(bloom_check_key(b, "FÖÖBÄR") == 0);
    fail_unless(bloom_check_key(b, "fÖöBÄr") == 0);

    fail_unless(bloom_check_key(b, "gazonk") != 0);

    bloom_free(b);

    return 0;
}

