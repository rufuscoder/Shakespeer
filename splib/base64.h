#ifndef _base64_h_
#define _base64_h_

#include <sys/types.h>

int base64_ntop(unsigned char const *src, size_t srclength, char *target, size_t targsize);
int base64_pton(char const *src, unsigned char *target, size_t targsize);

#endif

