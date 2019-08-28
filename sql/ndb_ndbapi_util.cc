/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_ndbapi_util.h"

#include <string.h> // memcpy

/*
  helper function to pack a ndb varchar
*/
char *ndb_pack_varchar(const NdbDictionary::Column *col, char *buf,
                       const char *str, int sz)
{
  switch (col->getArrayType())
  {
    case NdbDictionary::Column::ArrayTypeFixed:
      memcpy(buf, str, sz);
      break;
    case NdbDictionary::Column::ArrayTypeShortVar:
      *(uchar*)buf= (uchar)sz;
      memcpy(buf + 1, str, sz);
      break;
    case NdbDictionary::Column::ArrayTypeMediumVar:
      int2store(buf, sz);
      memcpy(buf + 2, str, sz);
      break;
  }
  return buf;
}


int
cmp_frm(const NdbDictionary::Table* ndbtab, const void* pack_data,
        size_t pack_length)
{
  DBUG_ENTER("cmp_frm");
  /*
    Compare the NDB tables FrmData with frm file blob in pack_data.
  */
  if ((pack_length != ndbtab->getFrmLength()) ||
      (memcmp(pack_data, ndbtab->getFrmData(), pack_length)))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

