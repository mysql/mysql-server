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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_MESSAGE_DECODER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_MESSAGE_DECODER_H_

#include <sys/types.h>

#include <memory>

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/message_cache.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/src/io/vio_input_stream.h"

namespace ngs {

enum class Frame_layout {
  k_frame,
  k_compressed_single_frame,
  k_compressed_multiple_frames,
  k_compressed_group_of_frames
};

class Client_interface;

/**
 X Protocol Message decoder

 Unserializes the binary data into cached protobuf message,
 so that they don't need to be reallocated every time.
*/
class Message_decoder {
 public:
  using ZeroCopyInputStream = google::protobuf::io::ZeroCopyInputStream;
  using CodedInputStream = google::protobuf::io::CodedInputStream;

  class Message_dispatcher_interface {
   public:
    virtual ~Message_dispatcher_interface() = default;

    virtual void handle(Message_request *message) = 0;
  };

  class Decode_error {
   public:
    /**
     * Constructor that marks that some internal error
     *
     * @param error_code - Internal error code and message
     */
    explicit Decode_error(const Error_code &error_code);

    /**
     * Constructor that marks that the IO occurred (failed with "errno")
     *
     * @param sys_error    error number that broke last IO operation (value of
     * "errno")
     */
    explicit Decode_error(const int sys_error);

    /**
     * Constructor that marks that user disconnected
     *
     * @param disconnected
     */
    explicit Decode_error(const bool disconnected = false);

   public:
    bool was_peer_disconnected() const;

    /**
     * This method returns error number with which last IO failed
     *
     * In case when IO error occurred, this class holds inside
     * value of "errno" which on has INT as underlying type.
     * This is enforced by return value of:
     *
     *     int vio_errno(MYSQL_VIO vio);
     *
     * and C++ standard.
     *
     * @return last system error number
     */
    int get_io_error() const;
    Error_code get_logic_error() const;

    bool was_error() const;

   private:
    bool m_disconnected{false};
    int m_sys_error{0};
    Error_code m_error_code;
  };

 public:
  Message_decoder(Message_dispatcher_interface *dispatcher,
                  Protocol_monitor_interface *monitor,
                  std::shared_ptr<Protocol_config> config);

  /**
    Parse X Protocol message by reading it from input steam and dispatch to
    external handler.

    All IO errors must be stores on stream object, which must also give user
    the possibility to check it. In case of IO error the return value might
    point a success.

    @param message_type   message that should be deserialized
    @param stream        object wrapping IO operations
    @param out_msg   returns message that is result of parsing

    @return Error_code is used only to pass logic error.
  */
  Decode_error parse_and_dispatch(const uint8_t message_type,
                                  const uint32_t message_size,
                                  xpl::Vio_input_stream *stream);

 private:
  static Error_code parse_coded_stream_generic(CodedInputStream *stream,
                                               Message *message);
  Decode_error parse_coded_stream_inner(CodedInputStream *coded_input,
                                        const uint8_t inner_message_type,
                                        const uint32_t inner_message_size);

  Decode_error parse_protobuf_frame(const uint8_t message_type,
                                    const uint32_t message_size,
                                    xpl::Vio_input_stream *net_input_stream);

  Message_dispatcher_interface *m_dispatcher;
  Protocol_monitor_interface *m_monitor;
  std::shared_ptr<Protocol_config> m_config;
  Message_cache m_cache;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_MESSAGE_DECODER_H_
