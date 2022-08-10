/*
   Copyright (c) 2005, 2022, Oracle and/or its affiliates.

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

#ifndef KEY_DESCRIPTOR_HPP
#define KEY_DESCRIPTOR_HPP

#include <ndb_types.h>
#include <ndb_limits.h>
#include "CArray.hpp"

#define JAM_FILE_ID 259


struct KeyDescriptor
{
  KeyDescriptor () { 
    primaryTableId = RNIL;
    noOfKeyAttr = hasCharAttr = noOfDistrKeys = noOfVarKeys = 0; 
  }
  
  Uint32 primaryTableId;
  Uint8 noOfKeyAttr;
  Uint8 hasCharAttr;
  Uint8 noOfDistrKeys;
  Uint8 noOfVarKeys;
  struct KeyAttr 
  {
    Uint32 attributeDescriptor;
    CHARSET_INFO* charsetInfo;
  } keyAttr[MAX_ATTRIBUTES_IN_INDEX];
};

extern CArray<KeyDescriptor> g_key_descriptor_pool;


#undef JAM_FILE_ID

#endif
