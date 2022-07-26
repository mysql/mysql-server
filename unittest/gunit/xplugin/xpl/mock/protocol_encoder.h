/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_PROTOCOL_ENCODER_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_PROTOCOL_ENCODER_H_

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/protocol_encoder.h"

namespace xpl {
namespace test {
namespace mock {

class Protocol_encoder : public iface::Protocol_encoder {
 public:
  Protocol_encoder();
  virtual ~Protocol_encoder() override;

  MOCK_METHOD(bool, is_building_row, (), (const, override));
  MOCK_METHOD(bool, send_result, (const ngs::Error_code &), (override));
  MOCK_METHOD(bool, send_ok, (), (override));
  MOCK_METHOD(bool, send_ok, (const std::string &), (override));
  MOCK_METHOD(bool, send_notice,
              (const iface::Frame_type, const iface::Frame_scope,
               const std::string &, const bool),
              (override));
  MOCK_METHOD(void, send_auth_ok, (const std::string &));
  MOCK_METHOD(void, send_auth_continue, (const std::string &), (override));
  MOCK_METHOD(bool, send_exec_ok, (), (override));
  MOCK_METHOD(bool, send_result_fetch_done, (), (override));
  MOCK_METHOD(bool, send_result_fetch_suspended, (), (override));
  MOCK_METHOD(bool, send_result_fetch_done_more_results, (), (override));
  MOCK_METHOD(bool, send_result_fetch_done_more_out_params, (), (override));

  MOCK_METHOD(bool, send_column_metadata,
              (const ngs::Encode_column_info *column_info), (override));
  MOCK_METHOD(protocol::XRow_encoder *, row_builder, (), (override));
  MOCK_METHOD(protocol::XMessage_encoder *, raw_encoder, (), (override));
  MOCK_METHOD(void, start_row, (), (override));
  MOCK_METHOD(void, abort_row, (), (override));
  MOCK_METHOD(bool, send_row, (), (override));
  MOCK_METHOD(bool, send_protobuf_message,
              (const uint8_t, const ngs::Message &, bool), (override));
  MOCK_METHOD(void, on_error, (int error), (override));
  MOCK_METHOD(iface::Protocol_monitor &, get_protocol_monitor, (), (override));
  MOCK_METHOD(ngs::Metadata_builder *, get_metadata_builder, (), (override));

  MOCK_METHOD(iface::Protocol_flusher *, get_flusher, (), (override));

  MOCK_METHOD(bool, send_error,
              (const ngs::Error_code &error_code, const bool init_error),
              (override));

  MOCK_METHOD(void, send_notice_rows_affected, (const uint64_t value),
              (override));
  MOCK_METHOD(void, send_notice_client_id, (const uint64_t id), (override));
  MOCK_METHOD(void, send_notice_last_insert_id, (const uint64_t id),
              (override));
  MOCK_METHOD(void, send_notice_account_expired, (), (override));
  MOCK_METHOD(void, send_notice_generated_document_ids,
              (const std::vector<std::string> &ids), (override));

  MOCK_METHOD(void, send_notice_txt_message, (const std::string &message),
              (override));
  MOCK_METHOD(iface::Protocol_flusher *, set_flusher_raw,
              (iface::Protocol_flusher * flusher));

  std::unique_ptr<iface::Protocol_flusher> set_flusher(
      std::unique_ptr<iface::Protocol_flusher> flusher) override {
    std::unique_ptr<iface::Protocol_flusher> result{
        set_flusher_raw(flusher.get())};
    return result;
  }
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  //  UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_PROTOCOL_ENCODER_H_
