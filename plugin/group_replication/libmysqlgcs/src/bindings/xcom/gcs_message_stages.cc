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

/*
  There will be a compile warning on os << type_code if no explicit type cast.
  the function is added to eliminate the warnings.
 */
template <class OSTREAM>
static inline OSTREAM &operator<<(OSTREAM &os, Stage_code type_code) {
  return os << static_cast<unsigned int>(type_code);
}

std::pair<bool, std::vector<Gcs_packet>> Gcs_message_stage::apply(
    Gcs_packet &&packet) {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, std::vector<Gcs_packet>());

  /* Save the packet payload size before this stage is applied. */
  auto &dynamic_header = packet.get_current_dynamic_header();
  assert(dynamic_header.get_stage_code() == get_stage_code());
  dynamic_header.set_payload_length(packet.get_payload_length());

  /* Transform the packet payload according to the specific stage logic. */
  bool failure;
  std::vector<Gcs_packet> packets_out;
  std::tie(failure, packets_out) = apply_transformation(std::move(packet));
  if (failure) goto end;

  /* Prepare the packets for the next stage. */
  for (auto &packet_out : packets_out) {
    packet_out.prepare_for_next_outgoing_stage();
  }

  result = std::make_pair(OK, std::move(packets_out));

end:
  return result;
}

std::pair<Gcs_pipeline_incoming_result, Gcs_packet> Gcs_message_stage::revert(
    Gcs_packet &&packet) {
  assert(packet.get_current_dynamic_header().get_stage_code() ==
         get_stage_code());
  auto result =
      std::make_pair(Gcs_pipeline_incoming_result::ERROR, Gcs_packet());
  Gcs_pipeline_incoming_result error_code;
  Gcs_packet packet_out;

  auto const skip_status = skip_revert(packet);
  switch (skip_status) {
    case Gcs_message_stage::stage_status::abort:
      goto end;
    case Gcs_message_stage::stage_status::apply:
      break;
    case Gcs_message_stage::stage_status::skip:
      goto skip_stage;
  }

  /* Transform the packet payload according to the specific stage logic. */
  std::tie(error_code, packet_out) = revert_transformation(std::move(packet));
  switch (error_code) {
    case Gcs_pipeline_incoming_result::OK_PACKET:
      break;
    case Gcs_pipeline_incoming_result::OK_NO_PACKET:
      result = std::make_pair(Gcs_pipeline_incoming_result::OK_NO_PACKET,
                              Gcs_packet());
      goto end;
    case Gcs_pipeline_incoming_result::ERROR:
      goto end;
  }

skip_stage:
  packet_out.prepare_for_next_incoming_stage();

  result = std::make_pair(Gcs_pipeline_incoming_result::OK_PACKET,
                          std::move(packet_out));
end:
  return result;
}

std::pair<bool, std::vector<Gcs_packet>> Gcs_message_pipeline::process_outgoing(
    Gcs_message_data const &payload, Cargo_type cargo) const {
  bool constexpr ERROR = true;
  auto result = std::make_pair(ERROR, std::vector<Gcs_packet>());
  auto const original_payload_size = payload.get_encode_size();
  Gcs_packet packet;
  uint64_t buffer_size = 0;

  Gcs_protocol_version current_version =
      m_pipeline_version.load(std::memory_order_relaxed);
  Gcs_protocol_version pipeline_version = current_version;
  /*
   The pipeline associated with the minimum protocol version is forced when a
   stage exchange message is sent because servers using any protocol version
   must be able to process the state exchange messages. Using the stages
   associated with this protocol version guarantees that any server will be able
   to read these messages and will then be able to compute the greatest common
   protocol version in use in the group.

   This is also necessary because the communication system is built on the
   assumption that state exchange messages don't go through all the stages
   in the pipeline, only into the compression stage if it is necessary.

   Note that the fixed header still carries the protocol version in use in the
   server.
   */
  if (cargo == Cargo_type::CT_INTERNAL_STATE_EXCHANGE) {
    pipeline_version = Gcs_protocol_version::V1;
  }

  /*
   Identify which stages will be applied.

   Previously we decided whether stage S+1 would be applied after applying
   stage S. This meant the decision of whether to apply S+1 or not took into
   account the eventual transformations S did.
   For instance, consider S is compression and S+1 is fragmentation. If S
   reduced the size of the payload to a value that is below S+1's threshold,
   S+1 would not be applied.

   We now decide a priori which stages will be applied. We do this because,
   since we know all the stages that will be applied, it allows us to allocate
   a buffer for the serialized packet that is able to hold the entire
   serialized packet.
   Previously, we copied the payload to a new buffer between every stage. Now,
   we only need to copy the payload to a new buffer in stages that actually
   transform the payload. Stages that simply add some metadata do not need to
   perform any copying or allocation.

   Some examples:

   LZ4
   Must allocate a new buffer because it compresses the payload.

   Fragmentation
   Must allocate N-1 new buffers if it produces N fragments.

   UUID
   Does not need to allocate nor copy anything because it only adds metadata.
   */
  bool failure;
  std::vector<Stage_code> stages_to_apply;
  std::tie(failure, stages_to_apply) =
      get_stages_to_apply(pipeline_version, original_payload_size);
  if (failure) goto end;

  /*
   Prepare the packet.

   Now that we have identified all the stages that the message will go
   through, create their dynamic and stage headers so we can add them to the
   packet. These, together with the application payload size, gives us the
   capacity required by the packet's serialization buffer.
   */
  std::tie(failure, packet) = create_packet(
      cargo, current_version, original_payload_size, stages_to_apply);
  if (failure) goto end;

  /* Copy the payload into the packet. */
  buffer_size = packet.get_payload_length();
  failure = payload.encode(packet.get_payload_pointer(), &buffer_size);
  if (failure) {
    /* purecov: begin inspected */
    MYSQL_GCS_LOG_ERROR("Error inserting the payload in the binding message.")
    goto end;
    /* purecov: end */
  }
  assert(original_payload_size == buffer_size);

  /* The packet is ready, send it through the pipeline. */
  result = apply_stages(std::move(packet), stages_to_apply);

end:
  return result;
}

std::pair<bool, std::vector<Stage_code>>
Gcs_message_pipeline::get_stages_to_apply(
    Gcs_protocol_version const &pipeline_version,
    uint64_t const &original_payload_size) const {
  assert(retrieve_pipeline(pipeline_version) != nullptr);
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, std::vector<Stage_code>());

  const Gcs_stages_list &all_stages = *retrieve_pipeline(pipeline_version);
  std::vector<Stage_code> stages_to_apply;
  stages_to_apply.reserve(all_stages.size());

  for (auto const &stage_code : all_stages) {
    assert(retrieve_stage(stage_code) != nullptr);
    Gcs_message_stage const &stage = *retrieve_stage(stage_code);

    if (stage.is_enabled()) {
      auto const error_code = stage.skip_apply(original_payload_size);
      switch (error_code) {
        case Gcs_message_stage::stage_status::abort:
          goto end;
        case Gcs_message_stage::stage_status::apply:
          stages_to_apply.push_back(stage_code);
          break;
        case Gcs_message_stage::stage_status::skip:
          break;
      }
    }
  }

  result = std::make_pair(OK, std::move(stages_to_apply));

end:
  return result;
}

std::pair<bool, Gcs_packet> Gcs_message_pipeline::create_packet(
    Cargo_type const &cargo, Gcs_protocol_version const &current_version,
    uint64_t const &original_payload_size,
    std::vector<Stage_code> const &stages_to_apply) const {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, Gcs_packet());
  std::vector<Gcs_dynamic_header> dynamic_headers;
  std::vector<std::unique_ptr<Gcs_stage_metadata>> stage_headers;

  auto const nr_stages = stages_to_apply.size();
  dynamic_headers.reserve(nr_stages);
  stage_headers.reserve(nr_stages);

  for (auto const &stage_code : stages_to_apply) {
    Gcs_message_stage &stage = *retrieve_stage(stage_code);
    dynamic_headers.push_back(Gcs_dynamic_header(stage_code, 0));
    stage_headers.push_back(stage.get_stage_header());
  }

  bool packet_ok;
  Gcs_packet packet;
  std::tie(packet_ok, packet) = Gcs_packet::make_outgoing_packet(
      cargo, current_version, std::move(dynamic_headers),
      std::move(stage_headers), original_payload_size);
  if (!packet_ok) {
    /* purecov: begin inspected */
    MYSQL_GCS_LOG_ERROR("Could not allocate memory to create packet.")
    goto end;
    /* purecov: end */
  }

  result = std::make_pair(OK, std::move(packet));

end:
  return result;
}

std::pair<bool, std::vector<Gcs_packet>> Gcs_message_pipeline::apply_stages(
    Gcs_packet &&packet, std::vector<Stage_code> const &stages) const {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, std::vector<Gcs_packet>());
  std::vector<Gcs_packet> packets_out;

  packets_out.push_back(std::move(packet));

  for (auto const &stage_code : stages) {
    assert(retrieve_stage(stage_code) != nullptr);
    Gcs_message_stage &stage = *retrieve_stage(stage_code);

    bool failure;
    std::tie(failure, packets_out) = apply_stage(std::move(packets_out), stage);
    if (failure) goto end;
  }

  result = std::make_pair(OK, std::move(packets_out));

end:
  return result;
}

std::pair<bool, std::vector<Gcs_packet>> Gcs_message_pipeline::apply_stage(
    std::vector<Gcs_packet> &&packets, Gcs_message_stage &stage) const {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, std::vector<Gcs_packet>());
  std::vector<Gcs_packet> packets_out;

  for (auto &out : packets) {
    bool failure;
    std::vector<Gcs_packet> packets_after_apply;
    std::tie(failure, packets_after_apply) = stage.apply(std::move(out));
    if (failure) {
      goto end;
    } else {
      for (auto &packet_after_apply : packets_after_apply) {
        packets_out.push_back(std::move(packet_after_apply));
      }
    }
  }

  result = std::make_pair(OK, std::move(packets_out));

end:
  return result;
}

std::pair<Gcs_pipeline_incoming_result, Gcs_packet>
Gcs_message_pipeline::process_incoming(Gcs_packet &&packet) const {
  auto result =
      std::make_pair(Gcs_pipeline_incoming_result::ERROR, Gcs_packet());

  /* Revert the stages from last to first. */
  auto const dynamic_headers = packet.get_dynamic_headers();
  for (auto it = dynamic_headers.rbegin(); it != dynamic_headers.rend(); it++) {
    Gcs_dynamic_header const &dynamic_header = *it;
    Gcs_pipeline_incoming_result error_code;

    std::tie(error_code, packet) =
        revert_stage(std::move(packet), dynamic_header.get_stage_code());
    switch (error_code) {
      case Gcs_pipeline_incoming_result::OK_PACKET:
        break;
      case Gcs_pipeline_incoming_result::OK_NO_PACKET:
        result = std::make_pair(Gcs_pipeline_incoming_result::OK_NO_PACKET,
                                Gcs_packet());
        goto end;
      case Gcs_pipeline_incoming_result::ERROR:
        goto end;
    }
  }

  result = std::make_pair(Gcs_pipeline_incoming_result::OK_PACKET,
                          std::move(packet));

end:
  return result;
}

std::pair<Gcs_pipeline_incoming_result, Gcs_packet>
Gcs_message_pipeline::revert_stage(Gcs_packet &&packet,
                                   Stage_code const &stage_code) const {
  assert(stage_code == packet.get_current_dynamic_header().get_stage_code());
  auto result =
      std::make_pair(Gcs_pipeline_incoming_result::ERROR, Gcs_packet());

  Gcs_message_stage *const stage = retrieve_stage(stage_code);
  bool const unknown_stage_code = (stage == nullptr);
  if (unknown_stage_code) {
    MYSQL_GCS_LOG_ERROR("Unable to deliver incoming message. "
                        << "Request for an unknown/invalid message handler.");
    goto end;
  }

  result = stage->revert(std::move(packet));

end:
  return result;
}

void Gcs_message_pipeline::update_members_information(
    const Gcs_member_identifier &me, const Gcs_xcom_nodes &xcom_nodes) const {
  for (auto &stage : m_handlers) {
    stage.second->update_members_information(me, xcom_nodes);
  }
}

Gcs_xcom_synode_set Gcs_message_pipeline::get_snapshot() const {
  Gcs_xcom_synode_set synods;

  for (auto &stage : m_handlers) {
    Gcs_xcom_synode_set synods_per_stage = stage.second->get_snapshot();
    synods.insert(synods_per_stage.begin(), synods_per_stage.end());
  }

  return synods;
}

Gcs_message_stage *Gcs_message_pipeline::retrieve_stage(
    Stage_code stage_code) const {
  const auto &it = m_handlers.find(stage_code);
  if (it != m_handlers.end()) return (*it).second.get();
  return nullptr;
}

const Gcs_stages_list *Gcs_message_pipeline::retrieve_pipeline(
    Gcs_protocol_version pipeline_version) const {
  const auto &it = m_pipelines.find(pipeline_version);
  if (it != m_pipelines.end()) return &(*it).second;
  return nullptr;
}

bool Gcs_message_pipeline::register_pipeline(
    std::initializer_list<Gcs_pair_version_stages> stages) {
  /*
   The clean up method should be called if the pipeline needs to be
   reconfigured.
   */
  assert(m_pipelines.size() == 0);

  /*
   Store the identifier of all handlers already registered.
   */
  std::set<Stage_code> registered_handlers;
  /*
   Store the identifier of all handlers assigned to a pipeline
   stage.
   */
  std::set<Stage_code> pipeline_handlers;
  /*
   Total number of pipeline stages for all versions.
   */
  size_t total_stages = 0;

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

bool Gcs_message_pipeline::set_version(Gcs_protocol_version pipeline_version) {
  bool const exists = (m_pipelines.find(pipeline_version) != m_pipelines.end());
  if (exists) {
    m_pipeline_version.store(pipeline_version, std::memory_order_relaxed);
  }
  return !exists;
}

Gcs_protocol_version Gcs_message_pipeline::get_version() const {
  return m_pipeline_version.load(std::memory_order_relaxed);
}

void Gcs_message_pipeline::cleanup() {
  m_handlers.clear();
  m_pipelines.clear();
}
