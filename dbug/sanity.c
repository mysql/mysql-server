/* Declarate _sanity() if not declared in main program */

#include <global.h>

extern int _sanity(const char *file,uint line);

#if defined(SAFEMALLOC) && !defined(MASTER)	/* Avoid errors in MySQL */
int _sanity(const char * file __attribute__((unused)),
            uint line __attribute__((unused)))
{
  return 0;
}
#endif
