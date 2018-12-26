/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef IKEYRINGIO_INCLUDED
#define IKEYRINGIO_INCLUDED

#include <my_global.h>
#include "keyring_key.h"
#include "i_serializer.h"

namespace keyring {


class IKeyring_io : public Keyring_alloc
{
public:
  virtual my_bool init(std::string *keyring_storage_url)= 0;
  virtual my_bool flush_to_backup(ISerialized_object *serialized_object)= 0;
  virtual my_bool flush_to_storage(ISerialized_object *serialized_object)=0;

  virtual ISerializer *get_serializer()= 0;
  virtual my_bool get_serialized_object(ISerialized_object **serialized_object)= 0;
  virtual my_bool has_next_serialized_object()= 0;

  virtual ~IKeyring_io() {}
};

} //namespace keyring

#endif //IKEYRINGIO_INCLUDED
