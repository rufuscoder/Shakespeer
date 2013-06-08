/*
 * Copyright 2005-2006 Martin Hedenfalk <martin@bzero.se>
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

#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "cmd_table.h"
#include "encoding.h"
#include "globals.h"
#include "nmdc.h"
#include "rx.h"
#include "log.h"
#include "xstr.h"

static void
cc_download_request_failed(cc_t *cc, const char *reason)
{
	return_if_fail(cc->current_queue);
	if(cc->current_queue)
	{
		ui_send_status_message(NULL, cc->hub ? cc->hub->address : NULL,
			"Download request for %s from %s failed: %s",
			cc->current_queue->source_filename, cc->nick,
			(reason && reason[0]) ? reason : "unknown reason");

		queue_set_active(cc->current_queue, 0);
		if(!cc->current_queue->is_filelist)
		{
			queue_remove_source(cc->current_queue->target_filename,
				cc->nick);
		}

		queue_free(cc->current_queue);
		cc->current_queue = NULL;

		/* Request another file if there is one in queue for us */
		cc_request_download(cc);
	}
}

/* $Failed error message */
/* $Error [error message] */
static int
cc_cmd_Failed(void *data, int argc, char **argv)
{
	cc_t *cc = data;
	cc_download_request_failed(cc, argv[0]);

	return 0;
}

/* $MaxedOut */
static int
cc_cmd_MaxedOut(void *data, int argc, char **argv)
{
	cc_t *cc = data;

	ui_send_status_message(NULL,
		cc->hub ? cc->hub->address : NULL,
		"Nick %s has no free slots", cc->nick);

	return -1;
}

/* $Send */
static int cc_cmd_Send(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_REQUEST, -1);

    /* FIXME: check for $Send with size ($Send 1234|) */

    if(cc->fd == -1)
    {
        return cc_send_command_as_is(cc, "$Error File Not Available|");
    }

    return_val_if_fail(cc_start_upload(cc) == 0, -1);
    return 1;
}

/* $Get <path>$<offset> */
/* offset is 1-based */
static int cc_cmd_Get(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_READY, -1);
    return_val_if_fail(cc->hub, -1);

    char *e = strchr(argv[0], '$');
    if(e == NULL || e == argv[0])
    {
        INFO("malformed Get request, ignoring");
        return 0;
    }
    *e = 0;

    char *utf8_path = str_convert_to_unescaped_utf8(argv[0], cc->hub->encoding);
    return_val_if_fail(utf8_path, 0);

    uint64_t offset = strtoull(e + 1, 0, 10);
    if(offset > 0)
    {
        offset--; /* offset is 1-based */
    }
    else
    {
        WARNING("Zero offset in $Get command");
    }

    xerr_t *err = 0;
    int rc = cc_upload_prepare(cc, utf8_path, offset, 0, &err);
    free(utf8_path);
    if(rc != 0)
    {
        rc = cc_send_command(cc, "$Error %s|", xerr_msg(err));
        xerr_free(err);
        return rc;
    }

    cc->slot_state = hub_request_upload_slot(cc->hub, cc->nick,
            cc->local_filename, cc->filesize);
    if(cc->slot_state == SLOT_NONE)
    {
        cc_send_command(cc, "$MaxedOut|");
        return -1;
    }

    cc->state = CC_STATE_REQUEST;
    return cc_send_command(cc, "$FileLength %"PRIu64"|", cc->filesize);
}

/* $ADCGET <type> <filename> <startpos> <bytes> <flag0>...<flagN> */
static int cc_cmd_ADCGET(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_READY, -1);

    rx_subs_t *subs = rx_search(argv[0], "([^ ]+) (.+) ([0-9]+) (-?[0-9]+)");
    if(subs == NULL || subs->nsubs != 4)
    {
        INFO("invalid ADCGET request");
        DEBUG("string = [%s]", argv[0]);
        cc_send_command_as_is(cc, "$Error Invalid request|");
        rx_free_subs(subs);
        return 0;
    }

    /* special handling for leaf data upload */
    if(strcmp(subs->subs[0], "tthl") == 0)
    {
        struct tth_entry *te = NULL;
        if(str_has_prefix(subs->subs[1], "TTH/"))
        {
            te = tth_store_lookup(global_tth_store, subs->subs[1] + 4);
            if(te)
            {
                /* Found the TTH, check if the file is shared. */
                if(share_lookup_file_by_inode(global_share, te->active_inode) == NULL)
                    te = NULL;
            }
        }
        else
        {
            /* This must be a really borken client */
            WARNING("Asks for leafdata by filename: borken client");

            char *local_path = share_translate_path(global_share, subs->subs[1]);
            if(local_path)
            {
                share_file_t *f = share_lookup_file(global_share, local_path);
                free(local_path);
                if(f)
                    te = tth_store_lookup_by_inode(global_tth_store, f->inode);
            }
        }

        if(te && tth_store_load_leafdata(global_tth_store, te) == 0)
        {
            cc->leafdata = te->leafdata;
            cc->leafdata_len = te->leafdata_len;
        }
        else
        {
            rx_free_subs(subs);
            return cc_send_command_as_is(cc, "$Error File Not Available|");
        }

        cc_send_command_as_is(cc, "$ADCSND tthl %s 0 %u|",
                subs->subs[1], cc->leafdata_len);
        rx_free_subs(subs);
        cc->bytes_to_transfer = cc->leafdata_len;
        cc->bytes_done = 0;
        cc->offset = 0;
        cc->local_filename = NULL;
        cc->state = CC_STATE_REQUEST;
        cc->local_fd = -1; /* no local file opened, we're sending leaf data */
        return_val_if_fail(cc_start_upload(cc) == 0, -1);
        return 1;
    }

    if(strcmp(subs->subs[0], "file") != 0)
    {
        INFO("unhandled ADCGET type [%s], ignoring", (char *)subs->subs[0]);
        rx_free_subs(subs);
        return -1;
    }

    uint64_t offset = strtoull(subs->subs[2], 0, 10);
    int64_t bytes_to_transfer = strtoull(subs->subs[3], 0, 10);
    if(bytes_to_transfer == -1LL)
    {
        bytes_to_transfer = 0;
    }

    /* ADCGET send filenames in UTF-8 */
    char *filename_utf8 = subs->subs[1];

    xerr_t *err = 0;
    int rc = cc_upload_prepare(cc, filename_utf8, offset, bytes_to_transfer, &err);
    if(rc != 0)
    {
        rx_free_subs(subs);
        rc = cc_send_command(cc, "$Error %s|", xerr_msg(err));
        xerr_free(err);
        return rc;
    }

    cc->slot_state = hub_request_upload_slot(cc->hub, cc->nick,
            cc->local_filename, cc->filesize);
    if(cc->slot_state == SLOT_NONE)
    {
        rx_free_subs(subs);
        cc_send_command(cc, "$MaxedOut|");
        return -1;
    }

    cc_send_command_as_is(cc, "$ADCSND file %s %"PRIu64" %"PRIu64"|",
            filename_utf8, cc->offset, cc->bytes_to_transfer);
    rx_free_subs(subs);

    cc->state = CC_STATE_REQUEST;

    return_val_if_fail(cc_start_upload(cc) == 0, -1);
    return 1;
}

/* $UGetBlock <start> <numbytes> <path> */
/* offset is 0-based */
/*
 <start> - starting index of the file 
 <numbytes> - the number of bytes to get or -1 for unknown (i.e. the whole file) 
 <filename - the name of the filename to get 
*/
static int cc_cmd_UGetBlock(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    rx_subs_t *subs = rx_search(argv[0], "([0-9]+) (-?[0-9]+) (.*)");
    if(subs == NULL || subs->nsubs != 3)
    {
        INFO("invalid UGetBlock request");
        rx_free_subs(subs);
        return cc_send_command(cc, "$Error Invalid request|");
    }

    /* UGetBlock send filenames in UTF-8 already */
    char *filename_utf8 = subs->subs[2];
    if(filename_utf8 == NULL)
    {
        WARNING("Invalid filename encoding in UGetBlock request:"
                " [%s] (ignored command)", argv[0]);
        rx_free_subs(subs);
        return cc_send_command(cc, "$Error File Not Available|");
    }

    uint64_t offset = strtoull(subs->subs[0], 0, 10);
    uint64_t bytes_to_transfer = strtoull(subs->subs[1], 0, 10);
    if(bytes_to_transfer == -1LL)
    {
        bytes_to_transfer = 0;
    }

    xerr_t *err = 0;
    int rc = cc_upload_prepare(cc, filename_utf8, offset, bytes_to_transfer, &err);
    rx_free_subs(subs);
    if(rc != 0)
    {
        rc = cc_send_command(cc, "$Error %s|", xerr_msg(err));
        xerr_free(err);
        return rc;
    }

    cc->slot_state = hub_request_upload_slot(cc->hub, cc->nick,
            cc->local_filename, cc->filesize);
    if(cc->slot_state == SLOT_NONE)
    {
        return cc_send_command(cc, "$MaxedOut|");
    }

    return_val_if_fail(cc_send_command(cc, "$Sending %"PRIu64"|",
                cc->bytes_to_transfer) == 0, -1);

    cc->state = CC_STATE_REQUEST;

    return_val_if_fail(cc_start_upload(cc) == 0, -1);
    return 1;
}

/* $Sending <numbytes> */
/* offset is 0-based */
static int cc_cmd_Sending(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_REQUEST, -1);

    cc->bytes_to_transfer = strtoull(argv[0], 0, 10);
    if(cc->filesize == 0ULL)
    {
        cc->filesize = cc->bytes_to_transfer;
    }

    return_val_if_fail(cc_start_download(cc) == 0, -1);
    return 1;
}

/* $ADCSND <type> <file> <offset> <nbytes> */
/* offset is 0-based */
static int cc_cmd_ADCSND(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_REQUEST, -1);

    rx_subs_t *subs = rx_search(argv[0], "([^ ]+) (.+) ([0-9]+) (-?[0-9]+)");
    if(subs == NULL || subs->nsubs != 4)
    {
        INFO("invalid ADCSND request");
        DEBUG("string = [%s]", argv[0]);
        rx_free_subs(subs);
        return -1;
    }

    cc->bytes_to_transfer = strtoull(subs->subs[3], 0, 10);
    if(cc->filesize == 0ULL)
    {
        cc->filesize = cc->bytes_to_transfer;
    }

    rx_free_subs(subs);

    return_val_if_fail(cc_start_download(cc) == 0, -1);
    return 1;
}

/* $GetListLen */
static int cc_cmd_GetListLen(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return cc_send_command(cc, "$ListLen 42|");
}

static int cc_cmd_FileLength(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_REQUEST, -1);
    return_val_if_fail(cc->current_queue, -1);

    cc->filesize = strtoull(argv[0], 0, 10);
    cc->bytes_to_transfer = cc->filesize - cc->offset;

    if(cc->current_queue->is_filelist)
    {
        /* set the filesize so transfer statstics work */
        queue_set_size(cc->current_queue, cc->filesize);
    }
    else
    {
        if(cc->current_queue->size != cc->filesize)
        {
            WARNING("requested size differs from reported (%"PRIu64" != %"PRIu64")",
                    cc->filesize, cc->current_queue->size);
            queue_set_size(cc->current_queue, cc->filesize);
        }
    }

    if(cc_start_download(cc) != 0)
        return -1;
    return cc_send_command(cc, "$Send|");
}

static int cc_cmd_Key(cc_t *cc, char *cmdline)
{
    return_val_if_fail(cc->state == CC_STATE_KEY, -1);

    /* don't bother to verify the key / lock */

    cc->state = CC_STATE_READY;

    evtimer_del(&cc->handshake_timer_event);

    return 0;
}

static int cc_cmd_Direction(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_DIRECTION, -1);

    if(strcmp(argv[0], "Upload") == 0)
    {
        if(cc->direction == CC_DIR_UPLOAD)
        {
            INFO("double upload connection, aborting");
            return -1;
        }
    }
    else if(strcmp(argv[0], "Download") == 0)
    {
        if(cc->direction == CC_DIR_DOWNLOAD)
        {
            unsigned challenge = strtoul(argv[1], 0, 10);
            INFO("double download connection");
            if(cc->challenge < challenge)
            {
                INFO("switching to upload");
                cc->direction = CC_DIR_UPLOAD;
                return_val_if_fail(cc->current_queue == NULL, -1);
            }
            else if(cc->challenge == challenge)
            {
                INFO("direction collision, aborting");
                return -1;
            }
        }
    }
    else
    {
        INFO("unknown direction: %s", argv[0]);
        return -1;
    }

    cc->state = CC_STATE_KEY;

    return 0;
}

static int cc_cmd_Supports(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    int i;
    for(i = 0; i < argc; i++)
    {
        if(argv[i])
        {
            if(strcmp(argv[i], "XmlBZList") == 0)
                cc->has_xmlbzlist = true;
            else if(strcmp(argv[i], "MiniSlots") == 0)
                /* whatever */ ;
            else if(strcmp(argv[i], "ADCGet") == 0)
                cc->has_adcget = true;
            else if(strcmp(argv[i], "TTHL") == 0)
                cc->has_tthl = true;
            else if(strcmp(argv[i], "TTHF") == 0)
                cc->has_tthf = true;
            else
                INFO("Client supports unknown feature %s", argv[i]);
        }
    }

    return 0;
}

static char *cc_dir2str(int direction)
{
    switch(direction)
    {
        case CC_DIR_UPLOAD:
            return "Upload";
            break;
        case CC_DIR_DOWNLOAD:
            return "Download";
            break;
        case CC_DIR_UNKNOWN:
        default:
            break;
    }
    return "Unknown";
}

static int cc_cmd_Lock(void *data, int argc, char **argv)
{
    cc_t *cc = data;

    return_val_if_fail(cc->state == CC_STATE_LOCK, -1);

    if(str_has_prefix(argv[0], "EXTENDEDPROTOCOL"))
    {
        cc->extended_protocol = true;
        DEBUG("Detected extended protocol: %s %s", argv[0], argv[1]);
    }

    if(cc->extended_protocol)
    {
        cc_send_command(cc, "$Supports MiniSlots XmlBZList ADCGet TTHL TTHF |");
    }

    cc->challenge = random();
    cc_send_command(cc, "$Direction %s %u|",
            cc_dir2str(cc->direction), cc->challenge);

    char *key = nmdc_lock2key(argv[0]);
    /* Don't use cc_send_command here, 'cause we must send the command as-is,
     * without any encoding transformations */
    cc_send_command_as_is(cc, "$Key %s|", key);
    free(key);

    cc->state = CC_STATE_DIRECTION;

    return 0;
}

static int
cc_cmd_MyNick(void *data, int argc, char **argv)
{
	cc_t *cc = data;
	char *nick = argv[0];

	return_val_if_fail(cc->state == CC_STATE_MYNICK, -1);

	if(strchr(nick, '$'))
	{
		INFO("invalid character in nickname (dropping client)");
		return -1;
	}

	char *utf8_nick = 0;
	cc->hub = hub_find_encoding_by_nick(nick, &utf8_nick);
	if(cc->hub == NULL)
	{
		INFO("Closing client connection with non-logged in user '%s'",
			nick);
		return -1;
	}
	return_val_if_fail(utf8_nick, -1);

	DEBUG("set client nick to '%s'", utf8_nick);
	DEBUG("Set corresponding hub to %s", cc->hub->address);

	/* upload or download? */
	if(queue_has_source_for_nick(utf8_nick))
	{
		cc->direction = CC_DIR_DOWNLOAD;
	}
	else
	{
		cc->direction = CC_DIR_UPLOAD;
	}

	/* check for already present connection */
	cc_t *xcc = cc_find_by_nick_and_direction(utf8_nick, cc->direction);
	cc->nick = utf8_nick;
	if(xcc)
	{
		/* If we're already downloading from this nick, force this
		 * connection into an upload. The other peer might want to
		 * download something from us at the same time.
		 */
		if(cc->direction == CC_DIR_DOWNLOAD)
		{
			DEBUG("already downloading from [%s],"
				"forcing upload mode", utf8_nick);
			cc->direction = CC_DIR_UPLOAD;
		}
		else
		{
			/* ...but only allow one upload connection per peer.
			 */
			WARNING("already uploading to %s, closing connection",
				utf8_nick);
			return -1;
		}
	}

	if(cc->incoming_connection)
	{
		cc_send_command(cc, "$MyNick %s|", cc->hub->me->nick);

		char *lock_pk = nmdc_makelock_pk(global_id_lock,
			global_id_version);

		/* Can't use cc_send_command here, 'cause the lock
		 * shouldn't be converted from utf-8 */
		cc_send_command_as_is(cc, "$Lock %s|", lock_pk);
		free(lock_pk);
	}

	cc->state = CC_STATE_LOCK;

	return 0;
}

static cmd_t cc_cmds[] = {
    {"$MyNick", cc_cmd_MyNick, 1},
    {"$Lock", cc_cmd_Lock, 2},
    {"$Supports", cc_cmd_Supports, 1},
    {"$Direction", cc_cmd_Direction, 2},
    {"$FileLength", cc_cmd_FileLength, 1},
    {"$Get", cc_cmd_Get, -1},
    {"$GetListLen", cc_cmd_GetListLen, 0},
    {"$Send", cc_cmd_Send, 0},
    {"$UGetBlock", cc_cmd_UGetBlock, -1},
    {"$ADCGET", cc_cmd_ADCGET, -1},
    {"$ADCSND", cc_cmd_ADCSND, -1},
    {"$Sending", cc_cmd_Sending, 1},
    {"$Error", cc_cmd_Failed, -1},
    {"$Failed", cc_cmd_Failed, -1},
    {"$MaxedOut", cc_cmd_MaxedOut, 0},
    {0, 0, -1}
};

int client_execute_command(int fd, void *data, char *cmdstr)
{
    if(str_has_prefix(cmdstr, "$Key "))
    {
        return cc_cmd_Key(data, cmdstr);
    }
    return cmd_dispatch(cmdstr, " ", 0, cc_cmds, data);
}

