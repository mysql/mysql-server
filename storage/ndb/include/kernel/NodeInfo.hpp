/*
   Copyright (C) 2003-2007 MySQL AB, 2008 Sun Microsystems, Inc.
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

#ifndef NODE_INFO_HPP
#define NODE_INFO_HPP

#include <NdbOut.hpp>
#include <mgmapi_config_parameters.h>
#include <ndb_version.h>

class NodeInfo {
public:
  NodeInfo();

  /**
   * NodeType
   */
  enum NodeType {
    DB  = NODE_TYPE_DB,      ///< Database node
    API = NODE_TYPE_API,      ///< NDB API node
    MGM = NODE_TYPE_MGM,      ///< Management node  (incl. NDB API)
    INVALID = 255 ///< Invalid type
  };
  NodeType getType() const;
  
  Uint32 m_version;       ///< Ndb version
  Uint32 m_mysql_version; ///< MySQL version
  Uint32 m_lqh_workers;   ///< LQH workers
  Uint32 m_type;          ///< Node type
  Uint32 m_connectCount;  ///< No of times connected
  bool   m_connected;     ///< Node is connected
  Uint32 m_heartbeat_cnt; ///< Missed heartbeats
  
  friend NdbOut & operator<<(NdbOut&, const NodeInfo&); 
};


inline
NodeInfo::NodeInfo(){
  m_version = 0;
  m_mysql_version = 0;
  m_lqh_workers = 0;
  m_type = INVALID;
  m_connectCount = 0;
  m_heartbeat_cnt= 0;
}

inline
NodeInfo::NodeType
NodeInfo::getType() const {
  return (NodeType)m_type;
}


class NdbVersion {
  Uint32 m_ver;
public:
  NdbVersion(Uint32 ver) : m_ver(ver) {};

  friend NdbOut& operator<<(NdbOut&, const NdbVersion&);
};


inline
NdbOut&
operator<<(NdbOut& ndbout, const NdbVersion& ver){
  ndbout.print("%d.%d.%d",
               ndbGetMajor(ver.m_ver),
               ndbGetMinor(ver.m_ver),
               ndbGetBuild(ver.m_ver));
  return ndbout;
}


inline
NdbOut &
operator<<(NdbOut& ndbout, const NodeInfo & info){
  ndbout << "[NodeInfo: ";
  switch(info.m_type){
  case NodeInfo::DB:
    ndbout << "DB";
    break;
  case NodeInfo::API:
    ndbout << "API";
    break;
  case NodeInfo::MGM:
    ndbout << "MGM";
    break;
  case NodeInfo::INVALID:
    ndbout << "INVALID";
    break;
  default:
    ndbout << "<Unknown: " << info.m_type << ">";
    break;
  }

  ndbout << " ndb version: " << NdbVersion(info.m_version)
	 << " mysql version: " << NdbVersion(info.m_mysql_version)
	 << " connect count: " << info.m_connectCount
	 << "]";
  return ndbout;
}

struct NodeVersionInfo
{
  STATIC_CONST( DataLength = 6 );
  struct 
  {
    Uint32 m_min_version;
    Uint32 m_max_version;
  } m_type [3]; // Indexed as NodeInfo::Type 
};

#endif
