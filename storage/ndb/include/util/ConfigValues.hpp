/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#ifndef __CONFIG_VALUES_HPP
#define __CONFIG_VALUES_HPP

#include <ndb_types.h>
#include <UtilBuffer.hpp>
#include <ConfigSection.hpp>
#include <ConfigObject.hpp>

class ConfigValues : public ConfigObject {
  friend class ConfigValuesFactory;
  friend class ConfigObject;
  friend class ConfigSection;
public:
  ConfigValues();
  ~ConfigValues();
  
  class ConstIterator {
    friend class ConfigValuesFactory;
    const ConfigValues & m_cfg;
  public:
    ConfigSection *m_curr_section;
    ConstIterator(const ConfigValues&c) : m_cfg(c)
    {
      m_curr_section = nullptr;
    }
    
    /** 
      Set section and section instance. Return false if no matching section
      or instance was found.
    */
    bool openSection(Uint32 section_type, Uint32 index);

    /** Close current section so that you can open another.*/
    void closeSection();

    /** 
      Get entry within current section. Return false if 'key' was not found.
     */
    bool get(Uint32 key, Uint32 * value) const;
    bool get(Uint32 key, Uint64 * value) const;
    bool get(Uint32 key, const char ** value) const;
    bool getTypeOf(Uint32 key, ConfigSection::ValueType * type) const;
    
    Uint32 get(Uint32 key, Uint32 notFound) const;
    Uint64 get64(Uint32 key, Uint64 notFound) const;
    const char * get(Uint32 key, const char * notFound) const;
    ConfigSection::ValueType getTypeOf(Uint32 key) const;
  };

  class Iterator : public ConstIterator {
    ConfigValues & m_cfg;
  public:
    Iterator(ConfigValues&c) : ConstIterator(c), m_cfg(c) {}
    Iterator(ConfigValues&c, const ConstIterator& i):ConstIterator(c),m_cfg(c){
      m_curr_section = i.m_curr_section;
    }
    
    bool set(Uint32 key, Uint32 value);
    bool set(Uint32 key, Uint64 value);
    bool set(Uint32 key, const char * value);
  };
  Uint32 pack_v1(UtilBuffer&) const;
  Uint32 pack_v2(UtilBuffer&, Uint32 node_id = 0) const;

private:
  friend class Iterator;
  friend class ConstIterator;
};

class ConfigValuesFactory {
public:
  ConfigValues * m_cfg;
  ConfigValuesFactory();
  ConfigValuesFactory(ConfigValues * m_cfg);
  ~ConfigValuesFactory();

  ConfigValues * getConfigValues();

  bool begin();
  bool commit(bool only_sort);
  bool createSection(Uint32 section_type, Uint32 type);
  bool put(const ConfigSection::Entry &);
  bool put(Uint32 key, Uint32 value);
  bool put64(Uint32 key, Uint64 value);
  bool put(Uint32 key, const char * value);
  void closeSection();

  bool unpack_buf(const UtilBuffer&);
  bool unpack_v1_buf(const UtilBuffer&);
  bool unpack_v1(const Uint32* src, Uint32 len);
  bool unpack_v2_buf(const UtilBuffer&);
  bool unpack_v2(const Uint32* src, Uint32 len);
  void print_error_code() const;
  int  get_error_code() const;

  static ConfigValues *extractCurrentSection(const ConfigValues::ConstIterator &);
};

inline void
ConfigValuesFactory::print_error_code() const
{
  m_cfg->print_error_code();
}

inline int
ConfigValuesFactory::get_error_code() const
{
  return m_cfg->get_error_code();
}

inline bool
ConfigValues::Iterator::set(Uint32 key, Uint32 value)
{
  ConfigSection::Entry entry;
  entry.m_key = key;
  entry.m_type = ConfigSection::IntTypeId;
  entry.m_int = value;
  return m_cfg.set(m_curr_section, entry, true);
}

inline bool
ConfigValues::Iterator::set(Uint32 key, Uint64 value)
{
  ConfigSection::Entry entry;
  entry.m_key = key;
  entry.m_type = ConfigSection::Int64TypeId;
  entry.m_int64 = value;
  return m_cfg.set(m_curr_section, entry, true);
}

inline bool
ConfigValues::Iterator::set(Uint32 key, const char *value)
{
  ConfigSection::Entry entry;
  entry.m_key = key;
  entry.m_type = ConfigSection::StringTypeId;
  entry.m_string = value;
  return m_cfg.set(m_curr_section, entry, true);
}

inline bool
ConfigValuesFactory::begin()
{
  return m_cfg->begin();
}

inline bool
ConfigValuesFactory::commit(bool only_sort)
{
  return m_cfg->commitConfig(only_sort);
}

inline bool
ConfigValuesFactory::unpack_v1(const Uint32 *src, Uint32 len)
{
  return m_cfg->unpack_v1(src, len);
}

inline bool
ConfigValuesFactory::unpack_v2(const Uint32 *src, Uint32 len)
{
  return m_cfg->unpack_v2(src, len);
}

inline
bool
ConfigValues::ConstIterator::get(Uint32 key, Uint32 * value) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return false;
  }
  if (likely(tmp.m_type == ConfigSection::IntTypeId))
  {
    *value = tmp.m_int;
    return true;
  }
  return false;
}

inline
bool
ConfigValues::ConstIterator::get(Uint32 key, Uint64 * value) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return false;
  }
  if (likely(tmp.m_type == ConfigSection::Int64TypeId))
  {
    *value = tmp.m_int64;
    return true;
  }
  return false;
}

inline
bool
ConfigValues::ConstIterator::get(Uint32 key, const char ** value) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return false;
  }
  if (likely(tmp.m_type == ConfigSection::StringTypeId))
  {
    *value = tmp.m_string;
    return true;
  }
  return false;
}

inline
bool 
ConfigValues::ConstIterator::getTypeOf(Uint32 key,
                                       ConfigSection::ValueType * type) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return false;
  }
  *type = tmp.m_type;
  return true;
}

inline
Uint32
ConfigValues::ConstIterator::get(Uint32 key, Uint32 notFound) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return notFound;
  }
  if (likely(tmp.m_type == ConfigSection::IntTypeId))
  {
    return tmp.m_int;
  }
  return notFound;
}

inline
Uint64
ConfigValues::ConstIterator::get64(Uint32 key, Uint64 notFound) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return notFound;
  }
  if (likely(tmp.m_type == ConfigSection::Int64TypeId))
  {
    return tmp.m_int64;
  }
  return notFound;
}

inline
const char *
ConfigValues::ConstIterator::get(Uint32 key, const char * notFound) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return notFound;
  }
  if (likely(tmp.m_type == ConfigSection::StringTypeId))
  {
    return tmp.m_string;
  }
  return notFound;
}

inline
ConfigSection::ValueType
ConfigValues::ConstIterator::getTypeOf(Uint32 key) const
{
  ConfigSection::Entry tmp;
  if (unlikely(!m_cfg.get(m_curr_section, key, tmp)))
  {
    return ConfigSection::InvalidTypeId;
  }
  return tmp.m_type;
}

inline
bool
ConfigValuesFactory::put(Uint32 key, Uint32 val)
{
  return m_cfg->put(key,val);
}

inline
bool
ConfigValuesFactory::put64(Uint32 key, Uint64 val)
{
  return m_cfg->put64(key,val);
}

inline
bool
ConfigValuesFactory::put(Uint32 key, const char * val)
{
  return m_cfg->put(key,val);
}

inline
Uint32
ConfigValues::pack_v1(UtilBuffer& buf) const
{
  Uint32 len = get_v1_packed_size();
  void * tmp = buf.append(len);
  if(unlikely(tmp == nullptr))
  {
    return 0;
  }
  ConfigObject::pack_v1((Uint32*)tmp, len);
  return len;
}

inline
Uint32
ConfigValues::pack_v2(UtilBuffer& buf, Uint32 node_id) const
{
  Uint32 len = get_v2_packed_size(node_id);
  void * tmp = buf.append(len);
  if(unlikely(tmp == nullptr))
  {
    return 0;
  }
  ConfigObject::pack_v2((Uint32*)tmp, len, node_id);
  return len;
}

inline
bool
ConfigValuesFactory::unpack_buf(const UtilBuffer& buf)
{
  if (OUR_V2_VERSION)
  {
    if (!m_cfg->unpack_v2((const Uint32*)buf.get_data(), buf.length()))
    {
      return m_cfg->unpack_v1((const Uint32*)buf.get_data(), buf.length());
    }
  }
  else
  {
    if (!m_cfg->unpack_v1((const Uint32*)buf.get_data(), buf.length()))
    {
      return m_cfg->unpack_v2((const Uint32*)buf.get_data(), buf.length());
    }
  }
  return true;
}

inline
bool
ConfigValuesFactory::unpack_v1_buf(const UtilBuffer& buf)
{
  return m_cfg->unpack_v1((const Uint32*)buf.get_data(), buf.length());
}

inline
bool
ConfigValuesFactory::unpack_v2_buf(const UtilBuffer& buf)
{
  return m_cfg->unpack_v2((const Uint32*)buf.get_data(), buf.length());
}
#endif
