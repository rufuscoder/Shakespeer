#ifndef _sphashd_client_h
#define _sphashd_client_h

#include "sphashd_client_cmd.h"
#include "sphashd_client_send.h"

int hs_start(void);
int hs_send_string(hs_t *hs, const char *string);
int hs_send_command(hs_t *hs, const char *fmt, ...)
    __attribute__ (( format(printf, 2, 3) ));
void hs_start_hash_feeder(void);
void hs_shutdown(void);
int hs_stop(void);
void hs_set_prio(unsigned int prio);
void hs_pause(void);
void hs_resume(void);

#endif

