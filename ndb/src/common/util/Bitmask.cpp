#include <Bitmask.hpp>
#include <NdbOut.hpp>

#ifndef __TEST_BITMASK__
void
BitmaskImpl::getFieldImpl(const Uint32 data[],
			  unsigned pos, unsigned l, Uint32 dst[])
{
  Uint32 word;
  Uint32 offset;
  int next_offset,i;
  int len= l;
  for(i=0,next_offset=0;len >0;i++)
  {
    word = pos >> 5;
    offset = pos & 31;

    if(i%32==0)
      dst[i/32]=0;

    if(!next_offset && (offset+len) > 32)
    {
      dst[i/32] = (data[word] >> offset) & ((1 << (32-offset)) - 1);
      next_offset = 32-offset;
    }
    else
    {
      dst[i/32]|= ((data[word] >> offset) & ((1 << len) - 1)) << next_offset;
      next_offset = 0;
    }
    
    if(len < 32-offset)
      break;
    
    len-=32-offset;
    pos+=32-offset;
  }
}
 
void
BitmaskImpl::setFieldImpl(Uint32 data[],
			  unsigned pos, unsigned l, const Uint32 src[])
{
  Uint32 word;
  Uint32 offset;
  Uint32 mask;
  int i=0,stored=0;
  int len= l;

  while(len>0)
  {
    ndbout_c("len: %d", len);
    word = pos >> 5;
    offset = pos & 31;
    
    if(offset+len > 32)
      stored = 32-offset;
    else
      stored = len;
    
    mask = ((1 << stored) - 1) << (i+offset)%32;
    data[word] = (data[word] & ~mask) | ((src[i/32] << (i+offset)%32) & mask);
    
    i+=stored;
    len-=32-offset;
    pos+=32-offset;
  }
}

#else

#define DEBUG 0
#include <Vector.hpp>
void do_test(int bitmask_size);

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

void require(bool b)
{
  if(!b) abort();
}

void 
do_test(int bitmask_size)
{
  Vector<Alloc> alloc_list;
  bitmask_size = (bitmask_size + 31) & ~31;
  Uint32 sz32 = (bitmask_size >> 5);
  Vector<Uint32> alloc_mask;
  Vector<Uint32> test_mask;
  Vector<Uint32> tmp;
  
  ndbout_c("Testing bitmask of size %d", bitmask_size);
  Uint32 zero = 0;
  alloc_mask.fill(sz32, zero);
  test_mask.fill(sz32, zero);
  tmp.fill(sz32, zero);
  
  for(int i = 0; i<1000; i++)
  {
    int pos = rand() % (bitmask_size - 1);
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
      if(DEBUG)
	ndbout_c("freeing [ %d %d ]", min, max);

      require(pos >= min && pos < max);
      BitmaskImpl::getField(sz32, test_mask.getBase(), min, max-min, 
			    tmp.getBase());
      if(memcmp(tmp.getBase(), alloc_list[j].data.getBase(),
		((max - min) + 31) >> 5) != 0)
      {
	printf("mask: ");
	size_t k;
	Alloc& a = alloc_list[j];
	for(k = 0; k<a.data.size(); k++)
	  printf("%.8x ", a.data[k]);
	printf("\n");

	printf("field: ");
	for(k = 0; k<(((max - min)+31)>>5); k++)
	  printf("%.8x ", tmp.getBase()[k]);
	printf("\n");
	abort();
      }
      while(min < max)
	BitmaskImpl::clear(sz32, alloc_mask.getBase(), min++);
      alloc_list.erase(j);
    }
    else
    {
      // Bit was free
      // 1) Check how much space is avaiable
      // 2) Create new allocation of random size
      // 3) Fill data with random data
      // 4) Update alloc mask
      while(pos+free < bitmask_size && 
	    !BitmaskImpl::get(sz32, alloc_mask.getBase(), pos+free))
	free++;

      Uint32 sz = (rand() % free); sz = sz ? sz : 1;
      Alloc a;
      a.pos = pos;
      a.size = sz;
      a.data.fill(sz >> 5, zero);
      if(DEBUG)
	ndbout_c("pos %d -> alloc [ %d %d ]", pos, pos, pos+sz);
      for(size_t j = 0; j<sz; j++)
      {
	BitmaskImpl::set(sz32, alloc_mask.getBase(), pos+j);
	if((rand() % 1000) > 500)
	  BitmaskImpl::set((sz + 31) >> 5, a.data.getBase(), j);
      }
      if(DEBUG)
      {
	printf("mask: ");
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
}

template class Vector<Alloc>;
template class Vector<Uint32>;

#endif
