/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

Gcs_packet::Gcs_packet() noexcept
    : m_fixed_header(),
      m_dynamic_headers(),
      m_stage_metadata(),
      m_next_stage_index(0),
      m_serialized_packet(nullptr, Gcs_packet_buffer_deleter()),
      m_serialized_packet_size(0),
      m_serialized_payload_offset(0),
      m_serialized_payload_size(0),
      m_serialized_stage_metadata_size(0),
      m_delivery_synode(),
      m_origin_synode() {}

std::pair<bool, Gcs_packet> Gcs_packet::make_outgoing_packet(
    Cargo_type const &cargo, Gcs_protocol_version const &current_version,
    std::vector<Gcs_dynamic_header> &&dynamic_headers,
    std::vector<std::unique_ptr<Gcs_stage_metadata>> &&stage_metadata,
    unsigned long long const &payload_size) {
  bool successful = true;

  Gcs_packet packet(cargo, current_version, std::move(dynamic_headers),
                    std::move(stage_metadata), payload_size);

  bool const couldnt_allocate = packet.allocate_serialization_buffer();

  // Do not return a partially initialized packet.
  if (couldnt_allocate) {
    /* purecov: begin inspected */
    packet = Gcs_packet();
    successful = false;
    /* purecov: end */
  }

  return std::make_pair(successful, std::move(packet));
}

Gcs_packet::Gcs_packet(
    Cargo_type const &cargo, Gcs_protocol_version const &current_version,
    std::vector<Gcs_dynamic_header> &&dynamic_headers,
    std::vector<std::unique_ptr<Gcs_stage_metadata>> &&stage_metadata,
    unsigned long long const &payload_size)
    : m_fixed_header(),
      m_dynamic_headers(std::move(dynamic_headers)),
      m_stage_metadata(std::move(stage_metadata)),
      m_next_stage_index(0),
      m_serialized_packet(nullptr, Gcs_packet_buffer_deleter()),
      m_serialized_packet_size(0),
      m_serialized_payload_offset(0),
      m_serialized_payload_size(0),
      m_serialized_stage_metadata_size(0),
      m_delivery_synode(),
      m_origin_synode() {
  auto const nr_stages = m_dynamic_headers.size();
  assert(nr_stages == m_stage_metadata.size());

  /* Calculate the size of the stage metadata. */
  for (auto const &metadata : m_stage_metadata) {
    m_serialized_stage_metadata_size += metadata->calculate_encode_length();
  }

  /* Populate the fixed header. */
  m_fixed_header.set_used_version(current_version);
  m_fixed_header.set_maximum_version(Gcs_protocol_version::HIGHEST_KNOWN);
  m_fixed_header.set_dynamic_headers_length(
      nr_stages * Gcs_dynamic_header::calculate_length());
  m_fixed_header.set_cargo_type(cargo);
  set_payload_length(payload_size);
}

std::pair<bool, Gcs_packet> Gcs_packet::make_from_existing_packet(
    Gcs_packet const &original_packet,
    unsigned long long const &new_payload_size) {
  bool successful = true;

  Gcs_packet packet(original_packet, new_payload_size);

  bool const couldnt_allocate = packet.allocate_serialization_buffer();

  // Do not return a partially initialized packet.
  if (couldnt_allocate) {
    /* purecov: begin inspected */
    packet = Gcs_packet();
    successful = false;
    /* purecov: end */
  }

  return std::make_pair(successful, std::move(packet));
}

Gcs_packet::Gcs_packet(Gcs_packet const &original_packet,
                       unsigned long long const &new_payload_size)
    : m_fixed_header(original_packet.get_fixed_header()),
      m_dynamic_headers(original_packet.get_dynamic_headers()),
      m_stage_metadata(),
      m_next_stage_index(original_packet.get_next_stage_index()),
      m_serialized_packet(nullptr, Gcs_packet_buffer_deleter()),
      m_serialized_packet_size(0),
      m_serialized_payload_offset(0),
      m_serialized_payload_size(new_payload_size),
      m_serialized_stage_metadata_size(0),
      m_delivery_synode(original_packet.get_delivery_synode()),
      m_origin_synode(original_packet.get_origin_synode()) {
  /* Copy the stage metadata. */
  for (auto const &original_metadata : original_packet.get_stage_metadata()) {
    auto metadata = original_metadata->clone();
    m_serialized_stage_metadata_size += metadata->calculate_encode_length();
    m_stage_metadata.push_back(std::move(metadata));
  }

  /* Update this packet's payload length. */
  set_payload_length(new_payload_size);
}

Gcs_packet Gcs_packet::make_incoming_packet(
    buffer_ptr &&buffer, unsigned long long buffer_size,
    synode_no const &delivery_synode, synode_no const &origin_synode,
    Gcs_message_pipeline const &pipeline) {
  Gcs_packet packet(delivery_synode, origin_synode);

  packet.deserialize(std::move(buffer), buffer_size, pipeline);

  return packet;
}

Gcs_packet::Gcs_packet(synode_no const &delivery_synode,
                       synode_no const &origin_synode)
    : m_fixed_header(),
      m_dynamic_headers(),
      m_stage_metadata(),
      m_next_stage_index(0),
      m_serialized_packet(nullptr, Gcs_packet_buffer_deleter()),
      m_serialized_packet_size(0),
      m_serialized_payload_offset(0),
      m_serialized_payload_size(0),
      m_serialized_stage_metadata_size(0),
      m_delivery_synode(delivery_synode),
      m_origin_synode(origin_synode) {}

Gcs_packet::Gcs_packet(Gcs_packet &&packet) noexcept
    : m_fixed_header(std::move(packet.m_fixed_header)),
      m_dynamic_headers(std::move(packet.m_dynamic_headers)),
      m_stage_metadata(std::move(packet.m_stage_metadata)),
      m_next_stage_index(std::move(packet.m_next_stage_index)),
      m_serialized_packet(std::move(packet.m_serialized_packet)),
      m_serialized_packet_size(std::move(packet.m_serialized_packet_size)),
      m_serialized_payload_offset(
          std::move(packet.m_serialized_payload_offset)),
      m_serialized_payload_size(std::move(packet.m_serialized_payload_size)),
      m_serialized_stage_metadata_size(
          std::move(packet.m_serialized_stage_metadata_size)),
      m_delivery_synode(std::move(packet.m_delivery_synode)),
      m_origin_synode(std::move(packet.m_origin_synode)) {
  packet.m_fixed_header = Gcs_internal_message_header();
  packet.m_next_stage_index = 0;
  packet.m_serialized_packet_size = 0;
  packet.m_serialized_payload_offset = 0;
  packet.m_serialized_payload_size = 0;
  packet.m_serialized_stage_metadata_size = 0;
}

Gcs_packet &Gcs_packet::operator=(Gcs_packet &&packet) noexcept {
  m_fixed_header = std::move(packet.m_fixed_header);
  m_dynamic_headers = std::move(packet.m_dynamic_headers);
  m_stage_metadata = std::move(packet.m_stage_metadata);
  m_next_stage_index = std::move(packet.m_next_stage_index);
  m_serialized_packet = std::move(packet.m_serialized_packet);
  m_serialized_packet_size = packet.m_serialized_packet_size;
  m_serialized_payload_offset = std::move(packet.m_serialized_payload_offset);
  m_serialized_payload_size = std::move(packet.m_serialized_payload_size);
  m_serialized_stage_metadata_size =
      std::move(packet.m_serialized_stage_metadata_size);
  m_delivery_synode = std::move(packet.m_delivery_synode);
  m_origin_synode = std::move(packet.m_origin_synode);

  packet.m_fixed_header = Gcs_internal_message_header();
  packet.m_next_stage_index = 0;
  packet.m_serialized_packet_size = 0;
  packet.m_serialized_payload_offset = 0;
  packet.m_serialized_payload_size = 0;
  packet.m_serialized_stage_metadata_size = 0;

  return (*this);
}

Gcs_internal_message_header const &Gcs_packet::get_fixed_header() const {
  return m_fixed_header;
}

std::vector<Gcs_dynamic_header> const &Gcs_packet::get_dynamic_headers() const {
  return m_dynamic_headers;
}

std::vector<std::unique_ptr<Gcs_stage_metadata>> const &

Gcs_packet::get_stage_metadata() const {
  return m_stage_metadata;
}

std::size_t const &Gcs_packet::get_next_stage_index() const {
  return m_next_stage_index;
}

void Gcs_packet::prepare_for_next_outgoing_stage() { m_next_stage_index++; }

void Gcs_packet::prepare_for_next_incoming_stage() { m_next_stage_index--; }

Gcs_dynamic_header &Gcs_packet::get_current_dynamic_header() {
  return m_dynamic_headers.at(m_next_stage_index);
}

Gcs_stage_metadata &Gcs_packet::get_current_stage_header() {
  return *m_stage_metadata.at(m_next_stage_index);
}

unsigned char *Gcs_packet::get_payload_pointer() {
  return &m_serialized_packet.get()[m_serialized_payload_offset];
}

void Gcs_packet::set_payload_length(unsigned long long const &new_length) {
  m_serialized_payload_size = new_length;
  m_fixed_header.set_payload_length(m_serialized_stage_metadata_size +
                                    new_length);
}

Gcs_protocol_version Gcs_packet::get_maximum_version() const {
  return m_fixed_header.get_maximum_version();
}

Gcs_protocol_version Gcs_packet::get_used_version() const {
  return m_fixed_header.get_used_version();
}

Cargo_type Gcs_packet::get_cargo_type() const {
  return m_fixed_header.get_cargo_type();
}

unsigned long long Gcs_packet::get_total_length() const {
  return m_fixed_header.get_total_length();
}

unsigned long long const &Gcs_packet::get_payload_length() const {
  return m_serialized_payload_size;
}

bool Gcs_packet::allocate_serialization_buffer() {
  assert(m_serialized_payload_size > 0);

  bool error = true;

  /* Allocate the serialization buffer. */
  unsigned long long buffer_size = m_fixed_header.get_total_length();
  unsigned char *buffer =
      static_cast<unsigned char *>(std::malloc(buffer_size));
  if (buffer != nullptr) {
    m_serialized_packet.reset(buffer);
    m_serialized_packet_size = buffer_size;
    m_serialized_payload_offset = buffer_size - m_serialized_payload_size;
    error = false;
  }

  return error;
}

std::pair<Gcs_packet::buffer_ptr, unsigned long long> Gcs_packet::serialize() {
  assert(m_serialized_packet.get() != nullptr);

  // Serialize the headers.
  unsigned char *slider = m_serialized_packet.get();
  slider += m_fixed_header.encode(slider);
  for (auto &dynamic_header : m_dynamic_headers) {
    slider += dynamic_header.encode(slider);
  }
  for (auto &stage_header : m_stage_metadata) {
    slider += stage_header->encode(slider);
  }

  // clang-format off
  MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(
      GCS_DEBUG_MSG_FLOW,
      std::ostringstream output;
      dump(output);
      MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW, "Output %s",
                                      output.str().c_str());
  )
  // clang-format on

  // We transfer ownership of the serialized buffer to the caller.
  m_serialized_packet_size = 0;
  m_serialized_payload_size = 0;

  return std::make_pair(std::move(m_serialized_packet),
                        m_fixed_header.get_total_length());
}

void Gcs_packet::deserialize(buffer_ptr &&buffer,
                             unsigned long long buffer_size,
                             Gcs_message_pipeline const &pipeline) {
  m_serialized_packet = std::move(buffer);
  m_serialized_packet_size = buffer_size;

  unsigned char *slider = m_serialized_packet.get();

  slider += m_fixed_header.decode(slider);

  // Decode the dynamic headers.
  unsigned long long processed_size = 0;
  for (unsigned long long size = m_fixed_header.get_dynamic_headers_length();
       size > 0; size -= processed_size) {
    Gcs_dynamic_header dynamic_header;
    processed_size = dynamic_header.decode(slider);
    m_dynamic_headers.push_back(std::move(dynamic_header));
    slider += processed_size;
  }

  // Decode the stage headers.
  processed_size = 0;
  for (Gcs_dynamic_header const &dynamic_header : m_dynamic_headers) {
    auto const &stage_code = dynamic_header.get_stage_code();
    auto &stage = pipeline.get_stage(stage_code);
    m_stage_metadata.push_back(stage.get_stage_header());
    auto &stage_header = m_stage_metadata.back();
    processed_size = stage_header->decode(slider);
    slider += processed_size;
  }
  m_serialized_stage_metadata_size = processed_size;

  // Adjust payload information.
  m_serialized_payload_offset = slider - m_serialized_packet.get();
  m_serialized_payload_size =
      m_serialized_packet.get() + m_fixed_header.get_total_length() - slider;

  // Ready to be processed by the pipeline.
  m_next_stage_index = (m_dynamic_headers.size() - 1);

  // clang-format off
  MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(
      GCS_DEBUG_MSG_FLOW,
      std::ostringstream output;
      dump(output);
      MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW, "Input %s",
                                      output.str().c_str());
  )
  // clang-format on
}

void Gcs_packet::dump(std::ostringstream &output) const {
  m_fixed_header.dump(output);

  for (auto &dynamic_header : m_dynamic_headers) {
    dynamic_header.dump(output);
  }

  for (auto &stage_header : m_stage_metadata) {
    stage_header->dump(output);
  }
}

Gcs_xcom_synode const &Gcs_packet::get_delivery_synode() const {
  return m_delivery_synode;
}

Gcs_xcom_synode const &Gcs_packet::get_origin_synode() const {
  return m_origin_synode;
}
