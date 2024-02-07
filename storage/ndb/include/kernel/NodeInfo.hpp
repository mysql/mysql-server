/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef NODE_INFO_HPP
#define NODE_INFO_HPP

#include <mgmapi_config_parameters.h>
#include <ndb_version.h>
#include <NdbOut.hpp>

#define JAM_FILE_ID 0

class NodeInfo {
 public:
  NodeInfo()
      : m_version(0),
        m_mysql_version(0),
        m_lqh_workers(0),
        m_query_threads(0),
        m_log_parts(0),
        m_type(INVALID),
        m_connectCount(0),
        m_connected(false) {}

  /**
   * NodeType
   */
  enum NodeType {
    DB = NODE_TYPE_DB,    ///< Database node
    API = NODE_TYPE_API,  ///< NDB API node
    MGM = NODE_TYPE_MGM,  ///< Management node  (incl. NDB API)
    INVALID = 255         ///< Invalid type
  };
  NodeType getType() const;

  Uint32 m_version;        ///< Ndb version
  Uint32 m_mysql_version;  ///< MySQL version
  Uint32 m_lqh_workers;    ///< LQH workers
  Uint32 m_query_threads;  ///< Query threads
  Uint32 m_log_parts;      ///< REDO Log parts
  Uint32 m_type;           ///< Node type
  Uint32 m_connectCount;   ///< No of times connected
  Uint32 m_connected;      ///< Node is connected

  friend NdbOut &operator<<(NdbOut &, const NodeInfo &);
};

inline NodeInfo::NodeType NodeInfo::getType() const { return (NodeType)m_type; }

class NdbVersion {
  Uint32 m_ver;

 public:
  NdbVersion(Uint32 ver) : m_ver(ver) {}

  friend NdbOut &operator<<(NdbOut &, const NdbVersion &);
};

inline NdbOut &operator<<(NdbOut &ndbout, const NdbVersion &ver) {
  ndbout.print("%d.%d.%d", ndbGetMajor(ver.m_ver), ndbGetMinor(ver.m_ver),
               ndbGetBuild(ver.m_ver));
  return ndbout;
}

inline NdbOut &operator<<(NdbOut &ndbout, const NodeInfo &info) {
  ndbout << "[NodeInfo: ";
  switch (info.m_type) {
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
         << " connect count: " << info.m_connectCount << "]";
  return ndbout;
}

struct NodeVersionInfo {
  static constexpr Uint32 DataLength = 6;
  struct {
    Uint32 m_min_version;
    Uint32 m_max_version;
  } m_type[3];  // Indexed as NodeInfo::Type
};

#undef JAM_FILE_ID

#endif
