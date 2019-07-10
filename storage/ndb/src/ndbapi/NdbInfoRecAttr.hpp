/*
   Copyright 2009, 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef NdbInfoRecAttr_H
#define NdbInfoRecAttr_H

class NdbInfoRecAttr {
public:
  const void* ptr() const {
    return m_data;
  }

  Uint32 u_32_value() const {
    assert(m_len == sizeof(Uint32));
    return *((Uint32 *) m_data);
  }

  Uint64 u_64_value() const {
    Uint64 val;
    assert(m_len == sizeof(Uint64));
    memcpy(&val, m_data, sizeof(Uint64));
    return val;
  }

  const char* c_str() const {
    assert(m_len > 0);
    return m_data;
  }

  Uint32 length() const {
    return m_len;
  }

  bool isNULL() const {
    return !m_defined;
  }

protected:
  friend class NdbInfoScanOperation;
  NdbInfoRecAttr() : m_data(NULL), m_len(0), m_defined(false) {};
  ~NdbInfoRecAttr() {};
private:
  const char* m_data;
  Uint32 m_len;
  bool m_defined;
};

#endif
