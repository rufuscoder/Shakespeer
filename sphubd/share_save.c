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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "he3.h"
#include "bz2.h"
#include "encoding.h"
#include "dstring.h"
#include "log.h"
#include "nfkc.h"
#include "share.h"
#include "globals.h"
#include "xstr.h"

typedef void (*share_print_file_func)(FILE *fp, int level, share_file_t *file);
typedef void (*share_print_directory_func)(FILE *fp, int level, const char *filename);

typedef struct share_save_context share_save_context_t;
struct share_save_context
{
    FILE *fp;
    int level;
    share_print_file_func file_pfunc;
    share_print_directory_func directory_start_pfunc;
    share_print_directory_func directory_end_pfunc;
};

static void share_scan_indent(FILE *fp, int level)
{
    return_if_fail(level >= 0);
    while(level--)
        fputc('\t', fp);
}

/* escapes xml data, returned string should be freed by caller */
static char *share_xml_escape(const char *string)
{
    if(string == NULL)
        return NULL;

    const char *p = string;
    dstring_t *ds = dstring_new(NULL);
    while(*p)
    {
        char c = *p;
        char *esc = 0;
        if(c == '<')
            esc = "&lt;";
        else if(c == '>')
            esc = "&gt;";
        else if(c == '&')
            esc = "&amp;";
        else if(c == '\'')
            esc = "&apos;";
        else if(c == '"')
            esc = "&quot;";
        if(esc)
            dstring_append(ds, esc);
        else
            dstring_append_char(ds, c);
        p++;
    }

    return dstring_free(ds, 0);
}

static void share_xml_print_file(FILE *fp, int level, share_file_t *file)
{
    /* convert the decomposed utf-8 string to composed form (eg, &Auml; is
     * converted to a single precomposed character instead of a base character
     * with a combining accent). This is required for DC++/Windows to correctly
     * display the filenames.
     */

    char *filename = strrchr(file->partial_path, '/');
    if(filename++ == 0)
	filename = file->partial_path;

    char *utf8_composed_filename = g_utf8_normalize(filename, -1,
            G_NORMALIZE_DEFAULT_COMPOSE);
    char *escaped_utf8_filename = share_xml_escape(utf8_composed_filename);
    free(utf8_composed_filename);

    share_scan_indent(fp, level);

    struct tth_inode *ti = tth_store_lookup_inode(global_tth_store, file->inode);
    if(ti)
    {
        fprintf(fp, "<File Name=\"%s\" Size=\"%"PRIu64"\" TTH=\"%s\"/>\r\n",
                escaped_utf8_filename, file->size, ti->tth);
    }
    else
    {
        fprintf(fp, "<File Name=\"%s\" Size=\"%"PRIu64"\"/>\r\n",
                escaped_utf8_filename, file->size);
    }
    free(escaped_utf8_filename);
}

static void share_xml_print_directory_start(FILE *fp, int level,
        const char *filename)
{
    char *utf8_composed_filename = g_utf8_normalize(filename, -1,
            G_NORMALIZE_DEFAULT_COMPOSE);
    char *escaped_utf8_filename = share_xml_escape(utf8_composed_filename);
    free(utf8_composed_filename);

    share_scan_indent(fp, level);
    fprintf(fp, "<Directory Name=\"%s\">\r\n", escaped_utf8_filename);
    free(escaped_utf8_filename);
}

static void share_xml_print_directory_end(FILE *fp, int level,
        const char *filename)
{
    share_scan_indent(fp, level);
    fprintf(fp, "</Directory>\r\n");
}

static int find_level(const char *filename)
{
    int level = 0;
    const char *e = filename;
    for(; *e; e++)
    {
        if(*e == '/')
            ++level;
    }
    return level;
}

/* ui/i18/apan
 * ui/i18/bepan
 *
 * => 7 ( = strlen("ui/i18/") )
 *
 *
 * ""
 * "pix/foo"
 *
 * => 0
 */

static int find_common_prefix(const char *p, const char *pp)
{
    if(pp == NULL)
        return 0;

    const char *orig_p = p;

    while(*p)
    {
        const char *slash_p = strchr(p, '/');
        const char *slash_pp = strchr(pp, '/');

        if(slash_p == NULL || slash_pp == NULL)
            break;

        size_t len = slash_p - p;
        if(len != slash_pp - pp)
            break;

        if(strncmp(p, pp, len) != 0)
            break;

        p += len + 1;
        pp += len + 1;
    }

    return p - orig_p;
}

static void share_save_file(share_t *share, share_save_context_t *ctx)
{
    share_mountpoint_t *last_mp = NULL;
    share_file_t *f;
    char *last_p = NULL;
    RB_FOREACH(f, file_tree, &share->files)
    {
        if(f->mp != last_mp)
        {
            /* New or changed mountpoint. */
            while(ctx->level--)
            {
                if(ctx->directory_end_pfunc)
                    ctx->directory_end_pfunc(ctx->fp, ctx->level, NULL);
            }
	    last_mp = f->mp;
            ctx->directory_start_pfunc(ctx->fp, 0, last_mp->virtual_root);
            ctx->level = 1;

            free(last_p);
            last_p = NULL;
        }

        char *tmp = f->partial_path + strspn(f->partial_path, "/"); /* skip inital '/' */
        char *last_slash = strrchr(tmp, '/');
        if(last_slash++ == NULL)
            last_slash = tmp;
        char *p = xstrndup(tmp, last_slash - tmp); /* sub-path */

        int i;
        int n = 0;

        if(last_p)
        {
            n = find_common_prefix(p, last_p);

            int down_level = find_level(last_p + n);
            for(i = 0; i < down_level; i++)
            {
                --ctx->level;
                if(ctx->directory_end_pfunc)
                {
                    ctx->directory_end_pfunc(ctx->fp, ctx->level, NULL);
                }
            }
        }

        int up_level = find_level(p + n);

        const char *dir = p + n;
        for(i = 0; i < up_level; i++)
        {
            const char *slash = strchr(dir, '/');
            if(slash == NULL)
                slash = dir + strlen(dir);
            if(ctx->directory_start_pfunc)
            {
                char *dirname = xstrndup(dir, slash - dir);
                ctx->directory_start_pfunc(ctx->fp, ctx->level, dirname);
                free(dirname);
            }
            dir = slash + 1;
            ++ctx->level;
        }

        free(last_p);
        last_p = p;

        ctx->file_pfunc(ctx->fp, ctx->level, f);
    }

    free(last_p);

    while(ctx->level--)
    {
        if(ctx->directory_end_pfunc)
            ctx->directory_end_pfunc(ctx->fp, ctx->level, NULL);
    }
}

static int share_save_xml(share_t *share, const char *filename, xerr_t **err)
{
    int rc = 0;

    g_return_val_if_fail(global_id_generator, -1);
    g_return_val_if_fail(global_id_version, -1);

    DEBUG("saving XML filelist to %s...", filename);
    FILE *fp = fopen(filename, "w");
    if (fp == 0) {
        xerr_set(err, -1, "%s: %s", filename, strerror(errno));
        rc = -1;
    }
    else {
        share_save_context_t ctx;
        memset(&ctx, 0, sizeof(share_save_context_t));

        ctx.fp = fp;
        ctx.file_pfunc = share_xml_print_file;
        ctx.directory_start_pfunc = share_xml_print_directory_start;
        ctx.directory_end_pfunc = share_xml_print_directory_end;

        fprintf(fp,
                "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\r\n"
                "<FileListing Version=\"1\" CID=\"%s\" Base=\"/\""
                " Generator=\"%s %s\">\r\n",
                share->cid, global_id_generator, global_id_version);
        share_save_file(share, &ctx);
        fprintf(fp, "</FileListing>\r\n");
        fclose(fp);

        char *dest;
        int num_returned_bytes = asprintf(&dest, "%s.bz2", filename);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
        rc = bz2_encode(filename, dest, err);
        free(dest);
    }

    return rc;
}

int share_save(share_t *share, unsigned type)
{
    int rc = 0;

    return_val_if_fail(share, -1);
    return_val_if_fail(type == FILELIST_XML, -1);

    if ((type & FILELIST_XML) == FILELIST_XML) {
        char *xml_filename;
        int num_returned_bytes = asprintf(&xml_filename, "%s/files.xml", global_working_directory);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
        if (!share->uptodate || access(xml_filename, F_OK) == -1)
            rc = share_save_xml(share, xml_filename, NULL);
        else
            DEBUG("share up to date and file exists, skipping saving xml file");

        free(xml_filename);
    }

    share->uptodate = (rc == 0 ? true : false);

    return rc;
}

