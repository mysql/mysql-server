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

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"

/**
 Error code for the pipeline's processing of incoming packets.
 */
enum class Gcs_pipeline_incoming_result {
  /** Successful, and returned a packet. */
  OK_PACKET,
  /**
   Successful, but produces no packet.
   E.g. the incoming packet is a fragment, so it was buffered until all
   fragments arrive and we reassemble the original message.
   */
  OK_NO_PACKET,
  /** Unsuccessful. */
  ERROR
};

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

  /**
   Check if the apply operation which affects outgoing packets should be
   executed (i.e. applied), skipped or aborted.

   If the outcome is code apply or code skip, the stage will process or skip
   the message, respectively. However, if the outcome is code abort, the
   message will be discarded and an error will be reported thus stopping the
   pipeline execution.

   For example, if a packet's length is less than a pre-defined threshold the
   packet is not compressed.

   @param original_payload_size The size of the packet to which the
          transformation should be applied.
   @return a status specifying whether the transformation should be executed,
           skipped or aborted
   */
  virtual stage_status skip_apply(
      uint64_t const &original_payload_size) const = 0;

  virtual std::unique_ptr<Gcs_stage_metadata> get_stage_header() = 0;

 protected:
  /**
   Check if the revert operation which affects incoming packets should be
   executed (i.e. applied), skipped or aborted.

   If the outcome is code apply or code skip, the stage will process or skip
   the message, respectively. However, if the outcome is code abort, the
   message will be discarded and an error will be reported thus stopping the
   pipeline execution.

   For example, if the packet length is greater than the maximum allowed
   compressed information an error is returned.

   @param packet The packet upon which the transformation should be applied
   @return a status specifying whether the transformation should be executed,
           skipped or aborted
   */
  virtual stage_status skip_revert(const Gcs_packet &packet) const = 0;

  /**
   Implements the logic of this stage's transformation to the packet, and
   returns a set of one, or more, transformed packets.

   @param[in] packet The packet upon which the transformation should be applied
   @retval {true, _} If there was an error applying the transformation
   @retval {false, P} If the transformation was successful, and produced the
           set of transformed packets P
   */
  virtual std::pair<bool, std::vector<Gcs_packet>> apply_transformation(
      Gcs_packet &&packet) = 0;

  /**
   Implements the logic to revert this stage's transformation to the packet,
   and returns one, or none, transformed packet.

   @param[in] packet The packet upon which the transformation should be reverted
   @retval {ERROR, _} If there was an error reverting the transformation
   @retval {OK_NO_PACKET, NP} If the transformation was reverted, but produced
           no packet
   @retval {OK_PACKET, P} If the transformation was reverted, and produced the
           packet P
   */
  virtual std::pair<Gcs_pipeline_incoming_result, Gcs_packet>
  revert_transformation(Gcs_packet &&packet) = 0;

 public:
  explicit Gcs_message_stage() : m_is_enabled(true) {}

  explicit Gcs_message_stage(bool enabled) : m_is_enabled(enabled) {}

  virtual ~Gcs_message_stage() = default;

  /**
   Return the unique stage code.
   @return the stage code.
   */
  virtual Stage_code get_stage_code() const = 0;

  /**
   Apply some transformation to the outgoing packet, and return a set of one,
   or more, transformed packets.

   @param[in] packet The packet upon which the transformation should be applied
   @retval {true, _} If there was an error applying the transformation
   @retval {false, P} If the transformation was successful, and produced the
           set of transformed packets P
   */
  std::pair<bool, std::vector<Gcs_packet>> apply(Gcs_packet &&packet);

  /**
   Revert some transformation from the incoming packet, and return one, or
   none, transformed packet.

   @param[in] packet The packet upon which the transformation should be reverted
   @retval {ERROR, _} If there was an error reverting the transformation
   @retval {OK_NO_PACKET, NP} If the transformation was reverted, but produced
           no packet
   @retval {OK_PACKET, P} If the transformation was reverted, and produced the
           packet P
   */
  std::pair<Gcs_pipeline_incoming_result, Gcs_packet> revert(
      Gcs_packet &&packet);

  /**
   Return whether the message stage is enabled or not.
   */
  bool is_enabled() const { return m_is_enabled; }

  /**
   Update the list of members in the group as this may be required by some
   stages in the communication pipeline. By default though, the call is simply
   ignored.

   @return If there is an error, true is returned. Otherwise, false is returned.
   */
  virtual bool update_members_information(const Gcs_member_identifier &,
                                          const Gcs_xcom_nodes &) {
    return false;
  }

  virtual Gcs_xcom_synode_set get_snapshot() const { return {}; }

  /**
   Enable or disable the message stage.

   @param is_enabled Whether the message stage is enabled or disabled.
   */
  void set_enabled(bool is_enabled) { m_is_enabled = is_enabled; }

 protected:
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

 private:
  bool m_is_enabled;
};

/**
 Definitions of structures that store the possible message stages and their
 handlers.
 */
using Gcs_stages_list = std::vector<Stage_code>;
using Gcs_map_type_handler =
    std::map<Stage_code, std::unique_ptr<Gcs_message_stage>>;
using Gcs_map_version_stages = std::map<Gcs_protocol_version, Gcs_stages_list>;
using Gcs_pair_version_stages =
    std::pair<const Gcs_protocol_version, Gcs_stages_list>;

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
  std::atomic<Gcs_protocol_version> m_pipeline_version;

 public:
  explicit Gcs_message_pipeline()
      : m_handlers(),
        m_pipelines(),
        m_pipeline_version(Gcs_protocol_version::HIGHEST_KNOWN) {}

  Gcs_message_pipeline(Gcs_message_pipeline &p) = delete;

  Gcs_message_pipeline &operator=(const Gcs_message_pipeline &p) = delete;

  Gcs_message_pipeline(Gcs_message_pipeline &&p) = delete;

  Gcs_message_pipeline &operator=(Gcs_message_pipeline &&p) = delete;

  virtual ~Gcs_message_pipeline() = default;

  /**
   This member function SHALL be called by the message sender. It makes the
   message go through the pipeline of stages before it is actually handed
   over to the group communication engine.

   Note that the fragmentation layer may produce more than one packet.

   @param[in] msg_data Message data to send.
   @param[in] cargo The cargo type of the message to send
   @retval {true, _} If there was an error in the pipeline
   @retval {false, P} If the pipeline was successful, and produced the
           set of transformed packets P
   */
  std::pair<bool, std::vector<Gcs_packet>> process_outgoing(
      Gcs_message_data const &msg_data, Cargo_type cargo) const;

  /**
   This member function SHALL be called by the receiver thread to process the
   packet through the stages it was processed when it was sent. This reverts
   the effect on the receiving end.

   @param packet The packet to process.
   @retval {ERROR, _} If there was an error in the pipeline
   @retval {OK_NO_PACKET, NP} If the pipeline was successful, but produced no
           packet
   @retval {OK_PACKET, P} If the pipeline was successful, and produced the
           packet P
   */
  std::pair<Gcs_pipeline_incoming_result, Gcs_packet> process_incoming(
      Gcs_packet &&packet) const;

  /**
   Update the list of members in the group as this may be required by some
   stages in the communication pipeline. By default though, the call is simply
   ignored.

   @param me The local member identifier.
   @param xcom_nodes List of members in the group.
   */
  void update_members_information(const Gcs_member_identifier &me,
                                  const Gcs_xcom_nodes &xcom_nodes) const;

  Gcs_xcom_synode_set get_snapshot() const;

  /**
   Register a stage to be used by the pipeline.

   @tparam T Stage class type
   @tparam Args Type of Parameters to the stage constructor
   @param args Parameters to the stage constructor
   */
  template <class T, class... Args>
  void register_stage(Args... args) {
    std::unique_ptr<T> stage(new T(args...));

    if (stage != nullptr) {
      Stage_code code = stage->get_stage_code();
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
  bool contains_stage(Stage_code code) const {
    return retrieve_stage(code) != nullptr;
  }

  /*
   Return a reference to a stage. Note that the stage must exist, otherwise,
   the call will lead to an undefined behavior.

   @param code Stage code
   @return a reference to a stage
   */
  Gcs_message_stage &get_stage(Stage_code code) const {
    Gcs_message_stage *ptr = retrieve_stage(code);
    assert(ptr != nullptr);
    return *ptr;
  }

  /**
   Register the stages per version that form the different pipelines.

   This method must be called after registering all the desired stages using
   register_stage.

   This method must only be called on an unregistered pipeline.
   If you want to reuse the pipeline, new calls to this method must be preceded
   by calls to cleanup and register_stage.

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
  bool contains_pipeline(Gcs_protocol_version pipeline_version) const {
    return retrieve_pipeline(pipeline_version) != nullptr;
  }

  /*
   Return a reference to a pipeline version. Note that the pipeline version
   must exist, otherwise, the call will lead to an undefined behavior.

   @param pipeline_version Pipeline version
   @return a reference to a pipeline
   */
  const Gcs_stages_list &get_pipeline(
      Gcs_protocol_version pipeline_version) const {
    const Gcs_stages_list *ptr = retrieve_pipeline(pipeline_version);
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
  bool set_version(Gcs_protocol_version pipeline_version);

  /**
   Return the pipeline version in use.
   */
  Gcs_protocol_version get_version() const;

 private:
  /**
   Retrieve the stages associated with a pipeline version.

   @param pipeline_version Pipeline version
   */
  const Gcs_stages_list *retrieve_pipeline(
      Gcs_protocol_version pipeline_version) const;

  /**
   This member function SHALL retrieve the associated stage if there is any,
   otherwise a null pointer is returned.

   @param stage_code unique stage code
   */
  Gcs_message_stage *retrieve_stage(Stage_code stage_code) const;

  /**
   This member function SHALL retrieve the current stage type code of a packet.

   @param p the packet to process.
   */
  Gcs_message_stage *retrieve_stage(const Gcs_packet &p) const;

  /**
   Find out which stages should be applied to an outgoing message.

   @param pipeline_version The pipeline version to use
   @param original_payload_size The size of the outgoing message
   @retval {true, _} If there was an error
   @retval {false, S} If successful, and the message should go through the
           sequence of stages S
   */
  std::pair<bool, std::vector<Stage_code>> get_stages_to_apply(
      Gcs_protocol_version const &pipeline_version,
      uint64_t const &original_payload_size) const;

  /**
   Create a packet for a message with size original_payload_size and type
   cargo, that will go through the stages stages_to_apply from pipeline
   version current_version.

   @param cargo The message type
   @param current_version The pipeline version
   @param original_payload_size The payload size
   @param stages_to_apply The stages that will be applied to the packet
   @retval {true, _} If there was an error creating the packet
   @retval {false, P} If successful, and created packet P
   */
  std::pair<bool, Gcs_packet> create_packet(
      Cargo_type const &cargo, Gcs_protocol_version const &current_version,
      uint64_t const &original_payload_size,
      std::vector<Stage_code> const &stages_to_apply) const;

  /**
   Apply the given stages to the given outgoing packet.

   @param packet The packet to transform
   @param stages The stages to apply
   @retval {true, _} If there was an error applying the stages
   @retval {false, P} If the stages were successfully applied, and produced
           the set of transformed packets P
   */
  std::pair<bool, std::vector<Gcs_packet>> apply_stages(
      Gcs_packet &&packet, std::vector<Stage_code> const &stages) const;

  /**
   Apply the given stage to the given outgoing packet.

   @param packets The packet to transform
   @param stage The stage to apply
   @retval {true, _} If there was an error applying the stage
   @retval {false, P} If the stage was successfully applied, and produced the
           set of transformed packets P
   */
  std::pair<bool, std::vector<Gcs_packet>> apply_stage(
      std::vector<Gcs_packet> &&packets, Gcs_message_stage &stage) const;

  /**
   Revert the given stage to the given incoming packet.

   @param packet The packet to transform
   @param stage_code The stage to revert
   @retval {ERROR, _} If there was an error in the stage
   @retval {OK_NO_PACKET, NP} If the stage was successfully reverted, but
           produced no packet
   @retval {OK_PACKET, P} If the stage was successfully reverted, and produced
           the packet P
   */
  std::pair<Gcs_pipeline_incoming_result, Gcs_packet> revert_stage(
      Gcs_packet &&packet, Stage_code const &stage_code) const;
};

#endif
