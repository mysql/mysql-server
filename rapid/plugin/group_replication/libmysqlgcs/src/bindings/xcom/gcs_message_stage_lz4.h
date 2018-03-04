/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
#define	GCS_MESSAGE_STAGE_LZ4_H

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

/**
  This class implements LZ4 compression. It is a stateless
  service class, thence it is thread safe.
 */
class Gcs_message_stage_lz4 : public Gcs_message_stage
{
private:
  void
  encode(unsigned char *hd,
         unsigned short hd_len,
         Gcs_message_stage::enum_type_code type_code,
         unsigned long long uncompressed);

  void
  decode(const unsigned char *hd,
         unsigned short *hd_len,
         Gcs_message_stage::enum_type_code *type,
         unsigned long long *uncompressed);
public:
  /**
   The on-the-wire field size for the uncompressed size field.
   */
  static const unsigned short WIRE_HD_UNCOMPRESSED_SIZE;

  /**
   The on-the-wire uncompressed size field offset within the stage header.
  */
  static const unsigned short WIRE_HD_UNCOMPRESSED_OFFSET;

  /**
   The default threshold value.
   */
  static const unsigned long long DEFAULT_THRESHOLD;

  /**
   Creates an instance of the stage with the default threshold set.
   */
  explicit Gcs_message_stage_lz4() : m_threshold(DEFAULT_THRESHOLD) { }

  /**
   Creates an instance of the stage with the given threshold.
   @param compress_threshold messages with the payload larger
                             than compress_threshold are compressed.
   */
  explicit Gcs_message_stage_lz4(unsigned long long compress_threshold)
  : m_threshold(compress_threshold) {}

  virtual ~Gcs_message_stage_lz4() { }

  virtual enum_type_code type_code() { return ST_LZ4; }

  /**
    Sets the threshold in bytes after which compression kicks in.

    @param threshold if the payload exceeds these many bytes, then
                     the message is compressed.
   */
  void set_threshold(unsigned long long threshold)
  { m_threshold= threshold; }

  /**
   This member function SHALL compress the contents of the packet and WILL
   modify its argument.

   Note that the buffer that packet contains SHALL be modified, since the
   packet will be deallocated and filled in with a new buffer that contains
   the compressed data.

   @param p the packet to encode.
   */
  virtual bool apply(Gcs_packet &p);

  /**
   This member function SHALL uncompress the contents of the packet.

   Note that the packet will be modified, since it will be deallocated and a
   new buffer with the contents of the uncompressed data shall be put inside.

   @param p the packet to uncompress.
   @return false on success, true on failures.
   */
  virtual bool revert(Gcs_packet &p);

private:

  /**
   This marks the threshold above which a message gets compressed. Messages
   that are smaller than this threshold are not compressed.
   */
  unsigned long long m_threshold;
};

#endif	/* GCS_MESSAGE_STAGE_LZ4_H */
