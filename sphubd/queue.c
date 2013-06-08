/*
 * Copyright (c) 2004-2007 Martin Hedenfalk <martin@bzero.se>
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

#include "sys_queue.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "queue.h"
#include "xstr.h"
#include "log.h"
#include "notifications.h"
#include "quote.h"

extern struct queue_store *q_store;

queue_t *queue_get_next_source_for_nick(const char *nick)
{
    queue_t *queue = NULL;
    return_val_if_fail(nick, NULL);

    /* first check if there is a filelist to download */
    queue_filelist_t *qf = queue_lookup_filelist(nick);
    if(qf && (qf->flags & QUEUE_TARGET_ACTIVE) == 0)
    {
        /* fill in a queue_t from the qf */
        queue = calloc(1, sizeof(queue_t));
        queue->nick = xstrdup(nick);
        queue->is_filelist = true;
        queue->auto_matched = ((qf->flags & QUEUE_TARGET_AUTO_MATCHED)
                == QUEUE_TARGET_AUTO_MATCHED);
        return queue;
    }

    /* next check if there is a directory that needs to be resolved */
    queue_directory_t *qd = queue_db_lookup_unresolved_directory_by_nick(nick);
    if(qd)
    {
        /* fill in a queue_t from the qf */
        queue = calloc(1, sizeof(queue_t));
        queue->nick = xstrdup(nick);
        queue->is_directory = true;
        queue->target_filename = xstrdup(qd->target_directory);
        queue->source_filename = xstrdup(qd->source_directory);
        return queue;
    }

    /* go through all targets where nick is a source
     */
    queue_source_t *qs_candidate = NULL;
    queue_target_t *qt_candidate = NULL;

    struct queue_source *qs;
    TAILQ_FOREACH(qs, &q_store->sources, link)
    {
        if(strcmp(qs->nick, nick) != 0)
            continue;

        queue_target_t *qt = queue_lookup_target(qs->target_filename);
        if(qt == NULL)
            continue;

        if((qt->flags & QUEUE_TARGET_ACTIVE) == QUEUE_TARGET_ACTIVE)
            /* skip targets already active */
            continue;

        if(qt->priority == 0)
            /* skip paused targets */
            continue;

        if(qs_candidate == NULL ||
           qt->priority > qt_candidate->priority ||
           qt->seq < qt_candidate->seq)
        {
	    qs_candidate = qs;
	    qt_candidate = qt;
        }
    }

    if(qs_candidate)
    {
        queue = calloc(1, sizeof(queue_t));
        queue->nick = xstrdup(nick);
        queue->source_filename = xstrdup(qs_candidate->source_filename);
        queue->target_filename = xstrdup(qs_candidate->target_filename);
        queue->tth = xstrdup(qt_candidate->tth);
        queue->size = qt_candidate->size;
        queue->is_filelist = false;
        queue->auto_matched = ((qt_candidate->flags & QUEUE_TARGET_AUTO_MATCHED)
                == QUEUE_TARGET_AUTO_MATCHED);
    }

    return queue;
}

void queue_free(queue_t *queue)
{
    if(queue)
    {
        free(queue->nick);
        free(queue->target_filename);
        free(queue->source_filename);
        free(queue->tth);
        free(queue);
    }
}

bool queue_has_source_for_nick(const char *nick)
{
    queue_t *queue = queue_get_next_source_for_nick(nick);
    if(queue)
    {
	DEBUG("found queue target [%s]", queue->target_filename);
        queue_free(queue);
        return true;
    }

    DEBUG("no queue found");
    return false;
}

/* Add a file to the download queue. Adds both a target and a source.
 */
int queue_add_internal(const char *nick, const char *source_filename,
        uint64_t size, const char *target_filename,
        const char *tth, int auto_matched_filelist,
        const char *target_directory)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(target_filename, -1);
    return_val_if_fail(source_filename, -1);

    /* ignore zero-sized files */
    if(size == 0)
    {
        INFO("Ignoring zero-sized file [%s]", target_filename);
        return 0;
    }

    /* add a queue target
     *
     * first lookup if there already exists a target
     */

    queue_target_t *qt = NULL;
    if(tth && *tth)
    {
        /* No need to match on the filename, TTH (and size?) is enough.
         *
         * This way this new target will just be added as a new source
         * instead of a new target.
         */
        qt = queue_lookup_target_by_tth(tth);
        if(qt && qt->size != size)
        {
            /* Size must also match */
            WARNING("TTH matches but size doesn't");
            qt = NULL;
        }
    }
    else
    {
        qt = queue_lookup_target(target_filename);
        if(qt && (qt->size != size || qt->tth[0] == 0))
        {
            /* Size must also match, and there should be no TTH (?) */
            qt = NULL;
        }
    }

    unsigned int default_priority = 3;
    /* FIXME: set priority based on filesize */

    if(qt == NULL)
    {
	qt = queue_target_add(target_filename, tth, target_directory, size, 0,
		default_priority, 0);
    }

    /* setup a source and add it to the queue */
    queue_add_source(nick, qt->filename, source_filename);

    /* notify ui:s
     */
    nc_send_queue_source_added_notification(nc_default(),
            target_filename, nick, source_filename);

    return 0;
}

int queue_add(const char *nick, const char *source_filename,
        uint64_t size, const char *target_filename, const char *tth)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(target_filename, -1);
    return_val_if_fail(source_filename, -1);
    return queue_add_internal(nick, source_filename, size, target_filename,
            tth, 0, NULL);
}

int queue_add_filelist(const char *nick, bool auto_matched_filelist)
{
    return_val_if_fail(nick, -1);

    int flags = 0;
    if(auto_matched_filelist)
        flags |= QUEUE_TARGET_AUTO_MATCHED;

    struct queue_filelist *qf = queue_lookup_filelist(nick);
    if(qf == NULL)
    {
	qf = calloc(1, sizeof(struct queue_filelist));
	qf->nick = strdup(nick);
	qf->priority = 5;
	qf->flags = flags;

	TAILQ_INSERT_TAIL(&q_store->filelists, qf, link);
	if(!q_store->loading)
	    queue_db_print_add_filelist(q_store->fp, qf);
        nc_send_filelist_added_notification(nc_default(), nick, qf->priority);
    }

    if(!auto_matched_filelist &&
       (qf->flags & QUEUE_TARGET_AUTO_MATCHED) == QUEUE_TARGET_AUTO_MATCHED)
    {
	/* If a filelist has been added by the auto-search
	 * feature, and before the download is complete the user
	 * decided to download it manually, reset the auto_matched flag
	 * so it is presented to the UI.
	 */
	qf->flags &= ~QUEUE_TARGET_AUTO_MATCHED;

	/* FIXME: persistence? */
    }

    return 0;
}

/* remove all sources with nick */
int queue_remove_nick(const char *nick)
{
    return_val_if_fail(nick, -1);

    return queue_remove_sources_by_nick(nick);
}

/* remove nick as a source for the target filename */
int
queue_remove_source(const char *target_filename, const char *nick)
{
    return_val_if_fail(target_filename, -1);
    return_val_if_fail(nick, -1);

    struct queue_source *qs;
    TAILQ_FOREACH(qs, &q_store->sources, link)
    {
        if(strcmp(qs->nick, nick) != 0)
            continue;

        if(strcmp(target_filename, qs->target_filename) == 0)
        {
            DEBUG("removing source [%s], target [%s]",
                    nick, qs->target_filename);
	    if(!q_store->loading)
		queue_db_print_remove_source(q_store->fp, qs);

            nc_send_queue_source_removed_notification(nc_default(),
                    qs->target_filename, nick);

	    queue_source_free(qs);
            break; /* there should be only one (nick, target) pair */
        }
    }

    return 0;
}

void
queue_set_active(queue_t *queue, int flag)
{
	return_if_fail(queue);

	if(queue->is_filelist)
	{
		queue_filelist_t *qf = queue_lookup_filelist(queue->nick);
		return_if_fail(qf);
		if(flag)
			qf->flags |= QUEUE_TARGET_ACTIVE;
		else
			qf->flags &= ~QUEUE_TARGET_ACTIVE;
	}
	else if(queue->is_directory)
	{
		WARNING("Directory downloads don't have active status");
	}
	else
	{
		return_if_fail(queue->target_filename);
		queue_target_t *qt =
			queue_lookup_target(queue->target_filename);
		return_if_fail(qt);
		if(flag)
			qt->flags |= QUEUE_TARGET_ACTIVE;
		else
			qt->flags &= ~QUEUE_TARGET_ACTIVE;
	}

	/* no need to make this persistent as it's volatile information */
}

void queue_set_size(queue_t *queue, uint64_t size)
{
    return_if_fail(queue);
    return_if_fail(queue->target_filename);
    return_if_fail(queue->is_filelist == 0);
    return_if_fail(queue->is_directory == 0);

    queue_target_t *qt = queue_lookup_target(queue->target_filename);
    return_if_fail(qt);

    qt->size = size;
    queue->size = size;

    /* FIXME: persistence? */
}

void queue_set_priority(const char *target_filename, unsigned priority)
{
    if(priority > 5)
    {
        WARNING("called with invalid priority %i, limited to 5", priority);
	priority = 5;
    }

    queue_target_t *qt = queue_lookup_target(target_filename);
    if(qt == NULL)
	return;

    if(qt->priority != priority)
    {
	qt->priority = priority;
	if(!q_store->loading)
	{
	    char *tmp = str_quote_backslash(target_filename, ":");
	    fprintf(q_store->fp, "=P:%s:%u\n", tmp, priority);
	    free(tmp);
	}

	nc_send_queue_priority_changed_notification(nc_default(),
	    target_filename, priority);
    }
    else
    {
	DEBUG("target [%s] already has priority %i",
	    target_filename, priority);
    }
}

void queue_send_to_ui(void)
{
    /* Send all filelists
     */
    struct queue_filelist *qf;
    TAILQ_FOREACH(qf, &q_store->filelists, link)
    {
        nc_send_filelist_added_notification(nc_default(),
                qf->nick, qf->priority);
    }

    /* Send all directories
     */
    struct queue_directory *qd;
    TAILQ_FOREACH(qd, &q_store->directories, link)
    {
        nc_send_queue_directory_added_notification(nc_default(),
                qd->target_directory, qd->nick);
    }

    /* Send all targets
     */
    struct queue_target *qt;
    TAILQ_FOREACH(qt, &q_store->targets, link)
    {
        nc_send_queue_target_added_notification(nc_default(),
                qt->filename, qt->size, qt->tth, qt->priority);
    }

    /* Send all sources
     */
    struct queue_source *qs;
    TAILQ_FOREACH(qs, &q_store->sources, link)
    {
        nc_send_queue_source_added_notification(nc_default(),
                qs->target_filename, qs->nick, qs->source_filename);
    }
}

#ifdef TEST

#include "globals.h"
#include "unit_test.h"

int got_filelist_notification = 0;
int got_target_removed_notification = 0;

void handle_filelist_added_notification(nc_t *nc, const char *channel,
        nc_filelist_added_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->nick);
    DEBUG("added filelist for %s", data->nick);
    fail_unless(strcmp(data->nick, "ba:r") == 0);
    got_filelist_notification = 1;
}

void handle_queue_target_removed_notification(nc_t *nc, const char *channel,
        nc_queue_target_removed_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_filename);
    DEBUG("removed target %s", data->target_filename);
    /* fail_unless(strcmp(data->target_directory, "target/directory") == 0); */
    got_target_removed_notification = 1;
}

void test_setup(void)
{
    global_working_directory = "/tmp/sp-queue-test.d";
    INFO("resetting queue database directory");
    system("/bin/rm -rf /tmp/sp-queue-test.d");
    system("mkdir /tmp/sp-queue-test.d");

    queue_init();

    fail_unless(queue_add("foo", "remote/path/to/file.img", 17471142,
                "file.img", "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    got_filelist_notification = 0;
}

void test_teardown(void)
{
    queue_close();
    INFO("resetting queue database directory");
    system("/bin/rm -rf /tmp/sp-queue-test.d");
}

/* Just add a file to the queue and check that we can get it back. */
void test_add_file(void)
{
    test_setup();

    /* set the target paused */
    queue_set_priority("file.img", 0);

    /* is there anything to download from "foo"? */
    queue_t *q = queue_get_next_source_for_nick("foo");
    /* no, the only queued file is paused */
    fail_unless(q == NULL);

    /* increase the priority */
    queue_set_priority("file.img", 3);

    q = queue_get_next_source_for_nick("foo");
    fail_unless(q);
    fail_unless(q->nick);
    fail_unless(strcmp(q->nick, "foo") == 0);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote/path/to/file.img") == 0);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "file.img") == 0);
    fail_unless(q->tth);
    fail_unless(strcmp(q->tth, "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(q->size == 17471142ULL);
    fail_unless(q->offset == 0ULL);
    fail_unless(q->is_filelist == 0);
    fail_unless(q->is_directory == 0);
    fail_unless(q->auto_matched == 0);

    /* we should also be able to look it up by the TTH */
    struct queue_target *qt = queue_lookup_target_by_tth(q->tth);
    fail_unless(qt);

    /* mark this file as active (ie, it is currently being downloaded) */
    queue_set_active(q, 1);
    queue_free(q);

    /* there shouldn't be any more sources for foo, the one and only file is
     * already active */
    fail_unless(!queue_has_source_for_nick("foo"));

    test_teardown();

    puts("PASS: queue: adding targets");
}

/* Add another source to the target and check the new source. Also remove the
 * target and verify both sources are empty.
 */
void test_add_source(void)
{
    test_setup();

    /* The source and target filenames are different, but the size and TTH
     * matches. This should override the filenames, and only add this file as
     * another source to the existing target. */
    fail_unless(queue_add("bar", "another/path/to_the/same-file.img", 17471142,
                "same-file.img", "IP4CTCABTUE6ZHZLFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    /* is there anything to download from "bar"? */
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "file.img") == 0);
    fail_unless(q->source_filename);
    DEBUG("source = [%s]", q->source_filename);
    fail_unless(strcmp(q->source_filename,
                "another/path/to_the/same-file.img") == 0);

    /* mark this file as active (ie, it is currently being downloaded) */
    queue_set_active(q, 1);

    /* this file is being downloaded from "bar", nothing to do for "foo" */
    fail_unless(!queue_has_source_for_nick("foo"));
    queue_free(q);

    got_target_removed_notification = 0;
    fail_unless(queue_remove_target("file.img") == 0);
    fail_unless(got_target_removed_notification == 1);

    /* the target is removed, nothing to do for both sources */
    fail_unless(!queue_has_source_for_nick("foo"));
    fail_unless(!queue_has_source_for_nick("bar"));

    test_teardown();

    puts("PASS: queue: adding sources");
}

/* Tests that the download order is correct.
 */
void test_queue_order(void)
{
    test_setup();

    fail_unless(queue_add("bar", "remote_file_0", 4096, "local_file_0",
                "ZXCVZXCVZXCVZXCVZXCVP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_2", 4096, "local_file_2",
                "QWERQWERQWRQWERLFS2OP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_1", 4096, "local_file_1",
                "ASDFASDFASDFASDFFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_0") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_0") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_2") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_2") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_1") == 0);
    queue_free(q);

    test_teardown();

    puts("PASS: queue: order");
}

void test_priorities(void)
{
    test_setup();

    fail_unless(queue_add("bar", "remote_file_0", 4096, "local_file_0",
                "ZXCVZXCVZXCVZXCVZXCVP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_2", 4096, "local_file_2",
                "QWERQWERQWRQWERLFS2OP5W7EMN3LMFS65H7D2Y") == 0);
    fail_unless(queue_add("bar", "remote_file_1", 4096, "local_file_1",
                "ASDFASDFASDFASDFFS2OP5W7EMN3LMFS65H7D2Y") == 0);

    queue_set_priority("local_file_2", 4);
    queue_set_priority("local_file_1", 2);
    queue_set_priority("local_file_0", 1);

    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_2") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_2") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_1") == 0);
    queue_free(q);
    fail_unless(queue_remove_target("local_file_1") == 0);

    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "remote_file_0") == 0);
    queue_free(q);

    test_teardown();

    puts("PASS: queue: priorities");
}

void test_filelist_dups(void)
{
    test_setup();

    fail_unless(queue_add_filelist("ba:r", 0) == 0);
    fail_unless(got_filelist_notification == 1);

    /* check that we can get the filelist */
    queue_filelist_t *qf = queue_lookup_filelist("ba:r");
    fail_unless(qf);

    /* If we add the filelist again, we should not get a duplicate notification */
    got_filelist_notification = 0;
    fail_unless(queue_add_filelist("ba:r", 0) == 0);
    fail_unless(got_filelist_notification == 0);

    test_teardown();
    puts("PASS: queue: filelist duplicates");
}

void test_persistence(void)
{
    INFO("testing persistence");
    test_setup();

    struct queue_target *qt = queue_lookup_target("file.img");
    fail_unless(qt);
    fail_unless(qt->priority == 3);

    queue_set_priority("file.img", 4);
    queue_add_filelist("ba:r", 1);
    queue_add_source("foo:2", "file.img", "sub\\dir\\sourcefile.img");

    /* add a new target... */
    fail_unless(queue_add("ba:r", "remote_file:0", 4096, "local_file:0",
                "ZXCVZXCVZXCVZXCVZXCVP5W7EMN3LMFS65H7D2Y") == 0);

    /* ...and remove it */
    fail_unless(queue_remove_target("local_file:0") == 0);

    /* close and re-open the queue
     */
    queue_close();
    queue_init();

    /* verify persistence of the queue
     */
    qt = queue_lookup_target("file.img");
    fail_unless(qt);
    fail_unless(qt->priority == 4);

    /* look up the standard source */
    queue_t *q = queue_get_next_source_for_nick("foo");
    fail_unless(q);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "file.img") == 0);
    queue_free(q);

    /* look up the extra source */
    q = queue_get_next_source_for_nick("foo:2");
    fail_unless(q);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "file.img") == 0);
    fail_unless(q->source_filename);
    DEBUG("source_filename = [%s]", q->source_filename);
    fail_unless(strcmp(q->source_filename, "sub\\dir\\sourcefile.img") == 0);
    queue_free(q);

    struct queue_filelist *qf = queue_lookup_filelist("ba:r");
    fail_unless(qf);
    fail_unless((qf->flags & QUEUE_TARGET_AUTO_MATCHED) ==
	QUEUE_TARGET_AUTO_MATCHED);

    /* look up the removed target */
    qt = queue_lookup_target("local_file_0");
    fail_unless(qt == NULL);

    queue_remove_filelist("ba:r");

    fail_unless( queue_remove_source("file.img", "foo:2") == 0 );

    /* close and re-open the queue again
     */
    queue_close();
    queue_init();

    /* this filelist should be removed */
    qf = queue_lookup_filelist("ba:r");
    fail_unless(qf == NULL);

    /* this source should be removed */
    q = queue_get_next_source_for_nick("foo:2");
    if(q)
	INFO("got unexpected target [%s]", q->target_filename);
    fail_unless(q == NULL);

    test_teardown();
}

void test_target_name_clashes(void)
{
    INFO("testing target name clashes");
    test_setup();

    /* Adding another target with the same name as a previous one but with
     * _different_ TTHs should result in the new targets name be modified.
     */
    fail_unless(queue_add("bar", "another/path/to/another-file.img", 17471142,
                "file.img", /* same target name as added in test_setup() */
		"DIFFERENTTTHTHATTHEPREVIOUSONE000123456") == 0);

    /* verify we can look up the download with a modified target filename */
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "file-1.img") == 0);
    queue_free(q);

    /* add yet another target with same name */

    const char *tth2 = "YETANOTHERTTHFORYETANOTHERFILE000123456";
    fail_unless(queue_add("bar",
	"yet/another/path/to/yet-another-file.img", 3123414,
	"file.img", /* same target name as added in test_setup() */
	tth2) == 0);

    /* look up the target by TTH and verify modified target filename */
    struct queue_target *qt = queue_lookup_target_by_tth(tth2);
    fail_unless(qt);
    fail_unless(qt->filename);
    fail_unless(strcmp(qt->filename, "file-2.img") == 0);

    test_teardown();
}

int main(void)
{
    sp_log_set_level("debug");

    nc_add_filelist_added_observer(nc_default(),
            handle_filelist_added_notification, NULL);
    nc_add_queue_target_removed_observer(nc_default(),
            handle_queue_target_removed_notification, NULL);

    test_add_file();
    test_add_source();
    test_queue_order();
    test_priorities();
    test_filelist_dups();
    test_persistence();
    test_target_name_clashes();

    return 0;
}

#endif

