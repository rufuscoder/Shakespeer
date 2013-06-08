#include <sys/types.h>
#include <sys/time.h>
#include <event.h>
#include <signal.h>
#include <unistd.h>

#include "spclient.h"
#include "unit_test.h"

sp_t *sp = NULL;

int sp_send_string(sp_t *sp, const char *string)
{
    if(sp->output == NULL)
    {
        sp->output = evbuffer_new();
    }
    evbuffer_add(sp->output, (void *)string, strlen(string));
    evbuffer_write(sp->output, sp->fd);
    return 0;
}

void setup(void)
{
    const char *working_directory = "/tmp/sp-ui_connect-test.d";
    system("/bin/rm -rf /tmp/sp-ui_connect-test.d");
    system("mkdir /tmp/sp-ui_connect-test.d");

    sp = sp_create(NULL);
    fail_unless(sp);
    printf("connecting to sphubd...\n");
    fail_unless(sp_connect(sp, working_directory, "../sphubd/sphubd") == 0);
    printf("connected to sphubd\n");
}

void teardown(void)
{
    FILE *fp = fopen("/tmp/sp-ui_connect-test.d/sphubd.pid", "r");
    fail_unless(fp);
    int sphubd_pid = -1;
    fail_unless(fscanf(fp, "%i", &sphubd_pid) == 1);
    fail_unless(kill(sphubd_pid, 0) == 0);
    fclose(fp);
    printf("sphubd is running as pid %i\n", sphubd_pid);

    fp = fopen("/tmp/sp-ui_connect-test.d/sphashd.pid", "r");
    fail_unless(fp);
    int sphashd_pid = -1;
    fail_unless(fscanf(fp, "%i", &sphashd_pid) == 1);
    fail_unless(kill(sphashd_pid, 0) == 0);
    fclose(fp);
    printf("sphashd is running as pid %i\n", sphashd_pid);

    fail_unless(sp_send_shutdown(sp) == 0);
    sleep(1);
    fail_unless(kill(sphubd_pid, 0) == -1);
    fail_unless(kill(sphashd_pid, 0) == -1);

    system("/bin/rm -rf /tmp/sp-ui_connect-test.d");
}

int main(void)
{
    setup();
    teardown();

    return 0;
}

