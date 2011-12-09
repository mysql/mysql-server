/*
   Copyright (C) 2003-2006, 2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
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

#ifndef Config_H
#define Config_H

#include <kernel/NodeBitmask.hpp>
#include "ConfigInfo.hpp"
#include <mgmapi.h>
#include "../mgmapi/mgmapi_configuration.hpp"


/**
 * @class Config
 * @brief Cluster Configuration Wrapper
 *
 * Adds a C++ wrapper around 'ndb_mgm_configuration' which is
 * exposed from mgmapi_configuration
 *
 */

class Config {
public:
  Config(struct ndb_mgm_configuration *config_values = NULL);
  Config(ConfigValues* config_values);
  Config(const Config*);
  virtual ~Config();

  void print(const char* section_filter = NULL, NodeId nodeid_filter = NULL,
             const char* param_filter = NULL,
             NdbOut& out = ndbout) const;

  /*
    Returns generation of the config
    0 => not set(yet), ie. config has never been committed
   */
  Uint32 getGeneration() const;
  bool setGeneration(Uint32);

  /*
    Returns name of the config
  */
  const char* getName() const;
  bool setName(const char* new_name);

  /*
    Returns primary MGM node of the config, this is used to
    protect the config being overwritten by an "old" config.ini
    or my.cnf - i.e as soon as the config.ini has been updated
    and reloaded from one node, the config.ini on other nodes
    become obsolete and a reload from those would revert to an
    old config.
    0 => config updated from mgmapi, no node is primary anymore
    1 - MAX_NODES => only node with specified nodeid can reload
                     config without force
   */
  Uint32 getPrimaryMgmNode() const;
  bool setPrimaryMgmNode(Uint32);

  /*
   Pack the config into a UtilBuffer and return it's size in bytes
  */
  Uint32 pack(UtilBuffer&) const;

  /*
    Pack the config as base64
  */
  bool pack64(BaseString&) const;

  /*
    Compare against another config and return a list of
    differences in a Properties object
  */
  void diff(const Config* other, Properties& diff_list,
            const unsigned* exclude=NULL) const;

  /*
    Print the difference against another config
   */
  void print_diff(const Config* other) const;


  /*
    Get the full connectstring for this configuration. ie
    a list of all the mgmd servers and their port separated
    by separator.
   */
  void getConnectString(BaseString&,
                        const BaseString& separator = BaseString(";")) const;

  /*
    Print the difference to string buffer
  */
  const char* diff2str(const Config* other, BaseString& str,
                       const unsigned* exclude = NULL) const;

  /*
    Determine if changing to the other config is illegal
  */
  bool illegal_change(const Config* other) const;

  /*
    Check if the config is equal to another config
  */
  bool equal(const Config*, const unsigned* exclude = NULL) const;

  /*
    Return the checksum of the config. The checksum can be used to compare
    two configs without having the whole config available(for example on
    a remote host). It can also be printed to log files for manual verification
    that same config is used
  */
  Uint32 checksum(void) const;

  /*
    Return bitmask of all defined nodes of a certain type
    returns all defined nodes by default.
   */
  void get_nodemask(NodeBitmask& mask,
                    ndb_mgm_node_type type = NDB_MGM_NODE_TYPE_UNKNOWN) const;

  struct ndb_mgm_configuration * m_configValues;
  struct ndb_mgm_configuration * values(void) const { return m_configValues; };

private:
  bool setValue(Uint32 section, Uint32 section_no,
                Uint32 id, Uint32 new_val);
  bool setValue(Uint32 section, Uint32 section_no,
                Uint32 id, const char* new_val);

  bool illegal_change(const Properties&) const;
  bool equal(const Properties&) const;
  const char* diff2str(const Properties&, BaseString& str) const;
};

class ConfigIter : public ndb_mgm_configuration_iterator {
public:
  ConfigIter(const Config* conf, unsigned type) :
    ndb_mgm_configuration_iterator(*conf->m_configValues, type) {};
};

#endif // Config_H
