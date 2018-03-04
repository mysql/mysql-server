/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_MSG_FILTER_H
#define GCS_MSG_FILTER_H

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

#include <iterator>
#include <map>
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
 */
class Gcs_message_stage
{
public:
  /**
   The offset of the header length within the stage header.
   */
  static const unsigned short WIRE_HD_LEN_OFFSET;

  /**
   The offset of the stage type code within the stage header.
   */
  static const unsigned short WIRE_HD_TYPE_OFFSET;

  /**
   The on-the-wire field size for the stage type code.
   */
  static const unsigned short WIRE_HD_TYPE_SIZE;

  /**
   The on-the-wire field size for the stage type code.
   */
  static const unsigned short WIRE_HD_LEN_SIZE;

  /**
   The type codes for the existing stages.

   NOTE: values from this enum must fit into WIRE_HD_TYPE_SIZE bytes storage.
   */
  enum enum_type_code
  {
    ST_UNKNOWN= 0,
    ST_LZ4= 1,

    /**
     No type codes can show after this one. If a type code is to be added,
     this needs to be incremented and the lowest type code available be
     assigned to the new stage.
     */
    ST_MAX_STAGES= 2
  };

  virtual ~Gcs_message_stage() { }

  /**
   Returns the unique type code of this filter.
   @return the type code of this stage.
   */
  virtual enum_type_code type_code()= 0;

  /**
   Applies this stage transformation to the outgoing message.

   @param p the packet to which the transformation should be applied.
   @return false on success, true otherwise.
   */
  virtual bool apply(Gcs_packet &p)= 0;

  /**
   Reverts the stage transformation on the incoming message.

   @param p the packat to which the transformation should be applied.
   @return false on success, true otherwise.
   */
  virtual bool revert(Gcs_packet &p)= 0;
};

/**
 This is the pipeline that an outgoing or incoming message has to go through
 when being sent to or received from the group respectively.

 The pipeline has stages registered and these are assembled in an outgoing
 pipeline. Then outgoing messages always have to traverse this pipeline.

 For incoming messages, the pipeline is built on the fly, according to the
 information contained in the message stage headers.
 */
class Gcs_message_pipeline
{
private:

  /**
   The registered stages. These are all stages that are known by this version
   of MySQL GCS. This needs to contain an instance of all possible stages,
   since it needs to handle cross-version message exchanges.
   */
  std::map<Gcs_message_stage::enum_type_code, Gcs_message_stage *> m_stage_reg;

  /**
   This is the pre-assembled outgoing pipeline. The vector is traversed
   in the given order and the stages with the given typecodes are applied to
   outgoing messages.
   */
  std::vector<Gcs_message_stage::enum_type_code> m_pipeline;

public:

  explicit Gcs_message_pipeline(): m_stage_reg(), m_pipeline() { }
  virtual ~Gcs_message_pipeline();

  /**
    This member function SHALL be called by the message sender. It makes the
    message go through the pipeline of stages before it is actually handed
    over to the group communication engine.

    @param p the Packet to send.
    @return false on success, true otherwise.
   */
  bool outgoing(Gcs_packet &p);

  /**
    This member function SHALL be called by the receiver thread to process the
    message through the stages it was processed when it was sent. This reverts
    the effect on the receiving end.

    @param p the packet to process.
    @return false on sucess, true otherwise.
   */
  bool incoming(Gcs_packet &p);

  /*
   This member function SHALL register stages on the pipeline. It must be
   called before the pipeline is used.

   @param s the instance of the given stage that one needs to register.
   */
  void register_stage(Gcs_message_stage *s);

  /*
   This member function SHALL configure the outgoing pipeline as specified
   in the parameter. Stages MUST have been registered before the pipeline is
   used.

   @param stages the list of stages that build up the pipeline.
   */
  void configure_outgoing_pipeline(std::vector<Gcs_message_stage::enum_type_code> stages)
  {
    // clean up the current setup
    m_pipeline.clear();

    // create the new one.
    std::copy(stages.begin(), stages.end(), std::back_inserter(m_pipeline));
  }

private:
  // make copy and assignment constructors private
  Gcs_message_pipeline(Gcs_message_pipeline &p);
  Gcs_message_pipeline& operator=(const Gcs_message_pipeline& p);
};
#endif
