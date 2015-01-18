/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_GCS_MESSAGE_INCLUDED
#define GCS_GCS_MESSAGE_INCLUDED

#include "gcs_group_identifier.h"
#include "gcs_member_identifier.h"
#include "gcs_types.h"

#include <vector>
#include <cstring>

#define GCS_MESSAGE_HEADER_SIZE_FIELD_LENGTH 4

using std::vector;

/**
  @enum message_delivery_guarantee

  This enumeration describes the QoS in which the Group Communication System(GCS)
  messages may be delivered. Binding implementations might not support all
  modes. Please refer to the GCS documentation for the supported modes.
 */
typedef enum message_delivery_guarantee
{
  //Mode in which messages have no guarantee of order
  NO_ORDER,
  /*
   Mode in which messages are delivered in all nodes in the same order.
   */
  TOTAL_ORDER,
  /*
    Mode in which, besides guaranteeing TOTAL_ORDER, it ensures that if a
    correct node delivers the message, then all correct node will do
    it also.
  */
  UNIFORM
} gcs_message_delivery_guarantee;

#define GCS_MESSAGE_DELIVERY_GUARANTEE_SIZE 2

/**
  @class Gcs_message

  Class that represents the data that is exchanged within a group. It is sent
  by a node having as destination the group. Thus,it is received by all members
  that pertain to the group in that moment.

  It is built using two major blocks: the header and the payload of the message.
  Each of the binding implementation will use these two fields at its own
  discretion.

  When the message is built, none of the data is passed unto it. One has to
  use the append_* methods in order to append data. Calling encode() at the end
  will provide a ready-to-send message.

  On the receiver side, one shall use decode() in order to have the header and
  the payload restored in the original message fields.

 */
class Gcs_message
{
public:
  /**
    Gcs_message constructor

    @param[in] origin The group member that sent the message
    @param[in] destination The group in which this message belongs
    @param[in] guarantee the delivery guarantee of the message
   */
  Gcs_message(Gcs_member_identifier origin,
              Gcs_group_identifier destination,
              gcs_message_delivery_guarantee guarantee);

  virtual ~Gcs_message();

  /**
    @return the message header
  */
  uchar* get_header();

  /**
    @return the message header length
  */
  size_t get_header_length();

  /**
    @return the message payload
  */
  uchar* get_payload();

  /**
    @return the message payload_length
  */
  size_t get_payload_length();

  /**
    @return the origin of this message
   */
  Gcs_member_identifier* get_origin();

  /**
    @return the destination of this message
   */
  Gcs_group_identifier* get_destination();

  /**
   Appends data to the header of the message

    @param[in] to_append the data to append
    @param[in] to_append_len the length of the data to append
   */
  void append_to_header(uchar* to_append, size_t to_append_len);

  /**
    Appends data to the payload of the message

    @param[in] to_append
    @param[in] to_append_len
   */
  void append_to_payload(uchar* to_append, size_t to_append_len);

  /**
    @return the delivery guarantee of this message
   */
  gcs_message_delivery_guarantee get_delivery_guarantee();

  /**
   Encodes the message header and payload into a uchar vector ready to send

    @return a reference to the encoded data in a vector
   */
  vector<uchar>* encode();

  /**
    Decodes data received via GCS and that belongs to a message. After decoding,
    all the message fields will be filled.

    @param[in] data Data received via network
    @param[in] data_len Data length received via network
   */
  void decode(uchar* data, size_t data_len);

private:

  /**
   Helper method to encode size_t

    @param[in] to_encode size_t to encode
    @return the encoded data
   */
  uchar* encode_size_t(size_t to_encode);

  vector<uchar> *header;
  vector<uchar> *payload;
  Gcs_member_identifier* origin;
  Gcs_group_identifier* destination;
  gcs_message_delivery_guarantee guarantee;
};

#endif // GCS_GCS_MESSAGE_INCLUDED
