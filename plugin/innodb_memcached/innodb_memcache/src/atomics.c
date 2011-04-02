
#include "atomics.h"


#if defined USE_DARWIN_ATOMICS

/* atomic_cmp_swap_int() is defined as a macro.  */
/* atomic_cmp_swap_ptr() is defined as a macro.  */

#elif defined USE_SOLARIS_ATOMICS 

bool atomic_cmp_swap_int(ndbmc_atomic32_t *loc, int old, int new) {
  int stored_old;
  
  membar_enter();
  stored_old = atomic_cas_32(loc, old, new);
  membar_exit();
  return (stored_old == old);  
}

bool atomic_cmp_swap_ptr(volatile void **loc, void *old, void *new) {
  void * stored_old;
  
  membar_enter();
  stored_old = atomic_cas_ptr(loc, old, new);
  membar_exit();
  return (stored_old == old);  
}


#elif defined USE_GCC_ATOMICS

bool atomic_cmp_swap_int(ndbmc_atomic32_t *loc, int old, int new) {
  bool did_cas;
  
  did_cas = __sync_bool_compare_and_swap(loc, (uint32_t) old, (uint32_t) new);
  __sync_synchronize();       /* memory barrier */
  return did_cas;
}

bool atomic_cmp_swap_ptr(volatile void **loc, void *old, void *new) {
  bool did_cas;
  
  did_cas = __sync_bool_compare_and_swap(loc, old, new);
  __sync_synchronize();       /* memory barrier */
  return did_cas;
}
#endif
