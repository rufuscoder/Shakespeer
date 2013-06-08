/*
 * Copyright 2004 Martin Hedenfalk <martin@bzero.se>
 * Based on codec.cxx from pyDC by Anakim Border <aborder@users.sourceforge.net>
 * rewritten in vanilla C
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "xerr.h"

typedef struct code code_t;
struct code
{
    unsigned int freq;
    unsigned int code;  /* assume a code fits in 32 bits */
    unsigned char data;
    int len;
};

typedef struct node node_t;
struct node
{
    node_t *left;
    node_t *right;
    code_t *code;
    unsigned int weight;
};

typedef struct bitfile bitfile_t;
struct bitfile
{
    FILE *fp;
    unsigned char data;
    int pos;
};

static int node_cmp(const void *a, const void *b)
{
    node_t *na = *(node_t **)a, *nb = *(node_t **)b;

    if(na->weight < nb->weight)
        return -1;
    else if(na->weight == nb->weight && nb->left == 0)
        return -1;
    return 1;
}

static void bitfile_put(bitfile_t *bitfile, int bit)
{
    if(bitfile->pos == 0)
        bitfile->data = 0;

    bitfile->data |= (bit << bitfile->pos);

    if(++bitfile->pos == 8)
    {
        fputc(bitfile->data, bitfile->fp);
        bitfile->pos = 0;
    }
}

static void bitfile_nput(bitfile_t *bitfile, unsigned int code, int len)
{
    int i;

    for(i = len-1; i >= 0; i--)
        bitfile_put(bitfile, (code >> i) & 0x01);
}

static int bitfile_get(bitfile_t *bitfile)
{
    int bit;

    if(bitfile->pos == 0)
    {
        int c = fgetc(bitfile->fp);
        if(c == EOF)
            WARNING("bitfile_get: EOF!");
        bitfile->data = c;
    }

    bit = (bitfile->data >> (bitfile->pos & 7)) & 1;

    if(++bitfile->pos == 8)
        bitfile->pos = 0;

    return bit;
}

static unsigned int bitfile_nget(bitfile_t *bitfile, int len)
{
    unsigned int code = 0;
    int i;

    for(i = 0; i < len; i++)
        code = (code << 1) | bitfile_get(bitfile);
    return code;
}

static void bitfile_flush(bitfile_t *bitfile)
{
    if(bitfile->pos != 0)
        fputc(bitfile->data, bitfile->fp);
    bitfile->pos = 0;
}

static void gen_codes(node_t *root, unsigned int base, int len)
{
    if(root == NULL)
        return;

    if(root->left)
    {
        len++;
        gen_codes(root->left, base << 1, len);
        gen_codes(root->right, (base << 1) | 1, len);
    }
    else
    {
        root->code->code = base;
        root->code->len = len;
    }
}

#ifdef DEBUG
static char *bits2str(unsigned int code, int len)
{
    static char buf[33];
    int i;

    for(i = 0; i < len; i++)
    {
        buf[i] = '0' + ((code & (1 << (len-1-i))) ? 1 : 0);
    }
    buf[i] = 0;
    return buf;
}
#endif

void he3_free_nodes(node_t *root)
{
    if(root)
    {
        if(root->left)
            he3_free_nodes(root->left);
        if(root->right)
            he3_free_nodes(root->right);
        free(root);
    }
}

int he3_encode(const char *ifilename, const char *ofilename, xerr_t **err)
{
    FILE *fpIn, *fpOut;
    code_t codetab[256];
    int c, i, n;
    node_t **nodes;
    int nleaves = 0;
    node_t *root = 0;
    unsigned char crc = 0;
    unsigned int len = 0;
    bitfile_t bitfile;

    memset(&bitfile, 0, sizeof(bitfile_t));

    if(ifilename)
        fpIn = fopen(ifilename, "r");
    else
        fpIn = stdin;

    if(fpIn == 0)
    {
        xerr_set(err, -1, "%s: %s", ifilename, strerror(errno));
        return -1;
    }

    if(ofilename)
        fpOut = fopen(ofilename, "w");
    else
        fpOut = stdout;

    if(fpOut == 0)
    {
        xerr_set(err, -1, "%s: %s", ofilename, strerror(errno));
        if(fpIn != stdin)
            fclose(fpIn);
        return -1;
    }

    /* Initialize code table
     */
    memset(&codetab, 0, sizeof(codetab));
    for(i = 0; i < 256; i++)
        codetab[i].data = (unsigned char)i;

    /* Calculate the frequency of all symbols
     */
    while((c = fgetc(fpIn)) != EOF)
    {
        crc = crc ^ (unsigned char)c;
        if(codetab[c].freq == 0)
            nleaves++;
        codetab[c].freq++;
        len++;
    }
    rewind(fpIn);

    /* Create leaf nodes
     */
    nodes = calloc(nleaves, sizeof(node_t *));
    for(i = n = 0; i < 256; i++)
    {
        if(codetab[i].freq)
        {
            node_t *node = calloc(1, sizeof(node_t));
            node->left = 0;
            node->right = 0;
            node->code = &codetab[i];
            node->weight = codetab[i].freq;
            nodes[n++] = node;
        }
    }

    for(i = 0; i + 1 < nleaves; i++)
    {
        qsort(nodes+i, nleaves-i, sizeof(node_t *), node_cmp);

        root = calloc(1, sizeof(node_t));
        root->left = nodes[i];
        root->right = nodes[i+1];
        root->weight = root->left->weight + root->right->weight;
        root->code = 0;
        nodes[i+1] = root;
    }

    gen_codes(root, 0, 0);

    fprintf(fpOut, "HE3\x0D%c%c%c%c%c%c%c", crc,
            len & 0xFF, (len >> 8) & 0xFF,
            (len >> 16) & 0xFF, (len >> 24) & 0xFF,
            nleaves & 0xFF, (nleaves >> 8) & 0XFF);

    for(i = 0; i < 256; i++)
    {
        if(codetab[i].freq)
        {
            fputc(codetab[i].data, fpOut);
            fputc(codetab[i].len, fpOut);
        }
    }

    bitfile.fp = fpOut;
    bitfile.pos = 0;

    for(i = 0; i < 256; i++)
    {
        if(codetab[i].freq)
        {
            bitfile_nput(&bitfile, codetab[i].code, codetab[i].len);
#ifdef DEBUG
            INFO("symbol 0x%02X: code: %s, length: %d",
                    codetab[i].data, bits2str(codetab[i].code, codetab[i].len),
                    codetab[i].len);
#endif
        }
    }

    bitfile_flush(&bitfile);

#ifdef DEBUG
    INFO("ftell(fpOut) == %ld", ftell(fpOut));
#endif
    while((c = fgetc(fpIn)) != EOF)
    {
#ifdef DEBUG_2
        fprintf(stdout, "encoding symbol 0x%02X (%c) as %s\n",
                c, isascii(c) ? c : '.',
                bits2str(codetab[c].code, codetab[c].len));
#endif
        bitfile_nput(&bitfile, codetab[c].code, codetab[c].len);
    }
    if(bitfile.pos > 0)
        fputc(bitfile.data, bitfile.fp);

    he3_free_nodes(root);
    free(nodes);

    if(fpIn != stdin)
        fclose(fpIn);
    if(fpOut != stdout)
        fclose(fpOut);

    return 0;
}

int he3_decode(const char *ifilename, const char *ofilename, xerr_t **err)
{
    FILE *fpIn, *fpOut;
    char magic[4];
    unsigned int len;
    unsigned char crc, calc_crc;
    unsigned int ncodes;
    code_t codetab[256];
    int i;
    bitfile_t bitfile;
    unsigned char *decodetab;
    int maxlen;
    int rc = 0;

    if (ifilename)
        fpIn = fopen(ifilename, "r");
    else
        fpIn = stdin;

    if (fpIn == 0) {
        xerr_set(err, -1, "%s: %s", ifilename, strerror(errno));
        return -1;
    }

    size_t bytes_read = fread(magic, 1, 4, fpIn);
    if (bytes_read == 0)
        DEBUG("fread did not return any bytes");
    if (memcmp(magic, "HE3\x0D", 4) != 0) {
        xerr_set(err, -1, "wrong magic");
        fclose(fpIn);
        return -1;
    }

    if (ofilename)
        fpOut = fopen(ofilename, "w");
    else
        fpOut = stdout;

    if (fpOut == 0) {
        xerr_set(err, -1, "%s: %s", ofilename, strerror(errno));
        if (fpIn != stdin)
            fclose(fpIn);
        return -1;
    }

    crc = fgetc(fpIn);

    len = (fgetc(fpIn) & 0xFF);
    len |= (fgetc(fpIn) & 0xFF) << 8;
    len |= (fgetc(fpIn) & 0xFF) << 16;
    len |= (fgetc(fpIn) & 0xFF) << 24;
#ifdef DEBUG
    INFO("length == %u", len);
#endif

    ncodes = (fgetc(fpIn) & 0xFF);
    ncodes |= (fgetc(fpIn) & 0xFF) << 8;

    memset(codetab, 0, sizeof(codetab));

    /* read code lengths
     */
    maxlen = 0;
    for (i = 0; i < ncodes; i++) {
        codetab[i].data = fgetc(fpIn);
        codetab[i].len = fgetc(fpIn);
        if (codetab[i].len > maxlen)
            maxlen = codetab[i].len;
    }

    decodetab = (unsigned char *)malloc(1 << (maxlen + 1));
    memset(decodetab, 0, 1 << (maxlen + 1));

    /* read codetable
     */
    bitfile.fp = fpIn;
    bitfile.data = 0;
    bitfile.pos = 0;
    for (i = 0; i < ncodes; i++) {
        codetab[i].code = bitfile_nget(&bitfile, codetab[i].len);
        decodetab[(1 << codetab[i].len) + codetab[i].code] = codetab[i].data;
    }

#ifdef DEBUG
    INFO("ftell(fpIn) == %ld", ftell(fpIn));
#endif

    calc_crc = 0;
    bitfile.data = 0;
    bitfile.pos = 0;
    for (i = 0; i < len; i++) {
        unsigned int pos = bitfile_get(&bitfile);
        unsigned int bytes = 1;
        unsigned int p;

        while(1) {
            p = (1 << bytes) + pos;
            if (decodetab[p] != 0) /* can't write 0x00 !? */
                break;
            pos = (pos << 1) | bitfile_get(&bitfile);
            bytes++;
        }

#ifdef DEBUG_2
        fprintf(stdout, "code: %s -> 0x%02X (%c)\n",
                bits2str(pos, bytes), decodetab[p],
                isascii(decodetab[p]) ? decodetab[p] : '.');
#endif
        fputc(decodetab[p], fpOut);
        calc_crc ^= decodetab[p];
    }
    free(decodetab);

    if (calc_crc != crc) {
        xerr_set(err, -1, "CRC error");
        rc = -1;
    }

    if (fpIn != stdin)
        fclose(fpIn);
    if (fpOut != stdout)
        fclose(fpOut);
    return rc;
}

#ifdef TEST
int main(int argc, char **argv)
{
    if(argc < 3)
        return 1;

    return he3_decode(argv[1], argv[2]);
}
#endif

