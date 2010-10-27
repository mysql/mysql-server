#include <assert.h>
#include <ndb_types.h>

void
add_thr_map(Uint32, Uint32, Uint32)
{
  assert(false);
}

void
add_main_thr_map()
{
  assert(false);
}

void
add_lqh_worker_thr_map(Uint32, Uint32)
{
  assert(false);
}

void
add_extra_worker_thr_map(Uint32, Uint32)
{
  assert(false);
}

Uint32
compute_jb_pages(struct EmulatorData*)
{
  return 0;
}
