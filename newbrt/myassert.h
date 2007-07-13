#ifndef MYASSERT_H
#define MYASSERT_H

#ifndef TESTER
#include <assert.h>
#else
extern void my_assert(int, const char *, int);
#define assert(x) my_assert(x, __FILE__, __LINE__)
#endif

#endif
