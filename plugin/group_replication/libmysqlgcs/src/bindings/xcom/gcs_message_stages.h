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

#ifndef GCS_MSG_STAGES_H
#define GCS_MSG_STAGES_H

#include <atomic>
#include <cassert>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"

/**
 This is a stage in the pipeline that processes messages when they are
 put through the send and receive code paths.

 A stage may apply a transformation to the payload of the message that it is
 handling. If it does morph the message, it will append a stage header to
 the message and change the payload accordingly. On the receiving side the
 GCS receiver thread will revert the transformation before delivering the
 message to the application.

 An example of a stage is the LZ4 stage that compresses the payload.

 Developers willing to create a new stage have to inherit from this class and
 implement six virtual methods that are self-explanatory. Note, however, that
 the current semantics assume that each new stage added to the pipeline will
 allocate a new buffer and copy the payload, which may be transformed or not,
 to it.

 This copy assumption makes it easier to create a simple infra-structure to
 add new stages. Currently, this does not represent a performance bottleneck
 but we may revisit this design if it becomes a problem. Note that a quick,
 but maybe not so simple way to overcome this limitation, is through the
 redefinition of the apply and revert methods.
 */
class Gcs_message_stage {
 public:
  enum class stage_status : unsigned int { apply, skip, abort };

 private:
  /**
   Check if the apply operation which affects outgoing packets should be
   executed (i.e. applied), skipped or aborted.

   If the outcome is @code apply or @code skip, the stage will process or skip
   the message, respectively. However, if the outcome is @code abort, the
   message will be discarded and an error will be reported thus stopping the
   pipeline execution.

   For example, if a packet's length is less than a pre-defined threshold the
   packet is not compressed.

   @param packet The packet to which the transformation should be applied.
   @return a status specifying whether the transformation should be executed,
           skipped or aborted
   */
  virtual stage_status skip_apply(const Gcs_packet &packet) const = 0;

  /**
   Check if the revert operation which affects incoming packets should be
   executed (i.e. applied), skipped or aborted.

   If the outcome is @code apply or @code skip, the stage will process or skip
   the message, respectively. However, if the outcome is @code abort, the
   message will be discarded and an error will be reported thus stopping the
   pipeline execution.

   For example, if the packet length is greater than the maximum allowed
   compressed information an error is returned.

   @param packet the packet upon which the transformation should be applied
   @return a status specifying whether the transformation should be executed,
   skipped or aborted
   */
  virtual stage_status skip_revert(const Gcs_packet &packet) const = 0;

  /**
   Calculate or estimate the new payload length that will be produced after
   applying some transformation to the original payload.

   This is used to allocate enough memory to accommodate the transformed
   payload. For example, it is used to allocate a buffer where the compressed
   payload will be stored.

   @param packet The packet upon which the transformation should be applied
   @return the length of the new packet
   */
  virtual unsigned long long calculate_payload_length(
      Gcs_packet &packet) const = 0;

  /**
   Apply some transformation to the old payload and stores the result into a
   new buffer.

   For example, it compresses the old payload and stores the compressed result
   into the new buffer.

   @param [in] version Protocol version of outgoing packet
   @param [out] new_payload_ptr Pointer to the new payload buffer
   @param [in] new_payload_length Calculated or estimated length of the new
   payload
   @param [in] old_payload_ptr Pointer to the old payload buffer
   @param [in] old_payload_length Length of the old payload
   @return a pair where the first element indicates whether the tranformation
   should be aborted and the second the length of the resulting payload
   */
  virtual std::pair<bool, unsigned long long> transform_payload_apply(
      unsigned int version, unsigned char *new_payload_ptr,
      unsigned long long new_payload_length, unsigned char *old_payload_ptr,
      unsigned long long old_payload_length) = 0;

  /**
   Apply some transformation to the old payload and stores the result into a
   new buffer.

   For example, it uncompresses the old payload and stores the uncompressed
   result into the new buffer.

   @param [in] version Protocol version of incoming packet
   @param [out] new_payload_ptr Pointer to the new payload buffer
   @param [in] new_payload_length Calculated or estimated length of the new
   payload
   @param [in] old_payload_ptr Pointer to the old payload buffer
   @param [in] old_payload_length Length of the old payload
   @return a pair where the first element indicates whether the tranformation
   should be aborted and the second the length of the transformed payload
   */
  virtual std::pair<bool, unsigned long long> transform_payload_revert(
      unsigned int version, unsigned char *new_payload_ptr,
      unsigned long long new_payload_length, unsigned char *old_payload_ptr,
      unsigned long long old_payload_length) = 0;

 public:
  /**
   The offset of the header length within the stage header.
   */
  static const unsigned short WIRE_HD_LEN_OFFSET = 0;

  /**
   On-the-wire field size for the stage type code.
   */
  static const unsigned short WIRE_HD_LEN_SIZE = 2;

  /**
   The offset of the stage type code within the stage header.
   */
  static const unsigned short WIRE_HD_TYPE_OFFSET = WIRE_HD_LEN_SIZE;

  /**
   On-the-wire field size for the stage type code.
   */
  static const unsigned short WIRE_HD_TYPE_SIZE = 4;

  /**
   The offset of the payload length within the stage header.
   */
  static const unsigned short WIRE_HD_PAYLOAD_LEN_OFFSET =
      WIRE_HD_TYPE_OFFSET + WIRE_HD_TYPE_SIZE;

  /**
   On-the-wire field size for the stage payload length.
   */
  static const unsigned short WIRE_HD_PAYLOAD_LEN_SIZE = 8;

  /**
   The type codes for the existing stages.

   NOTE: values from this enum must fit into WIRE_HD_TYPE_SIZE bytes storage.
   */
  enum class stage_code : unsigned int {
    // this type should not be used anywhere.
    ST_UNKNOWN = 0,

    // this type represents the compression stage
    ST_LZ4 = 1,

    /**
     No valid type codes can appear after this one. If a type code is to
     be added, this value needs to be incremented and the lowest type code
     available be assigned to the new stage.
     */
    ST_MAX_STAGES = 2
  };

  explicit Gcs_message_stage() : m_is_enabled(true) {}

  explicit Gcs_message_stage(bool enabled) : m_is_enabled(enabled) {}

  virtual ~Gcs_message_stage() {}

  /**
   Return the unique stage code.
   @return the stage code.
   */
  virtual stage_code get_stage_code() const = 0;

  /**
   Apply this stage transformation to the outgoing message.

   @param p the packet to which the transformation should be applied.
   @return false on success, true otherwise.
   */
  virtual bool apply(Gcs_packet &p);

  /**
   Revert the stage transformation on the incoming message.

   @param p the packat to which the transformation should be applied.
   @return false on success, true otherwise.
   */
  virtual bool revert(Gcs_packet &p);

  /**
   Return whether the message stage is enabled or not.
   */
  bool is_enabled() const { return m_is_enabled; }

  /**
   Enable or disable the message stage.

   @param is_enabled Whether the message stage is enabled or disabled.
   */
  void set_enabled(bool is_enabled) { m_is_enabled = is_enabled; }

 private:
  /**
   Replace the current buffer with the result of the new and updated
   buffer.

   @param packet The packet that contains the buffer with the old information.
   @param new_buffer The buffer with the new information.
   @param new_capacity The new buffer capacity.
   @param new_packet_length The new packet length.
   @param dyn_header_length The dynamic header length that will be added or
                            removed from the total amount.
   */
  void swap_buffer(Gcs_packet &packet, unsigned char *new_buffer,
                   unsigned long long new_capacity,
                   unsigned long long new_packet_length, int dyn_header_length);

  /**
   Encode the fixed part of the associated dynamic header information into
   the header buffer.

   @param header Pointer to the header buffer.
   @param header_length Length of the header information.
   @param old_payload_length Length of previous stage payload.
   */
  void encode(unsigned char *header, unsigned short header_length,
              unsigned long long old_payload_length);

  /**
   Decode the fixed part of the associated dynamic header information from the
   header buffer.

   @param header Pointer to the header buffer
   @param[out] header_length Pointer to the length of the header information
   @param[out] old_payload_length Pointer to the length of previous stage
   payload
   */
  void decode(const unsigned char *header, unsigned short *header_length,
              unsigned long long *old_payload_length);

  /**
   Calculate the fixed length of the dynamic header information generated by a
   stage.

   @return the length of the dynamic header
   */
  unsigned short calculate_dyn_header_length() const;

 private:
  /**
   Whether the message stage is enabled or disabled.
   */
  bool m_is_enabled;
};

/**
 Definitions of structures that store the possible message stages and their
 handlers.
 */
using pipeline_version_number = unsigned int;
using Gcs_outgoing_stages = std::vector<Gcs_message_stage::stage_code>;
using Gcs_map_type_handler =
    std::map<Gcs_message_stage::stage_code, std::unique_ptr<Gcs_message_stage>>;
using Gcs_map_version_stages =
    std::map<pipeline_version_number, Gcs_outgoing_stages>;
using Gcs_pair_version_stages =
    std::pair<const pipeline_version_number, Gcs_outgoing_stages>;

/**
 This is the pipeline that an outgoing or incoming message has to go through
 when being sent to or received from the group respectively.

 The message pipeline has stages registered and these are assembled in an
 outgoing pipeline. Then outgoing messages always have to traverse this
 pipeline. For incoming messages, the pipeline is built on the fly, according to
 the information contained in the message stage headers.

 The following rules are always enforced to guarantee safety:

 . A node always knows how to process protocol versions in the domain [initial
   version, max-version-known(node)] by keeping a complete versioned pipeline
 for the entire domain

 . Every time the pipeline or a message changes, the protocol version is
 incremented and new pipeline version is also created accordingly with new codes
 for all the stages

 . Running group can upgrade, but never downgrade, its protocol unless a user
   explicitly request to downgrade it

 . Older nodes attempting to join a group running a newer protocol will discard
   all messages because the messages will either: (a) contain an unknown cargo
   type, or (b) contain an unknown type code

 --- Adding a new stage ---

 If a developer needs to add a new stage to the pipeline, the header protocol
 version number has to be incremented and the pipeline stage updated as follows:

 Gcs_message_old_stage *old_stage =
   pipeline.register_stage<Gcs_message_old_stage>();
 Gcs_message_modified_old_stage *modified_old_stage =
   pipeline.register_stage<Gcs_message_modified_old_stage>();
 Gcs_message_new_stage *new_stage =
   pipeline.register_stage<Gcs_message_new_stage>();

 pipeline.register_pipeline(
   {
     {
       1, {old_stage->get_stage_code()}
     },
     {
       X, {modified_old_stage->get_stage_code(), new_stage->get_stage_code()}
     }
   });

 where X is the header protocol version after the update.

 Note that the difference between the two old stages is only the type code.

 --- Changing stage format ---

 If a developer needs to change any stage format, i.e. replace an existing stage
 of the pipeline, the header protocol version number has to be incremented and
 the pipeline stage updated as follows:

 Gcs_message_old_stage *old_stage =
   pipeline.register_stage<Gcs_message_old_stage>();
 Gcs_message_new_stage *new_stage =
   pipeline.register_stage<Gcs_message_new_stage>();

 pipeline.register_pipeline(
   {
     {
       1, {old_stage->get_stage_code()}
     },
     {
       X, {new_stage->get_stage_code()}
     }
   });

 where X is the header protocol version after the update.

 Note that a new pipeline stage with a unique type code has to be created.
 Besides, every message will carry the current protocol version in use and this
 information is available as part of the fixed header and can be read by any
 stage in order to decide how the message content shall be interpreted.

 --- Changing Cargo ---

 If a developer needs to change a cargo format or create a new one, a new cargo
 type must always be created as the current cargo types are not prepared to be
 extended and the header protocol version number has to be incremented and the
 pipeline stage updated as follows:

 Gcs_message_old_stage *old_stage =
   pipeline.register_stage<Gcs_message_old_stage>();
 Gcs_message_modified_old_stage *modified_old_stage =
   pipeline.register_stage<Gcs_message_modified_old_stage>();

 pipeline.register_pipeline(
   {
     {
       1, {old_stage->get_stage_code()}
     },
     {
       X, {modified_old_stage->get_stage_code()}
     }
   });

 where X is the header protocol version after the update.

 Although the cargo type has no direct relation to the message pipeline stages,
 increasing the protocol version number will allow nodes to decide if versions
 are compatible. Note that the difference between the two old stages is only
 the type code.
 */
class Gcs_message_pipeline {
 private:
  /**
   The registered stages. These are all stages that are known by this version
   of MySQL GCS. This needs to contain an instance of all possible stages,
   since it needs to handle cross-version communication.
   */
  Gcs_map_type_handler m_handlers;

  /**
   This is the pre-assembled outgoing pipelines for the different versions that
   are currently supported, meaning that the stages are traversed in the given
   order.
   */
  Gcs_map_version_stages m_pipelines;

  /**
   The pipeline version in use.
   */
  std::atomic<pipeline_version_number> m_pipeline_version;

 public:
  /**
   Minimum version used by a member whenever it wants to guarantee that a
   message will be received by any other member.
   */
  static const unsigned int MINIMUM_PROTOCOL_VERSION{1};

  /**
   Default version used by a member whenever it starts up and that must be
   incremented whenever there is any change in the pipeline stages.
   */
  static const unsigned int DEFAULT_PROTOCOL_VERSION{1};

  explicit Gcs_message_pipeline()
      : m_handlers(),
        m_pipelines(),
        m_pipeline_version(DEFAULT_PROTOCOL_VERSION) {}

  Gcs_message_pipeline(Gcs_message_pipeline &p) = delete;

  Gcs_message_pipeline &operator=(const Gcs_message_pipeline &p) = delete;

  Gcs_message_pipeline(Gcs_message_pipeline &&p) = delete;

  Gcs_message_pipeline &operator=(Gcs_message_pipeline &&p) = delete;

  virtual ~Gcs_message_pipeline() {}

  /**
    This member function SHALL be called by the message sender. It makes the
    message go through the pipeline of stages before it is actually handed
    over to the group communication engine.

    @param[in] hd Header to send.
    @param[in] p the Packet to send.
    @return false on success, true otherwise.
   */
  bool outgoing(Gcs_internal_message_header &hd, Gcs_packet &p) const;

  /**
    This member function SHALL be called by the receiver thread to process the
    message through the stages it was processed when it was sent. This reverts
    the effect on the receiving end.

    @param p the packet to process.
    @return false on sucess, true otherwise.
   */
  bool incoming(Gcs_packet &p) const;

  /**
   Register a stage to be used by the pipeline.

   @param args Parameters to the stage constructor
   */
  template <class T, class... Args>
  void register_stage(Args... args) {
    std::unique_ptr<T> stage(new T(args...));

    if (stage != nullptr) {
      Gcs_message_stage::stage_code code = stage->get_stage_code();
      Gcs_message_stage *ptr = retrieve_stage(code);
      if (ptr == nullptr) {
        m_handlers.insert(
            std::make_pair(stage->get_stage_code(), std::move(stage)));
      }
    }
  }

  /**
   Check whether a stage is registered or not.

   @param code Stage code
   @return whether a stage is registered or not.
   */
  bool contains_stage(Gcs_message_stage::stage_code code) const {
    return retrieve_stage(code) != nullptr;
  }

  /*
   Return a reference to a stage. Note that the stage must exist, otherwise,
   the call will lead to an undefined behavior.

   @param code Stage code
   @return a reference to a stage
   */
  const Gcs_message_stage &get_stage(Gcs_message_stage::stage_code code) const {
    Gcs_message_stage *ptr = retrieve_stage(code);
    assert(ptr != nullptr);
    return *ptr;
  }

  /**
   Register the stages per version that form the different pipelines.

   @param stages Initialization list that contains a mapping between a
                 version and the associated pipeline stages.

   @return false on success, true otherwise.
   */
  bool register_pipeline(std::initializer_list<Gcs_pair_version_stages> stages);

  /**
   Check whether a pipeline version is registered or not.

   @param pipeline_version Pipeline version
   @return whether a pipeline version is registered or not.
   */
  bool contains_pipeline(pipeline_version_number pipeline_version) const {
    return retrieve_pipeline(pipeline_version) != nullptr;
  }

  /*
   Return a reference to a pipeline version. Note that the pipeline version
   must exist, otherwise, the call will lead to an undefined behavior.

   @param pipeline_version Pipeline version
   @return a reference to a pipeline
   */
  const Gcs_outgoing_stages &get_pipeline(
      pipeline_version_number pipeline_version) const {
    const Gcs_outgoing_stages *ptr = retrieve_pipeline(pipeline_version);
    assert(ptr != nullptr);
    return *ptr;
  }

  /**
   Clean all data structures and objects created.
   */
  void cleanup();

  /**
   Set the pipeline version in use.

   @param pipeline_version Pipeline version.
   @return false if successfully set, true otherwise
   */
  bool set_version(pipeline_version_number pipeline_version);

  /**
   Return the pipeline version in use.
   */
  pipeline_version_number get_version() const;

 private:
  /*
   Retrieve the stages associated with a pipeline version.

   @param pipeline_version Pipeline version
   */
  const Gcs_outgoing_stages *retrieve_pipeline(
      pipeline_version_number pipeline_version) const;

  /*
   This member function SHALL retrive the associated stage if there is any,
   otherwise a null pointer is returned.

   @param stage_code unique stage code
   */
  Gcs_message_stage *retrieve_stage(
      Gcs_message_stage::stage_code stage_code) const;

  /*
   This member function SHALL retrive the current stage type code of a packet.

   @param p the packet to process.
   */
  Gcs_message_stage *retrieve_stage(const Gcs_packet &p) const;
};
#endif
