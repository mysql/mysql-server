/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 
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
#ifndef NDBMEMCACHE_TABSEP_H
#define NDBMEMCACHE_TABSEP_H

#include "ndbmemcache_config.h"
#include "Record.h"

class TabSeparatedValues {
  public:
  TabSeparatedValues(const char * string, Uint32 max_parts, size_t length); 
  int advance();              // inlined
  const char * getPointer();  // inlined
  size_t getLength();         // inlined

  private:
  Uint32 index;
  Uint32 parts;
  const char * pointers[MAX_VAL_COLUMNS];
  size_t lengths[MAX_VAL_COLUMNS];
  int find_tab(const char *, int) const;
};


inline int TabSeparatedValues::advance() {
  return ++index < parts ? 1 : 0;
}

inline const char * TabSeparatedValues::getPointer() {
  return pointers[index];
}

inline size_t TabSeparatedValues::getLength() {
  return lengths[index];
}


#endif
