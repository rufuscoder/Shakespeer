#ifndef _globals_h_
#define _globals_h_

#include <stdbool.h>

extern int global_init_completion;
extern int global_expected_shared_paths;
extern int global_port;
extern void *global_share;
extern void *global_search_listener;
extern char *global_working_directory;
extern char *argv0_path;
extern bool global_follow_redirects;
extern bool global_auto_match_filelists;
extern bool global_auto_search_sources;
extern unsigned global_hash_prio;
extern char *global_incomplete_directory;
extern char *global_download_directory;

extern char *global_id_version;
extern char *global_id_tag;
extern char *global_id_generator;
extern char *global_id_lock;

extern bool global_move_partial_directories;

extern void *global_tth_store;

#endif

