/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _xstr_h_
#define _xstr_h_

char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t len);
const char *xstrnchr(const char *s, unsigned maxlen, int c);

char *str_trim_end_inplace(char *str, const char *set);
char *str_trim_end(const char *str, const char *set);
int str_has_prefix(const char *s, const char *prefix);
int str_has_suffix(const char *s, const char *suffix);
char *str_replace_set(char *string, const char *set, char replacement);

#if defined(linux)
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
char *strcasestr(const char *big, const char *little);
#endif

char *q_strsep(char **p, const char *set);

#endif

