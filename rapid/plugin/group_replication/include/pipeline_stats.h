/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PIPELINE_STATS_INCLUDED
#define PIPELINE_STATS_INCLUDED

#include <map>
#include <string>
#include <vector>

#include "gcs_plugin_messages.h"
#include "plugin_psi.h"


/**
  Flow control modes:
    FCM_DISABLED  flow control disabled
    FCM_QUOTA introduces a delay only on transactions the exceed a quota
*/
enum Flow_control_mode
{
  FCM_DISABLED= 0,
  FCM_QUOTA
};
extern ulong flow_control_mode_var;


/**
  Flow control queue threshold for certifier and for applier.
*/
extern int flow_control_certifier_threshold_var;
extern int flow_control_applier_threshold_var;


/**
  @class Pipeline_stats_member_message

  Describes all statistics sent by members.
*/
class Pipeline_stats_member_message : public Plugin_gcs_message
{
public:
  enum enum_payload_item_type
  {
    // This type should not be used anywhere.
    PIT_UNKNOWN= 0,

    // Length of the payload item: 4 bytes
    PIT_TRANSACTIONS_WAITING_CERTIFICATION= 1,

    // Length of the payload item: 4 bytes
    PIT_TRANSACTIONS_WAITING_APPLY= 2,

    // Length of the payload item: 8 bytes
    PIT_TRANSACTIONS_CERTIFIED= 3,

    // Length of the payload item: 8 bytes
    PIT_TRANSACTIONS_APPLIED= 4,

    // Length of the payload item: 8 bytes
    PIT_TRANSACTIONS_LOCAL= 5,

    // No valid type codes can appear after this one.
    PIT_MAX= 6
  };

  /**
    Message constructor

    @param[in] transactions_waiting_certification
    @param[in] transactions_waiting_apply
    @param[in] transactions_certified
    @param[in] transactions_applied
    @param[in] transactions_local
  */
  Pipeline_stats_member_message(int32 transactions_waiting_certification,
                                int32 transactions_waiting_apply,
                                int64 transactions_certified,
                                int64 transactions_applied,
                                int64 transactions_local);

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Pipeline_stats_member_message(const unsigned char *buf, uint64 len);

  /**
    Message destructor
   */
  virtual ~Pipeline_stats_member_message();

  /**
    Get transactions waiting certification counter value.

    @return the counter value
  */
  int32 get_transactions_waiting_certification();

  /**
    Get transactions waiting apply counter value.

    @return the counter value
  */
  int32 get_transactions_waiting_apply();

  /**
    Get transactions certified.

    @return the counter value
  */
  int64 get_transactions_certified();

  /**
    Get transactions applied.

    @return the counter value
  */
  int64 get_transactions_applied();

  /**
    Get local transactions that member tried to commmit.

    @return the counter value
  */
  int64 get_transactions_local();

protected:
  /**
    Encodes the message contents for transmission.

    @param[out] buffer   the message buffer to be written
  */
  void encode_payload(std::vector<unsigned char> *buffer) const;

  /**
    Message decoding method

    @param[in] buffer the received data
    @param[in] end    the end of the buffer
  */
  void decode_payload(const unsigned char *buffer, const unsigned char* end);

private:
  int32 m_transactions_waiting_certification;
  int32 m_transactions_waiting_apply;
  int64 m_transactions_certified;
  int64 m_transactions_applied;
  int64 m_transactions_local;
};


/**
  @class Pipeline_stats_member_collector

  The pipeline collector for the local member stats.
*/
class Pipeline_stats_member_collector
{
public:
  /**
    Default constructor.
  */
  Pipeline_stats_member_collector();

  /**
    Destructor.
  */
  virtual ~Pipeline_stats_member_collector();

  /**
    Increment transactions waiting apply counter value.
  */
  void increment_transactions_waiting_apply();

  /**
    Decrement transactions waiting apply counter value.
  */
  void decrement_transactions_waiting_apply();

  /**
    Increment transactions certified counter value.
  */
  void increment_transactions_certified();

  /**
    Increment transactions applied counter value.
  */
  void increment_transactions_applied();

  /**
    Increment local transactions counter value.
  */
  void increment_transactions_local();

  /**
    Send member statistics to group.
  */
  void send_stats_member_message();

private:
  int32 m_transactions_waiting_apply;
  int64 m_transactions_certified;
  int64 m_transactions_applied;
  int64 m_transactions_local;
  mysql_mutex_t m_transactions_waiting_apply_lock;
};


/**
  @class Pipeline_member_stats

  Computed statistics per member.
*/
class Pipeline_member_stats
{
public:
  /**
    Default constructor.
  */
  Pipeline_member_stats();

  /**
    Constructor.
  */
  Pipeline_member_stats(Pipeline_stats_member_message &msg);

  /**
    Destructor.
  */
  virtual ~Pipeline_member_stats();

  /**
    Updates member statistics with a new message from the network
  */
  void update_member_stats(Pipeline_stats_member_message &msg,
                           uint64 stamp);

  /**
    Returns true if the node is behind on some user-defined criteria
  */
  bool is_flow_control_needed();

  /**
    Get transactions waiting certification counter value.

    @return the counter value
  */
  int32 get_transactions_waiting_certification();

  /**
    Get transactions waiting apply counter value.

    @return the counter value
  */
  int32 get_transactions_waiting_apply();

  /**
    Get transactions certified since last stats message.

    @return the counter value
  */
  int64 get_delta_transactions_certified();

  /**
    Get transactions applied since last stats message.

    @return the counter value
  */
  int64 get_delta_transactions_applied();

  /**
    Get local transactions that member tried to commmit
    since last stats message.

    @return the counter value
  */
  int64 get_delta_transactions_local();

  /**
    Get the last stats update stamp.

    @return the counter value
  */
  uint64 get_stamp();

#ifndef DBUG_OFF
  void debug(const char *member, int64 quota_size, int64 quota_used);
#endif

private:
  int32 m_transactions_waiting_certification;
  int32 m_transactions_waiting_apply;
  int64 m_transactions_certified;
  int64 m_delta_transactions_certified;
  int64 m_transactions_applied;
  int64 m_delta_transactions_applied;
  int64 m_transactions_local;
  int64 m_delta_transactions_local;
  uint64 m_stamp;
};


/**
  Data type that holds all members stats.
  The key value is the GCS member_id.
*/
typedef std::map<std::string, Pipeline_member_stats>
    Flow_control_module_info;

/**
  @class Flow_control_module

  The pipeline stats aggregator of all group members stats and
  flow control module.
*/
class Flow_control_module
{
public:
  static const int64 MAXTPS;
  static const double HOLD_FACTOR;
  static const double RELEASE_FACTOR;

  /**
    Default constructor.
  */
  Flow_control_module();

  /**
    Destructor.
  */
  virtual ~Flow_control_module();

  /**
    Handles a Pipeline_stats_message, updating the
    Flow_control_module_info and the delay, if needed.

    @param[in] data      the packet data
    @param[in] len       the packet length
    @param[in] member_id the GCS member_id which sent the message

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on queue
  */
  int handle_stats_data(const uchar *data, uint64 len,
                        const std::string& member_id);

  /**
    Evaluate the information received in the last flow control period
    and adjust the system parameters accordingly
  */
  void flow_control_step();

  /**
    Compute and wait the amount of time in microseconds that must
    be elapsed before a new message is sent.
    If there is no need to wait, the method returns immediately.

    @return the wait time
      @retval 0      No wait was done
      @retval >0     The wait time
  */
  int32 do_wait();

private:
  mysql_mutex_t m_flow_control_lock;
  mysql_cond_t  m_flow_control_cond;

  Flow_control_module_info m_info;

  /*
    Number of members that did have waiting transactions on
    certification and/or apply.
  */
  int32 m_holds_in_period;

  /*
   FCM_QUOTA
  */
  int64 m_quota_used;
  int64 m_quota_size;

  /*
    Counter incremented on every flow control step.
  */
  uint64 m_stamp;
};

#endif /* PIPELINE_STATS_INCLUDED */
