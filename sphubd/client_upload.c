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

#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "client.h"
#include "log.h"
#include "globals.h"
#include "xstr.h"
#include "xerr.h"

void cc_finish_upload(cc_t *cc)
{
    if(cc->local_fd != -1)
    {
	INFO("finished uploading file [%s]", cc->local_filename);
        close(cc->local_fd);
        cc->local_fd = -1;
        ui_send_upload_finished(NULL, cc->local_filename);
    }
    else
    {
	INFO("finished uploading leafdata for [%s]", cc->local_filename);
	/* don't free the leafdata, it belongs to the tth_store */
        cc->leafdata = NULL;
        cc->leafdata_index = 0;
        cc->leafdata_len = 0;
    }

    cc->state = CC_STATE_READY;
    cc->last_activity = time(0);

    if(cc->slot_state != SLOT_EXTRA)
    {
        /* extra slots last the whole connection */
        hub_free_upload_slot(cc->hub, cc->nick, cc->slot_state);
	cc->slot_state = SLOT_NONE;
    }

    free(cc->local_filename);
    cc->local_filename = NULL;
}

/* Reads at most nbytes bytes of data to be uploaded to client cc.
 * Places read bytes in *buf and returns number of bytes actually read.
 *
 * Data can be read either from a file (normal file upload) or from the
 * cc->leafdata buffer when uploading TTH leaf data.
 *
 * Returns >= 0 on success, or -1 on error.
 */
ssize_t cc_upload_read(cc_t *cc, void *buf, size_t nbytes)
{
    if(cc->local_fd != -1)
    {
        return read(cc->local_fd, buf, nbytes);
    }
    else
    {
        return_val_if_fail(cc->leafdata, -1);
        int left = cc->leafdata_len - cc->leafdata_index;
        if(left <= 0)
        {
            errno = EINVAL;
            return -1;
        }
        if(nbytes < left)
        {
            left = nbytes;
        }
        memcpy(buf, cc->leafdata + cc->leafdata_index, left);
        cc->leafdata_index += left;
        return left;
    }
}

int cc_start_upload(cc_t *cc)
{
    return_val_if_fail(cc->state == CC_STATE_REQUEST, -1);

    cc->transfer_start_time = time(0);

    cc->state = CC_STATE_BUSY;
    if(cc->local_fd != -1) /* otherwise we're sending leaf data */
    {
        ui_send_upload_starting(NULL, cc->hub->address, cc->nick,
                cc->local_filename, cc->filesize);
    }

    cc->last_transfer_activity = time(0);

    /* Start uploading process by writing the first chunk of data, ie filling
     * the bufferevent. Otherwise the out-event will not be triggered and we
     * risk getting into a deadlock. */
    cc_out_event(cc->bufev, cc);

    return 0;
}

/* Opens the source filename (already in utf8)
 * sets cc->local_filename to filename in local filesystem
 * if bytes_to_transfer == 0, sets it to the whole file (minus offset)
 * sets cc->offset, cc->bytes_to_transfer, cc->filesize and cc->bytes_done
 * returns 0 on success, or -1 on error
 */
int cc_upload_prepare(cc_t *cc, const char *filename,
        uint64_t offset, uint64_t bytes_to_transfer, xerr_t **err)
{
    char *local_filename = 0;
    int fl_type = 0;

    if (str_has_prefix(filename, "TTH/"))
        local_filename = share_translate_tth(global_share, filename + 4);
    else if ((fl_type = is_filelist(filename)) != FILELIST_NONE) {
        if (fl_type == FILELIST_DCLST) {
            xerr_set(err, -1, "NMDC-style lists no longer supported, please upgrade your client");
            ui_send_status_message(NULL, cc->hub->address,
                    "Peer '%s' uses obsolete client, filelist browsing denied",
                    cc->nick);
            return -1;
        }

        int num_returned_bytes = asprintf(&local_filename, "%s/%s", global_working_directory, filename);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
        if (share_save(global_share, fl_type) != 0) {
            WARNING("failed to save share");
            free(local_filename);
            xerr_set(err, -1, "File Not Available");
            return -1;
        }
    }
    else
        local_filename = share_translate_path(global_share, filename);

    if (local_filename == NULL) {
        INFO("%s: couldn't translate path, outside share?", filename);
        xerr_set(err, -1, "File Not Available");
        return -1;
    }

    if (strcmp(cc->nick, cc->hub->me->nick) == 0) {
        WARNING("attempt to spoof my own nick");
        free(local_filename);
        xerr_set(err, -1, "File Not Available");
        return -1;
    }

    struct stat stbuf;
    if (stat(local_filename, &stbuf) != 0) {
        INFO("%s: %s", local_filename, strerror(errno));
        free(local_filename);
        xerr_set(err, -1, "File Not Available");
        return -1;
    }

    if (!S_ISREG(stbuf.st_mode)) {
        INFO("%s: not a regular file", local_filename);
        free(local_filename);
        xerr_set(err, -1, "File Not Available");
        return -1;
    }

    if (bytes_to_transfer > 0) {
        if (stbuf.st_size < offset + bytes_to_transfer) {
            INFO("%s: Request for too many bytes: st_size = %"PRIu64","
                    " offset = %"PRIu64", bytes_to_transfer = %"PRIu64,
                    local_filename, (uint64_t)stbuf.st_size,
                    offset, bytes_to_transfer);
            free(local_filename);
            xerr_set(err, -1, "File Not Available");
            return -1;
        }
        cc->bytes_to_transfer = bytes_to_transfer;
    }
    else
        cc->bytes_to_transfer = stbuf.st_size - offset;
        
    DEBUG("set cc->bytes_to_transfer = %"PRIu64, cc->bytes_to_transfer);

    cc->local_fd = open(local_filename, O_RDONLY);
    if (cc->local_fd == -1) {
        INFO("failed to open %s: %s", local_filename, strerror(errno));
        free(local_filename);
        xerr_set(err, -1, "File Not Available");
        return -1;
    }
    INFO("opened local file [%s] on fd %d", local_filename, cc->local_fd);

    if (lseek(cc->local_fd, offset, SEEK_SET) == -1) {
        WARNING("lseek failed: %s", strerror(errno));
        free(local_filename);
        xerr_set(err, -1, "File Not Available");
        return -1;
    }

    cc->offset = offset;
    cc->local_filename = local_filename;
    cc->filesize = stbuf.st_size;
    cc->bytes_done = 0ULL;

    cc->upload_buf_size = cc->upload_buf_offset = 0;

    return 0;
}

