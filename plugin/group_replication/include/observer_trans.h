/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef OBSERVER_TRANS
#define OBSERVER_TRANS

#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"

/*
  Transaction lifecycle events observers.
*/
int group_replication_trans_before_dml(Trans_param *param, int &out);

int group_replication_trans_before_commit(Trans_param *param);

int group_replication_trans_before_rollback(Trans_param *param);

int group_replication_trans_after_commit(Trans_param *param);

int group_replication_trans_after_rollback(Trans_param *param);

extern Trans_observer trans_observer;

/*
  @class Transaction_message
  Class to convey the serialized contents of the TCLE
 */
class Transaction_message : public Plugin_gcs_message, public Basic_ostream {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: variable
    PIT_TRANSACTION_DATA = 1,

    // No valid type codes can appear after this one.
    PIT_MAX = 2
  };

  /**
   Default constructor
   */
  Transaction_message();
  virtual ~Transaction_message();

  /**
     Overrides Basic_ostream::write().
     Transaction_message is a Basic_ostream. Callers can write data into
     Transaction_message's data buffer though this method.

     @param[in] buffer  where the data will be read
     @param[in] length  the length of the data to write

     @return returns false if succeeds, otherwise true is returned.
  */
  bool write(const unsigned char *buffer, my_off_t length);

  /**
     Length of data in data vector

     @return data length
  */
  my_off_t length();

 protected:
  /*
   Implementation of the template methods
   */
  void encode_payload(std::vector<unsigned char> *buffer) const;
  void decode_payload(const unsigned char *buffer, const unsigned char *);

 private:
  std::vector<uchar> data;
};

#endif /* OBSERVER_TRANS */
