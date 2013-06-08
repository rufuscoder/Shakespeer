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

#include "tigertree.h"
#include "unit_test.h"

#define HASHER_BUFSIZ 256*1024

int main(void)
{
    struct tt_context tth;
    char *buf = "[ABCDEFGHIJKLMNOPQRSTYVWXYZabcdefghijklmnopqrstuvqzyx1234567890]\n";

    tt_init(&tth, 0);
    tt_update(&tth, (unsigned char *)buf, strlen(buf));
    tt_digest(&tth, NULL);

    char *hash_base32 = tt_base32(&tth);
    fail_unless(strcmp(hash_base32, "UUP2CKMGSUCSKXBQKSK7U76YVYFPUDXFNCYEOFI") == 0);

    return 0;
}

