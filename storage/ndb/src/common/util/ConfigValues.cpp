/*
   Copyright (C) 2004-2007 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <ConfigValues.hpp>
#include <NdbOut.hpp>
#include <NdbTCP.h>

static bool findKey(const Uint32 * vals, Uint32 sz, Uint32 key, Uint32 * pos);

/**
 * Key
 *
 * t = Type      -  4 bits 0-15
 * s = Section   - 14 bits 0-16383
 * k = Key value - 14 bits 0-16383
 *
 *           1111111111222222222233
 * 01234567890123456789012345678901
 * kkkkkkkkkkkkkkssssssssssssssoooo
 */
#define KP_TYPE_MASK     (15)
#define KP_TYPE_SHIFT    (28)
#define KP_SECTION_MASK  (0x3FFF)
#define KP_SECTION_SHIFT (14)
#define KP_KEYVAL_MASK   (0x3FFF)
#define KP_KEYVAL_SHIFT  (0)
#define KP_MASK          (0x0FFFFFFF)

static const Uint32 CFV_KEY_PARENT = (KP_KEYVAL_MASK - 1);
static const Uint32 CFV_KEY_FREE   = ~0;

static const char Magic[] = { 'N', 'D', 'B', 'C', 'O', 'N', 'F', 'V' };

//#define DEBUG_CV
#ifdef DEBUG_CV
#define DEBUG if(getenv("CV_DEBUG"))
#else
#define DEBUG if(0)
#endif

inline
ConfigValues::ValueType
getTypeOf(Uint32 k) {
  return (ConfigValues::ValueType)((k >> KP_TYPE_SHIFT) & KP_TYPE_MASK);
}

ConfigValues::ConfigValues(Uint32 sz, Uint32 dsz){
  m_size = sz;
  m_dataSize = dsz;
  m_stringCount = 0;
  m_int64Count = 0;
  for(Uint32 i = 0; i<m_size; i++){
    m_values[i << 1] = CFV_KEY_FREE;
  }
}

ConfigValues::~ConfigValues(){
  for(Uint32 i = 0; i<m_stringCount; i++){
    free(* getString(i));
  }
}

bool
ConfigValues::ConstIterator::get(Uint32 key, Entry * result) const {
  Uint32 pos;
  if(!findKey(m_cfg.m_values, m_cfg.m_size, key | m_currentSection, &pos)){
    return false;
  }
  
  result->m_key = key;
  return m_cfg.getByPos(pos, result);
}

bool
ConfigValues::getByPos(Uint32 pos, Entry * result) const {
  assert(pos < (2 * m_size));
  Uint32 keypart = m_values[pos];
  Uint32 val2 = m_values[pos+1];

  switch(::getTypeOf(keypart)){
  case IntType:
  case SectionType:
    result->m_int = val2;
    break;
  case StringType:
    result->m_string = * getString(val2);
    break;
  case Int64Type:
    result->m_int64 = * get64(val2);
    break;
  case InvalidType: 
  default:
    return false;
  }

  result->m_type = ::getTypeOf(keypart);
  
  return true;
}

Uint64 *
ConfigValues::get64(Uint32 index) const {
  assert(index < m_int64Count);
  const Uint32 * data = m_values + (m_size << 1);
  Uint64 * ptr = (Uint64*)data;
  ptr += index;
  return ptr;
}

char **
ConfigValues::getString(Uint32 index) const {
  assert(index < m_stringCount); 
  const Uint32 * data = m_values + (m_size << 1);
  char * ptr = (char*)data;
  ptr += m_dataSize; 
  ptr -= (index * sizeof(char *));
  return (char**)ptr;
}

bool
ConfigValues::ConstIterator::openSection(Uint32 key, Uint32 no){
  Uint32 curr = m_currentSection;
  
  Entry tmp;
  if(get(key, &tmp) && tmp.m_type == SectionType){
    m_currentSection = tmp.m_int;
    if(get(no, &tmp) && tmp.m_type == IntType){
      m_currentSection = tmp.m_int;
      /**
       * Validate
       */
      if(get(CFV_KEY_PARENT, &tmp)){
	return true;
      }
    }
  }
  
  m_currentSection = curr;
  return false;
}

bool
ConfigValues::ConstIterator::closeSection() {
  
  Entry tmp;
  if(get(CFV_KEY_PARENT, &tmp) && tmp.m_type == IntType){
    m_currentSection = tmp.m_int;
    return true;
  }

  return false;
}

bool
ConfigValues::Iterator::set(Uint32 key, Uint32 value){
  Uint32 pos;
  if(!findKey(m_cfg.m_values, m_cfg.m_size, key | m_currentSection, &pos)){
    return false;
  }

  if(::getTypeOf(m_cfg.m_values[pos]) != IntType){
    return false;
  }

  m_cfg.m_values[pos+1] = value;
  return true;
}

bool
ConfigValues::Iterator::set(Uint32 key, Uint64 value){
  Uint32 pos;
  if(!findKey(m_cfg.m_values, m_cfg.m_size, key | m_currentSection, &pos)){
    return false;
  }

  if(::getTypeOf(m_cfg.m_values[pos]) != Int64Type){
    return false;
  }
  
  * m_cfg.get64(m_cfg.m_values[pos+1]) = value;
  return true;
}

bool
ConfigValues::Iterator::set(Uint32 key, const char * value){
  Uint32 pos;
  if(!findKey(m_cfg.m_values, m_cfg.m_size, key | m_currentSection, &pos)){
    return false;
  }

  if(::getTypeOf(m_cfg.m_values[pos]) != StringType){
    return false;
  }

  char **  str = m_cfg.getString(m_cfg.m_values[pos+1]);
  free(* str);
  * str = strdup(value ? value : "");
  return true;
}

static
bool
findKey(const Uint32 * values, Uint32 sz, Uint32 key, Uint32 * _pos){
  Uint32 lo = 0;
  Uint32 hi = sz;
  Uint32 pos = (hi + lo) >> 1;

  DEBUG printf("findKey(H'%.8x %d)", key, sz);

  if (sz == 0)
  {
    DEBUG ndbout_c(" -> false, 0");
    * _pos = 0;
    return false;
  }

  Uint32 val = 0;
  Uint32 oldpos = pos + 1;
  while (pos != oldpos) 
  {
    DEBUG printf(" [ %d %d %d ] ", lo, pos, hi);
    assert(pos < hi);
    assert(pos >= lo);
    val = values[2*pos] & KP_MASK;
    if (key > val)
    {
      lo = pos;
    }
    else if (key < val)
    {
      hi = pos;
    }
    else 
    {
      * _pos = 2*pos;
      DEBUG ndbout_c(" -> true, %d", pos);
      return true;
    }
    oldpos = pos;
    pos = (hi + lo) >> 1;
  } 

  DEBUG printf(" pos: %d (key %.8x val: %.8x values[pos]: %x) key>val: %d ",
	       pos, key, val, values[2*pos] & KP_MASK,
	       key > val);

  pos += (key > val) ? 1 : 0;
  
  * _pos = 2*pos;
  DEBUG ndbout_c(" -> false, %d", pos);
  return false;
}


ConfigValuesFactory::ConfigValuesFactory(Uint32 keys, Uint32 data){
  m_sectionCounter = (1 << KP_SECTION_SHIFT);
  m_freeKeys = keys;
  m_freeData = (data + 7) & ~7;
  m_currentSection = 0;
  m_cfg = create(m_freeKeys, m_freeData);
}

ConfigValuesFactory::ConfigValuesFactory(ConfigValues * cfg){
  m_cfg = cfg;
  m_freeKeys = 0;
  m_freeData = m_cfg->m_dataSize;
  m_sectionCounter = (1 << KP_SECTION_SHIFT);  
  m_currentSection = 0;
  const Uint32 sz = 2 * m_cfg->m_size;
  for(Uint32 i = 0; i<sz; i += 2){
    const Uint32 key = m_cfg->m_values[i];
    if(key == CFV_KEY_FREE){
      m_freeKeys++;
    } else {
      switch(::getTypeOf(key)){
      case ConfigValues::IntType:
      case ConfigValues::SectionType:
	break;
      case ConfigValues::Int64Type:
	m_freeData -= sizeof(Uint64);
	break;
      case ConfigValues::StringType:
	m_freeData -= sizeof(char *);
	break;
      case ConfigValues::InvalidType:
	abort();
      }
      Uint32 sec = key & (KP_SECTION_MASK << KP_SECTION_SHIFT);
      m_sectionCounter = (sec > m_sectionCounter ? sec : m_sectionCounter);
    }
  }
}

ConfigValuesFactory::~ConfigValuesFactory()
{
  if(m_cfg)
  {
    m_cfg->~ConfigValues();
    free(m_cfg);
  }
}

ConfigValues *
ConfigValuesFactory::create(Uint32 keys, Uint32 data){
  Uint32 sz = sizeof(ConfigValues);
  sz += (2 * keys * sizeof(Uint32)); 
  sz += data;
  
  void * tmp = malloc(sz);
  return new (tmp) ConfigValues(keys, data);
}

void
ConfigValuesFactory::expand(Uint32 fk, Uint32 fs){
  if(m_freeKeys >= fk && m_freeData >= fs){
    return ;
  }

  DEBUG printf("[ fk fd ] : [ %d %d ]", m_freeKeys, m_freeData);
  
  m_freeKeys = (m_freeKeys >= fk ? m_cfg->m_size : fk + m_cfg->m_size);
  m_freeData = (m_freeData >= fs ? m_cfg->m_dataSize : fs + m_cfg->m_dataSize);
  m_freeData = (m_freeData + 7) & ~7;
  
  DEBUG ndbout_c(" [ %d %d ]", m_freeKeys, m_freeData);

  ConfigValues * m_tmp = m_cfg;
  m_cfg = create(m_freeKeys, m_freeData);
  put(* m_tmp);
  m_tmp->~ConfigValues();
  free(m_tmp);
}

void
ConfigValuesFactory::shrink(){
  if(m_freeKeys == 0 && m_freeData == 0){
    return ;
  }

  m_freeKeys = m_cfg->m_size - m_freeKeys;
  m_freeData = m_cfg->m_dataSize - m_freeData;
  m_freeData = (m_freeData + 7) & ~7;

  ConfigValues * m_tmp = m_cfg;
  m_cfg = create(m_freeKeys, m_freeData);
  put(* m_tmp);
  m_tmp->~ConfigValues();
  free(m_tmp);
}

bool
ConfigValuesFactory::openSection(Uint32 key, Uint32 no){
  ConfigValues::Entry tmp;
  const Uint32 parent = m_currentSection;

  ConfigValues::ConstIterator iter(* m_cfg);
  iter.m_currentSection = m_currentSection;
  if(!iter.get(key, &tmp)){

    tmp.m_key  = key;
    tmp.m_type = ConfigValues::SectionType;
    tmp.m_int  = m_sectionCounter;
    m_sectionCounter += (1 << KP_SECTION_SHIFT);

    if(!put(tmp)){
      return false;
    }
  }

  if(tmp.m_type != ConfigValues::SectionType){
    return false;
  }

  m_currentSection = tmp.m_int;

  tmp.m_key = no;
  tmp.m_type = ConfigValues::IntType;
  tmp.m_int = m_sectionCounter;
  if(!put(tmp)){
    m_currentSection = parent;
    return false;
  }
  m_sectionCounter += (1 << KP_SECTION_SHIFT);
  
  m_currentSection = tmp.m_int;
  tmp.m_type = ConfigValues::IntType;
  tmp.m_key = CFV_KEY_PARENT;
  tmp.m_int = parent;
  if(!put(tmp)){
    m_currentSection = parent;
    return false;
  }

  return true;
}

bool
ConfigValuesFactory::closeSection(){
  ConfigValues::ConstIterator iter(* m_cfg);
  iter.m_currentSection = m_currentSection;
  const bool b = iter.closeSection();
  m_currentSection = iter.m_currentSection;
  return b;
}
  
bool
ConfigValuesFactory::put(const ConfigValues::Entry & entry){
  
  if(m_freeKeys == 0 ||
     (entry.m_type == ConfigValues::StringType && m_freeData < sizeof(char *))
     || (entry.m_type == ConfigValues::Int64Type && m_freeData < 8 )){ 
    
    DEBUG ndbout_c("m_freeKeys = %d, m_freeData = %d -> expand",
		   m_freeKeys, m_freeData);
    
    expand(31, 20);
  }
  
  const Uint32 tmp = entry.m_key | m_currentSection;
  const Uint32 sz = m_cfg->m_size - m_freeKeys;

  Uint32 pos;
  if (findKey(m_cfg->m_values, sz, tmp, &pos))
  {
    DEBUG ndbout_c("key %x already found at pos: %d", tmp, pos);
    return false;
  }

  DEBUG {
    printf("H'before ");
    Uint32 prev = 0;
    for (Uint32 i = 0; i<sz; i++)
    {
      Uint32 val = m_cfg->m_values[2*i] & KP_MASK;
      ndbout_c("%.8x", val);
      assert(val >= prev);
      prev = val;
    }
  }
  
  if (pos != 2*sz)
  {
    DEBUG ndbout_c("pos: %d sz: %d", pos, sz);
    memmove(m_cfg->m_values + pos + 2, m_cfg->m_values + pos, 
	    4 * (2*sz - pos));
  }


  Uint32 key = tmp;
  key |= (entry.m_type << KP_TYPE_SHIFT);
  m_cfg->m_values[pos] = key;

  DEBUG {
    printf("H'after ");
    Uint32 prev = 0;
    for (Uint32 i = 0; i<=sz; i++)
    {
      Uint32 val = m_cfg->m_values[2*i] & KP_MASK;
      ndbout_c("%.8x", val);
      assert(val >= prev);
      prev = val;
    }
  }
  
  switch(entry.m_type){
  case ConfigValues::IntType:
  case ConfigValues::SectionType:
    m_cfg->m_values[pos+1] = entry.m_int;    
    m_freeKeys--;
    DEBUG printf("Putting at: %d(%d) (loop = %d) key: %d value: %d\n", 
		   pos, sz, 0, 
		   (key >> KP_KEYVAL_SHIFT) & KP_KEYVAL_MASK,
		   entry.m_int);
    return true;
  case ConfigValues::StringType:{
    Uint32 index = m_cfg->m_stringCount++;
    m_cfg->m_values[pos+1] = index;
    char **  ref = m_cfg->getString(index);
    * ref = strdup(entry.m_string ? entry.m_string : "");
    m_freeKeys--;
    m_freeData -= sizeof(char *);
    DEBUG printf("Putting at: %d(%d) (loop = %d) key: %d value(%d): %s\n", 
		   pos, sz, 0, 
		   (key >> KP_KEYVAL_SHIFT) & KP_KEYVAL_MASK,
		   index,
		   entry.m_string);
    return true;
  }
  case ConfigValues::Int64Type:{
    Uint32 index = m_cfg->m_int64Count++;
    m_cfg->m_values[pos+1] = index;
    * m_cfg->get64(index) = entry.m_int64;
    m_freeKeys--;
    m_freeData -= 8;
    DEBUG printf("Putting at: %d(%d) (loop = %d) key: %d value64(%d): %lld\n", 
		   pos, sz, 0, 
		   (key >> KP_KEYVAL_SHIFT) & KP_KEYVAL_MASK,
		   index,
		   entry.m_int64);
    return true;
  }
  case ConfigValues::InvalidType:
  default:
    return false;
  }
  return false;
}

void
ConfigValuesFactory::put(const ConfigValues & cfg){
  
  Uint32 curr = m_currentSection;
  m_currentSection = 0;

  ConfigValues::Entry tmp;
  for(Uint32 i = 0; i < 2 * cfg.m_size; i += 2){
    if(cfg.m_values[i] != CFV_KEY_FREE){
      tmp.m_key = cfg.m_values[i];
      cfg.getByPos(i, &tmp);
      put(tmp);
    }
  }

  m_currentSection = curr;
}

ConfigValues *
ConfigValuesFactory::extractCurrentSection(const ConfigValues::ConstIterator & cfg){
  ConfigValuesFactory * fac = new ConfigValuesFactory(20, 20);
  Uint32 curr = cfg.m_currentSection;
 
  ConfigValues::Entry tmp;
  for(Uint32 i = 0; i < 2 * cfg.m_cfg.m_size; i += 2){
    Uint32 keypart = cfg.m_cfg.m_values[i];
    const Uint32 sec = keypart & (KP_SECTION_MASK << KP_SECTION_SHIFT);
    const Uint32 key = keypart & KP_KEYVAL_MASK;
    if(sec == curr && key != CFV_KEY_PARENT){
      tmp.m_key = cfg.m_cfg.m_values[i];
      cfg.m_cfg.getByPos(i, &tmp);
      tmp.m_key = key;
      fac->put(tmp);
    }
  }
  
  ConfigValues * ret = fac->getConfigValues();
  delete fac;
  return ret;
}

ConfigValues *
ConfigValuesFactory::getConfigValues(){
  ConfigValues * ret = m_cfg;
  m_cfg = create(10, 10);
  return ret;
}

static int
mod4(unsigned int i){
  int res = i + (4 - (i % 4));
  return res;
}

Uint32
ConfigValues::getPackedSize() const {

  Uint32 size = 0;
  for(Uint32 i = 0; i < 2 * m_size; i += 2){
    Uint32 key = m_values[i];
    if(key != CFV_KEY_FREE){
      switch(::getTypeOf(key)){
      case IntType:
      case SectionType:
	size += 8;
	break;
      case Int64Type:
	size += 12;
	break;
      case StringType:
	size += 8; // key + len
	size += mod4(strlen(* getString(m_values[i+1])) + 1);
	break;
      case InvalidType:
      default:
	abort();
      }
    }
  }

  return size + sizeof(Magic) + 4; // checksum also
}

Uint32
ConfigValues::pack(void * _dst, Uint32 _len) const {
  Uint32 i;
  char * dst = (char*)_dst;
  memcpy(dst, Magic, sizeof(Magic)); dst += sizeof(Magic);

  for(i = 0; i < 2 * m_size; i += 2){
    Uint32 key = m_values[i];
    Uint32 val = m_values[i+1];
    if(key != CFV_KEY_FREE){
      switch(::getTypeOf(key)){
      case IntType:
      case SectionType:
	* (Uint32*)dst = htonl(key); dst += 4;
	* (Uint32*)dst = htonl(val); dst += 4;
	break;
      case Int64Type:{
	Uint64 i64 = * get64(val); 
	Uint32 hi = (Uint32)(i64 >> 32);
	Uint32 lo = (Uint32)(i64 & 0xFFFFFFFF);
	* (Uint32*)dst = htonl(key); dst += 4;
	* (Uint32*)dst = htonl(hi); dst += 4;
	* (Uint32*)dst = htonl(lo); dst += 4;
      }
	break;
      case StringType:{
	const char * str = * getString(val);
	Uint32 len = Uint32(strlen(str) + 1);
	* (Uint32*)dst = htonl(key); dst += 4;
	* (Uint32*)dst = htonl(len); dst += 4;
	memcpy(dst, str, len); 
	memset(dst+len, 0, mod4(len) - len);
	dst += mod4(len);
      }
	break;
      case InvalidType:
      default:
	abort();
      }
    }
  }

  const Uint32 * sum = (Uint32*)_dst;
  const Uint32 len = Uint32(((Uint32*)dst) - sum);
  Uint32 chk = 0;
  for(i = 0; i<len; i++){
    chk ^= htonl(sum[i]);
  }

  * (Uint32*)dst = htonl(chk); dst += 4;
  return 4 * (len + 1);
}

bool
ConfigValuesFactory::unpack(const void * _src, Uint32 len){

  if(len < sizeof(Magic) + 4){
    DEBUG abort();
    return false;
  }

  if(memcmp(_src, Magic, sizeof(Magic)) != 0){
    DEBUG abort();
    return false;
  }

  const char * src = (const char *)_src;
  const char * end = src + len - 4;
  src += sizeof(Magic);
  
  {
    Uint32 len32 = (len >> 2);
    const Uint32 * tmp = (const Uint32*)_src;
    Uint32 chk = 0;
    for(Uint32 i = 0; (i+1)<len32; i++){
      chk ^= ntohl(tmp[i]);
    }

    if(chk != ntohl(tmp[len32-1])){
      DEBUG abort();
      return false;
    }
  }

  const char * save = src;

  {
    Uint32 keys = 0;
    Uint32 data = 0;
    while(end - src > 4){
      Uint32 tmp = ntohl(* (const Uint32 *)src); src += 4;
      keys++;
      switch(::getTypeOf(tmp)){
      case ConfigValues::IntType:
      case ConfigValues::SectionType:
	src += 4;
	break;
      case ConfigValues::Int64Type:
	src += 8;
	data += 8;
	break;
      case ConfigValues::StringType:{
	Uint32 s_len = ntohl(* (const Uint32 *)src);
	src += 4 + mod4(s_len);
	data += sizeof(char*);
	break;
      }
      default:
	break;
      }
    }
    expand(keys, data);
  }
  
  src = save;
  ConfigValues::Entry entry;
  while(end - src > 4){
    Uint32 tmp = ntohl(* (const Uint32 *)src); src += 4;
    entry.m_key = tmp  & KP_MASK;
    entry.m_type = ::getTypeOf(tmp);
    switch(entry.m_type){
    case ConfigValues::IntType:
    case ConfigValues::SectionType:
      entry.m_int = ntohl(* (const Uint32 *)src); src += 4;
      break;
    case ConfigValues::Int64Type:{
      Uint64 hi = ntohl(* (const Uint32 *)src); src += 4;
      Uint64 lo = ntohl(* (const Uint32 *)src); src += 4;
      entry.m_int64 = (hi <<32) | lo;
    }
      break;
    case ConfigValues::StringType:{
      Uint32 s_len = ntohl(* (const Uint32 *)src); src += 4;
      size_t s_len2 = strlen((const char*)src);
      if(s_len2 + 1 != s_len){
	DEBUG abort();
	return false;
      }

      entry.m_string = (const char*)src; src+= mod4(s_len);
    }
      break;
    case ConfigValues::InvalidType:
    default:
      DEBUG abort();
      return false;
    }
    if(!put(entry)){
      DEBUG abort();
      return false;
    }
  }
  if(src != end){
    DEBUG abort();
    return false;
  }
  return true;
}

#ifdef __TEST_CV_HASH_HPP

int
main(void){
  srand(time(0));
  for(int t = 0; t<100; t++){
    const size_t len = directory(rand() % 1000);

    printf("size = %d\n", len);
    unsigned * buf = new unsigned[len];
    for(size_t key = 0; key<len; key++){
      Uint32 p = hash(key, len);
      for(size_t j = 0; j<len; j++){
	buf[j] = p;
	p = nextHash(key, len, p, j+1);
      }
    
      for(size_t j = 0; j<len; j++){
	Uint32 pos = buf[j];
	int unique = 0;
	for(size_t k = j + 1; k<len; k++){
	  if(pos == buf[k]){
	    if(unique > 0)
	      printf("size=%d key=%d pos(%d)=%d buf[%d]=%d\n", len, key, j, pos, k, buf[k]);
	    unique ++;
	  }
	}
	if(unique > 1){
	  printf("key = %d size = %d not uniqe!!\n", key, len);
	  for(size_t k = 0; k<len; k++){
	    printf("%d ", buf[k]);
	  }
	  printf("\n");
	}
      }
    }
    delete[] buf;
  }
  return 0;
}

#endif
