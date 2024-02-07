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

#ifndef __NDB_AUTO_PTR_HPP
#define __NDB_AUTO_PTR_HPP

#include <ndb_global.h>

template <typename T>
class NdbAutoPtr {
  T *m_obj;

 public:
  NdbAutoPtr(T *obj = 0) { m_obj = obj; }
  void reset(T *obj = 0) {
    if (m_obj) free(m_obj);
    m_obj = obj;
  }
  ~NdbAutoPtr() {
    if (m_obj) free(m_obj);
  }
};

template <typename T>
class NdbAutoObjPtr {
  T *m_obj;

 public:
  NdbAutoObjPtr(T *obj = 0) { m_obj = obj; }
  void reset(T *obj = 0) {
    if (m_obj) delete m_obj;
    m_obj = obj;
  }
  ~NdbAutoObjPtr() {
    if (m_obj) delete m_obj;
  }
};

template <typename T>
class NdbAutoObjArrayPtr {
  T *m_obj;

 public:
  NdbAutoObjArrayPtr(T *obj = 0) { m_obj = obj; }
  void reset(T *obj = 0) {
    if (m_obj) delete[] m_obj;
    m_obj = obj;
  }
  ~NdbAutoObjArrayPtr() {
    if (m_obj) delete[] m_obj;
  }
};

#endif
