/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_CERTIFIER
#define GCS_CERTIFIER

#include <map>
#include <string>
#include <list>
#include <vector>

#include "certifier_stats_interface.h"
#include "gcs_communication_interface.h"
#include "gcs_control_interface.h"
#include "member_info.h"
#include "plugin_messages.h"
#include "plugin_utils.h"
#include "pipeline_interfaces.h"

#include <mysql/group_replication_priv.h>

/**
  This class is a core component of the database state machine
  replication protocol. It implements conflict detection based
  on a certification procedure.

  Snapshot Isolation is based on assigning logical timestamp to optimistic
  transactions, i.e. the ones which successfully meet certification and
  are good to commit on all nodes in the group. This timestamp is a
  monotonically increasing counter, and is same across all nodes in the group.

  This timestamp, which in our algorithm is the snapshot version, is further
  used to update the certification info.
  The snapshot version maps the items in a transaction to the GTID_EXECUTED
  that this transaction saw when it was executed, that is, on which version
  the transaction was executed.

  If the incoming transaction snapshot version is a subset of a
  previous certified transaction for the same write set, the current
  transaction was executed on top of outdated data, so it will be
  negatively certified. Otherwise, this transaction is marked
  certified and goes into applier.
*/
typedef std::map<std::string, Gtid_set*> Certification_info;

class Certifier_broadcast_thread
{
public:
  /**
    Certifier_broadcast_thread constructor

    @param comm_intf       Reference to the GCS Communication Interface
    @param ctrl_intf       Reference to the GCS Control Interface
    @param local_node_info Reference to the local node information
   */
  Certifier_broadcast_thread(Gcs_communication_interface* comm_intf,
                             Gcs_control_interface* ctrl_intf,
                             Cluster_member_info* local_node_info);
  virtual ~Certifier_broadcast_thread();

  /**
    Initialize broadcast thread.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize();

  /**
    Terminate broadcast thread.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int terminate();

  /**
    Broadcast thread worker method.
  */
  void dispatcher();

  /**
    Sets the broadcast thread's GCS interfaces

    @param gcs_communication  Reference to the GCS Communication Interface
    @param gcs_control        Reference to the GCS Control Interface
   */
  void set_gcs_communication(Gcs_communication_interface* gcs_communication,
                             Gcs_control_interface* gcs_control)
  {
    this->gcs_communication = gcs_communication;
    this->gcs_control = gcs_control;
  }

  /**
    Sets the broadcast thread's GCS local node info

    @param local_node  Reference to the local node information
   */
  void set_local_node_info(Cluster_member_info* local_node)
  {
    this->local_node = local_node;
  }

private:
  /**
    Period (in microseconds) between stable transactions set
    broadcast.
  */
  static const int BROADCAST_PERIOD= 60000000; // microseconds

  /**
    Thread control.
  */
  bool aborted;
  pthread_t broadcast_pthd;
  pthread_mutex_t broadcast_pthd_lock;
  pthread_cond_t broadcast_pthd_cond;
  bool broadcast_pthd_running;

  //GCS interfaces to the Cluster where one belongs
  Gcs_communication_interface* gcs_communication;
  Gcs_control_interface* gcs_control;

  //Local node information
  Cluster_member_info* local_node;

  /**
    Broadcast local GTID_EXECUTED to group.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int broadcast_gtid_executed();
};


class Certifier_interface : public Certifier_stats
{
public:
  virtual ~Certifier_interface() {}
  virtual void handle_view_change()= 0;
  virtual int handle_certifier_data(uchar *data, uint len)= 0;

  virtual void get_certification_info(std::map<std::string, std::string> *cert_info,
                                      rpl_gno *sequence_number)= 0;
  virtual void set_certification_info(std::map<std::string, std::string> *cert_info,
                                      rpl_gno sequence_number)= 0;

  virtual void set_local_node_info(Cluster_member_info* local_info)= 0;

  virtual void set_gcs_interfaces(Gcs_communication_interface* comm_if,
                                  Gcs_control_interface* ctrl_if)= 0;
};


class Certifier: public Certifier_interface
{
public:
  Certifier();
  virtual ~Certifier();

  /**
    Initialize certifier.

    @param last_delivered_gno the last certified gno

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize(rpl_gno last_delivered_gno);

  /**
    Terminate certifier.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int terminate();

  /**
    Handle view changes on certifier.
   */
  virtual void handle_view_change();

  /**
    Queues the packet coming from the reader for future processing.

    @param[in]  data      the packet data
    @param[in]  len       the packet length

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on queue
  */
  virtual int handle_certifier_data(uchar *data, uint len);

  /**
    This member function SHALL certify the set of items against transactions
    that have already passed the certification test.

    @param snapshot_version   The incoming transaction snapshot version.
    @param write_set          The incoming transaction write set.
    @returns >0               sequence number (positively certified). If
                              generate_group_id is true and certification
                              positive a 1 is returned;
              0               negatively certified;
             -1               error.
   */
  rpl_gno certify(const Gtid_set *snapshot_version,
                  std::list<const char*> *write_set,
                  bool generate_group_id);

  /**
    Returns the transactions in stable set, that is, the set of transactions
    already applied on all group members.

    @returns                 transactions in stable set
   */
  Gtid_set* get_group_stable_transactions_set();

  /**
    Retrieves the current certification info and sequence number.

     @note if concurrent access is introduce to these variables,
     locking is needed in this method

     @param[out] cert_info        a pointer to retrieve the certification info
     @param[out] sequence_number  a pointer to retrieve the sequence number
  */
  virtual void get_certification_info(std::map<std::string, std::string> *cert_info,
                                      rpl_gno *sequence_number);

  /**
    Sets the certification info and sequence number according to the given values.

    @note if concurrent access is introduce to these variables,
    locking is needed in this method

    @param cert_info              certification info retrieved from recovery procedure
    @param sequence_number        sequence number retrieved from recovery procedure
  */
  virtual void set_certification_info(std::map<std::string, std::string> *cert_info,
                                      rpl_gno sequence_number);

  /**
    Get the number of postively certified transactions by the certifier
    */
  ulonglong get_positive_certified();

  /**
    Get method to retrieve the number of negatively certified transactions.
    */
  ulonglong get_negative_certified();

  /**
    Get method to retrieve the certification db size.
    */
  ulonglong get_certification_info_size();

  /**
    Get method to retrieve the last sequence number of the node.
    */
  rpl_gno get_last_sequence_number();

  /**
   All the methods below exist in order to inject dependencies into to the
   Certifier that one cannot do at object construction time
   */
  void set_local_node_info(Cluster_member_info* local_info);

  void set_gcs_interfaces(Gcs_communication_interface* comm_if,
                          Gcs_control_interface* ctrl_if);

private:

  //GCS interfaces to the Cluster where one belongs
  Gcs_communication_interface* gcs_communication;
  Gcs_control_interface* gcs_control;

  //Local node information
  Cluster_member_info* local_node;

  /**
    Is certifier initialized.
  */
  bool initialized;

  bool inline is_initialized()
  {
    return initialized;
  }

  void clear_certification_info();

  /**
    Next sequence number.
  */
  rpl_gno next_seqno;

  /**
    Certification database.
  */
  Certification_info certification_info;
  Sid_map *certification_info_sid_map;

  ulonglong positive_cert;
  ulonglong negative_cert;

  mysql_mutex_t LOCK_certification_info;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key key_LOCK_certification_info;
#endif

  /**
    Stable set and garbage collector variables.
  */
  Sid_map *stable_sid_map;
  Gtid_set *stable_gtid_set;
  Synchronized_queue<Data_packet *> *incoming;

  /**
    Broadcast thread.
  */
  Certifier_broadcast_thread *broadcast_thread;

  /* Methods */
  Certification_info::iterator begin();
  Certification_info::iterator end();

  /**
    Adds an item from transaction writeset to the certification DB.
    @param[in]  item             item in the writeset to be added to the
                                 Certification DB.
    @param[in]  snapshot_version Snapshot version of the incoming transaction
                                 which modified the above mentioned item.
    @return
    @retval     False       successfully added to the map.
                True        otherwise.
  */
  bool add_item(const char* item, const Gtid_set *snapshot_version);

  /**
    Find the snapshot_version corresponding to an item. Return if
    it exists, other wise return NULL;

    @param[in]  item          item for the snapshot version.
    @retval                   Gtid_set pointer if exists in the map.
                              Otherwise 0;
  */
  Gtid_set *get_certified_write_set_snapshot_version(const char* item);

  /**
    This member function shall add transactions to the stable set

    @param gtid     The GTID set of the transactions to be added
                    to the stable set.
    @returns        False if adds successfully,
                    True otherwise.
   */
  bool set_group_stable_transactions_set(Gtid_set* executed_gtid_set);

  /**
    Computes intersection between all sets received, so that we
    have the already applied transactions on all servers.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int stable_set_handle();

  /**
    Removes the intersection of the received transactions stable
    sets from certification database.
   */
  void garbage_collect();

  /**
    Clear incoming queue.
  */
  void clear_incoming();

/*
  Update method to store the count of the positively and negatively
  certified transaction on a particular node.
*/
  void update_certified_transaction_count(bool result);
};

/*
 @class Gtid_Executed_Message

  Class to convey the serialized contents of the previously executed GTIDs
 */
class Gtid_Executed_Message: public Gcs_plugin_message
{
public:
  /**
   Gtid_Executed_Message constructor
   */
  Gtid_Executed_Message();
  virtual ~Gtid_Executed_Message();

  /**
    Appends Gtid executed information in a raw format

   * @param[in] gtid_data encoded GTID data
   * @param[in] len GTID data length
   */
  void append_gtid_executed(uchar* gtid_data, size_t len);

protected:
  /*
   Implementation of the template methods of Gcs_plugin_message
   */
  void encode_message(std::vector<uchar>* buf);
  void decode_message(uchar* buf, size_t len);

private:
  std::vector<uchar> data;
};

#endif /* GCS_CERTIFIER */
