#ifdef TAP_TEST

#include "CountingPool.hpp"
#include "NdbTap.hpp"
#include "PackPool.hpp"
#include "PagePool.hpp"
#include "Pool.hpp"
#include "RWPool.hpp"
#include "test_context.hpp"
#include "WOPool.hpp"

struct record
{
  int dummy;
};

template class CountingPool<record, PackPool<record> >;
template class CountingPool<record, RecordPool<record, RWPool> >;
template class CountingPool<record, RecordPool<record, WOPool> >;
template class CountingPool<PackablePage, PackablePagePool>;

TAPTEST(CountingPool)
{
  Pool_context pc = test_context(10000);

  // Only compile test. See template instantiations above.

  OK(true);

  return 1;
}

#endif
