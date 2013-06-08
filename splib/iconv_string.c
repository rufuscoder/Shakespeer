/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>
#include <stdio.h>

#include "log.h"

char *iconv_string_full(const char *string, ssize_t length,
                        const char *src_encoding,
                        const char *dst_encoding,
                        size_t *src_used_bytes_p, size_t *dst_length_p,
                        int replacement_char)
{
    iconv_t cd;

    if(string == NULL)
        return NULL;

    if(length < 0)
        length = strlen(string);
    if(length == 0)
        return NULL;

    if(strcmp(src_encoding, dst_encoding) == 0)
        return strdup(string);

    cd = iconv_open(dst_encoding, src_encoding);
    if(cd == (iconv_t)-1)
    {
        WARNING("failed to open iconv: src=[%s], dst=[%s]", src_encoding, dst_encoding);
        return NULL;
    }

    size_t dst_length = length * 2; /* just a guess */
    char *dst = malloc(dst_length + 1);

    char *wstring = (char *)string;
    if(replacement_char > 0)
        wstring = strdup(string);

    ICONV_CONST char *inp = (ICONV_CONST char *)wstring;
    char *outp = dst;

    size_t inbytesleft = length;
    size_t outbytesleft = dst_length;

    while(1)
    {
        errno = 0;
        size_t rc = iconv(cd, &inp, &inbytesleft, &outp, &outbytesleft);

        if(rc == (size_t)-1)
        {
            switch(errno)
            {
                case EILSEQ:
                    /* inp points to the beginning of an invalid sequence */
                    if(replacement_char > 0)
                    {
                        *(char *)inp = (char)replacement_char;
                        /* try again */
                        break;
                    }
                    /* FALLTHROUGH */

                case EINVAL:
                    /* incomplete byte sequence encountered */
                default:

                    free(dst);
                    dst = NULL;
                    break;

                case E2BIG:
                    /* The output buffer has not enough space. */
                    /* Need to expand dst and try again. */
                    dst_length += length;

                    dst = realloc(dst, dst_length + 1);

                    size_t used = outp - dst;

                    outbytesleft = dst_length - used;
                    outp = dst + used;

                    break;
            }

            if(dst == NULL)
                break;
        }
        else
        {
            /* success */
            *outp = 0; /* zero-terminate result */
            break;
        }

    }

    if(src_used_bytes_p)
        *src_used_bytes_p = inp - wstring;

    if(replacement_char > 0)
        free(wstring);

    if(dst_length_p)
    {
        if(dst)
            *dst_length_p = dst_length - outbytesleft;
        else
            *dst_length_p = 0;
    }

    int save_errno = errno;
    errno = 0;
    if(iconv_close(cd) == 0 && save_errno != 0)
        errno = save_errno;

    return dst;
}

char *iconv_string(const char *string, ssize_t length,
                        const char *src_encoding,
                        const char *dst_encoding)
{
    return iconv_string_full(string, length, src_encoding, dst_encoding,
            NULL, NULL, -1);
}

char *iconv_string_lossy(const char *string, ssize_t length,
                        const char *src_encoding,
                        const char *dst_encoding)
{
    return iconv_string_full(string, length, src_encoding, dst_encoding,
            NULL, NULL, '?');
}

char *iconv_string_escaped(const char *string, ssize_t length,
                           const char *src_encoding,
                           const char *dst_encoding)
{
    /* FIXME: implement iconv_string_escaped correctly! */
    return iconv_string_full(string, length, src_encoding, dst_encoding,
            NULL, NULL, '?');
}

