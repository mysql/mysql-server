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

#ifndef GCS_PLUGIN_MESSAGES_INCLUDED
#define	GCS_PLUGIN_MESSAGES_INCLUDED

#include "gcs_types.h"
#include "gcs_message.h"

#include <stdlib.h>
#include <vector>

using std::vector;

/*
  @enum payload_gcs_message_code

  Types of messages that can be sent
 */
typedef enum enum_payload_type
{
  PAYLOAD_CERTIFICATION_EVENT,
  PAYLOAD_TRANSACTION_EVENT,
  PAYLOAD_RECOVERY_EVENT,
  PAYLOAD_STATE_EXCHANGE,
  PAYLOAD_END   // enum end
} payload_gcs_message_code;

//Size of the message code when encoded to send
static const int payload_gcs_message_code_encoded_size= 2;

/*
  @class Plugin_gcs_message

  Base class for all application-level messages that are sent via GCS

  A class that inherits from this one must:
  - Create its own constructor that calls the super constructor with the
    actual message_code
  - Implement encode_message and decode_message protected methods. The public
    encode and decode are implemented in this class and will deal with the common
    encoding of the header with the message type.
 */
class Plugin_gcs_message
{
public:
  virtual ~Plugin_gcs_message();

  void encode(vector<uchar>* buf);

  void decode(uchar* buf, size_t len);

  payload_gcs_message_code get_message_code();

protected:
  /**
    Plugin_gcs_message constructor. Only to be called by derivative classes

    @param[in] message_code Message code to be sent
   */
  Plugin_gcs_message(payload_gcs_message_code message_code);

  /*
   Child classes need to define protected encoder and decoder.
   This will follow the Template pattern to ensure that the code
   is always encoded/decoded by the parent class
   */
  virtual void encode_message(vector<uchar>* buf)= 0;

  virtual void decode_message(uchar* buf, size_t len)= 0;

private:
  payload_gcs_message_code message_code;
};

/*
  @class Plugin_gcs_message_utils

  Utilities class that provide access to raw data of a Plugin_gcs_message
  This will ease processing to avoid unnecessary decodings.
 */
class Plugin_gcs_message_utils
{
public:
  Plugin_gcs_message_utils();
  virtual ~Plugin_gcs_message_utils(){}

  /**
    Retrieves from a raw buffer from a Plugin_gcs_message, the message code

    @param[in] buf raw Plugin_gcs_message data

    @return the correspondent payload_gcs_message_code
   */
  static payload_gcs_message_code retrieve_code(uchar* buf);

  /**
    Retrieves raw data from a Plugin_gcs_message

    @param[in] raw_buf raw Plugin_gcs_message data
    @return a pointer do the beginning of the data buffer
   */
  static uchar* retrieve_data(Gcs_message* msg);

  /**
    Retrieves raw data size from a Plugin_gcs_message

    @param[in] raw_buf raw Plugin_gcs_message data
    @return a pointer do the beginning of the data buffer
   */
  static size_t retrieve_length(Gcs_message* msg);
};

#endif	/* GCS_PLUGIN_MESSAGES_INCLUDED */

