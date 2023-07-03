/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef GCS_MESSAGE_STAGE_LZ4_H
#define GCS_MESSAGE_STAGE_LZ4_H

#include <lz4.h>
#include <utility>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

/**
  This class implements LZ4 compression. It is a stateless
  service class, thence it is thread safe.
 */
class Gcs_message_stage_lz4 : public Gcs_message_stage {
 public:
  /*
   Methods inherited from the Gcs_message_stage class.
   */
  Gcs_message_stage::stage_status skip_apply(
      uint64_t const &original_payload_size) const override;

  std::unique_ptr<Gcs_stage_metadata> get_stage_header() override;

 protected:
  std::pair<bool, std::vector<Gcs_packet>> apply_transformation(
      Gcs_packet &&packet) override;

  std::pair<Gcs_pipeline_incoming_result, Gcs_packet> revert_transformation(
      Gcs_packet &&packet) override;

  Gcs_message_stage::stage_status skip_revert(
      const Gcs_packet &packet) const override;

 public:
  /**
   The default threshold value in bytes.
   */
  static constexpr unsigned long long DEFAULT_THRESHOLD = 1024;

  /**
   Creates an instance of the stage with the default threshold in bytes set.
   */
  explicit Gcs_message_stage_lz4() : m_threshold(DEFAULT_THRESHOLD) {}

  /**
   Creates an instance of the stage with the given threshold in bytes.

   @param enabled enables this message stage
   @param compress_threshold messages with the payload larger
                             than compress_threshold in bytes are compressed.
   */
  explicit Gcs_message_stage_lz4(bool enabled,
                                 unsigned long long compress_threshold)
      : Gcs_message_stage(enabled), m_threshold(compress_threshold) {}

  ~Gcs_message_stage_lz4() override = default;

  /*
   Return the stage code.
   */
  Stage_code get_stage_code() const override { return Stage_code::ST_LZ4_V1; }

  /**
    Sets the threshold in bytes after which compression kicks in.

    @param threshold if the payload exceeds these many bytes, then
                     the message is compressed.
   */
  void set_threshold(unsigned long long threshold) { m_threshold = threshold; }

  /**
   Return the maximum payload size in bytes that can be compressed.
   */
  static constexpr unsigned long long max_input_compression() noexcept {
    /*
    The code expects that the following assumption will always hold.
    */
    static_assert(
        LZ4_MAX_INPUT_SIZE <= std::numeric_limits<int>::max(),
        "Maximum input size for lz compression exceeds the expected value");
    return LZ4_MAX_INPUT_SIZE;
  }

 private:
  /**
   This marks the threshold in bytes above which a message gets compressed.
   Messages that are smaller than this threshold are not compressed.
   */
  unsigned long long m_threshold;
};

class Gcs_message_stage_lz4_v2 : public Gcs_message_stage_lz4 {
 public:
  /**
   Creates an instance of the stage with the default threshold in bytes.
   */
  explicit Gcs_message_stage_lz4_v2() : Gcs_message_stage_lz4() {}

  /**
   Creates an instance of the stage with the given threshold in bytes.

   @param enabled enables this message stage
   @param compress_threshold messages with the payload larger
   than compress_threshold in bytes are compressed.
   */
  explicit Gcs_message_stage_lz4_v2(bool enabled,
                                    unsigned long long compress_threshold)
      : Gcs_message_stage_lz4(enabled, compress_threshold) {}

  ~Gcs_message_stage_lz4_v2() override = default;

  /*
   Return the stage code.
   */
  Stage_code get_stage_code() const override { return Stage_code::ST_LZ4_V2; }
};

class Gcs_message_stage_lz4_v3 : public Gcs_message_stage_lz4 {
 public:
  /**
   Creates an instance of the stage with the default threshold in bytes.
   */
  explicit Gcs_message_stage_lz4_v3() : Gcs_message_stage_lz4() {}

  /**
   Creates an instance of the stage with the given threshold in bytes.

   @param enabled enables this message stage
   @param compress_threshold messages with the payload larger
   than compress_threshold in bytes are compressed.
   */
  explicit Gcs_message_stage_lz4_v3(bool enabled,
                                    unsigned long long compress_threshold)
      : Gcs_message_stage_lz4(enabled, compress_threshold) {}

  ~Gcs_message_stage_lz4_v3() override {}

  /*
   Return the stage code.
   */
  Stage_code get_stage_code() const override { return Stage_code::ST_LZ4_V3; }
};

#endif /* GCS_MESSAGE_STAGE_LZ4_H */
