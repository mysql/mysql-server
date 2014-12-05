/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef GCS_STATE_EXCHANGE_INCLUDED
#define	GCS_STATE_EXCHANGE_INCLUDED

#include <map>
#include <set>
#include <vector>
#include <stdio.h>

#include "gcs_corosync_utils.h"
#include "gcs_message.h"
#include "gcs_communication_interface.h"
#include "gcs_corosync_view_identifier.h"
#include "gcs_view.h"

#include <corosync/cpg.h>
#include <corosync/corotypes.h>

#define VARIABLE_VIEW_ID_LENGTH 8
#define VIEW_ID_LENGTH 4
#define STATE_EXCHANGE_HEADER_CODE_LENGTH 4

using std::map;
using std::vector;
using std::set;

/**
  @class Member_state

  Class that conveys the state to be exchanged between members, which is not
  provided by Corosync
 */
class Member_state
{
public:
  /**
    Member_state constructor

    @param[in] view_id_arg the view identifier from the node
    @param[in] exchangeable_data the generic data to be exchanged
   */
  Member_state(Gcs_corosync_view_identifier* view_id_arg,
               vector<uchar> *exchangeable_data);

  /**
    Member_state raw constructor

    @param[in] data the raw date
    @param[in] len the length of the raw data
   */
  Member_state(const uchar* data, size_t len);

  /**
    Member state destructor
   */
  ~Member_state();

  /**
    Encodes Member State to be sent through the network

    @param[out] buffer the buffer to write data into
   */
  void encode(vector<uchar>* buffer);

  /**
    @return the view identifier
   */
  Gcs_corosync_view_identifier* get_view_id()
  {
    return view_id;
  }

  /**
    @return the generic exchangeable data
   */
  vector<uchar>* get_data()
  {
    return data;
  }

private:
  Gcs_corosync_view_identifier* view_id;
  vector<uchar>* data;
};

/**
  @interface gcs_corosync_state_exchange_interface

  Interface that defines the operations that State Exchange will provide
 */
class Gcs_corosync_state_exchange_interface
{
public:
  virtual ~Gcs_corosync_state_exchange_interface(){};

  /**
    Accomplishes all necessary initialization steps
   */
  virtual void init()= 0;

  /**
    Resets the internal structures needed between State Exchanges
   */
  virtual void reset()= 0;

  /**
    Signals the module to start a State Exchange

    @param[in] total          corosync total members in the new view
    @param[in] total_entries  corosync total number of members in the new
                              view
    @param[in] left           corosync members that left in the new view
    @param[in] left_entries   corosync number of members that left in the
                              new view
    @param[in] joined raw     corosync members that joined in the new view
    @param[in] joined_entries corosync number of members that joined in the
                              new view
    @param[in] group          group name
    @param[in] data generic   exchanged data
    @param[in] current_view   the currently installed view
    @param[in] local_info     the local GCS member identifier

    @return true if the member is leaving
   */
  virtual bool state_exchange(const cpg_address *total,
                              size_t total_entries,
                              const struct cpg_address *left,
                              size_t left_entries,
                              const struct cpg_address *joined,
                              size_t joined_entries,
                              string* group,
                              vector<uchar> *data,
                              Gcs_view* current_view,
                              Gcs_member_identifier* local_info)= 0;

  /**
    Processes a member state message on an ongoing State Exchange round

    @param[in] ms_info received Member State
    @param[in] p_id the node that the Member State pertains

    @return true if State Exchanged is to be finished and the view can be
                 installed
   */
  virtual bool process_member_state(Member_state *ms_info,
                                    Gcs_member_identifier p_id)= 0;

  /**
    Validates if a given message contains a Member State
    @param[in] to_verify the Gcs_message to verify

    @return true if it contains a Member State
   */
  virtual bool is_state_exchange_message(Gcs_message *to_verify)= 0;

  /**
    Retrieves the new view identifier after a State Exchange

    @return the new view identifier
   */
  virtual Gcs_corosync_view_identifier* get_new_view_id()= 0;

  /**
    @return the members that joined in this State Exchange round
   */
  virtual set<Gcs_member_identifier*>* get_joined()= 0;

  /**
    @return the members that left in this State Exchange round
   */
  virtual set<Gcs_member_identifier*>* get_left()= 0;

  /**
    @return All the members in this State Exchange round
  */
  virtual set<Gcs_member_identifier*>* get_total()= 0;

  /**
    @return the group in which this State Exchange is occurring
   */
  virtual string* get_group()= 0;
};

/*
  @class gcs_corosync_state_exchange

  Implementation of the gcs_corosync_state_exchange_interface
 */
class Gcs_corosync_state_exchange: public Gcs_corosync_state_exchange_interface
{
public:
  static const int state_exchange_header_code;

public:
  /**
    State Exchange constructor

    @param[in] comm Communication interface reference to allow broadcasting of
                    member states
   */
  Gcs_corosync_state_exchange(Gcs_communication_interface* comm);

  virtual ~Gcs_corosync_state_exchange();

  //Implementation of gcs_corosync_state_exchange_interface
  void init();

  void reset();

  bool state_exchange(const cpg_address *total,
                      size_t total_entries,
                      const struct cpg_address *left,
                      size_t left_entries,
                      const struct cpg_address *joined,
                      size_t joined_entries,
                      string* group,
                      vector<uchar> *data,
                      Gcs_view* current_view,
                      Gcs_member_identifier* local_info);

  bool process_member_state(Member_state *ms_info,
                            Gcs_member_identifier p_id);

  bool is_state_exchange_message(Gcs_message *to_verify);

  set<Gcs_member_identifier*>* get_all_vc_members()
  {
    return &ms_total;
  }

  Gcs_corosync_view_identifier* get_new_view_id()
  {
    return max_view_id;
  }

  map<Gcs_member_identifier, Member_state*>* get_all_member_states()
  {
    return &member_states;
  }

  set<Gcs_member_identifier*>* get_joined()
  {
    return &ms_joined;
  }

  set<Gcs_member_identifier*>* get_left()
  {
    return &ms_left;
  }

  set<Gcs_member_identifier*>* get_total()
  {
    return &ms_total;
  }

  string* get_group()
  {
    return group_name;
  }


private:
  /**
    Computes if the local member is leaving

    @return true in case of the local member is leaving
   */
  bool is_leaving();

  /**
   Broadcasts the local state to all nodes in the Cluster
   */
  void broadcast_state();

  /**
    Updates the structure that waits for State Exchanges
   */
  void update_awaited_vector();

  /**
    Converts Corosync data to a set of internal representation

    @param[in] list Corosync list
    @param[in] num  Corosync list size
    @param[in] pset Set where the converted member ids will be written
   */
  void fill_member_set(const struct cpg_address *list,
                       size_t num,
                       set<Gcs_member_identifier*>& pset);

  Gcs_communication_interface *broadcaster;

  map<Gcs_member_identifier, uint> awaited_vector;

  /* view_id corresponding the last membership */
  Gcs_corosync_view_identifier* last_view_id;

  /*
   Set of id:s in GCS native format as reported by View-change handler.
   */
  set<Gcs_member_identifier*> ms_total, ms_left, ms_joined;
  /*
    Collection of State Message contents to facilitate view installation.
  */
  map<Gcs_member_identifier, Member_state*> member_states;

  /* View installation related: maximum view id out of State messages */
  Gcs_corosync_view_identifier* max_view_id;

  //data to be exchanged
  vector<uchar> *exchangeable_data;

  //Group name to exchange state
  string* group_name;

  //Local GCS member identification
  Gcs_member_identifier* local_information;
};

#endif  /* GCS_COROSYNC_STATE_EXCHANGE_INCLUDED */


