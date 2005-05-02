#include <Bitmask.hpp>
#include <NdbOut.hpp>

static
void print(const Uint32 src[], Uint32 len, Uint32 pos = 0)
{
  printf("b'");
  for(unsigned i = 0; i<len; i++)
  {
    if(BitmaskImpl::get((pos + len + 31) >> 5, src, i+pos))
      printf("1");
    else
      printf("0");
    if((i & 31) == 31)
      printf(" ");
  }
}

#ifndef __TEST_BITMASK__

void
BitmaskImpl::getFieldImpl(const Uint32 src[],
			  unsigned shiftL, unsigned len, Uint32 dst[])
{
  assert(shiftL < 32);

  unsigned shiftR = 32 - shiftL;
  unsigned undefined = shiftL ? ~0 : 0;

  * dst = shiftL ? * dst : 0;
  
  while(len >= 32)
  {
    * dst++ |= (* src) << shiftL;
    * dst = ((* src++) >> shiftR) & undefined;
    len -= 32;
  }
  
  if(len < shiftR)
  {
    * dst |= ((* src) & ((1 << len) - 1)) << shiftL;
  }
  else
  {
    * dst++ |= ((* src) << shiftL);
    * dst = ((* src) >> shiftR) & ((1 << (len - shiftR)) - 1) & undefined;
  }
}

void
BitmaskImpl::setFieldImpl(Uint32 dst[],
			  unsigned shiftL, unsigned len, const Uint32 src[])
{
  /**
   *
   * abcd ef00
   * 00ab cdef
   */
  assert(shiftL < 32);
  unsigned shiftR = 32 - shiftL;
  unsigned undefined = shiftL ? ~0 : 0;  
  while(len >= 32)
  {
    * dst = (* src++) >> shiftL;
    * dst++ |= ((* src) << shiftR) & undefined;
    len -= 32;
  }
  
  Uint32 mask = ((1 << len) -1);
  * dst = (* dst & ~mask);
  if(len < shiftR)
  {
    * dst |= ((* src++) >> shiftL) & mask;
  }
  else
  {
    * dst |= ((* src++) >> shiftL);
    * dst |= ((* src) & ((1 << (len - shiftR)) - 1)) << shiftR ;
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

static
bool cmp(const Uint32 b1[], const Uint32 b2[], Uint32 len)
{
  Uint32 sz32 = (len + 31) >> 5;
  for(int i = 0; i<len; i++)
  {
    if(BitmaskImpl::get(sz32, b1, i) ^ BitmaskImpl::get(sz32, b2, i))
      return false;
  }
  return true;
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
    BitmaskImpl::set((len + 31) >> 5, dst, i, (lrand() % 1000) > 500);
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
  _mask.fill(sz32+1, zero);
  _src.fill(sz32+1, zero);
  _dst.fill(sz32+1, zero);

  Uint32 * src = _src.getBase();
  Uint32 * dst = _dst.getBase();
  Uint32 * mask = _mask.getBase();

  memset(src, 0x0, sz);
  memset(dst, 0x0, sz);
  memset(mask, 0xFF, sz);
  rand(src, size);
  BitmaskImpl::setField(sz32, mask, pos, size, src);
  BitmaskImpl::getField(sz32, mask, pos, size, dst);
  printf("src: "); print(src, size+31); printf("\n");
  printf("msk: "); print(mask, (sz32 << 5) + 31); printf("\n");
  printf("dst: "); print(dst, size+31); printf("\n");
  require(cmp(src, dst, size+31));
};

static
void simple2(int size, int loops)
{
  ndbout_c("simple2 %d - ", size);
  Vector<Uint32> _mask;
  Vector<Uint32> _src;
  Vector<Uint32> _dst;

  Uint32 sz32 = (size + 32) >> 5;
  Uint32 sz = sz32 << 2;
  
  Uint32 zero = 0;
  _mask.fill(sz32+1, zero);
  _src.fill(sz32+1, zero);
  _dst.fill(sz32+1, zero);

  Uint32 * src = _src.getBase();
  Uint32 * dst = _dst.getBase();
  Uint32 * mask = _mask.getBase();

  Vector<Uint32> save;
  for(int i = 0; i<loops; i++)
  {
    memset(mask, 0xFF, sz);
    memset(dst, 0xFF, sz);
    int len;
    int pos = 0;
    while(pos+1 < size)
    {
      memset(src, 0xFF, sz);
      while(!(len = rand() % (size - pos)));
      BitmaskImpl::setField(sz32, mask, pos, len, src);
      if(memcmp(dst, mask, sz))
      {
	ndbout_c("pos: %d len: %d", pos, len);
	print(mask, size);
	abort();
      }
      printf("[ %d %d ]", pos, len);
      save.push_back(pos);
      save.push_back(len);
      pos += len;
    }

    for(int j = 0; j<save.size(); )
    {
      pos = save[j++];
      len = save[j++];
      memset(src, 0xFF, sz);
      BitmaskImpl::getField(sz32, mask, pos, len, src);
      if(memcmp(dst, src, sz))
      {
	ndbout_c("pos: %d len: %d", pos, len);
	printf("src: "); print(src, size); printf("\n");
	printf("dst: "); print(dst, size); printf("\n");
	printf("msk: "); print(mask, size); printf("\n");
	abort();
      }
    }
    ndbout_c("");
  }
}

static void 
do_test(int bitmask_size)
{
#if 1
  simple(rand() % 33, (rand() % 63)+1);
//#else
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
	print(tmp.getBase(), max - min);
	
	printf(" save: ");
	size_t k;
	Alloc& a = alloc_list[j];
	for(k = 0; k<a.data.size(); k++)
	  printf("%.8x ", a.data[k]);
	printf("\n");
      }
      int bytes = (max - min + 7) >> 3;
      if(!cmp(tmp.getBase(), alloc_list[j].data.getBase(), max - min))
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

      Uint32 sz = 
	(free <= 64 && ((lrand() % 100) > 80)) ? free : (lrand() % free); 
      sz = sz ? sz : 1;
      sz = pos + sz == bitmask_size ? sz - 1 : sz;
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
	print(a.data.getBase(), sz);
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
