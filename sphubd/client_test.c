#include "sphubd.h"
#include "client.h"
#include "hub.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "unit_test.h"

void cc_finish_download(cc_t *cc);

int main(void)
{
    /* initialize a memory-only database */
    fail_unless(db_init(NULL, NULL) == 0);

    /* initialize a share */
    global_share = share_new("/tmp");
    fail_unless(global_share);

    char *cwd = g_get_current_dir();
    fail_unless(share_add(global_share, cwd, TRUE) == 0);
    free(cwd);

    hub_t *hub = hub_new();
    fail_unless(hub);
    hub->me = user_new("nick", NULL, NULL, NULL, NULL, 0ULL, hub);
    fail_unless(hub->me);
    cc_t *cc = cc_new(-1, hub);
    fail_unless(cc);
    fail_unless(cc->state == CC_STATE_MYNICK);

    /* login a test user on the hub */
    user_t *test_user = user_new("foo", NULL, NULL, NULL, NULL, 0ULL, hub);
    g_hash_table_insert(hub->users, strdup(test_user->nick), test_user);

    /* fake remote nick of the client connection */
    cc->nick = strdup("foo");

    /* requesting a non-existent file should fail */
    int rc = cc_upload_prepare(cc, "non-existent-file", 0, 0);
    fail_unless(rc == -1);

    /* a user with same nick as me requesting a file should fail */
    free(cc->nick);
    cc->nick = strdup("nick");
    rc = cc_upload_prepare(cc, "sphubd\\sphubd.c", 0, 0);
    fail_unless(rc == -1);
    cc->nick = strdup("foo");

    /* cc_upload_prepare should set cc->offset correctly after called */
    cc->offset = 4711ULL;
    rc = cc_upload_prepare(cc, "sphubd\\sphubd.c", 0, 0);
    fail_unless(rc == 0);
    fail_unless(cc->offset == 0ULL);
    fail_unless(cc->bytes_to_transfer == cc->filesize);

    guint64 ofs = 17;
    rc = cc_upload_prepare(cc, "sphubd\\sphubd.c", ofs, 0);
    fail_unless(rc == 0);
    fail_unless(cc->offset == ofs);
    fail_unless(cc->bytes_to_transfer == cc->filesize - ofs);

    /* send commands to a file instead of to a hub */
    cc->fd = open("/tmp/client_test.log", O_RDWR|O_CREAT);
    fail_unless(cc->fd != -1);

    /* nothing in the download queue yet */
    fail_unless(cc_request_download(cc) == -1);

    /* ok, so add a file to the download queue */
    rc = queue_add("bar", "share\\bar-file.zip", 4711ULL, "/tmp/client_test_file.zip", "ABCDEFGHIJKLMNOPQRSTUVWXYZ012");
    fail_unless(rc == 0);

    /* wrong nick in the download queue */
    fail_unless(cc_request_download(cc) == -1);

    /* ok, so add two files to the download queue with correct nick */
    rc = queue_add("foo", "share\\foo-file2.zip", 17ULL, "/tmp/client_test_file2.zip", "ABCDEFGHIJKLMNOPQRSTUVWXYZ234");
    fail_unless(rc == 0);
    rc = queue_add("foo", "share\\foo-file.zip", 4711ULL, "/tmp/client_test_file.zip", "ABCDEFGHIJKLMNOPQRSTUVWXYZ012");
    fail_unless(rc == 0);

    fail_unless(cc_request_download(cc) == 0);
    fail_unless(cc->state == CC_STATE_REQUEST);
    /* should download foo-file.zip first, because they're sorted by filename */
    fail_unless(strcmp(cc->current_queue->target_filename, "/tmp/client_test_file.zip") == 0);

    cc->has_adcget = TRUE;
    cc->has_tthf = TRUE;

    cc_start_download(cc);
    fail_unless(cc->state == CC_STATE_BUSY);
    cc_finish_download(cc);
    /* cc_finish_download should request another file directly if there is one
     * in the download queue */
    fail_unless(cc->state == CC_STATE_REQUEST);
    fail_unless(strcmp(cc->current_queue->target_filename, "/tmp/client_test_file2.zip") == 0);

    cc_start_download(cc);
    fail_unless(cc->state == CC_STATE_BUSY);
    cc_finish_download(cc);
    fail_unless(cc->state == CC_STATE_READY);

    /* nothing more in the download queue */
    fail_unless(cc_request_download(cc) == -1);

    /* zero-sized files are not downloaded (except filelists) */
    rc = queue_add("foo", "share\\file3.zip", 0ULL, "/tmp/client_test_file3.zip", "ABCDEFGHIJKLMNOPQRSTUVWXYZ345");
    fail_unless(rc == 0);
    fail_unless(cc_request_download(cc) == -1);

    /* download a filelist */
    cc->has_xmlbzlist = true;
    rc = queue_add_filelist("foo", false);
    fail_unless(rc == 0);
    fail_unless(cc_request_download(cc) == 0);
    fail_unless(strcmp(cc->current_queue->target_filename, "/tmp/files.xml.foo.bz2") == 0);

    cc_start_download(cc);
    fail_unless(cc->state == CC_STATE_BUSY);
    cc_finish_download(cc);
    fail_unless(cc->state == CC_STATE_READY);

    return 0;
}

