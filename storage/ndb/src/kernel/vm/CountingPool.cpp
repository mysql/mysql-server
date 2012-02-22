#ifdef TAP_TEST

#include <ndb_global.h>
#include <NdbTap.hpp>
#include "CountingPool.hpp"
#include "Pool.hpp"
#include "RWPool.hpp"
#include "test_context.hpp"
#include "WOPool.hpp"

struct record
{
  int dummy;
};

template class CountingPool<record, RecordPool<record, RWPool> >;
template class CountingPool<record, RecordPool<record, WOPool> >;

TAPTEST(CountingPool)
{
  Pool_context pc = test_context(100);

  // Only compile test. See template instantiations above.

  OK(true);

  return 1;
}

#endif
