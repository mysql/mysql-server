/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef __NDB_AUTO_PTR_HPP
#define __NDB_AUTO_PTR_HPP

#include <ndb_global.h>

template<typename T>
class NdbAutoPtr {
  T * m_obj;
public:
  NdbAutoPtr(T * obj = 0){ m_obj = obj;}
  void reset(T * obj = 0) { if (m_obj) free(m_obj); m_obj = obj; }
  ~NdbAutoPtr() { if (m_obj) free(m_obj);}
};

template<typename T>
class NdbAutoObjPtr {
  T * m_obj;
public:
  NdbAutoObjPtr(T * obj = 0){ m_obj = obj;}
  void reset(T * obj = 0) { if (m_obj) delete m_obj; m_obj = obj; }
  ~NdbAutoObjPtr() { if (m_obj) delete m_obj;}
};

template<typename T>
class NdbAutoObjArrayPtr {
  T * m_obj;
public:
  NdbAutoObjArrayPtr(T * obj = 0){ m_obj = obj;}
  void reset(T * obj = 0) { if (m_obj) delete[] m_obj; m_obj = obj; }
  ~NdbAutoObjArrayPtr() { if (m_obj) delete[] m_obj;}
};

#endif
