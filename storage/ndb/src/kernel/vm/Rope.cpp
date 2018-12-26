/*
   Copyright (c) 2005, 2016, Oracle and/or its affiliates. All rights reserved.

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


#define DEBUG_ROPE 0

void
ConstRope::copy(char* buf) const {
  char * ptr = buf;
  if(DEBUG_ROPE)
    ndbout_c("ConstRope::copy() head = [ %d 0x%x 0x%x ]",
	     head.used, head.firstItem, head.lastItem);
  Uint32 left = head.used;
  Ptr<Segment> curr;
  curr.i = head.firstItem;
  while(left > 4 * getSegmentSize()){
    thePool.getPtr(curr);
    memcpy(buf, curr.p->data, 4 * getSegmentSize());
    curr.i = curr.p->nextPool;
    left -= 4 * getSegmentSize();
    buf += 4 * getSegmentSize();
  }
  if(left > 0){
    thePool.getPtr(curr);
    memcpy(buf, curr.p->data, left);
  }
  
  if(DEBUG_ROPE)
    ndbout_c("ConstRope::copy()-> %s", ptr);
}

int
ConstRope::compare(const char * str, Uint32 len) const {
  if(DEBUG_ROPE)
    ndbout_c("ConstRope[ %d  0x%x  0x%x ]::compare(%s, %d)", 
	     head.used, head.firstItem, head.lastItem, str, (int) len);
  Uint32 left = head.used > len ? len : head.used;
  Ptr<Segment> curr;
  curr.i = head.firstItem;
  while(left > 4 * getSegmentSize()){
    thePool.getPtr(curr);
    int res = memcmp(str, (const char*)curr.p->data, 4 * getSegmentSize());
    if(res != 0){
      if(DEBUG_ROPE)
	ndbout_c("ConstRope::compare(%s, %d, %s) -> %d", str, left,
		 (const char*)curr.p->data, res);
      return res;
    }
    curr.i = curr.p->nextPool;
    left -= 4 * getSegmentSize();
    str += 4 * getSegmentSize();
  }
  
  if(left > 0){
    thePool.getPtr(curr);
    int res = memcmp(str, (const char*)curr.p->data, left);
    if(res){
      if(DEBUG_ROPE)
	ndbout_c("ConstRope::compare(%s, %d, %s) -> %d", 
		 str, left, (const char*)curr.p->data, res);
      return res;
    }
  }
  if(DEBUG_ROPE)
    ndbout_c("ConstRope::compare(%s, %d) -> %d", str, (int) len, head.used > len);
  return head.used > len;
}

void
LocalRope::copy(char* buf) const {
  char * ptr = buf;
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::copy() head = [ %d 0x%x 0x%x ]",
	     head.used, head.firstItem, head.lastItem);
  Uint32 left = head.used;
  Ptr<Segment> curr;
  curr.i = head.firstItem;
  while(left > 4 * getSegmentSize()){
    thePool.getPtr(curr);
    memcpy(buf, curr.p->data, 4 * getSegmentSize());
    curr.i = curr.p->nextPool;
    left -= 4 * getSegmentSize();
    buf += 4 * getSegmentSize();
  }
  if(left > 0){
    thePool.getPtr(curr);
    memcpy(buf, curr.p->data, left);
  }
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::copy()-> %s", ptr);
}

int
LocalRope::compare(const char * str, Uint32 len) const {
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::compare(%s, %d)", str, (int) len);
  Uint32 left = head.used > len ? len : head.used;
  Ptr<Segment> curr;
  curr.i = head.firstItem;
  while(left > 4 * getSegmentSize()){
    thePool.getPtr(curr);
    int res = memcmp(str, (const char*)curr.p->data, 4 * getSegmentSize());
    if(res != 0){
      if(DEBUG_ROPE)
	ndbout_c("LocalRope::compare(%s, %d, %s) -> %d", str, (int) len,
		 (const char*)curr.p->data, res);
      return res;
    }
    
    curr.i = curr.p->nextPool;
    left -= 4 * getSegmentSize();
    str += 4 * getSegmentSize();
  }
  
  if(left > 0){
    thePool.getPtr(curr);
    int res = memcmp(str, (const char*)curr.p->data, left);
    if(res){
      if(DEBUG_ROPE)
	ndbout_c("LocalRope::compare(%s, %d) -> %d", str, (int) len, res);
      return res;
    }
  }
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::compare(%s, %d) -> %d", str, (int) len, head.used > len);
  return head.used > len;
}

bool
LocalRope::assign(const char * s, Uint32 len, Uint32 hash){
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::assign(%s, %d, 0x%x)", s, (int) len, hash);
  m_hash = hash;
  head.used = (head.used + 3) / 4;
  release();
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
    head.used = len;
    if(DEBUG_ROPE)
      ndbout_c("LocalRope::assign(...) head = [ %d 0x%x 0x%x ]",
	       head.used, head.firstItem, head.lastItem);
    return true;
  }
  return false;
}

void
LocalRope::erase(){
  head.used = (head.used + 3) / 4;
  release();
}

Uint32
LocalRope::hash(const char * p, Uint32 len){
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::hash(%s, %d)", p, len);
  Uint32 h = 0;
  for (; len > 0; len--)
    h = (h << 5) + h + (* p++);
  if(DEBUG_ROPE)
    ndbout_c("LocalRope::hash(...) -> 0x%x", h);
  return h;
}

bool
ConstRope::equal(const ConstRope& r2) const
{
  if (head.used != r2.head.used)
    return false;

  if (src.m_hash != r2.src.m_hash)
    return false;

  Uint32 left = head.used;
  Ptr<Segment> s1, s2;
  s1.i = head.firstItem;
  s2.i = r2.head.firstItem;
  while(left > 4 * getSegmentSize())
  {
    thePool.getPtr(s1);
    thePool.getPtr(s2);
    int res = memcmp(s1.p->data, s2.p->data, 4 * getSegmentSize());
    if(res != 0)
    {
      return false;
    }
    s1.i = s1.p->nextPool;
    s2.i = s2.p->nextPool;
    left -= 4 * getSegmentSize();
  }
  
  if(left > 0)
  {
    thePool.getPtr(s1);
    thePool.getPtr(s2);
    int res = memcmp(s1.p->data, s2.p->data, left);
    if (res != 0)
    {
      return false;
    }
  }
  return true;
}
