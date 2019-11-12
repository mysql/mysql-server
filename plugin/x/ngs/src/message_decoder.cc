// Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include <google/protobuf/io/coded_stream.h>

#include "mysqlx_error.h"
#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/message_decoder.h"

#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

namespace details {

Message_decoder::Decode_error get_network_error(
    xpl::Vio_input_stream *net_stream) {
  using Decode_error = Message_decoder::Decode_error;
  int io_error_code;
  if (!net_stream->was_io_error(&io_error_code)) return Decode_error{};

  if (0 == io_error_code) return Decode_error{true};

  return Decode_error{io_error_code};
}

}  // namespace details

const int k_max_recursion_limit = 100;

using CodedInputStream = google::protobuf::io::CodedInputStream;
using ZeroCopyInputStream = ::google::protobuf::io::ZeroCopyInputStream;

using Decode_error = Message_decoder::Decode_error;

Message_decoder::Decode_error::Decode_error(const bool disconnected)
    : m_disconnected(disconnected) {}

Message_decoder::Decode_error::Decode_error(const int sys_error)
    : m_sys_error(sys_error) {}

Message_decoder::Decode_error::Decode_error(const Error_code &error_code)
    : m_error_code(error_code) {}

bool Message_decoder::Decode_error::was_peer_disconnected() const {
  return m_disconnected;
}

int Message_decoder::Decode_error::get_io_error() const { return m_sys_error; }

Error_code Message_decoder::Decode_error::get_logic_error() const {
  return m_error_code;
}

bool Message_decoder::Decode_error::was_error() const {
  return get_logic_error() || was_peer_disconnected() || get_io_error() != 0;
}

Message_decoder::Message_decoder(Message_dispatcher_interface *dispatcher,
                                 Protocol_monitor_interface *monitor,
                                 std::shared_ptr<Protocol_config> config)
    : m_dispatcher(dispatcher), m_monitor(monitor), m_config(config) {}

/**

X Protocol frame structure:


| 4b             | 1 b * Payload-length....              |
| Payload-length | Payload-data...                       |
|                | Message-type | Message-frame          |


Message frame has following structures depending on message-type field:

* frame format for standard X Protocol messages (messages types):

| Message frame                          |
| 1b * (payload-length - payload-header) |
| protobuf-message-payload               |

* frame format for compression messages (COMPRESS_SINGLE, COMPRESS_MULTIPLE):

| Message frame                                                               |
| 1b               | 4b               |1b * (payload-length - payload-header) |
| comp-msg-type    |Uncompressed-size | Compressed-frame                      |

* frame format for compression messages (COMPRESS_GROUP):

| Message frame                                            |
| 4b               |1b * (payload-length - payload-header) |
| Uncompressed-size | Compressed-frame                     |


The "compressed-frame" payload must be processed with selected earlier
compression algorithm, after decompression the decompressed-frame might be
either a protobuf payload or header prepended protobuf payloads, depending on
"Message_type":

* COMPRESS_SINGLE:

| Decompressed-frame       |
| 1b *  Uncompressed-size  |
| protobuf-message-payload |

* COMPRESS_MULTIPLE:

| Decompressed-frame | | 4b             | 1b * protobuf-size1 | 4b             |
1b * protobuf-size2 |...     | | protobuf-size1 | protobuf-payload1   |
protobuf-size2 | protobuf-payload2   |...     |

* COMPRESS_GROUP:

| Decompressed-frame                                      |
| 4b             | 1b        | 1b * protobuf-size1 |...   |
| protobuf-size1 | msg-type1 | protobuf-payload1   |...   |


Potential problems:

1.  X Protocol frame header empty         - send error, drop connection
2.  X Protocol frame too big       - drop connection
3.  X Protocol frame unparsed data - send error, drop connection
4.  X Protocol frame protobuf msg not known  - send error, dispatched, skip data
5.  X Protocol frame protobuf msg not initialized - send error, drop connection
6.  X Protocol frame protobuf msg nested objlimit - send error, drop connection
7.  X Protocol frame compressed header missing    - send error, drop connection
8.  X Protocol frame compressed unparsed data     - send error, drop connection
9.  X Protocol compressed sub-frame empty
   a. frame-single - try to apply following rules
   b. frame-multiple - if internal-length missing then send error, drop
      connection
   c. frame-group - if internal-length or type missing then send error, drop
      connection
10. X Protocol compressed sub-frame too big - drop connection
11. X Protocol compressed sub-frame unparsed data - send error, drop connection
12. X Protocol compressed payload invalid - send error, drop connection
13. X Protocol compressed sub-frame protobuf msg not known - send error,
    dispatched, skip data
14. X Protocol compressed sub-frame protobuf msg not initialized - send error,
    drop connection
15. X Protocol compressed sub-frame protobuf msg nested objlimit - send error,
    drop connection

Covered by:

1. message_empty_payload.test
2. message_too_large.test
3. message_not_parsed_data.test
4. status_variable_errors_unknown_message_type.test
5. message_not_initialized.test
6. message_protobuf_nested.test
7. message_compressed_empty.test
9. message_compressed_empty.test
10.message_compressed_payload.test
11.not-testable
12.message_compressed_payload.test
13.message_compressed_payload.test
14.message_compressed_payload.test
15.message_compressed_payload.test

*/
Decode_error Message_decoder::parse_and_dispatch(
    const uint8_t message_type, const uint32_t message_size,
    xpl::Vio_input_stream *net_input_stream) {
  return parse_protobuf_frame(message_type, message_size, net_input_stream);
}

Error_code Message_decoder::parse_coded_stream_generic(
    google::protobuf::io::CodedInputStream *stream, Message *message) {
  DBUG_TRACE;
  // Protobuf limits the number of nested objects when decoding messages
  // lets set the value in explicit way (to ensure that is set accordingly
  // with out stack size)
  //
  // Protobuf doesn't print a readable error after reaching the limit
  // thus in case of failure we try to validate the limit by decrementing
  // and incrementing the value & checking result for failure
  stream->SetRecursionLimit(k_max_recursion_limit);
  if (!message->ParseFromCodedStream(stream)) {
    // Workaround
    stream->DecrementRecursionDepth();
    if (!stream->IncrementRecursionDepth()) {
      return Error(ER_X_BAD_MESSAGE,
                   "X Protocol message recursion limit (%i) exceeded",
                   k_max_recursion_limit);
    }

    return Error_code(ER_X_BAD_MESSAGE,
                      "Parse error unserializing protobuf message");
  }

  return {};
}

Decode_error Message_decoder::parse_protobuf_frame(
    const uint8_t message_type, const uint32_t message_size,
    xpl::Vio_input_stream *net_stream) {
  DBUG_TRACE;
  Message_request request;

  m_cache.alloc_message(message_type, &request);

  if (request.get_message()) {
    google::protobuf::io::CodedInputStream stream(net_stream);

    stream.SetTotalBytesLimit(std::numeric_limits<int>::max(), -1);
    stream.PushLimit(message_size);
    // variable 'mysqlx_max_allowed_packet' has been checked when buffer was
    // filling by data
    auto error = parse_coded_stream_generic(&stream, request.get_message());

    // If an network error occur, then it might be the reason of
    // error generated by `parse_coded_stream_generic`, thus IO errors
    // have much higher priority.
    if (net_stream->was_io_error())
      return details::get_network_error(net_stream);

    if (error) return Decode_error{error};

    if (stream.BytesUntilLimit())
      return Decode_error{
          Error_code(ER_X_BAD_MESSAGE, "Invalid message-frame.")};
  }

  m_dispatcher->handle(&request);

  return Decode_error{};
}

}  // namespace ngs
