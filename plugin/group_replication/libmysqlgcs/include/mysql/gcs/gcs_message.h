/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_GCS_MESSAGE_INCLUDED
#define GCS_GCS_MESSAGE_INCLUDED

#include <stdint.h>
#include <cstring>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

#define WIRE_PAYLOAD_LEN_SIZE 8
#define WIRE_HEADER_LEN_SIZE 4

/**
 @class Gcs_message_data

 This class serves as data container for information flowing in the GCS
 ecosystem. It has been isolated in order to be used in place where a
 full-blown Gcs_message does not make sense.

 For a full usage example, check the Gcs_message documentation.
 */
class Gcs_message_data {
 public:
  /**
    Constructor of Gcs_message_data which pre-allocates space to store
    the header and payload and should be used when creating messages to
    send information to remote peers.

    @param  header_capacity Determines the header's size.
    @param  payload_capacity Determines the payload's size.
  */
  explicit Gcs_message_data(const uint32_t header_capacity,
                            const uint64_t payload_capacity);

  /**
    Constructor of Gcs_message_data which pre-allocates space to store
    a message received from a remote peer.

    @param  data_len Data's length.
  */
  explicit Gcs_message_data(const uint64_t data_len);

  virtual ~Gcs_message_data();

  /**
    Appends data to the header of the message. The data MUST have been
    previously encoded in little endian format.

    If the data to be appended exceeds the pre-allocated buffer capacity
    an error is returned.

    @param[in] to_append the data to append
    @param[in] to_append_len the length of the data to append

    @return true on error, false otherwise.
  */

  bool append_to_header(const uchar *to_append, uint32_t to_append_len);

  /**
    Appends data to the payload of the message. The data MUST have been
    previously encoded in little endian format.

    If the data to be appended exceeds the pre-allocated buffer capacity
    an error is returned.

    @param[in] to_append the data to append
    @param[in] to_append_len the length of the data to append

    @return true on error, false otherwise.
  */

  bool append_to_payload(const uchar *to_append, uint64_t to_append_len);

  /**
   Release the buffer's owership which means that this object will not
   be responsible for deallocating its internal buffer. The caller should
   do so.

   This method should be used along with the following method:
   encode(**buffer, *buffer_len).
  **/

  void release_ownership();

  /**
   Encodes the header and payload into an internal buffer. If NULL pointer
   is provided or the data was not already appended to the buffer, an error
   is returned.

   The meta data is formated in little endian format, and is structured
   on the wire as depicted below:

   -----------------------------------------------
   | header len | payload len | header | payload |
   -----------------------------------------------

   @param[in,out] buffer Variable that will hold a pointer to the buffer
   @param[in,out] buffer_len Variable that will hold the buffer's size.

   @return true on error, false otherwise.
  */

  bool encode(uchar **buffer, uint64_t *buffer_len);

  /**
   Encodes the header and payload into a buffer provided by the caller.
   If the buffer is not large enough to store the encoded data or is a
   NULL pointer, an error is returned.

   @param [in,out] buffer Buffer to store the encoded data in the message.
   @param [in,out] buffer_len The length of the buffer where the data is to be
   stored. It contains the length of the data dumped into the buffer once the
   function succeeds.

   @return true on error, false otherwise.
   */

  bool encode(uchar *buffer, uint64_t *buffer_len);

  /**
    Decodes data received via GCS and that belongs to a message. After
    decoding, all the fields will be filled. The data MUST be in little endian
    format.

    If the buffer is not large enough to store the encoded data or is a
    NULL pointer, an error is returned.

    @param[in] data Data received via network
    @param[in] data_len Data length received via network

    @return true on error, false otherwise.
  */

  bool decode(const uchar *data, uint64_t data_len);

  /**
    @return the message header in little endian format
  */

  const uchar *get_header() const;

  /**
    @return the message header length
  */

  uint32_t get_header_length() const;

  /**
    @return the message payload in little endian format
  */

  const uchar *get_payload() const;

  /**
    @return the message payload_length
  */

  uint64_t get_payload_length() const;

  /**
    @return the size of the encoded data when put on the wire.
  */

  uint64_t get_encode_size() const;

  /**
    @return the size of the encoded payload when put on the wire.
  */

  uint64_t get_encode_payload_size() const;

  /**
    @return the size of the encoded header when put on the wire.
  */

  uint64_t get_encode_header_size() const;

 private:
  /*
    Pointer to the header's buffer.
  */
  uchar *m_header;

  /*
    Pointer to the next empty position in the header's buffer.
  */
  uchar *m_header_slider;

  /*
    Length of the header's buffer in use.
  */
  uint32_t m_header_len;

  /*
    Capacity of the header's buffer.
  */
  uint32_t m_header_capacity;

  /*
    Pointer to the payload's buffer.
  */
  uchar *m_payload;

  /*
    Pointer to the next empty position in the payload's buffer.
  */
  uchar *m_payload_slider;

  /*
    Length of the payload's buffer in use.
  */
  uint64_t m_payload_len;

  /*
    Capacity of the header's buffer.
  */
  uint64_t m_payload_capacity;

  /*
    Pointer to the begining of the buffer that contains both the
    header and the payload.
  */
  uchar *m_buffer;

  /*
    Length of the buffer that contains both the header, the
    payload and metadata information.
  */
  uint64_t m_buffer_len;

  /*
    Whether the current object owns the buffer and must free it
    when deleted.
  */
  bool m_owner;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_message_data(Gcs_message_data const &);
  Gcs_message_data &operator=(Gcs_message_data const &);
};

/**
  @class Gcs_message

  Class that represents the data that is exchanged within a group. It is sent
  by a member having the group as destination. It shall be received by all
  members that pertain to the group in that moment.

  It is built using two major blocks: the header and the payload of the
  message. A user of Gcs_message can freely add data to the header and to the
  payload.

  Each binding implementation might use these two fields at its own
  discretion but that data should be removed from the header/payload
  before its delivery to the client layer.

  When the message is built, none of the data is passed unto it. One has to
  use the append_* methods in order to append data. Calling encode() at the end
  will provide a ready-to-send message.

  On the receiver side, one shall use decode() in order to have the header and
  the payload restored in the original message fields.

  A typical use case of sending a message is:

  @code{.cpp}

  Gcs_control_interface       *gcs_ctrl_interface_instance;
  Gcs_communication_interface *gcs_comm_interface_instance; // obtained
                                                            // previously

  Gcs_member_identifier *myself=
    gcs_ctrl_interface_instance->get_local_information();

  Gcs_group_identifier destination("the_group");

  Gcs_message *msg= new Gcs_message(&myself, new Gcs_message_data(0, 3));

  uchar[] some_data= {(uchar)0x12,
                      (uchar)0x24,
                      (uchar)0x00};

  msg->get_message_data()->append_to_payload(&some_data,
                                             strlen(some_data));

  gcs_comm_interface_instance->send_message(msg);
  @endcode

  A typical use case of receiving a message is:

  @code{.cpp}
  class my_Gcs_communication_event_listener : Gcs_communication_event_listener
  {
    my_Gcs_communication_event_listener(my_Message_delegator *delegator)
    {
      this->delegator= delegator;
    }

    void on_message_received(Gcs_message &message)
    {
      // Do something when message arrives...
      delegator->process_gcs_message(message);
    }

    private:
      my_Message_delegator *delegator;
  }
  @endcode
*/
class Gcs_message {
 public:
  /**
    Gcs_message 1st constructor. This is used to build full messages.

    Note that the Gcs_message will acquire ownership of the data, i.e.
    Gcs_message_data, and will be responsible for deleting it.

    @param[in] origin The group member that sent the message
    @param[in] destination The group in which this message belongs
    @param[in] message_data External message data object.
  */

  explicit Gcs_message(const Gcs_member_identifier &origin,
                       const Gcs_group_identifier &destination,
                       Gcs_message_data *message_data);

  /**
    Gcs_message 2nd constructor. This is used to send messages but with
    an external Gcs_message_data object.

    Note that the Gcs_message will acquire ownership of the data, i.e.
    Gcs_message_data, and will be responsible for deleting it.

    @param[in] origin The group member that sent the message
    @param[in] message_data External message data object.
  */

  explicit Gcs_message(const Gcs_member_identifier &origin,
                       Gcs_message_data *message_data);

  virtual ~Gcs_message();

  /**
    @return the origin of this message
  */

  const Gcs_member_identifier &get_origin() const;

  /**
    @return the destination of this message. It might be NULL.
  */

  const Gcs_group_identifier *get_destination() const;

  /**
    @return the message data to be filled.
  */

  Gcs_message_data &get_message_data() const;

 private:
  void init(const Gcs_member_identifier *origin,
            const Gcs_group_identifier *destination,
            Gcs_message_data *message_data);

  Gcs_member_identifier *m_origin;
  Gcs_group_identifier *m_destination;
  Gcs_message_data *m_data;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_message(Gcs_message const &);
  Gcs_message &operator=(Gcs_message const &);
};

#endif  // GCS_GCS_MESSAGE_INCLUDED
