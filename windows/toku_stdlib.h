/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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
