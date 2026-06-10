/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "libchangestreams/src/lib/mysql/cs/codec/pb/reader_state_codec_pb.h"
#include "libchangestreams/include/mysql/cs/reader/state.h"
#include "mysql/gtids/gtids.h"  // Gtid_set
#include "reader_state.pb.h"
#include "scope_guard.h"  // Scope_guard

namespace cs::reader::codec::pb::example {

// Warning: this is not for production use
void read_from_stream(std::istream &stream, cs::reader::State &out) {
  static constexpr auto return_ok = mysql::utils::Return_status::ok;
  cs::reader::codec::pb::example::State state_codec;
  std::string sibuf;

  while (stream.good()) {
    char ibuf[1024];
    stream.read(ibuf, 1024);
    sibuf.append(ibuf, stream.gcount());
  }

  // real failure
  if (stream.fail() && !stream.eof()) return;

  bool success = false;
  Scope_guard guard([&success, &stream] {
    if (!success) stream.setstate(std::ios_base::failbit);
  });

  if (!state_codec.ParseFromString(sibuf)) return;

  // NOTE: does not build with LITE_RUNTIME in some platforms
  // if (!state_codec.ParseFromIstream(&stream)) {
  //   stream.setstate(std::ios_base::failbit);
  //   return;
  // }

  mysql::gtids::Gtid_set gtid_set;
  for (const auto &pb_tsid_and_intervals : state_codec.gtids()) {
    std::string pb_uuid = pb_tsid_and_intervals.uuid();
    mysql::gtids::Tsid tsid;
    if (!mysql::strconv::decode_text(pb_uuid, tsid.uuid()).is_ok()) return;
    if (tsid.tag().assign(pb_tsid_and_intervals.tag()) != return_ok) return;
    for (const auto &pb_interval : pb_tsid_and_intervals.range()) {
      mysql::gtids::Gtid_interval interval;
      if (interval.assign(pb_interval.start(), pb_interval.end() + 1) !=
          return_ok)
        return;
      if (gtid_set.inplace_union(tsid, interval) != return_ok) return;
    }
  }
  out.add_gtid_set(gtid_set);
  success = true;
}

void write_to_stream(std::ostream &stream, cs::reader::State &in) {
  auto &gtid_set = in.get_gtids();
  cs::reader::codec::pb::example::State state_codec;

  for (const auto &[tsid, interval_set] : gtid_set) {
    auto *ranges = state_codec.add_gtids();
    ranges->set_uuid(mysql::strconv::throwing::encode_text(tsid.uuid()));
    if (!tsid.tag().empty()) {
      ranges->set_tag(mysql::strconv::throwing::encode_text(tsid.tag()));
    }
    for (const auto &interval : interval_set) {
      auto range = ranges->add_range();
      range->set_start(interval.start());
      range->set_end(interval.exclusive_end() - 1);
    }
  }

  std::string obuffer;
  if (!state_codec.SerializeToString(&obuffer)) {
    stream.setstate(std::ios_base::failbit);
    return;
  }
  stream << obuffer;

  // NOTE: does not build with LITE_RUNTIME in some platforms
  // if (!state_codec.SerializeToOstream(&stream)) {
  //   stream.setstate(std::ios_base::failbit);
  // }
}

MY_COMPILER_DIAGNOSTIC_PUSH()
// This tests a deprecated feature, so the deprecation warning is expected.
MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wdeprecated-declarations")

stringstream &stringstream::operator>>(cs::reader::State &to_decode_into) {
  cs::reader::codec::pb::example::read_from_stream(*this, to_decode_into);
  return (*this);
}

stringstream &stringstream::operator<<(cs::reader::State &to_encode_from) {
  cs::reader::codec::pb::example::write_to_stream(*this, to_encode_from);
  return (*this);
}

MY_COMPILER_DIAGNOSTIC_POP()

}  // namespace cs::reader::codec::pb::example
