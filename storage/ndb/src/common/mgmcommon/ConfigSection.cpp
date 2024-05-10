/*
   Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include <kernel_types.h>
#include <ndb_global.h>
#include <ndb_limits.h>
#include <stdlib.h>
#include <string.h>
#include <ConfigObject.hpp>
#include <ConfigSection.hpp>
#include <Properties.hpp>
#include "util/require.h"
#ifndef _WIN32
#include <arpa/inet.h>
#endif
#include <algorithm>
#include <bitset>

#include <EventLogger.hpp>

// #define DEBUG_MALLOC 1
#ifdef DEBUG_MALLOC
#define DEB_MALLOC(arglist)      \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_MALLOC(arglist) \
  do {                      \
  } while (0)
#endif

// #define DEBUG_UNPACK_V1 1
#ifdef DEBUG_UNPACK_V1
#define DEB_UNPACK_V1(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_UNPACK_V1(arglist) \
  do {                         \
  } while (0)
#endif

ConfigSection::ConfigSection(ConfigObject *cfg_object)
    : m_magic(CONFIG_V2_MAGIC),
      m_cfg_object(cfg_object),
      m_num_entries(0),
      m_config_section_type(InvalidConfigSection),
      m_section_type(InvalidSectionTypeId),
      m_node(0),
      m_node1(0),
      m_node2(0) {}

ConfigSection::ConfigSection()
    : m_magic(CONFIG_V2_MAGIC),
      m_cfg_object(nullptr),
      m_num_entries(0),
      m_config_section_type(InvalidConfigSection),
      m_section_type(InvalidSectionTypeId),
      m_node(0),
      m_node1(0),
      m_node2(0) {}

ConfigSection::~ConfigSection() {
  if (!is_real_section()) {
    /* Neither invalid config section or pointer sections should have any
     * entries.
     */
    require(m_entry_array.size() == 0);
    return;
  }
  require(m_entry_array.size() == m_num_entries);
  for (Uint32 i = 0; i < m_num_entries; i++) {
    Entry *entry = m_entry_array[i];
    free_entry(entry);
  }
}

ConfigSection::Entry::Entry() {
  m_string = nullptr;
  m_key = 0;
  m_type = IntTypeId;
}

void ConfigSection::free_entry(Entry *entry) {
  if (entry->m_type == StringTypeId) {
    DEB_MALLOC(("this(%p)free(%u) => %p", this, __LINE__, entry->m_string));
    free(const_cast<char *>(entry->m_string));
  }
  delete entry;
}

/* Static functions */
Uint32 ConfigSection::loc_mod4_v1(Uint32 len) { return len + (4 - (len % 4)); }

Uint32 ConfigSection::loc_mod4_v2(Uint32 len) {
  return len + ((4 - (len & 3)) & 3);
}

void ConfigSection::set_checksum(Uint32 *packed_ptr, Uint32 len_words) {
  Uint32 checksum = 0;
  for (Uint32 i = 0; i < (len_words - 1); i++) {
    checksum ^= htonl(packed_ptr[i]);
  }
  packed_ptr[len_words - 1] = htonl(checksum);
}

void ConfigSection::create_int_value(Uint32 **v1_ptr, Uint32 val) {
  val = htonl(val);
  Uint32 *curr_ptr = *v1_ptr;
  *curr_ptr = val;
  curr_ptr++;
  *v1_ptr = curr_ptr;
}

void ConfigSection::create_v1_entry_key(Uint32 **v1_ptr, Uint32 type,
                                        Uint32 key, Uint32 section_id) {
  require(key <= OLD_KP_KEYVAL_MASK);
  require(section_id <= OLD_KP_SECTION_MASK);
  require(type <= OLD_KP_TYPE_MASK);
  Uint32 val = type << OLD_KP_TYPE_SHIFT;
  val += (key << OLD_KP_KEYVAL_SHIFT);
  val += (section_id << OLD_KP_SECTION_SHIFT);
  create_int_value(v1_ptr, val);
}

void ConfigSection::create_v2_entry_key(Uint32 **v2_ptr, Uint32 type,
                                        Uint32 key) {
  require(type <= V2_TYPE_MASK);
  require(key <= V2_KEY_MASK);
  Uint32 val = (type << V2_TYPE_SHIFT);
  val += key;
  create_int_value(v2_ptr, val);
}

Uint32 ConfigSection::read_v2_int_value(const Uint32 **src) {
  const Uint32 *data = *src;
  *src = data + 1;
  return ntohl(*data);
}

/* ConfigSection::Entry functions */
Uint32 ConfigSection::Entry::get_v2_length() const {
  Uint32 len = 0;
  switch (m_type) {
    case IntTypeId: {
      len = 2;
      break;
    }
    case Int64TypeId: {
      len = 3;
      break;
    }
    case StringTypeId: {
      Uint32 str_len = strlen(m_string);
      len = 2 + (loc_mod4_v2(str_len + 1) / 4);
      break;
    }
    default: {
      require(false);
      break;
    }
  }
  return len;
}

Uint32 ConfigSection::Entry::get_v1_length() const {
  Uint32 len = 0;
  switch (m_type) {
    case IntTypeId: {
      len = 2;
      break;
    }
    case Int64TypeId: {
      len = 3;
      break;
    }
    case StringTypeId: {
      Uint32 str_len = strlen(m_string);
      len = 2 + (loc_mod4_v1(str_len + 1) / 4);
      break;
    }
    default: {
      require(false);
      break;
    }
  }
  return len;
}

void ConfigSection::Entry::create_v1_entry(Uint32 **v1_ptr,
                                           Uint32 section_id) const {
  switch (m_type) {
    case IntTypeId: {
      create_v1_entry_key(v1_ptr, IntTypeId, m_key, section_id);
      create_int_value(v1_ptr, m_int);
      break;
    }
    case Int64TypeId: {
      Uint64 val = m_int64;
      Uint32 low = Uint32(val & 0xFFFFFFFF);
      Uint32 high = Uint32(val >> 32);
      create_v1_entry_key(v1_ptr, Int64TypeId, m_key, section_id);
      create_int_value(v1_ptr, high);
      create_int_value(v1_ptr, low);
      break;
    }
    case StringTypeId: {
      Uint32 str_len = strlen(m_string);
      create_v1_entry_key(v1_ptr, StringTypeId, m_key, section_id);
      create_int_value(v1_ptr, str_len + 1);
      Uint32 str_word_len = loc_mod4_v1(str_len + 1) / 4;
      Uint32 *curr_ptr = *v1_ptr;
      memcpy(curr_ptr, m_string, str_len);
      /**
       * The memory is zeroed before creating these entries, so no need
       * to zero pad the string.
       */
      curr_ptr += str_word_len;
      *v1_ptr = curr_ptr;
      break;
    }
    default: {
      require(false);
      break;
    }
  }
}

void ConfigSection::Entry::create_v2_entry(Uint32 **v2_ptr) const {
  switch (m_type) {
    case IntTypeId: {
      create_v2_entry_key(v2_ptr, IntTypeId, m_key);
      create_int_value(v2_ptr, m_int);
      break;
    }
    case Int64TypeId: {
      Uint64 val = m_int64;
      Uint32 low = Uint32(val & 0xFFFFFFFF);
      Uint32 high = Uint32(val >> 32);
      create_v2_entry_key(v2_ptr, Int64TypeId, m_key);
      create_int_value(v2_ptr, high);
      create_int_value(v2_ptr, low);
      break;
    }
    case StringTypeId: {
      Uint32 str_len = strlen(m_string);
      create_v2_entry_key(v2_ptr, StringTypeId, m_key);
      create_int_value(v2_ptr, str_len + 1);
      Uint32 str_word_len = loc_mod4_v2(str_len + 1) / 4;
      Uint32 *curr_ptr = *v2_ptr;
      memcpy(curr_ptr, m_string, str_len);
      /**
       * The memory is zeroed before creating these entries, so no need
       * to zero pad the string.
       */
      curr_ptr += str_word_len;
      *v2_ptr = curr_ptr;
      break;
    }
    default: {
      require(false);
      break;
    }
  }
}
Uint32 ConfigSection::Entry::unpack_entry(const Uint32 **data) {
  Uint32 key = read_v2_int_value(data);
  Uint32 key_type = (key >> V2_TYPE_SHIFT) & V2_TYPE_MASK;
  ValueType type = (ValueType)key_type;
  key = (key >> V2_KEY_SHIFT) & V2_KEY_MASK;
  m_key = key;
  m_type = type;
  switch (type) {
    case IntTypeId: {
      m_int = read_v2_int_value(data);
      break;
    }
    case Int64TypeId: {
      Uint32 high = read_v2_int_value(data);
      Uint32 low = read_v2_int_value(data);
      Uint64 val = (Uint64(high) << 32) + Uint64(low);
      m_int64 = val;
      break;
    }
    case StringTypeId: {
      Uint32 inp_str_len = read_v2_int_value(data);
      Uint32 str_len = strlen((const char *)*data);
      if (inp_str_len != (str_len + 1)) {
        return WRONG_STRING_LENGTH;
      }
      char *str = (char *)malloc(inp_str_len);
      DEB_MALLOC(("(this: %p)malloc(%u) => %p", this, __LINE__, str));
      require(str != nullptr);
      const Uint32 *curr_ptr = *data;
      memcpy(str, curr_ptr, str_len);
      str[str_len] = 0;
      m_string = str;
      Uint32 str_len_words = loc_mod4_v2(inp_str_len) / 4;
      curr_ptr += str_len_words;
      *data = curr_ptr;
      break;
    }
    default: {
      return WRONG_VALUE_TYPE;
    }
  }
  return 0;
}

bool ConfigSection::Entry::equal(Entry *cmp) const {
  if (m_type != cmp->m_type || m_key != cmp->m_key) {
    return false;
  }
  if (m_type == IntTypeId) {
    return (m_int == cmp->m_int);
  } else if (m_type == Int64TypeId) {
    return (m_int64 == cmp->m_int64);
  } else if (m_type == StringTypeId) {
    Uint32 first_len = strlen(m_string);
    Uint32 second_len = strlen(cmp->m_string);
    if (first_len != second_len) {
      return false;
    }
    if (memcmp(m_string, cmp->m_string, first_len) != 0) {
      return false;
    }
  } else {
    require(false);
  }
  return true;
}

Uint32 ConfigSection::get_v2_length() const {
  Uint32 len = 3;
  for (Uint32 i = 0; i < m_num_entries; i++) {
    Entry *entry = m_entry_array[i];
    len += entry->get_v2_length();
  }
  return len;
}

ConfigSection *ConfigSection::get_default_section() const {
  switch (m_section_type) {
    case DataNodeTypeId: {
      return m_cfg_object->m_data_node_default_section;
    }
    case ApiNodeTypeId: {
      return m_cfg_object->m_api_node_default_section;
    }
    case MgmNodeTypeId: {
      return m_cfg_object->m_mgm_node_default_section;
    }
    case SystemSectionId: {
      return m_cfg_object->m_system_section;
      break;
    }
    case TcpTypeId: {
      return m_cfg_object->m_tcp_default_section;
    }
    case ShmTypeId: {
      return m_cfg_object->m_shm_default_section;
    }
    default: {
      require(false);
      break;
    }
  }
  return nullptr;
}

Uint32 ConfigSection::get_section_type_value() {
  Uint32 val = 0;
  switch (m_section_type) {
    case DataNodeTypeId: {
      val = DATA_NODE_TYPE;
      break;
    }
    case ApiNodeTypeId: {
      val = API_NODE_TYPE;
      break;
    }
    case MgmNodeTypeId: {
      val = MGM_NODE_TYPE;
      break;
    }
    case TcpTypeId: {
      val = TCP_TYPE;
      break;
    }
    case ShmTypeId: {
      val = SHM_TYPE;
      break;
    }
    case SystemSectionId: {
      val = CONFIG_SECTION_SYSTEM;
      break;
    }
    default: {
      require(false);
      break;
    }
  }
  return val;
}

static bool compare_entry_key(ConfigSection::Entry *first,
                              ConfigSection::Entry *second) {
  if (first == second) return false;
  if (first->m_key < second->m_key)
    return true;
  else if (first->m_key > second->m_key)
    return false;
  /* Two entries should never have the same key */
  require(false);
  return false;
}

Uint32 ConfigSection::get_v1_length() const {
  check_magic();
  // sorted entries in key key order
  std::vector<Entry *> sorted_entries(m_entry_array);
  std::sort(sorted_entries.begin(), sorted_entries.end(), compare_entry_key);
  ConfigSection *default_section = get_default_section();
  Uint32 len = 0;
  Uint32 my_inx = 0;
  Uint32 default_inx = 0;
  /**
   * In v1 we don't send any default sections. This means that we need
   * to merge the section with the default section to get the length of
   * the section when sent in v1 format.
   *
   * We do this by scanning them (they are both stored in key order when
   * in not updatable format. If we find a key that exists in this
   * section we will use that and otherwise we will use the default
   * key. If it exists in both the section and in the default section
   * we will use the value from the section. We will move to the next
   * key in an appropriate manner.
   */
  while (default_inx < default_section->m_num_entries ||
         my_inx < m_num_entries) {
    if ((default_inx >= default_section->m_num_entries) ||
        ((my_inx < m_num_entries) &&
         (sorted_entries[my_inx]->m_key <
          default_section->m_entry_array[default_inx]->m_key)))

    {
      len += sorted_entries[my_inx]->get_v1_length();
      my_inx++;
    } else if ((my_inx >= m_num_entries) ||
               (sorted_entries[my_inx]->m_key >
                default_section->m_entry_array[default_inx]->m_key)) {
      len += default_section->m_entry_array[default_inx]->get_v1_length();
      default_inx++;
    } else {
      len += sorted_entries[my_inx]->get_v1_length();
      my_inx++;
      default_inx++;
    }
  }
  require(my_inx == m_num_entries &&
          default_inx == default_section->m_num_entries);
  /**
   * Add two more entries for type of section and parent.
   */
  len += 4;
  return len;
}

void ConfigSection::create_v1_section(Uint32 **v1_ptr, Uint32 section_id) {
  check_magic();
  // sorted entries in key key order
  std::vector<Entry *> sorted_entries(m_entry_array);
  std::sort(sorted_entries.begin(), sorted_entries.end(), compare_entry_key);
  ConfigSection *default_section = get_default_section();

  Uint32 my_inx = 0;
  Uint32 default_inx = 0;
  /**
   * We perform the same merge join loop for creating v1 sections
   * as we did when calculating the length of the v1 section.
   * Both this section and the default section is sorted in key
   * key order and the array is packed such that there are no
   * holes in the array.
   */
  while (default_inx < default_section->m_num_entries ||
         my_inx < m_num_entries) {
    if ((default_inx >= default_section->m_num_entries) ||
        ((my_inx < m_num_entries) &&
         (sorted_entries[my_inx]->m_key <
          default_section->m_entry_array[default_inx]->m_key))) {
      Entry *my_entry = sorted_entries[my_inx];
      my_entry->create_v1_entry(v1_ptr, section_id);
      my_inx++;
    } else if ((my_inx >= m_num_entries) ||
               (sorted_entries[my_inx]->m_key >
                default_section->m_entry_array[default_inx]->m_key)) {
      Entry *default_entry = default_section->m_entry_array[default_inx];
      default_entry->create_v1_entry(v1_ptr, section_id);
      default_inx++;
    } else {
      Entry *my_entry = sorted_entries[my_inx];
      my_entry->create_v1_entry(v1_ptr, section_id);
      my_inx++;
      default_inx++;
    }
  }
  require(my_inx == m_num_entries &&
          default_inx == default_section->m_num_entries);
  {
    /**
     * Add type of section and parent (== 0) to be in accordance
     * with v1 format.
     */
    create_v1_entry_key(v1_ptr, IntTypeId, CONFIG_TYPE_OF_SECTION, section_id);

    Uint32 val = get_section_type_value();
    create_int_value(v1_ptr, val);

    create_v1_entry_key(v1_ptr, IntTypeId, CONFIG_KEY_PARENT, section_id);
    create_int_value(v1_ptr, 0);
  }
  return;
}

void ConfigSection::create_v2_section(Uint32 **v2_ptr) const {
  check_magic();
  create_int_value(v2_ptr, get_v2_length());
  create_int_value(v2_ptr, m_num_entries);
  create_int_value(v2_ptr, m_section_type);
  for (Uint32 i = 0; i < m_num_entries; i++) {
    Entry *entry = m_entry_array[i];
    entry->create_v2_entry(v2_ptr);
  }
}

ConfigSection::Entry *ConfigSection::find_key(Uint32 key) const {
  for (Uint32 i = 0; i < m_num_entries; i++) {
    Entry *curr_entry = m_entry_array[i];
    if (curr_entry->m_key == key) return curr_entry;
  }
  return nullptr;
}

bool ConfigSection::get(Uint32 key, Entry &entry) {
  check_magic();
  Entry *loc_entry = find_key(key);
  if (loc_entry != nullptr) {
    entry = *loc_entry;
    if (entry.m_type == StringTypeId && entry.m_string == nullptr) {
      entry.m_string = "";
    }
    return true;
  }
  return false;
}

void ConfigSection::set_config_section_type(
    ConfigSectionType config_section_type) {
  m_config_section_type = config_section_type;
}

void ConfigSection::set_section_type(SectionType section_type) {
  m_section_type = section_type;
}

bool ConfigSection::set_section_type(Entry &entry) {
  /**
   * This is a type of section identifier.
   * We will record this in the object and
   * not as a key value.
   */
  if (unlikely(entry.m_type != IntTypeId)) {
    m_cfg_object->m_error_code = WRONG_DATA_TYPE_OF_SECTION;
    return false;
  }
  Uint32 type = entry.m_int;
  if (m_config_section_type == NodeSection) {
    if (type == DATA_NODE_TYPE) {
      m_section_type = DataNodeTypeId;
    } else if (type == API_NODE_TYPE) {
      m_section_type = ApiNodeTypeId;
    } else if (type == MGM_NODE_TYPE) {
      m_section_type = MgmNodeTypeId;
    } else {
      m_cfg_object->m_error_code = WRONG_NODE_TYPE;
      return false;
    }
  } else if (m_config_section_type == CommSection) {
    if (type == TCP_TYPE) {
      m_section_type = TcpTypeId;
    } else if (type == SHM_TYPE) {
      m_section_type = ShmTypeId;
    } else {
      m_cfg_object->m_error_code = WRONG_COMM_TYPE;
      return false;
    }
  } else if (m_config_section_type == SystemSection) {
    m_section_type = SystemSectionId;
  } else {
    m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
    return false;
  }
  return true;
}

void ConfigSection::set_config_section_type() {
  switch (m_section_type) {
    case DataNodeTypeId:
    case ApiNodeTypeId:
    case MgmNodeTypeId: {
      m_config_section_type = NodeSection;
      break;
    }
    case TcpTypeId:
    case ShmTypeId: {
      m_config_section_type = CommSection;
      break;
    }
    case SystemSectionId: {
      m_config_section_type = SystemSection;
      break;
    }
    default: {
      require(false);
      break;
    }
  }
}

bool ConfigSection::set_string(Entry *update_entry, Entry &input_entry,
                               bool free_string) {
  if (input_entry.m_type == StringTypeId) {
    Uint32 len = strlen(input_entry.m_string);
    char *str = (char *)malloc(len + 1);
    DEB_MALLOC(("malloc(%u) => %p", __LINE__, str));
    if (unlikely(str == nullptr)) {
      m_cfg_object->m_error_code = MEMORY_ALLOC_ERROR;
      return false;
    }
    const char *free_str = update_entry->m_string;
    memcpy(str, input_entry.m_string, len);
    str[len] = 0;
    input_entry.m_string = str;
    if (free_string) {
      DEB_MALLOC(("free(%u) => %p", __LINE__, free_str));
      free(const_cast<char *>(free_str));
    }
  }
  return true;
}

bool ConfigSection::set(Entry &entry, bool free_string) {
  check_magic();
  if (entry.m_key == CONFIG_TYPE_OF_SECTION) {
    return set_section_type(entry);
  }
  Entry *found_entry = find_key(entry.m_key);
  if (found_entry == nullptr) {
    found_entry = new Entry;
    DEB_MALLOC(("new(%u) => %p", __LINE__, found_entry));
    if (unlikely(!set_string(found_entry, entry, false))) {
      DEB_MALLOC(("delete(%u) => %p", __LINE__, found_entry));
      delete found_entry;
      return false;
    }
    m_entry_array.push_back(found_entry);
    m_num_entries++;
  } else {
    if (unlikely(found_entry->m_type != entry.m_type)) {
      m_cfg_object->m_error_code = WRONG_DATA_TYPE_IN_SET;
      return false;
    }
    if (unlikely(!set_string(found_entry, entry, free_string))) {
      return false;
    }
  }
  (*found_entry) = entry;
  set_node_ids(found_entry);
  return true;
}

void ConfigSection::set_node_ids(ConfigSection::Entry *entry) {
  if (entry->m_key == CONFIG_NODE_ID && entry->m_type == IntTypeId) {
    m_node = entry->m_int;
  } else if (entry->m_key == CONFIG_FIRST_NODE_ID &&
             entry->m_type == IntTypeId) {
    m_node1 = entry->m_int;
  } else if (entry->m_key == CONFIG_SECOND_NODE_ID &&
             entry->m_type == IntTypeId) {
    m_node2 = entry->m_int;
  }
}

ConfigSection::Entry *ConfigSection::copy_entry(
    const ConfigSection::Entry *dup_entry) const {
  ConfigSection::Entry *new_entry = new Entry;
  *new_entry = *dup_entry;
  if (dup_entry->m_type == StringTypeId) {
    const char *str = strdup(dup_entry->m_string);
    require(str != nullptr);
    new_entry->m_string = str;
  }
  return new_entry;
}

void ConfigSection::copy_default(ConfigSection *def_cs) {
  require(def_cs->is_real_section());
  Uint32 def_num_entries = def_cs->m_num_entries;
  for (Uint32 i = 0; i < def_num_entries; i++) {
    Entry *def_entry = def_cs->m_entry_array[i];
    Uint32 def_key = def_entry->m_key;
    Entry *entry = find_key(def_key);
    if (entry == nullptr) {
      m_entry_array.push_back(copy_entry(def_entry));
      m_num_entries++;
    }
  }
  verify_section();
  sort();
}

void ConfigSection::verify_section() {
  switch (get_section_type()) {
    case ConfigSection::DataNodeTypeId:
    case ConfigSection::ApiNodeTypeId:
    case ConfigSection::MgmNodeTypeId: {
      require(m_config_section_type == NodeSection);
      Entry *entry = find_key(CONFIG_NODE_ID);
      require(entry != nullptr && m_node > 0 && entry->m_type == IntTypeId &&
              m_node == entry->m_int);
      break;
    }
    case ConfigSection::TcpTypeId:
    case ConfigSection::ShmTypeId: {
      require(m_config_section_type == CommSection);
      Entry *entry1 = find_key(CONFIG_FIRST_NODE_ID);
      Entry *entry2 = find_key(CONFIG_SECOND_NODE_ID);
      require(entry1 != nullptr && entry2 != nullptr && m_node1 > 0 &&
              m_node2 > 0 && entry1->m_type == IntTypeId &&
              entry2->m_type == IntTypeId && m_node1 == entry1->m_int &&
              m_node2 == entry2->m_int);
      break;
    }
    case ConfigSection::SystemSectionId:
      require(m_config_section_type == SystemSection);
      break;
    default:
      require(!is_real_section());
      require(m_entry_array.size() == 0);
      break;
  }
}

void ConfigSection::set_node_id_from_keys() {
  switch (get_section_type()) {
    case ConfigSection::DataNodeTypeId:
    case ConfigSection::ApiNodeTypeId:
    case ConfigSection::MgmNodeTypeId: {
      Entry *entry = find_key(CONFIG_NODE_ID);
      require(entry != nullptr && entry->m_type == IntTypeId);
      m_node = entry->m_int;
      break;
    }
    case ConfigSection::TcpTypeId:
    case ConfigSection::ShmTypeId: {
      Entry *entry1 = find_key(CONFIG_FIRST_NODE_ID);
      require(entry1 != nullptr && entry1->m_type == IntTypeId);
      Entry *entry2 = find_key(CONFIG_SECOND_NODE_ID);
      require(entry2 != nullptr && entry2->m_type == IntTypeId);
      m_node1 = entry1->m_int;
      m_node2 = entry2->m_int;
      break;
    }
    default: {
      break;
    }
  }
}

ConfigSection *ConfigSection::copy() const {
  ConfigSection *new_config_section = new ConfigSection(m_cfg_object);
  DEB_MALLOC(("new(%u) => %p", __LINE__, new_config_section));
  require(is_real_section());
  new_config_section->m_magic = this->m_magic;
  new_config_section->m_config_section_type = this->m_config_section_type;
  new_config_section->m_section_type = this->m_section_type;
  new_config_section->set_config_section_type();
  Uint32 num_entries = 0;
  for (Uint32 i = 0; i < m_num_entries; i++) {
    const Entry *curr_entry = m_entry_array[i];
    new_config_section->m_entry_array.push_back(copy_entry(curr_entry));
    num_entries++;
  }
  new_config_section->m_num_entries = num_entries;
  new_config_section->set_node_id_from_keys();
  new_config_section->verify_section();
  new_config_section->sort();
  return new_config_section;
}

ConfigSection *ConfigSection::copy_no_primary_keys(
    const Key_bitset &keys) const {
  ConfigSection *new_config_section = new ConfigSection(m_cfg_object);
  DEB_MALLOC(("new(%u) => %p", __LINE__, new_config_section));
  require(is_real_section());
  new_config_section->m_magic = this->m_magic;
  new_config_section->m_config_section_type = this->m_config_section_type;
  new_config_section->m_section_type = this->m_section_type;
  new_config_section->set_config_section_type();
  Uint32 num_entries = 0;
  for (Uint32 i = 0; i < m_num_entries; i++) {
    const Entry *curr_entry = m_entry_array[i];
    Uint32 key = curr_entry->m_key;
    /* The node id parameters are primary keys for section they belongs to,
     * never copy them.
     */
    if (keys[key] && key != CONFIG_NODE_ID && key != CONFIG_FIRST_NODE_ID &&
        key != CONFIG_SECOND_NODE_ID) {
      new_config_section->m_entry_array.push_back(copy_entry(curr_entry));
      num_entries++;
    }
  }
  new_config_section->m_num_entries = num_entries;

  // Clear member copies of node ids since they are not copied.
  new_config_section->m_node = 0;
  new_config_section->m_node1 = 0;
  new_config_section->m_node2 = 0;

  /* Since node ids are missing this section can not in general be verified by
   * verify_section().
   */

  new_config_section->sort();
  return new_config_section;
}

void ConfigSection::handle_default_section(ConfigSection *default_section) {
  /**
   * For each entry in this ConfigSection object we will check
   * if the entry is in the default section with the same value.
   * If so we will remove the entry from this object since it
   * is equal to the default value.
   * Some special configuration parameters we will never treat
   * as default.
   */
  Uint32 new_num_entries = 0;
  std::vector<Entry *> new_entry_array;
  for (Uint32 i = 0; i < m_num_entries; i++) {
    Entry *curr_entry = m_entry_array[i];
    Uint32 key = curr_entry->m_key;
    Entry *default_entry = default_section->find_key(key);
    if (default_entry != nullptr && curr_entry->equal(default_entry)) {
      /**
       * We can remove the current entry from the ConfigSection
       * object since it is a duplicate of what is in the
       * default section for this entry.
       */
      free_entry(curr_entry);
    } else {
      new_entry_array.push_back(curr_entry);
      new_num_entries++;
    }
  }
  m_num_entries = new_num_entries;
  m_entry_array.clear();
  m_entry_array = new_entry_array;
  m_entry_array.shrink_to_fit();
  verify_section();
  sort();
}

void ConfigSection::sort() {
  m_entry_array.shrink_to_fit();
  std::sort(m_entry_array.begin(), m_entry_array.end(), compare_entry_key);
}

void ConfigSection::unpack_section_header(const Uint32 **data,
                                          Uint32 &header_len,
                                          Uint32 &num_entries) {
  header_len = read_v2_int_value(data);
  num_entries = read_v2_int_value(data);
  m_section_type = (SectionType)read_v2_int_value(data);
}

bool ConfigSection::unpack_section_entries(const Uint32 **data,
                                           Uint32 header_len,
                                           Uint32 num_entries) {
  require(m_num_entries == 0);
  if (num_entries != 0) {
    for (Uint32 i = 0; i < num_entries; i++) {
      m_entry_array.push_back(new Entry);
      m_num_entries++;
      Uint32 ret_code = m_entry_array[i]->unpack_entry(data);
      if (unlikely(ret_code != 0)) {
        m_cfg_object->m_error_code = ret_code;
        return false;
      }
      set_node_ids(m_entry_array[i]);
    }
  } else if (header_len != 3) {
    m_cfg_object->m_error_code = WRONG_EMPTY_SECTION_LENGTH;
    return false;
  } else {
    m_num_entries = 0;
  }
  return true;
}

bool ConfigSection::unpack_system_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  if (unlikely(m_section_type != SystemSectionId)) {
    m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
    require(false);
    return false;
  }
  require(set_system_section());
  return unpack_section_entries(data, header_len, num_entries);
}

bool ConfigSection::unpack_node_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  switch (m_section_type) {
    case DataNodeTypeId:
    case ApiNodeTypeId:
    case MgmNodeTypeId: {
      break;
    }
    default: {
      require(false);
      m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
      return false;
    }
  }
  require(set_node_section());
  return unpack_section_entries(data, header_len, num_entries);
}

bool ConfigSection::unpack_data_node_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  if (unlikely(m_section_type != DataNodeTypeId)) {
    m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
    require(false);
    return false;
  }
  require(set_node_section());
  return unpack_section_entries(data, header_len, num_entries);
}

bool ConfigSection::unpack_api_node_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  if (unlikely(m_section_type != ApiNodeTypeId)) {
    require(false);
    m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
    return false;
  }
  require(set_node_section());
  return unpack_section_entries(data, header_len, num_entries);
}

bool ConfigSection::unpack_mgm_node_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  if (unlikely(m_section_type != MgmNodeTypeId)) {
    require(false);
    m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
    return false;
  }
  require(set_node_section());
  return unpack_section_entries(data, header_len, num_entries);
}

bool ConfigSection::unpack_tcp_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  if (unlikely(m_section_type != TcpTypeId)) {
    m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
    require(false);
    return false;
  }
  require(set_comm_section());
  return unpack_section_entries(data, header_len, num_entries);
}

bool ConfigSection::unpack_shm_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  if (unlikely(m_section_type != ShmTypeId)) {
    m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
    require(false);
    return false;
  }
  require(set_comm_section());
  return unpack_section_entries(data, header_len, num_entries);
}

bool ConfigSection::unpack_comm_section(const Uint32 **data) {
  Uint32 header_len = 0;
  Uint32 num_entries = 0;
  unpack_section_header(data, header_len, num_entries);
  switch (m_section_type) {
    case TcpTypeId:
    case ShmTypeId: {
      break;
    }
    default: {
      m_cfg_object->m_error_code = WRONG_SECTION_TYPE;
      require(false);
      return false;
    }
  }
  require(set_comm_section());
  return unpack_section_entries(data, header_len, num_entries);
}

void ConfigSection::get_keys(Key_bitset &keys) const {
  for (Uint32 i = 0; i < m_num_entries; i++) {
    Entry *entry = m_entry_array[i];
    keys.set(entry->m_key);
  }
}
