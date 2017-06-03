#ifndef __COMMON_H_
#define __COMMON_H_

#include "list.h"

int asprintf(char **strp, const char *fmt, ...);
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

#endif