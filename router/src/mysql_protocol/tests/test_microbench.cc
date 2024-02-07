/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/classic_protocol_codec_frame.h"
#include "mysqlrouter/classic_protocol_codec_message.h"

#include <array>

#include <gtest/gtest.h>

#include "unittest/gunit/benchmark.h"

namespace cl = classic_protocol;

void Query_Decode_Borrowed(size_t iter) {
  using Msg = cl::borrowed::message::client::Query;
  using Frm = cl::frame::Frame<Msg>;

  Frm frm{static_cast<uint8_t>(iter), {"foo"}};

  constexpr auto ar_size =
#if 0
      cl::Codec<Frm>({0, {"foo"}}, {}).size()
#else
      4 + 1 + 3
#endif
      ;
  static_assert(ar_size == 4 + 1 + 3);
  std::array<std::byte, 4 + 1 + 3> enc_buf{};

  // encode once, decode often.
  auto enc_res = cl::Codec<Frm>(frm, {}).encode(net::buffer(enc_buf));
  if (!enc_res) abort();
  if (*enc_res != 4 + 1 + 3) abort();

  while ((iter--) != 0) {
    auto dec_res = cl::Codec<Frm>::decode(net::buffer(enc_buf), {});
    if (!dec_res) abort();
  }
}

void Query_Encode_Borrowed(size_t iter) {
  using Msg = cl::borrowed::message::client::Query;
  using Frm = cl::frame::Frame<Msg>;

  while ((iter--) != 0) {
    Frm frm{static_cast<uint8_t>(iter), {"foo"}};
    constexpr auto ar_size =
#if 0
        cl::Codec<Frm>({0, {"foo"}}, {}).size()
#else
        4 + 1 + 3
#endif
        ;
    static_assert(ar_size == 4 + 1 + 3);

    std::array<std::byte, 4 + 1 + 3> enc_buf{};

    auto res = cl::Codec<Frm>(frm, {}).encode(net::buffer(enc_buf));
    if (!res) abort();
    if (*res != 4 + 1 + 3) abort();
  }
}

void Query_Decode(size_t iter) {
  using Msg = cl::message::client::Query;
  using Frm = cl::frame::Frame<Msg>;

  Frm frm{static_cast<uint8_t>(iter), {"foo"}};
  std::array<std::byte, 4 + 1 + 3> enc_buf{};

  // encode once, decode often.
  auto enc_res = cl::Codec<Frm>(frm, {}).encode(net::buffer(enc_buf));
  if (!enc_res) abort();
  if (*enc_res != 4 + 1 + 3) abort();

  while ((iter--) != 0) {
    auto dec_res = cl::Codec<Frm>::decode(net::buffer(enc_buf), {});
    if (!dec_res) abort();
  }
}

void Query_Encode(size_t iter) {
  using Msg = cl::message::client::Query;
  using Frm = cl::frame::Frame<Msg>;

  while ((iter--) != 0) {
    Frm frm{static_cast<uint8_t>(iter), {"foo"}};
    std::array<std::byte, 4 + 1 + 3> enc_buf{};

    auto res = cl::Codec<Frm>(frm, {}).encode(net::buffer(enc_buf));
    if (!res) abort();
    if (*res != 4 + 1 + 3) abort();
  }
}

void Ping_Encode(size_t iter) {
  using Msg = cl::message::client::Ping;
  using Frm = cl::frame::Frame<Msg>;

  while ((iter--) != 0) {
    Frm frm{static_cast<uint8_t>(iter), {}};
    std::array<std::byte, cl::Codec<Frm>({0, {}}, {}).size()> buf{};

    auto res = cl::Codec<Frm>(frm, {}).encode(net::buffer(buf));
    if (!res) abort();
    if (*res != 4 + 1) abort();
  }
}

void ResetConnection_Encode(size_t iter) {
  using Msg = cl::message::client::ResetConnection;
  using Frm = cl::frame::Frame<Msg>;

  while ((iter--) != 0) {
    Frm frm{static_cast<uint8_t>(iter), {}};
    std::array<std::byte, cl::Codec<Frm>({0, {}}, {}).size()> buf{};

    auto res = cl::Codec<Frm>(frm, {}).encode(net::buffer(buf));
    if (!res) abort();
    if (*res != 4 + 1) abort();
  }
}

void Quit_Encode(size_t iter) {
  using Msg = cl::message::client::Quit;
  using Frm = cl::frame::Frame<Msg>;

  while ((iter--) != 0) {
    Frm frm{static_cast<uint8_t>(iter), {}};
    std::array<std::byte, cl::Codec<Frm>({0, {}}, {}).size()> buf{};

    auto res = cl::Codec<Frm>(frm, {}).encode(net::buffer(buf));
    if (!res) abort();
    if (*res != 4 + 1) abort();
  }
}

BENCHMARK(ResetConnection_Encode)
BENCHMARK(Quit_Encode)
BENCHMARK(Ping_Encode)
BENCHMARK(Query_Encode)
BENCHMARK(Query_Decode)
BENCHMARK(Query_Encode_Borrowed)
BENCHMARK(Query_Decode_Borrowed)

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
