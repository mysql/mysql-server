
#include <malloc.h>
#include <stdlib.h>

extern "C" {
  void (* ndb_new_handler)() = 0;
}

void *operator new (size_t sz)
{
  void * p = malloc (sz ? sz : 1);
  if(p)
    return p;
  if(ndb_new_handler)
    (* ndb_new_handler)();
  abort();
}

void *operator new[] (size_t sz)
{
  void * p = (void *) malloc (sz ? sz : 1);
  if(p)
    return p;
  if(ndb_new_handler)
    (* ndb_new_handler)();
  abort();
}

void operator delete (void *ptr)
{
  if (ptr)
    free(ptr);
}

void operator delete[] (void *ptr) throw ()
{
  if (ptr)
    free(ptr);
}

/**
 * GCC linking problem...
 */
#if ( __GNUC__ == 3 )
extern "C" { int __cxa_pure_virtual() {return 0;} }
#endif
