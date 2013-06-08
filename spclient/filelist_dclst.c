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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "filelist.h"
#include "iconv_string.h"
#include "xstr.h"
#include "log.h"

static int fl_indentation(const char *line)
{
    int i = 0;
    while(line[i] == '\t')
        ++i;
    return i;
}

static char *fl_dclst_read_line(FILE *fp)
{
    char buf[1024];
    char *e = fgets(buf, sizeof(buf), fp);
    if(e == NULL)
        return NULL;
    char *line = iconv_string_lossy(buf, -1, "WINDOWS-1252", "UTF-8");
    return line;
}

static fl_dir_t *fl_parse_dclst_recursive(FILE *fp, char **saved,
        int level, char *path)
{
    fl_dir_t *dir = calloc(1, sizeof(fl_dir_t));
    TAILQ_INIT(&dir->files);
    dir->path = strdup(path ? path : "");

    while (1) {
        char *line = 0;
        if (saved && *saved) {
            line = *saved;
            if (saved)
                *saved = NULL;
        }
        else {
            line = fl_dclst_read_line(fp);
            if (line == NULL)
                break;
        }
        str_trim_end_inplace(line, NULL);

        int tabs = fl_indentation(line);
        if (tabs < level) {
            if(saved)
                *saved = line;
            break;
        }

        char *pipe = strchr(line + tabs, '|');
        if (pipe)
            *pipe = 0;

        char *filename = line + tabs;

        fl_file_t *file = calloc(1, sizeof(fl_file_t));
        file->name = xstrdup(filename);

        if (pipe) {
            /* regular file */
            file->type = share_filetype(filename);
            file->size = strtoull(pipe + 1, NULL, 10);
            dir->size += file->size;
        }
        else {
            /* directory */
            file->type = SHARE_TYPE_DIRECTORY;

            char *newpath;
            if (path) {
                int num_returned_bytes = asprintf(&newpath, "%s\\%s", path, filename);
                if (num_returned_bytes == -1)
                    DEBUG("asprintf did not return anything");
            }
            else
                newpath = xstrdup(filename);

            file->dir = fl_parse_dclst_recursive(fp, saved, level + 1, newpath);
            free(newpath);
            dir->nfiles += file->dir->nfiles;
            dir->size += file->dir->size;
        }

        dir->nfiles++;
        TAILQ_INSERT_TAIL(&dir->files, file, link);

        free(line);
    }

    return dir;
}

fl_dir_t *fl_parse_dclst(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if(fp == NULL)
    {
        INFO("failed to open %s: %s", filename, strerror(errno));
        return NULL;
    }
    char *saved = 0;
    fl_dir_t *root = fl_parse_dclst_recursive(fp, &saved, 0, NULL);
    fclose(fp);

    return root;
}

#ifdef TEST

#include "unit_test.h"

int main(void)
{
    fl_dir_t *fl = fl_parse_dclst("fl_test2.DcLst");
    fail_unless(fl);
    fail_unless(fl->nfiles == 36);
    fail_unless(fl->size == 611569);
    fl_dir_t *root = fl_find_directory(fl, "spclient\\CVS");
    fail_unless(root);
    fail_unless(root->nfiles == 3);
    fl_free_dir(fl);

    return 0;
}

#endif

