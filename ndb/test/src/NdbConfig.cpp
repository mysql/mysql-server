/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "NdbConfig.hpp"
#include <NdbOut.hpp>
#include <NDBT_Output.hpp>
#include <assert.h>
#include <NdbConfig.h>
#include <ConfigRetriever.hpp>
#include <ndb_version.h>



bool
NdbConfig::getPropsForNode(unsigned int node_id, 
		    const char* type, 
		    const Properties ** props) const {

  /**
   * Fetch configuration from management server
   */
  char buf[255];
  ConfigRetriever cr;


  Properties * p = cr.getConfig(host,
				port,
				node_id, 
				NDB_VERSION);
  if(p == 0){
    const char * s = cr.getErrorString();
    if(s == 0)
      s = "No error given!";

    ndbout << "Could not fetch configuration" << endl;
    ndbout << s << endl;
    return false;
  } 
  
  /**
   * Setup cluster configuration data
   */
  if (!p->get("Node", node_id, props)) {
    ndbout << "Invalid configuration fetched no info for nodeId = " 
	   << node_id << endl;
    return false;
  }
  const char * str;
  if(!((*props)->get("Type", &str) && strcmp(str, type) == 0)){
    ndbout <<"Invalid configuration fetched, type != " << type << endl;
    return false;
  } 

  return true;
}

bool
NdbConfig::getProperty(unsigned int node_id,
		       const char* type,
		       const char* name, 
		       const char ** value) const {
  const Properties * db = 0;
  
  if(!getPropsForNode(node_id, type, &db)){
    return false;
  }
  
  if (!db->get(name, value)){
    ndbout << name << " not found" << endl;
    return false;
  }

  return true;
}

bool
NdbConfig::getProperty(unsigned int node_id, 
		       const char* type,
		       const char* name, 
		       Uint32 * value) const {
  const Properties * db = 0;
  
  if(!getPropsForNode(node_id, type, &db)){
    return false;
  }
  
  if (!db->get(name, value)){
    ndbout << name << " not found" << endl;
    return false;
  }

  return true;
}


bool
NdbConfig::getHostName(unsigned int node_id,
		       const char ** hostname) const {
  /**
   * Fetch configuration from management server
   */
  char buf[255];
  ConfigRetriever cr;


  Properties * p = cr.getConfig(host,
				port,
				node_id, 
				NDB_VERSION);
  if(p == 0){
    const char * s = cr.getErrorString();
    if(s == 0)
      s = "No error given!";

    ndbout << "Could not fetch configuration" << endl;
    ndbout << s << endl;
    return false;
  } 
  
  /**
   * Setup cluster configuration data
   */
  const Properties * node_props;
  if (!p->get("Node", node_id, &node_props)) {
    ndbout << "Invalid configuration fetched no info for node = " 
	   << node_id << endl;
    return false;
  }
  const char* computer_id_str;
  if (!node_props->get("ExecuteOnComputer", &computer_id_str)){
    ndbout << "ExecuteOnComputer not found" << endl;
    return false;
  }
  

  const Properties * comp_props;
  if (!p->get("Computer", atoi(computer_id_str), &comp_props)) {
    ndbout << "Invalid configuration fetched no info for computer = " 
	   << node_id << endl;
    return false;
  }
  if (!comp_props->get("HostName", hostname)){
    ndbout <<  "HostName not found" << endl;
    return false;
  }
 

  return true;
}

