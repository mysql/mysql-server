/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef ConfigSection_H
#define ConfigSection_H

#include "util/require.h"
#include <bitset>
#include <vector>

#define CONFIG_V2_MAGIC 0x87654321

class ConfigObject;

class ConfigSection
{
  /* Friends */
  friend class ConfigObject;
  friend class ConfigValues;

public: /* Public data types */
  enum ValueType {
    InvalidTypeId = 0,
    IntTypeId     = 1,
    StringTypeId  = 2,
    SectionTypeId = 3,
    Int64TypeId   = 4
  };

  enum SectionType {
    InvalidSectionTypeId = 0,
    DataNodeTypeId = 1,
    ApiNodeTypeId = 2,
    MgmNodeTypeId = 3,
    TcpTypeId = 4,
    ShmTypeId = 5,
    SystemSectionId = 6
  };

  class Entry
  {
  public:
    Entry();
    Uint32 get_v1_length() const;
    Uint32 get_v2_length() const;
    void create_v1_entry(Uint32**, Uint32) const;
    void create_v2_entry(Uint32**) const;
    bool equal(Entry*) const;
    Uint32 unpack_entry(const Uint32 **data);
    Uint32 m_key;
    ValueType m_type;
    union {
      Uint32 m_int;
      Uint64 m_int64;
      const char *m_string;
    };
  };

  struct UpdateEntry {
    Entry old_entry;
    Entry new_entry;
  };

  enum ConfigSectionType
  {
    InvalidConfigSection = 0,
    BaseSection = 1,
    NodePointerSection = 2,
    CommPointerSection = 3,
    SystemPointerSection = 4,
    NodeSection = 5,
    CommSection = 6,
    SystemSection = 7
  };

  static constexpr Uint32 CONFIG_MAX_KEY_COUNT = 999;
  using Key_bitset = std::bitset<CONFIG_MAX_KEY_COUNT>;

public: /* Public methods */
  ConfigSection();
  ConfigSection(ConfigObject*);
  ~ConfigSection();

  bool get(Uint32 key, Entry&);
  bool set(Entry &entry, bool free_string);
  
  Uint32 get_node_id();
  Uint32 get_first_node_id();
  Uint32 get_second_node_id();

  SectionType get_section_type();

  void get_keys(Key_bitset& keys) const;
private: /* Private methods */
  void sort();
  void set_node_ids(Entry*);
  ConfigSection* get_default_section() const;
  Uint32 get_section_type_value();
  void check_magic() const;
  Entry* find_key(Uint32 key) const;
  Uint32 find_index(Uint32 key);
  bool set_section_type(Entry& entry);
  void set_section_type(SectionType section_type);
  void set_config_section_type(ConfigSectionType type);
  void set_config_section_type();

  bool set_base_section();
  bool set_pointer_node_section();
  bool set_pointer_comm_section();
  bool set_pointer_system_section();

  bool is_pointer_section() const;
  bool is_pointer_node_section() const;
  bool is_pointer_comm_section() const;
  bool is_pointer_system_section() const;
  bool is_real_section() const;

  bool set_node_section();
  bool set_comm_section();
  bool set_system_section();

  ConfigSection *copy() const;
  ConfigSection *copy_no_primary_keys(const Key_bitset& keys) const;
  void copy_default(ConfigSection *default_cs);
  Entry* copy_entry(const Entry *copy_entry) const;
  void handle_default_section(ConfigSection *curr);

  Uint32 get_v1_length() const;
  Uint32 get_v2_length() const;
  void create_v1_section(Uint32**, Uint32);
  void create_v2_section(Uint32**) const;
  void create_v2_entry(Uint32 **v1_ptr, Entry* entry) const;

  bool unpack_entry(Entry *entry, const Uint32 **data);
  void unpack_section_header(const Uint32 **data,
                             Uint32 &header_len,
                             Uint32 &num_entries);
  bool unpack_section_entries(const Uint32 **data,
                              Uint32 header_len,
                              Uint32 num_entries);

  bool unpack_system_section(const Uint32 **data);
  bool unpack_node_section(const Uint32 **data);
  bool unpack_data_node_section(const Uint32 **data);
  bool unpack_api_node_section(const Uint32 **data);
  bool unpack_mgm_node_section(const Uint32 **data);
  bool unpack_tcp_section(const Uint32 **data);
  bool unpack_shm_section(const Uint32 **data);
  bool unpack_comm_section(const Uint32 **data);
  bool unpack_section(const Uint32 **data);

  Entry* getEntry(Uint32 index) const;
  Uint32 get_num_entries() const;
  bool set_string(Entry *update_entry,
                  Entry &input_entry,
                  bool free_string);
  void free_entry(Entry *entry);
  void verify_section();
  void set_node_id_from_keys();
  /* Static functions */
  static Uint32 read_v2_int_value(const Uint32 **src);
  static void create_v2_entry_key(Uint32 **v2_ptr,
                                  Uint32 type,
                                  Uint32 section_id);
  static void create_v1_entry_key(Uint32 **v1_ptr,
                                  Uint32 type,
                                  Uint32 key,
                                  Uint32 section_id);
  static void create_int_value(Uint32 **ptr, Uint32 val);
  static void set_checksum(Uint32 *packed_ptr, Uint32 len_words);
  static Uint32 loc_mod4_v1(Uint32);
  static Uint32 loc_mod4_v2(Uint32);
private: /* Private data */
  std::vector<Entry*> m_entry_array;
  Uint32 m_magic;
  ConfigObject *m_cfg_object;
  Uint32 m_num_entries;
  ConfigSectionType m_config_section_type;
  SectionType m_section_type;
  Uint32 m_node;
  Uint32 m_node1;
  Uint32 m_node2;
};

inline ConfigSection::Entry*
ConfigSection::getEntry(Uint32 index) const
{
  if (index >= m_num_entries)
    return nullptr;
  return m_entry_array[index];
}

inline Uint32
ConfigSection::get_num_entries() const
{
  return m_num_entries;
}

inline ConfigSection::SectionType
ConfigSection::get_section_type()
{
  return m_section_type;
}

inline void
ConfigSection::check_magic() const
{
  require(m_magic == CONFIG_V2_MAGIC);
}

inline bool
ConfigSection::set_base_section()
{
  check_magic();
  if (m_config_section_type == InvalidConfigSection)
  {
    m_config_section_type = BaseSection;
    return true;
  }
  if (m_config_section_type != BaseSection)
  {
    return false;
  }
  return true;
}

inline bool
ConfigSection::set_pointer_node_section()
{
  check_magic();
  if (m_config_section_type == InvalidConfigSection)
  {
    m_config_section_type = NodePointerSection;
    return true;
  }
  return false;
}

inline bool
ConfigSection::set_pointer_comm_section()
{
  check_magic();
  if (m_config_section_type == InvalidConfigSection)
  {
    m_config_section_type = CommPointerSection;
    return true;
  }
  return false;
}

inline bool
ConfigSection::set_pointer_system_section()
{
  check_magic();
  if (m_config_section_type == InvalidConfigSection)
  {
    m_config_section_type = SystemPointerSection;
    return true;
  }
  return false;
}

inline Uint32
ConfigSection::get_node_id()
{
  return m_node;
}

inline Uint32
ConfigSection::get_first_node_id()
{
  return m_node1;
}

inline Uint32
ConfigSection::get_second_node_id()
{
  return m_node2;
}

inline bool
ConfigSection::is_real_section() const
{
  check_magic();
  return ((m_config_section_type == NodeSection) ||
          (m_config_section_type == CommSection) ||
          (m_config_section_type == SystemSection));
}

inline bool
ConfigSection::is_pointer_section() const
{
  check_magic();
  return ((m_config_section_type == NodePointerSection) ||
          (m_config_section_type == CommPointerSection) ||
          (m_config_section_type == SystemPointerSection));
}

inline bool
ConfigSection::is_pointer_node_section() const
{
  check_magic();
  return (m_config_section_type == NodePointerSection);
}

inline bool
ConfigSection::is_pointer_comm_section() const
{
  check_magic();
  return (m_config_section_type == CommPointerSection);
}

inline bool
ConfigSection::is_pointer_system_section() const
{
  check_magic();
  return (m_config_section_type == SystemPointerSection);
}

inline bool
ConfigSection::set_node_section()
{
  check_magic();
  if (unlikely(m_config_section_type != InvalidConfigSection))
  {
    return false;
  }
  m_config_section_type = NodeSection;
  return true;
}

inline bool
ConfigSection::set_comm_section()
{
  check_magic();
  if (unlikely(m_config_section_type != InvalidConfigSection))
  {
    return false;
  }
  m_config_section_type = CommSection;
  return true;
}

inline bool
ConfigSection::set_system_section()
{
  check_magic();
  if (unlikely(m_config_section_type != InvalidConfigSection))
  {
    return false;
  }
  m_config_section_type = SystemSection;
  return true;
}

/**
 * Old binary config format layout of bits.
 *
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
#define OLD_KP_TYPE_MASK     (15)
#define OLD_KP_TYPE_SHIFT    (28)
#define OLD_KP_SECTION_MASK  (0x3FFF)
#define OLD_KP_SECTION_SHIFT (14)
#define OLD_KP_KEYVAL_MASK   (0x3FFF)
#define OLD_KP_KEYVAL_SHIFT  (0)
#define OLD_KP_MASK          (0x0FFFFFFF)

#define V2_TYPE_SHIFT (28)
#define V2_TYPE_MASK  (15)
#define V2_KEY_SHIFT  (0)
#define V2_KEY_MASK   (0x0FFFFFFF)

static const Uint32 CONFIG_KEY_PARENT = (OLD_KP_KEYVAL_MASK - 1);

static const Uint32 CONFIG_TYPE_OF_SECTION = 999;
static const Uint32 CONFIG_NODE_ID = 3;
static const Uint32 CONFIG_FIRST_NODE_ID = 400;
static const Uint32 CONFIG_SECOND_NODE_ID = 401;

static const Uint32 CONFIG_SECTION_SYSTEM = 1000;
static const Uint32 CONFIG_SECTION_NODE = 2000;
static const Uint32 CONFIG_SECTION_CONNECTION = 3000;

static const Uint32 DATA_NODE_TYPE = 0;
static const Uint32 API_NODE_TYPE = 1;
static const Uint32 MGM_NODE_TYPE = 2;

static const Uint32 TCP_TYPE = 0;
static const Uint32 SHM_TYPE = 1;

static const Uint32 CONF_SYSTEM_TYPE = 1000;
#endif
