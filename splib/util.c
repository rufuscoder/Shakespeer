/*
 * Copyright 2004-2006 Martin Hedenfalk <martin@bzero.se>
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
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>

#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include "util.h"
#include "log.h"
#include "args.h"
#include "base32.h"
#include "xstr.h"
#include "dstring.h"
#include "dstring_url.h"
#include "rx.h"

#ifndef COREDUMPS_ENABLED
# define COREDUMPS_ENABLED 0
#endif

#define MAX_HUMAN_LEN 10
#define HUMAN_SIZE_STR_TABLE_SIZE 5

char *str_size_human(uint64_t size)
{
    static int str_table_index = 0;
    static char str_table[HUMAN_SIZE_STR_TABLE_SIZE][MAX_HUMAN_LEN];

    ++str_table_index;
    str_table_index %= HUMAN_SIZE_STR_TABLE_SIZE;

    char *s = str_table[str_table_index];

    if(size < 1024ULL)
    {
        snprintf(s, MAX_HUMAN_LEN, "%u B", (unsigned)size);
    }
    else if(size < 999.5*1024) /* kilobinary */
    {
        snprintf(s, MAX_HUMAN_LEN, "%.1f KiB", (double)size/1024);
    }
    else if(size < 999.5*1024*1024) /* megabinary */
    {
        snprintf(s, MAX_HUMAN_LEN, "%.1f MiB", (double)size/(1024*1024));
    }
    else if(size < 999.5*1024*1024*1024) /* gigabinary */
    {
        snprintf(s, MAX_HUMAN_LEN, "%.1f GiB", (double)size/(1024*1024*1024));
    }
    else /* terabinary */
    {
        snprintf(s, MAX_HUMAN_LEN, "%.2f TiB",
                (double)size/((uint64_t)1024*1024*1024*1024));
    }

    return s;
}

char *data_to_hex(const char *data, unsigned int len)
{
    char *strHexData;
    char *e;
    unsigned int i, k = 0;

    e = strHexData = (char *)malloc(len * (3 + 1) +
                                    ((len / 16) + 1) * (7 + 8) + 1 + 16*3);
    for(i = 0; i < len; i++)
    {
        if(i % 16 == 0)
        {
            snprintf(e, 8, "\n%04X: ", i);
            e += 7;
            k = 0;
        }
        snprintf(e, 4, "%02X ", (unsigned char)data[i] & 0xFF);
        e += 3;
        k++;

        if(k == 16 || i + 1 == len)
        {
            unsigned int j;
            snprintf(e, 9, "        ");
            e += 8;
            for(j = 0; j < 16 - k; j++)
            {
                snprintf(e, 4, "   ");
                e += 3;
            }
            for(j = 0; j < k; j++)
                *e++ = isprint(data[i+1+j-k]) ? data[i+1+j-k] : '.';
        }
    }

    *e++ = '\n';
    *e = 0;

    return strHexData;
}

void print_command(const char *command, const char *fmt, ...)
{
    char *prestr;

    va_list ap;
    va_start(ap, fmt);
    int num_returned_bytes = vasprintf(&prestr, fmt, ap);
    if (num_returned_bytes == -1)
        DEBUG("vasprintf did not return anything");
    va_end(ap);

    if (str_has_prefix(command, "$Key")) {
        char *hex = data_to_hex(command + 5, strlen(command + 5));
        DEBUG("%s $Key 0x%s|", prestr, hex);
        free(hex);
    }
    else if (str_has_prefix(command, "$Lock")) {
        char *hex = data_to_hex(command + 6, strlen(command + 6));
        DEBUG("%s $Lock %s|", prestr, hex);
        free(hex);
    }
    else if (str_has_prefix(command, "add-hash$")) {
	    char *f = strchr(command + 9, '$');
	    if(f) {
	        f = strchr(f + 1, '$');
	        char *l = xstrndup(command, f - command);
	        DEBUG("%s %s", prestr, l);
	        free(l);
	    }
    }
    else if (str_has_prefix(command, "$MyPass"))
        DEBUG("%s $MyPass ...|", prestr);
    else
        DEBUG("%s %s", prestr, command);

    free(prestr);
}

char *tilde_expand_path(const char *path)
{
    return_val_if_fail(path, NULL);
    if (path[0] == '~' && (path[1] == '/' || path[1] == 0)) {
        const char *home = getenv("HOME");
        if (home) {
            char *ret;
            int num_returned_bytes = asprintf(&ret, "%s%s", home, path + 1);
            if (num_returned_bytes == -1)
                DEBUG("vasprintf did not return anything");
            return ret;
        }
    }
    return strdup(path);
}

char *absolute_path(const char *path)
{
    return_val_if_fail(path, NULL);

    char *abspath = NULL;
    if (path[0] == '~')
        abspath = tilde_expand_path(path);
    else if (path[0] != '/') {
        char cwd[MAXPATHLEN];
        char *returned_buf = getcwd(cwd, MAXPATHLEN);
        if (!returned_buf)
            DEBUG("getcwd did not return a valid path");
        int num_returned_bytes = asprintf(&abspath, "%s/%s", cwd, path);
        if (num_returned_bytes == -1)
            DEBUG("vasprintf did not return anything");
    }
    else
        abspath = strdup(path);

    return abspath;
}

char *verify_working_directory(const char *basedir)
{
    struct stat sb;
    char *abs_basedir = absolute_path(basedir);

    if((stat(abs_basedir, &sb) == 0 && !S_ISDIR(sb.st_mode)) ||
       mkpath(abs_basedir) != 0)
    {
        free(abs_basedir);
        abs_basedir = NULL;
    }
    return abs_basedir;
}

/* returned string should be freed by caller
 */
char *get_working_directory(void)
{
#ifdef __APPLE__
    char *basedir = verify_working_directory("~/Library/Application Support/ShakesPeer");
#else
    char *basedir = verify_working_directory("~/.shakespeer");
#endif

    if(basedir == 0)
    {
        const char *tmp = getenv("TMPDIR");
        if(tmp == NULL) tmp = getenv("TMP");
        if(tmp == NULL) tmp = getenv("TEMP");
        if(tmp == NULL) tmp = "/tmp";

        basedir = strdup(tmp);
        WARNING("Couldn't find application support directory,"
                  " falling back to temp directory '%s'", basedir);
    }

    return basedir;
}

/* Finds the start of the word including the position in <pos> in the string
 * <string>. <delimiters> are all valid word break characters. The end of the
 * string (*pos == '\0') is also treated as a word break. Returns a pointer >=
 * string && <= pos. Only handles backslash quoting. Returns NULL on failure.
 */
const char *str_find_word_start(const char *string, const char *pos,
        const char *delimiters)
{
    if(string == NULL || pos < string || delimiters == NULL)
    {
        return NULL;
    }

    while(--pos && pos > string)
    {
        if(strchr(delimiters, pos[0]) != 0 && pos[-1] != '\\')
        {
            return pos + 1;
        }
    }

    return string;
}

void set_corelimit(int enabled)
{
    struct rlimit rl;
    if(getrlimit(RLIMIT_CORE, &rl) == 0)
    {
        if(enabled)
            rl.rlim_cur = rl.rlim_max;
        else
            rl.rlim_cur = 0;

        if(setrlimit(RLIMIT_CORE, &rl) == -1)
        {
            WARNING("setrlimit(RLIMIT_CORE): %s", strerror(errno));
        }
    }
    else
    {
        WARNING("getrlimit(RLIMIT_CORE): %s", strerror(errno));
    }
    int result = chdir("/");
    if (result == -1)
        DEBUG("chdir could not change directory");
    result = chdir("/tmp");
        DEBUG("chdir could not change directory");
}

int
sp_daemonize(void)
{
	pid_t pid;

	pid = fork();
	if(pid < 0)
	{
		WARNING("failed to daemonize: %s", strerror(errno));
 		return -1;
	}
	if(pid > 0)
	{
		/* parent */
		exit(0);
	}

	setsid();

	/* set stdin, stdout and stderr to /dev/null */
	close(0);
	close(1);
	close(2);
	int fd = open("/dev/null", O_RDWR);
	int result = dup(fd);
	if (result == -1)
	    DEBUG("dup did not return a valid file descriptor");
	result = dup(fd);
	if (result == -1)
	    DEBUG("dup did not return a valid file descriptor");

	result = chdir("/");
	if (result == -1)
	    DEBUG("chdir could not change directory");

	set_corelimit(COREDUMPS_ENABLED); /* enable coredumps */

	/* block SIGPIPE signal */
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGPIPE);
	if(sigprocmask(SIG_BLOCK, &sigset, NULL) != 0)
		WARNING("sigprocmask: %s", strerror(errno));

	return 0;
}

pid_t sp_get_pid(const char *workdir, const char *appname)
{
    return_val_if_fail(workdir && appname, 0);

    char *pidfile;
    int num_returned_bytes = asprintf(&pidfile, "%s/%s.pid", workdir, appname);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    pid_t pid = -1;

    FILE *fp = fopen(pidfile, "r");
    if(fp != NULL)
    {
        int rc = fscanf(fp, "%u", &pid);
        fclose(fp);
        if(rc != 1)
        {
            WARNING("failed to read pidfile '%s'", pidfile);
            pid = -1;
        }
        else
        {
            if(kill(pid, 0) != 0)
            {
                WARNING("pid %u from pidfile is invalid, stale pidfile?",
                        pid);
                pid = -1;
                sp_remove_pid(workdir, appname);
            }
        }
    }

    free(pidfile);
    return pid;
}

int sp_write_pid(const char *workdir, const char *appname)
{
    return_val_if_fail(workdir && appname, -1);

    char *pidfile;
    int num_returned_bytes = asprintf(&pidfile, "%s/%s.pid", workdir, appname);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");

    FILE *fp = fopen(pidfile, "w");
    if (fp == NULL)
        WARNING("failed to open pidfile '%s': %s", pidfile, strerror(errno));
    else
        fprintf(fp, "%u\n", getpid());

    free(pidfile);
    
    if (fp) {
        fclose(fp);
        return 0;
    }
    else
        return -1;
}

void sp_remove_pid(const char *workdir, const char *appname)
{
    return_if_fail(workdir && appname);

    char *pidfile = 0;
    int num_returned_bytes = asprintf(&pidfile, "%s/%s.pid", workdir, appname);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
    unlink(pidfile); /* ignore errors */
    free(pidfile);
}

int valid_tth(const char *tth)
{
    if(tth == NULL)
        return 0;
    int tthlen = strlen(tth);
    if(tthlen < 39 || tthlen > 40)
        return 0;
    char decbuf[25];
    return (base32_decode_into(tth, tthlen, decbuf) == 24 ? 1 : 0);
}

static const char *share_filetypes[][12] = {
    /* 0:  */ {0},
    /* 1: any */ {0},
    /* 2: audio */ {"mp3", "mp2", "wav", "au", "rm", "mid", "sm", "ogg", "wma", "m4a", 0},
    /* 3: compressed */ {"zip", "arj", "rar", "lzh", "gz", "z", "arc", "pak", "sit", "dmg", "bz2", 0},
    /* 4: documents */ {"doc", "txt", "wri", "pdf", "ps", "tex", "html", "rtf", 0},
    /* 5: executables */ {"pm", "exe", "bat", "com", 0},
    /* 6: pictures */ {"gif", "jpg", "jpeg", "bmp", "pcx", "png", "wmf", "psd", 0},
    /* 7: video */ {"mpg", "mpeg", "avi", "asf", "mov", "wmv", "ogm", 0}
};

share_type_t share_filetype(const char *filename)
{
    return_val_if_fail(filename, SHARE_TYPE_ANY);

    share_type_t type;
    int i;
    char *ext = strrchr(filename, '.');

    type = SHARE_TYPE_ANY;

    if(ext++)
    {
        for(i = 2; i < 8; i++)
        {
            int j;
            for(j = 0; share_filetypes[i][j]; j++)
            {
                if(strcasecmp(ext, share_filetypes[i][j]) == 0)
                {
                    type = i;
                    break;
                }
            }
            if(type != SHARE_TYPE_ANY)
                break;
        }
    }

    return type;
}

/* FIXME: missing test */
char *str_shorten_path(const char *path, int maxlen)
{
    char *p = strdup(path);
    while (1) {
        if (strlen(p) <= maxlen)
            break;

        char *fs = strchr(p, '/'); /* first slash */
        char *ss = NULL;           /* second slash */
        if (fs) {
            char *sse = fs;
            while (1) {
                ss = strchr(++sse, '/');
                if (ss == NULL || strcmp(sse, "/.../") != 0) {
                    ss = NULL;
                    break;
                }
                sse = ss;
            }
        }
        if (ss == NULL || fs == p) {
            char *tmp;
            int num_returned_bytes = asprintf(&tmp, "...%s", p + strlen(p) - (maxlen - 3));
            if (num_returned_bytes == -1)
                DEBUG("asprintf did not return anything");
            free(p);
            p = tmp;
            break;
        }
        else {
            char *tmp = xstrndup(p, fs - p);
            char *tmp2;
            int num_returned_bytes = asprintf(&tmp2, "%s/...%s", tmp, ss);
            if (num_returned_bytes == -1)
                DEBUG("vasprintf did not return anything");
            free(tmp);
            free(p);
            p = tmp2;
        }
    }

    return p;
}

/* looks for an existing (compressed) filelist from nick in the working
 * directory returns a malloced string if found (either xml or dclst type)
 *
 * returns NULL if not found
 */
char *find_filelist(const char *working_directory, const char *nick)
{
    return_val_if_fail(working_directory, NULL);
    return_val_if_fail(nick, NULL);

    int num_returned_bytes;
    char *xnick = strdup(nick);
    str_replace_set(xnick, "/", '_');

    /* first check for an xml filelist */
    char *filelist_path;
    num_returned_bytes = asprintf(&filelist_path, "%s/files.xml.%s.bz2", working_directory, xnick);
    if (num_returned_bytes == -1)
        DEBUG("asprintf did not return anything");
        
    if (access(filelist_path, F_OK) != 0) {
        /* nope, look for a dclst filelist */
        free(filelist_path);
        num_returned_bytes = asprintf(&filelist_path, "%s/MyList.%s.DcLst", working_directory, xnick);
        if (num_returned_bytes == -1)
            DEBUG("asprintf did not return anything");

        if (access(filelist_path, F_OK) != 0) {
            free(filelist_path);
            filelist_path = NULL;
        }
    }

    free(xnick);

    return filelist_path;
}

int is_filelist(const char *filename)
{
    return_val_if_fail(filename, FILELIST_NONE);

    int rc = FILELIST_NONE;
    const char *basename = strrchr(filename, '/');
    if(basename++ == NULL) basename = filename;

    if(str_has_prefix(basename, "MyList") /*&& g_str_has_suffix(basename, ".DcLst")*/)
    {
        rc = FILELIST_DCLST;
    }
    else if(str_has_prefix(basename, "files.xml") /*&& g_str_has_suffix(basename, ".bz2")*/)
    {
        rc = FILELIST_XML;
    }

    return rc;
}

/* Splits the <hostport> string into a host and port value. <hostport> assumed
 * to be on the form "host[:port]". Sets port to zero if no port specified, or
 * -1 if invalid port. Returns 0 on success, or -1 if called with invalid
 *  arguments.
 */
int split_host_port(const char *hostport, char **host, int *port)
{
    return_val_if_fail(hostport && *hostport, -1);
    return_val_if_fail(*hostport != ':', -1);
    return_val_if_fail(host, -1);
    return_val_if_fail(port, -1);

    const char *colon = strchr(hostport, ':');
    if(colon)
    {
        *host = xstrndup(hostport, colon - hostport);

        char *e = 0;
        errno = 0;
        uint64_t p = strtoul(colon + 1, &e, 10);
        if(colon[1] == 0 || *e != 0 || (p == ULONG_MAX && errno == ERANGE))
        {
            *port = -1;
        }
        else
        {
            *port = p;
        }
    }
    else
    {
        *host = strdup(hostport);
        *port = 0;
    }

    return 0;
}

pid_t sp_exec(const char *path, const char *basedir)
{
	INFO("spawning executable: %s -w %s -d %s",
		path, basedir, sp_log_get_level());
	pid_t pid = fork();
	if(pid == 0)
	{
		/* child process */

		char *exp_path = tilde_expand_path(path);
		assert(exp_path);
		execl(exp_path, exp_path,
			"-w", basedir,
			"-d", sp_log_get_level(),
			(char *)NULL);

		WARNING("failed to exec %s: %s", exp_path, strerror(errno));
		exit(1);
	}
	else if(pid < 0)
	{
		WARNING("failed to fork: %s", strerror(errno));
	}
	else
	{
		INFO("forked, pid = %i", pid);
	}

	return pid;
}

char *get_exec_path(const char *argv0)
{
    char *p = strdup(argv0);
    char *e = strrchr(p, '/');
    if(e)
        *e = 0;
    else
    {
        free(p);
        p = strdup(".");
    }
    char *argv0_path = absolute_path(p);
    free(p);

    return argv0_path;
}

