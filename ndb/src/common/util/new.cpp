
#include <ndb_global.h>
#include <NdbMem.h>

extern "C" {
  void (* ndb_new_handler)() = 0;
}

#ifdef USE_MYSYS_NEW

void *operator new (size_t sz)
{
  void * p = NdbMem_Allocate(sz ? sz : 1);
  if(p)
    return p;
  if(ndb_new_handler)
    (* ndb_new_handler)();
  abort();
}

void *operator new[] (size_t sz)
{
  void * p = (void *) NdbMem_Allocate(sz ? sz : 1);
  if(p)
    return p;
  if(ndb_new_handler)
    (* ndb_new_handler)();
  abort();
}

void operator delete (void *ptr)
{
  if (ptr)
    NdbMem_Free(ptr);
}

void operator delete[] (void *ptr) throw ()
{
  if (ptr)
    NdbMem_Free(ptr);
}

#endif // USE_MYSYS_NEW
