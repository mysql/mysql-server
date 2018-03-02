/*
   Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "Rope.hpp"
#include "DataBuffer.hpp"

#define JAM_FILE_ID 330

#ifdef TEST_ROPE
#define DEBUG_ROPE 1
#else
#define DEBUG_ROPE 0
#endif

const char *
ConstRope::firstSegment(Ptr<Segment> &it) const {
  it.i = head.firstItem;
  if(it.i == RNIL) return 0;
  thePool.getPtr(it);
  return (const char *) it.p->data;
}

const char *
ConstRope::nextSegment(Ptr<Segment> &it) const {
  it.i = it.p->nextPool;
  if(it.i == RNIL) return 0;
  thePool.getPtr(it);
  return (const char *) it.p->data;
}

void
ConstRope::copy(char* buf) const {
  char * ptr = buf;
  if(DEBUG_ROPE)
    ndbout_c("ConstRope::copy() head = [ %d 0x%x 0x%x ]",
	     head.used, head.firstItem, head.lastItem);
  Uint32 left = m_length;
  Ptr<Segment> it;
  const char * data = firstSegment(it);
  while(left > getSegmentSizeInBytes()){
    memcpy(buf, data, getSegmentSizeInBytes());
    data = nextSegment(it);
    left -= getSegmentSizeInBytes();
    buf += getSegmentSizeInBytes();
  }
  if(left > 0){
    memcpy(buf, data, left);
  }
  
  if(DEBUG_ROPE)
    ndbout_c("ConstRope::copy()-> %s", ptr);
}

int
ConstRope::compare(const char * str, Uint32 len) const {
  if(DEBUG_ROPE)
    ndbout_c("ConstRope[ %d  0x%x  0x%x ]::compare(%s, %d)", 
	     head.used, head.firstItem, head.lastItem, str, (int) len);
  Uint32 left = m_length > len ? len : m_length;
  Ptr<Segment> it;
  const char * data = firstSegment(it);
  while(left > getSegmentSizeInBytes()){
    int res = memcmp(str, data, getSegmentSizeInBytes());
    if(res != 0){
      if(DEBUG_ROPE)
	ndbout_c("ConstRope::compare(%s, %d, %s) -> %d", str, left, data, res);
      return res;
    }
    data = nextSegment(it);
    left -= getSegmentSizeInBytes();
    str += getSegmentSizeInBytes();
  }
  
  if(left > 0){
    int res = memcmp(str, data, left);
    if(res){
      if(DEBUG_ROPE)
	ndbout_c("ConstRope::compare(%s, %d, %s) -> %d", str, left, data, res);
      return res;
    }
  }
  if(DEBUG_ROPE)
    ndbout_c("ConstRope::compare(%s, %d) -> %d", str, (int) len, m_length > len);
  return m_length > len;
}

char *
LocalRope::firstSegment(Ptr<Segment> &it) const {
  it.i = head.firstItem;
  if(it.i == RNIL) return 0;
  thePool.getPtr(it);
  return (char *) it.p->data;
}

char *
LocalRope::nextSegment(Ptr<Segment> &it) const {
  it.i = it.p->nextPool;
  if(it.i == RNIL) return 0;
  thePool.getPtr(it);
  return (char *) it.p->data;
}

void
LocalRope::copy(char* buf) const {
  RopeHandle handle(head, m_length);
  ConstRope self(thePool, handle);
  self.copy(buf);
}

int
LocalRope::compare(const char * str, Uint32 len) const {
  RopeHandle handle(head, m_length);
  ConstRope self(thePool, handle);
  return self.compare(str, len);
}

bool
LocalRope::assign(const char * s, Uint32 len, Uint32 hash){
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::assign(%s, %d, 0x%x)", s, (int) len, hash);
  m_hash = hash;
  erase();
  if(append((const Uint32*)s, len >> 2)){
    if(len & 3){
      Uint32 buf = 0;
      const char * src = (const char*)(((Uint32*)s)+(len >> 2));
      char* dst = (char*)&buf;
      Uint32 left = len & 3;
      while(left){
	* dst ++ = * src++;
	left--;
      }
      if(!append(&buf, 1))
	return false;
    }
    m_length = len;
    if(DEBUG_ROPE)
      ndbout_c("LocalRope::assign(...) head = [ %d 0x%x 0x%x ]",
	       head.used, head.firstItem, head.lastItem);
    return true;
  }
  return false;
}

void
LocalRope::erase(){
  m_length = 0;
  release();
}

Uint32
LocalRope::hash(const char * p, Uint32 len, Uint32 starter){
  Uint32 h = starter;
  for (; len > 0; len--)
    h = (h << 5) + h + (* p++);
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::hash(%s, %d) : 0x%x -> 0x%x", p, len, starter, h);
  return h;
}

bool
ConstRope::equal(const ConstRope& r2) const
{
  if (m_length != r2.m_length)
    return false;

  if (src.m_hash != r2.src.m_hash)
    return false;

  Uint32 left = m_length;
  Ptr<Segment> s1, s2;
  const char * s1_data = firstSegment(s1);
  const char * s2_data = firstSegment(s2);
  while(left > getSegmentSizeInBytes())
  {
    int res = memcmp(s1_data, s2_data, getSegmentSizeInBytes());
    if(res != 0)
    {
      return false;
    }
    s1_data = nextSegment(s1);
    s2_data = nextSegment(s2);
    left -= getSegmentSizeInBytes();
  }
  
  if(left > 0)
  {
    int res = memcmp(s1_data, s2_data, left);
    if (res != 0)
    {
      return false;
    }
  }
  return true;
}

/* Unit test
*/

#ifdef TEST_ROPE

int main(int argc, char ** argv) {
  ndb_init();
  RopePool c_rope_pool;
  c_rope_pool.setSize(10000);

  char buffer_sml[32];
  const char * a_string = "One Two Three Four Five Six Seven Eight Nine Ten";
  RopeHandle h1, h2, h3, h4, h5, h6;
  bool ok;

  /* Create a scope for the LocalRope */
  {
    LocalRope lr1(c_rope_pool, h1);
    assert(lr1.size() == 0);
    assert(lr1.empty());
    ok = lr1.assign(a_string);
    assert(ok);
    assert(lr1.size() == strlen(a_string) + 1);
    assert(! lr1.empty());
    assert(! lr1.compare(a_string));
    printf("LocalRope lr1 size: %d\n", lr1.size());
  }
  /* When the LocalRope goes out of scope, its head is copied back into the
     RopeHandle, which can then be used to construct a ConstRope.
  */
  ConstRope cr1(c_rope_pool, h1);
  printf("ConstRope cr1 size: %d\n", cr1.size());

  /* Copy a zero-length rope */
  {
    LocalRope lr6(c_rope_pool, h6);
  }
  ConstRope cr6(c_rope_pool, h6);
  cr6.copy(buffer_sml);

  /* Assign & copy a string that is exactly the size as a rope segment */
  const char * str28 = "____V____X____V____X____VII";
  char buf28[28];
  {
    LocalRope lr5(c_rope_pool, h5);
    lr5.assign(str28);
    lr5.copy(buf28);
    memset(buf28, 0, 28);
  }
  ConstRope cr5(c_rope_pool, h5);
  cr5.copy(buf28);


//  printf("SegmentSizeInBytes %d\n", h1.m_head.getSegmentSizeInBytes());

//  /* Test buffered-style reading from ConstRope
//  */
//  Uint32 offset = 0;
//  int nread = 0;
//  printf("ConstRope cr1 nread: %d offset: %d \n", nread, offset);
//  nread = cr1.readBuffered(buffer_sml, 32, offset);
//  printf("ConstRope cr1 nread: %d offset: %d \n", nread, offset);
//  nread = cr1.readBuffered(buffer_sml, 32, offset);
//  printf("ConstRope cr1 nread: %d offset: %d \n", nread, offset);
//  /* All done: */
//  assert(offset = cr1.size());
//  /* Read once more; should return 0: */
//  nread = cr1.readBuffered(buffer_sml, 32, offset);
//  assert(nread == 0);
//
//  /* Test buffered-style writing to LocalRope
//  */
//  LocalRope lr2(c_rope_pool, h2);
//  lr2.appendBuffer(a_string, 40);
//  printf("lr2 size: %d \n", lr2.size());
//  assert(lr2.size() == 40);
//  lr2.appendBuffer(a_string, 40);
//  printf("lr2 size: %d \n", lr2.size());
//  assert(lr2.size() == 80);
//
//  /* Identical strings should have the same hash code whether they were stored
//     in one part or in two.  Here is a scope for two local ropes that should
//     end up with the same hash.
//  */
//  {
//    LocalRope lr3(c_rope_pool, h3);
//    lr3.assign(a_string, 16);
//    lr3.appendBuffer(a_string + 16, 16);
//
//    LocalRope lr4(c_rope_pool, h4);
//    lr4.assign(a_string, 32);
//  }
//  printf("Hashes:  h3=%u, h4=%u \n", h3.m_hash, h4.m_hash);
//  assert(h3.m_hash == h4.m_hash);

  ndb_end(0);
  return 0;
}

#endif
