#include <Bitmask.hpp>
#include <NdbOut.hpp>

static
void print(const Uint32 src[], Uint32 len, Uint32 pos = 0)
{
  printf("b'");
  for(int i = 0; i<len; i++)
  {
    if(BitmaskImpl::get((pos + len + 31) >> 5, src, i+pos))
      printf("1");
    else
      printf("0");
    if((i & 7) == 7)
      printf(" ");
  }
}

#ifndef __TEST_BITMASK__
void
BitmaskImpl::getFieldImpl(const Uint32 src[],
			  unsigned shift, unsigned len, Uint32 dst[])
{
  assert(shift < 32);

  if(len <= (32 - shift))
  {
    * dst++ |= ((* src) & ((1 << len) - 1)) << shift;
  }
  else
  {
    abort();
    while(len > 32)
    {
      * dst++ |= (* src) << shift;
      * dst = (* src++) >> (32 - shift);
      len -= 32;
    }
  }
}

void
BitmaskImpl::setFieldImpl(Uint32 dst[],
			  unsigned shift, unsigned len, const Uint32 src[])
{
  assert(shift < 32);
  
  Uint32 mask;
  if(len < (32 - shift))
  {
    mask = (1 << len) - 1;
    * dst = (* dst & ~mask) | (((* src++) >> shift) & mask);
  } 
  else 
  {
    abort();
  }
}

#else

#define DEBUG 0
#include <Vector.hpp>
static void do_test(int bitmask_size);

int
main(int argc, char** argv)
{
  int loops = argc > 1 ? atoi(argv[1]) : 1000;
  int max_size = argc > 2 ? atoi(argv[2]) : 1000;

  
  for(int i = 0; i<loops; i++)
    do_test(1 + (rand() % max_size));
}

struct Alloc
{
  Uint32 pos;
  Uint32 size;
  Vector<Uint32> data;
};

static void require(bool b)
{
  if(!b) abort();
}

static int val_pos = 0;
static int val[] = { 384, 241, 32, 
		     1,1,1,1, 0,0,0,0, 1,1,1,1, 0,0,0,0,
		     241 };

static int lrand()
{
#if 0
  return val[val_pos++];
#else
  return rand();
#endif
}

static
void rand(Uint32 dst[], Uint32 len)
{
  for(int i = 0; i<len; i++)
    BitmaskImpl::set((len + 31) >> 5, dst, i,  (lrand() % 1000) > 500);
}

static
void simple(int pos, int size)
{
  ndbout_c("simple pos: %d size: %d", pos, size);
  Vector<Uint32> _mask;
  Vector<Uint32> _src;
  Vector<Uint32> _dst;
  Uint32 sz32 = (size + pos + 32) >> 5;
  const Uint32 sz = 4 * sz32;
  
  Uint32 zero = 0;
  _mask.fill(sz32, zero);
  _src.fill(sz32, zero);
  _dst.fill(sz32, zero);

  Uint32 * src = _src.getBase();
  Uint32 * dst = _dst.getBase();
  Uint32 * mask = _mask.getBase();

  memset(src, 0x0, sz);
  memset(dst, 0x0, sz);
  memset(mask, 0x0, sz);
  rand(src, size);
  BitmaskImpl::setField(sz32, mask, pos, size, src);
  BitmaskImpl::getField(sz32, mask, pos, size, dst);
  printf("src: "); print(src, size); printf("\n");
  printf("msk: "); print(mask, size, pos); printf("\n");
  printf("dst: "); print(dst, size); printf("\n");
  require(memcmp(src, dst, sz) == 0);
};

static void 
do_test(int bitmask_size)
{
#if 0
  simple(rand() % 33, (rand() % 31)+1);
#else
  Vector<Alloc> alloc_list;
  bitmask_size = (bitmask_size + 31) & ~31;
  Uint32 sz32 = (bitmask_size >> 5);
  Vector<Uint32> alloc_mask;
  Vector<Uint32> test_mask;
  
  ndbout_c("Testing bitmask of size %d", bitmask_size);
  Uint32 zero = 0;
  alloc_mask.fill(sz32, zero);
  test_mask.fill(sz32, zero);
  
  for(int i = 0; i<5000; i++)
  {
    Vector<Uint32> tmp;
    tmp.fill(sz32, zero);

    int pos = lrand() % (bitmask_size - 1);
    int free = 0;
    if(BitmaskImpl::get(sz32, alloc_mask.getBase(), pos))
    {
      // Bit was allocated
      // 1) Look up allocation
      // 2) Check data
      // 3) free it
      size_t j;
      int min, max;
      for(j = 0; j<alloc_list.size(); j++)
      {
	min = alloc_list[j].pos;
	max = min + alloc_list[j].size;
	if(pos >= min && pos < max)
	{
	  break;
	}
      }
      require(pos >= min && pos < max);
      BitmaskImpl::getField(sz32, test_mask.getBase(), min, max-min, 
			    tmp.getBase());
      if(DEBUG)
      {
	printf("freeing [ %d %d ]", min, max);
	printf("- mask: ");
	for(size_t k = 0; k<(((max - min)+31)>>5); k++)
	  printf("%.8x ", tmp.getBase()[k]);

	printf("save: ");
	size_t k;
	Alloc& a = alloc_list[j];
	for(k = 0; k<a.data.size(); k++)
	  printf("%.8x ", a.data[k]);
	printf("\n");
      }
      int bytes = (max - min + 7) >> 3;
      if(memcmp(tmp.getBase(), alloc_list[j].data.getBase(), bytes) != 0)
      {
	abort();
      }
      while(min < max)
	BitmaskImpl::clear(sz32, alloc_mask.getBase(), min++);
      alloc_list.erase(j);
    }
    else
    {
      Vector<Uint32> tmp;
      tmp.fill(sz32, zero);
      
      // Bit was free
      // 1) Check how much space is avaiable
      // 2) Create new allocation of lrandom size
      // 3) Fill data with lrandom data
      // 4) Update alloc mask
      while(pos+free < bitmask_size && 
	    !BitmaskImpl::get(sz32, alloc_mask.getBase(), pos+free))
	free++;

      Uint32 sz = (lrand() % free); 
      sz = sz ? sz : 1;
      sz = (sz > 31) ? 31 : sz;
      Alloc a;
      a.pos = pos;
      a.size = sz;
      a.data.fill(((sz+31)>> 5)-1, zero);
      if(DEBUG)
	printf("pos %d -> alloc [ %d %d ]", pos, pos, pos+sz);
      for(size_t j = 0; j<sz; j++)
      {
	BitmaskImpl::set(sz32, alloc_mask.getBase(), pos+j);
	if((lrand() % 1000) > 500)
	  BitmaskImpl::set((sz + 31) >> 5, a.data.getBase(), j);
      }
      if(DEBUG)
      {
	printf("- mask: ");
	size_t k;
	for(k = 0; k<a.data.size(); k++)
	  printf("%.8x ", a.data[k]);
	printf("\n");
      }
      BitmaskImpl::setField(sz32, test_mask.getBase(), pos, sz, 
			    a.data.getBase());
      alloc_list.push_back(a);
    }
  }
#endif
}

template class Vector<Alloc>;
template class Vector<Uint32>;

#endif
