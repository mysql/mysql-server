/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef GROUP_SERVICE_MESSAGE_H
#define GROUP_SERVICE_MESSAGE_H

#include <string>
#include <vector>

#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "sql/malloc_allocator.h"

class Group_service_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: variable
    PIT_TAG = 1,

    // Length of the payload item: variable
    PIT_DATA = 2,

    // No valid type codes can appear after this one.
    PIT_MAX = 3
  };

  /**
    Group service message constructor.
  */
  Group_service_message();

  /**
    Group service message constructor for raw data.

    @param[in] buf raw data
    @param[in] len raw length
  */
  Group_service_message(const uchar *buf, size_t len);

  /**
    Group service message destructor
   */
  ~Group_service_message() override;

  /**
     Set data to message that will be transmitted to group members.
     Memory ownership belongs to the message creator.

     @param[in] data         where the data will be read
     @param[in] data_length  the length of the data to write

     @return returns false if succeeds, otherwise true is returned.
  */
  bool set_data(const uchar *data, const size_t data_length);

  /**
     Return data on message

     @return content of the message
  */
  const uchar *get_data();

  /**
     Return the length of the data on message

     @return length of the content of the message
  */
  size_t get_data_length();

  /**
     Set the tag that identifies the message

     @param[in] tag         tag name identify message

     @return returns false if succeeds, otherwise tag is invalid and true
     shall returned
  */
  bool set_tag(const char *tag);

  /**
     Return the tag that identifies the message

     @return const string that identifies content of message
  */
  const std::string &get_tag() { return m_tag; }

  /**
     Return the length of the tag that identifies the message

     @return length of the string that identifies content of message
  */
  size_t get_tag_length() { return m_tag.length(); }

 protected:
  /**
    Encodes the group service message contents for transmission.

    @param[out] buffer   the message buffer to be written
  */
  void encode_payload(std::vector<unsigned char> *buffer) const override;

  /**
    Group service message decoding method

    @param[in] buffer the received data
  */
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *) override;

 private:
  /**The message identifier*/
  std::string m_tag;
  /**The message data*/
  std::vector<uchar, Malloc_allocator<uchar>> m_data;
  /**
     A pointer to the message data, memory ownership belongs to the
     message creator.
  */
  const unsigned char *m_data_pointer;
  size_t m_data_pointer_length;
};

#endif /* GROUP_SERVICE_MESSAGE_H */
