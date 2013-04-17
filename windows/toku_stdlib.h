#ifndef _TOKU_STDLIB_H
#define _TOKU_STDLIB_H

#include <stdlib.h>

#if defined(__cplusplus)
extern "C" {
#endif

long int random(void);
void srandom(unsigned int seed);

int unsetenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);

#if defined(__cplusplus)
};
#endif

#endif
