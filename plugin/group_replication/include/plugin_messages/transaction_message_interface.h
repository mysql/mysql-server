/* Copyright (c) 2018, 2021, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TRANSACTION_MESSAGE_INTERFACE_INCLUDED
#define TRANSACTION_MESSAGE_INTERFACE_INCLUDED

#include <mysql/group_replication_priv.h>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"

/*
  @class Transaction_message_interface

  Parent class of the transaction messages-
 */
class Transaction_message_interface : public Plugin_gcs_message,
                                      public Basic_ostream {
 public:
  explicit Transaction_message_interface(enum_cargo_type cargo_type)
      : Plugin_gcs_message(cargo_type) {}
  ~Transaction_message_interface() override {}

  /**
     Length of data in data vector

     @return data length
  */
  virtual my_off_t length() = 0;
};

#endif /* TRANSACTION_MESSAGE_INTERFACE_INCLUDED */
