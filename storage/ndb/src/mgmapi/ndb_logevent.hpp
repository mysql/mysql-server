/*
   Copyright (c) 2005, 2022, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#ifndef NDB_LOGEVENT_HPP
#define NDB_LOGEVENT_HPP

#include <ndb_logevent.h>

struct Ndb_logevent_body_row {
  enum Ndb_logevent_type type;     // type
  const char            *token;    // token to use for text transfer
  int                    index;                // index into theData array
  int                    (*index_fn)(int); // conversion function on the data array[index]
  int                    offset;   // offset into struct ndb_logevent
  int                    size;     // offset into struct ndb_logevent
};

extern
struct Ndb_logevent_body_row ndb_logevent_body[];

#endif
