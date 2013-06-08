#ifndef _sphashd_h_
#define _sphashd_h_

#include "sphashd_cmd.h"
#include "sphashd_send.h"

struct hash_entry
{
    TAILQ_ENTRY(hash_entry) link;
    char *filename;
};

int hc_send_command(hc_t *hc, const char *fmt, ...)
    __attribute__ (( format(printf, 2, 3) ));

#endif

