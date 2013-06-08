/*
 * Copyright 2004-2007 Martin Hedenfalk <martin@bzero.se>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "filelist.h"
#include "queue.h"
#include "log.h"
#include "notifications.h"
#include "xstr.h"

extern struct queue_store *q_store;

/* helper function to walk the filelist tree and add found files to the queue */
static void queue_resolve_directory_recursively(const char *nick,
        fl_dir_t *root,
        const char *directory,
        const char *target_directory,
        unsigned *nfiles_p)
{
    fl_file_t *file;
    int num_returned_bytes;
    TAILQ_FOREACH(file, &root->files, link)
    {
        char *target;
        num_returned_bytes = asprintf(&target, "%s/%s", directory, file->name);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");

        if(file->dir)
        {
            queue_resolve_directory_recursively(nick, file->dir,
                    target, target_directory, nfiles_p);
        }
        else
        {
            char *source;
            num_returned_bytes = asprintf(&source, "%s\\%s", root->path, file->name);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");

            queue_add_internal(nick, source, file->size, target,
                    file->tth, 0, target_directory);

            if(nfiles_p)
                (*nfiles_p)++;
            free(source);
        }
        free(target);
    }
}

/* Resolves all files and subdirectories in a directory download request
 * through the filelist. Adds those resolved files to the download queue.
 *
 * Returns 0 if the directory was directly resolved (filelist already exists)
 * or 1 if the filelist has been queued and another attempt at resolving the
 * directory should be done once the filelist is available.
 *
 * Returns -1 on error.
 */
int queue_resolve_directory(const char *nick,
        const char *source_directory,
        const char *target_directory,
        unsigned *nfiles_p)
{
    DEBUG("resolving directory [%s] for nick [%s]", source_directory, nick);

    char *filelist_path = find_filelist(global_working_directory, nick);

    if(filelist_path)
    {
        /* parse the filelist and add all files in the directory to the queue */
	DEBUG("found filelist for [%s] in [%s]", nick, filelist_path);

        /* FIXME: might need to do this asynchronously */
        /* FIXME: no need to keep the whole filelist in memory (parse
         * incrementally) */
        fl_dir_t *root = fl_parse(filelist_path, NULL);
        if(root)
        {
            fl_dir_t *fl = fl_find_directory(root, source_directory);
            if(fl)
            {
                unsigned nfiles = 0;

                queue_resolve_directory_recursively(nick, fl,
                        target_directory, target_directory,
                        &nfiles);

                if(nfiles_p)
                    *nfiles_p = nfiles;

                /* update the resolved flag for this directory */
		queue_db_set_resolved(target_directory, nfiles);
            }
            else
            {
                INFO("source directory not found, removing from queue");
                queue_remove_directory(target_directory);
            }
        }
        fl_free_dir(root);
        free(filelist_path);
    }
    else
    {
        /* get the filelist first so we can see what files to download */
	DEBUG("filelist for [%s] not available, queueing", nick);
        queue_add_filelist(nick, 1 /* auto-matched */);
        return 1;
    }

    return 0;
}

int queue_add_directory(const char *nick,
        const char *source_directory,
        const char *target_directory)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(source_directory, -1);
    return_val_if_fail(target_directory, -1);

    while(*target_directory == '/')
        ++target_directory;

    /* FIXME: duplicate target directories ??? */

    /* Add a directory request so we can keep track of directory download
     * progress. This also triggers us to re-resolve the directory when a
     * filelist is downloaded (unless it's directly resolvable below).
     */
    queue_db_add_directory(target_directory, nick, source_directory);
    nc_send_queue_directory_added_notification(nc_default(),
	target_directory, nick);

    unsigned nfiles = 0;
    queue_resolve_directory(nick, source_directory, target_directory, &nfiles);

    return 0;
}

/* Remove all files in the directory.
 */
int queue_remove_directory(const char *target_directory)
{
    return_val_if_fail(target_directory, -1);

    while(*target_directory == '/')
        ++target_directory;

    /* Loop through all targets and look for targets belonging to the
     * target_directory.
     */

    DEBUG("removing targets in directory [%s]", target_directory);

    struct queue_target *qt, *next;
    for(qt = TAILQ_FIRST(&q_store->targets); qt; qt = next)
    {
	next = TAILQ_NEXT(qt, link);

        if(qt->target_directory &&
	   strcmp(target_directory, qt->target_directory) == 0)
        {
            queue_remove_sources(qt->filename);
            DEBUG("removing target [%s]", qt->filename);
	    queue_db_remove_target(qt->filename);
        }
    }

    queue_db_remove_directory(target_directory);

    return 0;
}

#ifdef TEST

#include "unit_test.h"
#include "bz2.h"

int got_filelist_notification = 0;
int got_directory_notification = 0;
int got_directory_removed_notification = 0;
int got_target_removed_notification = 0;

void handle_filelist_added_notification(nc_t *nc, const char *channel,
        nc_filelist_added_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->nick);
    DEBUG("added filelist for %s", data->nick);
    fail_unless(strcmp(data->nick, "bar") == 0);
    got_filelist_notification = 1;
}

void handle_queue_directory_added_notification(nc_t *nc, const char *channel,
        nc_queue_directory_added_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_directory);
    DEBUG("added directory %s", data->target_directory);
    fail_unless(strcmp(data->target_directory, "target/directory") == 0);
    fail_unless(data->nick);
    fail_unless(strcmp(data->nick, "bar") == 0);
    got_directory_notification = 1;
}

void handle_queue_directory_removed_notification(nc_t *nc, const char *channel,
        nc_queue_directory_removed_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_directory);
    DEBUG("removed directory %s", data->target_directory);
    fail_unless(strcmp(data->target_directory, "target/directory") == 0);
    got_directory_removed_notification = 1;
}

void handle_queue_target_removed_notification(nc_t *nc, const char *channel,
        nc_queue_target_removed_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_filename);
    DEBUG("removed target %s", data->target_filename);
    /* fail_unless(strcmp(data->target_directory, "target/directory") == 0); */
    ++got_target_removed_notification;
}

void test_setup(void)
{
    global_working_directory = "/tmp/sp-queue_directory-test.d";
    system("/bin/rm -rf /tmp/sp-queue_directory-test.d");
    system("mkdir /tmp/sp-queue_directory-test.d");

    queue_init();
}

void test_teardown(void)
{
    queue_close();
    // system("/bin/rm -rf /tmp/sp-queue_directory-test.d");
}

/* create a sample filelist for the "bar" user */
void test_create_filelist(void)
{
    /* create a filelist in our working directory */
    char *fl_path;
    asprintf(&fl_path, "%s/files.xml.bar", global_working_directory);

    FILE *fp = fopen(fl_path, "w");
    fail_unless(fp);

    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\n"
            "<FileListing Version=\"1\" CID=\"NOFUKZZSPMR4M\" Base=\"/\" Generator=\"DC++ 0.674\">\n"
            "<Directory Name=\"source\">\n"
            "  <Directory Name=\"directory\">\n"
            "    <File Name=\"filen\" Size=\"26577\" TTH=\"ABAJCAPSGKJMY7IFTZA7XSE2AINPGZYMHIWXVSY\"/>\n"
            "    <File Name=\"filen2\" Size=\"1234567\" TTH=\"ABAJCAPSGKJMY7IFTZA7XSE2AINPGZYMXXXXXXX\"/>\n"
            "      <Directory Name=\"subdir\">\n"
            "        <File Name=\"filen3\" Size=\"2345678\" TTH=\"ABAJCAPSGKJMY7IFTZA7XSE2AINPGZYMXXXZZZZ\"/>\n"
            "      </Directory>\n"
            "  </Directory>\n"
            "</Directory>\n"
            "</FileListing>\n");
    fail_unless(fclose(fp) == 0);

    char *fl_path_bz2;
    asprintf(&fl_path_bz2, "%s.bz2", fl_path);

    xerr_t *err = NULL;
    bz2_encode(fl_path, fl_path_bz2, &err);
    fail_unless(err == NULL);

    fail_unless(unlink(fl_path) == 0);

    free(fl_path);
    free(fl_path_bz2);
}

/* add a directory and make sure we get notifications of added directory + the
 * users filelist */
void test_add_directory_no_filelist(void)
{
    test_setup();

    got_filelist_notification = 0;
    got_directory_notification = 0;

    unlink("/tmp/files.xml.bar");

    DEBUG("adding directory");
    fail_unless(queue_add_directory("bar",
                "source\\directory", "target/directory") == 0);
    fail_unless(got_filelist_notification == 1);
    fail_unless(got_directory_notification == 1);

    /* now we should download the filelist */
    DEBUG("downloading the filelist");
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->nick);
    fail_unless(strcmp(q->nick, "bar") == 0);
    fail_unless(q->is_filelist == 1);
    queue_free(q);

    /* we got it */ 
    test_create_filelist();
    fail_unless(queue_remove_filelist("bar") == 0);

    /* Next queue should be the placeholder directory. This time we have the
     * filelist, so resolving it should be successful. */
    DEBUG("resolving the filelist");
    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->nick);
    fail_unless(strcmp(q->nick, "bar") == 0);
    fail_unless(q->is_directory);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "source\\directory") == 0);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "target/directory") == 0);

    unsigned nfiles = 0;
    got_directory_removed_notification = 0;
    fail_unless(queue_resolve_directory(q->nick,
                q->source_filename, q->target_filename, &nfiles) == 0);
    queue_free(q);
    fail_unless(nfiles == 3);

    test_teardown();
    puts("PASSED: add directory w/o filelist");
}

/* Add a directory from a user whose filelist we've already downloaded.
 */
void test_add_directory_existing_filelist(void)
{
    test_setup();
    test_create_filelist();

    got_filelist_notification = 0;
    got_directory_notification = 0;

    /* add a directory to the download queue, make sure the filelist we created
     * above is used (ie, no new filelist should be added to the queue) */
    fail_unless(queue_add_directory("bar", "source\\directory",
                "target/directory") == 0);
    fail_unless(got_filelist_notification == 0);
    fail_unless(got_directory_notification == 1);

    /* we should be able to download the file in the filelist */
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(q->target_filename);
    DEBUG("source_filename = %s", q->source_filename);
    fail_unless(q->tth);
    fail_unless(strncmp(q->tth, "ABAJCAPSGKJMY7IFTZA7XSE2AINPGZ", 30) == 0);
    queue_free(q);

    /* All resolved files should belong to the same queue_directory_t
     */
    queue_target_t *qt = queue_lookup_target("target/directory/subdir/filen3");
    fail_unless(qt);
    DEBUG("target_directory = [%s]", qt->target_directory);
    fail_unless(strcmp(qt->target_directory, "target/directory") == 0);

    qt = queue_lookup_target("target/directory/filen");
    fail_unless(qt);
    DEBUG("target_directory = [%s]", qt->target_directory);
    fail_unless(strcmp(qt->target_directory, "target/directory") == 0);

    got_directory_removed_notification = 0;
    got_target_removed_notification = 0;
    fail_unless(queue_remove_target("target/directory/filen") == 0);
    fail_unless(queue_remove_target("target/directory/subdir/filen3") == 0);
    fail_unless(got_target_removed_notification == 2);
    fail_unless(got_directory_removed_notification == 0);

    /* We've download the whole directory. When removing the last file in the
     * directory we should get a notification that the whole directory is
     * removed from the queue. */

    got_directory_removed_notification = 0;
    got_target_removed_notification = 0;
    fail_unless(queue_remove_target("target/directory/filen2") == 0);
    fail_unless(got_target_removed_notification == 1);
    fail_unless(got_directory_removed_notification == 1);

    test_teardown();
    puts("PASSED: add directory w/ existing filelist");
}

void test_remove_directory(void)
{
    test_setup();
    test_create_filelist();

    /* there should be nothing in the queue to start with */
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q == NULL);

    /* add a directory */
    fail_unless(queue_add_directory("bar", "source\\directory",
                "target/directory") == 0);

    /* look it up */
    struct queue_directory *qd = queue_db_lookup_directory("target/directory");
    fail_unless(qd);
    fail_unless(qd->nfiles == 3);
    fail_unless(qd->nfiles == qd->nleft);

    /* test persistence: close and re-open the queue
     */
    queue_close();
    queue_init();

    /* now we should have 3 targets to download */
    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(!q->is_directory);

    /* look it up again, should still be 3 files left */
    qd = queue_db_lookup_directory("target/directory");
    fail_unless(qd);
    fail_unless(qd->nfiles == 3);
    fail_unless(qd->nfiles == qd->nleft);

    /* and remove the directory */
    fail_unless(queue_remove_directory("target/directory") == 0);

    /* test persistence: close and re-open the queue
     */
    queue_close();
    queue_init();

    /* again, nothing in the queue */
    q = queue_get_next_source_for_nick("bar");
    fail_unless(q == NULL);

    fail_unless(queue_db_lookup_directory("target/directory") == NULL);

    test_teardown();
    puts("PASSED: remove directory");
}

/* verify that different priorities works for different targets within a
 * directory
 */
void test_directory_priorities(void)
{
    test_setup();
    test_create_filelist();

    /* add a directory */
    fail_unless(queue_add_directory("bar", "source\\directory",
                "target/directory") == 0);

    queue_set_priority("target/directory/filen", 1);
    queue_set_priority("target/directory/filen2", 2);
    queue_set_priority("target/directory/subdir/filen3", 4);

    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "source\\directory\\subdir\\filen3") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("target/directory/subdir/filen3") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "source\\directory\\filen2") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("target/directory/filen2") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "source\\directory\\filen") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("target/directory/filen") == 0);

    test_teardown();

    puts("PASSED: directory priorities");
}

int main(void)
{
    sp_log_set_level("debug");

    nc_add_filelist_added_observer(nc_default(),
            handle_filelist_added_notification, NULL);
    nc_add_queue_directory_added_observer(nc_default(),
            handle_queue_directory_added_notification, NULL);
    nc_add_queue_directory_removed_observer(nc_default(),
            handle_queue_directory_removed_notification, NULL);
    nc_add_queue_target_removed_observer(nc_default(),
            handle_queue_target_removed_notification, NULL);

    test_add_directory_no_filelist();
    test_add_directory_existing_filelist();
    test_remove_directory();
    test_directory_priorities();

    return 0;
}

#endif

