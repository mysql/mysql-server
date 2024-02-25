/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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


#include "util/require.h"
#include <ndb_global.h>
#include <NdbTick.h>
#include <EventLogger.hpp>
#include <DataBuffer.hpp>
#include <ArrayPool.hpp>

#define JAM_FILE_ID 290


#undef test

struct Buffer {
  Buffer(Uint32 size){ m_sz = size; buffer = new Uint32[m_sz]; m_len = 0;}
  ~Buffer(){ delete [] buffer;}
  
  Uint32 m_sz;
  Uint32 m_len;
  Uint32 * buffer;
};


template<Uint32 sz>
void
compare(DataBuffer<sz,ArrayPool<DataBufferSegment<sz>>> & db, Buffer & buf){
  typename DataBuffer<sz,ArrayPool<DataBufferSegment<sz>>>::DataBufferIterator it;
  require(buf.m_len <= db.getSize());
  db.first(it);
  for(Uint32 i = 0; i<buf.m_len; i++){
    if(buf.buffer[i] != * it.data){
      db.print(stdout);
      abort();
    }
    db.next(it);
  }

  for(Uint32 i = 0; i<buf.m_len; i++){
    if(!db.position(it, i))
      abort();
    if(buf.buffer[i] != * it.data){
      db.print(stdout);
      abort();
    }
  }
}

template<Uint32 sz>
void
test(Uint32 loops, Uint32 iter){

  ndbout_c("DataBuffer<%d> loops=%d iter=%d", sz, loops, iter);

  while(loops-- > 0){
    Uint32 size = sz*((10 + (rand() % (10 * sz)) + sz - 1)/sz);

    typename DataBuffer<sz,ArrayPool<DataBufferSegment<sz>>>::DataBufferPool thePool;
    typename DataBuffer<sz,ArrayPool<DataBufferSegment<sz>>>::DataBufferIterator it;
    DataBuffer<sz,ArrayPool<DataBufferSegment<sz>>> db(thePool);

    thePool.setSize((size + sz - 1) / sz);
    Buffer buf(size);

    bool testOverRun = true;

    for(Uint32 i = 0; i<iter; i++){
      Uint32 c = (rand() % (testOverRun ? 7 : 4));
      Uint32 free = (size - db.getSize());
      Uint32 alloc = 0;
      if(free == 0){
        c = (testOverRun ? c : 0);
        if(c >= 1 && c <= 3)
          c += 3;
      }

      if(free <= 1)
        alloc = 1;
      else
        alloc = 1 + (rand() % (free - 1));

      //ndbout_c("iter=%d case=%d free=%d alloc=%d", i, c, free, alloc);
      switch(c){
        case 0: // Release
          db.first(it);
          for(; !it.curr.isNull(); db.next(it))
            * it.data = 0;

          db.release();
          require(db.getSize() == 0);
          buf.m_len = 0;
          break;
        case 1:{ // Append (success)
          for(Uint32 i = 0; i<alloc; i++)
            buf.buffer[buf.m_len + i] = buf.m_len + i;//rand();

          require(db.append(&buf.buffer[buf.m_len], alloc));
          buf.m_len += alloc;
          require(buf.m_len == db.getSize());
          break;
        }
        case 2: { // Seize(1) (success)
          for(Uint32 i = 0; i<alloc; i++){
            buf.buffer[buf.m_len + i] = buf.m_len + i;//rand();
            require(db.seize(1));
            require(db.position(it, db.getSize()-1));
            * it.data = buf.buffer[buf.m_len + i];
          }
          buf.m_len += alloc;
          require(buf.m_len == db.getSize());
          break;
        }
        case 3: { // Seize(n) (success)
          for(Uint32 i = 0; i<alloc; i++){
            buf.buffer[buf.m_len + i] = buf.m_len + i;//rand();
          }
          Uint32 pos = db.getSize();
          require(db.seize(alloc));
          require(db.position(it, pos));
          for(Uint32 i = 0; i<alloc; i++){
            * it.data = buf.buffer[buf.m_len + i];
            db.next(it);
          }
          buf.m_len += alloc;
          require(buf.m_len == db.getSize());
          break;
        }
        case 4: { // Append fail
          Uint32 dbSize = db.getSize();
          require(!db.append(buf.buffer, alloc + free));
          require(db.getSize() == dbSize);
          break;
        }
        case 5: { // Seize(1) - fail
          for(Uint32 i = 0; i<free; i++){
            require(db.seize(1));
          }
          Uint32 dbSize = db.getSize();
          require(!db.seize(1));
          require(db.getSize() == dbSize);
          break;
        }
        case 6: { // Seize(n) - fail
          Uint32 dbSize = db.getSize();
          require(!db.seize(alloc + free));
          require(db.getSize() == dbSize);
          break;
        }
      }
      compare(db, buf);
    }
  }
}

int
main(void){
  ndb_init();
  srand(NdbTick_CurrentMillisecond());
 
  test<1>(1000, 1000);
  test<11>(1000, 1000);
  test<15>(1000, 1000);
  test<16>(1000, 1000);
  test<17>(1000, 1000);
#if 0
#endif
  ndb_end(0);
  return 0;
}

