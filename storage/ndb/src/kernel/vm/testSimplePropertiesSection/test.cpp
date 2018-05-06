/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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
#include <NdbTick.h>
#include <TransporterDefinitions.hpp>
#include <SimpleProperties.hpp>
#include <LongSignal.hpp>

#define JAM_FILE_ID 225


#undef test

struct Buffer {
  Buffer(Uint32 size){ m_sz = size; buffer = new Uint32[m_sz]; m_len = 0;}
  ~Buffer(){ delete [] buffer;}
  
  Uint32 m_sz;
  Uint32 m_len;
  Uint32 * buffer;
};

inline
void 
require(bool b){
  if(!b)
    abort();
}

/* Calculate number of segments to release based on section size
 * Always release one segment, even if size is zero
 */
#define relSz(x) ((x == 0)? 1 : ((x + SectionSegment::DataLength - 1) / SectionSegment::DataLength))

void
release(SectionSegmentPool & thePool, SegmentedSectionPtr & ptr){
  const Uint32 sz = relSz(ptr.sz);
  thePool.releaseList(sz,
		      ptr.i, 
		      ptr.p->m_lastSegment);
}

void
compare(SimplePropertiesSectionReader & db, Buffer & buf){

  {
    bool fail = false;
    db.reset();
    for(Uint32 i = 0; i<buf.m_len; i++){
      Uint32 tmp;
      if(!db.getWord(&tmp)){
	ndbout_c("getWord(...) failed i=%d size=%d", i, buf.m_len);
	abort();
      }
      
      if(tmp != buf.buffer[i]){
	ndbout_c("getWord(...)=%d != buf[%d]=%d size=%d", tmp, i, 
		 buf.buffer[i], buf.m_len);
	fail = true;
      }
    }
    require(!fail);
  }

  {
    db.reset();
    Buffer buf2(buf.m_sz);
    if(!db.getWords(buf2.buffer, buf.m_len))
      abort();
    
    bool fail = false;
    for(Uint32 i = 0; i<buf.m_len; i++){
      if(buf.buffer[i] != buf2.buffer[i]){
	ndbout_c("getWords(...) buf[%d] != buf2[%d] size=%d", i, i, buf.m_len);
	fail = true;
      }
    }
    require(!fail);
  }
}


void
test(Uint32 sz, Uint32 loops, Uint32 iter){

  ndbout_c("SimplePropertiesSection sz=%d loops=%d iter=%d", sz, loops, iter);
  
  while(loops-- > 0){
    Uint32 size = sz*((10 + (rand() % (10 * sz)) + sz - 1)/sz);
    
    Buffer buf(size);
    SectionSegmentPool thePool; thePool.setSize(size);

    for(Uint32 i = 0; i<iter; i++){
      Uint32 c = 0 + (rand() % (2));
      
      const Uint32 alloc = 1 + (rand() % (size - 1));
      SegmentedSectionPtr dst;

      if(0)
	ndbout_c("size: %d loops: %d iter: %d c=%d alloc=%d", 
		 size, loops, i, c, alloc);
      
      switch(c){ 
      case 0:{
	for(Uint32 i = 0; i<alloc; i++)
	  buf.buffer[i] = i; //rand();
	buf.m_len = alloc;
	
	SimplePropertiesSectionWriter w(thePool);
	for(Uint32 i = 0; i<alloc; i++){
	  w.putWord(buf.buffer[i]);
	}
	w.getPtr(dst);
	break;
      }
      case 1:{
	for(Uint32 i = 0; i<alloc; i++)
	  buf.buffer[i] = i; //rand();
	buf.m_len = alloc;
	
	SimplePropertiesSectionWriter w(thePool);
	Uint32 i = 0;
	while(i < alloc){
	  Uint32 sz = rand() % (alloc - i + 1);
	  w.putWords(&buf.buffer[i], sz);
	  i += sz;
	}
	w.getPtr(dst);
	break;
      }
      case 2:{
	break;
      }
      }
      SimplePropertiesSectionReader r(dst, thePool);
      compare(r, buf);
      release(thePool, dst);
      require(thePool.getSize() == thePool.getNoOfFree());
    }
  }
}

int
main(void){
  
  srand(NdbTick_CurrentMillisecond());

  //test( 1, 1000, 1000);
  test(54, 1000, 1000);
  test(59, 1000, 1000);
  test(60, 1000, 1000);
  test(61, 1000, 1000);
  return 0;
}

void
ErrorReporter::handleAssert(const char * msg, const char * file, int line)
{
  ndbout << "ErrorReporter::handleAssert activated - " 
	 << " line= " << line << endl;
  abort();
}
