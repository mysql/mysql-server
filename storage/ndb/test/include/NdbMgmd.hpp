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

#ifndef NDB_MGMD_HPP
#define NDB_MGMD_HPP

#include <BaseString.hpp>

#include <mgmapi.h>
#include <mgmapi_debug.h>

class NdbMgmd {
  BaseString m_connect_str;
  NdbMgmHandle m_handle;

  void error(const char* msg)
  {
    ndbout_c("NdbMgmd:%s", msg);

    if (m_handle){
      ndbout_c(" error: %d, line: %d, desc: %s",
               ndb_mgm_get_latest_error(m_handle),
               ndb_mgm_get_latest_error_line(m_handle),
               ndb_mgm_get_latest_error_desc(m_handle));
    }
  }
public:
  NdbMgmd() :
    m_connect_str(getenv("NDB_CONNECTSTRING")),
    m_handle(NULL)
    {
      if (!m_connect_str.length()){
        fprintf(stderr, "Could not init NdbConnectString");
        abort();
      }
    }

  ~NdbMgmd()
  {
    if (m_handle)
      ndb_mgm_destroy_handle(&m_handle);
  }

  NdbMgmHandle handle(void) const {
    return m_handle;
  }

  const char* getConnectString() const {
    return m_connect_str.c_str();
  }

  bool connect(void) {
    m_handle= ndb_mgm_create_handle();
    if (!m_handle){
      error("connect: ndb_mgm_create_handle failed");
      return false;
    }

    if (ndb_mgm_set_connectstring(m_handle, getConnectString()) != 0){
      error("connect: ndb_mgm_set_connectstring failed");
      return false;
    }

    if (ndb_mgm_connect(m_handle,0,0,0) != 0){
      error("connect: ndb_mgm_connect failed");
      return false;
    }
    return true;
  }

};

#endif
