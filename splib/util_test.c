/*
 * Copyright 2005-2006 Martin Hedenfalk <martin@bzero.se>
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

#include <stdio.h>

#include "util.h"
#include "unit_test.h"

int main(void)
{
    /*
     * split_host_port
     */
    char *host = 0;
    int port = -1;
    fail_unless( split_host_port("host:17", &host, &port) == 0);
    fail_unless(strcmp(host, "host") == 0);
    fail_unless(port == 17);
    free(host);

    host = 0;
    port = 17;
    fail_unless( split_host_port("", &host, &port) == -1);
    fail_unless( split_host_port("foo:bar", &host, &port) == 0);
    fail_unless(strcmp(host, "foo") == 0);
    fail_unless(port == -1);

    fail_unless( split_host_port("foo:42", NULL, &port) == -1);
    fail_unless( split_host_port("foo:42", &host, NULL) == -1);
    fail_unless( split_host_port("foo:42", NULL, NULL) == -1);
    fail_unless( split_host_port(NULL, &host, &port) == -1);

    fail_unless( split_host_port(":28589", &host, &port) == -1);

    /*
     * str_find_word_start
     */
    char *s = "word start";
    fail_unless(str_find_word_start(s, s + 7, " ") == s + 5);

    s = "word start";
    fail_unless(str_find_word_start(s, s + 5, " ") == s + 5);

    s = "word start";
    fail_unless(str_find_word_start(s, s + 2, " ") == s);

    s = "word\\ start";
    fail_unless(str_find_word_start(s, s + 7, " ") == s);

    s = "\\ word\\ start";
    fail_unless(str_find_word_start(s, s + 5, " ") == s);

    s = "";
    fail_unless(str_find_word_start(s, s, " ") == s);

    s = "word ";
    fail_unless(str_find_word_start(s, s + 5, " ") == s + 5);

    s = "word ";
    fail_unless(str_find_word_start(s, s + 4, " ") == s);

    /*
     * valid_tth
     */
    fail_unless(valid_tth(NULL) == 0);
    fail_unless(valid_tth("NOT A VALID TTH") == 0);
    fail_unless(valid_tth("QBVGSER2GIH34DOZ4WDWU5QOCZIS7RLHAA5NAPI") == 1);
    fail_unless(valid_tth("QBVGSER2GIH34DOZ4WDWU5QOCZIS7RLHAA5NAPI=") == 1);
    fail_unless(valid_tth("QBVGSER2GIH34DOZ4WDWU5QOCZIS7RLHAA5NAP") == 0);

    /*
     * str_size_human
     */

    fail_unless(strcmp(str_size_human(1024), "1.0 KiB") == 0);
    asprintf(&s, "%s %s %s", str_size_human(1024*1024), str_size_human(2*1024*1024), str_size_human(1.5*1024));
    printf("s = %s\n", s);
    fail_unless(strcmp(s, "1.0 MiB 2.0 MiB 1.5 KiB") == 0);
    fail_unless(strcmp(str_size_human(1024), str_size_human(2048)) == -1);
    free(s);

    return 0;
}

