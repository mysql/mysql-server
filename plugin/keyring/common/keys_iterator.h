/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_KEYS_ITERATOR_H
#define MYSQL_KEYS_ITERATOR_H

#include "i_keys_container.h"
#include "logger.h"
#include <vector>

namespace keyring {

class Keys_iterator
{
public:
  Keys_iterator() {};
  Keys_iterator(ILogger* logger);
  void init(void);
  bool get_key(Key_metadata **km);
  void deinit(void);

  ~Keys_iterator(void);
public:
  ILogger *logger;
  std::vector<Key_metadata> key_metadata_list;
  std::vector<Key_metadata>::iterator key_metadata_list_iterator;
};

}//namespace keyring

#endif //MYSQL_KEYS_ITERATOR_H

