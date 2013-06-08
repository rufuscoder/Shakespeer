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

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "filelist.h"
#include "log.h"
#include "xerr.h"
#include "bz2.h"
#include "he3.h"

/* Parse the filelist in the given file. Handles both xml and old-style dclst
 * filelists. Compressed files are automatically decompressed. If there
 * already is a decompressed filelist with mtime > original, no decompression
 * is necessary.
 */
fl_dir_t *fl_parse(const char *filename, xerr_t **err)
{
    /* Check type of filelist.
     */
    int type = is_filelist(filename);
    return_val_if_fail(type != FILELIST_NONE, NULL);

    /* Check for a compressed filelist.
     */
    char *filename_noext = strdup(filename);
    char *ext = strrchr(filename_noext, '.');
    if(ext && (strcmp(ext, ".bz2") == 0 || strcmp(ext, ".DcLst") == 0))
    {
	/* strip the .bz2 or .DcLst suffix */
	*ext++ = 0;

	/* Check for existing decompressed filelist.
	 */
	bool need_decompression = true;
	struct stat stbuf;
	if(stat(filename_noext, &stbuf) == 0)
	{
	    struct stat stbuf_orig;
	    if(stat(filename, &stbuf_orig) != 0)
	    {
		/* huh!? original doesn't exist? */
		WARNING("%s: %s", filename, strerror(errno));
		free(filename_noext);
		return NULL;
	    }

	    /* compare modification times */
	    if(stbuf.st_mtime >= stbuf_orig.st_mtime)
	    {
		/* decompressed filelist up-to-date */
		need_decompression = false;
		DEBUG("re-using decompressed filelist [%s]", filename_noext);
	    }
	}

	if(need_decompression)
	{
	    /* decompress the filelist */
	    DEBUG("decompressing filelist [%s] -> [%s]", filename, filename_noext);
	    if(strcmp(ext, "bz2") == 0)
		bz2_decode(filename, filename_noext, err);
	    else
		he3_decode(filename, filename_noext, err);
	    if(err && *err)
	    {
		WARNING("failed to decompress filelist: %s", xerr_msg(*err));
		return NULL;
	    }
	}
    }

    /* FIXME: should pass the xerr_t to the parse functions too */
    if(type == FILELIST_DCLST)
        return fl_parse_dclst(filename_noext);
    else
        return fl_parse_xml(filename_noext);

    free(filename_noext);
}

static void fl_free_file(fl_file_t *flf)
{
    if(flf)
    {
        free(flf->name);
        free(flf->tth);
        if(flf->dir)
            fl_free_dir(flf->dir);
        free(flf);
    }
}

void fl_free_dir(fl_dir_t *dir)
{
    if(dir)
    {
        fl_file_t *f, *next;
        for(f = TAILQ_FIRST(&dir->files); f; f = next)
        {
            next = TAILQ_NEXT(f, link);
            TAILQ_REMOVE(&dir->files, f, link);
            fl_free_file(f);
        }
        free(dir->path);
        free(dir);
    }
}

fl_dir_t *fl_find_directory(fl_dir_t *root, const char *directory)
{
    assert(root);
    assert(root->path);

    if(strcmp(root->path, directory) == 0)
        return root;

    fl_file_t *file;
    TAILQ_FOREACH(file, &root->files, link)
    {
        if(file->dir &&
           strncmp(file->dir->path, directory, strlen(file->dir->path)) == 0)
        {
            if(strcmp(file->dir->path, directory) == 0)
                return file->dir;
            fl_dir_t *found_dir = fl_find_directory(file->dir, directory);
            if(found_dir)
                return found_dir;
            /* else keep looking */
        }
    }

    return NULL;
}

