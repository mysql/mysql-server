/*
   Copyright (C) 2003-2006 MySQL AB
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

#include "NdbConfig.hpp"
#include <NdbOut.hpp>
#include <NDBT_Output.hpp>
#include <assert.h>
#include <NdbConfig.h>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>
#include <mgmapi.h>
#include <mgmapi_config_parameters.h>
#include <mgmapi_configuration.hpp>

bool
NdbConfig::getHostName(unsigned int node_id, const char ** hostname) {
  
  ndb_mgm_configuration * p = getConfig();
  if(p == 0){
    return false;
  }
  
  /**
   * Setup cluster configuration data
   */
  ndb_mgm_configuration_iterator iter(* p, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, node_id)){
    ndbout << "Invalid configuration fetched, DB missing" << endl;
    return false;
  }

  if (iter.get(CFG_NODE_HOST, hostname)){
    ndbout << "Host not found" << endl;
    return false;
  }
  
  return true;
}

bool
NdbConfig::getProperty(unsigned nodeid, 
		       unsigned type, unsigned key, Uint32 * val){
  ndb_mgm_configuration * p = getConfig();
  if(p == 0){
    return false;
  }
  
  /**
   * Setup cluster configuration data
   */
  ndb_mgm_configuration_iterator iter(* p, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, nodeid)){
    ndbout << "Invalid configuration fetched, DB missing" << endl;
    return false;
  }

  unsigned _type;
  if (iter.get(CFG_TYPE_OF_SECTION, &_type) || type != _type){
    ndbout << "No such node in configuration" << endl;
    return false;
  }

  if (iter.get(key, val)){
    ndbout << "No such key: " << key << " in configuration" << endl;
    return false;
  }
  
  return true;
}

