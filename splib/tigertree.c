/* (PD) 2001 The Bitzi Corporation
 * Please see file COPYING or http://bitzi.com/publicdomain 
 * for more info.
 *
 * tigertree.c - Implementation of the TigerTree algorithm
 *
 * NOTE: The TigerTree hash value cannot be calculated using a
 * constant amount of memory; rather, the memory required grows
 * with the size of input. (Roughly, one more interim value must
 * be remembered for each doubling of the input size.) The
 * default TT_CONTEXT struct size reserves enough memory for
 * input up to 2^64 in length
 *
 * Requires the tiger() function as defined in the reference
 * implementation provided by the creators of the Tiger
 * algorithm. See
 *
 *    http://www.cs.technion.ac.il/~biham/Reports/Tiger/
 *
 * Modified by Martin Hedenfalk <martin.hedenfalk at gmail.com> for use in ShakesPeer.
 *
 * $Id: tigertree.c,v 1.12 2006/04/09 12:54:31 mhe Exp $
 *
 */

#include "tiger.h"

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
# include <sys/types.h>
# define U_INT64_TO_LE(x) htole64((x))
#elif defined(__APPLE__)
# include <machine/byte_order.h>
# define U_INT64_TO_LE(x) NXSwapHostLongLongToLittle((x))
#elif defined(__linux__)
# include <endian.h>
# include <byteswap.h>
# if __LITTLE_ENDIAN__
#  define U_INT64_TO_LE(x) (x)
# else
#  define U_INT64_TO_LE(x) bswap_64((x))
# endif
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "tigertree.h"
#include "base32.h"
#include "base64.h"

/* Initialize the tigertree context */
void tt_init(TT_CONTEXT *ctx, unsigned int leafsize)
{
    ctx->count = 0;
    ctx->leaf[0] = 0; /* flag for leaf  calculation -- never changed */
    ctx->node[0] = 1; /* flag for inner node calculation -- never changed */
    ctx->block = ctx->leaf + 1 ; /* working area for blocks */
    ctx->index = 0;   /* partial block pointer/block length */
    ctx->top = ctx->nodes;
    ctx->leafsize = leafsize;
    ctx->leaves = NULL;
    ctx->leaves_len = 0;
    ctx->fin = 0;
}

static void tt_append_leaf(TT_CONTEXT *ctx, void *data)
{
    if(ctx->leafsize)
    {
        ctx->leaves = realloc(ctx->leaves, ctx->leaves_len + TIGERSIZE);
        memcpy(ctx->leaves + ctx->leaves_len, data, TIGERSIZE);
        ctx->leaves_len += TIGERSIZE;
    }
}

void tt_compose(TT_CONTEXT *ctx)
{
    u_int8_t *node = ctx->top - XNODESIZE;
    assert(node >= ctx->nodes);

    u_int32_t *bsp = (u_int32_t *)(node + TIGERSIZE);
    u_int32_t *bsp2 = (u_int32_t *)(node + XTIGERSIZE + TIGERSIZE);

    if(*bsp == ctx->leafsize)
    {
        tt_append_leaf(ctx, node);
        tt_append_leaf(ctx, node + XTIGERSIZE);
    }
    else if(ctx->fin && *bsp2 <= ctx->leafsize && *bsp > ctx->leafsize)
    {
        tt_append_leaf(ctx, node + XTIGERSIZE);
    }

    memmove(ctx->node + 1, node, TIGERSIZE); /* copy to scratch area */
    memmove(ctx->node + 1 + TIGERSIZE, node + XTIGERSIZE, TIGERSIZE); /* copy to scratch area */

    tiger((u_int64_t *)(ctx->node), (u_int64_t)(NODESIZE + 1), (u_int64_t *)(ctx->top)); /* combine two nodes */
    ((u_int64_t *)node)[0] = U_INT64_TO_LE(((u_int64_t *)ctx->top)[0]);           /* move up result */
    ((u_int64_t *)node)[1] = U_INT64_TO_LE(((u_int64_t *)ctx->top)[1]);
    ((u_int64_t *)node)[2] = U_INT64_TO_LE(((u_int64_t *)ctx->top)[2]);

    *bsp *= 2;
    *bsp2 *= 2;

    /* assert(*bsp == *bsp2); */
    ctx->top -= XTIGERSIZE;                      /* update top ptr */
}

static void tt_block(TT_CONTEXT *ctx)
{
    u_int64_t b;

    tiger((u_int64_t *)ctx->leaf, (u_int64_t)ctx->index + 1, (u_int64_t *)ctx->top);
    ((u_int64_t *)ctx->top)[0] = U_INT64_TO_LE(((u_int64_t *)ctx->top)[0]);
    ((u_int64_t *)ctx->top)[1] = U_INT64_TO_LE(((u_int64_t *)ctx->top)[1]);
    ((u_int64_t *)ctx->top)[2] = U_INT64_TO_LE(((u_int64_t *)ctx->top)[2]);
    u_int32_t *bsp = (u_int32_t *)(ctx->top + TIGERSIZE);
    *bsp = BLOCKSIZE;
    ctx->top += XTIGERSIZE;
    ++ctx->count;
    b = ctx->count;
    while((b & 0x01) == 0) /* while evenly divisible by 2... */
    {
        tt_compose(ctx);
        b = b >> 1;
    }
}

void tt_update(TT_CONTEXT *ctx, u_int8_t *buffer, u_int32_t len)
{
    assert(ctx->index <= BLOCKSIZE);
    if(ctx->index)
    { /* Try to fill partial block */
        unsigned left = BLOCKSIZE - ctx->index;
        if(len < left)
        {
            memmove(ctx->block + ctx->index, buffer, len);
            ctx->index += len;
            return; /* Finished */
        }
        else
        {
            memmove(ctx->block + ctx->index, buffer, left);
            ctx->index = BLOCKSIZE;
            tt_block(ctx);
            buffer += left;
            len -= left;
        }
    }

    while(len >= BLOCKSIZE)
    {
        memmove(ctx->block, buffer, BLOCKSIZE);
        ctx->index = BLOCKSIZE;
        tt_block(ctx);
        buffer += BLOCKSIZE;
        len -= BLOCKSIZE;
    }
    if((ctx->index = len))     /* This assignment is intended */
    {
        /* Buffer leftovers */
        memmove(ctx->block, buffer, len);
    }
}

/* no need to call this directly; tt_digest calls it for you */
static void tt_final(TT_CONTEXT *ctx)
{
     /* do last partial block, unless index is 1 (empty leaf) */
     /* AND we're past the first block */
    if((ctx->index > 0) || (ctx->top == ctx->nodes))
    {
        tt_block(ctx);
    }
}

void tt_digest(TT_CONTEXT *ctx, u_int8_t *s)
{
    assert(ctx->index <= BLOCKSIZE);
    tt_final(ctx);

    ctx->fin = 1;
    while((ctx->top - XTIGERSIZE) > ctx->nodes)
    {
        tt_compose(ctx);
    }
    if(ctx->leaves_len == 0)
        tt_append_leaf(ctx, ctx->nodes);
    if(s)
        memmove(s, ctx->nodes, TIGERSIZE);
}

void tt_destroy(TT_CONTEXT *ctx)
{
    if(ctx && ctx->leaves)
    {
        free(ctx->leaves);
        ctx->leaves = 0;
    }
}

/* returned string should be free'd by the caller
 * should only be called after tt_digest
 */
char *tt_base32(TT_CONTEXT *ctx)
{
    return base32_encode((char *)ctx->nodes, TIGERSIZE);
}

char *tt_leafdata_base32(TT_CONTEXT *ctx)
{
    if(ctx->leaves_len)
        return base32_encode((char *)ctx->leaves, ctx->leaves_len);
    return NULL;
}

char *tt_leafdata_base64(TT_CONTEXT *ctx)
{
    if(ctx->leaves_len)
    {
        int enclen = ctx->leaves_len * 2 + 4;
        char *leafdata_base64 = (char *)malloc(enclen);
        assert(base64_ntop((unsigned char *)ctx->leaves, ctx->leaves_len,
                    leafdata_base64, enclen) > 0);
        return leafdata_base64;
    }
    return NULL;
}

u_int64_t tt_calc_block_size(u_int64_t filesize, unsigned max_levels)
{
    u_int64_t tmp = BLOCKSIZE;
    u_int64_t maxHashes = 1ULL << (max_levels - 1);
    while((maxHashes * tmp) < filesize)
        tmp *= 2;
    if(tmp < 64*1024)
        tmp = 64*1024;
    return tmp;
}

