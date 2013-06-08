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

#ifndef _queue_h_
#define _queue_h_

#define QUEUE_TARGET_ACTIVE 1
#define QUEUE_TARGET_AUTO_MATCHED 2
#define QUEUE_DIRECTORY_RESOLVED 4

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

typedef struct queue_target queue_target_t;
struct queue_target
{
	TAILQ_ENTRY(queue_target) link;

	char *filename; /* target filename in local filesystem */
	char tth[40];

	char *target_directory; /* relative to download directory */
	uint64_t size;
	unsigned flags;
	time_t ctime;
	int priority;
	unsigned seq;
};

typedef struct queue_source queue_source_t;
struct queue_source
{
	TAILQ_ENTRY(queue_source) link;

	char *target_filename;
	char *nick;
	char *source_filename;
};

typedef struct queue_filelist queue_filelist_t;
struct queue_filelist
{
	TAILQ_ENTRY(queue_filelist) link;

	char *nick;

	unsigned flags;
	int priority;
};

typedef struct queue_directory queue_directory_t;
struct queue_directory
{
	TAILQ_ENTRY(queue_directory) link;

	char *target_directory;
	char *nick;

	char *source_directory;
	unsigned flags;
	unsigned nfiles;
	unsigned nleft;
};

struct queue_store
{
	FILE *fp;
	unsigned line_number;
	bool loading;
	unsigned sequence;

	TAILQ_HEAD(, queue_target) targets;
	TAILQ_HEAD(, queue_source) sources;
	TAILQ_HEAD(, queue_filelist) filelists;
	TAILQ_HEAD(, queue_directory) directories;
};

typedef struct queue queue_t;
struct queue
{
	char *nick;
	char *source_filename;
	char *target_filename;
	char *tth;
	uint64_t size;
	uint64_t offset;
	bool is_filelist;
	bool is_directory;
	bool auto_matched;
};

void queue_init(void);
void queue_close(void);

void queue_free(queue_t *queue);

queue_target_t *queue_lookup_target(const char *target_filename);
queue_target_t *queue_lookup_target_by_tth(const char *tth);

struct queue_target *queue_target_add(const char *target_filename,
	const char *tth,
	const char *target_directory,
	uint64_t size,
	unsigned flags,
	int priority,
	unsigned sequence);

struct queue_target *queue_target_duplicate(struct queue_target *qt);

void queue_target_free(struct queue_target *qt);
void queue_source_free(struct queue_source *qs);

int queue_add_target(queue_target_t *qt);
int queue_db_remove_target(const char *target_filename);
int queue_remove_target(const char *target_filename);

int queue_db_print_add_target(FILE *fp, struct queue_target *qt);
int queue_db_print_add_source(FILE *fp, struct queue_source *qs);
int queue_db_print_remove_source(FILE *fp, struct queue_source *qs);
int queue_db_print_add_filelist(FILE *fp, struct queue_filelist *qf);
int queue_db_print_add_directory(FILE *fp, struct queue_directory *qd);
int queue_db_print_set_resolved(FILE *fp, struct queue_directory *qd);

int queue_add_source(const char *nick, const char *target_filename,
        const char *source_filename);

int queue_remove_sources(const char *target_filename);
int queue_remove_sources_by_nick(const char *nick);

int queue_add_filelist(const char *nick, bool auto_matched_filelist);
int queue_db_add_filelist(const char *nick, queue_filelist_t *qf);
int queue_update_filelist(const char *nick, queue_filelist_t *qf);
int queue_remove_filelist(const char *nick);

void queue_db_add_directory(const char *target_directory,
        const char *nick, const char *source_directory);

queue_directory_t *queue_db_lookup_directory(const char *target_directory);
queue_directory_t *queue_db_lookup_unresolved_directory_by_nick(const char *nick);
int queue_db_remove_directory(const char *target_directory);

void queue_db_set_resolved(const char *target_directory, unsigned nfiles);

queue_filelist_t *queue_lookup_filelist(const char *nick);

int queue_add_internal(const char *nick, const char *remote_filename,
        uint64_t size, const char *local_filename, const char *tth,
        int auto_matched_filelist, const char *target_directory);

void queue_set_size(queue_t *queue, uint64_t size);
void queue_set_active(queue_t *queue, int flag);
queue_t *queue_get_next_source_for_nick(const char *nick);
bool queue_has_source_for_nick(const char *nick);
int queue_set_target_filename(queue_t *queue, const char *filename);
void queue_set_source_filename(queue_t *queue, const char *filename);
int queue_remove_directory(const char *target_directory);

void queue_send_to_ui(void);

int queue_remove_source(const char *local_filename, const char *nick);
int queue_add(const char *nick, const char *remote_filename, uint64_t size,
        const char *local_filename, const char *tth);
int queue_add_directory(const char *nick,
        const char *source_directory,
        const char *target_directory);
int queue_remove_nick(const char *nick);
void queue_set_priority(const char *target_filename, unsigned priority);

/* queue_auto_search.c
 */
void queue_auto_search_init(void);
void queue_schedule_auto_search_sources(int enable);

/* queue_resolve.c
 */
int queue_resolve_directory(const char *nick,
        const char *source_directory,
        const char *target_directory,
        unsigned *nfiles_p);

/* queue_connect.c
 */
typedef int (*queue_connect_callback_t)(const char *nick, void *user_data);

void queue_connect_set_interval(int seconds);
void queue_connect_schedule_trigger(queue_connect_callback_t callback_function);

#endif

