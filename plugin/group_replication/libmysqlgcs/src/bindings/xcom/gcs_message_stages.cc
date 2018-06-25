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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>
#include <tuple>
#include <type_traits>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"

const unsigned short Gcs_message_stage::WIRE_HD_LEN_SIZE;
const unsigned short Gcs_message_stage::WIRE_HD_TYPE_SIZE;
const unsigned short Gcs_message_stage::WIRE_HD_PAYLOAD_LEN_SIZE;
const unsigned short Gcs_message_stage::WIRE_HD_LEN_OFFSET;
const unsigned short Gcs_message_stage::WIRE_HD_TYPE_OFFSET;
const unsigned short Gcs_message_stage::WIRE_HD_PAYLOAD_LEN_OFFSET;

/*
  There will be a compile warning on os << type_code if no explicit type cast.
  the function is added to eliminate the warnings.
 */
template <class OSTREAM>
static inline OSTREAM &operator<<(OSTREAM &os,
                                  Gcs_message_stage::stage_code type_code) {
  return os << static_cast<unsigned int>(type_code);
}

void Gcs_message_stage::swap_buffer(Gcs_packet &packet,
                                    unsigned char *new_buffer,
                                    unsigned long long new_capacity,
                                    unsigned long long new_packet_length,
                                    int dyn_header_length) {
  unsigned char *old_buffer = nullptr;
  Gcs_internal_message_header hd;

  /*
   Replace the old buffer with the new buffer which has the transformed payload.
   */
  old_buffer = packet.swap_buffer(new_buffer, new_capacity);

  /*
   Load a header object based on the information stored in the old buffer and
   update those which have definitely changed. In particular, the new packet
   length and the "dynamic headers length".

   The "dynamic headers length" is used to distinguish whether there are
   additional stages to process or not. The value of dyn_header_length is
   positive when a new stage is being applied, and negative when the effects of
   a stage is being reverted.
   */
  hd.decode(old_buffer);
  hd.set_total_length(new_packet_length);
  hd.set_dynamic_headers_length(hd.get_dynamic_headers_length() +
                                dyn_header_length);

  /*
   Update (i.e. store) the fixed header information in the new buffer.
   */
  hd.encode(packet.get_buffer());

  /*
   Reload the header details into the packet
   */
  packet.reload_header(hd);
  assert(packet.get_version() == hd.get_version() && packet.get_version() != 0);

  /*
   Delete the old buffer which is not useful anymore.
   */
  free(old_buffer);
}

void Gcs_message_stage::encode(unsigned char *header,
                               unsigned short header_length,
                               unsigned long long old_payload_length) {
  unsigned int stage_code_enc = static_cast<unsigned int>(get_stage_code());

  unsigned short hd_len_enc = htole16(header_length);
  memcpy(header + WIRE_HD_LEN_OFFSET, &hd_len_enc, WIRE_HD_LEN_SIZE);
  static_assert(sizeof(decltype(header_length)) == WIRE_HD_LEN_SIZE,
                "The header_length size does not match storage capacity");
  static_assert(sizeof(decltype(header_length)) == sizeof(decltype(hd_len_enc)),
                "The header_length size is not equal to hd_len_enc size");

  stage_code_enc = htole32(stage_code_enc);
  memcpy(header + WIRE_HD_TYPE_OFFSET, &stage_code_enc, WIRE_HD_TYPE_SIZE);
  static_assert(sizeof(decltype(stage_code_enc)) == WIRE_HD_TYPE_SIZE,
                "The stage_code_enc size does not match the storage capacity");

  unsigned long long old_payload_length_enc = htole64(old_payload_length);
  memcpy(header + WIRE_HD_PAYLOAD_LEN_OFFSET, &old_payload_length_enc,
         WIRE_HD_PAYLOAD_LEN_SIZE);
  static_assert(
      sizeof(decltype(old_payload_length)) == WIRE_HD_PAYLOAD_LEN_SIZE,
      "The old_payload_length size does not match the storage capacity");
  static_assert(sizeof(decltype(old_payload_length)) ==
                    sizeof(decltype(old_payload_length_enc)),
                "The old_payload_length size is not equal to "
                "old_payload_length_enc size");
}

void Gcs_message_stage::decode(const unsigned char *hd,
                               unsigned short *header_length,
                               unsigned long long *old_payload_length) {
  const unsigned char *slider = hd;
  unsigned int type_code_enc;
  Gcs_message_stage::stage_code stage_code MY_ATTRIBUTE((unused));

  memcpy(header_length, slider, WIRE_HD_LEN_SIZE);
  *header_length = le16toh(*header_length);
  slider += WIRE_HD_LEN_SIZE;
  static_assert(sizeof(decltype(*header_length)) == WIRE_HD_LEN_SIZE,
                "The header_length size does not match the storage capacity");

  memcpy(&type_code_enc, slider, WIRE_HD_TYPE_SIZE);
  type_code_enc = le32toh(type_code_enc);
  stage_code = static_cast<Gcs_message_stage::stage_code>(type_code_enc);
  DBUG_ASSERT(stage_code == get_stage_code());
  slider += WIRE_HD_TYPE_SIZE;
  static_assert(sizeof(decltype(type_code_enc)) == WIRE_HD_TYPE_SIZE,
                "The type_code_enc size does not match the storage capacity");

  memcpy(old_payload_length, slider, WIRE_HD_PAYLOAD_LEN_SIZE);
  *old_payload_length = le64toh(*old_payload_length);
  static_assert(
      sizeof(decltype(*old_payload_length)) == WIRE_HD_PAYLOAD_LEN_SIZE,
      "The old_payload_length size does not match the storage capacity");
}

unsigned short Gcs_message_stage::calculate_dyn_header_length() const {
  return static_cast<unsigned short>(WIRE_HD_LEN_SIZE + WIRE_HD_TYPE_SIZE +
                                     WIRE_HD_PAYLOAD_LEN_SIZE);
}

bool Gcs_message_stage::apply(Gcs_packet &packet) {
  /*
   Check whether the stage is enabled or not.
   */
  if (!is_enabled()) {
    return false;
  }

  /*
   Check if the stage execution should be skipped or not due to any
   other reason which is specific of the current stage
   */
  auto result_skip = skip_apply(packet);
  if (result_skip != stage_status::apply) {
    /*
     Whether there was an issue or not and report it to the
     caller.
     */
    return result_skip == stage_status::abort;
  }

  /*
   Allocate memory to store the dymanic header and the transformed payload.
   */
  unsigned short dyn_header_length = calculate_dyn_header_length();
  unsigned long long fixed_header_length = packet.get_header_length();
  unsigned long long new_payload_length = calculate_payload_length(packet);
  unsigned long long new_packet_length =
      fixed_header_length + dyn_header_length + new_payload_length;
  assert(dyn_header_length >= (WIRE_HD_LEN_SIZE + WIRE_HD_TYPE_SIZE));
  unsigned long long new_capacity =
      Gcs_packet::calculate_capacity(new_packet_length);
  unsigned char *new_buffer = Gcs_packet::create_buffer(new_capacity);
  if (new_buffer == nullptr) {
    MYSQL_GCS_LOG_ERROR("Cannot allocate memory to store payload of size "
                        << new_capacity << ".");
    return true;
  }

  /*
   Transform the payload and store the result transformation into the new buffer
   and return the new payload length.

   Basicaly, it applies some transformation to the old payload and stores the
   result into the new buffer. For example, it may compress the old payload and
   store the compacted result in the new bufer. The new payload length is an
   attempt to estimate the space consumed by the transformed result but the
   actual length will be returned by the function call.
   */
  unsigned char *new_payload_ptr =
      new_buffer + fixed_header_length + dyn_header_length;
  unsigned char *old_payload_ptr = packet.get_payload();
  unsigned long long old_payload_length = packet.get_payload_length();

  bool error = false;
  std::tie(error, new_payload_length) = transform_payload_apply(
      packet.get_version(), new_payload_ptr, new_payload_length,
      old_payload_ptr, old_payload_length);
  if (error) {
    free(new_buffer);
    return true;
  }

  new_packet_length =
      fixed_header_length + dyn_header_length + new_payload_length;

  /*
   Encode the new dynamic header into the new buffer.
   */
  unsigned char *dyn_header_ptr = new_buffer + fixed_header_length;
  encode(dyn_header_ptr, dyn_header_length, old_payload_length);

  /*
   Replace the old buffer with the new buffer, which has the transformed
   content, and free the old buffer.
   */
  swap_buffer(packet, new_buffer, new_capacity, new_packet_length,
              dyn_header_length);

  return false;
}

bool Gcs_message_stage::revert(Gcs_packet &packet) {
  /*
   Note that the execution will usually not get here because if the dynamic
   headers length is zero, it is not possible to find out that there is
   information on a dynamic stage to be consumed.

   However, this is here as a protection to avoid problems with test cases that
   may try to call this method directly.
   */
  if (packet.get_dyn_headers_length() <= 0) {
    return true;
  }

  /*
   Check if the stage execution should be skipped or not.
   */
  auto result_skip = skip_revert(packet);
  if (result_skip != stage_status::apply) {
    /*
     Whether there was an issue or not and report it to the
     caller.
     */
    return result_skip == stage_status::abort;
  }

  /*
   Decode the dynamic header associated with the stage.

   Note that the decoded header must match the calculated value for the current
   stage. If they don't match, this represents a malformed message.
   */
  unsigned short dyn_header_length = 0;
  unsigned long long new_payload_length = 0;
  decode(packet.get_payload(), &dyn_header_length, &new_payload_length);
  if (dyn_header_length != calculate_dyn_header_length()) {
    MYSQL_GCS_LOG_ERROR("Dynamic header does not have the expected size: found "
                        << dyn_header_length << ", expected "
                        << calculate_dyn_header_length() << ".");
    return true;
  }
  assert(dyn_header_length >= (WIRE_HD_LEN_SIZE + WIRE_HD_TYPE_SIZE));

  /*
   Allocate memory to store the retrieved and transformed payloads associated to
   previous stages.
   */
  unsigned long long fixed_header_size = packet.get_header_length();
  unsigned long long new_capacity =
      Gcs_packet::calculate_capacity(new_payload_length + fixed_header_size);
  unsigned char *new_buffer = Gcs_packet::create_buffer(new_capacity);
  if (new_buffer == nullptr) {
    MYSQL_GCS_LOG_ERROR("Cannot allocate memory to store payload of size "
                        << new_capacity << ".");
    return true;
  }

  /*
   Revert any transformation applied to the payload and store the result into
   the new buffer and return the new payload length.

   For example, it may uncompress a payload which has been previously compressed
   and returns the uncompressed payload length.
   */
  unsigned char *old_payload_ptr = packet.get_payload() + dyn_header_length;
  unsigned long long old_payload_length =
      packet.get_payload_length() - dyn_header_length;
  unsigned char *new_payload_ptr = new_buffer + fixed_header_size;

  bool error = false;
  std::tie(error, new_payload_length) = transform_payload_revert(
      packet.get_version(), new_payload_ptr, new_payload_length,
      old_payload_ptr, old_payload_length);
  if (error) {
    free(new_buffer);
    return true;
  }

  unsigned long long new_packet_length = fixed_header_size + new_payload_length;

  /*
   Replace the old buffer with new buffer, which has the transformed content,
   and free the old buffer.
   */
  swap_buffer(packet, new_buffer, new_capacity, new_packet_length,
              -dyn_header_length);

  return false;
}

const unsigned int Gcs_message_pipeline::MINIMUM_PROTOCOL_VERSION;

const unsigned int Gcs_message_pipeline::DEFAULT_PROTOCOL_VERSION;

bool Gcs_message_pipeline::outgoing(Gcs_internal_message_header &hd,
                                    Gcs_packet &p) const {
  pipeline_version_number current_version =
      m_pipeline_version.load(std::memory_order_relaxed);
  pipeline_version_number pipeline_version = current_version;

  /*
   The pipeline associated with the minimum protocol version is forced when a
   stage exchange message is sent because servers using any protocol version
   must be able to process the state exchange messages. Using the stages
   associated with this protocol version guarantees that any server will be able
   to read these messages and will then be able to compute the greatest common
   protocol version in use in the group.

   Note that the fixed header still carries the protocol version in use in the
   server.
   */
  if (hd.get_cargo_type() ==
      Gcs_internal_message_header::cargo_type::CT_INTERNAL_STATE_EXCHANGE) {
    pipeline_version = Gcs_message_pipeline::MINIMUM_PROTOCOL_VERSION;
  }

  const Gcs_outgoing_stages *pipeline = retrieve_pipeline(pipeline_version);
  assert(pipeline != nullptr);

  /*
   Fix the header information with the correct protocol version as this is
   the only place where such information can be atomically and safely set up.
  */
  hd.set_version(current_version);
  hd.encode(p.get_buffer());
  p.reload_header(hd);

  bool error = false;
  for (auto &stage_type_code : *pipeline) {
    Gcs_message_stage *stage = retrieve_stage(stage_type_code);
    assert(stage != nullptr);
    if (stage->is_enabled() && (error = stage->apply(p))) return error;
  }

  return error;
}

bool Gcs_message_pipeline::incoming(Gcs_packet &p) const {
  bool error = false;
  Gcs_message_stage *stage = nullptr;

  while (p.get_dyn_headers_length() > 0 && !error) {
    /*
     The execution will exit the loop when a stage is not found meaning that
     either the node is old and does not have support to a message stage, the
     packet was tampered/corrupted or there was any other error processing the
     packet.
     */
    if ((stage = retrieve_stage(p)) != nullptr)
      error = stage->revert(p);
    else {
      MYSQL_GCS_LOG_ERROR("Unable to deliver incoming message. "
                          << "Request for an unknown/invalid message handler.");
      error = true;
    }
  }
  return error;
}

Gcs_message_stage *Gcs_message_pipeline::retrieve_stage(
    const Gcs_packet &p) const {
  unsigned int stage_code_enc = 0;
  static_assert(
      sizeof(decltype(stage_code_enc)) == Gcs_message_stage::WIRE_HD_TYPE_SIZE,
      "The stage_code_enc size does not match the storage capacity");

  memcpy(&stage_code_enc,
         p.get_payload() + Gcs_message_stage::WIRE_HD_TYPE_OFFSET,
         Gcs_message_stage::WIRE_HD_TYPE_SIZE);
  stage_code_enc = le32toh(stage_code_enc);

  return retrieve_stage(
      static_cast<Gcs_message_stage::stage_code>(stage_code_enc));
}

Gcs_message_stage *Gcs_message_pipeline::retrieve_stage(
    Gcs_message_stage::stage_code stage_code) const {
  const auto &it = m_handlers.find(stage_code);
  if (it != m_handlers.end()) return (*it).second.get();
  return nullptr;
}

const Gcs_outgoing_stages *Gcs_message_pipeline::retrieve_pipeline(
    pipeline_version_number pipeline_version) const {
  const auto &it = m_pipelines.find(pipeline_version);
  if (it != m_pipelines.end()) return &(*it).second;
  return nullptr;
}

bool Gcs_message_pipeline::register_pipeline(
    std::initializer_list<Gcs_pair_version_stages> stages) {
  /*
   Store the identifier of all handlers already registered.
   */
  std::set<Gcs_message_stage::stage_code> registered_handlers;
  /*
   Store the identifier of all handlers assigned to a pipeline
   stage.
   */
  std::set<Gcs_message_stage::stage_code> pipeline_handlers;
  /*
   Total number of pipeline stages for all versions.
   */
  size_t total_stages = 0;

  /*
   The clean up method should be called if the pipeline needs to be configured.
   */
  assert(m_pipelines.size() == 0);

  /*
   Check if there is actually a handler for the provided stages, meaning that
   handlers which are uniquely identified will form a set that have the same
   size of the list provided as parameter.
   */
  for (const auto &handler : m_handlers) {
    assert(handler.second->get_stage_code() == handler.first);
    registered_handlers.insert(handler.second->get_stage_code());
  }

  /*
   Check if all the stages in the different pipelines have an appropriate
   handler and all have unique identifiers (.i.e. type code).
   */
  for (const auto &version_stages : stages) {
    pipeline_handlers.insert(version_stages.second.begin(),
                             version_stages.second.end());
    total_stages += version_stages.second.size();
  }

  if (registered_handlers != pipeline_handlers) {
    MYSQL_GCS_LOG_ERROR(
        "Configuration error in pipeline. The set of handlers doesn't match "
        "the handlers required by all the stages in the different versions.");
    return true;
  }

  if (total_stages != registered_handlers.size()) {
    MYSQL_GCS_LOG_ERROR(
        "Any stage in any pipeline must have a unique indentifier "
        "associated to it.");
    return true;
  }

  m_pipelines.insert(stages);

  return false;
}

bool Gcs_message_pipeline::set_version(
    pipeline_version_number pipeline_version) {
  bool const exists = (m_pipelines.find(pipeline_version) != m_pipelines.end());
  if (exists) {
    m_pipeline_version.store(pipeline_version, std::memory_order_relaxed);
  }
  return !exists;
}

pipeline_version_number Gcs_message_pipeline::get_version() const {
  return m_pipeline_version.load(std::memory_order_relaxed);
}

void Gcs_message_pipeline::cleanup() {
  m_handlers.clear();
  m_pipelines.clear();
}
