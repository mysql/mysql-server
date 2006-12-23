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
