#include "Rope.hpp"

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
ConstRope::compare(const char * str, size_t len) const {
  if(DEBUG_ROPE)
    ndbout_c("ConstRope[ %d 0x%x 0x%x ]::compare(%s, %d)", 
	     head.used, head.firstItem, head.lastItem, str, len);
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
    ndbout_c("ConstRope::compare(%s, %d) -> %d", str, len, head.used > len);
  return head.used > len;
}

void
Rope::copy(char* buf) const {
  char * ptr = buf;
  if(DEBUG_ROPE)
    ndbout_c("Rope::copy() head = [ %d 0x%x 0x%x ]",
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
    ndbout_c("Rope::copy()-> %s", ptr);
}

int
Rope::compare(const char * str, size_t len) const {
  if(DEBUG_ROPE)
    ndbout_c("Rope::compare(%s, %d)", str, len);
  Uint32 left = head.used > len ? len : head.used;
  Ptr<Segment> curr;
  curr.i = head.firstItem;
  while(left > 4 * getSegmentSize()){
    thePool.getPtr(curr);
    int res = memcmp(str, (const char*)curr.p->data, 4 * getSegmentSize());
    if(res != 0){
      if(DEBUG_ROPE)
	ndbout_c("Rope::compare(%s, %d, %s) -> %d", str, len, 
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
	ndbout_c("Rope::compare(%s, %d) -> %d", str, len, res);
      return res;
    }
  }
  if(DEBUG_ROPE)
    ndbout_c("Rope::compare(%s, %d) -> %d", str, len, head.used > len);
  return head.used > len;
}

bool
Rope::assign(const char * s, size_t len, Uint32 hash){
  if(DEBUG_ROPE)
    ndbout_c("Rope::assign(%s, %d, 0x%x)", s, len, hash);
  m_hash = hash;
  head.used = (head.used + 3) / 4;
  release();
  if(append((const Uint32*)s, len >> 2)){
    if(len & 3){
      Uint32 buf = 0;
      const char * src = (const char*)(((Uint32*)s)+(len >> 2));
      char* dst = (char*)&buf;
      size_t left = len & 3;
      while(left){
	* dst ++ = * src++;
	left--;
      }
      if(!append(&buf, 1))
	return false;
    }
    head.used = len;
    if(DEBUG_ROPE)
      ndbout_c("Rope::assign(...) head = [ %d 0x%x 0x%x ]",
	       head.used, head.firstItem, head.lastItem);
    return true;
  }
  return false;
}

void
Rope::erase(){
  head.used = (head.used + 3) / 4;
  release();
}

Uint32
Rope::hash(const char * p, Uint32 len){
  if(DEBUG_ROPE)
    ndbout_c("Rope::hash(%s, %d)", p, len);
  Uint32 h = 0;
  for (; len > 0; len--)
    h = (h << 5) + h + (* p++);
  if(DEBUG_ROPE)
    ndbout_c("Rope::hash(...) -> 0x%x", h);
  return h;
}

