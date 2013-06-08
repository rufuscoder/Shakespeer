/*
 * Copyright (c) 2005-2007 Martin Hedenfalk <martin@bzero.se>
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

#ifndef _fl_h_
#define _fl_h_

#include "sys_queue.h"

#include <stdio.h>

#include "xml.h"
#include "util.h"
#include "xerr.h"

struct fl_file;

typedef struct fl_dir fl_dir_t;
struct fl_dir
{
    LIST_ENTRY(fl_dir) link;

    TAILQ_HEAD(fl_file_list, fl_file) files;
    char *path;
    unsigned nfiles;
    uint64_t size;
};

typedef struct fl_file fl_file_t;
struct fl_file
{
    TAILQ_ENTRY(fl_file) link;
    
    char *name;
    share_type_t type;
    uint64_t size;
    char *tth;

    fl_dir_t *dir;
};

fl_dir_t *fl_parse(const char *filename, xerr_t **err);

typedef void (*fl_xml_file_callback_t)(const char *path, const char *tth,
        uint64_t size, void *user_data);

typedef struct fl_xml_ctx fl_xml_ctx_t;
struct fl_xml_ctx
{
    LIST_HEAD(, fl_dir) dir_stack;
    fl_dir_t *root;
    FILE *fp;
    void *user_data;
    fl_xml_file_callback_t file_callback;
    xml_ctx_t *xml;
};

int fl_parse_xml_chunk(fl_xml_ctx_t *ctx);
fl_xml_ctx_t *fl_xml_prepare_file(const char *filename,
        fl_xml_file_callback_t file_callback, void *user_data);
void fl_xml_free_context(fl_xml_ctx_t *ctx);
fl_dir_t *fl_parse_xml(const char *filename);

fl_dir_t *fl_parse_dclst(const char *filename);
void fl_sort_recursive(fl_dir_t *dir);
void fl_free_dir(fl_dir_t *dir);
fl_dir_t *fl_find_directory(fl_dir_t *root, const char *directory);

#endif

