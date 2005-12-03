#include <ndb_global.h>
#include "tuppage.hpp"
#include <Vector.hpp>

struct Record
{
  Uint32 idx;
  Uint32 size;
  Uint32* data;
};

#define TRACE(x) x

static
void
cmp(const Uint32 *p1, const Uint32 *p2, Uint32 words)
{
  if(memcmp(p1, p2, 4*words) == 0)
    return;
  
  for(Uint32 i = 0; i<words; i++)
    printf(" %.8x", p1[i]);
  printf("\n");

  for(Uint32 i = 0; i<words; i++)
    printf(" %.8x", p2[i]);
  printf("\n");

  abort();
}

static
void
do_test(int loops, int dist[3])
{
  int allocated= 0;
  Record records[8192];

  Tup_varsize_page page, tmp;
  page.init();

  for(int i = 0; i<loops; i++)
  {
    for(int j = 0; j<allocated; j++)
    {
      Record rec= records[j];
      Uint32* ptr= page.get_ptr(rec.idx);
      cmp(ptr, rec.data, rec.size);
    }
    
loop:
    int op;
    int rnd= rand() % 100;
    for(op= 0; op<3; op++)
      if(rnd < dist[op])
	break;
    
    if(allocated == 0)
      op= 0;
    if(page.free_space <= 2 && op == 0) goto loop;
    
    switch(op){
    case 0: // Alloc
    {
      Record rec;
      rec.size= 1 + (rand() % (page.free_space-1));
      rec.data = new Uint32[rec.size];
      for(Uint32 i= 0; i<rec.size; i++)
      {
	rec.data[i] = rand();
      }
      ndbout << "Alloc " << rec.size << flush;
      rec.idx= page.alloc_record(rec.size, &tmp, 0);
      ndbout << " -> " << rec.idx << endl;
      Uint32* ptr= page.get_ptr(rec.idx);
      memcpy(ptr, rec.data, 4*rec.size);
      records[allocated++] = rec;
      break;
    }
    case 1: // Free
    {
      int no= rand() % allocated;
      Record rec= records[no];
      ndbout << "Free no: " << no << " idx: " << rec.idx << endl;
      Uint32* ptr= page.get_ptr(rec.idx);
      cmp(ptr, rec.data, rec.size);
      delete[] rec.data;
      page.free_record(rec.idx, 0);

      for (unsigned k = no; k + 1 < allocated; k++)
	records[k] = records[k+1];
      allocated--;
      
      break;
    }
    case 2: // Reorg
      ndbout << "Reorg" << endl;      
      page.reorg(&tmp);
      break;
    case 3: 
      ndbout << "Expand" << endl;
      
    }

  }
  ndbout << page << endl;
}

int
main(void)
{
  ndb_init();
  
  int t1[] = { 30, 90, 100 };
  int t2[] = { 45, 90, 100 };
  int t3[] = { 60, 90, 100 };
  int t4[] = { 75, 90, 100 };
  
  do_test(10000, t1);
  do_test(10000, t2);
  do_test(10000, t3);
  do_test(10000, t4);
}

template class Vector<Record>;

// hp3750
struct Signal { Signal(); int foo; };
Signal::Signal(){}
