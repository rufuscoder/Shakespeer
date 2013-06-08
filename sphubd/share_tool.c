#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>

#include <sys/types.h>
#include <event.h>

#include "share.h"
#include "log.h"
#include "xstr.h"

#include "globals.h"
#include "notifications.h"
#include "sphashd_client.h"

#define CLRON "\e[34m"
#define CLROFF "\e[0m"

static int num_shares = 0;
static char *search_string = NULL;
static int save_filelist_flag = 0;

void initiate_search(void);

int ui_send_status_message(void *ui, const char *hub_address, const char *message, ...)
{
	printf("%s\n", message);
	return 0;
}

static void show_share_stats(void)
{
    /* Get and print statistics.
     */

    if(isatty(1))
    {
        share_stats_t stats;

        share_get_stats(global_share, &stats);
        unsigned nunique = stats.ntotfiles - stats.nduplicates;
        uint64_t uniqsize = stats.totsize - stats.size;
        printf(CLRON
                "Sharing %s, %u files total (%u duplicates),"
                " %u of %u unique files hashed,"
                " %s of %s hashed (%.1f%%)."
                CLROFF "\n",
                str_size_human(stats.size),
                stats.ntotfiles, stats.nduplicates,
                stats.nfiles, nunique,
                str_size_human(stats.size), str_size_human(uniqsize),
                100 * ((double)stats.size / (double)uniqsize));
    }
}

void handle_hashing_complete_notification(
        nc_t *nc,
        const char *channel,
        nc_hashing_complete_t *data,
        void *user_data)
{
    if(isatty(1))
        printf(CLRON "Finished hashing all files" CLROFF "\n");
}

void handle_share_scan_finished_notification(
        nc_t *nc,
        const char *channel,
        nc_share_scan_finished_t *data,
        void *user_data)
{
    if(isatty(1))
        printf(CLRON "Done scanning %s" CLROFF "\n", data->path);
    printf("%i directories left to scan\n", --num_shares);
    if(num_shares == 0)
    {
        initiate_search();

        if(save_filelist_flag)
        {
            share_save(global_share, FILELIST_XML | FILELIST_DCLST);
        }
    }
}

static void shutdown_event(int fd, short why, void *data)
{
    printf(CLRON "Catched Ctrl-C, exiting..." CLROFF "\n");
    hs_shutdown();
    show_share_stats();

    /* share_close(global_share); */
    tth_store_close();
    global_share = 0;

    /* be nice to valgrind */
    free(global_working_directory);
    sp_log_close();

    exit(6);
}

int search_match_cb(const share_search_t *search,
        share_file_t *file, const char *tth, void *data)
{
    printf(CLRON "search match: %s%s, TTH/%s" CLROFF "\n",
	file->mp->local_root, file->partial_path, tth);
    /* if(tth) */
        /* printf("TTH:%s\n", tth); */
    return 0;
}

static void timer_event(int fd, short why, void *data)
{
    show_share_stats();
    struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
    evtimer_add(data, &tv);
}

void do_search(char *search_string)
{
    share_search_t s;
    memset(&s, 0, sizeof(s));

    s.size_restriction = SHARE_SIZE_NONE;

    if(str_has_prefix(search_string, "TTH:"))
    {
        s.tth = search_string + 4;
        s.type = SHARE_TYPE_TTH;
        printf(CLRON "Searching for TTH %s" CLROFF "\n", s.tth);
    }
    else
    {
        s.words = arg_create(search_string, " \t,-_", 0);
        s.type = SHARE_TYPE_ANY;
        printf(CLRON "Searching for '%s'" CLROFF "\n", search_string);
    }

    share_search(global_share, &s, search_match_cb, NULL);
}

void initiate_search(void)
{
    if(search_string)
    {
        if(strcmp(search_string, "-") == 0)
        {
            char buf[1024];
            while(fgets(buf, sizeof(buf), stdin) != NULL)
            {
                str_trim_end_inplace(buf, NULL);
                do_search(buf);
            }
        }
        else
        {
            do_search(search_string);
        }
    }
}

int main(int argc, char **argv)
{
    /* FIXME: DRY (sphubd.c) */
    char *p, *e;
    p = strdup(argv[0]);
    e = strrchr(p, '/');
    if(e)
        *e = 0;
    else
    {
        free(p);
        p = strdup(".");
    }
    argv0_path = absolute_path(p);
    free(p);

	global_incomplete_directory = ".";
	global_download_directory = ".";

    const char *debug_level = "debug";
    int c;
    while((c = getopt(argc, argv, "w:d:s:fh")) != EOF)
    {
        switch(c)
        {
            case 'w':
                global_working_directory = verify_working_directory(optarg);
                break;
            case 'd':
                debug_level = optarg;
                break;
            case 's':
                search_string = optarg;
                break;
            case 'f':
                save_filelist_flag = 1;
                break;
            case 'h':
                printf("syntax: share_tool [-fh] [-w DIR] [-d level] [-s keyword] path ...\n");
                return 0;
            case '?':
            default:
                return 2;
        }
    }

    if(global_working_directory == NULL)
        global_working_directory = strdup("/tmp");

    sp_log_set_level(debug_level);

    global_share = share_new();
    assert(global_share);
    share_tth_init_notifications(global_share);

    tth_store_init();
    /* the share depends on libevent */
    event_init();

    /* install signal handlers
     */
    struct event sigint_event;
    signal_set(&sigint_event, SIGINT, shutdown_event, NULL);
    signal_add(&sigint_event, NULL);

    struct event timer;
    evtimer_set(&timer, timer_event, &timer);
    struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
    evtimer_add(&timer, &tv);

    /* Register notification handlers
     */
    nc_add_share_scan_finished_observer(nc_default(),
            handle_share_scan_finished_notification, NULL);
    nc_add_hashing_complete_observer(nc_default(),
            handle_hashing_complete_notification, NULL);

    hs_start();
    hs_set_prio(0); /* highest prio */

    int i;
    for(i = optind; i < argc; i++)
    {
        if(isatty(1))
            printf(CLRON "Adding shared path: %s" CLROFF "\n", argv[i]);
        str_trim_end_inplace(argv[i], "/");
        share_add(global_share, argv[i]);
        ++num_shares;
    }

    show_share_stats();

    event_dispatch();

    hs_shutdown();
    tth_store_close();
    /* share_close(global_share); */

    return 0;
}

