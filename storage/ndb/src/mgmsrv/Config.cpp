/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "Config.hpp"

#include <mgmapi.h>
#include <NdbOut.hpp>
#include "ConfigInfo.hpp"

#include <HashMap.hpp>

Config::Config(struct ndb_mgm_configuration *config_values) :
  m_configValues(config_values)
{
}


Config::Config(ConfigValues *config_values) :
  m_configValues((struct ndb_mgm_configuration*)config_values)
{
}

Config::Config(const Config* conf)
{
  // TODO Magnus, improve copy constructor
  // to not use pack/unpack
  assert(conf);
  UtilBuffer buf;
  conf->pack(buf);
  ConfigValuesFactory cvf;
  cvf.unpack(buf);
  m_configValues= (struct ndb_mgm_configuration*)cvf.getConfigValues();
}


Config::~Config() {
  ndb_mgm_destroy_configuration(m_configValues);
}

unsigned sections[]=
{
  CFG_SECTION_SYSTEM,
  CFG_SECTION_NODE,
  CFG_SECTION_CONNECTION
};
const size_t num_sections= sizeof(sections)/sizeof(unsigned);

static const ConfigInfo g_info;

void
Config::print(const char* section_filter, NodeId nodeid_filter,
              const char* param_filter,
              NdbOut& out) const {

  for(unsigned i= 0; i < num_sections; i++) {
    unsigned section= sections[i];
    ConfigIter it(this, section);

    if (it.first())
      continue;

    for(;it.valid();it.next()) {

      Uint32 section_type;
      if(it.get(CFG_TYPE_OF_SECTION, &section_type) != 0)
        continue;

      const ConfigInfo::ParamInfo* pinfo= NULL;
      ConfigInfo::ParamInfoIter param_iter(g_info,
                                           section,
                                           section_type);

      const char* section_name= g_info.sectionName(section, section_type);

      // Section name filter
      if (section_filter &&                     // Filter is on
          strcmp(section_filter, section_name)) // Value is different
        continue;

      // NodeId filter
      Uint32 nodeid = 0;
      it.get(CFG_NODE_ID, &nodeid);
      if (nodeid_filter &&                   // Filter is on
          nodeid_filter != nodeid)           // Value is different
        continue;

      /*  Loop through the section and print those values that exist */
      Uint32 val;
      Uint64 val64;
      const char* val_str;
      while((pinfo= param_iter.next())){

        // Param name filter
        if (param_filter &&                      // Filter is on
            strcmp(param_filter, pinfo->_fname)) // Value is different
          continue;

        if (section_name) // Print section name only first time
        {
          out << "[" << section_name << "]" << endl;
          section_name= NULL;
        }

        if (!it.get(pinfo->_paramId, &val))
          out << pinfo->_fname << "=" << val << endl;
        else if (!it.get(pinfo->_paramId, &val64))
          out << pinfo->_fname << "=" << val64 << endl;
        else if (!it.get(pinfo->_paramId, &val_str))
          out << pinfo->_fname << "=" << val_str << endl;
      }
    }
  }
}



Uint32
Config::getGeneration() const
{
  Uint32 generation;
  ConfigIter iter(this, CFG_SECTION_SYSTEM);

  if (iter.get(CFG_SYS_CONFIG_GENERATION, &generation))
    return 0;

  return generation;
}


Uint32
Config::getPrimaryMgmNode() const
{
  Uint32 primaryMgmNode;
  ConfigIter iter(this, CFG_SECTION_SYSTEM);

  if (iter.get(CFG_SYS_PRIMARY_MGM_NODE, &primaryMgmNode))
    return 0;

  return primaryMgmNode;
}


const char*
Config::getName() const
{
  const char* name;
  ConfigIter iter(this, CFG_SECTION_SYSTEM);

  if (iter.get(CFG_SYS_NAME, &name))
    return 0;

  return name;
}


bool
Config::setValue(Uint32 section, Uint32 section_no,
                 Uint32 id, Uint32 new_val)
{
  ConfigValues::Iterator iter(m_configValues->m_config);
  if (!iter.openSection(section, section_no))
    return false;

  if (!iter.set(id, new_val))
    return false;

  return true;
}


bool
Config::setValue(Uint32 section, Uint32 section_no,
                 Uint32 id, const char* new_val)
{
  ConfigValues::Iterator iter(m_configValues->m_config);
  if (!iter.openSection(section, section_no))
    return false;

  if (!iter.set(id, new_val))
    return false;

  return true;
}


bool
Config::setGeneration(Uint32 new_gen)
{
  return setValue(CFG_SECTION_SYSTEM, 0,
                  CFG_SYS_CONFIG_GENERATION,
                  new_gen);
}


bool
Config::setPrimaryMgmNode(Uint32 new_primary)
{
  return setValue(CFG_SECTION_SYSTEM, 0,
                  CFG_SYS_PRIMARY_MGM_NODE,
                  new_primary);
}


bool
Config::setName(const char* new_name)
{
  return setValue(CFG_SECTION_SYSTEM, 0,
                  CFG_SYS_NAME,
                  new_name);
}


Uint32
Config::pack(UtilBuffer& buf) const
{
  return m_configValues->m_config.pack(buf);
}


#include <ndb_base64.h>

bool
Config::pack64(BaseString& encoded) const
{
  UtilBuffer buf;
  if (m_configValues->m_config.pack(buf) == 0)
    return false;

  /*
    Expand the string to correct length by filling with Z.
    The base64 encoded data of UtilBuffer can be of max length (1024*1024)/3*4
    hence using int to store the length.
  */
  encoded.assfmt("%*s",
                 (int)base64_needed_encoded_length(buf.length()),
                 "Z");

  if (base64_encode(buf.get_data(),
                    buf.length(),
                    (char*)encoded.c_str()))
    return false;
  return true;
}


enum diff_types {
  DT_DIFF,            // Value differed
  DT_MISSING_VALUE,   // Value didn't exist
  DT_MISSING_SECTION, // Section missing
  DT_ILLEGAL_CHANGE    // Illegal change detected
};


static void
add_diff(const char* name, const char* key,
         Properties& diff,
         const char* value_name, Properties* value)
{
  Properties *section;
  // Create a new section if it did not exist
  if (!diff.getCopy(key, &section)){
    Properties new_section(true);
    new_section.put("Key", key);
    new_section.put("Name", name);

    require(diff.put(key, &new_section));

    // Get copy of section
    require(diff.getCopy(key, &section));
  }

  // Make sure type of diff has been set
  Uint32 type;
  require(value->get("Type", &type));

  require(value->put("Name", value_name));

  // Add the value to the section if not already added
  // (a changed value will be detected twice)
  if (!section->put(value_name, value))
    require(section->getPropertiesErrno() ==
            E_PROPERTIES_ELEMENT_ALREADY_EXISTS);

  // Put the updated section into the diff
  require(diff.put(key, section, true));

  delete section;
}


static void
compare_value(const char* name, const char* key,
              const ConfigInfo::ParamInfo* pinfo,
              ConfigValues::ConstIterator& it,
              ConfigValues::ConstIterator& it2,
              Properties& diff)
{
  Uint32 pid= pinfo->_paramId;
  {
    Uint32 val;
    if (it.get(pid, &val) == true) {
      Uint32 val2;
      if (it2.get(pid, &val2) == true) {
        if (val != val2){
          Properties info(true);
          info.put("Type", DT_DIFF);
          info.put("New", val2);
          info.put("Old", val);
          add_diff(name, key,
                   diff,
                   pinfo->_fname, &info);
        }
      }
      else
      {
        Properties info(true);
        info.put("Type", DT_MISSING_VALUE);
        info.put("Old", val);
        add_diff(name, key,
                 diff,
                 pinfo->_fname, &info);
      }
      return;
    }
  }

  {
    Uint64 val;
    if (it.get(pid, &val) == true) {
      Uint64 val2;
      if (it2.get(pid, &val2) == true) {
        if (val != val2) {
          Properties info(true);
          info.put("Type", DT_DIFF);
          info.put64("New", Uint64(val2));
          info.put64("Old", Uint64(val));
          add_diff(name, key,
                   diff,
                   pinfo->_fname, &info);
        }
      }
      else
      {
        Properties info(true);
        info.put("Type", DT_MISSING_VALUE);
        info.put64("Old", Uint64(val));
        add_diff(name, key,
                 diff,
                 pinfo->_fname, &info);
      }
      return;
    }
  }

  {
    const char* val;
    if (it.get(pid, &val) == true) {
      const char* val2;
      if (it2.get(pid, &val2) == true) {
        if (strcmp(val, val2)) {
          Properties info(true);
          info.put("Type", DT_DIFF);
          info.put("New", val2);
          info.put("Old", val);
          add_diff(name, key,
                   diff,
                   pinfo->_fname, &info);
        }
      }
      else
      {
        Properties info(true);
        info.put("Type", DT_MISSING_VALUE);
        info.put("Old", val);
        add_diff(name, key,
                 diff,
                 pinfo->_fname, &info);
      }
      return;
    }
  }
}


static void
diff_system(const Config* a, const Config* b, Properties& diff)
{
  ConfigIter itA(a, CFG_SECTION_SYSTEM);
  ConfigIter itB(b, CFG_SECTION_SYSTEM);

  // Check each possible configuration value
  const ConfigInfo::ParamInfo* pinfo= NULL;
  ConfigInfo::ParamInfoIter param_iter(g_info,
                                       CFG_SECTION_SYSTEM,
                                       CFG_SECTION_SYSTEM);
  while((pinfo= param_iter.next())) {
    /*  Loop through the section and compare values */
    compare_value("SYSTEM", "", pinfo, itA.m_config, itB.m_config, diff);
  }
}


static void
diff_nodes(const Config* a, const Config* b, Properties& diff)
{
  ConfigIter itA(a, CFG_SECTION_NODE);

  for(;itA.valid(); itA.next())
  {

    /* Get typ of Node */
    Uint32 nodeType;
    require(itA.get(CFG_TYPE_OF_SECTION, &nodeType) == 0);

    BaseString name(g_info.sectionName(CFG_SECTION_NODE, nodeType));

    /* Get NodeId which is "primary key" */
    Uint32 nodeId;
    require(itA.get(CFG_NODE_ID, &nodeId) == 0);

    BaseString key;
    key.assfmt("NodeId=%d", nodeId);

    /* Position itB in the section with same NodeId */
    ConfigIter itB(b, CFG_SECTION_NODE);
    if (itB.find(CFG_NODE_ID, nodeId) != 0)
    {
      // A whole node has been removed
      Properties info(true);
      info.put("Type", DT_MISSING_SECTION);
      info.put("Why", "Node removed");
      add_diff(name.c_str(), key.c_str(),
               diff,
               "Node removed", &info);

      continue;
    }

    /* Make sure it has the same node type */
    Uint32 nodeType2;
    require(itB.get(CFG_TYPE_OF_SECTION, &nodeType2) == 0);
    if ((nodeType == NODE_TYPE_DB || nodeType == NODE_TYPE_MGM) &&
        nodeType != nodeType2)
    {
      // DB or MGM node has changed type -> not allowed change
      Properties info(true);
      info.put("Type", DT_ILLEGAL_CHANGE);
      info.put("Why", "Node has changed type");
      add_diff(name.c_str(), key.c_str(),
               diff,
               "Node type changed", &info);
      continue;
    }

    // Check each possible configuration value
    const ConfigInfo::ParamInfo* pinfo= NULL;
    ConfigInfo::ParamInfoIter param_iter(g_info, CFG_SECTION_NODE, nodeType);
    while((pinfo= param_iter.next())) {
      /*  Loop through the section and compare values */
      compare_value(name.c_str(), key.c_str(), pinfo,
                    itA.m_config, itB.m_config, diff);
    }
  }
}


struct NodePair {
  Uint32 nodeId1;
  Uint32 nodeId2;
  NodePair(Uint32 n1, Uint32 n2) : nodeId1(n1), nodeId2(n2) {};
};

static void
diff_connections(const Config* a, const Config* b, Properties& diff)
{
  // Build lookup table to make it a quick operation to check
  // if a given connection(with "primary key" NodeId1+NodeId2)
  // exists in other config, in such case return the section number
  // so that the section can be retrieved quickly
  HashMap<NodePair, Uint32> lookup;
  {
    Uint32 nodeId1, nodeId2;
    ConfigIter itB(b, CFG_SECTION_CONNECTION);
    for(; itB.valid(); itB.next())
    {
      require(itB.get(CFG_CONNECTION_NODE_1, &nodeId1) == 0);
      require(itB.get(CFG_CONNECTION_NODE_2, &nodeId2) == 0);

      require(lookup.insert(NodePair(nodeId1, nodeId2), itB.m_sectionNo));
    }
  }

  ConfigIter itA(a, CFG_SECTION_CONNECTION);

  for(;itA.valid(); itA.next())
  {
    /* Get typ of connection */
    Uint32 connectionType;
    require(itA.get(CFG_TYPE_OF_SECTION, &connectionType) == 0);

    BaseString name(g_info.sectionName(CFG_SECTION_CONNECTION, connectionType));

    /* Get NodeId1 and NodeId2 which is "primary key" */
    Uint32 nodeId1_A, nodeId2_A;
    require(itA.get(CFG_CONNECTION_NODE_1, &nodeId1_A) == 0);
    require(itA.get(CFG_CONNECTION_NODE_2, &nodeId2_A) == 0);

    BaseString key;
    key.assfmt("NodeId1=%d;NodeId2=%d", nodeId1_A, nodeId2_A);

    /* Lookup connection and get section no if it exists */
    Uint32 sectionNo;
    if (!lookup.search(NodePair(nodeId1_A, nodeId2_A), sectionNo))
    {
      // A connection has been removed
      Properties info(true);
      info.put("Type", DT_MISSING_SECTION);
      info.put("Why", "Connection removed");
      add_diff(name.c_str(), key.c_str(),
               diff,
               "Connection removed", &info);

      continue;
    }

    /* Open the connection section in other config */
    ConfigValues::ConstIterator itB(b->m_configValues->m_config);
    require(itB.openSection(CFG_SECTION_CONNECTION, sectionNo) == true);

    Uint32 nodeId1_B = 0; /* Silence compiler warning */
    Uint32 nodeId2_B = 0; /* Silence compiler warning */
    require(itB.get(CFG_CONNECTION_NODE_1, &nodeId1_B) == true);
    require(itB.get(CFG_CONNECTION_NODE_2, &nodeId2_B) == true);
    require(nodeId1_A == nodeId1_B && nodeId2_A == nodeId2_B);

    // Check each possible configuration value
    const ConfigInfo::ParamInfo* pinfo= NULL;
    ConfigInfo::ParamInfoIter param_iter(g_info,
                                         CFG_SECTION_CONNECTION,
                                         connectionType);
    while((pinfo= param_iter.next())) {
      /*  Loop through the section and compare values */
      compare_value(name.c_str(), key.c_str(), pinfo, itA.m_config, itB, diff);
    }
  }
}


static bool
include_section(const unsigned* exclude, unsigned section){
  if (exclude == NULL)
    return true;

  while(*exclude){
    if (*exclude == section)
      return false;
    exclude++;
  }
  return true;
}


/**
  Generate a diff list
*/

void Config::diff(const Config* other, Properties& diff,
                  const unsigned* exclude) const {

  if (include_section(exclude, CFG_SECTION_SYSTEM)) {
    diff_system(this, other, diff);
    diff_system(other, this, diff);
  }
  if (include_section(exclude, CFG_SECTION_NODE)) {
    diff_nodes(this, other, diff);
    diff_nodes(other, this, diff);
  }
  if (include_section(exclude, CFG_SECTION_CONNECTION)) {
    diff_connections(this, other, diff);
    diff_connections(other, this, diff);
  }
}


static const char*
p2s(const Properties* prop, const char* name, BaseString& buf){
  PropertiesType type;
  require(prop->getTypeOf(name, &type));
  switch(type){
  case PropertiesType_Uint32:
  {
    Uint32 val;
    require(prop->get(name, &val));
    buf.assfmt("%u", val);
    break;
  }
  case PropertiesType_Uint64:
  {
    Uint64 val;
    require(prop->get(name, &val));
    buf.assfmt("%llu", val);
    break;
  }
  case PropertiesType_char:
  {
    require(prop->get(name, buf));
    break;
  }
  default:
    require(false);
    break;
  }
  return buf.c_str();
}


const char*
Config::diff2str(const Properties& diff_list, BaseString& str) const
{
  const char* name;
  Properties::Iterator prop_it(&diff_list);
  while ((name= prop_it.next())){

    const Properties *node;
    require(diff_list.get(name, &node));

    require(node->get("Name", &name));
    str.appfmt("[%s]\n", name);

    BaseString key;
    require(node->get("Key", key));
    if (key.length() > 0){
      Vector<BaseString> keys;
      key.split(keys, ";");
      for (unsigned i= 0; i < keys.size(); i++)
        str.appfmt("%s\n", keys[i].c_str());
    }

    BaseString buf;
    Properties::Iterator prop_it2(node);
    while ((name= prop_it2.next())){

      const Properties *what;
      if (!node->get(name, &what))
        continue;

      Uint32 type;
      require(what->get("Type", &type));
      require(what->get("Name", &name));
      switch (type) {
      case DT_DIFF:
      {
        str.appfmt("-%s=%s\n", name, p2s(what, "Old", buf));
        str.appfmt("+%s=%s\n", name, p2s(what, "New", buf));
        break;
      }

      case DT_MISSING_VALUE:
      {
        str.appfmt("-%s=%s\n", name, p2s(what, "Old", buf));
        break;
      }

      case DT_MISSING_SECTION:
      {
        const char* why;
        if (what->get("Why", &why))
          str.appfmt("%s\n", why);
        break;
      }

      case DT_ILLEGAL_CHANGE:
      {
        const char* why;
        str.appfmt("Illegal change\n");
        if (what->get("Why", &why))
          str.appfmt("%s\n", why);
        break;
      }

      default:
        str.appfmt("Illegal 'type' found in diff_list\n");
        require(false);
        break;
      }
    }
    str.appfmt("\n");
  }
  return str.c_str();
}


void Config::print_diff(const Config* other) const {
  Properties diff_list;
  diff(other, diff_list);
  BaseString str;
  ndbout_c("%s", diff2str(diff_list, str));
}


const char*
Config::diff2str(const Config* other, BaseString& str, const unsigned * exclude) const {
  Properties diff_list;
  diff(other, diff_list, exclude);
  return diff2str(diff_list, str);
}


bool Config::equal(const Properties& diff_list) const {
  int count= 0;
  Properties::Iterator prop_it(&diff_list);
  while ((prop_it.next()))
    count++;
  return (count == 0);
}


bool Config::equal(const Config* other, const unsigned * exclude) const {
  Properties diff_list;
  diff(other, diff_list, exclude);
  return equal(diff_list);
}



bool Config::illegal_change(const Properties& diff_list) const {
  bool illegal= false;
  const char* name;
  Properties::Iterator prop_it(&diff_list);
  while ((name= prop_it.next())){

    const Properties *node;
    require(diff_list.get(name, &node));

    Properties::Iterator prop_it2(node);
    while ((name= prop_it2.next())){

      const Properties *what;
      if (!node->get(name, &what))
        continue;

      Uint32 type;
      require(what->get("Type", &type));
      if (type == DT_ILLEGAL_CHANGE)
      {
        illegal= true;
        break;
      }
    }
  }
  return illegal;
}


bool Config::illegal_change(const Config* other) const {
  Properties diff_list;
  diff(other, diff_list);
  return illegal_change(diff_list);
}


void Config::getConnectString(BaseString& connectstring,
                              const BaseString& separator) const
{
  bool first= true;
  ConfigIter it(this, CFG_SECTION_NODE);

  for(;it.valid(); it.next())
  {
    /* Get type of Node */
    Uint32 nodeType;
    require(it.get(CFG_TYPE_OF_SECTION, &nodeType) == 0);

    if (nodeType != NODE_TYPE_MGM)
      continue;

    Uint32 port;
    const char* hostname;
    require(it.get(CFG_NODE_HOST, &hostname) == 0);
    require(it.get(CFG_MGM_PORT, &port) == 0);

    if (!first)
      connectstring.append(separator);
    first= false;

    connectstring.appfmt("%s:%d", hostname, port);

  }
  ndbout << connectstring << endl;
}


void
Config::get_nodemask(NodeBitmask& mask,
                     ndb_mgm_node_type type) const
{
  mask.clear();
  ConfigIter it(this, CFG_SECTION_NODE);
  for (; it.valid(); it.next())
  {
    Uint32 node_type;
    require(it.get(CFG_TYPE_OF_SECTION, &node_type) == 0);

    if (type == NDB_MGM_NODE_TYPE_UNKNOWN || // UNKOWN -> add all nodes to mask
        type == (ndb_mgm_node_type)node_type)
    {
      Uint32 nodeid;
      require(it.get(CFG_NODE_ID, &nodeid) == 0);
      mask.set(nodeid);
    }
  }
}


Uint32
Config::checksum(void) const {
  Uint32 chk;

  UtilBuffer buf;
  pack(buf);

  // Checksum is the last 4 bytes in buffer
  const char* chk_ptr = (const char*)buf.get_data();
  chk_ptr += buf.length() - sizeof(Uint32);
  chk = *(Uint32*) chk_ptr;

  return chk;
}

