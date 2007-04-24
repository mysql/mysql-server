/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NODE_INFO_HPP
#define NODE_INFO_HPP

#include <NdbOut.hpp>
#include <mgmapi_config_parameters.h>

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
  m_type = INVALID;
  m_connectCount = 0;
  m_heartbeat_cnt= 0;
}

inline
NodeInfo::NodeType
NodeInfo::getType() const {
  return (NodeType)m_type;
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

  ndbout << " ndb version: " << info.m_version
	 << " mysql version; " << info.m_mysql_version
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
