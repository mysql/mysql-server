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
    REP = NODE_TYPE_REP,      ///< Replication node (incl. NDB API)
    INVALID = 255 ///< Invalid type
  };
  NodeType getType() const;
  
  Uint32 m_version;       ///< Node version
  Uint32 m_signalVersion; ///< Signal version
  Uint32 m_type;          ///< Node type
  Uint32 m_connectCount;  ///< No of times connected
  bool   m_connected;     ///< Node is connected
  
  friend NdbOut & operator<<(NdbOut&, const NodeInfo&); 
};


inline
NodeInfo::NodeInfo(){
  m_version = 0;
  m_signalVersion = 0;
  m_type = INVALID;
  m_connectCount = 0;
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
  case NodeInfo::REP:
    ndbout << "REP";
    break;
  case NodeInfo::INVALID:
    ndbout << "INVALID";
    break;
  default:
    ndbout << "<Unknown: " << info.m_type << ">";
    break;
  }

  ndbout << " version: " << info.m_version
	 << " sig. version; " << info.m_signalVersion
	 << " connect count: " << info.m_connectCount
	 << "]";
  return ndbout;
}

#endif
