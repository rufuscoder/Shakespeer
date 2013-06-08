#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "globals.h"
#include "tthdb.h"
#include "log.h"
#include "xstr.h"

int main(int argc, char **argv)
{
	if(argc > 1)
	global_working_directory = argv[1];
	else
	global_working_directory = "/tmp";

	sp_log_set_level("warning");

	tth_store_init();
	return_val_if_fail(global_tth_store, 1);

	uint64_t leafdata_size = 0;
	unsigned ntths = 0;

	struct tth_entry *te;
	RB_FOREACH(te, tth_entries_head,
	&((struct tth_store *)global_tth_store)->entries)
	{
		printf("%s:", te->tth);

		int rc = tth_store_load_leafdata(global_tth_store, te);
		if(rc == 0)
		{
			printf(" leafdata size=%u\n", te->leafdata_len);
			leafdata_size += te->leafdata_len;
		}
		else
			printf(" FAILED TO LOAD LEAFDATA\n");

		ntths++;
	}

	printf("\n%u tths, average leafdata size: %"PRIu64"\n",
	ntths, leafdata_size / (ntths == 0 ? 1 : ntths));

	tth_store_close();

	return 0;
}

