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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "filelist.h"
#include "log.h"
#include "xstr.h"

static void fl_xml_parse_start_tag(void *user_data,
        const char *el, const char **attr)
{
    fl_xml_ctx_t *ctx = user_data;
    assert(ctx);

    /* the current directory is at the top of the stack */
    fl_dir_t *curdir = LIST_FIRST(&ctx->dir_stack);
    assert(curdir);
    
    int num_returned_bytes;

    if (strcasecmp(el, "Directory") == 0) {
        int i;
        const char *dirname = 0;
        for (i = 0; attr && attr[i]; i += 2) {
            if(strcasecmp(attr[i], "Name") == 0) {
                dirname = attr[i + 1];
                break;
            }
        }

        if (dirname == 0)
            WARNING("Missing Name attribute in Directory tag");
        else {
            fl_dir_t *dir = calloc(1, sizeof(fl_dir_t));
            TAILQ_INIT(&dir->files);
            int num_returned_bytes = asprintf(&dir->path, "%s%s%s", curdir->path, *curdir->path ? "\\" : "", dirname);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");

            if (ctx->file_callback == NULL) {
                fl_file_t *f = calloc(1, sizeof(fl_file_t));
                f->name = strdup(dirname);
                f->type = SHARE_TYPE_DIRECTORY;
                f->dir = dir;

                TAILQ_INSERT_TAIL(&curdir->files, f, link);
                curdir->nfiles++;
            }

            /* push the new directory on the stack */
            LIST_INSERT_HEAD(&ctx->dir_stack, dir, link);
        }
    }
    else if (strcasecmp(el, "File") == 0) {
        const char *name = 0, *tth = 0;
        uint64_t size = 0;

        int i;
        for (i = 0; attr && attr[i]; i += 2) {
            if(strcmp(attr[i], "Name") == 0)
                name = attr[i + 1];
            else if(strcmp(attr[i], "Size") == 0)
                size = strtoull(attr[i + 1], NULL, 10);
            else if(strcmp(attr[i], "TTH") == 0)
                tth = attr[i + 1];
        }

        if (ctx->file_callback) {
            if (name && tth) {
                char *path = 0;
                num_returned_bytes = asprintf(&path, "%s\\%s", curdir->path, name);
                if (num_returned_bytes == -1)
                    DEBUG("asprintf did not return anything");
                ctx->file_callback(path, tth, size, ctx->user_data);
                free(path);
            }
        }
        else {
	    /* If no callback wants to handle the file, we collect
	     * all files in a list to be processed later.
	     */
            fl_file_t *f = calloc(1, sizeof(fl_file_t));
            f->name = strdup(name);
            f->type = share_filetype(name);
            f->size = size;
            f->tth = xstrdup(tth);

            TAILQ_INSERT_TAIL(&curdir->files, f, link);

            curdir->nfiles++;
            curdir->size += f->size;
        }
    }
}

static void fl_xml_parse_end_tag(void *user_data, const char *el)
{
    fl_xml_ctx_t *ctx = user_data;
    assert(ctx);
    assert(LIST_FIRST(&ctx->dir_stack) != NULL);

    if(strcasecmp(el, "Directory") == 0)
    {
        /* pop the current directory off the stack */
        fl_dir_t *curdir = LIST_FIRST(&ctx->dir_stack);
        LIST_REMOVE(curdir, link);

        if(ctx->file_callback == NULL)
        {
            fl_dir_t *dir = LIST_FIRST(&ctx->dir_stack);
            dir->nfiles += curdir->nfiles;
            dir->size += curdir->size;
        }
        else
        {
            free(curdir->path);
            free(curdir);
        }
    }
}

int fl_parse_xml_chunk(fl_xml_ctx_t *ctx)
{
    return xml_parse_chunk(ctx->xml, NULL);
}

fl_xml_ctx_t *fl_xml_prepare_file(const char *filename,
        fl_xml_file_callback_t file_callback, void *user_data)
{
    return_val_if_fail(filename, NULL);

    FILE *fp = fopen(filename, "r");
    return_val_if_fail(fp, NULL);

    fl_dir_t *root = calloc(1, sizeof(fl_dir_t));
    TAILQ_INIT(&root->files);
    root->path = strdup("");

    fl_xml_ctx_t *ctx = calloc(1, sizeof(fl_xml_ctx_t));

    LIST_INIT(&ctx->dir_stack);
    LIST_INSERT_HEAD(&ctx->dir_stack, root, link);

    ctx->root = root;
    ctx->fp = fp;
    ctx->user_data = user_data;
    ctx->file_callback = file_callback;

    ctx->xml = xml_init_fp(fp,
            fl_xml_parse_start_tag, fl_xml_parse_end_tag, ctx);

    return ctx;
}

void fl_xml_free_context(fl_xml_ctx_t *ctx)
{
    return_if_fail(ctx);

    xml_ctx_free(ctx->xml);
    fclose(ctx->fp);
    free(ctx);
}

fl_dir_t *fl_parse_xml(const char *filename)
{
    fl_xml_ctx_t *ctx = fl_xml_prepare_file(filename, NULL, NULL);
    return_val_if_fail(ctx, NULL);

    while(fl_parse_xml_chunk(ctx) == 0)
    {
        /* do nothing */ ;
    }

    fl_dir_t *root = ctx->root;
    fl_xml_free_context(ctx);

    return root;
}

#ifdef TEST

#include "unit_test.h"

int main(void)
{
    sp_log_set_level("debug");

    fl_dir_t *fl = fl_parse_xml("fl_test1.xml");
    fail_unless(fl);
    fail_unless(fl->nfiles == 40);
    fail_unless(fl->size == 612026);
    fl_dir_t *root = fl_find_directory(fl, "spclient\\CVS - copy");
    fail_unless(root);
    fail_unless(root->nfiles == 3);
    fl_free_dir(fl);

    fl = fl_parse_xml("fl_test3-invalid-utf8.xml");
    fail_unless(fl);
    fail_unless(fl->nfiles == 40);
    fail_unless(fl->size == 612026);
    fl_free_dir(fl);

    return 0;
}

#endif

