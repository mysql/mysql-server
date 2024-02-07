/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

#include <NdbOut.hpp>
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include "portlib/ndb_socket.h"  // ntohl()
#include "util/cstrbuf.h"

static char *f_strdup(std::string_view s) {
  if (s.data() == nullptr) return nullptr;
  char *p = (char *)malloc(s.size() + 1);
  memcpy(p, s.data(), s.size());
  p[s.size()] = 0;
  return p;
}

/**
 * Note has to be a multiple of 4 bytes
 */
const char Properties::version[] = {2, 0, 0, 1, 1, 1, 1, 4};

/**
 * PropertyImpl
 */
struct PropertyImpl {
  PropertiesType valueType;
  const char *name;
  void *value;

  ~PropertyImpl();
  PropertyImpl();
  PropertyImpl(const char *name, Uint32 value);
  PropertyImpl(const char *name, Uint64 value);
  PropertyImpl(const char *name, std::string_view value);
  PropertyImpl(const char *name, const Properties *value);
  PropertyImpl(const PropertyImpl &prop);
  PropertyImpl(PropertyImpl &&prop);
  PropertyImpl &operator=(const PropertyImpl &obj);
  PropertyImpl &operator=(PropertyImpl &&obj);

  bool append(std::string_view value);
};

/**
 * PropertiesImpl
 */
class PropertiesImpl {
  PropertiesImpl(const PropertiesImpl &) = delete;
  PropertiesImpl &operator=(const PropertiesImpl &) = delete;

 public:
  PropertiesImpl(Properties *, bool case_insensitive);
  PropertiesImpl(Properties *, const PropertiesImpl &);
  ~PropertiesImpl();

  Properties *properties;

  std::unordered_map<std::string, PropertyImpl> content;

  bool m_insensitive;
  void setCaseInsensitiveNames(bool value);

  PropertyImpl *get(const char *name);
  PropertyImpl *put(PropertyImpl &);
  PropertyImpl *put(PropertyImpl *);
  void remove(const char *name);
  void clear();

  Uint32 getTotalItems() const;

  void setErrno(Uint32 pErr, Uint32 osErr = 0) {
    properties->setErrno(pErr, osErr);
  }

  const char *getProps(const char *name, PropertiesImpl **impl);
  const char *getPropsPut(const char *name, PropertiesImpl **impl);

  friend class IteratorImpl;
};

class IteratorImpl {
 public:
  IteratorImpl(const PropertiesImpl *prop);
  const char *first();
  const char *next();

 private:
  const PropertiesImpl *m_impl;
  std::unordered_map<std::string, PropertyImpl>::const_iterator m_iterator;
};

/**
 * Methods for Property
 */
Property::Property(const char *name, Uint32 value) {
  impl = new PropertyImpl(name, value);
}

Property::Property(const char *name, const char *value) {
  impl = new PropertyImpl(name, std::string_view{value});
}

Property::Property(const char *name, std::string_view value) {
  impl = new PropertyImpl(name, value);
}

Property::Property(const char *name, const class Properties *value) {
  impl = new PropertyImpl(name, value);

  ((Properties *)impl->value)
      ->setCaseInsensitiveNames(value->getCaseInsensitiveNames());
}

Property::~Property() { delete impl; }

/**
 * Methods for Properties
 */
Properties::Properties(bool case_insensitive) {
  parent = nullptr;
  impl = new PropertiesImpl(this, case_insensitive);
}

Properties::Properties(const Properties &org) {
  parent = nullptr;
  impl = new PropertiesImpl(this, *org.impl);
}

Properties::Properties(const Property *anArray, int arrayLen) {
  impl = new PropertiesImpl(this, false);

  put(anArray, arrayLen);
}

Properties &Properties::operator=(const Properties &org) {
  if (this == &org) {
    return *this;
  }
  delete impl;

  propErrno = 0;
  osErrno = 0;
  impl = new PropertiesImpl(this, *org.impl);
  parent = nullptr;

  return *this;
}

Properties::~Properties() {
  clear();
  delete impl;
}

void Properties::put(const Property *anArray, int arrayLen) {
  if (anArray == nullptr) return;
  for (int i = 0; i < arrayLen; i++) impl->put(anArray[i].impl);
}

template <class T>
bool put(PropertiesImpl *impl, const char *name, T value, bool replace) {
  if (name == nullptr) {
    impl->setErrno(E_PROPERTIES_INVALID_NAME);
    return false;
  }

  PropertiesImpl *tmp = nullptr;
  const char *short_name = impl->getPropsPut(name, &tmp);

  if (tmp == nullptr) {
    impl->setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (tmp->get(short_name) != nullptr) {
    if (!replace) {
      impl->setErrno(E_PROPERTIES_ELEMENT_ALREADY_EXISTS);
      return false;
    }
    tmp->remove(short_name);
  }
  PropertyImpl toPut = PropertyImpl(short_name, value);

  return (tmp->put(toPut) != nullptr);
}

bool Properties::put(const char *name, Uint32 value, bool replace) {
  return ::put(impl, name, value, replace);
}

bool Properties::put64(const char *name, Uint64 value, bool replace) {
  return ::put(impl, name, value, replace);
}

bool Properties::put(const char *name, const char *value, bool replace) {
  return ::put(impl, name, std::string_view{value}, replace);
}

bool Properties::put(const char *name, std::string_view value, bool replace) {
  return ::put(impl, name, value, replace);
}

bool Properties::append(const char *name, const char *value) {
  return append(name, std::string_view{value});
}

bool Properties::append(const char *name, std::string_view value) {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (nvp->valueType != PropertiesType_char) {
    setErrno(E_PROPERTIES_INVALID_TYPE);
    return false;
  }

  if (!nvp->append(value)) {
    setErrno(E_PROPERTIES_ERROR_MALLOC_WHILE_UNPACKING);
    return false;
  }

  setErrno(E_PROPERTIES_OK);
  return true;
}

bool Properties::put(const char *name, const Properties *value, bool replace) {
  return ::put(impl, name, value, replace);
}

bool Properties::getTypeOf(const char *name, PropertiesType *type) const {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  setErrno(E_PROPERTIES_OK);
  *type = nvp->valueType;
  return true;
}

bool Properties::contains(const char *name) const {
  PropertyImpl *nvp = impl->get(name);
  return nvp != nullptr;
}

bool Properties::get(const char *name, Uint32 *value) const {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (nvp->valueType == PropertiesType_Uint32) {
    *value = *(Uint32 *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }

  if (nvp->valueType == PropertiesType_Uint64) {
    Uint64 tmp = *(Uint64 *)nvp->value;
    Uint64 max = 1;
    max <<= 32;
    if (tmp < max) {
      *value = (Uint32)tmp;
      setErrno(E_PROPERTIES_OK);
      return true;
    }
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool Properties::get(const char *name, Uint64 *value) const {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (nvp->valueType == PropertiesType_Uint32) {
    Uint32 tmp = *(Uint32 *)nvp->value;
    *value = (Uint64)tmp;
    setErrno(E_PROPERTIES_OK);
    return true;
  }

  if (nvp->valueType == PropertiesType_Uint64) {
    *value = *(Uint64 *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool Properties::get(const char *name, const char **value) const {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (nvp->valueType == PropertiesType_char) {
    *value = (const char *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool Properties::get(const char *name, BaseString &value) const {
  const char *tmp = "";
  bool ret;
  ret = get(name, &tmp);
  value.assign(tmp);
  return ret;
}

bool Properties::get(const char *name, const Properties **value) const {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }
  if (nvp->valueType == PropertiesType_Properties) {
    *value = (const Properties *)nvp->value;
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool Properties::getCopy(const char *name, char **value) const {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (nvp->valueType == PropertiesType_char) {
    *value = f_strdup((const char *)nvp->value);
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

bool Properties::getCopy(const char *name, Properties **value) const {
  PropertyImpl *nvp = impl->get(name);
  if (nvp == nullptr) {
    setErrno(E_PROPERTIES_NO_SUCH_ELEMENT);
    return false;
  }

  if (nvp->valueType == PropertiesType_Properties) {
    *value = new Properties(*(const Properties *)nvp->value);
    setErrno(E_PROPERTIES_OK);
    return true;
  }
  setErrno(E_PROPERTIES_INVALID_TYPE);
  return false;
}

void Properties::clear() { impl->clear(); }

void Properties::remove(const char *name) { impl->remove(name); }

void Properties::print(FILE *out, const char *prefix) const {
  if (prefix == nullptr) {
    prefix = "";
  }

  for (auto i : impl->content) {
    switch (i.second.valueType) {
      case PropertiesType_Uint32:
        fprintf(out, "%s%s = (Uint32) %d\n", prefix, i.second.name,
                *(Uint32 *)i.second.value);
        break;
      case PropertiesType_Uint64:
        fprintf(out, "%s%s = (Uint64) %lld\n", prefix, i.second.name,
                *(Uint64 *)i.second.value);
        break;
      case PropertiesType_char:
        fprintf(out, "%s%s = (char*) \"%s\"\n", prefix, i.second.name,
                (char *)i.second.value);
        break;
      case PropertiesType_Properties: {
        cstrbuf<1024> new_prefix;
        new_prefix.append(prefix);
        new_prefix.append(i.second.name);
        new_prefix.append(1, delimiter);
        new_prefix.replace_end_if_truncated(truncated_prefix_mark);
        ((Properties *)i.second.value)->print(out, new_prefix.c_str());
        break;
      }
      case PropertiesType_Undefined:
        assert(0);
    }
  }
}

Properties::Iterator::Iterator(const Properties *prop) : m_prop(prop) {
  m_iterImpl = new IteratorImpl(prop->impl);
}

Properties::Iterator::~Iterator() { delete m_iterImpl; }

const char *Properties::Iterator::first() { return m_iterImpl->first(); }

const char *Properties::Iterator::next() { return m_iterImpl->next(); }

IteratorImpl::IteratorImpl(const PropertiesImpl *impl) : m_impl(impl) {
  m_iterator = m_impl->content.begin();
}

const char *IteratorImpl::first() {
  m_iterator = m_impl->content.begin();
  return next();
}

const char *IteratorImpl::next() {
  if (m_iterator != m_impl->content.end()) {
    const char *ret = m_iterator->second.name;
    ++m_iterator;
    return ret;
  } else {
    return nullptr;
  }
}

/**
 * Methods for PropertiesImpl
 */
PropertiesImpl::PropertiesImpl(Properties *p, bool case_insensitive) {
  this->properties = p;
  setCaseInsensitiveNames(case_insensitive);
}

PropertiesImpl::PropertiesImpl(Properties *p, const PropertiesImpl &org)
    : content(org.content) {
  this->properties = p;
  this->m_insensitive = org.m_insensitive;
}

PropertiesImpl::~PropertiesImpl() {}

void PropertiesImpl::setCaseInsensitiveNames(bool value) {
  m_insensitive = value;
}

PropertyImpl *PropertiesImpl::get(const char *name) {
  PropertiesImpl *tmp = nullptr;
  const char *short_name = getProps(name, &tmp);
  if (tmp == nullptr) {
    return nullptr;
  }

  std::string str(short_name);
  if (m_insensitive) {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }

  auto iter = tmp->content.find(str);
  if (iter != tmp->content.end()) {
    return &iter->second;
  }

  return nullptr;
}

PropertyImpl *PropertiesImpl::put(PropertyImpl *nvp) {
  std::string str(nvp->name);

  if (m_insensitive) {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }

  PropertyImpl &put_element = content[str];

  // element should not be present
  assert(put_element.valueType == PropertiesType_Undefined);

  put_element = *nvp;

  if (put_element.valueType == PropertiesType_Properties) {
    ((Properties *)put_element.value)->parent = properties;
  }

  return &put_element;
}

PropertyImpl *PropertiesImpl::put(PropertyImpl &nvp) {
  std::string str(nvp.name);

  if (m_insensitive) {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }

  PropertyImpl &put_element = content[str];

  // element should not be present
  assert(put_element.valueType == PropertiesType_Undefined);
  put_element = std::move(nvp);

  if (put_element.valueType == PropertiesType_Properties) {
    ((Properties *)put_element.value)->parent = properties;
  }

  return &put_element;
}

void PropertiesImpl::remove(const char *name) {
  std::string str(name);
  if (m_insensitive) {
    std::transform(str.begin(), str.end(), str.begin(), tolower);
  }
  content.erase(str);
}

void PropertiesImpl::clear() { content.clear(); }

Uint32 PropertiesImpl::getTotalItems() const {
  int ret = 0;

  for (auto &x : content) {
    if (x.second.valueType == PropertiesType_Properties) {
      ret += ((Properties *)x.second.value)->impl->getTotalItems();
    } else {
      ret++;
    }
  }
  return ret;
}

const char *PropertiesImpl::getProps(const char *name, PropertiesImpl **impl) {
  const char *ret = name;
  const char *tmp = strchr(name, Properties::delimiter);
  if (tmp == nullptr) {
    *impl = this;
    return ret;
  } else {
    Uint32 sz = Uint32(tmp - name);
    char *tmp2 = (char *)malloc(sz + 1);
    memcpy(tmp2, name, sz);
    tmp2[sz] = 0;

    PropertyImpl *nvp = get(tmp2);

    free(tmp2);

    if (nvp == nullptr) {
      *impl = nullptr;
      return nullptr;
    }
    if (nvp->valueType != PropertiesType_Properties) {
      *impl = nullptr;
      return name;
    }
    return ((Properties *)nvp->value)->impl->getProps(tmp + 1, impl);
  }
}

const char *PropertiesImpl::getPropsPut(const char *name,
                                        PropertiesImpl **impl) {
  const char *ret = name;
  const char *tmp = strchr(name, Properties::delimiter);
  if (tmp == nullptr) {
    *impl = this;
    return ret;
  } else {
    Uint32 sz = Uint32(tmp - name);
    char *tmp2 = (char *)malloc(sz + 1);
    memcpy(tmp2, name, sz);
    tmp2[sz] = 0;

    PropertyImpl *nvp = get(tmp2);

    if (nvp == nullptr) {
      Properties *tmpP = new Properties();
      PropertyImpl tmpPI = PropertyImpl(tmp2, tmpP);
      PropertyImpl *nvp2 = put(tmpPI);

      delete tmpP;
      free(tmp2);

      return ((Properties *)nvp2->value)->impl->getPropsPut(tmp + 1, impl);
    }
    free(tmp2);
    if (nvp->valueType != PropertiesType_Properties) {
      *impl = nullptr;
      return name;
    }
    return ((Properties *)nvp->value)->impl->getPropsPut(tmp + 1, impl);
  }
}

struct CharBuf {
  char *buffer;
  Uint32 bufLen;
  Uint32 contentLen;

  CharBuf() {
    buffer = nullptr;
    bufLen = 0;
    contentLen = 0;
  }

  ~CharBuf() { free(buffer); }

  void clear() { contentLen = 0; }
  bool add(const char *str, Uint32 strLen) {
    if (!expand(contentLen + strLen + 1)) return false;
    memcpy(&buffer[contentLen], str, strLen);
    contentLen += strLen;
    buffer[contentLen] = 0;
    return true;
  }

  bool add(char c) { return add(&c, 1); }

  bool expand(Uint32 newSize) {
    if (newSize >= bufLen) {
      char *tmp = (char *)malloc(newSize + 1024);
      memset(tmp, 0, newSize + 1024);
      if (tmp == nullptr) return false;
      if (contentLen > 0) memcpy(tmp, buffer, contentLen);
      if (buffer != nullptr) free(buffer);
      buffer = tmp;
      bufLen = newSize + 1024;
    }
    return true;
  }
};

PropertyImpl::~PropertyImpl() {
  free(const_cast<char *>(name));
  switch (valueType) {
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

PropertyImpl::PropertyImpl()
    : valueType(PropertiesType_Undefined), name(nullptr), value(nullptr) {}

PropertyImpl::PropertyImpl(const char *_name, Uint32 _value)
    : valueType(PropertiesType_Uint32),
      name(f_strdup(_name)),
      value(new Uint32(_value)) {}

PropertyImpl::PropertyImpl(const char *_name, Uint64 _value)
    : valueType(PropertiesType_Uint64),
      name(f_strdup(_name)),
      value(new Uint64(_value)) {}

PropertyImpl::PropertyImpl(const char *_name, std::string_view _value)
    : valueType(PropertiesType_char),
      name(f_strdup(_name)),
      value(f_strdup(_value)) {}

PropertyImpl::PropertyImpl(const char *_name, const Properties *_value)
    : valueType(PropertiesType_Properties),
      name(f_strdup(_name)),
      value(new Properties(*_value)) {}

PropertyImpl::PropertyImpl(const PropertyImpl &prop) {
  switch (prop.valueType) {
    case PropertiesType_Uint32:
      name = f_strdup(prop.name);
      value = new Uint32;
      *((Uint32 *)value) = *(Uint32 *)prop.value;
      valueType = PropertiesType_Uint32;
      break;
    case PropertiesType_Uint64:
      name = f_strdup(prop.name);
      value = new Uint64;
      *((Uint64 *)value) = *(Uint64 *)prop.value;
      valueType = PropertiesType_Uint64;
      break;
    case PropertiesType_char:
      name = f_strdup(prop.name);
      value = f_strdup((char *)prop.value);
      valueType = PropertiesType_char;
      break;
    case PropertiesType_Properties:
      name = f_strdup(prop.name);
      value = new Properties(*(Properties *)prop.value);
      valueType = PropertiesType_Properties;
      break;
    default:
      fprintf(stderr, "Type:%d\n", prop.valueType);
      assert(0);
  }
}

PropertyImpl::PropertyImpl(PropertyImpl &&prop)
    : valueType(prop.valueType), name(prop.name), value(prop.value) {
  prop.valueType = PropertiesType_Undefined;
  prop.name = nullptr;
  prop.value = nullptr;
}

PropertyImpl &PropertyImpl::operator=(const PropertyImpl &prop) {
  if (this == &prop) {
    return *this;
  }

  free(const_cast<char *>(name));
  switch (valueType) {
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

  switch (prop.valueType) {
    case PropertiesType_Uint32:
      name = f_strdup(prop.name);
      value = new Uint32(*(Uint32 *)prop.value);
      this->valueType = PropertiesType_Uint32;
      break;
    case PropertiesType_Uint64:
      name = f_strdup(prop.name);
      value = new Uint64(*(Uint64 *)prop.value);
      valueType = PropertiesType_Uint64;
      break;
    case PropertiesType_char:
      name = f_strdup(prop.name);
      value = f_strdup((char *)prop.value);
      valueType = PropertiesType_char;
      break;
    case PropertiesType_Properties:
      name = f_strdup(prop.name);
      value = new Properties(*(Properties *)prop.value);
      valueType = PropertiesType_Properties;
      break;
    default:
      fprintf(stderr, "Type:%d\n", prop.valueType);
      assert(0);
  }
  return *this;
}

PropertyImpl &PropertyImpl::operator=(PropertyImpl &&obj) {
  assert(this != &obj);
  using std::swap;

  swap(valueType, obj.valueType);
  swap(name, obj.name);
  swap(value, obj.value);

  return *this;
}

bool PropertyImpl::append(std::string_view value) {
  assert(this->valueType == PropertiesType_char);
  assert(this->value != nullptr);
  assert(value.data() != nullptr);

  const size_t old_len = strlen(reinterpret_cast<char *>(this->value));
  const size_t new_len = old_len + value.size();
  char *new_value = reinterpret_cast<char *>(realloc(this->value, new_len + 1));
  if (new_value == nullptr) {
    return false;
  }
  memcpy(new_value + old_len, value.data(), value.size());
  new_value[new_len] = 0;
  this->value = new_value;
  return true;
}

const Uint32 E_PROPERTIES_OK = 0;
const Uint32 E_PROPERTIES_INVALID_NAME = 1;
const Uint32 E_PROPERTIES_NO_SUCH_ELEMENT = 2;
const Uint32 E_PROPERTIES_INVALID_TYPE = 3;
const Uint32 E_PROPERTIES_ELEMENT_ALREADY_EXISTS = 4;
const Uint32 E_PROPERTIES_ERROR_MALLOC_WHILE_UNPACKING = 5;

/**
 * These are methods that used to be inline
 *
 * But Diab 4.1f could not compile -release with to many inlines
 */
void Properties::setErrno(Uint32 pErr, Uint32 osErr) const {
  if (parent != nullptr) {
    parent->setErrno(pErr, osErr);
    return;
  }

  propErrno = pErr;
  osErrno = osErr;
}

/**
 * Inlined get/put(name, no, ...) - methods
 */

bool Properties::put(const char *name, Uint32 no, Uint32 val, bool replace) {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put(tmp, val, replace);
  free(tmp);
  return res;
}

bool Properties::put64(const char *name, Uint32 no, Uint64 val, bool replace) {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put64(tmp, val, replace);
  free(tmp);
  return res;
}

bool Properties::put(const char *name, Uint32 no, const char *val,
                     bool replace) {
  return put(name, no, std::string_view{val}, replace);
}

bool Properties::put(const char *name, Uint32 no, std::string_view val,
                     bool replace) {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put(tmp, val, replace);
  free(tmp);
  return res;
}

bool Properties::put(const char *name, Uint32 no, const Properties *val,
                     bool replace) {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = put(tmp, val, replace);
  free(tmp);
  return res;
}

bool Properties::getTypeOf(const char *name, Uint32 no,
                           PropertiesType *type) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = getTypeOf(tmp, type);
  free(tmp);
  return res;
}

bool Properties::contains(const char *name, Uint32 no) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = contains(tmp);
  free(tmp);
  return res;
}

bool Properties::get(const char *name, Uint32 no, Uint32 *value) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}

bool Properties::get(const char *name, Uint32 no, Uint64 *value) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}

bool Properties::get(const char *name, Uint32 no, const char **value) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}

bool Properties::get(const char *name, Uint32 no,
                     const Properties **value) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = get(tmp, value);
  free(tmp);
  return res;
}

bool Properties::getCopy(const char *name, Uint32 no, char **value) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = getCopy(tmp, value);
  free(tmp);
  return res;
}

bool Properties::getCopy(const char *name, Uint32 no,
                         Properties **value) const {
  size_t tmp_len = strlen(name) + 20;
  char *tmp = (char *)malloc(tmp_len);
  BaseString::snprintf(tmp, tmp_len, "%s_%d", name, no);
  bool res = getCopy(tmp, value);
  free(tmp);
  return res;
}

void Properties::setCaseInsensitiveNames(bool value) {
  impl->setCaseInsensitiveNames(value);
}

bool Properties::getCaseInsensitiveNames() const { return impl->m_insensitive; }

template bool put(PropertiesImpl *, const char *, Uint32, bool);
template bool put(PropertiesImpl *, const char *, Uint64, bool);
template bool put(PropertiesImpl *, const char *, std::string_view, bool);
template bool put(PropertiesImpl *, const char *, const Properties *, bool);
