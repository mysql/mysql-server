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

#include "util/require.h"
#include <ndb_global.h>
#include <string.h>
#include <kernel_types.h>
#include <Properties.hpp>
#include <ndb_limits.h>
#include <ConfigSection.hpp>
#include "ConfigObject.hpp"
#include "portlib/ndb_socket.h"  // ntohl()
#include <stdlib.h>
#include <algorithm>
#include <EventLogger.hpp>

//#define DEBUG_MALLOC 1
#ifdef DEBUG_MALLOC
#define DEB_MALLOC(arglist) do { g_eventLogger->info arglist; } while (0)
#else
#define DEB_MALLOC(arglist) do { } while (0)
#endif

//#define DEBUG_UNPACK_V1 1
#ifdef DEBUG_UNPACK_V1
#define DEB_UNPACK_V1(arglist) do { g_eventLogger->info arglist; } while (0)
#else
#define DEB_UNPACK_V1(arglist) do { } while (0)
#endif


static const char Magic_v1[] = { 'N', 'D', 'B', 'C', 'O', 'N', 'F', 'V' };
static const char Magic_v2[] = { 'N', 'D', 'B', 'C', 'O', 'N', 'F', '2' };

ConfigObject::ConfigObject() :
  m_curr_cfg_section(nullptr),
  m_num_sections(0),
  m_system_section(nullptr),
  m_num_node_sections(0),
  m_num_data_nodes(0),
  m_num_api_nodes(0),
  m_num_mgm_nodes(0),
  m_num_comm_sections(0),
  m_data_node_default_section(nullptr),
  m_api_node_default_section(nullptr),
  m_mgm_node_default_section(nullptr),
  m_tcp_default_section(nullptr),
  m_shm_default_section(nullptr),
  m_error_code(0)
{
}

ConfigObject::~ConfigObject()
{
  for (Uint32 i = 0; i < m_num_sections; i++)
  {
    DEB_MALLOC(("delete(%u) => %p", __LINE__, m_cfg_sections[i]));
    delete m_cfg_sections[i];
  }
  DEB_UNPACK_V1(("Free default data node section"));
  DEB_MALLOC(("delete(%u) => %p", __LINE__, m_data_node_default_section));
  delete m_data_node_default_section;
  DEB_UNPACK_V1(("Free default api node section"));
  DEB_MALLOC(("delete(%u) => %p", __LINE__, m_api_node_default_section));
  delete m_api_node_default_section;
  DEB_UNPACK_V1(("Free default mgm node section"));
  DEB_MALLOC(("delete(%u) => %p", __LINE__, m_mgm_node_default_section));
  delete m_mgm_node_default_section;
  DEB_UNPACK_V1(("Free default tcp section"));
  DEB_MALLOC(("delete(%u) => %p", __LINE__, m_tcp_default_section));
  delete m_tcp_default_section;
  DEB_UNPACK_V1(("Free default shm section"));
  DEB_MALLOC(("delete(%u) => %p", __LINE__, m_shm_default_section));
  delete m_shm_default_section;
}

ConfigObject*
ConfigObject::copy_current(ConfigSection *curr_section) const
{
  ConfigObject *new_co = new ConfigObject();
  DEB_MALLOC(("new(%u) => %p", __LINE__, new_co));
  ConfigSection *new_cs = curr_section->copy();
  if (new_cs == nullptr)
  {
    DEB_MALLOC(("delete(%u) => %p", __LINE__, new_co));
    delete new_co;
  }
  new_co->m_cfg_sections.push_back(new_cs);
  new_co->m_num_sections = 1;
  new_co->m_curr_cfg_section = new_cs;
  new_co->m_num_default_sections = 0;

  ConfigSection::SectionType section_type = curr_section->get_section_type();
  switch (section_type)
  {
    case ConfigSection::DataNodeTypeId:
    {
      new_co->m_num_data_nodes = 1;
      new_co->m_num_node_sections = 1;
      new_co->m_node_sections.push_back(new_cs);
      if (m_data_node_default_section != nullptr)
      {
        new_cs->copy_default(m_data_node_default_section);
      }
      break;
    }
    case ConfigSection::ApiNodeTypeId:
    {
      new_co->m_num_api_nodes = 1;
      new_co->m_num_node_sections = 1;
      new_co->m_node_sections.push_back(new_cs);
      if (m_api_node_default_section != nullptr)
      {
        new_cs->copy_default(m_api_node_default_section);
      }
      break;
    }
    case ConfigSection::MgmNodeTypeId:
    {
      new_co->m_num_mgm_nodes = 1;
      new_co->m_num_node_sections = 1;
      new_co->m_node_sections.push_back(new_cs);
      if (m_mgm_node_default_section != nullptr)
      {
        new_cs->copy_default(m_mgm_node_default_section);
      }
      break;
    }
    case ConfigSection::TcpTypeId:
    {
      new_co->m_num_comm_sections = 1;
      new_co->m_comm_sections.push_back(new_cs);
      if (m_tcp_default_section != nullptr)
      {
        new_cs->copy_default(m_tcp_default_section);
      }
      break;
    }
    case ConfigSection::ShmTypeId:
    {
      new_co->m_num_comm_sections = 1;
      new_co->m_comm_sections.push_back(new_cs);
      if (m_shm_default_section != nullptr)
      {
        new_cs->copy_default(m_shm_default_section);
      }
      break;
    }
    case ConfigSection::SystemSectionId:
    {
      new_co->m_system_section = new_cs;
      break;
    }
    default:
    {
      return nullptr;
    }
  }
  return new_co;
}

void
ConfigObject::createSections(Uint32 num_sections)
{
  m_num_sections = num_sections;
  for (Uint32 i = 0; i < num_sections; i++)
  {
    m_cfg_sections.push_back(new ConfigSection(this));
    DEB_MALLOC(("new(%u) => %p", __LINE__, m_cfg_sections[i]));
  }
}

bool
ConfigObject::createSection(Uint32 section_type, Uint32 type)
{
  ConfigSection *cs = new ConfigSection(this);
  DEB_MALLOC(("new(%u) => %p", __LINE__, cs));
  if (section_type == CONFIG_SECTION_NODE)
  {
    if (type == DATA_NODE_TYPE)
    {
      cs->set_section_type(ConfigSection::DataNodeTypeId);
      cs->set_config_section_type(ConfigSection::NodeSection);
    }
    else if (type == API_NODE_TYPE)
    {
      cs->set_section_type(ConfigSection::ApiNodeTypeId);
      cs->set_config_section_type(ConfigSection::NodeSection);
    }
    else if (type == MGM_NODE_TYPE)
    {
      cs->set_section_type(ConfigSection::MgmNodeTypeId);
      cs->set_config_section_type(ConfigSection::NodeSection);
    }
    else
    {
      DEB_MALLOC(("delete(%u) => %p", __LINE__, cs));
      delete cs;
      m_error_code = WRONG_NODE_TYPE;
      return false;
    }
  }
  else if (section_type == CONFIG_SECTION_CONNECTION)
  {
    if (type == TCP_TYPE)
    {
      cs->set_section_type(ConfigSection::TcpTypeId);
      cs->set_config_section_type(ConfigSection::CommSection);
    }
    else if (type == SHM_TYPE)
    {
      cs->set_section_type(ConfigSection::ShmTypeId);
      cs->set_config_section_type(ConfigSection::CommSection);
    }
    else
    {
      DEB_MALLOC(("delete(%u) => %p", __LINE__, cs));
      delete cs;
      m_error_code = WRONG_COMM_TYPE;
      return false;
    }
  }
  else if (section_type == CONFIG_SECTION_SYSTEM)
  {
    cs->set_section_type(ConfigSection::SystemSectionId);
    cs->set_config_section_type(ConfigSection::SystemSection);
  }
  else
  {
    DEB_MALLOC(("delete(%u) => %p", __LINE__, cs));
    delete cs;
    m_error_code = WRONG_SECTION_TYPE;
    return false;
  }
  m_curr_cfg_section = cs;
  m_cfg_sections.push_back(cs);
  m_num_sections++;
  return true;
}

ConfigSection*
ConfigObject::openSection(Uint32 section_type, Uint32 index) const
{
  ConfigSection *cs = nullptr;
  if (section_type == 0)
  {
    if (index >= m_num_sections)
    {
      return nullptr;
    }
    cs = m_cfg_sections[index];
  }
  else if (section_type == CONFIG_SECTION_NODE)
  {
    if (index >= m_num_node_sections)
    {
      return nullptr;
    }
    cs = m_node_sections[index];
  }
  else if (section_type == CONFIG_SECTION_CONNECTION)
  {
    if (index >= m_num_comm_sections)
    {
      return nullptr;
    }
    cs = m_comm_sections[index];
  }
  else if (section_type == CONFIG_SECTION_SYSTEM)
  {
    if (unlikely(index > 0))
    {
      return nullptr;
    }
    cs = m_system_section;
  }
  else
  {
    return nullptr;
  }
  return cs;
}

void
ConfigObject::closeSection()
{
  m_curr_cfg_section = nullptr;
}

bool
ConfigObject::set(ConfigSection *curr_section,
                  ConfigSection::Entry &entry,
                  bool free_string)
{
  if (unlikely(curr_section == nullptr))
  {
    return false;
  }
  return curr_section->set(entry, free_string);
}

Uint32
ConfigObject::getNextEntry(Uint32 key, ConfigSection::Entry *in_entry) const
{
  require(m_num_sections == 1);
  require(m_num_default_sections == 0);
  {
    ConfigSection *cs = m_cfg_sections[0];
    Uint32 num_entries = cs->get_num_entries();
    if (key < num_entries)
    {
      ConfigSection::Entry *entry = cs->getEntry(key);
      *in_entry = *entry;
      return key + 1;
    }
  }
  return 0;
}

bool
ConfigObject::get(ConfigSection *curr_section,
                  Uint32 key,
                  ConfigSection::Entry &entry) const
{
  if (unlikely(curr_section == nullptr))
  {
    return false;
  }
  if (key == CONFIG_TYPE_OF_SECTION)
  {
    Uint32 type = curr_section->get_section_type_value();
    entry.m_type = ConfigSection::IntTypeId;
    entry.m_key = CONFIG_TYPE_OF_SECTION;
    entry.m_int = type;
    return true;
  }
  if (curr_section->get(key, entry))
  {
    return true;
  }
  ConfigSection *cs = curr_section->get_default_section();
  return cs->get(key, entry);
}

bool
ConfigObject::put(Uint32 key, Uint32 val)
{
  //g_eventLogger->info("put(%u, %u)", key, val);
  ConfigSection::Entry entry;
  entry.m_key = key;
  entry.m_type = ConfigSection::IntTypeId;
  entry.m_int = val;
  return m_curr_cfg_section->set(entry, false);
}

bool
ConfigObject::put64(Uint32 key, Uint64 val)
{
  //g_eventLogger->info("put(%u, %llu)", key, val);
  ConfigSection::Entry entry;
  entry.m_key = key;
  entry.m_type = ConfigSection::Int64TypeId;
  entry.m_int64 = val;
  return m_curr_cfg_section->set(entry, false);
}

bool
ConfigObject::put(Uint32 key, const char *str)
{
  //g_eventLogger->info("put(%u, %s)", key, str);
  ConfigSection::Entry entry;
  entry.m_key = key;
  entry.m_type = ConfigSection::StringTypeId;
  entry.m_string = str;
  return m_curr_cfg_section->set(entry, false);
}

int
ConfigObject::get_error_code() const
{
  return (int)m_error_code;
}

void
ConfigObject::print_error_code() const
{
  g_eventLogger->info("ConfigObject::m_error_code = %u", m_error_code);
}

bool
ConfigObject::unpack_v1(const Uint32 *src, Uint32 len)
{
  m_error_code = 0;
  /**
   * 1) Check that Magic 8 bytes at start are correct.
   * 2) Check that checksum is correct.
   * 3) Scan through the config binary to discover the
   *    number of sections. We will use this number to
   *    create the desired number of ConfigSection
   *    objects.
   * 4) Create an array to store the ConfigSection objects
   *    in and create a ConfigSection for each entry in the
   *    array.
   * 5) Read all entries in the configuration binary and
   *    insert them into the correct ConfigSection object.
   *
   * When we reach a SectionType key we will treat it
   * in a special manner. This is a special section that
   * only describes the sections of a certain type.
   * First we get the node sections listed in section 1.
   * Thus section 1 is not a section in the sense that
   * v2 of the binary format uses. It is a descriptive
   * context section that in v2 is solved through a
   * section header instead.
   *
   * After listing all node sections we will have a
   * descriptive section for the system section (there is
   * always just one of the system section).
   * After the system section we get a listing of all
   * communication sections.
   *
   * The code doesn't make use of this order since each
   * SectionType entry contains a key that is either
   * CONFIG_SECTION_SYSTEM = 1000 for system section,
   * CONFIG_SECTION_NODE = 2000 for nodes,
   * CONFIG_SECTION_CONNECTION = 3000 for communication sections.
   *
   * This means that we will always have setup the ConfigSection
   * object to know what type of section it is before the keys
   * are set in it. This is particularly important to prepare for
   * the key CFG_TYPE_OF_SECTION = 999. This is 0,1,2 for nodes
   * where 0 = data node, 1 = API node and 2 is MGM node. But for
   * communication section 0 means TCP/IP communication section
   * and 1 means shared memory section. Thus it isn't possible
   * to interpret those values unless one knows what section
   * variant we are.
   *
   * 6) To decrease the size of the configuration we create
   *    default objects of all concerned types (data node,
   *    API nodes, MGM nodes, TCP sections and SHM sections).
   *    Since this will remove a lot of keys from ConfigSection's
   *    we will perform this task before we commit the
   *    ConfigSection's.
   *
   * 7) The final step we perform is calling commit on all
   *    ConfigSection objects. This will optimise the data
   *    structure in the ConfigSection object for fast reading
   *    of the data. This includes the newly created default
   *    sections.
   *
   * 8) We also call commit on the ConfigObject to create
   *    fast paths to all nodes to get to the
   *    communication objects quickly.
   */
  if (unlikely(len < sizeof(Magic_v1) + 4))
  {
    m_error_code = WRONG_MAGIC_SIZE;
    return false;
  }
  if (unlikely(memcmp(src, Magic_v1, sizeof(Magic_v1)) != 0))
  {
    m_error_code = WRONG_MAGIC_CONTENT;
    return false;
  }
  const char *data = (const char *)src;
  const char *end = data + len - 4;
  data += sizeof(Magic_v1);
  
  {
    /* 2) above */
    Uint32 len32 = (len >> 2);
    const Uint32 * tmp = (const Uint32*)src;
    Uint32 chk = 0;
    for (Uint32 i = 0; (i+1) < len32; i++)
    {
      chk ^= ntohl(tmp[i]);
    }
    if (unlikely(chk != ntohl(tmp[len32-1])))
    {
      m_error_code = WRONG_CHECKSUM;
      return false;
    }
  }

  const char *save = data;
  Uint32 num_sections = 0;
  {
    /* 3) above */
    while(end - data > 4)
    {
      Uint32 tmp = ntohl(* (const Uint32 *)data);
      Uint32 section_id = get_old_section(tmp);
      if ((section_id + 1) > num_sections)
      {
        num_sections = section_id + 1;
      }
      data += 4;
      switch(get_old_type(tmp))
      {
        case ConfigSection::SectionTypeId:
        case ConfigSection::IntTypeId:
        {
	  data += 4;
	  break;
        }
        case ConfigSection::Int64TypeId:
        {
	  data += 8;
	  break;
        }
        case ConfigSection::StringTypeId:
        {
          Uint32 s_len = ntohl(* (const Uint32 *)data);
	  data += 4 + ConfigSection::loc_mod4_v1(s_len);
	  break;
        }
        default:
        {
          m_error_code = WRONG_ENTRY_TYPE;
          return false;
        }
      }
    }
  }
  data = save;

  /* 4) above */
  createSections(num_sections);

  /* 5) above */
  ConfigSection::Entry entry;
  bool section_type;
  while(end - data > 4)
  {
    Uint32 tmp = ntohl(* (const Uint32 *)data);
    data += 4;
    entry.m_key = get_old_key(tmp);
    entry.m_type = get_old_type(tmp);
    Uint32 curr_sect = get_old_section(tmp);
    section_type = false;
    DEB_UNPACK_V1(("type: %u, key: %u, section: %u",
                   get_old_type(tmp),
                   get_old_key(tmp),
                   get_old_section(tmp)));
    switch(entry.m_type)
    {
      case ConfigSection::SectionTypeId:
      {
        section_type = true;
        entry.m_int = ntohl(* (const Uint32 *)data);
        DEB_UNPACK_V1(("SectionType: %u", entry.m_int));
        data += 4;
        break;
      }
      case ConfigSection::IntTypeId:
      {
        entry.m_int = ntohl(* (const Uint32 *)data);
        DEB_UNPACK_V1(("IntType: %u", entry.m_int));
        data += 4;
        break;
      }
      case ConfigSection::Int64TypeId:
      {
        Uint64 hi = ntohl(* (const Uint32 *)data);
        data += 4;
        Uint64 lo = ntohl(* (const Uint32 *)data);
        data += 4;
        entry.m_int64 = (hi << 32) | lo;
        DEB_UNPACK_V1(("Int64Type: %llu", entry.m_int64));
        break;
      }
      case ConfigSection::StringTypeId:
      {
        Uint32 s_len = ntohl(* (const Uint32 *)data);
        data += 4;
        Uint32 s_len2 = strlen(data);
        if (unlikely(s_len2 + 1 != s_len))
        {
          m_error_code = WRONG_STRING_LENGTH;
	  return false;
        }
        entry.m_string = data;
        DEB_UNPACK_V1(("StringType(%p): %s, strlen: %u",
                       entry.m_string,
                       entry.m_string,
                       s_len2));
        data += ConfigSection::loc_mod4_v1(s_len);
        break;
      }
      default:
      {
        m_error_code = WRONG_ENTRY_TYPE;
        return false;
      }
    }
    ConfigSection *curr_cfg_sect = m_cfg_sections[curr_sect];
    if (section_type)
    {
      /**
       * Section types list a section that is going to
       * contain valid information.
       * The section listing those will not contain any
       * valid information and thus should not accept any
       * entries.
       */
      Uint32 ref_sect = 
        (entry.m_int >> OLD_KP_SECTION_SHIFT) & OLD_KP_SECTION_MASK;
      if (unlikely(!curr_cfg_sect->set_base_section()))
      {
        m_error_code = SET_NOT_REAL_SECTION_ERROR;
        return false;
      }
      if (entry.m_key == CONFIG_SECTION_NODE)
      {
        DEB_UNPACK_V1(("ref_sect: %u set as pointer node", ref_sect));
        if (unlikely(!m_cfg_sections[ref_sect]->set_pointer_node_section()))
        {
          m_error_code = SET_NODE_SECTION_ERROR;
          return false;
        }
      }
      else if (entry.m_key == CONFIG_SECTION_CONNECTION)
      {
        DEB_UNPACK_V1(("ref_sect: %u set as pointer comm", ref_sect));
        if (unlikely(!m_cfg_sections[ref_sect]->set_pointer_comm_section()))
        {
          m_error_code = SET_CONNECTION_SECTION_ERROR;
          return false;
        }
      }
      else if (entry.m_key == CONFIG_SECTION_SYSTEM)
      {
        DEB_UNPACK_V1(("ref_sect: %u set as pointer system", ref_sect));
        if (unlikely(!m_cfg_sections[ref_sect]->set_pointer_system_section()))
        {
          m_error_code = SET_SYSTEM_SECTION_ERROR;
          return false;
        }
      }
      else
      {
        m_error_code = UNDEFINED_SECTION_TYPE;
        return false;
      }
    }
    else if (curr_cfg_sect->is_pointer_section())
    {
      Uint32 ref_sect = 
        (entry.m_int >> OLD_KP_SECTION_SHIFT) & OLD_KP_SECTION_MASK;
      if (curr_cfg_sect->is_pointer_node_section())
      {
        DEB_UNPACK_V1(("ref_sect: %u set as node", ref_sect));
        m_cfg_sections[ref_sect]->set_node_section();
      }
      else if (curr_cfg_sect->is_pointer_comm_section())
      {
        DEB_UNPACK_V1(("ref_sect: %u set as comm", ref_sect));
        m_cfg_sections[ref_sect]->set_comm_section();
      }
      else if (curr_cfg_sect->is_pointer_system_section())
      {
        DEB_UNPACK_V1(("ref_sect: %u set as system", ref_sect));
        m_cfg_sections[ref_sect]->set_system_section();
      }
      else
      {
        m_error_code = NO_SUCH_POINTER_TYPE;
        return false;
      }
    }
    else if (curr_cfg_sect->is_real_section())
    {
      if (entry.m_key == CONFIG_KEY_PARENT)
      {
        /**
         * Ignore this key, only contains a 0 as parent all
         * the time.
         */
        if (entry.m_int != 0)
        {
          m_error_code = WRONG_PARENT_POINTER;
          return false;
        }
      }
      else if (unlikely(!curr_cfg_sect->set(entry, false)))
      {
        m_error_code = MEMORY_ALLOC_ERROR;
        return false;
      }
    }
    else
    {
      m_error_code = NO_SUCH_SECTION_TYPE;
      return false;
    }
  }
  if (data != end)
  {
    m_error_code = WRONG_AMOUNT_OF_DATA;
    return false;
  }
  remove_pointer_sections();
  return commitConfig(false);
}

void
ConfigObject::remove_pointer_sections()
{
  Uint32 num_sections = 0;
  std::vector<ConfigSection*> new_cfg_sections;

  for (Uint32 i = 0; i < m_num_sections; i++)
  {
    ConfigSection *cs = m_cfg_sections[i];
    if (!cs->is_real_section())
    {
      delete cs;
    }
    else
    {
      new_cfg_sections.push_back(cs);
      num_sections++;
    }
  }
  m_num_sections = num_sections;
  m_cfg_sections.clear();
  m_cfg_sections = new_cfg_sections;
  m_cfg_sections.shrink_to_fit();
}

bool
ConfigObject::commitConfig(bool only_sort)
{
  /* 7) above */
  for (Uint32 i = 0; i < m_num_sections; i++)
  {
    DEB_UNPACK_V1(("Commit section %u", i));
    m_cfg_sections[i]->verify_section();
    m_cfg_sections[i]->sort();
  }
  /* 6) above */
  if (!only_sort)
  {
    create_default_sections();
  }
  DEB_UNPACK_V1(("Commit default DB node section"));
  if (m_data_node_default_section != nullptr)
  {
    m_data_node_default_section->sort();
  }
  DEB_UNPACK_V1(("Commit default API node section"));
  if (m_api_node_default_section != nullptr)
  {
    m_api_node_default_section->sort();
  }
  DEB_UNPACK_V1(("Commit default MGM node section"));
  if (m_mgm_node_default_section != nullptr)
  {
    m_mgm_node_default_section->sort();
  }
  DEB_UNPACK_V1(("Commit default TCP section"));
  if (m_tcp_default_section != nullptr)
  {
    m_tcp_default_section->sort();
  }
  DEB_UNPACK_V1(("Commit default SHM section"));
  if (m_shm_default_section != nullptr)
  {
    m_shm_default_section->sort();
  }
  /* 8) above */
  DEB_UNPACK_V1(("Commit ConfigObject"));
  if (unlikely(!build_arrays(only_sort)))
  {
    return false;
  }
  DEB_UNPACK_V1(("Successful complete"));
  return true;
}

void
ConfigObject::create_default_sections()
{
  /* Configuration parameters can be mandatory, optional with system default
   * values, or optional without system default values.
   *
   * The sections will contain all parameters explicitly set and parameters
   * that are not set but have system default values.
   *
   * Parameters that are neither set explicitly or do not have system default
   * values will not be part of section.
   *
   * Make sure that the default sections we create only contains of parameters
   * that are present in all sections.
   *
   * Note that all sections have some mandatory keys.
   */
  ConfigSection::Key_bitset data_node_default_keys;
  ConfigSection::Key_bitset api_node_default_keys;
  ConfigSection::Key_bitset mgm_node_default_keys;
  ConfigSection::Key_bitset tcp_default_keys;
  ConfigSection::Key_bitset shm_default_keys;

  data_node_default_keys.set();
  api_node_default_keys.set();
  mgm_node_default_keys.set();
  tcp_default_keys.set();
  shm_default_keys.set();

  for (Uint32 i = 0; i < m_num_sections; i++)
  {
    ConfigSection *current = m_cfg_sections[i];
    ConfigSection::Key_bitset keys;
    current->get_keys(keys);
    ConfigSection::SectionType section_type = current->get_section_type();
    switch (section_type)
    {
      case ConfigSection::DataNodeTypeId:
      {
        data_node_default_keys &= keys;
        break;
      }
      case ConfigSection::ApiNodeTypeId:
      {
        api_node_default_keys &= keys;
        break;
      }
      case ConfigSection::MgmNodeTypeId:
      {
        mgm_node_default_keys &= keys;
        break;
      }
      case ConfigSection::TcpTypeId:
      {
        tcp_default_keys &= keys;
        break;
      }
      case ConfigSection::ShmTypeId:
      {
        shm_default_keys &= keys;
        break;
      }
      case ConfigSection::SystemSectionId:
      {
        /* Only one system section, so no need of a default */
        break;
      }
      default:
      {
        g_eventLogger->info("section_type: %u", section_type);
        require(false);
        break;
      }
    }
  }

  /**
   * The default sections is created from the first section of the type
   * found. We never put node id and first node id and second node id
   * into default section since they are used to uniquely identify node
   * and communication sections.
   */
  for (Uint32 i = 0; i < m_num_sections; i++)
  {
    ConfigSection *current = m_cfg_sections[i];
    ConfigSection::SectionType section_type = current->get_section_type();
    switch (section_type)
    {
      case ConfigSection::DataNodeTypeId:
      {
        if (m_data_node_default_section == nullptr)
        {
          DEB_UNPACK_V1(("Copy DB node section %u", i));
          m_data_node_default_section =
              current->copy_no_primary_keys(data_node_default_keys);
        }
        DEB_UNPACK_V1(("Handle DB node section %u", i));
        current->handle_default_section(m_data_node_default_section);
        break;
      }
      case ConfigSection::ApiNodeTypeId:
      {
        if (m_api_node_default_section == nullptr)
        {
          DEB_UNPACK_V1(("Copy API node section %u", i));
          m_api_node_default_section =
              current->copy_no_primary_keys(api_node_default_keys);
        }
        DEB_UNPACK_V1(("Handle API node section %u", i));
        current->handle_default_section(m_api_node_default_section);
        break;
      }
      case ConfigSection::MgmNodeTypeId:
      {
        if (m_mgm_node_default_section == nullptr)
        {
          DEB_UNPACK_V1(("Copy MGM node section %u", i));
          m_mgm_node_default_section =
              current->copy_no_primary_keys(mgm_node_default_keys);
        }
        DEB_UNPACK_V1(("Handle MGM node section %u", i));
        current->handle_default_section(m_mgm_node_default_section);
        break;
      }
      case ConfigSection::TcpTypeId:
      {
        if (m_tcp_default_section == nullptr)
        {
          DEB_UNPACK_V1(("Copy TCP section %u", i));
          m_tcp_default_section =
              current->copy_no_primary_keys(tcp_default_keys);
        }
        DEB_UNPACK_V1(("Handle TCP section %u", i));
        current->handle_default_section(m_tcp_default_section);
        break;
      }
      case ConfigSection::ShmTypeId:
      {
        if (m_shm_default_section == nullptr)
        {
          DEB_UNPACK_V1(("Copy SHM section %u", i));
          m_shm_default_section =
              current->copy_no_primary_keys(shm_default_keys);
        }
        DEB_UNPACK_V1(("Handle SHM section %u", i));
        current->handle_default_section(m_shm_default_section);
        break;
      }
      case ConfigSection::SystemSectionId:
      {
        /* Only one system section, so no need of a default */
        break;
      }
      default:
      {
        g_eventLogger->info("section_type: %u", section_type);
        require(false);
        break;
      }
    }
  }
}

bool
ConfigObject::begin()
{
  return true;
}

bool
ConfigObject::read_v2_header_info(const Uint32 **data)
{
  m_v2_tot_len = ConfigSection::read_v2_int_value(data);
  Uint32 v2 = ConfigSection::read_v2_int_value(data);
  m_num_default_sections = ConfigSection::read_v2_int_value(data);
  m_num_data_nodes = ConfigSection::read_v2_int_value(data);
  m_num_api_nodes = ConfigSection::read_v2_int_value(data);
  m_num_mgm_nodes = ConfigSection::read_v2_int_value(data);
  m_num_comm_sections = ConfigSection::read_v2_int_value(data);
  m_num_node_sections = m_num_data_nodes +
                        m_num_api_nodes +
                        m_num_mgm_nodes;
  if (v2 != 2)
  {
    /**
     * This version of the code can only handle version 1 and 2
     * of the configuration binary.
     */
    m_error_code = WRONG_VERSION_RECEIVED;
    return false;
  }
  if (m_num_default_sections != 5 ||
      m_num_data_nodes > MAX_NDB_NODES ||
      m_num_data_nodes + m_num_api_nodes > MAX_NODES ||
      m_num_data_nodes == 0 ||
      m_num_api_nodes == 0 ||
      m_num_mgm_nodes == 0 ||
      m_num_comm_sections == 0)
  {
    m_error_code = INCONSISTENT_CONFIGURATION;
    return false;
  }
  return true;
}

bool
ConfigObject::unpack_default_sections(const Uint32 **data)
{
  m_data_node_default_section = new ConfigSection(this);
  if (unlikely(m_data_node_default_section == nullptr))
  {
    m_error_code = MEMORY_ALLOC_ERROR;
    return false;
  }
  DEB_MALLOC(("new(%u) => %p", __LINE__, m_data_node_default_section));
  if (unlikely(!m_data_node_default_section->unpack_data_node_section(data)))
  {
    return false;
  }

  m_api_node_default_section = new ConfigSection(this);
  if (unlikely(m_api_node_default_section == nullptr))
  {
    m_error_code = MEMORY_ALLOC_ERROR;
    return false;
  }
  DEB_MALLOC(("new(%u) => %p", __LINE__, m_api_node_default_section));
  if (unlikely(!m_api_node_default_section->unpack_api_node_section(data)))
  {
    return false;
  }

  m_mgm_node_default_section = new ConfigSection(this);
  if (unlikely(m_mgm_node_default_section == nullptr))
  {
    m_error_code = MEMORY_ALLOC_ERROR;
    return false;
  }
  DEB_MALLOC(("new(%u) => %p", __LINE__, m_mgm_node_default_section));
  if (unlikely(!m_mgm_node_default_section->unpack_mgm_node_section(data)))
  {
    return false;
  }

  m_tcp_default_section = new ConfigSection(this);
  if (unlikely(m_tcp_default_section == nullptr))
  {
    m_error_code = MEMORY_ALLOC_ERROR;
    return false;
  }
  DEB_MALLOC(("new(%u) => %p", __LINE__, m_tcp_default_section));
  if (unlikely(!m_tcp_default_section->unpack_tcp_section(data)))
  {
    return false;
  }

  m_shm_default_section = new ConfigSection(this);
  if (unlikely(m_shm_default_section == nullptr))
  {
    m_error_code = MEMORY_ALLOC_ERROR;
    return false;
  }
  DEB_MALLOC(("new(%u) => %p", __LINE__, m_shm_default_section));
  if (unlikely(!m_shm_default_section->unpack_shm_section(data)))
  {
    return false;
  }
  return true;
}

bool ConfigObject::unpack_system_section(const Uint32 **data)
{
  if (unlikely(!m_system_section->unpack_system_section(data)))
  {
    return false;
  }
  return true;
}

bool ConfigObject::unpack_node_sections(const Uint32 **data)
{
  for (Uint32 i = 0; i < m_num_node_sections; i++)
  {
    if (unlikely(!m_node_sections[i]->unpack_node_section(data)))
    {
      return false;
    }
  }
  return true;
}

bool ConfigObject::unpack_comm_sections(const Uint32 **data)
{
  for (Uint32 i = 0; i < m_num_comm_sections; i++)
  {
    if (unlikely(!m_comm_sections[i]->unpack_comm_section(data)))
    {
      return false;
    }
  }
  return true;
}

/**
 *
 * The binary configuration structure will look like this:
 *
 * Magic number (8 bytes = NDBCONF2)
 * Header section (7 words)
 *  1. Total length in words of configuration binary
 *  2. Configuration binary version (this is version 2)
 *  3. Number of default sections in configuration binary
 *     - Data node defaults
 *     - API node defaults
 *     - MGM server node defaults
 *     - TCP communication defaults
 *     - SHM communication defaults
 *     So always 5 in this version
 *  4. Number of data nodes
 *  5. Number of API nodes
 *  6. Number of MGM server nodes
 *  7. Number of communication sections
 * Data node default section
 * API node default section
 * MGM server node default section
 * TCP communication default section
 * SHM communication default section
 * System section
 * Node sections
 * Communication sections
 *
 * There is no requirements on order of node sections, but normally
 * they are listed in node id order and similarly for communication
 * sections.
 *
 * Each node and communication section has a header. This header has
 * the following information.
 *
 * 1. Total length of this section in words
 * 2. Number of key entries in this section
 * 3. Section type
 *
 * There are 6 section types:
 * Data nodes
 * API nodes
 * MGM server nodes
 * TCP communication
 * SHM communication
 * System section
 * 
 * Each key entry has the following content.
 * KeyIdAndType
 * Value
 * 
 * The KeyIdAndType is 28 bits of key identity and
 * 4 bits of Data Type.
 *
 * There are 3 data types currently:
 * IntTypeId: The value is an unsigned 4 byte integer
 * Int64TypeId: The value is an unsigned 8 byte integer
 * StringTypeId: The value is a string
 *
 * There is no support currently for character sets in
 * the strings.
 *
 * So an IntTypeId uses 2 words, an Int64TypeId uses 3 words.
 * 
 * StringTypeId has at least 3 words. Its layout is:
 * KeyIdAndType
 * String length (1 word, the length includes the 0 byte at end)
 * String (using a zero padded string on a fixed number of words)
 *
 * Thus the string "ABS" will be stored as ABS\0 in 1 word, the
 * string "ABSOLUTE" will be stored as ABSOLUTE\0\0\0\0 in 3 words.
 *
 * The ConfigObject is implemented as one class and each section
 * is implemented as a separate class called ConfigSection.
 */
bool
ConfigObject::unpack_v2(const Uint32 *src, Uint32 len)
{
  const Uint32 *data = src;
  if (unlikely(len < sizeof(Magic_v2) + 4))
  {
    m_error_code = WRONG_MAGIC_SIZE;
    return false;
  }
  if (unlikely(memcmp(src, Magic_v2, sizeof(Magic_v2)) != 0))
  {
    m_error_code = WRONG_MAGIC_CONTENT;
    return false;
  }
  if (!check_checksum(src, len))
  {
    m_error_code = WRONG_CHECKSUM;
    return false;
  }
  data += 2; //Step to header information
  if (!read_v2_header_info(&data))
  {
    return false;
  }

  createSections(m_num_node_sections + m_num_comm_sections + 1);

  for (Uint32 i = 0; i < (m_num_sections - 1); i++)
  {
    ConfigSection *cs = m_cfg_sections[i];
    if (i < m_num_node_sections)
    {
      m_node_sections.push_back(cs);
    }
    else
    {
      m_comm_sections.push_back(cs);
    }
  }
  m_system_section = m_cfg_sections[m_num_sections - 1];

  if (unlikely(!unpack_default_sections(&data)))
  {
    return false;
  }
  if (unlikely(!unpack_system_section(&data)))
  {
    return false;
  }
  if (unlikely(!unpack_node_sections(&data)))
  {
    return false;
  }
  if (unlikely(!unpack_comm_sections(&data)))
  {
    return false;
  }
  data++; // Step past checksum
  Uint32 tot_len_words = Uint32(data - src);
  if (unlikely(tot_len_words != m_v2_tot_len))
  {
    m_error_code = WRONG_V2_UNPACK_LENGTH;
    return false;
  }
  if (unlikely(tot_len_words != (len / 4)))
  {
    m_error_code = WRONG_V2_INPUT_LENGTH;
    return false;
  }
  require(commitConfig(true));
  return true;
}

bool ConfigObject::check_checksum(const Uint32 *src, Uint32 len)
{
  Uint32 len32 = (len >> 2);
  const Uint32 * tmp = (const Uint32*)src;
  Uint32 chk = 0;
  for (Uint32 i = 0; (i+1) < len32; i++)
  {
    chk ^= ntohl(tmp[i]);
  }
  if (unlikely(chk != ntohl(tmp[len32-1])))
  {
    return false;
  }
  return true;
}

void ConfigObject::create_v1_header_section(Uint32 **v1_ptr,
                                            Uint32 &curr_section) const
{
  Uint32 num_early_node_sections =
    m_num_api_nodes + m_num_mgm_nodes;
  {
    ConfigSection::create_v1_entry_key(v1_ptr,
                                       ConfigSection::SectionTypeId,
                                       CONFIG_SECTION_SYSTEM,
                                       0);
    Uint32 section_value = num_early_node_sections + 2;
    section_value <<= OLD_KP_SECTION_SHIFT;
    ConfigSection::create_int_value(v1_ptr, section_value);
  }
  {
    ConfigSection::create_v1_entry_key(v1_ptr,
                                       ConfigSection::SectionTypeId,
                                       CONFIG_SECTION_NODE,
                                       0);
    Uint32 section_value = 1;
    section_value <<= OLD_KP_SECTION_SHIFT;
    ConfigSection::create_int_value(v1_ptr, section_value);
  }
  {
    ConfigSection::create_v1_entry_key(v1_ptr,
                                       ConfigSection::SectionTypeId,
                                       CONFIG_SECTION_CONNECTION,
                                       0);
    Uint32 section_value = num_early_node_sections + 4;
    section_value <<= OLD_KP_SECTION_SHIFT;
    ConfigSection::create_int_value(v1_ptr, section_value);
  }
  curr_section = 1;
}

void ConfigObject::create_v1_node_header_section(Uint32 **v1_ptr,
                                                 Uint32 &curr_section) const
{
  Uint32 num_non_data_nodes = m_num_api_nodes + m_num_mgm_nodes;
  for (Uint32 i = 0; i < num_non_data_nodes; i++)
  {
    ConfigSection::create_v1_entry_key(v1_ptr,
                                       ConfigSection::IntTypeId,
                                       i,
                                       1);
    Uint32 section_value = i + 2;
    section_value <<= OLD_KP_SECTION_SHIFT;
    ConfigSection::create_int_value(v1_ptr, section_value);
  }
  Uint32 num_data_nodes = m_num_data_nodes;
  for (Uint32 i = 0; i < num_data_nodes; i++)
  {
    ConfigSection::create_v1_entry_key(v1_ptr,
                                       ConfigSection::IntTypeId,
                                       num_non_data_nodes + i,
                                       1);
    Uint32 section_value =
      5 + num_non_data_nodes + m_num_comm_sections + i;
    section_value <<= OLD_KP_SECTION_SHIFT;
    ConfigSection::create_int_value(v1_ptr, section_value);
  }
  curr_section = 2;
}

void ConfigObject::create_v1_node_specific_sections(Uint32 **v1_ptr,
                                    ConfigSection::SectionType sect_type,
                                    Uint32 &curr_section) const
{
  for (Uint32 i = 0; i < m_num_sections; i++)
  {
    ConfigSection *cs = m_cfg_sections[i];
    if (cs->m_section_type == sect_type)
    {
      cs->create_v1_section(v1_ptr, curr_section);
      curr_section++;
    }
  }
}

void ConfigObject::create_v1_api_node_sections(Uint32 **v1_ptr,
                                               Uint32 &curr_section) const
{
  create_v1_node_specific_sections(v1_ptr,
                                   ConfigSection::ApiNodeTypeId,
                                   curr_section);
}

void ConfigObject::create_v1_mgm_node_sections(Uint32 **v1_ptr,
                                               Uint32 &curr_section) const
{
  create_v1_node_specific_sections(v1_ptr,
                                   ConfigSection::MgmNodeTypeId,
                                   curr_section);
}

void ConfigObject::create_v1_system_header_section(Uint32 **v1_ptr,
                                                   Uint32 &curr_section) const
{
  {
    ConfigSection::create_v1_entry_key(v1_ptr,
                                       ConfigSection::IntTypeId,
                                       0,
                                       curr_section);
    Uint32 section_value = curr_section + 1;
    section_value <<= OLD_KP_SECTION_SHIFT;
    ConfigSection::create_int_value(v1_ptr, section_value);
  }
  curr_section++;
}

void ConfigObject::create_v1_system_section(Uint32 **v1_ptr,
                                            Uint32 &curr_section) const
{
  m_system_section->create_v1_section(v1_ptr, curr_section);
  curr_section++;
}

void ConfigObject::create_v1_comm_header_section(Uint32 **v1_ptr,
                                                 Uint32 &curr_section) const
{
  for (Uint32 i = 0; i < m_num_comm_sections; i++)
  {
    ConfigSection::create_v1_entry_key(v1_ptr,
                                       ConfigSection::IntTypeId,
                                       i,
                                       curr_section);
    Uint32 section_value = curr_section + 1 + i;
    section_value <<= OLD_KP_SECTION_SHIFT;
    ConfigSection::create_int_value(v1_ptr, section_value);
  }
  curr_section++;
}

#if 0
static Uint32
get_len(Uint32 *packed_v1_ptr, Uint32 *v1_ptr)
{
  Uint32 len = (v1_ptr - packed_v1_ptr);
  return len;
}
#endif

void ConfigObject::create_v1_comm_specific_sections(Uint32 **v1_ptr,
                                 ConfigSection::SectionType sect_type,
                                 Uint32 &curr_section) const
{
  Uint32 first_j_to_check = 0;
  for (Uint32 i = 0; i < m_num_comm_sections; i++)
  {
    ConfigSection *comm_cs = m_comm_sections[i];
    Uint32 first_node_id = comm_cs->get_first_node_id();
    bool found = false;
    for (Uint32 j = first_j_to_check; j < m_num_node_sections; j++)
    {
      ConfigSection *node_cs = m_node_sections[j];
      if (node_cs->get_node_id() == first_node_id)
      {
        if (node_cs->m_section_type == sect_type)
        {
          first_j_to_check = j;
          found = true;
        }
        break;
      }
    }
    if (found)
    {
      comm_cs->create_v1_section(v1_ptr, curr_section);
      curr_section++;
    }
  }
}

void ConfigObject::create_v1_comm_sections(Uint32 **v1_ptr,
                                           Uint32 &curr_section) const
{
  create_v1_comm_specific_sections(v1_ptr,
                                   ConfigSection::DataNodeTypeId,
                                   curr_section);
  create_v1_comm_specific_sections(v1_ptr,
                                   ConfigSection::ApiNodeTypeId,
                                   curr_section);
  create_v1_comm_specific_sections(v1_ptr,
                                   ConfigSection::MgmNodeTypeId,
                                   curr_section);
}

void ConfigObject::create_v1_data_node_sections(Uint32 **v1_ptr,
                                                Uint32 &curr_section) const
{
  create_v1_node_specific_sections(v1_ptr,
                                   ConfigSection::DataNodeTypeId,
                                   curr_section);
}

Uint32
ConfigObject::get_v1_packed_size() const
{
  Uint32 v1_len_words = 0;
  v1_len_words += 2; //Magic content
  v1_len_words += (3 * 2); // Section 0
  v1_len_words += (m_num_node_sections * 2); // Section 1
  v1_len_words += 2; // System section reference
  v1_len_words += (m_num_comm_sections * 2);
  for (Uint32 i = 0; i < m_num_sections; i++)
  {
    v1_len_words += m_cfg_sections[i]->get_v1_length();
  }
  v1_len_words += 1; //Checksum
  Uint32 len = 4 * v1_len_words;
  return len;
}

void
ConfigObject::pack_v1(Uint32 *packed_v1_ptr, Uint32 len) const
{
  Uint32 *v1_ptr = packed_v1_ptr;
  Uint32 curr_section = 0;

  memset(packed_v1_ptr, 0, len);
  memcpy(v1_ptr, Magic_v1, sizeof(Magic_v1));
  v1_ptr += 2; //Magic word is two words
  create_v1_header_section(&v1_ptr, curr_section);
  create_v1_node_header_section(&v1_ptr, curr_section);
  create_v1_mgm_node_sections(&v1_ptr, curr_section);
  create_v1_api_node_sections(&v1_ptr, curr_section);
  create_v1_system_header_section(&v1_ptr, curr_section);
  create_v1_system_section(&v1_ptr, curr_section);
  create_v1_comm_header_section(&v1_ptr, curr_section);
  create_v1_comm_sections(&v1_ptr, curr_section);
  create_v1_data_node_sections(&v1_ptr, curr_section);
  ConfigSection::set_checksum(packed_v1_ptr, (len / 4));
  v1_ptr++;
  require((packed_v1_ptr + (len / 4)) == v1_ptr);
}

void
ConfigObject::create_v2_header_section(Uint32 **v2_ptr,
                                       Uint32 tot_len,
                                       Uint32 num_comm_sections) const
{
  ConfigSection::create_int_value(v2_ptr, tot_len);
  ConfigSection::create_int_value(v2_ptr, 2); // Version 2 of configuration binary
  ConfigSection::create_int_value(v2_ptr, 5); // Num default sections
  ConfigSection::create_int_value(v2_ptr, m_num_data_nodes);
  ConfigSection::create_int_value(v2_ptr, m_num_api_nodes);
  ConfigSection::create_int_value(v2_ptr, m_num_mgm_nodes);
  ConfigSection::create_int_value(v2_ptr, num_comm_sections);
}

void
ConfigObject::create_empty_default_trp_section(Uint32 **v2_ptr,
                                               Uint32 type) const
{
  ConfigSection::create_int_value(v2_ptr, 3);
  ConfigSection::create_int_value(v2_ptr, 0);
  ConfigSection::create_int_value(v2_ptr, type);
}

Uint32
ConfigObject::get_num_comm_sections(Uint32 node_id) const
{
  Uint32 num_comm_sections = 0;
  if (node_id == 0)
  {
    return m_num_comm_sections;
  }
  for (Uint32 i = 0; i < m_num_comm_sections; i++)
  {
    ConfigSection *cs = m_comm_sections[i];
    if (cs->get_first_node_id() == node_id ||
        cs->get_second_node_id() == node_id)
    {
      num_comm_sections++;
    }
  }
  return num_comm_sections;
}

Uint32
ConfigObject::get_v2_packed_size(Uint32 node_id) const
{
  Uint32 v2_len_words = 0;
  v2_len_words += 2; //Magic content
  v2_len_words += 7; //Header

  v2_len_words += m_data_node_default_section->get_v2_length();
  v2_len_words += m_api_node_default_section->get_v2_length();
  v2_len_words += m_mgm_node_default_section->get_v2_length();
  if (m_tcp_default_section != nullptr)
  {
    v2_len_words += m_tcp_default_section->get_v2_length();
  }
  else
  {
    v2_len_words += 3;
  }
  if (m_shm_default_section != nullptr)
  {
    v2_len_words += m_shm_default_section->get_v2_length();
  }
  else
  {
    v2_len_words += 3;
  }
  v2_len_words += m_system_section->get_v2_length();
  for (Uint32 i = 0; i < m_num_node_sections; i++)
  {
    v2_len_words += m_node_sections[i]->get_v2_length();
  }
  for (Uint32 i = 0; i < m_num_comm_sections; i++)
  {
    ConfigSection *cs = m_comm_sections[i];
    if (node_id == 0 ||
        cs->get_first_node_id() == node_id ||
        cs->get_second_node_id() == node_id)
    {
      v2_len_words += cs->get_v2_length();
    }
  }
  v2_len_words += 1; //Checksum
  Uint32 len = 4 * v2_len_words;
  return len;
}

void
ConfigObject::pack_v2(Uint32 *packed_v2_ptr, Uint32 len, Uint32 node_id) const
{
  Uint32 *v2_ptr = packed_v2_ptr;

  memset(packed_v2_ptr, 0, len);
  memcpy(v2_ptr, Magic_v2, sizeof(Magic_v2));
  v2_ptr += 2; //Magic word is two words

  Uint32 num_comm_sections = get_num_comm_sections(node_id);
  create_v2_header_section(&v2_ptr, (len / 4), num_comm_sections);
  m_data_node_default_section->create_v2_section(&v2_ptr);
  m_api_node_default_section->create_v2_section(&v2_ptr);
  m_mgm_node_default_section->create_v2_section(&v2_ptr);
  if (m_tcp_default_section != nullptr)
  {
    m_tcp_default_section->create_v2_section(&v2_ptr);
  }
  else
  {
    create_empty_default_trp_section(&v2_ptr, ConfigSection::TcpTypeId);
  }
  if (m_shm_default_section != nullptr)
  {
    m_shm_default_section->create_v2_section(&v2_ptr);
  }
  else
  {
    create_empty_default_trp_section(&v2_ptr, ConfigSection::ShmTypeId);
  }
  m_system_section->create_v2_section(&v2_ptr);
  for (Uint32 i = 0; i < m_num_node_sections; i++)
  {
    ConfigSection *cs = m_node_sections[i];
    cs->create_v2_section(&v2_ptr);
  }
  for (Uint32 i = 0; i < m_num_comm_sections; i++)
  {
    ConfigSection *cs = m_comm_sections[i];
    if (node_id == 0 ||
        cs->get_first_node_id() == node_id ||
        cs->get_second_node_id() == node_id)
    {
      cs->create_v2_section(&v2_ptr);
    }
  }
  ConfigSection::set_checksum(packed_v2_ptr, (len / 4));
  v2_ptr++;
  require((packed_v2_ptr + (len / 4)) == v2_ptr);
}

static
bool
compare_node_sections(ConfigSection *first, ConfigSection *second)
{
  if (first == second)
    return false;
  Uint32 first_node_id = first->get_node_id();
  Uint32 second_node_id = second->get_node_id();
  /* We should never have two node sections with same node id */
  require(first_node_id != second_node_id);
  return (first_node_id < second_node_id);
}

static
bool
compare_comm_sections(ConfigSection *first, ConfigSection *second)
{
  if (first == second)
    return false;
  Uint32 first_node_id = first->get_first_node_id();
  Uint32 second_node_id = second->get_first_node_id();
  if (first_node_id < second_node_id)
    return true;
  else if (first_node_id > second_node_id)
    return false;
  first_node_id = first->get_second_node_id();
  second_node_id = second->get_second_node_id();
  if (first_node_id < second_node_id)
    return true;
  else if (first_node_id > second_node_id)
    return false;
  /* We should never have two comm sections with same node ids */
  require(false);
  return false;
}

bool
ConfigObject::build_arrays(bool only_sort)
{
  if (!only_sort)
  {
    Uint32 num_nodes = 0;
    Uint32 num_comm_sections = 0;
    Uint32 num_mgm_nodes = 0;
    Uint32 num_data_nodes = 0;
    Uint32 num_api_nodes = 0;

    for (Uint32 i = 0; i < m_num_sections; i++)
    {
      ConfigSection *section = m_cfg_sections[i];
      ConfigSection::SectionType sect_type = section->get_section_type();
      switch (sect_type)
      {
        case ConfigSection::DataNodeTypeId:
        {
          num_nodes++;
          num_data_nodes++;
          m_node_sections.push_back(section);
          break;
        }
        case ConfigSection::ApiNodeTypeId:
        {
          num_nodes++;
          num_api_nodes++;
          m_node_sections.push_back(section);
          break;
        }
        case ConfigSection::MgmNodeTypeId:
        {
          num_nodes++;
          num_mgm_nodes++;
          m_node_sections.push_back(section);
          break;
        }
        case ConfigSection::SystemSectionId:
        {
          if (m_system_section != nullptr)
          {
            m_error_code = WRONG_AMOUNT_OF_SYSTEM_SECTIONS;
            return false;
          }
          m_system_section = section;
          break;
        }
        case ConfigSection::TcpTypeId:
        case ConfigSection::ShmTypeId:
        {
          num_comm_sections++;
          m_comm_sections.push_back(section);
          break;
        }
        default:
        {
          break;
        }
      }
    }

    m_num_node_sections = num_nodes;
    m_num_comm_sections = num_comm_sections;
    m_num_data_nodes = num_data_nodes;
    m_num_api_nodes = num_api_nodes;
    m_num_mgm_nodes = num_mgm_nodes;
  }
  m_node_sections.shrink_to_fit();
  std::sort(m_node_sections.begin(),
            m_node_sections.end(),
            compare_node_sections);

  m_comm_sections.shrink_to_fit();
  std::sort(m_comm_sections.begin(),
            m_comm_sections.end(),
            compare_comm_sections);
  return true;
}
