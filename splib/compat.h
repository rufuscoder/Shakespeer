#ifndef _compat_h_
#define _compat_h_

#if defined(MISSING_FGETLN)
char *fgetln(FILE *fp, size_t *len);
#endif

#endif

