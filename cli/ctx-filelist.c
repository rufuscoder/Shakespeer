/*
 * Copyright 2004-2005 Martin Hedenfalk <martin@bzero.se>
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

#include "shakespeer.h"
#include "quote.h"
#include "xstr.h"

static int internal_complete_filelist(WordCompletion *cpl, const char *line,
        int word_start, int word_end, int only_dirs)
{
    int len = word_end - word_start;
    char *partial = str_unquote(line + word_start, len);
    len = strlen(partial); /* correction for removed quote characters */

    fl_file_t *f;
    TAILQ_FOREACH(f, &filelist_cdir->files, link)
    {
        if(f && str_has_prefix(f->name, partial) && (!only_dirs || f->dir))
        {
            char *q = str_quote_backslash(f->name + len, " \t\r\n");
            cpl_add_completion(cpl, line, word_start, word_end, q, f->dir ? "\\" : NULL, " ");
            free(q);
        }
    }

    return 0;
}

int cmd_complete_filelist(WordCompletion *cpl, const char *line, int word_start, int word_end)
{
    return internal_complete_filelist(cpl, line, word_start, word_end, 0);
}

int cmd_complete_filelist_directories(WordCompletion *cpl, const char *line, int word_start, int word_end)
{
    return internal_complete_filelist(cpl, line, word_start, word_end, 1);
}

static void display_share_dir(fl_dir_t *dir, int only_directories,
        const char *filter)
{
    fl_file_t *f;
    TAILQ_FOREACH(f, &dir->files, link)
    {
        if(filter == 0 || strstr(f->name, filter))
        {
            if(!only_directories || f->dir)
            {
                if(f->dir)
                {
                    msg("      <DIR>  %s\\", f->name);
                }
                else
                {
                    msg(" %10s  %s", str_size_human(f->size), f->name);
                }
            }
        }
    }
}

int func_filelist_ls(sp_t *sp, arg_t *args)
{
    char *filter = 0;
    if(args->argc > 1)
        filter = args->argv[1];

    display_share_dir(filelist_cdir, 0, filter);

    return 0;
}

int func_filelist_lsdirs(sp_t *sp, arg_t *args)
{
    char *filter = 0;
    if(args->argc > 1)
        filter = args->argv[1];

    display_share_dir(filelist_cdir, 1, filter);

    return 0;
}

static fl_dir_t *find_parent_for(fl_dir_t *dir, fl_dir_t *cdir)
{
    fl_file_t *file;
    TAILQ_FOREACH(file, &dir->files, link)
    {
        if(file->dir)
        {
            if(cdir == file->dir)
                return dir;
            fl_dir_t *foo = find_parent_for(file->dir, cdir);
            if(foo)
                return foo;
        }
    }

    return 0;
}

static fl_file_t *fl_lookup_file(fl_dir_t *dir, const char *filename)
{
    fl_file_t *f;
    TAILQ_FOREACH(f, &dir->files, link)
    {
        if(strcmp(filename, f->name) == 0)
            return f;
    }

    return NULL;
}

int func_filelist_cd(sp_t *sp, arg_t *args)
{
    if(strcmp(args->argv[1], "..") == 0)
    {
        filelist_cdir = find_parent_for(current_filelist->root, filelist_cdir);
        if(filelist_cdir == 0)
            filelist_cdir = current_filelist->root;
        return 0;
    }

    fl_file_t *file = fl_lookup_file(filelist_cdir, args->argv[1]);

    if(file == NULL)
    {
        msg("no such directory: %s", args->argv[1]);
    }
    else
    {
        if(file->dir == NULL)
            msg("not a directory: %s", file->name);
        else
            filelist_cdir = file->dir;
    }

    return 0;
}

int func_filelist_get(sp_t *sp, arg_t *args)
{
    fl_file_t *file = fl_lookup_file(filelist_cdir, args->argv[1]);
    if(file == NULL)
    {
        msg("no such file");
    }
    else
    {
        char *source;

        asprintf(&source, "%s\\%s", filelist_cdir->path, file->name);
        sp_send_download_file(sp, current_filelist->hubaddress,
                current_filelist->nick,
                source, file->size, file->name, file->tth);
        free(source);
    }

    return 0;
}

static void download_recursive(sp_t *sp, fl_dir_t *dir, const char *root)
{
    char *slash = strrchr(dir->path, '\\');
    if(slash++ == 0)
        slash = dir->path;

    char *new_root;
    asprintf(&new_root, "%s/%s", root, slash);

    fl_file_t *file;
    TAILQ_FOREACH(file, &dir->files, link)
    {
        if(file->dir)
        {
            download_recursive(sp, file->dir, new_root);
        }
        else
        {
            char *target;
            char *source;
            asprintf(&target, "%s/%s", new_root, file->name);
            asprintf(&source, "%s\\%s", dir->path, file->name);
            sp_send_download_file(sp, current_filelist->hubaddress,
                    current_filelist->nick,
                    source, file->size, target, file->tth);
            free(source);
            free(target);
        }
    }
    free(new_root);

}

int func_filelist_get_directory(sp_t *sp, arg_t *args)
{
    fl_file_t *file = fl_lookup_file(filelist_cdir, args->argv[1]);
    if(file == NULL)
    {
        msg("no such file: %s", args->argv[1]);
    }
    else
    {
        if(file->dir)
        {
            download_recursive(sp, file->dir,
                    cfg_getstr(cfg, "download-directory"));
        }
        else
        {
            msg("not a directory: %s", file->name);
        }
    }

    return 0;
}

int func_filelist_exit(sp_t *sp, arg_t *args)
{
    context = CTX_HUB;
    return 0;
}

#if 0
static int display_search_result(search_t *search, fl_file_t *file, void *data)
{
    char *hstr = str_size_human(file->size);
    printf("%s (%s)\n", file->path, hstr);
    return 0;
}
#endif

int func_filelist_search(sp_t *sp, arg_t *args)
{
#if 0
    search_t *s = g_new0(search_t, 1);
    char *tmp = arg_join(args, 1, -1, "$");
    arg_t *foo = arg_create(tmp, "$", 0);
    s->words = foo;
    s->type = SHARE_TYPE_ANY;
    share_search(current_filelist->share, s, display_search_result, NULL);
    arg_free(foo);
    free(s);
#endif
    msg("not implemented");
    return 0;
}

