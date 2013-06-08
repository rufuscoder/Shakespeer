#include "sys_queue.h"

#include <string.h>

#include "util.h"
#include "log.h"
#include "globals.h"
#include "queue.h"

extern struct queue_store *q_store;

int main(int argc, char **argv)
{
	global_working_directory = get_working_directory();
	sp_log_set_level("debug");
	queue_init();

	printf("Filelists:\n");
	queue_filelist_t *qf;
	TAILQ_FOREACH(qf, &q_store->filelists, link)
	{
		printf("%s\n", qf->nick);
	}

	printf("Directories:\n");
	queue_directory_t *qd;
	TAILQ_FOREACH(qd, &q_store->directories, link)
	{
		printf("%s (%i/%i left)\n",
			qd->target_directory, qd->nleft, qd->nfiles);
	}

	printf("Targets:\n");
	queue_target_t *qt;
	TAILQ_FOREACH(qt, &q_store->targets, link)
	{
		printf("%30s (directory %s)\n",
			qt->filename, qt->target_directory);
	}

	printf("Sources:\n");
	queue_source_t *qs;
	TAILQ_FOREACH(qs, &q_store->sources, link)
	{
		printf("%10s: %30s\n", qs->nick, qs->target_filename);
	}

	queue_close();

	return 0;
}

