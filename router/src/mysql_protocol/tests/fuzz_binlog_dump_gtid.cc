/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/classic_protocol_codec_message.h"
#include "mysqlrouter/classic_protocol_message.h"

#include <iostream>

#include "hexify.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  auto data = net::const_buffer(Data, Size);

  if (data.size() < 4) return 0;

  // byte 0-3: caps
  // rest    : msg

  const uint32_t caps = *reinterpret_cast<const uint32_t *>(data.data());
  data += 4;

  using msg_type = classic_protocol::borrowed::message::client::BinlogDumpGtid;

  auto decode_res = classic_protocol::Codec<msg_type>::decode(data, caps);
  if (decode_res) {
    // if it decode, can we encode it again?

    std::vector<uint8_t> buf;
    auto encode_res = classic_protocol::encode(decode_res->second, caps,
                                               net::dynamic_buffer(buf));

    if (!encode_res) {
      std::cerr << "Encoding encoded msg failed: " << encode_res.error()
                << "\n";
      abort();
    }

    // ... and decode again?
    auto decode_again_res =
        classic_protocol::Codec<msg_type>::decode(net::buffer(buf), caps);

    if (!decode_again_res) {
      std::cerr << "Decoding encoded msg failed: " << decode_again_res.error()
                << "\n"
                << "Input:\n"
                << mysql_harness::hexify(buf) << "\n"
                << "(original input):\n"
                << mysql_harness::hexify(data) << "\n";

      abort();
    }
  }

  return 0;
}
