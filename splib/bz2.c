/*
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
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
#include <errno.h>
#include <bzlib.h>
#include <string.h>

#include "xerr.h"

int bz2_encode(const char *ifilename, const char *ofilename, xerr_t **err)
{
    FILE *fpIn, *fpOut;
    BZFILE *bzfp;
    int bzerror;
    unsigned char buf[4096];
    size_t n;

    fpIn = fopen(ifilename, "r");
    if(fpIn == 0)
    {
        xerr_set(err, -1, "%s: %s", ifilename, strerror(errno));
        return -1;
    }

    fpOut = fopen(ofilename, "w");
    if(fpOut == 0)
    {
        xerr_set(err, -1, "%s: %s", ofilename, strerror(errno));
        fclose(fpIn);
        return -1;
    }

    bzfp = BZ2_bzWriteOpen(&bzerror, fpOut, 6, 0, 0);
    if(bzerror == BZ_OK)
    {
        do
        {
            n = fread(buf, 1, 4096, fpIn);
            if(n != 4096)
            {
                if(ferror(fpIn))
                {
                    xerr_set(err, -1, "%s: %s", ifilename, strerror(errno));
                    break;
                }
            }

            BZ2_bzWrite(&bzerror, bzfp, buf, n);
            if(bzerror != BZ_OK)
            {
                xerr_set(err, -1, "bzWrite failed with error code %d",
                        bzerror);
                break;
            }
        } while(n == 4096);
    }
    else
    {
        xerr_set(err, -1,
                "bz2_encode: bzWriteOpen failed with error code %d", bzerror);
    }

    BZ2_bzWriteClose(&bzerror, bzfp, 0, NULL, NULL);
    fclose(fpOut);
    fclose(fpIn);

    return 0;
}

int bz2_decode(const char *ifilename, const char *ofilename, xerr_t **err)
{
    FILE *fpIn, *fpOut;
    BZFILE *bzfp;
    int bzerror;
    unsigned char buf[4096];
    int n, w;

    fpIn = fopen(ifilename, "r");
    if(fpIn == 0)
    {
        xerr_set(err, -1, "%s: %s", ifilename, strerror(errno));
        return -1;
    }

    fpOut = fopen(ofilename, "w");
    if(fpOut == 0)
    {
        xerr_set(err, -1, "%s: %s", ofilename, strerror(errno));
        fclose(fpIn);
        return -1;
    }

    bzfp = BZ2_bzReadOpen(&bzerror, fpIn, 0, 0, NULL, 0);
    if(bzerror == BZ_OK)
    {
        do
        {
            n = BZ2_bzRead(&bzerror, bzfp, buf, 4096);
            if(bzerror != BZ_OK && bzerror != BZ_STREAM_END)
            {
                xerr_set(err, -1, "bzRead failed with error code %d", bzerror);
                break;
            }

            w = fwrite(buf, 1, n, fpOut);
            if(w != n)
            {
                xerr_set(err, -1, "%s: %s", ofilename, strerror(errno));
                break;
            }

        } while(bzerror != BZ_STREAM_END);
    }
    else
        xerr_set(err, -1, "bzReadOpen failed with error code %d", bzerror);

    BZ2_bzReadClose(&bzerror, bzfp);
    fclose(fpOut);
    fclose(fpIn);

    return 0;
}

