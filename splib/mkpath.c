/*
 * Copyright 2004-2005 Martin Hedenfalk <martin@bzero.se>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#ifdef STDC_HEADERS /* HAVE_STRING_H */
# include <stdlib.h>
# include <string.h>
#else
# include <strings.h>
# ifndef strchr
#  define strchr index
# endif
#endif

#include "xstr.h"

static char *trim(char *str, char *trim_chars)
{
    char *endstr;

    if(!str)
        return str;

    endstr = strchr(str, 0) - 1;

    while(endstr >= str && strchr(trim_chars, *endstr))
    {
        *endstr = 0;
        endstr--;
    }

    return str;
}

int mkpath(const char *path)
{
    struct stat stbuf;
    char *xpath;
   
    if (path == 0)
        return -1;
    xpath = strdup(path);

    if(stat(xpath, &stbuf) == 0)
    { /* path already exists */
        free(xpath);
        /* check if it's a directory */
        if(!S_ISDIR(stbuf.st_mode))
        {
            return -1;
        }
        /* the complete path exists and is ok */
        return 0;
    }
    else
    { /* path does not exist */
        char *slash, *oslash = xpath;
        char *xdir;
        int done = 0;

        while(!done)
        {
            slash = strchr(oslash, '/');
            if(slash)
                ++slash;
            else
            {
                slash = xpath + strlen(xpath);
                done = 1;
            }
            xdir = xstrndup(xpath, slash - xpath);
#ifdef TESTA
            printf("xdir == '%s'\n", xdir);
#endif
            if(xdir[0] != '/')
                trim(xdir, "/");
            if(stat(xdir, &stbuf) == 0)
            {
                /* check if it's a directory */
                if(!S_ISDIR(stbuf.st_mode))
                {
                    free(xpath);
                    free(xdir);
                    return -1;
                }
            }
            else
            {
                /* directory doesn't exist, create it */
                if(mkdir(xdir, 0755) != 0)
                {
                    free(xpath);
                    free(xdir);
                    return -1;
                }
            }
            free(xdir);
            oslash = slash;
        }
    }

    free(xpath);
    return 0;
}


#ifdef TESTA

int main(int argc, char **argv)
{
    if(argc < 2)
        return 1;

    if(mkpath(argv[1]) != 0)
        perror(argv[1]);
}

#endif

