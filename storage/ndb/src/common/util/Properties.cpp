/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <Properties.hpp>

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include <string>
#include <algorithm>
#include <utility>

static
char * f_strdup(const char * s){
  if(!s) return 0;
  return strdup(s);
}

/**
 * Note has to be a multiple of 4 bytes
 */
const char Properties::version[] = { 2, 0, 0, 1, 1, 1, 1, 4 };
const char Properties::delimiter = ':';

/**
 * PropertyImpl
 */
struct PropertyImpl{
  PropertiesType valueType;
  const char * name;
  void * value;

  ~PropertyImpl();
  PropertyImpl();
  PropertyImpl(const char * name, Uint32 value);
  PropertyImpl(const char * name, Uint64 value);
  PropertyImpl(const char * name, const char * value);
  PropertyImpl(const char * name, const Properties * value);
  PropertyImpl(const PropertyImpl &prop);
  PropertyImpl(PropertyImpl &&prop);
  PropertyImpl& operator=(const PropertyImpl &obj);
  PropertyImpl& operator=(PropertyImpl &&obj);

  bool append(const char * value);

};

/**
 * PropertiesImpl
 */
class PropertiesImpl {
  PropertiesImpl(const PropertiesImpl &);           // Not implemented
  PropertiesImpl& operator=(const PropertiesImpl&); // Not implemented
public:
  PropertiesImpl(Properties *, bool case_insensitive);
  PropertiesImpl(Properties *, const PropertiesImpl &);
  ~PropertiesImpl();

  Properties * properties;
  
  std::unordered_map<std::string, PropertyImpl> content;

  bool m_insensitive;
  void setCaseInsensitiveNames(bool value);
  
  PropertyImpl * get(const char * name);
  PropertyImpl * put(PropertyImpl &);
  PropertyImpl * put(PropertyImpl*);
  void remove(const char * name);
  void clear();
  
  Uint32 getPackedSize(Uint32 pLen) const;
  bool unpack(const Uint32 * buf, Uint32 &bufLen, Properties * top, int items);
  
  Uint32 getTotalItems() const;

  void setErrno(Uint32 pErr, Uint32 osErr = 0){
    properties->setErrno(pErr, osErr);
  }

  const char * getProps(const char * name, PropertiesImpl ** impl);
  const char * getPropsPut(const char * name, PropertiesImpl ** impl);

  friend class IteratorImpl;
};

class IteratorImpl
{
public:
  IteratorImpl(const PropertiesImpl* prop);
  const char* first();
  const char* next();
private:
  const PropertiesImpl* m_impl;
  std::unordered_map<std::string, PropertyImpl> :: const_iterator m_iterator;
};

/**
 * Methods for Property
 */
Property::Property(const char * name, Uint32 value){
  impl = new PropertyImpl(name, value);
}

Property::Property(const char * name, const char * value){
  impl = new PropertyImpl(name, value);
}

Property::Property(const char * name, const class Properties * value){
  impl = new PropertyImpl(name, value);

  ((Properties*)impl->value)->setCaseInsensitiveNames(value->getCaseInsensitiveNames());
}

Property::~Property(){
  delete impl;
}

/**
 * Methods for Properties
 */
Properties::Properties(bool case_insensitive){
  parent = 0;
  impl = new PropertiesImpl(this, case_insensitive);
}

Properties::Properties(const Properties & org){
  parent = 0;
  impl = new PropertiesImpl(this, * org.impl);
}

Properties::Properties(const Property * anArray, int arrayLen){
  impl = new PropertiesImpl(this, false);

  put(anArray, arrayLen);
}

Properties::~Properties(){
  clear();
  delete impl;
}

void
Properties::put(const Property * anArray, int arrayLen){
  if(anArray == 0)
    return;
  for(int i = 0; i<arrayLen; i++)
    impl->put(anArray[i].impl);
}

template <class T>
bool
put(PropertiesImpl * impl, const char * name, T value, bool replace){
  if(name == 0){
    impl->setErrno(E_PROPERTIES_INVALID_NAME);
    return false;
  }

  PropertiesImpl * tmp = 0;
  const char * short_name = impl->getPropsPut(name, &tmp);

  if(tmp == 0){
    impl->setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  
  if(tmp->get(short_name) != 0){
    if(!replace)
    {
      impl->setErrno(E_PROPERTIES_ELEMENT_ALREADY_EXISTS);
      return false;
    }
    tmp->remove(short_name);
  }
  PropertyImpl toPut = PropertyImpl(short_name, value);

  return (tmp->put(toPut) != 0);
}

bool
Properties::put(const char * name, Uint32 value, bool replace){
  return ::put(impl, name, value, replace);
}

bool
Properties::put64(const char * name, Uint64 value, bool replace){
  return ::put(impl, name, value, replace);
}

bool 
Properties::put(const char * name, const char * value, bool replace){
  return ::put(impl, name, value, replace);
}

bool
Properties::append(const char * name, const char * value)
{
  PropertyImpl * nvp = impl->get(name);
  if (nvp == NULL)
  {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (nvp->valueType != PropertiesType_char)
  {
    setErrno(E_PROPERTIES_INVALID_TYPE);
    return false;
  }

  if (!nvp->append(value))
  {
    setErrno(E_PROPERTIES_ERROR_MALLOC_WHILE_UNPACKING);
    return false;
  }

  setErrno(E_PROPERTIES_OK);
  return true;
}

bool 
Properties::put(const char * name, const Properties * value, bool replace){
  return ::put(impl, name, value, replace);
}

bool
Properties::getTypeOf(const char * name, PropertiesType * type) const {
  PropertyImpl * nvp = impl->get(name);
  if(nvp == 0){
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  setErrno(E_PROPERTIES_OK);
  * type = nvp->valueType;
  return true;
}

bool
Properties::contains(const char * name) const {
  PropertyImpl * nvp = impl->get(name);
  return nvp != 0;
}

bool
Properties::get(const char * name, Uint32 * value) const {
  PropertyImpl * nvp = impl->get(name);
  if(nvp == 0){
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  
  if(nvp->valueType == PropertiesType_Uint32){
    * value = * (Uint32 *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }

  if(nvp->valueType == PropertiesType_Uint64){
    Uint64 tmp = * (Uint64 *)nvp->value;
    Uint64 max = 1; max <<= 32;
    if(tmp < max){
      * value = (Uint32)tmp;
      setErrno(E_PROPERTIES_OK);
      return true;
    }
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool
Properties::get(const char * name, Uint64 * value) const {
  PropertyImpl * nvp = impl->get(name);
  if(nvp == 0){
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  
  if(nvp->valueType == PropertiesType_Uint32){
    Uint32 tmp = * (Uint32 *)nvp->value;
    * value = (Uint64)tmp;
    setErrno(E_PROPERTIES_OK);
    return true;
  }

  if(nvp->valueType == PropertiesType_Uint64){
    * value = * (Uint64 *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool
Properties::get(const char * name, const char ** value) const {
  PropertyImpl * nvp = impl->get(name);
  if(nvp == 0){
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if(nvp->valueType == PropertiesType_char){
    * value = (const char *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool
Properties::get(const char * name, BaseString& value) const {
  const char *tmp = "";
  bool ret;
  ret = get(name, &tmp);
  value.assign(tmp);
  return ret;
}

bool
Properties::get(const char * name, const Properties ** value) const {
  PropertyImpl * nvp = impl->get(name);
  if(nvp == 0){
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  if(nvp->valueType == PropertiesType_Properties){
    * value = (const Properties *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool
Properties::getCopy(const char * name, char ** value) const {
  PropertyImpl * nvp = impl->get(name);
  if(nvp == 0){
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if(nvp->valueType == PropertiesType_char){
    * value = f_strdup((const char *)nvp->value);
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool
Properties::getCopy(const char * name, Properties ** value) const {
  PropertyImpl * nvp = impl->get(name);
  if(nvp == 0){
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  
  if(nvp->valueType == PropertiesType_Properties){
    * value = new Properties(* (const Properties *)nvp->value);
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

void 
Properties::clear(){
  impl->clear();
}

void
Properties::remove(const char * name) {
  impl->remove(name);
}

void
Properties::print(FILE * out, const char * prefix) const{
  char buf[1024];
  if(prefix == 0)
    buf[0] = 0;
  else
    strncpy(buf, prefix, 1024);
  
  for (auto i : impl->content){
    switch(i.second.valueType){
    case PropertiesType_Uint32:
      fprintf(out, "%s%s = (Uint32) %d\n", buf, i.second.name,
	      *(Uint32 *)i.second.value);
      break;
    case PropertiesType_Uint64:
      fprintf(out, "%s%s = (Uint64) %lld\n", buf, i.second.name,
	      *(Uint64 *)i.second.value);
      break;
    case PropertiesType_char:
      fprintf(out, "%s%s = (char*) \"%s\"\n", buf, i.second.name,
	      (char *)i.second.value);
      break;
    case PropertiesType_Properties:
      char buf2 [1024];
      BaseString::snprintf(buf2, sizeof(buf2), "%s%s%c",buf, i.second.name,
	      Properties::delimiter);
      ((Properties *)i.second.value)->print(out, buf2);
      break;
    case PropertiesType_Undefined:
      assert(0);
    }
  }
}

Properties::Iterator::Iterator(const Properties* prop) :
  m_prop(prop)
{
  m_iterImpl = new IteratorImpl(prop->impl);
}

Properties::Iterator::~Iterator()
{
  delete m_iterImpl;
}

const char*
Properties::Iterator::first() {
  return m_iterImpl->first();
}

const char*
Properties::Iterator::next() {
  return m_iterImpl->next();
}


IteratorImpl::IteratorImpl(const PropertiesImpl* impl) :
  m_impl(impl)
{
  m_iterator = m_impl->content.begin();
}

const char*
IteratorImpl::first()
{
  m_iterator = m_impl->content.begin();
  return next();
}


const char*
IteratorImpl::next()
{
  if (m_iterator != m_impl->content.end())
  {
    const char* ret  = m_iterator->second.name;
    ++m_iterator;
    return ret;
  }
  else
  {
    return NULL;
  }
}


Uint32
Properties::getPackedSize() const {
  Uint32 sz = 0;
  
  sz += sizeof(version); // Version id of properties object
  sz += 4;               // No Of Items
  sz += 4;               // Checksum

  return sz + impl->getPackedSize(0);
}

static
Uint32
computeChecksum(const Uint32 * buf, Uint32 words){
  Uint32 sum = 0;
  for(unsigned int i = 0; i<words; i++)
    sum ^= htonl(buf[i]);
  
  return sum;
}

bool
Properties::unpack(const Uint32 * buf, Uint32 bufLen){
  const Uint32 * bufStart = buf;
  Uint32 bufLenOrg = bufLen;
  
  if(bufLen < sizeof(version)){
    setErrno(E_PROPERTIES_INVALID_BUFFER_TO_SHORT);
    return false;
  }
  
  if(memcmp(buf, version, sizeof(version)) != 0){
    setErrno(E_PROPERTIES_INVALID_VERSION_WHILE_UNPACKING);
    return false;
  }
  bufLen -= sizeof(version);
  
  // Note that version must be a multiple of 4
  buf += (sizeof(version) / 4); 
  
  if(bufLen < 4){
    setErrno(E_PROPERTIES_INVALID_BUFFER_TO_SHORT);
    return false;
  }

  Uint32 totalItems = ntohl(* buf);
  buf++; bufLen -= 4;
  bool res = impl->unpack(buf, bufLen, this, totalItems);
  if(!res)
    return res;

  Uint32 sum = computeChecksum(bufStart, (bufLenOrg-bufLen)/4);
  if(sum != ntohl(bufStart[(bufLenOrg-bufLen)/4])){
    setErrno(E_PROPERTIES_INVALID_CHECKSUM);
    return false;
  }
  return true;
}

/**
 * Methods for PropertiesImpl
 */
PropertiesImpl::PropertiesImpl(Properties * p, bool case_insensitive){
  this->properties = p;
  setCaseInsensitiveNames(case_insensitive);
}

PropertiesImpl::PropertiesImpl(Properties * p, const PropertiesImpl & org)
: content(org.content)
{
  this->properties = p;
  this->m_insensitive = org.m_insensitive;
}

PropertiesImpl::~PropertiesImpl(){
}

void
PropertiesImpl::setCaseInsensitiveNames(bool value){
  m_insensitive = value;
}

PropertyImpl *
PropertiesImpl::get(const char * name) {
  PropertiesImpl * tmp = 0;
  const char * short_name = getProps(name, &tmp);
  if(tmp == 0){
    return 0;
  }

  std::string str(short_name);
  if (m_insensitive)
  {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }

  auto iter = tmp->content.find(str);
  if (iter != tmp->content.end())
  {
    return &iter->second;
  }

  return 0;
}


PropertyImpl *
PropertiesImpl::put(PropertyImpl*  nvp){

  std::string str(nvp->name);

  if (m_insensitive)
  {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }

  PropertyImpl& put_element = content[str];

  // element should not be present
  assert(put_element.valueType == PropertiesType_Undefined);

  put_element = *nvp;

  if(put_element.valueType == PropertiesType_Properties)
  {
    ((Properties*)put_element.value)->parent = properties;
  }

  return &put_element;
}


PropertyImpl *
PropertiesImpl::put(PropertyImpl&  nvp){

  std::string str(nvp.name);

  if (m_insensitive)
  {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }

  PropertyImpl& put_element = content[str];

  // element should not be present
  assert(put_element.valueType == PropertiesType_Undefined);
  put_element = std::move(nvp);

  if(put_element.valueType == PropertiesType_Properties)
  {
    ((Properties*)put_element.value)->parent = properties;
  }

  return &put_element;
}

void
PropertiesImpl::remove(const char * name){
  std::string str(name);
  if (m_insensitive)
  {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }
  content.erase(str);
}

void
PropertiesImpl::clear()
{
  content.clear();
}

Uint32 
PropertiesImpl::getTotalItems() const {
  int ret = 0;

  for (auto& x: content)
  {
    if (x.second.valueType == PropertiesType_Properties)
    {
      ret += ((Properties*)x.second.value)->impl->getTotalItems();
    }
    else
    {
      ret++;
    }
  }
  return ret;
}

const char *
PropertiesImpl::getProps(const char * name, 
			 PropertiesImpl ** impl) {
  const char * ret = name;
  const char * tmp = strchr(name, Properties::delimiter);
  if(tmp == 0){
    * impl = this;
    return ret;
  } else {
    Uint32 sz = Uint32(tmp - name);
    char * tmp2 = (char*)malloc(sz + 1);
    memcpy(tmp2, name, sz);
    tmp2[sz] = 0;

    PropertyImpl * nvp = get(tmp2);

    free(tmp2);

    if(nvp == 0){
      * impl = 0;
      return 0;
    }
    if(nvp->valueType != PropertiesType_Properties){
      * impl = 0;
      return name;
    }
    return ((Properties*)nvp->value)->impl->getProps(tmp+1, impl);
  }
}

const char * 
PropertiesImpl::getPropsPut(const char * name, 
			    PropertiesImpl ** impl) {
  const char * ret = name;
  const char * tmp = strchr(name, Properties::delimiter);
  if(tmp == 0){
    * impl = this;
    return ret;
  } else {
    Uint32 sz = Uint32(tmp - name);
    char * tmp2 = (char*)malloc(sz + 1);
    memcpy(tmp2, name, sz);
    tmp2[sz] = 0;
    
    PropertyImpl * nvp = get(tmp2);

    if(nvp == 0){
      Properties   * tmpP  = new Properties();
      PropertyImpl tmpPI = PropertyImpl(tmp2, tmpP);
      PropertyImpl * nvp2 = put(tmpPI);

      delete tmpP;
      free(tmp2);

      return ((Properties*)nvp2->value)->impl->getPropsPut(tmp+1, impl);
    }
    free(tmp2);
    if(nvp->valueType != PropertiesType_Properties){
      * impl = 0;
      return name;
    }
    return ((Properties*)nvp->value)->impl->getPropsPut(tmp+1, impl);
  }
}

int
mod4(unsigned int i){
  int res = i + (4 - (i % 4));
  return res;
}

Uint32
PropertiesImpl::getPackedSize(Uint32 pLen) const {
  Uint32 sz = 0;
  for (auto& x: content)
  {
    if(x.second.valueType == PropertiesType_Properties)
    {
      Properties * p = (Properties*)x.second.value;
      sz += p->impl->getPackedSize(pLen+(Uint32)(x.first.length())+1);
    }
    else
    {
      sz += 4; // Type
      sz += 4; // Name Len
      sz += 4; // Value Len
      sz += mod4(pLen + (unsigned)(x.first.length())); // Name
      switch(x.second.valueType){
      case PropertiesType_char:
        sz += mod4((unsigned)strlen((char *)x.second.value));
        break;
      case PropertiesType_Uint32:
        sz += mod4(4);
        break;
      case PropertiesType_Uint64:
        sz += mod4(8);
        break;
      case PropertiesType_Properties:
      default:
        assert(0);
      }
    }
  }
  return sz;
}

struct CharBuf {
  char * buffer;
  Uint32 bufLen;
  Uint32 contentLen;

  CharBuf(){
    buffer     = 0;
    bufLen     = 0;
    contentLen = 0;
  }

  ~CharBuf(){
    free(buffer);
  }
  
  void clear() { contentLen = 0;}
  bool add(const char * str, Uint32 strLen){
    if(!expand(contentLen + strLen + 1))
      return false;
    memcpy(&buffer[contentLen], str, strLen);
    contentLen += strLen;
    buffer[contentLen] = 0;
    return true;
  }
  
  bool add(char c){
    return add(&c, 1);
  }

  bool expand(Uint32 newSize){
    if(newSize >= bufLen){
      
      char * tmp = (char*)malloc(newSize + 1024);
      memset(tmp, 0, newSize + 1024);
      if(tmp == 0)
	return false;
      if(contentLen > 0)
	memcpy(tmp, buffer, contentLen);
      if(buffer != 0)
	free(buffer);
      buffer = tmp;
      bufLen = newSize + 1024;
    }
    return true;
  }
};

bool
PropertiesImpl::unpack(const Uint32 * buf, Uint32 &bufLen, Properties * top,
		       int _items){
  CharBuf charBuf;
  while(_items > 0){
    Uint32 tmp[3]; 
    
    if(bufLen <= 12){
      top->setErrno(E_PROPERTIES_BUFFER_TO_SMALL_WHILE_UNPACKING);
      return false;
    }

    tmp[0] = ntohl(buf[0]);
    tmp[1] = ntohl(buf[1]);
    tmp[2] = ntohl(buf[2]);
    buf    += 3;
    bufLen -= 12;

    PropertiesType pt   = (PropertiesType)tmp[0];
    Uint32 nameLen      = tmp[1];
    Uint32 valueLen     = tmp[2];
    Uint32 nameLenRead  = mod4(nameLen);
    Uint32 valueLenRead = mod4(valueLen);

    Uint32 sz = nameLenRead + valueLenRead;
    if(bufLen < sz){
      top->setErrno(E_PROPERTIES_BUFFER_TO_SMALL_WHILE_UNPACKING);
      return false;
    }

    if(!charBuf.expand(sz)){
      top->setErrno(E_PROPERTIES_ERROR_MALLOC_WHILE_UNPACKING, errno);
      return false;
    }

    memcpy(charBuf.buffer, buf, sz);
    buf    += (sz / 4);
    bufLen -= sz ;

    char * valBuf  = charBuf.buffer;
    char * nameBuf = charBuf.buffer + valueLenRead;
    
    nameBuf[nameLen] = 0;
    valBuf[valueLen] = 0;
    
    bool res3 = false;
    switch(pt){
    case PropertiesType_Uint32:
      res3 = top->put(nameBuf, ntohl(* (Uint32 *)valBuf), true);
      break;
    case PropertiesType_Uint64:{
      Uint64 hi = ntohl(* (Uint32 *)valBuf);
      Uint64 lo = ntohl(* (Uint32 *)(valBuf + 4));
      res3 = top->put64(nameBuf, (hi << 32) + lo, true);
    }
      break;
    case PropertiesType_char:
      res3 = top->put(nameBuf, valBuf, true);
      break;
    case PropertiesType_Properties:
    case PropertiesType_Undefined:
      assert(0);
    }
    if(!res3){
      return false;
    }
    _items--;
  }
  return true;
}

PropertyImpl::~PropertyImpl(){
  free((char*)name);
  switch(valueType){
  case PropertiesType_Uint32:
    delete (Uint32 *)value;
    break;
  case PropertiesType_Uint64:
    delete (Uint64 *)value;
    break;
  case PropertiesType_char:
    free((char *)value);
    break;
  case PropertiesType_Properties:
    delete (Properties *)value;
    break;
  case PropertiesType_Undefined:
    break;
  default:
    assert(0);
  }
}

PropertyImpl::PropertyImpl() :
    valueType(PropertiesType_Undefined),
    name(nullptr),
    value(nullptr) {}

PropertyImpl::PropertyImpl(const char * _name, Uint32 _value) :
    valueType(PropertiesType_Uint32),
    name(f_strdup(_name)),
    value(new Uint32(_value)) {}

PropertyImpl::PropertyImpl(const char * _name, Uint64 _value) :
    valueType(PropertiesType_Uint64),
    name(f_strdup(_name)),
    value(new Uint64(_value)) {}

PropertyImpl::PropertyImpl(const char * _name, const char * _value) :
    valueType(PropertiesType_char),
    name(f_strdup(_name)),
    value(f_strdup(_value)) {}

PropertyImpl::PropertyImpl(const char * _name, const Properties * _value) :
    valueType(PropertiesType_Properties),
    name(f_strdup(_name)),
    value(new Properties(* _value)) {}


PropertyImpl::PropertyImpl(const PropertyImpl &prop)
{
  switch (prop.valueType)
  {
    case PropertiesType_Uint32:
      name = f_strdup(prop.name);
      value = new Uint32;
      * ((Uint32 *)value) = *(Uint32*)prop.value;
      valueType = PropertiesType_Uint32;
      break;
    case PropertiesType_Uint64:
      name = f_strdup(prop.name);
      value = new Uint64;
      * ((Uint64 *)value) = *(Uint64*)prop.value;
      valueType = PropertiesType_Uint64;
      break;
    case PropertiesType_char:
      name = f_strdup(prop.name);
      value = f_strdup((char*)prop.value);
      valueType = PropertiesType_char;
      break;
    case PropertiesType_Properties:
      name = f_strdup(prop.name);
      value = new Properties(*(Properties*) prop.value);
      valueType = PropertiesType_Properties;
      break;
    default:
      fprintf(stderr, "Type:%d\n", prop.valueType);
      assert(0);
  }
}

PropertyImpl::PropertyImpl(PropertyImpl &&prop) :
    valueType(prop.valueType),
    name(prop.name),
    value(prop.value)
{
  prop.valueType = PropertiesType_Undefined;
  prop.name = nullptr;
  prop.value = nullptr;
}

PropertyImpl& PropertyImpl::operator = (const PropertyImpl &prop)
{
  if (this == &prop)
  {
    return *this;
  }

  free((char*)name);
  switch(valueType)
  {
  case PropertiesType_Uint32:
    delete (Uint32 *)value;
    break;
  case PropertiesType_Uint64:
    delete (Uint64 *)value;
    break;
  case PropertiesType_char:
    free((char *)value);
    break;
  case PropertiesType_Properties:
    delete (Properties *)value;
    break;
  case PropertiesType_Undefined:
    break;
  default:
    assert(0);
  }

  switch (prop.valueType)
  {
    case PropertiesType_Uint32:
      name = f_strdup(prop.name);
      value = new Uint32(*(Uint32*)prop.value);
      this->valueType = PropertiesType_Uint32;
      break;
    case PropertiesType_Uint64:
      name = f_strdup(prop.name);
      value = new Uint64(*(Uint64*)prop.value);
      valueType = PropertiesType_Uint64;
      break;
    case PropertiesType_char:
      name = f_strdup(prop.name);
      value = f_strdup((char*)prop.value);
      valueType = PropertiesType_char;
      break;
    case PropertiesType_Properties:
      name = f_strdup(prop.name);
      value = new Properties(*(Properties*) prop.value);
      valueType = PropertiesType_Properties;
      break;
    default:
      fprintf(stderr, "Type:%d\n", prop.valueType);
      assert(0);
  }
  return *this;
}

PropertyImpl&
PropertyImpl::operator=(PropertyImpl &&obj)
{
  assert(this != &obj);
  using std::swap;

  swap(valueType, obj.valueType);
  swap(name, obj.name);
  swap(value, obj.value);

  return *this;
}

bool
PropertyImpl::append(const char * value)
{
  assert(this->valueType == PropertiesType_char);
  assert(this->value != NULL);
  assert(value != NULL);

  const size_t old_len = strlen(reinterpret_cast<char*>(this->value));
  const size_t new_len = old_len + strlen(value);
  char * new_value = reinterpret_cast<char*>(realloc(this->value, new_len + 1));
  if (new_value == NULL)
  {
    return false;
  }
  const size_t append_len = new_len + 1 - old_len;
  memcpy(new_value + old_len, value, append_len);
  this->value = new_value;
  return true;
}


const Uint32 E_PROPERTIES_OK                                      = 0;
const Uint32 E_PROPERTIES_INVALID_NAME                            = 1;
const Uint32 E_PROPERTIES_NO_SUCH_ELEMENT                         = 2;
const Uint32 E_PROPERTIES_INVALID_TYPE                            = 3;
const Uint32 E_PROPERTIES_ELEMENT_ALREADY_EXISTS                  = 4;

const Uint32 E_PROPERTIES_ERROR_MALLOC_WHILE_PACKING              = 5;
const Uint32 E_PROPERTIES_INVALID_VERSION_WHILE_UNPACKING         = 6;
const Uint32 E_PROPERTIES_INVALID_BUFFER_TO_SHORT                 = 7;
const Uint32 E_PROPERTIES_ERROR_MALLOC_WHILE_UNPACKING            = 8;
const Uint32 E_PROPERTIES_INVALID_CHECKSUM                        = 9;
const Uint32 E_PROPERTIES_BUFFER_TO_SMALL_WHILE_UNPACKING         = 10;

/**
 * These are methods that used to be inline
 *
 * But Diab 4.1f could not compile -release with to many inlines
 */
void
Properties::setErrno(Uint32 pErr, Uint32 osErr) const {
  if(parent != 0){
    parent->setErrno(pErr, osErr);
    return ;
  }
  
  /**
   * propErrno & osErrno used to be mutable,
   * but diab didn't know what mutable meant.
   */
  *((Uint32*)&propErrno) = pErr;
  *((Uint32*)&osErrno)   = osErr;
}

/**
 * Inlined get/put(name, no, ...) - methods
 */
 
bool
Properties::put(const char * name, Uint32 no, Uint32 val, bool replace){
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put(tmp, val, replace);
  free(tmp);
  return res;
}

bool
Properties::put64(const char * name, Uint32 no, Uint64 val, bool replace){
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put64(tmp, val, replace);
  free(tmp);
  return res;
}


bool 
Properties::put(const char * name, Uint32 no, const char * val, bool replace){
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put(tmp, val, replace);
  free(tmp);
  return res;
}


bool 
Properties::put(const char * name, Uint32 no, const Properties * val, 
		bool replace){
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put(tmp, val, replace);
  free(tmp);
  return res;
}


bool 
Properties::getTypeOf(const char * name, Uint32 no, 
		      PropertiesType * type) const {
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = getTypeOf(tmp, type);
  free(tmp);
  return res;
}

bool 
Properties::contains(const char * name, Uint32 no) const {
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = contains(tmp);
  free(tmp);
  return res;
}

bool 
Properties::get(const char * name, Uint32 no, Uint32 * value) const{
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}

bool 
Properties::get(const char * name, Uint32 no, Uint64 * value) const{
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}


bool 
Properties::get(const char * name, Uint32 no, const char ** value) const {
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}


bool 
Properties::get(const char * name, Uint32 no, const Properties ** value) const{
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}


bool 
Properties::getCopy(const char * name, Uint32 no, char ** value) const {
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = getCopy(tmp, value);
  free(tmp);
  return res;
}


bool 
Properties::getCopy(const char * name, Uint32 no, Properties ** value) const {
  size_t tmp_len = strlen(name)+20;
  char * tmp = (char*)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = getCopy(tmp, value);
  free(tmp);
  return res;
}

void
Properties::setCaseInsensitiveNames(bool value){
  impl->setCaseInsensitiveNames(value);
}

bool
Properties::getCaseInsensitiveNames() const {
  return impl->m_insensitive;
}

template bool put(PropertiesImpl *, const char *, Uint32, bool);
template bool put(PropertiesImpl *, const char *, Uint64, bool);
template bool put(PropertiesImpl *, const char *, const char *, bool);
template bool put(PropertiesImpl *, const char *, const Properties*, bool);
