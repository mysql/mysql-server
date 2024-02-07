/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef RECOVERY_METADATA_MESSAGE_COMPRESSED_PARTS_INCLUDED
#define RECOVERY_METADATA_MESSAGE_COMPRESSED_PARTS_INCLUDED

#include "my_inttypes.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message.h"

/**
  This class contains custom iterator written to decode compressed
  certification info of Recovery Metadata Message.
*/
class Recovery_metadata_message_compressed_parts {
 public:
  /**
    Recovery_metadata_message_compressed_parts constructor.

    @param[in] recovery_metadata_message pointer to Recovery_metadata_message.
    @param[in] count                     Number of packets of Compressed
                                         certification info.
  */
  Recovery_metadata_message_compressed_parts(
      Recovery_metadata_message *recovery_metadata_message, uint count);

  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;

    /**
      Iterator constructor.

      @param[in] recovery_metadata_message pointer to
                                           Recovery_metadata_message.
      @param[in] packet_count              Number of packets of Compressed
                                           certification info.
    */
    Iterator(Recovery_metadata_message *recovery_metadata_message,
             uint packet_count);

    /**
      The Recovery Metadata payload may contain multiple packets of compressed
      data. The dereference operator will return tuple of compressed payload,
      compressed payload length and length of payload before it was compressed
      i.e. uncompressed payload length for one of the packet.

      @return the tuple of compressed payload, compressed payload length and
              uncompressed payload length.
    */
    std::tuple<const unsigned char *, unsigned long long, unsigned long long>
    operator*();

    /**
      The Recovery Metadata payload may contain multiple packets of compressed
      data. The prefix increment operator will increment pointer to compressed
      payload position so that it can point to next Recovery Metadata
      compressed data packet.

      @return a reference to Iterator object.
    */
    Iterator &operator++();

    /**
      The Recovery Metadata payload may contain multiple packets of compressed
      data. The postfix increment operator will increment pointer to
      compressed payload position so that it can point to next Recovery
      Metadata compressed data packet.

      @return a copy of Iterator before increment.
    */
    Iterator operator++(int);

    /**
      The Recovery Metadata payload may contain multiple packets of compressed
      data. The comparsion operator compare packets.

      @retval false   If m_count or packet number of packets is not equal.
      @retval true    If m_count or packet number of packets is equal.
    */
    bool operator==(Iterator &b);

    /**
      The Recovery Metadata payload may contain multiple packets of compressed
      data. The comparsion operator compare packets.

      @retval false   If m_count or packet number of packets is equal.
      @retval true    If m_count or packet number of packets is not equal.
    */
    bool operator!=(Iterator &b);

   private:
    /* Compressed payload position */
    const unsigned char *m_payload_pos;

    /* Compressed payload length */
    unsigned long long m_payload_length{0};

    /* Compressed payload uncompressed length */
    unsigned long long m_payload_uncompressed_length{0};

    /* Compressed payload packet number */
    uint m_count{0};

    /* Recovery_metadata_message pointer */
    Recovery_metadata_message *m_recovery_metadata_message{nullptr};
  };

  /**
    The Recovery Metadata payload may contain multiple packets of compressed
    data. This will return a pointer to the first compressed data packet.

    @return a pointer to Iterator front or first compressed data packet.
  */
  Iterator begin() noexcept;

  /**
    The Recovery Metadata payload may contain multiple packets of compressed
    data. This will return a pointer to the last compressed data packet.

    @return a pointer to Iterator back or last compressed data packet.
  */
  Iterator end() noexcept;

 private:
  /* Compressed payload position */
  const unsigned char *m_payload_start;

  /* Compressed payload packet count */
  uint m_payload_packet_count{0};

  /* Recovery_metadata_message pointer */
  Recovery_metadata_message *m_recovery_metadata_message{nullptr};
};

#endif /* RECOVERY_METADATA_MESSAGE_COMPRESSED_PARTS_INCLUDED */
