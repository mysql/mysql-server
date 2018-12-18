/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_split.h"

#include <algorithm>  // std::min
#include <cassert>
#include <cstring>  // std::memcpy
#include <limits>
#include <unordered_set>
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xxhash.h"

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"

const unsigned short Gcs_split_header_v2::WIRE_HD_NUM_MESSAGES_SIZE;
const unsigned short Gcs_split_header_v2::WIRE_HD_SENDER_ID_SIZE;
const unsigned short Gcs_split_header_v2::WIRE_HD_MESSAGE_ID_SIZE;
const unsigned short Gcs_split_header_v2::WIRE_HD_MESSAGE_PART_ID_SIZE;
const unsigned short Gcs_split_header_v2::WIRE_HD_PAYLOAD_SIZE;

unsigned long long Gcs_split_header_v2::encode(unsigned char *buffer) const {
  unsigned char *slider = buffer;

  uint64_t le_sender_id = htole64(m_sender_id);
  memcpy(slider, &le_sender_id, WIRE_HD_SENDER_ID_SIZE);
  slider += WIRE_HD_SENDER_ID_SIZE;
  static_assert(sizeof(decltype(m_sender_id)) == sizeof(decltype(le_sender_id)),
                "The m_sender id is not equal to le_sender_id");

  unsigned int le_num_messages = htole32(m_num_messages);
  memcpy(slider, &le_num_messages, WIRE_HD_NUM_MESSAGES_SIZE);
  slider += WIRE_HD_NUM_MESSAGES_SIZE;
  static_assert(
      sizeof(decltype(m_num_messages)) == sizeof(decltype(le_num_messages)),
      "The num_messages size is not equal to le_num_messages size");

  Gcs_message_id le_message_id = htole64(m_message_id);
  memcpy(slider, &le_message_id, WIRE_HD_MESSAGE_ID_SIZE);
  slider += WIRE_HD_MESSAGE_ID_SIZE;
  static_assert(
      sizeof(decltype(m_message_id)) == sizeof(decltype(le_message_id)),
      "The m_message_id size is not equal to le_message_id size");

  unsigned int le_message_part_id = htole32(m_message_part_id);
  memcpy(slider, &le_message_part_id, WIRE_HD_MESSAGE_PART_ID_SIZE);
  slider += WIRE_HD_MESSAGE_PART_ID_SIZE;
  static_assert(
      sizeof(decltype(m_message_part_id)) ==
          sizeof(decltype(le_message_part_id)),
      "The m_message_part_id size is not equal to le_message_part_id size");

  unsigned long long le_payload_len = htole64(m_payload_length);
  memcpy(slider, &le_payload_len, WIRE_HD_PAYLOAD_SIZE);
  slider += WIRE_HD_PAYLOAD_SIZE;
  static_assert(
      sizeof(decltype(m_payload_length)) == sizeof(decltype(le_payload_len)),
      "The m_payload_length size is not equal to le_payload_len size");

  return slider - buffer;
}

unsigned long long Gcs_split_header_v2::decode(const unsigned char *buffer) {
  const unsigned char *slider = buffer;

  memcpy(&m_sender_id, slider, WIRE_HD_SENDER_ID_SIZE);
  m_sender_id = le64toh(m_sender_id);
  slider += WIRE_HD_SENDER_ID_SIZE;

  memcpy(&m_num_messages, slider, WIRE_HD_NUM_MESSAGES_SIZE);
  m_num_messages = le32toh(m_num_messages);
  slider += WIRE_HD_NUM_MESSAGES_SIZE;

  memcpy(&m_message_id, slider, WIRE_HD_MESSAGE_ID_SIZE);
  m_message_id = le64toh(m_message_id);
  slider += WIRE_HD_MESSAGE_ID_SIZE;

  memcpy(&m_message_part_id, slider, WIRE_HD_MESSAGE_PART_ID_SIZE);
  m_message_part_id = le32toh(m_message_part_id);
  slider += WIRE_HD_MESSAGE_PART_ID_SIZE;

  memcpy(&m_payload_length, slider, WIRE_HD_PAYLOAD_SIZE);
  m_payload_length = le64toh(m_payload_length);
  slider += WIRE_HD_PAYLOAD_SIZE;

  return slider - buffer;
}

void Gcs_split_header_v2::dump(std::ostringstream &output) const {
  output << "split header=<sender id=(" << m_sender_id << "), number messages=("
         << m_num_messages << "), message id=(" << m_message_id
         << "), message part=(" << m_message_part_id << "), payload length=("
         << m_payload_length << "), header length=("
         << calculate_encode_length() << ")>";
}

/**
 Calculate the sender identification which currently is a hash over
 the member identifier and its incarnation identifier (i.e. member's
 uuid).

 Note, however, that the current member's uuid is simply a timestamp
 and for that reason the hash is created as aforementioned.
 */
Gcs_sender_id calculate_sender_id(const Gcs_xcom_node_information &node) {
  std::string info(node.get_member_id().get_member_id());
  info.append(node.get_member_uuid().actual_value);

  return GCS_XXH64(info.c_str(), info.size(), 0);
}

bool Gcs_message_stage_split_v2::update_members_information(
    const Gcs_member_identifier &me, const Gcs_xcom_nodes &xcom_nodes) {
  /*
   Calculate the hash identifier associated with all members.
   */
  std::unordered_set<uint64_t> hash_set;
  for (const auto &node : xcom_nodes.get_nodes()) {
    Gcs_sender_id sender_id = calculate_sender_id(node);
    hash_set.insert(sender_id);
  }

  /*
   Update the sender uuid based on the newly informed member if it was not
   previously updated.
   */
  if (m_sender_id == 0) {
    const Gcs_xcom_node_information *const local_node = xcom_nodes.get_node(me);
    m_sender_id = calculate_sender_id(*local_node);
  }

  /*
   Remove mapping for a node that does not belong to the group anymore.
   */
  std::vector<Gcs_sender_id> expelled;
  for (const auto &map : m_packets_per_source) {
    if (hash_set.find(map.first) == hash_set.end()) {
      expelled.push_back(map.first);
    }
  }

  for (const auto &sender_id : expelled) {
    MYSQL_GCS_LOG_DEBUG(
        "Member %s is removing node %llu from the split pipeline mapping.",
        me.get_member_id().c_str(), static_cast<unsigned long long>(sender_id))
    remove_sender(sender_id);
  }

  /*
   Add nodes that are trying to join the group and were not seen before.
   */
  std::vector<Gcs_sender_id> joined;
  for (const auto &sender_id : hash_set) {
    MYSQL_GCS_LOG_DEBUG(
        "Member %s is adding node %llu into the split pipeline mapping.",
        me.get_member_id().c_str(), static_cast<unsigned long long>(sender_id))
    insert_sender(sender_id);
  }

  return false;
}

Gcs_message_stage::stage_status Gcs_message_stage_split_v2::skip_apply(
    uint64_t const &original_payload_size) const {
  auto result = stage_status::abort;
  unsigned long long nr_fragments = 0;
  bool would_create_too_many_fragments = true;

  bool const fragmentation_off = (m_split_threshold == 0);
  bool const packet_too_small = (original_payload_size < m_split_threshold);

  if (fragmentation_off || packet_too_small) {
    result = stage_status::skip;
    goto end;
  }

  nr_fragments =
      (original_payload_size + m_split_threshold - 1) / m_split_threshold;
  would_create_too_many_fragments =
      (nr_fragments >= std::numeric_limits<unsigned int>::max());

  if (would_create_too_many_fragments) {
    MYSQL_GCS_LOG_ERROR(
        "Maximum number of messages has been reached. Please, increase the "
        "maximum group communication message size value to decrease the number "
        "of messages.")
    result = stage_status::abort;
    goto end;
  }

  result = stage_status::apply;

end:
  return result;
}

std::unique_ptr<Gcs_stage_metadata>
Gcs_message_stage_split_v2::get_stage_header() {
  auto *header = new Gcs_split_header_v2();
  header->set_sender_id(m_sender_id);
  header->set_message_id(
      m_next_message_number.fetch_add(1, std::memory_order_relaxed));
  return std::unique_ptr<Gcs_stage_metadata>(header);
}

std::pair<bool, std::vector<Gcs_packet>>
Gcs_message_stage_split_v2::apply_transformation(Gcs_packet &&packet) {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, std::vector<Gcs_packet>());
  std::vector<Gcs_packet> packets_out;
  unsigned long long original_payload_length = packet.get_payload_length();

  /* Calculate number of fragments we will produce. */
  unsigned long long max_nr_fragments =
      (original_payload_length + m_split_threshold - 1) / m_split_threshold;
  DBUG_ASSERT(max_nr_fragments < std::numeric_limits<unsigned int>::max());
  auto nr_fragments = static_cast<unsigned int>(max_nr_fragments);
  DBUG_ASSERT(nr_fragments >= 1);

  if (nr_fragments == 1) {
    apply_transformation_single_fragment(packet);

    packets_out.push_back(std::move(packet));
    result = std::make_pair(OK, std::move(packets_out));
  } else {
    result = create_fragments(std::move(packet), nr_fragments);
  }

  return result;
}

void Gcs_message_stage_split_v2::apply_transformation_single_fragment(
    Gcs_packet &packet) const {
  DBUG_ASSERT(packet.get_payload_length() <= m_split_threshold);

  /*
   Populate the stage header.

   There is a single fragment, which is this one.
   */
  Gcs_split_header_v2 &stage_header =
      static_cast<Gcs_split_header_v2 &>(packet.get_current_stage_header());
  stage_header.set_message_part_id(0);
  stage_header.set_num_messages(1);
  stage_header.set_payload_length(packet.get_payload_length());

  // clang-format off
    MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(
      GCS_DEBUG_MSG_FLOW,
      std::ostringstream output;
      packet.dump(output);
      MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW, "Splitting output %s",
                                      output.str().c_str());
    )
  // clang-format on
}

std::pair<bool, std::vector<Gcs_packet>>
Gcs_message_stage_split_v2::create_fragments(
    Gcs_packet &&packet, unsigned int const &nr_fragments) const {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, std::vector<Gcs_packet>());
  unsigned long long last_fragment_payload_length = 0;
  unsigned long long original_payload_length = packet.get_payload_length();
  std::vector<Gcs_packet> packets_out;
  bool failure = true;
  Gcs_packet fragment;

  /*
   Process the first fragment.

   We reuse the whole packet as the first fragment by simply truncating its
   payload. We do this to avoid the extra allocation and copy.
   */
  Gcs_packet &first_fragment = packet;
  Gcs_split_header_v2 &first_fragment_header =
      static_cast<Gcs_split_header_v2 &>(
          first_fragment.get_current_stage_header());
  first_fragment_header.set_num_messages(nr_fragments);
  first_fragment_header.set_message_part_id(0);
  first_fragment_header.set_payload_length(m_split_threshold);

  // clang-format off
  /*
   Pointer to the original, unfragmented payload.

   We use this pointer as a slider to the original payload, so we can copy the
   appropriate portion of the payload to each fragment.
   It looks something like this:

   original_payload_pointer
   |
   |  original_payload_pointer   original_payload_pointer
   |          +                      +
   |  1 * m_split_threshold      (N-1) * sender_threshold
   |          |                      |
   v          v                      v
   +----------+--..................--+----------+
   |    #0    |                      |   #N-1   |
   +----------+--..................--+----------+
    \___  ___/                        \___  ___/
        \/                                \/
   m_split_threshold         last_fragment_payload_length
   */
  // clang-format on
  unsigned char const *original_payload_pointer = packet.get_payload_pointer();
  // Skip over the payload's part of the first fragment.
  original_payload_pointer += m_split_threshold;

  /* Create the fragments in the interval [1, nr_fragments-1[. */
  unsigned int fragment_nr = 1;
  for (; fragment_nr < (nr_fragments - 1);
       fragment_nr++, original_payload_pointer += m_split_threshold) {
    std::tie(failure, fragment) =
        create_fragment(fragment_nr, first_fragment, original_payload_pointer,
                        m_split_threshold);
    if (failure) goto end;

    packets_out.push_back(std::move(fragment));
  }

  /* Create the last fragment. */
  last_fragment_payload_length = original_payload_length % m_split_threshold;
  last_fragment_payload_length =
      (last_fragment_payload_length != 0 ? last_fragment_payload_length
                                         : m_split_threshold);
  std::tie(failure, fragment) =
      create_fragment(fragment_nr, first_fragment, original_payload_pointer,
                      last_fragment_payload_length);
  if (failure) goto end;
  packets_out.push_back(std::move(fragment));

  /* Truncate the first fragment, which contains the original payload. */
  first_fragment.set_payload_length(m_split_threshold);

  // clang-format off
  MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(
    GCS_DEBUG_MSG_FLOW,
    std::ostringstream output;
    first_fragment.dump(output);
    MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW, "Splitting output %s",
                                    output.str().c_str());
  )
  // clang-format on

  packets_out.push_back(std::move(first_fragment));

  result = std::make_pair(OK, std::move(packets_out));

end:
  return result;
}

std::pair<bool, Gcs_packet> Gcs_message_stage_split_v2::create_fragment(
    unsigned int const &fragment_part_id, Gcs_packet const &other_fragment,
    unsigned char const *const original_payload_pointer,
    unsigned long long const &fragment_size) const {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, Gcs_packet());
  unsigned char *fragment_payload_pointer = nullptr;
  Gcs_split_header_v2 *fragment_header = nullptr;

  /*
   Create the fragment.

   The fragment gets a copy of the headers from other_fragment.
   */
  bool packet_ok;
  Gcs_packet fragment;
  std::tie(packet_ok, fragment) =
      Gcs_packet::make_from_existing_packet(other_fragment, fragment_size);
  if (!packet_ok) goto end;

  /* Copy the payload part of this fragment to the packet. */
  fragment_payload_pointer = fragment.get_payload_pointer();
  std::memcpy(fragment_payload_pointer, original_payload_pointer,
              fragment_size);

  /*
   Fix the headers.

   Set the correct fragment part ID and payload size in the stage header, the
   payload size in the fixed header.
   */
  fragment_header =
      static_cast<Gcs_split_header_v2 *>(&fragment.get_current_stage_header());
  fragment_header->set_message_part_id(fragment_part_id);
  fragment_header->set_payload_length(fragment_size);
  fragment.set_payload_length(fragment_size);

  // clang-format off
  MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(
    GCS_DEBUG_MSG_FLOW,
    std::ostringstream output;
    fragment.dump(output);
    MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW, "Splitting output %s",
                                    output.str().c_str());
  )
  // clang-format on

  result = std::make_pair(OK, std::move(fragment));

end:
  return result;
}

std::pair<Gcs_pipeline_incoming_result, Gcs_packet>
Gcs_message_stage_split_v2::revert_transformation(Gcs_packet &&packet) {
  auto result =
      std::make_pair(Gcs_pipeline_incoming_result::ERROR, Gcs_packet());
  auto &header =
      static_cast<Gcs_split_header_v2 &>(packet.get_current_stage_header());

  // clang-format off
  MYSQL_GCS_DEBUG_EXECUTE_WITH_OPTION(
      GCS_DEBUG_MSG_FLOW,
      std::ostringstream output;
      header.dump(output);
      MYSQL_GCS_LOG_DEBUG_WITH_OPTION(GCS_DEBUG_MSG_FLOW, "Split input %s",
                                      output.str().c_str());
  )
  // clang-format on

  // Discard the packet if it comes from someone who is not in the current view.
  if (unknown_sender(header)) goto end;

  /*
   Process the packet.

   If it is the final fragment, we assemble the complete packet.
   If it is not, we add it to the table of fragments.
   */
  if (is_final_fragment(header)) {
    Gcs_packets_list fragments;
    // If there are other fragments, get them.
    if (header.get_num_messages() > 1) fragments = get_fragments(header);

    fragments.push_back(std::move(packet));

    bool failure;
    Gcs_packet whole_packet;
    std::tie(failure, whole_packet) = reassemble_fragments(fragments);
    if (!failure) {
      result = std::make_pair(Gcs_pipeline_incoming_result::OK_PACKET,
                              std::move(whole_packet));
    }
  } else {
    bool const failure = insert_fragment(std::move(packet));
    if (!failure) {
      result = std::make_pair(Gcs_pipeline_incoming_result::OK_NO_PACKET,
                              Gcs_packet());
    }
  }

end:
  return result;
}

bool Gcs_message_stage_split_v2::unknown_sender(
    Gcs_split_header_v2 const &fragment_header) const {
  auto packets_per_source_it =
      m_packets_per_source.find(fragment_header.get_sender_id());
  return (packets_per_source_it == m_packets_per_source.end());
}

bool Gcs_message_stage_split_v2::is_final_fragment(
    Gcs_split_header_v2 const &fragment_header) const {
  bool result = false;
  Gcs_packets_list const *fragment_list = nullptr;
  std::size_t nr_fragments_received = 0;

  auto packets_per_source_it =
      m_packets_per_source.find(fragment_header.get_sender_id());
  DBUG_ASSERT(packets_per_source_it != m_packets_per_source.end());

  Gcs_packets_per_content const &packets_per_content =
      packets_per_source_it->second;
  auto packets_per_content_it =
      packets_per_content.find(fragment_header.get_message_id());
  bool const is_first = (packets_per_content_it == packets_per_content.end());
  if (is_first) {
    // If this is the first and only fragment, it is the last as well.
    if (fragment_header.get_num_messages() == 1) {
      result = true;
    } else {
      goto end;
    }
  } else {
    fragment_list = &packets_per_content_it->second;
    nr_fragments_received = fragment_list->size();
    result = (nr_fragments_received == fragment_header.get_num_messages() - 1);
  }

end:
  return result;
}

Gcs_packets_list Gcs_message_stage_split_v2::get_fragments(
    Gcs_split_header_v2 const &fragment_header) {
  DBUG_ASSERT(fragment_header.get_num_messages() > 1);
  auto packets_per_source_it =
      m_packets_per_source.find(fragment_header.get_sender_id());

  Gcs_packets_per_content &packets_per_content = packets_per_source_it->second;
  auto packets_per_content_it =
      packets_per_content.find(fragment_header.get_message_id());

  Gcs_packets_list fragment_list = std::move(packets_per_content_it->second);

  packets_per_content.erase(packets_per_content_it);

  return fragment_list;
}

std::pair<bool, Gcs_packet> Gcs_message_stage_split_v2::reassemble_fragments(
    Gcs_packets_list &fragments) const {
  DBUG_ASSERT(fragments.size() > 0);
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, Gcs_packet());

  auto &some_fragment = fragments[0];

  /*
   Create a packet big enough to hold the reassembled payload.

   The size of the reassembled payload is stored in the dynamic header, i.e.
   the payload size before the stage was applied.
   */
  unsigned long long whole_payload_length =
      some_fragment.get_current_dynamic_header().get_payload_length();
  bool packet_ok;
  Gcs_packet whole_packet;
  std::tie(packet_ok, whole_packet) = Gcs_packet::make_from_existing_packet(
      some_fragment, whole_payload_length);
  if (!packet_ok) goto end;

  // clang-format off
  /*
   Reassemble the payload.

   Consider that there are N fragments. The size of fragments [0; N-1] is the
   fragmentation threshold of the sender. For every fragment, we use this
   information to slide whole_payload_pointer to the correct position in the
   payload buffer.
   It looks something like this:

   whole_payload_pointer
        +
   0 * sender_threshold
        |
        |  whole_payload_pointer   whole_payload_pointer
        |          +                      +
        |  1 * sender_threshold    (N-2) * sender_threshold
        |          |                      |
        |          |                      |
        |          |                      |   whole_payload_pointer
        |          |                      |          +
        |          |                      |   whole_payload_length
        |          |                      |          -
        |          |                      |   last_fragment_length
        |          |                      |          |
        v          v                      v          v
        +----------+--..................--+----------+----------+
        |    #0    |                      |   #N-2   |   #N-1   |
        +----------+--..................--+----------+----------+
         \___  ___/                        \___  ___/ \___  ___/
             \/                                \/         \/
        sender_threshold         sender_threshold       last_fragment_length

         \______________________  _________________________/
                                \/
                        whole_payload_length
   */
  // clang-format on
  for (auto &fragment : fragments) {
    auto *whole_payload_pointer = whole_packet.get_payload_pointer();
    auto const &fragment_header =
        static_cast<Gcs_split_header_v2 &>(fragment.get_current_stage_header());

    // Slide whole_payload_pointer to the correct place.
    auto const &fragment_nr = fragment_header.get_message_part_id();
    bool const is_last_fragment =
        (fragment_nr == fragment_header.get_num_messages() - 1);
    if (!is_last_fragment) {
      auto const &sender_threshold = fragment_header.get_payload_length();
      whole_payload_pointer += fragment_nr * sender_threshold;
    } else {
      auto const &last_fragment_length = fragment.get_payload_length();
      whole_payload_pointer += whole_payload_length - last_fragment_length;
    }

    std::memcpy(whole_payload_pointer, fragment.get_payload_pointer(),
                fragment.get_payload_length());
  }

  result = std::make_pair(OK, std::move(whole_packet));

end:
  return result;
}

Gcs_message_stage::stage_status Gcs_message_stage_split_v2::skip_revert(
    const Gcs_packet &) const {
  return stage_status::apply;
}

bool Gcs_message_stage_split_v2::insert_fragment(Gcs_packet &&packet) {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  bool result = ERROR;
  auto &header =
      static_cast<Gcs_split_header_v2 &>(packet.get_current_stage_header());
  Gcs_packets_list *fragment_list = nullptr;

  /* Get the table with fragments from sender. */
  auto packets_per_source_it =
      m_packets_per_source.find(header.get_sender_id());
  DBUG_ASSERT(packets_per_source_it != m_packets_per_source.end());

  /*
   Insert this fragment into the list of fragments of the packet.

   If this is the first fragment, we create the fragment list first.
   */
  auto &packets_per_content = (*packets_per_source_it).second;
  auto packets_per_content_it =
      packets_per_content.find(header.get_message_id());
  bool const is_first_fragment =
      (packets_per_content_it == packets_per_content.end());
  if (is_first_fragment) {
    // Create the fragment list.
    bool success = false;
    Gcs_packets_list new_fragment_list;
    new_fragment_list.reserve(header.get_num_messages());
    if (new_fragment_list.capacity() != header.get_num_messages()) {
      /* purecov: begin inspected */
      MYSQL_GCS_LOG_ERROR(
          "Error allocating space to contain the set of slice packets")
      goto end;
      /* purecov: end */
    }

    // Insert the fragment list into the table.
    std::tie(packets_per_content_it, success) = packets_per_content.insert(
        std::make_pair(header.get_message_id(), std::move(new_fragment_list)));
    if (!success) {
      MYSQL_GCS_LOG_ERROR("Error gathering packet to eventually reassemble it")
      goto end;
    }
  }
  // Insert the fragment into the list.
  fragment_list = &packets_per_content_it->second;
  fragment_list->push_back(std::move(packet));
  DBUG_ASSERT(fragment_list->size() < header.get_num_messages());

  result = OK;

end:
  return result;
}

void Gcs_message_stage_split_v2::insert_sender(const Gcs_sender_id &sender_id) {
  auto packets_per_source_it = m_packets_per_source.find(sender_id);
  if (packets_per_source_it == m_packets_per_source.end()) {
    m_packets_per_source.insert(
        std::make_pair(sender_id, Gcs_packets_per_content()));
  }
}

Gcs_xcom_synode_set Gcs_message_stage_split_v2::get_snapshot() const {
  Gcs_xcom_synode_set fragment_synodes;
  for (auto const &packets_per_sender : m_packets_per_source) {
    for (auto const &packets_per_content : packets_per_sender.second) {
      for (auto const &fragment : packets_per_content.second) {
        fragment_synodes.insert(fragment.get_delivery_synode());
      }
    }
  }
  return fragment_synodes;
}

void Gcs_message_stage_split_v2::remove_sender(const Gcs_sender_id &sender_id) {
  m_packets_per_source.erase(sender_id);
}
