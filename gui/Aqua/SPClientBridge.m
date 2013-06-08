/* vim: ft=objc
 *
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

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <event.h>

#include "nmdc.h"
#include "log.h"
#include "spclient.h"
#ifndef VERSION
#include "../../version.h"
#endif

#import "SPApplicationController.h"
#import "SPBookmarkController.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"
#import "SPMainWindowController.h"

static int spcb_port(sp_t *sp, int port)
{
    if (port == -1) {
        if ([[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsConnectionMode] == 0)
            sp_send_set_port(sp, [[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsPort]);
        else
            [[SPApplicationController sharedApplicationController] setPassiveMode];
    }
    
    return 0;
}

static int spcb_share_stats(sp_t *sp, const char *path,
        uint64_t size, uint64_t totsize, uint64_t dupsize,
        unsigned nfiles, unsigned ntotfiles, unsigned nduplicates)
{
    sendNotification(SPNotificationShareStats,
            @"path", path ? [NSString stringWithUTF8String:path] : @"",
            @"size", [NSNumber numberWithUnsignedLongLong:size],
            @"totsize", [NSNumber numberWithUnsignedLongLong:totsize],
            @"dupsize", [NSNumber numberWithUnsignedLongLong:dupsize],
            @"nfiles", [NSNumber numberWithUnsignedInt:nfiles],
            @"ntotfiles", [NSNumber numberWithUnsignedInt:ntotfiles],
            @"nduplicates", [NSNumber numberWithUnsignedInt:nduplicates],
            nil);
    return 0;
}

static int spcb_share_duplicate_found(sp_t *sp, const char *path)
{
    sendNotification(SPNotificationShareDuplicateFound,
                     @"path", [NSString stringWithUTF8String:path],
                     nil);
    return 0;
}

static int spcb_transfer_aborted(sp_t *sp, const char *local_filename)
{
    sendNotification(SPNotificationTransferAborted,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            nil);
    return 0;
}

static int spcb_queue_add_filelist(sp_t *sp, const char *nick,
        unsigned int priority)
{
    sendNotification(SPNotificationQueueAddFilelist,
            @"nick", [NSString stringWithUTF8String:nick],
            @"priority", [NSNumber numberWithUnsignedInt:priority],
            nil);
    return 0;
}

static int spcb_queue_add_directory(sp_t *sp, const char *target_directory,
        const char *nick)
{
    sendNotification(SPNotificationQueueAddDirectory,
            @"targetDirectory", [NSString stringWithUTF8String:target_directory],
            @"nick", [NSString stringWithUTF8String:nick],
            nil);
    return 0;
}

static int spcb_queue_add_target(sp_t *sp,
        const char *local_filename, uint64_t size,
        const char *tth, unsigned int priority)
{
    sendNotification(SPNotificationQueueAdd,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            @"size", [NSNumber numberWithUnsignedLongLong:size],
            @"tth", tth ? [NSString stringWithUTF8String:tth] : @"",
            @"priority", [NSNumber numberWithUnsignedInt:priority],
            nil);
    return 0;
}

static int spcb_queue_remove_target(sp_t *sp, const char *local_filename)
{
    sendNotification(SPNotificationQueueRemove,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            nil);
    return 0;
}

static int spcb_queue_remove_filelist(sp_t *sp, const char *nick)
{
    sendNotification(SPNotificationQueueRemoveFilelist,
            @"nick", [NSString stringWithUTF8String:nick],
            nil);
    return 0;
}

static int spcb_queue_remove_directory(sp_t *sp, const char *target_directory)
{
    sendNotification(SPNotificationQueueRemoveDirectory,
            @"targetFilename", [NSString stringWithUTF8String:target_directory],
            nil);
    return 0;
}

static int spcb_source_add(sp_t *sp, const char *local_filename, const char *nick, const char *remote_filename)
{
    sendNotification(SPNotificationSourceAdd,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            @"nick", [NSString stringWithUTF8String:nick],
            @"sourceFilename", [NSString stringWithUTF8String:remote_filename],
            nil);
    return 0;
}

static int spcb_source_remove(sp_t *sp, const char *local_filename, const char *nick)
{
    sendNotification(SPNotificationSourceRemove,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            @"nick", [NSString stringWithUTF8String:nick],
            nil);
    return 0;
}

static int spcb_hub_redirect(sp_t *sp, const char *hub_address, const char *new_address)
{
    sendNotification(SPNotificationHubRedirect,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"newAddress", [NSString stringWithUTF8String:new_address],
            nil);
    return 0;
}

static int spcb_download_finished(sp_t *sp, const char *local_filename)
{
    sendNotification(SPNotificationDownloadFinished,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            nil);
    return 0;
}

static int spcb_directory_finished(sp_t *sp, const char *target_directory)
{
    sendNotification(SPNotificationDirectoryFinished,
            @"targetFilename", [NSString stringWithUTF8String:target_directory],
            nil);
    return 0;
}

static int spcb_upload_finished(sp_t *sp, const char *local_filename)
{
    sendNotification(SPNotificationUploadFinished,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            nil);
    return 0;
}

static int spcb_filelist_finished(sp_t *sp, const char *hub_address, const char *nick, const char *filename)
{
    sendNotification(SPNotificationFilelistFinished,
            @"hubAddress", hub_address ? [NSString stringWithUTF8String:hub_address] : @"",
            @"nick", [NSString stringWithUTF8String:nick],
            @"targetFilename", [NSString stringWithUTF8String:filename],
            nil);
    return 0;
}

static int spcb_connection_closed(sp_t *sp, const char *nick, int direction)
{
    sendNotification(SPNotificationConnectionClosed,
            @"nick", [NSString stringWithUTF8String:nick],
            @"direction", [NSNumber numberWithInt:direction],
            nil);
    return 0;
}

static int spcb_connection_failed(sp_t *sp, const char *hub_address)
{
    sendNotification(SPNotificationConnectionFailed,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            nil);
    return 0;
}

static int spcb_download_starting(sp_t *sp, const char *hub_address, const char *nick,
        const char *remote_filename, const char *local_filename, uint64_t filesize)
{
    sendNotification(SPNotificationDownloadStarting,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:nick],
            @"sourceFilename", [NSString stringWithUTF8String:remote_filename],
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            @"size", [NSNumber numberWithUnsignedLongLong:filesize],
            nil);
    return 0;
}

static int spcb_upload_starting(sp_t *sp, const char *hub_address, const char *nick,
        const char *local_filename, uint64_t filesize)
{
    sendNotification(SPNotificationUploadStarting,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:nick],
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            @"size", [NSNumber numberWithUnsignedLongLong:filesize],
            nil);
    return 0;
}

static int spcb_transfer_stats(sp_t *sp, const char *local_filename,
        uint64_t offset,
        uint64_t filesize, unsigned bytes_per_sec)
{
    sendNotification(SPNotificationTransferStats,
            @"targetFilename", [NSString stringWithUTF8String:local_filename],
            @"offset", [NSNumber numberWithUnsignedLongLong:offset],
            @"size", [NSNumber numberWithUnsignedLongLong:filesize],
            @"bps", [NSNumber numberWithInt:bytes_per_sec],
            nil);
    return 0;
}

static int spcb_search_response(sp_t *sp, int id, const char *hub_address, const char *nick,
        const char *filename, int filetype, uint64_t size, int openslots, int totalslots, const char *tth,
        const char *speed)
{
    sendNotification(SPNotificationSearchResponse,
            @"tag", [NSNumber numberWithInt:id],
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:nick],
            @"filename", [NSString stringWithUTF8String:filename],
            @"type", [NSNumber numberWithInt:filetype],
            @"size", [NSNumber numberWithUnsignedLongLong:size],
            @"openslots", [NSNumber numberWithInt:openslots],
            @"totalslots", [NSNumber numberWithInt:totalslots],
            @"tth", tth ? [NSString stringWithUTF8String:tth] : @"",
            @"speed", speed ? [NSString stringWithUTF8String:speed] : @"",
            nil);
    /* TODO: free sr */
    return 0;
}

static int spcb_hub_add(sp_t *sp, const char *address, const char *hubname,
        const char *nick, const char *description, const char *encoding)
{
    sendNotification(SPNotificationHubAdd,
            @"hubAddress", [NSString stringWithUTF8String:address],
            @"name", [NSString stringWithUTF8String:hubname],
            @"nick", [NSString stringWithUTF8String:nick],
            @"description", description ? [NSString stringWithUTF8String:description] : @"",
            @"encoding", encoding ? [NSString stringWithUTF8String:encoding] : @"",
            nil);
    return 0;
}

static int spcb_public_message(sp_t *sp, const char *hub_address, const char *nick, const char *message)
{
    char *message_unescaped = nmdc_unescape(message);
    sendNotification(SPNotificationPublicMessage,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", nick ? [NSString stringWithUTF8String:nick] : @"",
            @"message", message ? [NSString stringWithUTF8String:message_unescaped] : @"",
            nil);
    free(message_unescaped);
    return 0;
}

static int spcb_private_message(sp_t *sp, const char *hub_address, const char *my_nick,
        const char *remote_nick, const char *remote_display_nick, const char *message)
{
    sendNotification(SPNotificationStartChat,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"remote_nick", [NSString stringWithUTF8String:remote_nick],
            @"my_nick", [NSString stringWithUTF8String:my_nick],
            nil);

    char *message_unescaped = nmdc_unescape(message);
    sendNotification(SPNotificationPrivateMessage,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:remote_nick],
            @"display_nick", [NSString stringWithUTF8String:remote_display_nick],
            @"message", message ? [NSString stringWithUTF8String:message_unescaped] : @"",
            nil);
    free(message_unescaped);
    return 0;
}

static int spcb_status_message(sp_t *sp, const char *hub_address, const char *message)
{
    sendNotification(SPNotificationStatusMessage,
            @"hubAddress", hub_address ? [NSString stringWithUTF8String:hub_address] : @"",
            @"message", [NSString stringWithUTF8String:message],
            nil);
    return 0;
}

static int spcb_user_login(sp_t *sp, const char *hub_address, const char *nick,
        const char *description, const char *tag, const char *speed,
        const char *email, uint64_t share_size, int is_operator,
        unsigned int extra_slots)
{
    sendNotification(SPNotificationUserLogin,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:nick],
            @"description", description ? [NSString stringWithUTF8String:description] : @"",
            @"tag", tag ? [NSString stringWithUTF8String:tag] : @"",
            @"speed", speed ? [NSString stringWithUTF8String:speed] : @"",
            @"email", email ? [NSString stringWithUTF8String:email] : @"",
            @"size", [NSNumber numberWithUnsignedLongLong:share_size],
            @"isOperator", [NSNumber numberWithBool:is_operator],
            @"extraSlots", [NSNumber numberWithUnsignedInt:extra_slots],
            nil);
    return 0;
}

static int spcb_user_update(sp_t *sp, const char *hub_address, const char *nick,
        const char *description, const char *tag, const char *speed,
        const char *email, uint64_t share_size, int is_operator,
        unsigned int extra_slots)
{
    sendNotification(SPNotificationUserUpdate,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:nick],
            @"description", description ? [NSString stringWithUTF8String:description] : @"",
            @"tag", tag ? [NSString stringWithUTF8String:tag] : @"",
            @"speed", speed ? [NSString stringWithUTF8String:speed] : @"",
            @"email", email ? [NSString stringWithUTF8String:email] : @"",
            @"size", [NSNumber numberWithUnsignedLongLong:share_size],
            @"isOperator", [NSNumber numberWithBool:is_operator],
            @"extraSlots", [NSNumber numberWithUnsignedInt:extra_slots],
            nil);
    return 0;
}

static int spcb_hubname_changed(sp_t *sp, const char *hub_address, const char *new_name)
{
    sendNotification(SPNotificationHubnameChanged,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"newHubname", [NSString stringWithUTF8String:new_name],
            nil);
    return 0;
}

static int spcb_user_logout(sp_t *sp, const char *hub_address, const char *nick)
{
    sendNotification(SPNotificationUserLogout,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:nick],
            nil);
    return 0;
}

static int spcb_get_password(sp_t *sp, const char *hub_address, const char *nick)
{
    sendNotification(SPNotificationNeedPassword,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"nick", [NSString stringWithUTF8String:nick],
            nil);
    return 0;
}

static int spcb_server_version(sp_t *sp, const char *version)
{
    if (strcmp(version, VERSION) != 0) {
        [[SPApplicationController sharedApplicationController] removeSphubdConnection];
        [[SPApplicationController sharedApplicationController]
            versionMismatch:[NSString stringWithUTF8String:version]];
    }
    
    return 0;
}

static int spcb_user_command(sp_t *sp, const char *hub_address, int type, int context,
        const char *description, const char *command)
{
    sendNotification(SPNotificationUserCommand,
            @"hubAddress", [NSString stringWithUTF8String:hub_address],
            @"type", [NSNumber numberWithInt:type],
            @"context", [NSNumber numberWithInt:context],
            @"description", description ? [NSString stringWithUTF8String:description] : @"",
            @"command", command ? [NSString stringWithUTF8String:command] : @"",
            nil);
    return 0;
}

static int spcb_set_priority(sp_t *sp, const char *target_filename, unsigned int priority)
{
    sendNotification(SPNotificationSetPriority,
            @"targetFilename", [NSString stringWithUTF8String:target_filename],
            @"priority", [NSNumber numberWithUnsignedInt:priority],
            nil);
    return 0;
}

static int spcb_hub_disconnected(sp_t *sp, const char *hub_address)
{
    sendNotification(SPNotificationHubDisconnected,
            @"hubAddress", hub_address ? [NSString stringWithUTF8String:hub_address] : @"",
            nil);
    return 0;
}

static int spcb_stored_filelists(sp_t *sp, const char *nicks)
{
    NSArray *nicksArray = [[NSString stringWithUTF8String:nicks] componentsSeparatedByString:@" "];

    if ([nicksArray count] > 0)
        sendNotification(SPNotificationStoredFilelists, @"nicks", nicksArray, nil);

    return 0;
}

static int spcb_init_completion(sp_t *sp, int level)
{
    sendNotification(SPNotificationInitCompletion, @"level", [NSNumber numberWithInt:level], nil);
    return 0;
}

void sp_register_callbacks(sp_t *sp)
{
    /* setup the callback functions */
    sp->cb_user_logout = spcb_user_logout;
    sp->cb_user_login = spcb_user_login;
    sp->cb_user_update = spcb_user_update;
    sp->cb_public_message = spcb_public_message;
    sp->cb_private_message = spcb_private_message;
    sp->cb_search_response = spcb_search_response;
    sp->cb_filelist_finished = spcb_filelist_finished;
    sp->cb_hubname = spcb_hubname_changed;
    sp->cb_status_message = spcb_status_message;
    sp->cb_download_starting = spcb_download_starting;
    sp->cb_upload_starting = spcb_upload_starting;
    sp->cb_download_finished = spcb_download_finished;
    sp->cb_directory_finished = spcb_directory_finished;
    sp->cb_upload_finished = spcb_upload_finished;
    sp->cb_transfer_aborted = spcb_transfer_aborted;
    sp->cb_queue_add_target = spcb_queue_add_target;
    sp->cb_queue_add_directory = spcb_queue_add_directory;
    sp->cb_queue_add_filelist = spcb_queue_add_filelist;
    sp->cb_queue_add_source = spcb_source_add;
    sp->cb_queue_remove_target = spcb_queue_remove_target;
    sp->cb_queue_remove_filelist = spcb_queue_remove_filelist;
    sp->cb_queue_remove_directory = spcb_queue_remove_directory;
    sp->cb_queue_remove_source = spcb_source_remove;
    sp->cb_hub_redirect = spcb_hub_redirect;
    sp->cb_transfer_stats = spcb_transfer_stats;
    sp->cb_hub_add = spcb_hub_add;
    sp->cb_port = spcb_port;
    sp->cb_connection_closed = spcb_connection_closed;
    sp->cb_connect_failed = spcb_connection_failed;
    sp->cb_share_stats = spcb_share_stats;
    sp->cb_share_duplicate_found = spcb_share_duplicate_found;
    sp->cb_get_password = spcb_get_password;
    sp->cb_server_version = spcb_server_version;
    sp->cb_user_command = spcb_user_command;
    sp->cb_set_priority = spcb_set_priority;
    sp->cb_hub_disconnected = spcb_hub_disconnected;
    sp->cb_stored_filelists = spcb_stored_filelists;
    sp->cb_init_completion = spcb_init_completion;
}

void sendNotification(NSString *notificationName, NSString *key1, id arg1, ...)
{
    NSMutableDictionary *argDict = [[NSMutableDictionary alloc] init];

    [argDict setObject:arg1 forKey:key1];

    va_list ap;
    va_start(ap, arg1);
    id key;
    id arg;
    while ((key = va_arg(ap, id)) != nil) {
        arg = va_arg(ap, id);
        if (arg == nil)
            break;
        [argDict setObject:arg forKey:key];
    }
    va_end(ap);

    [[NSNotificationCenter defaultCenter] postNotificationName:notificationName
                                                        object:[SPApplicationController sharedApplicationController]
                                                      userInfo:argDict];
    [argDict release];
}

static void sp_queue_write(sp_t *sp)
{
    assert(sp);

    NSLog(@"Writing %i queued bytes", EVBUFFER_LENGTH(sp->output));
    if (evbuffer_write(sp->output, sp->fd) == -1) {
        NSLog(@"evbuffer_write: %s", strerror(errno));
    }
    else {
        if (EVBUFFER_LENGTH(sp->output) == 0) {
            /* all queued data is written, disable output callback */
            CFSocketDisableCallBacks((CFSocketRef)sp->user_data, kCFSocketWriteCallBack);
        }
    }
}

void sp_callback(CFSocketRef s, CFSocketCallBackType callbackType,
        CFDataRef address, const void *data, void *info)
{
    sp_t *sp = info;
    assert(sp);

    if (callbackType == kCFSocketWriteCallBack) {
        sp_queue_write(sp);
    }
    else {
        if (sp_in_event(sp->fd, 0, sp) != 0) {
            [[SPApplicationController sharedApplicationController] removeSphubdConnection];
            sendNotification(SPNotificationServerDied, @"errorMessage", @"unknown error", nil);
        }
    }
}

int sp_queue_data(sp_t *sp, const char *string, size_t len)
{
    NSLog(@"Queueing %i bytes", len);
    evbuffer_add(sp->output, (void *)string, len);

    /* re-enable output callback */
    CFSocketEnableCallBacks((CFSocketRef)sp->user_data, kCFSocketWriteCallBack);

    return 0;
}

int sp_send_string(sp_t *sp, const char *string)
{
    int should_queue = 0;
    int rc = 0;
    size_t nbytes = strlen(string);

    return_val_if_fail(sp, -1);
    return_val_if_fail(sp->output, -1);
    return_val_if_fail(sp->fd != -1, -1);
    return_val_if_fail(string, -1);

    if (EVBUFFER_LENGTH(sp->output) > 0) {
        should_queue = 1;
    }
    else {
        rc = write(sp->fd, string, nbytes);
        if (rc == 0) {
            /* EOF */
            return -1;
        }
        else if (rc < nbytes || (rc == -1 && errno == EAGAIN)) {
            should_queue = 1;
        }
    }

    if (should_queue) {
        int d = (rc <= 0 ? 0 : rc);
        nbytes -= d;
        sp_queue_data(sp, string + d, nbytes);
    }

    return -1;
}

