/*
 * Copyright (c) 2007 Martin Hedenfalk <martin@bzero.se>
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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>

#include "xstr.h"
#include "globals.h"
#include "log.h"
#include "queue.h"
#include "notifications.h"
#include "quote.h"
#include "compat.h"

#define QUEUE_DB_FILENAME "queue2.db"

struct queue_store *q_store = NULL;

static void queue_db_normalize(void);

static void
queue_parse_add_target(char *buf, size_t len)
{
	buf += 3;  /* skip past "+T:" */

	DEBUG("parsing target add [%s]", buf);

	/* syntax is 'filename:target_dir:size:tth:flags:ctime:prio:seq' */

	char *filename = q_strsep(&buf, ":");
	return_if_fail(filename && *filename);

	char *target_directory = q_strsep(&buf, ":");
	if(*target_directory == '\0')
		target_directory = NULL;

	char *size = q_strsep(&buf, ":");
	return_if_fail(size && *size);
	uint64_t s = strtoull(size, NULL, 10);

	char *tth = q_strsep(&buf, ":");
	return_if_fail(tth && *tth);

	char *flags = q_strsep(&buf, ":");
	return_if_fail(flags && *flags);
	int f = atoi(flags);

	char *ctime = q_strsep(&buf, ":");
	return_if_fail(ctime && *ctime);

	char *prio = q_strsep(&buf, ":");
	return_if_fail(prio && *prio);
	int p = atoi(prio);

	char *sequence = q_strsep(&buf, ":");
	return_if_fail(sequence && *sequence);
	unsigned seq = atoi(sequence);

	queue_target_add(filename, tth, target_directory, s, f, p, seq);
}

static void
queue_parse_add_source(char *buf, size_t len)
{
	buf += 3;  /* skip past "+S:" */

	/* syntax is 'nick:target_filename:source_filename' */

	char *nick = q_strsep(&buf, ":");
	char *target_filename = q_strsep(&buf, ":");
	char *source_filename = q_strsep(&buf, ":");

	return_if_fail(*nick);
	return_if_fail(*target_filename);
	return_if_fail(*source_filename);

	queue_add_source(nick, target_filename, source_filename);
}

static void
queue_parse_add_filelist(char *buf, size_t len)
{
	buf += 3;  /* skip past "+F:" */

	/* syntax is 'nick:flags' */
	char *nick = q_strsep(&buf, ":");
	return_if_fail(*nick);

	int flags = atoi(buf);

	bool auto_matched = (flags & QUEUE_TARGET_AUTO_MATCHED) ==
		QUEUE_TARGET_AUTO_MATCHED;

	queue_add_filelist(nick, auto_matched);
}

static void
queue_parse_add_directory(char *buf, size_t len)
{
	buf += 3;  /* skip past "+D:" */

	/* syntax is 'target_directory:nick:source_directory */

	char *target_directory = q_strsep(&buf, ":");
	char *nick = q_strsep(&buf, ":");
	char *source_directory = q_strsep(&buf, ":");

	return_if_fail(*target_directory);
	return_if_fail(*nick);
	return_if_fail(*source_directory);

	queue_db_add_directory(target_directory, nick, source_directory);
}

static void
queue_parse_remove_directory(char *buf, size_t len)
{
	buf += 3;  /* skip past "-D:" */

	/* syntax is 'target_directory' */
	queue_remove_directory(buf);
}

static void
queue_parse_set_directory_resolved(char *buf, size_t len)
{
	buf += 3;  /* skip past "=R:" */

	/* syntax is 'target_directory:nfiles' */

	char *target_directory = q_strsep(&buf, ":");
	return_if_fail(*target_directory);

	/* FIXME: use strtonum */
	unsigned nfiles = strtoul(buf, NULL, 10);

	queue_db_set_resolved(target_directory, nfiles);
}

static void
queue_parse_set_priority(char *buf, size_t len)
{
	buf += 3;  /* skip past "=P:" */

	/* syntax is 'target_filename:priority' */

	char *target_filename = q_strsep(&buf, ":");
	return_if_fail(*target_filename);
	int priority = atoi(buf);

	queue_set_priority(target_filename, priority);
}

static void
queue_parse_remove_target(char *buf, size_t len)
{
	buf += 3;  /* skip past "-T:" */

	/* syntax is 'target_filename' */
	queue_remove_target(buf);
}

static void
queue_parse_remove_filelist(char *buf, size_t len)
{
	buf += 3;  /* skip past "-F:" */

	/* syntax is 'nick' */
	queue_remove_filelist(buf);
}

static void
queue_parse_remove_source(char *buf, size_t len)
{
	buf += 3;  /* skip past "-S:" */

	/* syntax is 'target_filename:nick' */

	char *target_filename = q_strsep(&buf, ":");
	char *nick = q_strsep(&buf, ":");
	return_if_fail(*target_filename && *nick);

	queue_remove_source(target_filename, nick);
}

static void
queue_load(void)
{
	return_if_fail(q_store);
	return_if_fail(q_store->fp);

	rewind(q_store->fp);
	q_store->loading = true;

	int ntargets = 0, nsources = 0, nfilelists = 0, ndirectories = 0;
	char *buf, *lbuf = NULL;
	size_t len;
	while((buf = fgetln(q_store->fp, &len)) != NULL)
	{
		q_store->line_number++;

		if(buf[len - 1] == '\n')
			buf[len - 1] = 0;
		else
		{
			/* EOF without EOL, copy and add the NUL */
			lbuf = malloc(len + 1);
			assert(lbuf);
			memcpy(lbuf, buf, len);
			lbuf[len] = 0;
			buf = lbuf;
		}

		DEBUG("read [%s], len %zu", buf, len);

		if(len < 3 || buf[2] != ':')
			continue;

		if(strncmp(buf, "+T:", 3) == 0)
		{
			queue_parse_add_target(buf, len);
			ntargets++;
		}
		else if(strncmp(buf, "-T:", 3) == 0)
		{
			queue_parse_remove_target(buf, len);
			ntargets--;
		}
		else if(strncmp(buf, "+S:", 3) == 0)
		{
			queue_parse_add_source(buf, len);
			nsources++;
		}
		else if(strncmp(buf, "-S:", 3) == 0)
		{
			queue_parse_remove_source(buf, len);
			nsources--;
		}
		else if(strncmp(buf, "+F:", 3) == 0)
		{
			queue_parse_add_filelist(buf, len);
			nfilelists++;
		}
		else if(strncmp(buf, "-F:", 3) == 0)
		{
			queue_parse_remove_filelist(buf, len);
			nfilelists--;
		}
		else if(strncmp(buf, "+D:", 3) == 0)
		{
			queue_parse_add_directory(buf, len);
			ndirectories++;
		}
		else if(strncmp(buf, "-D:", 3) == 0)
		{
			queue_parse_remove_directory(buf, len);
			ndirectories--;
		}
		else if(strncmp(buf, "=R:", 3) == 0)
		{
			queue_parse_set_directory_resolved(buf, len);
		}
		else if(strncmp(buf, "=P:", 3) == 0)
		{
			queue_parse_set_priority(buf, len);
		}
		else
		{
			ERROR("unknown directive on line %u",
				q_store->line_number);
		}
	}
	free(lbuf);

	INFO("loaded %i targets, %i sources, %i filelists, %i directories (%u lines)",
		ntargets, nsources, nfilelists, ndirectories, q_store->line_number);

	q_store->loading = false;
}

static void
queue_db_open_logfile(void)
{
	return_if_fail(q_store);

	if(q_store->fp)
		fclose(q_store->fp);

	DEBUG("opening databases in file %s", QUEUE_DB_FILENAME);
	char *qlog_filename;
	int num_returned_bytes = asprintf(&qlog_filename, "%s/%s", global_working_directory, QUEUE_DB_FILENAME);
	if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
	q_store->fp = fopen(qlog_filename, "a+");
	if(q_store->fp == NULL)
		ERROR("%s: %s", qlog_filename, strerror(errno));
	free(qlog_filename);
}

void
queue_init(void)
{
	INFO("initializing queue");

	q_store = calloc(1, sizeof(struct queue_store));
	q_store->sequence = 1;

	TAILQ_INIT(&q_store->targets);
	TAILQ_INIT(&q_store->sources);
	TAILQ_INIT(&q_store->filelists);
	TAILQ_INIT(&q_store->directories);

	queue_db_open_logfile();

	return_if_fail(q_store->fp);
	queue_load();

	/* set the log file line buffered */
	setvbuf(q_store->fp, NULL, _IOLBF, 0);
}

void
queue_target_free(struct queue_target *qt)
{
	if(qt)
	{
		TAILQ_REMOVE(&q_store->targets, qt, link);
		free(qt->filename);
		free(qt->target_directory);
		free(qt);
	}
}

void
queue_source_free(struct queue_source *qs)
{
	if(qs)
	{
		TAILQ_REMOVE(&q_store->sources, qs, link);
		free(qs->target_filename);
		free(qs->nick);
		free(qs->source_filename);
		free(qs);
	}
}

static void
queue_filelist_free(struct queue_filelist *qf)
{
	if(qf)
	{
		TAILQ_REMOVE(&q_store->filelists, qf, link);
		free(qf->nick);
		free(qf);
	}
}

static void
queue_directory_free(struct queue_directory *qd)
{
	if(qd)
	{
		TAILQ_REMOVE(&q_store->directories, qd, link);
		free(qd->target_directory);
		free(qd->nick);
		free(qd->source_directory);
		free(qd);
	}
}

void
queue_close(void)
{
	return_if_fail(q_store);

	INFO("closing queue");

	queue_db_normalize();

	fclose(q_store->fp);
	q_store->fp = NULL;

	struct queue_target *qt;
	while((qt = TAILQ_FIRST(&q_store->targets)) != NULL)
		queue_target_free(qt);

	struct queue_source *qs;
	while((qs = TAILQ_FIRST(&q_store->sources)) != NULL)
		queue_source_free(qs);

	struct queue_filelist *qf;
	while((qf = TAILQ_FIRST(&q_store->filelists)) != NULL)
		queue_filelist_free(qf);

	struct queue_directory *qd;
	while((qd = TAILQ_FIRST(&q_store->directories)) != NULL)
		queue_directory_free(qd);

	free(q_store);
	q_store = NULL;

	INFO("queue closed");
}

queue_target_t *
queue_lookup_target(const char *target_filename)
{
	return_val_if_fail(q_store, NULL);
	return_val_if_fail(target_filename, NULL);

	struct queue_target *qt;
	TAILQ_FOREACH(qt, &q_store->targets, link)
	{
		if(strcmp(qt->filename, target_filename) == 0)
			return qt;
	}

	return NULL;
}

queue_target_t *
queue_lookup_target_by_tth(const char *tth)
{
	return_val_if_fail(q_store, NULL);
	return_val_if_fail(tth, NULL);

	struct queue_target *qt;
	TAILQ_FOREACH(qt, &q_store->targets, link)
	{
		if(strcmp(qt->tth, tth) == 0)
			return qt;
	}

	return NULL;
}

queue_filelist_t *
queue_lookup_filelist(const char *nick)
{
	return_val_if_fail(q_store, NULL);
	return_val_if_fail(nick, NULL);

	struct queue_filelist *qf;
	TAILQ_FOREACH(qf, &q_store->filelists, link)
	{
		if(strcmp(qf->nick, nick) == 0)
			return qf;
	}

	return NULL;
}

queue_directory_t *
queue_db_lookup_directory(const char *target_directory)
{
	return_val_if_fail(q_store, NULL);
	return_val_if_fail(target_directory, NULL);

	struct queue_directory *qd;
	TAILQ_FOREACH(qd, &q_store->directories, link)
	{
		if(strcmp(qd->target_directory, target_directory) == 0)
			return qd;
	}

	return NULL;
}

struct queue_target *
queue_target_add(const char *target_filename,
	const char *tth,
	const char *target_directory,
	uint64_t size,
	unsigned flags,
	int priority,
	unsigned sequence)
{
	/* Make sure the target filename is unique
	 */
	char *unique_target_filename = strdup(target_filename);
	int index = 1;
	char *base_filename = NULL, *extension = NULL;

	struct queue_target *qt = NULL;
	while(true)
	{
		qt = queue_lookup_target(unique_target_filename);
		if(qt == NULL)
			break;

		/* If the TTH is the same, the caller is using this
		 * the wrong way. The caller should only add another
		 * source.
		 */
		if(tth && qt->tth)
			return_val_if_fail(strcmp(tth, qt->tth) != 0, NULL);

		if(base_filename == NULL)
		{
			char *last_dot = strrchr(target_filename, '.');
			if(last_dot == NULL)
			{
				base_filename = strdup(target_filename);
				extension = strdup("");
			}
			else
			{
				base_filename = xstrndup(target_filename,
					last_dot - target_filename);
				extension = strdup(last_dot + 1);
			}
		}

		/* ok, missing or differing TTHs, make target filename unique */
		free(unique_target_filename);
		int num_returned_bytes = asprintf(&unique_target_filename, "%s-%i.%s", base_filename, index++, extension);
		if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
	}

	free(base_filename);
	free(extension);

	DEBUG("adding target [%s]", unique_target_filename);

        qt = calloc(1, sizeof(struct queue_target));
	qt->filename = strdup(unique_target_filename);
        if(tth)
            strlcpy(qt->tth, tth, sizeof(qt->tth));
	qt->target_directory = xstrdup(target_directory);
        qt->size = size;
        time(&qt->ctime);
	qt->flags = flags;
        qt->priority = priority;

	if(sequence == 0)
		qt->seq = q_store->sequence++;
	else
	{
		qt->seq = sequence;
		if(sequence >= q_store->sequence)
			q_store->sequence = sequence + 1;
	}

	TAILQ_INSERT_TAIL(&q_store->targets, qt, link);

	if(!q_store->loading)
	{
		queue_db_print_add_target(q_store->fp, qt);

		/* notify UI:s */
		nc_send_queue_target_added_notification(nc_default(),
			qt->filename, qt->size, qt->tth, qt->priority);
	}

	return qt;
}

/* returns 0 if the source was added, else non-zero */
int
queue_add_source(const char *nick, const char *target_filename,
	const char *source_filename)
{
	return_val_if_fail(nick, -1);
	return_val_if_fail(source_filename, -1);
	return_val_if_fail(target_filename, -1);

	/* Lookup the (nick, target_filename) pair.
	 */
	struct queue_source *qs = NULL;
	TAILQ_FOREACH(qs, &q_store->sources, link)
	{
		if(strcmp(qs->nick, nick) == 0 &&
		   strcmp(qs->target_filename, target_filename) == 0)
		{
			break;
		}
	}

	if(qs == NULL)
	{
		qs = calloc(1, sizeof(struct queue_source));
		qs->nick = strdup(nick);
		qs->target_filename = strdup(target_filename);
		qs->source_filename = strdup(source_filename);

		DEBUG("adding nick [%s], source [%s] for target [%s]",
			nick, source_filename, target_filename);

		TAILQ_INSERT_TAIL(&q_store->sources, qs, link);

		if(!q_store->loading)
			queue_db_print_add_source(q_store->fp, qs);
	}
	else
	{
		INFO("already got [%s] as a source for [%s]",
			nick, qs->target_filename);
		return -1;
	}

	/* FIXME: what if source_filename has changed? possible? */

	return 0; /* queue source was added */
}

int
queue_remove_filelist(const char *nick)
{
	return_val_if_fail(q_store, -1);
	return_val_if_fail(nick, -1);

	struct queue_filelist *qf = queue_lookup_filelist(nick);
	if(qf)
	{
		queue_filelist_free(qf);

		if(!q_store->loading)
		{
			char *tmp = str_quote_backslash(nick, ":");
			fprintf(q_store->fp, "-F:%s\n", tmp);
			free(tmp);
		}

		/* notify ui:s */
		nc_send_filelist_removed_notification(nc_default(), nick);
	}
	else
		WARNING("no such filelist: [%s]", nick);

	return 0;
}

int
queue_db_remove_target(const char *target_filename)
{
	return_val_if_fail(q_store, -1);
	return_val_if_fail(target_filename, -1);

	struct queue_target *qt = queue_lookup_target(target_filename);
	if(qt)
	{
		DEBUG("removing target [%s]", qt->filename);

		if(!q_store->loading)
		{
			char *tmp = str_quote_backslash(target_filename, ":");
			fprintf(q_store->fp, "-T:%s\n", tmp);
			free(tmp);
		}

		/* notify ui:s */
		nc_send_queue_target_removed_notification(nc_default(),
			target_filename);

		queue_target_free(qt);
	}

	return 0;
}

int
queue_remove_target(const char *target_filename)
{
	return_val_if_fail(target_filename, -1);

	queue_directory_t *qd = NULL;
	queue_target_t *qt = queue_lookup_target(target_filename);
	if(qt == NULL)
	{
		WARNING("Target [%s] doesn't exist", target_filename);
	}
	else if(qt->target_directory)
	{
		/* this target belongs to a directory download */
		qd = queue_db_lookup_directory(qt->target_directory);
		if(qd)
		{
			qd->nleft--;
			/* when replaying the log, this should be updated */
		}
		else
		{
			WARNING("Target directory [%s] doesn't exist!?",
				qt->target_directory);
		}
	}

	/* If we're downloading a directory, check if the whole directory is
	 * complete. */
	if(qd)
	{
		DEBUG("directory [%s] has %u files left",
			qt->target_directory, qd->nleft);
		if(qd->nleft == 0)
		{
			queue_db_remove_directory(qt->target_directory);
		}
	}

	if(queue_db_remove_target(target_filename) != 0)
		return -1;

	/* target removed, now remove all its sources */
	queue_remove_sources(target_filename);

	return 0;
}

/* removes all sources for the target filename */
int
queue_remove_sources(const char *target_filename)
{
	return_val_if_fail(target_filename, -1);

	DEBUG("removing sources for target [%s]", target_filename);

	struct queue_source *qs, *next;
	for(qs = TAILQ_FIRST(&q_store->sources); qs; qs = next)
	{
		next = TAILQ_NEXT(qs, link);
		if(strcmp(target_filename, qs->target_filename) == 0)
		{
			DEBUG("removing source [%s], target [%s]",
				qs->nick, qs->target_filename);

			if(!q_store->loading)
				queue_db_print_remove_source(q_store->fp, qs);
			queue_source_free(qs);
		}
	}

	return 0;
}

int
queue_remove_sources_by_nick(const char *nick)
{
	return_val_if_fail(nick, -1);

	DEBUG("removing sources for nick [%s]", nick);

	struct queue_source *qs, *next;
	for(qs = TAILQ_FIRST(&q_store->sources); qs; qs = next)
	{
		next = TAILQ_NEXT(qs, link);

		if(strcmp(qs->nick, nick) != 0)
			continue;

		DEBUG("removing source [%s], target [%s]",
			nick, qs->target_filename);

		if(!q_store->loading)
			queue_db_print_remove_source(q_store->fp, qs);

		nc_send_queue_source_removed_notification(nc_default(),
			qs->target_filename, nick);

		queue_source_free(qs);
	}

	return 0;
}

void
queue_db_add_directory(const char *target_directory,
	const char *nick, const char *source_directory)
{
	return_if_fail(target_directory);
	return_if_fail(nick);
	return_if_fail(source_directory);

	struct queue_directory *qd = queue_db_lookup_directory(target_directory);
	if(qd == NULL)
	{
		DEBUG("target_directory [%s]", target_directory);
		DEBUG("source_directory [%s]", source_directory);
		DEBUG("nick [%s]", nick);

		qd = calloc(1, sizeof(struct queue_directory));
		qd->target_directory = strdup(target_directory);
		TAILQ_INSERT_TAIL(&q_store->directories, qd, link);
	}

	free(qd->nick);
	free(qd->source_directory);
	qd->nick = strdup(nick);
	qd->source_directory = strdup(source_directory);

	if(!q_store->loading)
		queue_db_print_add_directory(q_store->fp, qd);
}

int
queue_db_remove_directory(const char *target_directory)
{
	DEBUG("removing directory [%s]", target_directory);

	struct queue_directory *qd =
		queue_db_lookup_directory(target_directory);
	if(qd == NULL)
		return 0;

	if(!q_store->loading)
	{
		char *tmp = str_quote_backslash(target_directory, ":");
		fprintf(q_store->fp, "-D:%s\n", tmp);
		free(tmp);
	}

	/* notify ui:s */
	nc_send_queue_directory_removed_notification(nc_default(),
		target_directory);

	queue_directory_free(qd);

	return 0;
}

queue_directory_t *
queue_db_lookup_unresolved_directory_by_nick(const char *nick)
{
	return_val_if_fail(nick, NULL);

	struct queue_directory *qd;
	TAILQ_FOREACH(qd, &q_store->directories, link)
	{
		if(strcmp(nick, qd->nick) == 0 &&
			(qd->flags & QUEUE_DIRECTORY_RESOLVED) == 0)
		{
			DEBUG("nick [%s], target [%s], source [%s], flags %i",
				qd->nick, qd->target_directory,
				qd->source_directory, qd->flags);
			return qd;
		}
	}

	return NULL;
}

void
queue_db_set_resolved(const char *target_directory, unsigned nfiles)
{
	DEBUG("setting [%s] as resolved, [%u] files", target_directory, nfiles);

	return_if_fail(target_directory);
	queue_directory_t *qd = queue_db_lookup_directory(target_directory);
	return_if_fail(qd);

	if((qd->flags & QUEUE_DIRECTORY_RESOLVED) == QUEUE_DIRECTORY_RESOLVED)
	{
		WARNING("Directory [%s] already resolved!", target_directory);
	}
	else
	{
		qd->flags |= QUEUE_DIRECTORY_RESOLVED;
		qd->nfiles = nfiles;
		qd->nleft = nfiles;

		DEBUG("Updating directory [%s] with resolved flag",
			target_directory);

		/* log resolved and number of files in the directory */
		if(!q_store->loading)
			queue_db_print_set_resolved(q_store->fp, qd);
	}
}

struct queue_target *
queue_target_duplicate(struct queue_target *qt)
{
	return_val_if_fail(qt, NULL);

	struct queue_target *qt_dup = calloc(1, sizeof(struct queue_target));
	memcpy(qt_dup, qt, sizeof(struct queue_target));
	qt_dup->filename = xstrdup(qt->filename);
	qt_dup->target_directory = xstrdup(qt->target_directory);

	return qt_dup;
}

int
queue_db_print_add_target(FILE *fp, struct queue_target *qt)
{
	return_val_if_fail(fp, -1);
	return_val_if_fail(qt, -1);

	char *tmp1 = str_quote_backslash(qt->filename, ":");
	char *tmp2 = str_quote_backslash(qt->target_directory, ":");

	int rc = fprintf(fp,
		"+T:%s:%s:%"PRIu64":%s:%u:%lu:%i:%u\n",
		tmp1,
		tmp2 ? tmp2 : "",
		qt->size,
		qt->tth ? qt->tth : "",
		qt->flags,
		(unsigned long)qt->ctime,
		qt->priority,
		qt->seq);

	free(tmp1);
	free(tmp2);

	return rc;
}

int
queue_db_print_add_source(FILE *fp, struct queue_source *qs)
{
	return_val_if_fail(fp, -1);
	return_val_if_fail(qs, -1);

	char *tmp1 = str_quote_backslash(qs->nick, ":");
	char *tmp2 = str_quote_backslash(qs->target_filename, ":");
	char *tmp3 = str_quote_backslash(qs->source_filename, ":");
	int rc = fprintf(fp, "+S:%s:%s:%s\n", tmp1, tmp2, tmp3);
	free(tmp1);
	free(tmp2);
	free(tmp3);

	return rc;
}

int
queue_db_print_remove_source(FILE *fp, struct queue_source *qs)
{
	return_val_if_fail(fp, -1);
	return_val_if_fail(qs, -1);

	char *tmp1 = str_quote_backslash(qs->target_filename, ":");
	char *tmp2 = str_quote_backslash(qs->nick, ":");
	int rc = fprintf(q_store->fp, "-S:%s:%s\n", tmp1, tmp2);
	free(tmp1);
	free(tmp2);

	return rc;
}

int
queue_db_print_add_filelist(FILE *fp, struct queue_filelist *qf)
{
	return_val_if_fail(fp, -1);
	return_val_if_fail(qf, -1);

	char *tmp = str_quote_backslash(qf->nick, ":");
	int rc = fprintf(fp, "+F:%s:%i\n", tmp, qf->flags);
	free(tmp);

	return rc;
}

int
queue_db_print_add_directory(FILE *fp, struct queue_directory *qd)
{
	return_val_if_fail(fp, -1);
	return_val_if_fail(qd, -1);

	char *tmp1 = str_quote_backslash(qd->target_directory, ":");
	char *tmp2 = str_quote_backslash(qd->nick, ":");
	char *tmp3 = str_quote_backslash(qd->source_directory, ":");
	int rc = fprintf(fp, "+D:%s:%s:%s\n", tmp1, tmp2, tmp3);
	free(tmp1);
	free(tmp2);
	free(tmp3);

	return rc;
}

int
queue_db_print_set_resolved(FILE *fp, struct queue_directory *qd)
{
	return_val_if_fail(fp, -1);
	return_val_if_fail(qd, -1);

	int rc = 0;
	if((qd->flags & QUEUE_DIRECTORY_RESOLVED) == QUEUE_DIRECTORY_RESOLVED)
	{
		char *tmp = str_quote_backslash(qd->target_directory, ":");
		rc = fprintf(fp, "=R:%s:%u\n", tmp, qd->nfiles);
		free(tmp);
	}

	return rc;
}

static int
queue_db_save(FILE *fp)
{
	return_val_if_fail(fp, -1);
	return_val_if_fail(q_store, -1);
	return_val_if_fail(!q_store->loading, -1);

	/* save targets */
	struct queue_target *qt;
	TAILQ_FOREACH(qt, &q_store->targets, link)
	{
		if(queue_db_print_add_target(fp, qt) < 0)
			return -1;
	}

	/* save sources */
	struct queue_source *qs;
	TAILQ_FOREACH(qs, &q_store->sources, link)
	{
		if(queue_db_print_add_source(fp, qs) < 0)
			return -1;
	}

	/* save filelists */
	struct queue_filelist *qf;
	TAILQ_FOREACH(qf, &q_store->filelists, link)
	{
		if(queue_db_print_add_filelist(fp, qf) < 0)
			return -1;
	}

	/* save directories */
	struct queue_directory *qd;
	TAILQ_FOREACH(qd, &q_store->directories, link)
	{
		if(qd->nleft > 0)
		{
			queue_db_print_add_directory(fp, qd);
			queue_db_print_set_resolved(fp, qd);
		}
	}

	return 0;
}

static void
queue_db_normalize(void)
{
	INFO("normalizing queue database");

	char *tmpfile;
	int num_returned_bytes;
	num_returned_bytes = asprintf(&tmpfile, "%s/%s.tmp", global_working_directory, QUEUE_DB_FILENAME);
	if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
	FILE *tmp_fp = fopen(tmpfile, "w"); /* truncate existing file */
	if (tmp_fp == NULL) {
		ERROR("failed to open temporary queue database: %s", strerror(errno));
		free(tmpfile);
		return;
	}

	int rc = queue_db_save(tmp_fp);
	if (fclose(tmp_fp) == 0) {
		if (rc != 0) {
			free(tmpfile);
			return;
		}

		char *qlog_filename;
		num_returned_bytes = asprintf(&qlog_filename, "%s/%s", global_working_directory, QUEUE_DB_FILENAME);
		if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");

		INFO("atomically replacing queue database");

		if(rename(tmpfile, qlog_filename) != 0) 
		{
			ERROR("rename: %s", strerror(errno));
		}
		free(qlog_filename);

		queue_db_open_logfile();
	}
	else
		WARNING("failed to close file: %s", strerror(errno));
	free(tmpfile);
}

