/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__DICTIONARY_OBJECT_INCLUDED
#define DD__DICTIONARY_OBJECT_INCLUDED

#include "my_global.h"

#include "dd/types/entity_object.h"       // dd::Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Dictionary_object_table;

///////////////////////////////////////////////////////////////////////////

/**
  Base class that represents a dictionary object like
  dd::Schema, dd::Table, dd::View and more. These are
  primary dictionary object which are created/searched/dropped by
  DD users.

  This class does not represent object like dd::Index, dd::Column
  which are child object of Dictionary object.
*/
class Dictionary_object : virtual public Entity_object
{
public:
  virtual ~Dictionary_object()
  { };
  virtual const Dictionary_object_table &object_table() const= 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__DICTIONARY_OBJECT_INCLUDED
