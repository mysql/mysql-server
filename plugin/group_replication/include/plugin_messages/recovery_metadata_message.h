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

#ifndef RECOVERY_METADATA_MESSAGE_INCLUDED
#define RECOVERY_METADATA_MESSAGE_INCLUDED

#include <map>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/gr_compression.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

class Recovery_metadata_message : public Plugin_gcs_message {
 public:
  /**
    Recovery Metadata message payload type
  */
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,
    // Length of the payload item: variable
    PIT_VIEW_ID = 1,
    // Length of the payload item: variable
    PIT_RECOVERY_METADATA_COMPRESSION_TYPE = 2,
    // Length of the payload item: variable
    PIT_UNTIL_CONDITION_AFTER_GTIDS = 3,
    // Length of the payload item: variable
    PIT_COMPRESSED_CERTIFICATION_INFO_PACKET_COUNT = 4,
    // Length of the payload item: variable
    PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD = 5,
    // Length of the payload item: variable
    PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH = 6,
    // Length of the payload item: variable
    PIT_RECOVERY_METADATA_MESSAGE_ERROR = 7,
    // Length of the payload item: variable
    PIT_SENT_TIMESTAMP = 8,
    // No valid type codes can appear after this one.
    PIT_MAX = 9
  };

  /**
    Recovery Metadata message payload type name
  */
  std::map<int, std::string> m_payload_item_type_string{
      {PIT_UNKNOWN, "Unknown Type"},
      {PIT_VIEW_ID, "View ID"},
      {PIT_RECOVERY_METADATA_COMPRESSION_TYPE, "Compression Type"},
      {PIT_UNTIL_CONDITION_AFTER_GTIDS, "Executed Gtid Set"},
      {PIT_COMPRESSED_CERTIFICATION_INFO_PACKET_COUNT,
       "Compressed Certification Info Packet Count"},
      {PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD,
       "Compressed Certification Info Payload"},
      {PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH,
       "Certification Info packet ucompressed length"},
      {PIT_RECOVERY_METADATA_MESSAGE_ERROR, "Sender Message Error"},
      {PIT_SENT_TIMESTAMP, "Sent Timestamp"}};

  /**
    Recovery Metadata message payload error
  */
  typedef enum {
    /* GR Recovery Metadata no error */
    RECOVERY_METADATA_NO_ERROR = 0,
    /* GR Recovery Metadata send error */
    RECOVERY_METADATA_ERROR = 1
  } Recovery_metadata_message_payload_error;

  /**
    Recovery Metadata message error
  */
  enum class enum_recovery_metadata_message_error {
    // Recovery Metadata Message received/decoded without error.
    RECOVERY_METADATA_MESSAGE_OK,
    // Received Recovery Metadata Message Certification Info is empty.
    ERR_CERT_INFO_EMPTY,
    // Received Recovery Metadata Message executed gtid_set encoding error.
    ERR_AFTER_GTID_SET_ENCODING,
    // Received Recovery Metadata Message payload buffer empty.
    ERR_PAYLOAD_BUFFER_EMPTY,
    // Received Recovery Metadata Message payload type decoding error.
    ERR_PAYLOAD_TYPE_DECODING,
    // Received Recovery Metadata Message payload decoding error.
    ERR_PAYLOAD_DECODING,
    // Received Recovery Metadata Message payload type not decoded.
    ERR_PAYLOAD_TYPE_NOT_DECODED,
    // Received Recovery Metadata Message payload type not known.
    ERR_PAYLOAD_TYPE_UNKOWN
  };

  /**
    Message constructor

    @param[in] view_id          the view_id generated when new member joined
    @param[in] error            the message error type
    @param[in] compression_type the compression type with which
                                Certification Information compressed
  */
  Recovery_metadata_message(
      const std::string &view_id,
      Recovery_metadata_message_payload_error error =
          RECOVERY_METADATA_NO_ERROR,
      GR_compress::enum_compression_type compression_type =
          GR_compress::enum_compression_type::ZSTD_COMPRESSION);

  /**
    Message destructor
  */
  virtual ~Recovery_metadata_message() override;

  /**
    Message constructor for raw data

    @param[in] buf              raw data
    @param[in] len              raw length
  */
  Recovery_metadata_message(const uchar *buf, size_t len);

  /**
    Return the time at which the message contained in the buffer was sent.
    @see Metrics_handler::get_current_time()

    @param[in] buffer                       the buffer to decode from.
    @param[in] length                       the buffer length

    @return the time on which the message was sent.
  */
  static uint64_t get_sent_timestamp(const unsigned char *buffer,
                                     size_t length);

  /**
    Return the message view_id which was generated when new member joined.

    @return  the view_id which was generated when new member joined.
  */
  std::string &get_encode_view_id();

  /**
    Return the message error type.

    @return  the Recovery_metadata_message_payload_error message error.
  */
  Recovery_metadata_message_payload_error get_encode_message_error();

  /**
    Set the message error type
  */
  void set_encode_message_error();

  /**
    Return the compression type with which Certification Information compressed.

    @return  the GR_compress::enum_compression_type compression type.
  */
  GR_compress::enum_compression_type get_encode_compression_type();

  /**
    Returns the gtid executed from the certification info map of the donor. It
    is the set of transactions that is executed at the time of
    view change at donor.

    @return the reference to string containing the gtid executed from the
            certification info map of the donor.
  */
  std::string &get_encode_group_gtid_executed();

  /**
    Returns the compressed Certification Information divided in multiple
    elements of the vector.

    @return the reference to vector containing compressed Certification
            Information.
  */
  std::vector<GR_compress *> &get_encode_compressor_list();

  /**
    Set members joining the view.

    @param[in] joining_members   members joining the view
  */
  void set_joining_members(std::vector<Gcs_member_identifier> &joining_members);

  /**
    Set members ONLINE in the view.

    @param[in] online_members   members ONLINE in view
  */
  void set_valid_metadata_senders(
      std::vector<Gcs_member_identifier> &online_members);

  /**
    Set the vector m_valid_metadata_sender_list.
  */
  void sort_valid_metadata_sender_list_using_uuid();

  /**
    Set metadata sender.

    @param[in] sender_gcs_id  GCS Member ID of the sender.
  */
  void set_metadata_sender(Gcs_member_identifier &sender_gcs_id);

  /**
    Check if recovery metadata donor has left the group.

    @return the recovery metadata send status
      @retval true    donor left the group
      @retval false   donor is still on the group
      true donor left

  */
  bool donor_left();

  /**
    Compute the current metadata sender and return the GCS Member ID of the
    current metadata sender.

    @return the Gcs_member_identifier of the current sender
  */
  std::pair<bool, Gcs_member_identifier>
  compute_and_get_current_metadata_sender();

  /**
    Checks if local member is metadata sender.

    @note: compute_and_get_current_metadata_sender should be called before this
    function to make sure metadata sender is updated. Since
    am_i_recovery_metadata_sender will be called many times, compute of the
    current metadata sender has been separated into
    compute_and_get_current_metadata_sender.

    @return the status if local member is metadata sender.
      @retval false   local member is recovery metadata sender.
      @retval true    local member is not recovery metadata sender.
  */
  bool am_i_recovery_metadata_sender();

  /**
    Remove the members that left the group from the joining and valid sender
    list.

    @param[in] member_left  All GCS Member ID that left the group.
  */
  void delete_members_left(std::vector<Gcs_member_identifier> &member_left);

  /**
    Return if joiner and valid metadata sender list is empty or not.
    If any of the list is empty true is return.

    @return the recovery metadata send status
      @retval false   Atleast 1 joiner and valid metadata sender is present in
    group
      @retval true    Either there are no joiner or valid metadata sender
  */
  bool is_joiner_or_valid_sender_list_empty();

  /**
    Save copy of undecoded metadata, so it can be decoded and used later when
    required.

    The metadata is broadcasted to all group members but only joiners require
    after_gtids and compressed certification info part of metadata. And on all
    other members only require view_id and metadata error part of metadata.
    The view id and metadata error part only is decoded by GCS thread. Only
    joiner require this rest of metadata part for receovery and processing on
    GCS thread will block it for longer duration, so it's copy is saved in this
    function and decoded by recovery thread.

    @return the status showing metadata message second part saved success.
      @retval false   OK
      @retval true    Error
  */
  bool save_copy_of_recovery_metadata_payload();

  /**
    Delete the saved copy of undecoded metadata.
  */
  void delete_copy_of_recovery_metadata_payload();

  /**
    Set the decoded message error type
  */
  void set_decoded_message_error();

  /**
    Return view_id generated when new member joined.
    Decodes and return view_id generated when new member joined. If the view_id
    was decoded earlier it return saved view_id value.

    @return the std::pair of <enum_recovery_metadata_message_error error type,
                              refernce to View ID string>
  */
  std::pair<enum_recovery_metadata_message_error,
            std::reference_wrapper<std::string>>
  get_decoded_view_id();

  /**
    Return the payload send message error received error in Recovery Metadata
    message. If the payload send message error was decoded earlier it return
    saved error value.

    @return the std::pair of <enum_recovery_metadata_message_error error,
                              Recovery_metadata_message_payload_error
                                payload_error>.
  */
  std::pair<enum_recovery_metadata_message_error,
            Recovery_metadata_message_payload_error>
  get_decoded_message_error();

  /**
    Return the compression type with which Certification Information compressed
    in received Recovery Metadata message. If the compression type was already
    decoded earlier it returns saved compression type value.

    @return the std::pair of <enum_recovery_metadata_message_error error,
                              GR_compress::enum_compression_type>.
  */
  std::pair<enum_recovery_metadata_message_error,
            GR_compress::enum_compression_type>
  get_decoded_compression_type();

  /**
    Return the executed gtid set received from sender in received Recovery
    Metadata message. If the executed gtid set was already decoded earlier it
    returns saved executed gtid set value.

    @return the std::pair of <enum_recovery_metadata_message_error error,
                              executed gtid set>.
  */
  std::pair<enum_recovery_metadata_message_error,
            std::reference_wrapper<std::string>>
  get_decoded_group_gtid_executed();

  /**
    Return the Compressed Certification Info packet count received in Recovery
    Metadata message. If the Compressed Certification Info packet count was
    already decoded earlier it returns saved value.

    @return the std::pair of <enum_recovery_metadata_message_error error,
                              Compress Certification Info packet count>.
  */
  std::pair<enum_recovery_metadata_message_error, unsigned int>
  get_decoded_compressed_certification_info_packet_count();

  /**
    Return the Certification Info packet's uncompressed_length i.e. size of
    packet before compression. The Recovery Metadata message can have multiple
    packets of Compressed Certification Info. If the start position argument is
    provided the next Certification Info packet uncompressed_length packet type
    i.e. PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH packet type is
    searched and fetched after that position.

    @param[in] payload_start_pos  the next Certification Info packet
                                  uncompressed_length packet type is searched
                                  and fetched after the position.

    @return the std::tuple of <enum_recovery_metadata_message_error error,
            Certification Info packet uncompressed_length i.e. size of
            packet before compression, length of uncompressed_length payload>.
  */
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             unsigned long long, unsigned long long>
  get_decoded_compressed_certification_info_uncompressed_length(
      const unsigned char *payload_start_pos);

  /**
    Return the compressed Certification Info payload. The Recovery Metadata
    message can have multiple packets of Compressed Certification Info. If
    the start position argument is provided the next compressed Certification
    Info packet type i.e. PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD packet type
    is searched and fetched after that position.

    @param[in] payload_start_pos  the next compressed Certification Info packet
                                  packet type is searched and fetched after the
                                  position.

    @return the std::tuple of <enum_recovery_metadata_message_error error,
            compressed Certification Info packet, length of compressed
            Certification Info packet>.
  */
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
  get_decoded_compressed_certification_info_payload(
      const unsigned char *payload_start_pos);

 protected:
  /**
    Encodes the message contents for transmission.

    @param[out] buffer   the message buffer to be written
  */
  void encode_payload(std::vector<unsigned char> *buffer) const override;

  /**
    Message decoding method

    @param[in] buffer the received data
    @param[in] end    the end of the buffer
  */
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *end) override;

 private:
  /**
    Encodes the certification info for transmission.

    @param[out] buffer the buffer to encode to

    @return the operation status
      @retval false    OK
      @retval true     Error
  */
  bool encode_compressed_certification_info_payload(
      std::vector<unsigned char> *buffer) const;

  /** The view_id generated when new member joined */
  std::string m_encode_view_id;

  /** The enum_recovery_metadata_message_error message error type */
  Recovery_metadata_message_payload_error m_encode_metadata_message_error;

  /** The compression type with which Certification Information compressed */
  GR_compress::enum_compression_type m_encode_metadata_compression_type;

  /**
    The executed gtid set received from sender.
  */
  std::string m_encoded_group_gtid_executed{};

  /**
    Message decoding method to decode received metadata payload.
    The metadata is broadcasted to all group members but only joiners require
    after_gtids and compressed certification info part of metadata. And on all
    other members only require view_id and metadata error part of metadata.

    @param[in] payload_type  the payload type to be decoded
    @param[in] payload_start If payload_start is provided then payload types are
                             search from that position

    @return the std::pair of payload type and
            enum_recovery_metadata_message_error error type.
  */
  std::tuple<enum_recovery_metadata_message_error, const unsigned char *,
             unsigned long long>
  decode_payload_type(int payload_type,
                      const unsigned char *payload_start = nullptr) const;

  /** The view_id received from sender. */
  std::string m_decoded_view_id;

  /**
    The executed gtid set from sender.
  */
  std::string m_decoded_group_gtid_executed{};

  /**
    The pair of <enum_recovery_metadata_message_error error, View ID>.
    The View ID generated when new member joined.
  */
  std::pair<enum_recovery_metadata_message_error,
            std::reference_wrapper<std::string>>
      m_decoded_view_id_error;

  /**
    The pair of <enum_recovery_metadata_message_error error,
                 Recovery_metadata_message_payload_error payload_error>.
    The payload error is received error in Recovery Metadata message and
    determines errors while delivering message to group.
  */
  std::pair<enum_recovery_metadata_message_error,
            Recovery_metadata_message_payload_error>
      m_decoded_message_send_error{
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          RECOVERY_METADATA_NO_ERROR};

  /**
    The pair of <enum_recovery_metadata_message_error error,
                 GR_compress::enum_compression_type>.
    The compression type with which Certification Information compressed.
  */
  std::pair<enum_recovery_metadata_message_error,
            GR_compress::enum_compression_type>
      m_decoded_compression_type_error{
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          GR_compress::enum_compression_type::ZSTD_COMPRESSION};

  /**
    The pair of <enum_recovery_metadata_message_error error,
                 executed gtid set>.
    The executed gtid set is received from sender.
  */
  std::pair<enum_recovery_metadata_message_error,
            std::reference_wrapper<std::string>>
      m_decoded_group_gtid_executed_error;

  /**
    The pair of <enum_recovery_metadata_message_error error,
                 Compress Certification Info packet count>.
    The Compress Certification Info packet count is the number of compressed
    Certification Info packets received from the Recovery Metadata sender.
  */
  std::pair<enum_recovery_metadata_message_error, unsigned int>
      m_decoded_certification_info_packet_count_error{
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          0};

  /**
    The tuple of <enum_recovery_metadata_message_error error,
                  Certification Info packet uncompressed_length i.e. size of
                  packet before compression,
                  length of uncompressed_length payload>.
  */
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             unsigned long long, unsigned long long>
      m_decoded_certification_info_uncompressed_length_error;

  /**
    The std::tuple of <enum_recovery_metadata_message_error error,
                       compressed Certification Info packet,
                       length of compressed Certification Info packet>.
  */
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      m_decoded_compressed_certification_info_error;

  /**
    The Certification Information in compressed and serialized format,
    used by the joining member for the recovery.
  */
  mutable std::vector<GR_compress *> m_encode_compressor_list;

  /** The members joining in the view */
  std::vector<Gcs_member_identifier> m_members_joined_in_view;

  /** The members ONLINE in the view */
  std::vector<Gcs_member_identifier> m_valid_metadata_senders;

  /** The GCS Member ID of the member sending recovery metadata. */
  Gcs_member_identifier m_member_id_sending_metadata;

  // These variables are used for joiner recovery to mark position on message
  // buffer to be decoded.
  /**
    This pointer is added to start of buffer part which is yet to be decoded.
    The buffer is the metadata message payload received from metadata sender
    members consisting of after_gtids, compressed certification info payload.
  */
  const unsigned char *m_decode_metadata_buffer;

  /**
    The metadata is broadcasted to all group members but only joiners require
    after_gtids and compressed certification info part of metadata which is
    processed by recovery thread, while view_id and metadata error part of
    metadata required by other members is processed by GCS thread. The
    processing of after_gtids and compressed certification info on GCS thread
    will block it for longer duration, so it's copy is saved in this
  */
  bool m_decode_is_metadata_buffer_local_copy{false};

  /**
    The end position of received metadata message payload.
  */
  const unsigned char *m_decode_metadata_end;

  /**
    The length of yet to be decoded part of buffer.
    The buffer is the metadata message payload received from metadata sender
    members consisting of after_gtids, compressed certification info payload.
  */
  size_t m_decode_metadata_length{0};
};

#endif /* RECOVERY_METADATA_MESSAGE_INCLUDED */
