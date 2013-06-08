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

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/time.h>
#include <event.h>

#include "log.h"
#include "util.h"

#define SP_LOG_MAX_FILES 5
#define SP_LOG_MAX_BYTES_NORMAL 2*1024*1024
#define SP_LOG_MAX_BYTES_DEBUG 10*1024*1024

static int sp_log_max_bytes = SP_LOG_MAX_BYTES_NORMAL;

static FILE *logfp = NULL;
static char *logfile = NULL;
static int max_log_level = LOG_LEVEL_INFO;

static int sp_log_reinit(void);

void sp_log_set_level(const char *level)
{
    return_if_fail(level);
    sp_log_max_bytes = SP_LOG_MAX_BYTES_NORMAL;
    if(strcasecmp(level, "none") == 0)
        max_log_level = LOG_LEVEL_ERROR;
    else if(strcasecmp(level, "warning") == 0)
        max_log_level = LOG_LEVEL_WARNING;
    else if(strcasecmp(level, "message") == 0)
        max_log_level = LOG_LEVEL_INFO;
    else if(strcasecmp(level, "info") == 0)
        max_log_level = LOG_LEVEL_INFO;
    else if(strcasecmp(level, "debug") == 0)
    {
        max_log_level = LOG_LEVEL_DEBUG;
        sp_log_max_bytes = SP_LOG_MAX_BYTES_DEBUG;
    }
}

const char *sp_log_get_level(void)
{
    if(max_log_level == LOG_LEVEL_DEBUG)
        return "debug";
    else if(max_log_level == LOG_LEVEL_INFO)
        return "info";
    else if(max_log_level == LOG_LEVEL_WARNING)
        return "warning";
    else if(max_log_level == LOG_LEVEL_ERROR)
        return "error";
    else
        return "none";
}

static void sp_glog_func(int log_level, const char *message)
{
    if(log_level > max_log_level)
        return;

    FILE *fp = logfp;
    if(fp == NULL)
        fp = stderr;

    time_t now = time(0);
    struct tm *tm = localtime(&now);
    char tmbuf[32];
    strftime(tmbuf, sizeof(tmbuf), "%a %e %H:%M:%S", tm);
    if((log_level & (LOG_LEVEL_WARNING | LOG_LEVEL_ERROR | LOG_LEVEL_CRITICAL)) > 0)
    {
        fprintf(fp, "%s ***** %s *****\n", tmbuf, message);
    }
    else
    {
        fprintf(fp, "%s %s\n", tmbuf, message);
    }

    if(fp == logfp && ftell(logfp) > sp_log_max_bytes)
    {
        fprintf(logfp, "logfile turned over due to size > %u\n", sp_log_max_bytes);
        fclose(logfp);
        logfp = NULL;
        sp_log_reinit();
    }
}

void sp_vlog(int level, const char *fmt, va_list ap)
{
    char *msg;
    int num_returned_bytes = vasprintf(&msg, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    sp_glog_func(level, msg);
    free(msg);
}

void sp_log(int level, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    sp_vlog(level, fmt, ap);
    va_end(ap);
}

static void sp_log_rotate(const char *logfile)
{
    struct stat sb;
    int num_returned_bytes;
    int rc = stat(logfile, &sb);
    if (rc == 0 && sb.st_size > sp_log_max_bytes) {
        int i;
        for (i = 0; i < SP_LOG_MAX_FILES; i++) {
            char *tmp;
            num_returned_bytes = asprintf(&tmp, "%s.%i", logfile, i);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");
            int exists = access(tmp, F_OK);
            free(tmp);
            if (exists != 0)
                break;
        }
        int maxi = i;
        if (maxi == SP_LOG_MAX_FILES) {
            char *tmp;
            num_returned_bytes = asprintf(&tmp, "%s.%i", logfile, SP_LOG_MAX_FILES - 1);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");
            unlink(tmp);
            free(tmp);
            --maxi;
        }
        for (i = maxi; i > 0; i--) {
            char *prev;
            char *next;
            num_returned_bytes = asprintf(&prev, "%s.%i", logfile, i - 1);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");
            num_returned_bytes = asprintf(&next, "%s.%i", logfile, i);
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");

            rename(prev, next);
            free(prev);
            free(next);
        }

        char *next;
        num_returned_bytes = asprintf(&next, "%s.0", logfile);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
        rename(logfile, next);
        free(next);
    }
}

static void event_log_callback(int severity, const char *msg)
{
    int log_level;
    switch(severity)
    {
        case _EVENT_LOG_DEBUG:
            log_level = LOG_LEVEL_DEBUG;
            break;
        case _EVENT_LOG_MSG:
            log_level = LOG_LEVEL_INFO;
            break;
        case _EVENT_LOG_WARN:
            log_level = LOG_LEVEL_WARNING;
            break;
        case _EVENT_LOG_ERR:
        default:
            log_level = LOG_LEVEL_ERROR;
            break;
    }
    sp_glog_func(log_level, msg);
}

static int sp_log_reinit(void)
{
    return_val_if_fail(logfile, -1);
    return_val_if_fail(logfp == NULL, -1);

    sp_log_rotate(logfile);
    logfp = fopen(logfile, "a");
    fprintf(logfp, "opened logfile '%s'\n", logfile);
    if(logfp)
    {
        event_set_log_callback(event_log_callback);
        
        setvbuf(logfp, NULL, _IOLBF, 0);
    }
    return 0;
}

int sp_log_init(const char *workdir, const char *prefix)
{
    /* special case when upgrading from 0.3 to 0.4 */
    char *old_logfile;
    int num_returned_bytes;
    num_returned_bytes = asprintf(&old_logfile, "%s/log-%s", workdir, prefix);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    unlink(old_logfile); /* ignore errors */
    free(old_logfile);

    if (logfp == NULL) {
        return_val_if_fail(workdir && prefix, -1);
        char *forced_workdir = 0;
#ifdef __APPLE__
        /* if we're using the default working directory on Mac OS X, force the
         * logfiles into ~/Library/Logs (yes, this is a hack) */
        char *default_workdir = get_working_directory();
        if (strcmp(default_workdir, workdir) == 0) {
            forced_workdir = verify_working_directory("~/Library/Logs");
            if (forced_workdir)
                workdir = forced_workdir;
        }
        free(default_workdir);
#endif
        num_returned_bytes = asprintf(&logfile, "%s/%s.log", workdir, prefix);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");
        free(forced_workdir);
        sp_log_reinit();
    }
    return logfp ? 0 : -1;
}

void sp_log_close(void)
{
    if(logfp)
    {
        fclose(logfp);
        logfp = 0;
        free(logfile);
        logfile = 0;
    }
}

