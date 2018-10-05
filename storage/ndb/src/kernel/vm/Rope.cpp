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
#undef ROPE_COPY_BUFFER_SIZE
#define ROPE_COPY_BUFFER_SIZE 24
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

/* Returns number of bytes read, or 0 at EOF */
int
ConstRope::readBuffered(char* buf, Uint32 bufSize,
                        Uint32 & rope_offset) const {
   if(DEBUG_ROPE)
    ndbout_c("ConstRope::readBuffered(sz=%u,offset=%u) head = [ %d 0x%x 0x%x ]",
            bufSize, rope_offset, head.used, head.firstItem, head.lastItem);

  Uint32 offset = rope_offset;
  require(m_length >= offset);
  Uint32 bytesLeft = m_length - offset;
  require((bytesLeft == 0) || (rope_offset % 4 == 0));
  Uint32 bytesWritten = 0;

  /* Skip forward */
  Ptr<Segment> it;
  const char * data = firstSegment(it);
  while(offset > getSegmentSizeInBytes()) {
    data = nextSegment(it);
    offset -= getSegmentSizeInBytes();
  }

  /* Read */
  while(bytesLeft > 0 && bytesWritten < bufSize) {
    Uint32 readBytes = getSegmentSizeInBytes() - offset;
    if(readBytes > bytesLeft) readBytes = bytesLeft;
    if(readBytes + bytesWritten > bufSize) readBytes = bufSize - bytesWritten;

    memcpy(buf + bytesWritten, data + offset, readBytes);

    bytesLeft -= readBytes;
    bytesWritten += readBytes;
    offset = 0;
    data = nextSegment(it);
  }

  rope_offset += bytesWritten;
  return (int) bytesWritten;
}

void
ConstRope::copy(char* buf) const {
  /* Assume that buffer is big enough */
  Uint32 offset = 0;
  readBuffered(buf, m_length, offset);
  if(DEBUG_ROPE)
    ndbout_c("ConstRope::copy()-> %s", buf);
}

bool ConstRope::copy(LocalRope & dest) {
  const int bufsize = ROPE_COPY_BUFFER_SIZE;
  char buffer[bufsize];
  int nread;
  Uint32 offset = 0;
  dest.erase();
  while((nread = readBuffered(buffer, bufsize, offset)) > 0)
    if(! dest.appendBuffer(buffer, nread))
      return false;
  return true;
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

bool packFinalWord(const char * src, Uint32 & dest, Uint32 len) {
  dest = 0;
  Uint32 left = len % 4;
  if(left) {
    src += len-left;
    char* dst = (char*)&dest;
    while(left--)
      * dst++ = * src++;
    return true;
  }
  return false;
}

bool
LocalRope::appendBuffer(const char * s, Uint32 len) {
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::appendBuffer(%d)", (int) len);
  bool ok = append((const Uint32*) s, len >> 2);
  if(ok) {
    Uint32 tail;
    if(packFinalWord(s, tail, len))
      ok = append(&tail, 1);
    m_length += len;
    m_hash = hash(s, len, m_hash);
  }
  return ok;
}

bool
LocalRope::assign(const char * s, Uint32 len, Uint32 hash){
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::assign(%s, %d, 0x%x)", s, (int) len, hash);
  erase();
  m_hash = hash;
  bool ok = append((const Uint32*) s, len >> 2);
  if(ok) {
    Uint32 tail;
    if(packFinalWord(s, tail, len))
      ok = append(&tail, 1);
    m_length = len;
  }
  return ok;
}

void
LocalRope::erase(){
  m_length = 0;
  m_hash = 0;
  release();
}

Uint32
LocalRope::hash(const char * p, Uint32 len, Uint32 starter){
  Uint32 h = starter;
  const char * data = p;
  for (; len > 0; len--)
    h = (h << 5) + h + (* data++);
  if(DEBUG_ROPE) {
    char msg_buffer[ROPE_COPY_BUFFER_SIZE];
    strncpy(msg_buffer, p, sizeof(msg_buffer));
    msg_buffer[sizeof(msg_buffer)-1] = '\0';
    ndbout_c("LocalRope::hash(%s, %d) : 0x%x -> 0x%x",
             msg_buffer, len, starter, h);
  }
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
  {
    ConstRope cr6(c_rope_pool, h6);
    cr6.copy(buffer_sml);
  }

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

  /* Test buffered-style reading from ConstRope
  */
  assert(! cr1.compare(a_string));
  Uint32 offset = 0;
  int nread = 0;
  printf(" --> START readBuffered TEST <-- \n");
  printf("ConstRope cr1 nread: %d offset: %d \n", nread, offset);
  nread = cr1.readBuffered(buffer_sml, 32, offset);
  printf("ConstRope cr1 nread: %d offset: %d \n", nread, offset);
  assert(! strncmp(a_string, buffer_sml, nread));
  nread = cr1.readBuffered(buffer_sml, 32, offset);
  printf("ConstRope cr1 nread: %d offset: %d \n", nread, offset);
  assert(! strncmp(a_string + offset - nread, buffer_sml, nread));
  /* All done: */
  assert(offset = cr1.size());
  /* Read once more; should return 0: */
  nread = cr1.readBuffered(buffer_sml, 32, offset);
  assert(nread == 0);
  printf(" --> END readBuffered TEST <-- \n");

  /* Test buffered-style writing to LocalRope
  */
  printf(" --> START appendBuffer TEST <-- \n");
  {
    LocalRope lr2(c_rope_pool, h2);
    lr2.appendBuffer(a_string, 40);
    printf("lr2 size: %d \n", lr2.size());
    assert(lr2.size() == 40);
    lr2.appendBuffer(a_string, 40);
    printf("lr2 size: %d \n", lr2.size());
    assert(lr2.size() == 80);
  }
  /* Identical strings should have the same hash code whether they were stored
     in one part or in two.  Here is a scope for two local ropes that should
     end up with the same hash.
  */
  {
    ndbout_c("Hash test h3:");
    LocalRope lr3(c_rope_pool, h3);
    lr3.assign(a_string, 16);
    lr3.appendBuffer(a_string + 16, 16);

    ndbout_c("Hash test h4:");
    LocalRope lr4(c_rope_pool, h4);
    lr4.assign(a_string, 32);
  }
  printf("Hashes:  h3=%u, h4=%u \n", h3.hashValue(), h4.hashValue());
  assert(h3.hashValue() == h4.hashValue());
  printf(" --> END appendBuffer TEST <-- \n");

  /* Test ConstRope::copy(LocalRope &)
  */
  ndbout_c(" --> START ConstRope::copy() TEST <--");
  ConstRope cr2(c_rope_pool, h2);
  printf("cr2 size: %d \n", cr2.size());
  assert(cr2.size() == 80);
  {
    LocalRope lr3(c_rope_pool, h3);
    cr2.copy(lr3);
    printf("lr3 size: %d \n", lr3.size());
    assert(lr3.size() == 80);
  }
  ConstRope cr3(c_rope_pool, h3);
  assert(cr3.size() == 80);
  assert(h2.hashValue() == h3.hashValue());
  assert(cr2.equal(cr3));
  ndbout_c(" --> END ConstRope::copy() TEST <--");

  /* Test that RopeHandles can be assigned */
  h6 = h3;
  assert(h6.hashValue() == h3.hashValue());
  ConstRope cr6(c_rope_pool, h6);
  assert(cr6.size() == cr3.size());
  assert(cr3.equal(cr6));

  ndb_end(0);
  return 0;
}

#endif
