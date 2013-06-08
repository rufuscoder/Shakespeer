/*
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
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

#include "ui.h"
#include "io.h"
#include "util.h"

#include <spclient.h>

#include <sys/types.h>
#include <sys/wait.h>

#define fail_unless(test) \
    do { if(!(test)) { \
        fprintf(stderr, \
                "----------------------------------------------\n" \
                "%s:%d: test FAILED:\nFailed test: %s\n" \
                "----------------------------------------------\n", \
                __FILE__, __LINE__, #test); \
        exit(1); \
    } } while(0)

#define fail_unless_str(str, expected) \
    do { \
        fail_unless((str)); \
        fail_unless(strcmp((str), (expected)) == 0); \
    } while(0)

static int num_cb_called = 0;
static GMainLoop *loop = NULL;

static gboolean test_in_event(GIOChannel *io_channel, GIOCondition condition, gpointer data)
{
    fail_unless(io_channel);
    INFO("data available");
    int rc = TRUE;
    do
    {
        /* data available */
        char *line = io_read_command(io_channel);
        if(line == NULL)
            rc = FALSE;
        else
        {
            INFO("  read command [%s]", line);
            str_trim_end_inplace(line, "|");
            rc = ui_dispatch_command(line, "$", TRUE, data) == 0 ? TRUE : FALSE;
            free(line);
        }
        if(rc == FALSE)
            break;
    } while(g_io_channel_get_buffer_condition(io_channel) == G_IO_IN);
    return rc;
}

static int test_cb_search_all(ui_t *ui, const char *search_string, guint64 size,
        int size_restriction, int file_type, unsigned int id)
{
    fail_unless(ui);
    fail_unless_str(search_string, "search_string");
    fail_unless(size == 4711);
    fail_unless(id == 17);
    ++num_cb_called;
    return 0;
}

static int test_cb_search(ui_t *ui, const char *hub_address, const char *search_string,
        guint64 size, int size_restriction, int file_type, unsigned int id)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub");
    fail_unless_str(search_string, "search_string");
    fail_unless(size == 4711);
    fail_unless(id == 17);
    ++num_cb_called;
    return 0;
}

static int test_cb_connect(ui_t *ui, const char *hub_address, const char *nick,
        const char *email, const char *description, const char *speed, int passive, const char *password,
        const char *encoding)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub2");
    fail_unless_str(nick, "nick2");
    fail_unless_str(email, "email2");
    fail_unless(description == NULL);
    fail_unless_str(speed, "taggtråd");
    fail_unless(passive == FALSE);
    fail_unless_str(password, "lösenord");
    fail_unless_str(encoding, "CP1252");
    ++num_cb_called;
    return 0;
}

static int test_cb_disconnect(ui_t *ui, const char *hub_address)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub3");
    ++num_cb_called;
    return 0;
}

static int test_cb_public_message(ui_t *ui, const char *hub_address, const char *message)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub4");
    fail_unless_str(message, "message4");
    ++num_cb_called;
    return 0;
}

static int test_cb_private_message(ui_t *ui, const char *hub_address,
        const char *nick, const char *message)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub5");
    fail_unless_str(nick, "nick5");
    fail_unless_str(message, "message5");
    ++num_cb_called;
    return 0;
}

static int test_cb_download_file(ui_t *ui, const char *hub_address,
        const char *nick, const char *source_filename, guint64 size,
        const char *target_filename, const char *tth)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub6");
    fail_unless_str(nick, "nick6");
    fail_unless_str(source_filename, "source6");
    fail_unless(size == 345645);
    fail_unless_str(target_filename, "target6");
    fail_unless_str(tth, "TTH6");
    ++num_cb_called;
    return 0;
}

static int test_cb_download_filelist(ui_t *ui, const char *hub_address, const char *nick)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub7");
    fail_unless_str(nick, "nick7");
    ++num_cb_called;
    return 0;
}

static int test_cb_queue_remove_target(ui_t *ui, const char *target_filename)
{
    fail_unless(ui);
    fail_unless_str(target_filename, "target8");
    ++num_cb_called;
    return 0;
}

static int test_cb_queue_remove_filelist(ui_t *ui, const char *nick)
{
    fail_unless(ui);
    fail_unless_str(nick, "nick9");
    ++num_cb_called;
    return 0;
}

static int test_cb_queue_remove_source(ui_t *ui, const char *target_filename, const char *nick)
{
    fail_unless(ui);
    fail_unless_str(target_filename, "target10");
    fail_unless_str(nick, "nick10");
    ++num_cb_called;
    return 0;
}

static int test_cb_queue_remove_nick(ui_t *ui, const char *nick)
{
    fail_unless(ui);
    fail_unless_str(nick, "nick11");
    ++num_cb_called;
    return 0;
}

static int test_cb_cancel_transfer(ui_t *ui, const char *target_filename)
{
    fail_unless(ui);
    fail_unless_str(target_filename, "target12");
    ++num_cb_called;
    return 0;
}

static int test_cb_set_transfer_stats_interval(ui_t *ui, unsigned int seconds)
{
    fail_unless(ui);
    fail_unless(seconds == 13);
    ++num_cb_called;
    return 0;
}

static int test_cb_set_port(ui_t *ui, int port)
{
    fail_unless(ui);
    fail_unless(port == 1414);
    ++num_cb_called;
    return 0;
}

static int test_cb_add_shared_path(ui_t *ui, const char *path)
{
    fail_unless(ui);
    fail_unless_str(path, "/path15");
    ++num_cb_called;
    return 0;
}

static int test_cb_remove_shared_path(ui_t *ui, const char *path)
{
    fail_unless(ui);
    fail_unless_str(path, "/path16");
    ++num_cb_called;
    return 0;
}

static int test_cb_set_password(ui_t *ui, const char *hub_address, const char *password)
{
    fail_unless(ui);
    fail_unless_str(hub_address, "hub17");
    fail_unless_str(password, "password17");
    ++num_cb_called;
    return 0;
}

static int test_cb_shutdown(ui_t *ui)
{
    fail_unless(ui);
    ++num_cb_called;
    g_main_loop_quit(loop);
    return 0;
}

void server(const char *working_directory)
{
    char *socket_filename = g_strdup_printf("%s/sphubd", working_directory);
    GIOChannel *ioc = io_bind_unix_socket(socket_filename);
    free(socket_filename);
    fail_unless(ioc);

    GIOChannel *aioc = io_accept_connection(ioc);
    fail_unless(aioc);

    g_io_channel_set_line_term(aioc, "|", 1);
    ui_t *ui = ui_init();
    fail_unless(ui);
    ui->io_channel = aioc;

    /* setup callbacks */
    ui->cb_search = test_cb_search;
    ui->cb_search_all = test_cb_search_all;
    ui->cb_connect = test_cb_connect;
    ui->cb_disconnect = test_cb_disconnect;
    ui->cb_public_message = test_cb_public_message;
    ui->cb_private_message = test_cb_private_message;
    ui->cb_download_file = test_cb_download_file;
    ui->cb_download_filelist = test_cb_download_filelist;
    ui->cb_queue_remove_target = test_cb_queue_remove_target;
    ui->cb_queue_remove_filelist = test_cb_queue_remove_filelist;
    ui->cb_queue_remove_source = test_cb_queue_remove_source;
    ui->cb_queue_remove_nick = test_cb_queue_remove_nick;
    ui->cb_cancel_transfer = test_cb_cancel_transfer;
    ui->cb_transfer_stats_interval = test_cb_set_transfer_stats_interval;
    ui->cb_set_port = test_cb_set_port;
    ui->cb_add_shared_path = test_cb_add_shared_path;
    ui->cb_remove_shared_path = test_cb_remove_shared_path;
    ui->cb_set_password = test_cb_set_password;

    /* must be last, quits the runloop */
    ui->cb_shutdown = test_cb_shutdown;

    loop = g_main_loop_new(NULL, FALSE);
    g_io_add_watch(ui->io_channel, G_IO_IN | G_IO_ERR | G_IO_HUP, test_in_event, ui);
    g_main_loop_run(loop);
    printf("main loop finished\n");
}

void client(const char *working_directory)
{
    sp_t *sp = sp_create(NULL);

    fail_unless(sp_connect(sp, working_directory, NULL) == 0);
    fail_unless(sp->io_channel);

    sp_send_search_all(sp, "search_string", 4711, SHARE_SIZE_MAX, SHARE_TYPE_ANY, 17);
    sp_send_search(sp, "hub", "search_string", 4711, SHARE_SIZE_MAX, SHARE_TYPE_ANY, 17);
    sp_send_connect(sp, "hub2", "nick2", "email2", NULL, "taggtråd", FALSE, "lösenord", "CP1252");
    sp_send_disconnect(sp, "hub3");
    sp_send_public_message(sp, "hub4", "message4");
    sp_send_private_message(sp, "hub5", "nick5", "message5");
    sp_send_download_file(sp, "hub6", "nick6", "source6", 345645, "target6", "TTH6");
    sp_send_download_filelist(sp, "hub7", "nick7");
    sp_send_queue_remove_target(sp, "target8");
    sp_send_queue_remove_filelist(sp, "nick9");
    sp_send_queue_remove_source(sp, "target10", "nick10");
    sp_send_queue_remove_nick(sp, "nick11");
    sp_send_cancel_transfer(sp, "target12");
    sp_send_transfer_stats_interval(sp, 13);
    sp_send_set_port(sp, 1414);
    sp_send_add_shared_path(sp, "/path15");
    sp_send_remove_shared_path(sp, "/path16");
    sp_send_set_password(sp, "hub17", "password17");

    /* must be last, shuts down the server */
    sp_send_shutdown(sp);

    /* sp_free(sp); */
}

int main(void)
{
    const char *working_directory = verify_working_directory("/tmp/ui_test");
    fail_unless(working_directory);

    pid_t pid = fork();
    if(pid == 0)
    {
        server(working_directory);
        printf("num_cb_called = %d\n", num_cb_called);
        fail_unless(num_cb_called == 19);
    }
    else
    {
        sleep(1);
        client(working_directory);
        int status = 0;
        printf("waiting for pid %d\n", pid);
        int rc = wait(&status);
        fail_unless(rc == pid);
        fail_unless(WIFEXITED(status));
        fail_unless(WEXITSTATUS(status) == 0);
    }

    return 0;
}

