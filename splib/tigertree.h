/* (PD) 2001 The Bitzi Corporation
 * Please see file COPYING or http://bitzi.com/publicdomain 
 * for more info.
 *
 * $Id: tigertree.h,v 1.9 2006/03/20 16:56:42 mhe Exp $
 */

#ifndef _tigertree_h_
#define _tigertree_h_

#include "tiger.h"

/* tiger hash result size, in bytes */
#define TIGERSIZE 24
#define XTIGERSIZE (24+sizeof(uint32_t))

/* size of each block independently tiger-hashed, not counting leaf 0x00 prefix */
#define BLOCKSIZE 1024

/* size of input to each non-leaf hash-tree node, not counting node 0x01 prefix */
#define NODESIZE (TIGERSIZE*2)
#define XNODESIZE (XTIGERSIZE*2)

/* default size of interim values stack, in TIGERSIZE
 * blocks. If this overflows (as it will for input
 * longer than 2^64 in size), havoc may ensue. */
#define TIGER_STACKSIZE XTIGERSIZE*56

typedef struct tt_context TT_CONTEXT;
struct tt_context {
  uint64_t count;                   /* total blocks processed */
  unsigned char leaf[1+BLOCKSIZE]; /* leaf in progress */
  unsigned char *block;            /* leaf data */
  unsigned char node[1+NODESIZE]; /* node scratch space */
  unsigned int index;                      /* index into block */
  unsigned char *top;             /* top (next empty) stack slot */
  unsigned char nodes[TIGER_STACKSIZE]; /* stack of interim node values */
  unsigned leafsize;
  void *leaves;
  unsigned leaves_len;
  int fin;
};

void tt_init(TT_CONTEXT *ctx, unsigned int leafsize);
void tt_update(TT_CONTEXT *ctx, unsigned char *buffer, unsigned len);
void tt_digest(TT_CONTEXT *ctx, unsigned char *hash);
char *tt_base32(TT_CONTEXT *ctx);
char *tt_leafdata_base32(TT_CONTEXT *ctx);
char *tt_leafdata_base64(TT_CONTEXT *ctx);
void tt_destroy(TT_CONTEXT *ctx);
uint64_t tt_calc_block_size(uint64_t filesize, unsigned max_levels);

#endif

