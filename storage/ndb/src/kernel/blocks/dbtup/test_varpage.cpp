/*
   Copyright (c) 2005, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <time.h>

#include "tuppage.hpp"
#include <Vector.hpp>

#define JAM_FILE_ID 428


struct Record
{
  Uint32 idx;
  Uint32 size;
  Uint32* data;
};

NdbOut&
operator <<(NdbOut& out, const Record& rec)
{
  out << "[ idx: " << rec.idx << " sz: " << rec.size << " ]";
  return out;
}

#define TRACE(x) x

static
bool
cmp(const Uint32 *p1, const Uint32 *p2, Uint32 words)
{
  if(memcmp(p1, p2, 4*words) == 0)
    return true;
  
  for(Uint32 i = 0; i<words; i++)
    printf(" %.8x", p1[i]);
  printf("\n");

  for(Uint32 i = 0; i<words; i++)
    printf(" %.8x", p2[i]);
  printf("\n");

  return false;
}

static
void
do_test(int loops, int dist[5])
{
  fprintf(stderr, "do_test(%d, [ %d %d %d %d %d ])\n",
	  loops, 
	  dist[0],
	  dist[1],
	  dist[2],
	  dist[3],
	  dist[4]);
  int allocated= 0;
  Record records[8192];

  Tup_varsize_page page, tmp;
  page.init();

  for(int i = 0; i<loops; i++)
  {
    assert(page.high_index + page.insert_pos <= page.DATA_WORDS);

    for(int j = 0; j<allocated; j++)
    {
      Record rec= records[j];
      Uint32* ptr= page.get_ptr(rec.idx);
      Uint32 pos = page.get_ptr(rec.idx) - page.m_data;
      if (page.get_entry_len(rec.idx) != rec.size)
      {
	ndbout << "INVALID LEN " << j << " " << rec << " pos: " << pos << endl;
	ndbout << page << endl;
	abort();
      }
      
      if(!cmp(ptr, rec.data, rec.size))
      {
	ndbout << "FAILED " << j << " " << rec << " pos: " << pos << endl;
	ndbout << page << endl;
	abort();
      }
    }
    
loop:
    int op;
    int rnd= rand() % 100;
    for(op= 0; op<5; op++)
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
      ndbout << "Alloc hi: " << page.high_index << " (" << 
	((rnd < 30) ? "any" : 
	 (rnd < 60) ? "dir" : 
	 (rnd < 80) ? "exp" : "fail") << ") ";
      ndbout << rec.size << flush;
      if (rnd < 30)
      {
	rec.idx= page.alloc_record(rec.size, &tmp, 0);
      }
      else if (rnd < 60)
      {
        // Alloc with id, from directory
	Vector<Uint32> free;
	for(Uint32 i = page.high_index - 1; i > 0; i--)
	{
	  if (page.get_index_word(i) & page.FREE)
	  {
	    free.push_back(i);
	    if (free.size() > 100)
	      break;
	  }
	}
	if (free.size())
	{
	  rec.idx = free[rand() % free.size()];
	  if (page.alloc_record(rec.idx, rec.size, &tmp) != rec.idx)
	  {
	    abort();
	  }
	}
	else
	{
	  rec.idx = page.high_index;
	  if (page.alloc_record(rec.idx, rec.size, &tmp) != rec.idx)
	  {
	    if (rec.size + 1 != page.free_space)
	      abort();
	    delete [] rec.data;
	    ndbout_c(" FAIL");
	    break;
	  }
	}
      }
      else if(rnd < 80)
      {
        // Alloc with id, outside of directory
	rec.idx = page.high_index + (rand() % (page.free_space - rec.size));
	if (page.alloc_record(rec.idx, rec.size, &tmp) != rec.idx)
	{
	  abort();
	}
      }
      else
      {
	rec.idx = page.high_index + (page.free_space - rec.size) + 1;
	if (page.alloc_record(rec.idx, rec.size, &tmp) == rec.idx)
	{
	  abort();
	}
	delete [] rec.data;
	ndbout_c(" FAIL");
	break;
      }
      
      Uint32 pos = page.get_ptr(rec.idx) - page.m_data;
      ndbout << " -> " << rec.idx 
	     << " pos: " << pos << endl;
      Uint32* ptr= page.get_ptr(rec.idx);
      memcpy(ptr, rec.data, 4*rec.size);
      records[allocated++] = rec;
      break;
    }
    case 1: // Free
    {
      int no= rand() % allocated;
      Record rec= records[no];
      Uint32 pos = page.get_ptr(rec.idx) - page.m_data;
      ndbout << "Free hi: " << page.high_index << " no: " << no << " idx: " << rec.idx << " pos: " << pos << endl;
      Uint32* ptr= page.get_ptr(rec.idx);
      assert(page.get_entry_len(rec.idx) == rec.size);
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
    {
      Uint32 free = page.free_space;
      if (free <= 2)
      {
	goto shrink;
      }
      free /= 2;
      int  no = rand() % allocated;
      Record rec= records[no];
      ndbout << "Expand no: " << no << " idx: " << rec.idx
	     << " add: " << free << " reorg: " 
	     << !page.is_space_behind_entry(rec.idx, free)
	     << endl;
      if (!page.is_space_behind_entry(rec.idx, free))
      {
	Uint32 buffer[8192];
	Uint32 len = page.get_entry_len(rec.idx);
	memcpy(buffer, page.get_ptr(rec.idx), 4*len);
	page.set_entry_len(rec.idx, 0);
	page.free_space += len;
	page.reorg(&tmp);
	memcpy(page.get_free_space_ptr(), buffer, 4*len);
	page.set_entry_offset(rec.idx, page.insert_pos);
	free += len;
	records[no].size = 0;
      }

      page.grow_entry(rec.idx, free);
      records[no].size += free;
      Uint32 *ptr = page.get_ptr(rec.idx);
      Uint32 *new_data = new Uint32[records[no].size];
      for(Uint32 i= 0; i<records[no].size; i++)
      {
	ptr[i] = new_data[i] = rand();
      }
      delete []rec.data;
      records[no].data = new_data;
      break;
    }
    case 4:
    {
  shrink:
      int no = rand() % allocated;
      Record rec = records[no];
      Uint32 sz = rec.size / 2 + 1;
      ndbout << "Shrink no: " << no << " idx: " << rec.idx << " remove: " 
	     <<  (rec.size - sz) << endl;
      page.shrink_entry(rec.idx, sz);
      records[no].size = sz;
      break;
    }
    }

  }
  ndbout << page << endl;
}

int
main(int argc, char **argv)
{
  ndb_init();

  if (argc > 1)
  {
    time_t seed = time(0);
    srand(seed);
    fprintf(stderr, "srand(%d)\n", seed);
  }
  // alloc, free, reorg, grow, shrink
  
  int t1[] = { 10, 60, 70, 85, 100 };
  int t2[] = { 30, 60, 70, 85, 100 };
  int t3[] = { 50, 60, 70, 85, 100 };
  
  do_test(10000, t1);
  do_test(10000, t2);
  do_test(10000, t3);

  return 0;
}

template class Vector<Record>;

// hp3750
struct Signal { Signal(); int foo; };
Signal::Signal(){}
