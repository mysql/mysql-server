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

#ifndef GCS_MSG_H
#define GCS_MSG_H

#include <cassert>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <vector>
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message_headers.h"

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_synode.h"

class Gcs_message_pipeline;

/**
 Deleter for objects managed by a std::unique_ptr that were allocated using
 the malloc family of functions instead of the new operator.
 */
struct Gcs_packet_buffer_deleter {
  void operator()(unsigned char *buffer) const { std::free(buffer); }
};

/**
 This class is an abstraction for the packet concept. It is used to manipulate
 the contents of a buffer that is to be sent to the network in an optimal way.

 The on-the-wire layout looks like this:

 +--------------+-----------------+----------------+-----------+
 | fixed header | dynamic headers | stage metadata |  payload  |
 +--------------+-----------------+----------------+-----------+
 */
class Gcs_packet {
 public:
  using buffer_ptr = std::unique_ptr<unsigned char, Gcs_packet_buffer_deleter>;

 private:
  /**
   Fixed header which is common regardless whether the packet has been
   changed by a stage or not.
  */
  Gcs_internal_message_header m_fixed_header;

  /**
   List of dynamic headers created by the stages by which the packet has
   passed through and changed it.
  */
  std::vector<Gcs_dynamic_header> m_dynamic_headers;

  /**
   List of stage metadata created by the stages by which the packet has passed
   through.
   This list always has the same length as m_dynamic_headers, and the
   following holds:

   For every i, m_stage_metadata[i] and m_dynamic_headers[i] correspond to
   the same stage.
   */
  std::vector<std::unique_ptr<Gcs_stage_metadata>> m_stage_metadata;

  /**
   Index of the next stage to apply/revert in both m_dynamic_headers and
   m_stage_metadata.
   */
  std::size_t m_next_stage_index{0};

  /**
   The buffer containing all serialized data for this packet.
   */
  buffer_ptr m_serialized_packet;

  /**
   The capacity of the serialization buffer.
   */
  unsigned long long m_serialized_packet_size{0};

  /**
   The offset in m_serialized_packet where the application payload starts.
   */
  std::size_t m_serialized_payload_offset{0};

  /**
   The size of the serialized application payload in m_serialized_packet.
   */
  unsigned long long m_serialized_payload_size{0};

  /**
   The size of the serialized m_stage_metadata in m_serialized_packet.
   */
  unsigned long long m_serialized_stage_metadata_size{0};

  /**
   The XCom synode in which this packet was delivered.
   */
  Gcs_xcom_synode m_delivery_synode;

  /**
   The XCom synode in which this packet was delivered.
   */
  Gcs_xcom_synode m_origin_synode;

 public:
  /**
   This factory method is to be used when sending a packet.

   @param cargo The message type
   @param current_version The pipeline version
   @param dynamic_headers The dynamic headers of the stages the packet will go
   through
   @param stage_metadata The stage metadata of the stages the packet will go
   through
   @param payload_size The payload size
   @retval {true, Gcs_packet} If packet is created successfully
   @retval {false, _} If memory could not be allocated
   */
  static std::pair<bool, Gcs_packet> make_outgoing_packet(
      Cargo_type const &cargo, Gcs_protocol_version const &current_version,
      std::vector<Gcs_dynamic_header> &&dynamic_headers,
      std::vector<std::unique_ptr<Gcs_stage_metadata>> &&stage_metadata,
      unsigned long long const &payload_size);

  /**
   This factory method is to be used when modifying a packet. This builds a
   packet with all the same headers, metadata, and state of @c original_packet.

   It is used, for example, by:
   - The compression stage of the pipeline, to derive the compressed packet from
     the original, uncompressed packet.
   - The fragmentation stage of the pipeline, to derive the fragments from the
     original packet.

   @param original_packet The packet to "clone"
   @param new_payload_size The payload size of this packet
   @retval {true, Gcs_packet} If packet is created successfully
   @retval {false, _} If memory could not be allocated
   */
  static std::pair<bool, Gcs_packet> make_from_existing_packet(
      Gcs_packet const &original_packet,
      unsigned long long const &new_payload_size);

  /**
   This factory method is to be used when receiving a packet from the network.

   @param buffer Buffer with a serialized packet
   @param buffer_size Size of the buffer
   @param delivery_synode The XCom synode where the packet was decided on
   @param origin_synode The XCom synode that identifies the origin of the packet
   @param pipeline The message pipeline
   @returns A packet initialized from the buffer
   */
  static Gcs_packet make_incoming_packet(buffer_ptr &&buffer,
                                         unsigned long long buffer_size,
                                         synode_no const &delivery_synode,
                                         synode_no const &origin_synode,
                                         Gcs_message_pipeline const &pipeline);

  Gcs_packet() noexcept;

  /**
   These constructors are to be used when move semantics may be needed.
   */
  Gcs_packet(Gcs_packet &&packet) noexcept;
  Gcs_packet &operator=(Gcs_packet &&packet) noexcept;

  Gcs_packet(const Gcs_packet &packet) = delete;
  Gcs_packet &operator=(const Gcs_packet &packet) = delete;

  /**
   Retrieve this packet's header.
   @returns The packet's header
   */
  Gcs_internal_message_header const &get_fixed_header() const;

  /**
   Retrieve this packet's dynamic headers.
   @returns The packet's dynamic headers
   */
  std::vector<Gcs_dynamic_header> const &get_dynamic_headers() const;

  /**
   Retrieve this packet's stage metadata.
   @returns The packet's stage metadata
   */
  std::vector<std::unique_ptr<Gcs_stage_metadata>> const &get_stage_metadata()
      const;

  std::size_t const &get_next_stage_index() const;

  void prepare_for_next_outgoing_stage();
  void prepare_for_next_incoming_stage();

  Gcs_dynamic_header &get_current_dynamic_header();

  Gcs_stage_metadata &get_current_stage_header();

  unsigned char *get_payload_pointer();

  void set_payload_length(unsigned long long const &new_length);

  /**
   Return the value of the maximum supported version.
   */
  Gcs_protocol_version get_maximum_version() const;

  /**
   Return the value of the version in use.
   */
  Gcs_protocol_version get_used_version() const;

  /**
   Return the cargo type.
   */
  Cargo_type get_cargo_type() const;

  /**
   Return the total length.
   */
  unsigned long long get_total_length() const;

  /**
   Return the payload length.
   */
  unsigned long long const &get_payload_length() const;

  /**
   Encode the packet content into its serialization buffer, and release
   ownership of the serialization buffer.

   This method must only be called on a valid packet, i.e. a packet for which
   @c allocate_serialization_buffer was called and returned true.

   @retval {buffer, buffer_size} The buffer with the serialized packet, and
   its size
   */
  std::pair<buffer_ptr, unsigned long long> serialize();

  /**
   Create a string representation of the packet to be logged.

   @param output Reference to the output stream where the string will be
   created.
   */
  void dump(std::ostringstream &output) const;

  Gcs_xcom_synode const &get_delivery_synode() const;

  Gcs_xcom_synode const &get_origin_synode() const;

 private:
  /**
   Constructor called by @c make_to_send.

   @param cargo The message type
   @param current_version The pipeline version
   @param dynamic_headers The dynamic headers of the stages the packet will go
   through
   @param stage_metadata The stage metadata of the stages the packet will go
   through
   @param payload_size The payload size
   */
  explicit Gcs_packet(
      Cargo_type const &cargo, Gcs_protocol_version const &current_version,
      std::vector<Gcs_dynamic_header> &&dynamic_headers,
      std::vector<std::unique_ptr<Gcs_stage_metadata>> &&stage_metadata,
      unsigned long long const &payload_size);

  /**
   Constructor called by @c make_from_existing_packet.

   @param original_packet The packet to "clone"
   @param new_payload_size The payload size of this packet
   */
  explicit Gcs_packet(Gcs_packet const &original_packet,
                      unsigned long long const &new_payload_size);

  /**
   Constructor called by @c make_from_serialized_buffer.

   @param delivery_synode The XCom synode where the packet was decided on
   @param origin_synode The XCom synode that identifieis the origin of this
                        packet
   */
  explicit Gcs_packet(synode_no const &delivery_synode,
                      synode_no const &origin_synode);

  /**
   Allocates the underlying buffer where the packet will be serialized to using
   @c serialize.

   @returns true if the required buffer could not be allocated, false otherwise
   */
  bool allocate_serialization_buffer();

  /**
   Decode the packet content from the given buffer containing a serialized
   packet.

   @param buffer Buffer containing a serialized packet
   @param buffer_size Size of the buffer
   @param pipeline The message pipeline
   */
  void deserialize(buffer_ptr &&buffer, unsigned long long buffer_size,
                   Gcs_message_pipeline const &pipeline);
};

#endif  // GCS_MSG_H
