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

#ifndef ConfigObject_H
#define ConfigObject_H

#include <vector>

#define OUR_V2_VERSION true

/* Error codes */
#define WRONG_MAGIC_SIZE 1
#define WRONG_MAGIC_CONTENT 2
#define WRONG_CHECKSUM 3
#define MEMORY_ALLOC_ERROR 4
#define WRONG_STRING_LENGTH 5
#define WRONG_ENTRY_TYPE 6
#define SET_NOT_REAL_SECTION_ERROR 7
#define SET_REAL_SECTION_ERROR 8
#define SET_NODE_SECTION_ERROR 9
#define SET_CONNECTION_SECTION_ERROR 10
#define SET_SYSTEM_SECTION_ERROR 11
#define UNDEFINED_SECTION_TYPE 12
#define WRONG_PARENT_POINTER 13
#define WRONG_AMOUNT_OF_DATA 14
#define WRONG_AMOUNT_OF_SYSTEM_SECTIONS 15
#define NO_SUCH_POINTER_TYPE 16
#define NO_SUCH_SECTION_TYPE 17
#define INCONSISTENT_NUM_ENTRIES 18
#define WRONG_V2_UNPACK_LENGTH 19
#define WRONG_V2_INPUT_LENGTH 20
#define WRONG_EMPTY_SECTION_LENGTH 21
#define WRONG_SECTION_TYPE 22
#define WRONG_VALUE_TYPE 23
#define WRONG_NODE_TYPE 24
#define WRONG_COMM_TYPE 25
#define WRONG_VERSION_RECEIVED 26
#define INCONSISTENT_CONFIGURATION 27
#define WRONG_DATA_TYPE_OF_SECTION 28
#define WRONG_DATA_TYPE_IN_SET 29

class ConfigSection;

class ConfigObject
{
  friend class ConfigSection;

public: /* Public methods */
  ConfigObject();
  ~ConfigObject();

  /**
   * Create a new section, with section type and node/comm type
   * added.
   * This section will become the current section where we can
   * perform put's.
   */
  bool createSection(Uint32 section_type, Uint32 type);
  void closeSection();

  /**
   * Open a section, the first parameter is the section type
   * where
   * 1000 is System section
   * 2000 is Node sections and
   * 3000 is the Communication sections
   * No other values of section type are allowed. The index
   * is the section id within this section type.
   */
  ConfigSection* openSection(Uint32 section_type, Uint32 index) const;

  /**
   * Fill the current section with a new key and its value.
   * 3 different functions based on their data type.
   */
  bool put(Uint32 key, Uint32 val);
  bool put64(Uint32 key, Uint64 val);
  bool put(Uint32 key, const char *string);

  /**
   * Get an key entry from the current section.
   */
  bool get(ConfigSection *curr_section,
           Uint32 key,
           ConfigSection::Entry &entry) const;

  /**
   * Set one item in the current section.
   */
  bool set(ConfigSection *curr_section,
           ConfigSection::Entry &entry,
           bool free_string);

  /**
   * A configuration binary is used to create the ConfigObject.
   * This binary can have been retrieved from a get config call towards the
   * NDB management server, received in a message from another MGM server
   * in process of updating the config, it could be retrieved from a client
   * that desires to update the configuration and it can have been read from
   * the binary configuration file.
   */
  bool unpack_v2(const Uint32 *cfg_binary, Uint32 size);

  /**
   * A configuration binary is created from the contents of the ConfigObject.
   * This returns a pointer to a contiguous memory area that has to be free'd
   * by the caller.
   */
  Uint32 get_v2_packed_size(Uint32 node_id) const;
  void pack_v2(Uint32 *packed_v2_ptr, Uint32 len, Uint32 node_id = 0) const;

  /**
   * Same as unpack, but this unpacks the old format and creates a ConfigObject
   * based on the old binary format. This is used when a node retrieves a
   * configuration from an old MGM server or when a new MGM server sends a
   * configuration to a node using the old configuration format.
   */
  bool unpack_v1(const Uint32 *cfg_binary, Uint32 size);

  /**
   * Same as pack, but this returns a configuration binary in the old format.
   */
  Uint32 get_v1_packed_size() const;
  void pack_v1(Uint32 *packed_v1_ptr, Uint32 len) const;

  /**
   * Create a new ConfigObject consisting of only the specified ConfigSection.
   */
  ConfigObject* copy_current(ConfigSection *cs) const;

  /**
   * Read all entries in configuration one by one.
   */
  Uint32 getNextEntry(Uint32 key, ConfigSection::Entry *in_entry) const;

  /**
   * Call begin before structural changes to ConfigObject.
   * Call commitConfig when done.
   */
  bool begin();

  /**
   * We have completed inserting data into the config, commit the
   * changes.
   */
  bool commitConfig(bool only_sort);

  /**
   * Print the error code from the previous call to ConfigObject.
   */
  void print_error_code() const;
  int get_error_code() const;

private: /* Private methods */
  Uint32 get_num_comm_sections(Uint32 node_id) const;
  /* Remove pointer sections as final unpack_v1 step */
  void remove_pointer_sections();
  /**
   * Method used when unpacking a configuration binary, it will create an
   * array, and fill the array with pointers to ConfigSection objects of
   * currently unknown type.
   */
  void createSections(Uint32 num_sections);

  /**
   * Get the type from key using the old packed format.
   * Get the section id from key using old packed format.
   * Get the key from key using old packed format.
   */
  ConfigSection::ValueType get_old_type(Uint32);
  Uint32 get_old_section(Uint32);
  Uint32 get_old_key(Uint32);

  /* Create the default sections */
  void create_default_sections();
  bool build_arrays(bool from_unpack);

  /**
   * Functions to create the v1 configuration binary.
   */
  void create_v1_header_section(Uint32 **v1_ptr,
                                Uint32 &curr_section) const;
  void create_v1_node_header_section(Uint32 **v1_ptr,
                                     Uint32 &curr_section) const;
  void create_v1_mgm_node_sections(Uint32 **v1_ptr,
                                   Uint32 &curr_section) const;
  void create_v1_api_node_sections(Uint32 **v1_ptr,
                                   Uint32 &curr_section) const;
  void create_v1_system_header_section(Uint32 **v1_ptr,
                                       Uint32 &curr_section) const;
  void create_v1_system_section(Uint32 **v1_ptr,
                                Uint32 &curr_section) const;
  void create_v1_comm_header_section(Uint32 **v1_ptr,
                                     Uint32 &curr_section) const;
  void create_v1_comm_sections(Uint32 **v1_ptr,
                               Uint32 &curr_section) const;
  void create_v1_data_node_sections(Uint32 **v1_ptr,
                                    Uint32 &curr_section) const;
  void create_v1_entry(Uint32**,
                       Uint32 type,
                       Uint32 key,
                       Uint32 section);
  void create_v1_comm_specific_sections(Uint32 **v1_ptr,
                                        ConfigSection::SectionType sect_type,
                                        Uint32 &curr_section) const;
  void create_v1_node_specific_sections(Uint32 **v1_ptr,
                                        ConfigSection::SectionType sect_type,
                                        Uint32 &curr_section) const;

  /**
   * Functions to create v2 configuration binary.
   */
  void create_v2_header_section(Uint32** v2_ptr,
                                         Uint32 tot_len,
                                         Uint32 num_comm_sections) const;
  void create_empty_default_trp_section(Uint32 **v2_ptr,
                                        Uint32 type) const;

  /**
   * Functions to unpack v2 configuration binary
   */
  bool check_checksum(const Uint32 *src, Uint32 len_bytes);
  bool unpack_default_sections(const Uint32 **data);
  bool unpack_system_section(const Uint32 **data);
  bool unpack_node_sections(const Uint32 **data);
  bool unpack_comm_sections(const Uint32 **data);
  bool read_v2_header_info(const Uint32 **data);

private: /* Private data */
  /**
   * Variable pointing to the currently opened ConfigSection object.
   */
  ConfigSection *m_curr_cfg_section;
  /**
   * The array of the current set of ConfigSection objects in this
   * configuration.
   */
  std::vector<ConfigSection*> m_cfg_sections;
  /**
   * Number of ConfigSection* entries in m_cfg_sections array.
   */
  Uint32 m_num_sections;
  /**
   * Pointer to the system section
   */
  ConfigSection *m_system_section;
  /**
   * Pointer to an array of node sections.
   * Number of entries in the array.
   */
  std::vector<ConfigSection*> m_node_sections;
  Uint32 m_num_node_sections;

  Uint32 m_num_data_nodes;
  Uint32 m_num_api_nodes;
  Uint32 m_num_mgm_nodes;

  Uint32 m_v2_tot_len;
  Uint32 m_num_default_sections;
  /**
   * Pointer to an array of communication sections.
   * Number of entries in the array.
   */
  std::vector<ConfigSection*> m_comm_sections;
  Uint32 m_num_comm_sections;

  /**
   *
   */
  ConfigSection *m_data_node_default_section;
  ConfigSection *m_api_node_default_section;
  ConfigSection *m_mgm_node_default_section;
  ConfigSection *m_tcp_default_section;
  ConfigSection *m_shm_default_section;

protected:
  /**
   * Error code after failure of some kind.
   */
  Uint32 m_error_code;
};

inline ConfigSection::ValueType 
ConfigObject::get_old_type(Uint32 key)
{
  return (ConfigSection::ValueType)(key >> OLD_KP_TYPE_SHIFT);
}

inline Uint32
ConfigObject::get_old_section(Uint32 key)
{
  return ((key >> OLD_KP_SECTION_SHIFT) & OLD_KP_SECTION_MASK);
}

inline Uint32
ConfigObject::get_old_key(Uint32 key)
{
  return ((key >> OLD_KP_KEYVAL_SHIFT) & OLD_KP_KEYVAL_MASK);
}
#endif
