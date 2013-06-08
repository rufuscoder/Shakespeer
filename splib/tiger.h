#ifndef _tiger_h_
#define _tiger_h_

#include <stdint.h>

typedef uint64_t word64;
typedef uint32_t word32;
typedef uint8_t byte;

void tiger(word64 *str, word64 length, word64 res[3]);

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
# if linux
#  include <endian.h>
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#   define __LITTLE_ENDIAN__ 1
#  else
#   define __BIG_ENDIAN__ 1
#  endif
# else
#  include <machine/endian.h>
#  if _BYTE_ORDER == _LITTLE_ENDIAN
#   define __LITTLE_ENDIAN__ 1
#  else
#   define __BIG_ENDIAN__ 1
#  endif
# endif
#endif

#endif

