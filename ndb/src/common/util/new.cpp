
#include <ndb_global.h>

extern "C" {
  void (* ndb_new_handler)() = 0;
}

#ifdef USE_MYSYS_NEW

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

#endif // USE_MYSYS_NEW
