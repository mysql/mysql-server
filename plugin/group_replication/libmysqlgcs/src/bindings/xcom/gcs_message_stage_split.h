/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef GCS_MESSAGE_STAGE_SPLIT_H
#define GCS_MESSAGE_STAGE_SPLIT_H

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

using Gcs_message_id = unsigned long long;
using Gcs_sender_id = uint64_t;

/**
  This class is responsible for controlling message fragmentation and bundling
  and produces messages with the following format:

  ---------------------------------------------------------------
  | Sender Id | Message Id | Part Id | Num Msg | Size | Payload |
  ---------------------------------------------------------------

  . Sender Id - Member Identifier (i.e. incarnation UUID) that is used to
  uniquely identify the original sender.

  . Message Id - Message Identifier (i.e. transaction counter) that is used to
    uniquely identify the original message.

  . Part Id - Fragment of the original message that the payload corresponds to.

  . Num Msg - The total number of messages that together compose the original
    message.

  . Size - The payload size that is being carried on by this message.

  . Payload - Payload that corresponds to the part of the original message that
    it is carrying on.
*/
class Gcs_split_header_v2 : public Gcs_stage_metadata {
 public:
  /**
   On-the-wire field size for the number of messages.
   */
  static const unsigned short WIRE_HD_NUM_MESSAGES_SIZE = 4;

  /**
   On-the-wire field size for the sender identification size.
   */
  static const unsigned short WIRE_HD_SENDER_ID_SIZE = 8;

  /**
   On-the-wire field size for the message sequence (i.e. identification).
   */
  static const unsigned short WIRE_HD_MESSAGE_ID_SIZE = 8;

  /**
   On-the-wire field size for the message part sequence (i.e. identification).
   */
  static const unsigned short WIRE_HD_MESSAGE_PART_ID_SIZE = 4;

  /**
   On-the-wire field size for payload length.
   */
  static const unsigned short WIRE_HD_PAYLOAD_SIZE = 8;

 private:
  /**
   Uniquely identify the sender which the message belongs to.
   */
  Gcs_sender_id m_sender_id;

  /**
   Uniquely identify the message so that we can reassemble split messages.
   */
  Gcs_message_id m_message_id{0};
  static_assert(sizeof(decltype(m_message_id)) == WIRE_HD_MESSAGE_ID_SIZE,
                "The m_message_id size does not match the storage capacity");

  /**
   Determine the number of original messages that are included here.
   */
  unsigned int m_num_messages{1};
  static_assert(sizeof(decltype(m_num_messages)) == WIRE_HD_NUM_MESSAGES_SIZE,
                "The m_num_messages size does not match the storage capacity");

  /**
   Determine the part in the original message that the current payload
   corresponds to. Note that the value starts at 0.
   */
  unsigned int m_message_part_id{0};
  static_assert(
      sizeof(decltype(m_message_part_id)) == WIRE_HD_MESSAGE_PART_ID_SIZE,
      "The m_message_part_id size does not match the storage capacity");

  /**
   Size of the current payload which is a full message or part of a message.
   */
  unsigned long long m_payload_length{0};
  static_assert(sizeof(decltype(m_payload_length)) == WIRE_HD_PAYLOAD_SIZE,
                "The m_payload_len size does not match the storage capacity");

 public:
  explicit Gcs_split_header_v2() = default;

  explicit Gcs_split_header_v2(const Gcs_sender_id &sender_id,
                               Gcs_message_id message_id,
                               unsigned int num_messages,
                               unsigned int message_part_id,
                               unsigned long long payload_length) noexcept
      : m_sender_id(sender_id),
        m_message_id(message_id),
        m_num_messages(num_messages),
        m_message_part_id(message_part_id),
        m_payload_length(payload_length) {}

  std::unique_ptr<Gcs_stage_metadata> clone() override {
    return std::unique_ptr<Gcs_split_header_v2>(new Gcs_split_header_v2(*this));
  }

  /**
   Set the sender identifier.

   @param sender_id Sender identification.
   */
  void set_sender_id(const Gcs_sender_id &sender_id) {
    m_sender_id = sender_id;
  }

  /**
   Return the sender identifier.
   */
  const Gcs_sender_id &get_sender_id() const { return m_sender_id; }

  /**
   Set the message identifier.

   @param message_id Message identification.
   */
  void set_message_id(Gcs_message_id message_id) { m_message_id = message_id; }

  /**
   Return the message identifier.
   */
  Gcs_message_id get_message_id() const { return m_message_id; }

  /**
   Set the number of messages bundled together.

   @param num_messages Number of messages bundled together.
   */
  void set_num_messages(unsigned int num_messages) {
    m_num_messages = num_messages;
  }

  /**
   Return the number of messages bundled together.
   */
  unsigned int get_num_messages() const { return m_num_messages; }

  /**
   Set the part that identifies this message.

   @param message_part_id Part that identifies this message.
   */
  void set_message_part_id(unsigned int message_part_id) {
    m_message_part_id = message_part_id;
  }

  /**
   Return the part that identifies this message.
   */
  unsigned int get_message_part_id() const { return m_message_part_id; }

  /**
   Set the payload length.

   @param payload_length Payload buffer length.
   */
  void set_payload_length(unsigned long long payload_length) {
    m_payload_length = payload_length;
  }

  /**
   Return the payload length.
   */
  unsigned long long get_payload_length() const { return m_payload_length; }

  /**
   Decodes the contents of the buffer and sets the field values according to
   the values decoded. The buffer MUST be encoded in little endian format.

   @param buffer The buffer to decode from.
   @return Length of the encoded information.
   */
  unsigned long long decode(const unsigned char *buffer) override;

  /**
   Encode the contents of this instance into the buffer. The encoding SHALL
   be done in little endian format.

   @param buffer The buffer to encode to.
   @return Length of the encoded information.
   */
  unsigned long long encode(unsigned char *buffer) const override;

  /**
   Calculate the length used to store the stage header information.
   */
  unsigned long long calculate_encode_length() const override {
    return fixed_encode_length();
  }

  /**
   Create a string representation of the header to be logged.

   @param output Reference to the output stream where the string will be
          created.
   */
  void dump(std::ostringstream &output) const override;

 private:
  /**
   Helper method to calculate the length used to store the stage header
   information.
   */
  static unsigned long long fixed_encode_length() {
    return WIRE_HD_NUM_MESSAGES_SIZE + WIRE_HD_SENDER_ID_SIZE +
           WIRE_HD_MESSAGE_ID_SIZE + WIRE_HD_MESSAGE_PART_ID_SIZE +
           WIRE_HD_PAYLOAD_SIZE;
  }
};

using Gcs_packets_list = std::vector<Gcs_packet>;
using Gcs_packets_per_content =
    std::unordered_map<Gcs_message_id, Gcs_packets_list>;
using Gcs_packets_per_sender =
    std::unordered_map<Gcs_sender_id, Gcs_packets_per_content>;

class Gcs_message_stage_split_v2 : public Gcs_message_stage {
 public:
  /**
   Default split threshold.
   */
  static constexpr unsigned long long DEFAULT_THRESHOLD = 1048576;

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

 private:
  /*
   Set of packets received that cannot be immediately delivered because its
   related fragments were not received yet.
   */
  Gcs_packets_per_sender m_packets_per_source;

  /**
   Unique sender identifier that is dynamically generated when a node rejoins
   the group.
   */
  Gcs_sender_id m_sender_id{0};

  /**
   This marks the threshold in bytes above which a message shall be split.
   */
  unsigned long long m_split_threshold{DEFAULT_THRESHOLD};

  /**
   Unique message identifier per sender.
   */
  std::atomic<Gcs_message_id> m_next_message_number{1};

 public:
  /**
   Creates an instance of the stage.

   @param enabled enables this message stage
   @param split_threshold messages with the payload larger
                          than split_threshold in bytes are split.
   */
  explicit Gcs_message_stage_split_v2(bool enabled,
                                      unsigned long long split_threshold)
      : Gcs_message_stage(enabled),
        m_packets_per_source(),
        m_split_threshold(split_threshold) {}

  ~Gcs_message_stage_split_v2() override { m_packets_per_source.clear(); }

  /**
   Return the stage code.
   */
  Stage_code get_stage_code() const override { return Stage_code::ST_SPLIT_V2; }

  /**
   Update the list of members in the group as this is required to process split
   messages.

   @param me The local member identifier.
   @param xcom_nodes List of members in the group.
   @return If there is an error, true is returned. Otherwise, false is returned.
   */
  bool update_members_information(const Gcs_member_identifier &me,
                                  const Gcs_xcom_nodes &xcom_nodes) override;

  /**
    Sets the threshold in bytes after which messages are split.

    @param split_threshold If the payload exceeds these many bytes, then
                           the message is split.
   */
  void set_threshold(unsigned long long split_threshold) {
    m_split_threshold = split_threshold;
  }

 private:
  /**
   Insert a packet into the mapping that keeps track of fragments.

   This method must only called when the packet received is part of a fragmented
   message.

   @param packet fragment Fragment that will be collected to reconstruct the
   original
   @returns true if successful, false otherwise
   */
  bool insert_fragment(Gcs_packet &&packet);

  /**
   Insert a sender into the mapping that keeps track of sliced packets.

   @param sender_id Source identification
   */
  void insert_sender(const Gcs_sender_id &sender_id);

  /**
   Remove a sender from the mapping that keeps track of sliced packets.

   @param sender_id Source identification
   */
  void remove_sender(const Gcs_sender_id &sender_id);

  Gcs_xcom_synode_set get_snapshot() const override;

  void apply_transformation_single_fragment(Gcs_packet &packet) const;

  std::pair<bool, std::vector<Gcs_packet>> create_fragments(
      Gcs_packet &&packet, unsigned int const &nr_fragments) const;

  std::pair<bool, Gcs_packet> create_fragment(
      unsigned int const &fragment_part_id, Gcs_packet const &other_fragment,
      unsigned char const *const original_payload_pointer,
      unsigned long long const &fragment_size) const;

  bool unknown_sender(Gcs_split_header_v2 const &fragment_header) const;

  bool is_final_fragment(Gcs_split_header_v2 const &fragment_header) const;

  /**
   Fetch the fragments associated with the given metadata.
   Removes the fragments from the table of ongoing tranmissions.

   This method must only be called if there were previous calls to @c
   insert_fragment, i.e. if given metadata is about a fragmented message.

   @param fragment_header Fragmentation metadata
   @returns the list of already received fragments.
   */
  Gcs_packets_list get_fragments(Gcs_split_header_v2 const &fragment_header);

  /**
   Reassembles the given fragment list into the original, whole packet.

   This method must only be called with a non-empty packet list.

   @param fragments The list of packet to reassemble
   @retval {true, Gcs_packet} If reassembled successfully
   @retval {false, _} If we could not allocate memory for the reassembled packet
   */
  std::pair<bool, Gcs_packet> reassemble_fragments(
      Gcs_packets_list &fragments) const;
};

class Gcs_message_stage_split_v3 : public Gcs_message_stage_split_v2 {
 public:
  /**
   Creates an instance of the stage.

   @param enabled enables this message stage
   @param split_threshold messages with the payload larger
                          than split_threshold in bytes are split.
   */
  explicit Gcs_message_stage_split_v3(bool enabled,
                                      unsigned long long split_threshold)
      : Gcs_message_stage_split_v2(enabled, split_threshold) {}

  ~Gcs_message_stage_split_v3() override {}

  /**
   Return the stage code.
   */
  Stage_code get_stage_code() const override { return Stage_code::ST_SPLIT_V3; }
};

#endif /* GCS_MESSAGE_STAGE_SPLIT_H */
