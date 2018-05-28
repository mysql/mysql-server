/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <utility>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

/**
  This class implements LZ4 compression. It is a stateless
  service class, thence it is thread safe.
 */
class Gcs_message_stage_lz4 : public Gcs_message_stage {
 private:
  /*
   Methods inherited from the Gcs_message_stage class.
   */
  virtual Gcs_message_stage::stage_status skip_apply(
      const Gcs_packet &packet) const;

  virtual Gcs_message_stage::stage_status skip_revert(
      const Gcs_packet &packet) const;

  virtual unsigned long long calculate_payload_length(Gcs_packet &packet) const;

  virtual std::pair<bool, unsigned long long> transform_payload_apply(
      unsigned int version, unsigned char *new_payload_ptr,
      unsigned long long new_payload_length, unsigned char *old_payload_ptr,
      unsigned long long old_payload_length);

  virtual std::pair<bool, unsigned long long> transform_payload_revert(
      unsigned int version, unsigned char *new_payload_ptr,
      unsigned long long new_payload_length, unsigned char *old_payload_ptr,
      unsigned long long old_payload_length);

 public:
  /**
   The default threshold value in bytes.
   */
  static const unsigned long long DEFAULT_THRESHOLD = 1024;

  /**
   Creates an instance of the stage with the default threshold in bytes set.
   */
  explicit Gcs_message_stage_lz4() : m_threshold(DEFAULT_THRESHOLD) {}

  /**
   Creates an instance of the stage with the given threshold in bytes.

   @param compress_threshold messages with the payload larger
                             than compress_threshold in bytes are compressed.
   */
  explicit Gcs_message_stage_lz4(bool enabled,
                                 unsigned long long compress_threshold)
      : Gcs_message_stage(enabled), m_threshold(compress_threshold) {}

  virtual ~Gcs_message_stage_lz4() {}

  /*
   Return the stage code.
   */
  virtual stage_code get_stage_code() const { return stage_code::ST_LZ4; }

  /**
    Sets the threshold in bytes after which compression kicks in.

    @param threshold if the payload exceeds these many bytes, then
                     the message is compressed.
   */
  void set_threshold(unsigned long long threshold) { m_threshold = threshold; }

  /**
   Return the maximum payload size in bytes that can be compressed.
   */
  static unsigned long long max_input_compression();

 private:
  /**
   This marks the threshold in bytes above which a message gets compressed.
   Messages that are smaller than this threshold are not compressed.
   */
  unsigned long long m_threshold;
};

#endif /* GCS_MESSAGE_STAGE_LZ4_H */
