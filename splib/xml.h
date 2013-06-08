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

#ifndef _xml_h_
#define _xml_h_

#include <expat.h>
#include <stdio.h>
#include <stdbool.h>

#include "xerr.h"

typedef void (*xml_open_func_t)(void *data, const char *el, const char **attr);
typedef void (*xml_close_func_t)(void *data, const char *el);

typedef struct xml_ctx xml_ctx_t;
struct xml_ctx
{
    FILE *fp;
    char buf[1024];
    size_t len;
    bool eof;
    
    char *encoding;
    void *user_data;

    XML_Parser parser;
    xml_open_func_t open_func;
    xml_close_func_t close_func;
};

xml_ctx_t *xml_init_fp(FILE *fp,
        xml_open_func_t open_func,
        xml_close_func_t close_func,
        void *user_data);

int xml_parse_chunk(xml_ctx_t *ctx, xerr_t **err);
void xml_ctx_free(xml_ctx_t *ctx);

#endif

