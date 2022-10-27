/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#ifndef CERTIFIER_INCLUDE
#define CERTIFIER_INCLUDE

#include <assert.h>
#include <mysql/group_replication_priv.h>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/certifier_stats_interface.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/pipeline_interfaces.h"
#include "plugin/group_replication/include/plugin_utils.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_interface.h"

/**
  This class extends Gtid_set to include a reference counter.

  It is for Certifier only, so it is single-threaded and no locks
  are needed since Certifier already ensures sequential use.

  It is to be used to share by multiple entries in the
  certification info and released when the last reference to it
  needs to be freed.
*/
class Gtid_set_ref : public Gtid_set {
 public:
  Gtid_set_ref(Sid_map *sid_map, int64 parallel_applier_sequence_number)
      : Gtid_set(sid_map),
        reference_counter(0),
        parallel_applier_sequence_number(parallel_applier_sequence_number) {}

  virtual ~Gtid_set_ref() = default;

  /**
    Increment the number of references by one.

    @return the number of references
  */
  size_t link() { return ++reference_counter; }

  /**
    Decrement the number of references by one.

    @return the number of references
  */
  size_t unlink() {
    assert(reference_counter > 0);
    return --reference_counter;
  }

  int64 get_parallel_applier_sequence_number() const {
    return parallel_applier_sequence_number;
  }

 private:
  size_t reference_counter;
  int64 parallel_applier_sequence_number;
};

/**
  This class is a core component of the database state machine
  replication protocol. It implements conflict detection based
  on a certification procedure.

  Snapshot Isolation is based on assigning logical timestamp to optimistic
  transactions, i.e. the ones which successfully meet certification and
  are good to commit on all members in the group. This timestamp is a
  monotonically increasing counter, and is same across all members in the group.

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
class Certifier_broadcast_thread {
 public:
  /**
    Certifier_broadcast_thread constructor
  */
  Certifier_broadcast_thread();
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
    Period (in seconds) between stable transactions set
    broadcast.
  */
  static const int BROADCAST_GTID_EXECUTED_PERIOD = 60;  // seconds

 private:
  /**
    Thread control.
  */
  bool aborted;
  THD *broadcast_thd;
  my_thread_handle broadcast_pthd;
  mysql_mutex_t broadcast_run_lock;
  mysql_cond_t broadcast_run_cond;
  mysql_mutex_t broadcast_dispatcher_lock;
  mysql_cond_t broadcast_dispatcher_cond;
  thread_state broadcast_thd_state;
  size_t broadcast_counter;
  int broadcast_gtid_executed_period;

  /**
    Broadcast local GTID_EXECUTED to group.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int broadcast_gtid_executed();
};

class Certifier_interface : public Certifier_stats {
 public:
  ~Certifier_interface() override = default;
  virtual void handle_view_change() = 0;
  virtual int handle_certifier_data(
      const uchar *data, ulong len,
      const Gcs_member_identifier &gcs_member_id) = 0;

  virtual void get_certification_info(
      std::map<std::string, std::string> *cert_info) = 0;
  virtual int set_certification_info(
      std::map<std::string, std::string> *cert_info) = 0;
  virtual int stable_set_handle() = 0;
  virtual bool set_group_stable_transactions_set(
      Gtid_set *executed_gtid_set) = 0;
  virtual void enable_conflict_detection() = 0;
  virtual void disable_conflict_detection() = 0;
  virtual bool is_conflict_detection_enable() = 0;
  virtual ulonglong get_certification_info_size() override = 0;
};

class Certifier : public Certifier_interface {
 public:
  typedef std::unordered_map<
      std::string, Gtid_set_ref *, std::hash<std::string>,
      std::equal_to<std::string>,
      Malloc_allocator<std::pair<const std::string, Gtid_set_ref *>>>
      Certification_info;

  Certifier();
  ~Certifier() override;

  /**
    Key used to store errors in the certification info
    on View_change_log_event.
  */
  static const std::string CERTIFICATION_INFO_ERROR_NAME;

  /**
    Initialize certifier.

    @param gtid_assignment_block_size the group gtid assignment block size

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize(ulonglong gtid_assignment_block_size);

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
  void handle_view_change() override;

  /**
    Queues the packet coming from the reader for future processing.

    @param[in] data          the packet data
    @param[in] len           the packet length
    @param[in] gcs_member_id the member_id which sent the message

    @return the operation status
      @retval 0      OK
      @retval !=0    Error on queue
  */
  int handle_certifier_data(
      const uchar *data, ulong len,
      const Gcs_member_identifier &gcs_member_id) override;

  /**
    This member function SHALL certify the set of items against transactions
    that have already passed the certification test.

    @param snapshot_version   The incoming transaction snapshot version.
    @param write_set          The incoming transaction write set.
    @param generate_group_id  Flag that indicates if transaction group id
                              must be generated.
    @param member_uuid        The UUID of the member from which this
                              transaction originates.
    @param gle                The incoming transaction global identifier
                              event.
    @param local_transaction  True if this transaction did originate from
                              this member, false otherwise.

    @retval >0                transaction identifier (positively certified).
                              If generate_group_id is false and certification
                              positive a 1 is returned;
    @retval  0                negatively certified;
    @retval -1                error.
   */
  rpl_gno certify(Gtid_set *snapshot_version,
                  std::list<const char *> *write_set, bool generate_group_id,
                  const char *member_uuid, Gtid_log_event *gle,
                  bool local_transaction);

  /**
    Returns the transactions in stable set in text format, that is, the set of
    transactions already applied on all group members.

    @param[out] buffer  Pointer to pointer to string. The method will set it to
                        point to the newly allocated buffer, or NULL on out of
                        memory.
                        Caller must free the allocated memory.
    @param[out] length  Length of the generated string.

    @return the operation status
      @retval 0         OK
      @retval !=0       Out of memory error
   */
  int get_group_stable_transactions_set_string(char **buffer,
                                               size_t *length) override;

  /**
    Retrieves the current certification info.

     @note if concurrent access is introduce to these variables,
     locking is needed in this method

     @param[out] cert_info        a pointer to retrieve the certification info
  */
  void get_certification_info(
      std::map<std::string, std::string> *cert_info) override;

  /**
    Sets the certification info according to the given value.

    @note if concurrent access is introduce to these variables,
    locking is needed in this method

    @param[in] cert_info  certification info retrieved from recovery procedure

    @retval  > 0  Error during setting certification info.
    @retval  = 0  Everything went fine.
  */
  int set_certification_info(
      std::map<std::string, std::string> *cert_info) override;

  /**
    Get the number of postively certified transactions by the certifier
    */
  ulonglong get_positive_certified() override;

  /**
    Get method to retrieve the number of negatively certified transactions.
    */
  ulonglong get_negative_certified() override;

  /**
    Get method to retrieve the certification db size.
    */
  ulonglong get_certification_info_size() override;

  /**
    Get method to retrieve the last conflict free transaction.

    @param[out] value The last conflict free transaction
    */
  void get_last_conflict_free_transaction(std::string *value) override;

  /**
    Generate group GTID for a view change log event.

    @retval  >0         view change GTID
    @retval  otherwise  Error on GTID generation
  */
  Gtid generate_view_change_group_gtid();

  /**
    Public method to add the given gno value to the group_gtid_executed set
    which is used to support skip gtid functionality.

    @param[in] gno  The gno of the transaction which will be added to the
                    group_gtid executed GTID set. The sidno used for this
    transaction will be the group_sidno. The gno here belongs specifically to
    the group UUID.

    @retval  1  error during addition.
    @retval  0  success.
  */
  int add_group_gtid_to_group_gtid_executed(rpl_gno gno);

  /**
    Public method to add the given GTID value in the group_gtid_executed set
    which is used to support skip gtid functionality.

    @param[in] gle  The gtid value that needs to the added in the
                    group_gtid_executed GTID set.

    @retval  1  error during addition.
    @retval  0  success.
  */
  int add_specified_gtid_to_group_gtid_executed(Gtid_log_event *gle);

  /**
    Computes intersection between all sets received, so that we
    have the already applied transactions on all servers.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int stable_set_handle() override;

  /**
    This member function shall add transactions to the stable set

    @param executed_gtid_set  The GTID set of the transactions to be added
                              to the stable set.

    @note when set, the stable set will cause the garbage collection
          process to be invoked

    @retval False  if adds successfully,
    @retval True   otherwise.
   */
  bool set_group_stable_transactions_set(Gtid_set *executed_gtid_set) override;

  /**
    Enables conflict detection.
  */
  void enable_conflict_detection() override;

  /**
    Disables conflict detection.
  */
  void disable_conflict_detection() override;

  /**
    Check if conflict detection is enable.

    @retval True   conflict detection is enable
    @retval False  otherwise
  */
  bool is_conflict_detection_enable() override;

 private:
  /**
   Key used to store group_gtid_executed on certification
   info on View_change_log_event.
  */
  static const std::string GTID_EXTRACTED_NAME;

  /**
    Is certifier initialized.
  */
  bool initialized;

  /**
    Variable to store the sidno used for transactions which will be logged
    with the group_uuid.
  */
  rpl_sidno group_gtid_sid_map_group_sidno;

  /**
    The sidno used for view log events as seen by the group sid map
    */
  rpl_sidno views_sidno_group_representation;
  /**
    The sidno used for view log events as seen by the server sid map
    */
  rpl_sidno views_sidno_server_representation;

  /**
    Method to initialize the group_gtid_executed gtid set with the server gtid
    executed set and applier retrieved gtid set values.

    @param get_server_gtid_retrieved  add applier retrieved gtid set to
                                      group_gtid_executed gtid set

    @retval 1  error during initialization
    @retval 0  success

  */
  int initialize_server_gtid_set(bool get_server_gtid_retrieved = false);

  /**
    This function computes the available GTID intervals from group
    UUID and stores them on group_available_gtid_intervals.
  */
  void compute_group_available_gtid_intervals();

  /**
    This function reserves a block of GTIDs from the
    group_available_gtid_intervals list.

    @retval Gtid_set::Interval which is the interval os GTIDs attributed
  */
  Gtid_set::Interval reserve_gtid_block(longlong block_size);

  /**
    This function updates parallel applier indexes.
    It must be called for each remote transaction.

    @param[in] update_parallel_applier_last_committed_global
                      If true parallel_applier_last_committed_global
                      is updated to the current sequence number
                      (before update sequence number).

    Note: parallel_applier_last_committed_global should be updated
          on the following situations:
          1) Transaction without write set is certified, since it
             represents the lowest last_committed for all future
             transactions;
          2) After certification info garbage collection, since we
             do not know what write sets were purged, which may cause
             transactions last committed to be incorrectly computed.
  */
  void increment_parallel_applier_sequence_number(
      bool update_parallel_applier_last_committed_global);

  /**
    Internal method to add the given gtid gno in the group_gtid_executed set.
    This will be used in the skip gtid implementation.

    @note this will update the last know local transaction GTID.

    @param[in] sidno  rpl_sidno part of the executing gtid of the ongoing
                      transaction.

    @param[in] gno  rpl_gno part of the executing gtid of the ongoing
                    transaction.
  */
  void add_to_group_gtid_executed_internal(rpl_sidno sidno, rpl_gno gno);

  /**
    This method is used to get the next valid GNO for the given sidno,
    for the transaction that is being executed. It checks the already
    used up GNOs and based on that chooses the next possible value.
    This method will consult group_available_gtid_intervals to
    assign GTIDs in blocks according to gtid_assignment_block_size
    when `sidno` is the group sidno.

    @param member_uuid        The UUID of the member from which this
                              transaction originates. It will be NULL
                              on View_change_log_event.
    @param sidno              The sidno that will be used on GTID

    @retval >0                The GNO to be used.
    @retval -1                Error: GNOs exhausted for group UUID.
  */
  rpl_gno get_next_available_gtid(const char *member_uuid, rpl_sidno sidno);

  /**
    This method is used to get the next valid GNO for the
    transaction that is being executed. It checks the already used
    up GNOs and based on that chooses the next possible value.
    This method will consult group_available_gtid_intervals to
    assign GTIDs in blocks according to gtid_assignment_block_size.

    @param member_uuid        The UUID of the member from which this
                              transaction originates. It will be NULL
                              on View_change_log_event.

    @retval >0                The GNO to be used.
    @retval -1                Error: GNOs exhausted for group UUID.
  */
  rpl_gno get_group_next_available_gtid(const char *member_uuid);

  /**
    Generate the candidate GNO for the current transaction.
    The candidate will be on the interval [start, end] or a error
    be returned.
    This method will consult group_gtid_executed to avoid generate
    the same value twice.

    @param sidno              The sidno that will be used to retrieve GNO
    @param start              The first possible value for the GNO
    @param end                The last possible value for the GNO

    @retval >0                The GNO to be used.
    @retval -1                Error: GNOs exhausted for group UUID.
    @retval -2                Error: generated GNO is bigger than end.
  */
  rpl_gno get_next_available_gtid_candidate(rpl_sidno sidno, rpl_gno start,
                                            rpl_gno end) const;

  bool inline is_initialized() { return initialized; }

  void clear_certification_info();

  /**
    Method to clear the members.
  */
  void clear_members();

  /**
    Last conflict free transaction identification.
  */
  Gtid last_conflict_free_transaction;

  /**
    Certification database.
  */
  Certification_info certification_info;
  Sid_map *certification_info_sid_map;

  ulonglong positive_cert;
  ulonglong negative_cert;
  int64 parallel_applier_last_committed_global;
  int64 parallel_applier_sequence_number;

#if !defined(NDEBUG)
  bool certifier_garbage_collection_block;
  bool same_member_message_discarded;
#endif

  mysql_mutex_t LOCK_certification_info;

  /**
    Stable set and garbage collector variables.
  */
  Checkable_rwlock *stable_gtid_set_lock;
  Sid_map *stable_sid_map;
  Gtid_set *stable_gtid_set;
  Synchronized_queue<Data_packet *> *incoming;

  std::vector<std::string> members;

  /*
    Flag to indicate that certifier is handling already applied
    transactions during distributed recovery procedure.

    On donor we may have local transactions certified after
    View_change_log_event (VCLE) logged into binary log before VCLE.
    That is, these local transactions will be appear on recovery
    and also on GCS messages. One can see on example scenario below:

     GCS order | donor binary log order | joiner apply order
    -----------+------------------------+--------------------
        T1     |          T1            |       T1
        T2     |          T2            |       T2
        V1     |          T3            |       T3 (recovery)
        T3     |          V1            |       V1
               |                        |       T3 (GCS)
    -----------+------------------------+--------------------

    T3 is delivered to donor by both recovery and GCS, so joiner needs
    to ensure that T3 has the same global identifier on both cases, so
    that it is correctly skipped on the second time it is applied.

    We ensure that T3 (and other transactions on that situation) have
    the same global identifiers on joiner by:
      1) When the VCLE is applied, we set on joiner certification info
         the same exact certification that was on donor, including the
         set of certified transactions before the joiner joined:
         group_gtid_extracted.
      2) We compare group_gtid_extracted and group_gtid_executed:
         If group_gtid_extracted is a non equal subset of
         group_gtid_executed, it means that we are on the above
         scenario, that is, when applying the last transaction from
         the distributed recovery process we have more transactions
         than the ones certified before the view on which joiner joined.
         So until group_gtid_extracted is a non equal subset of
         group_gtid_executed certifier will generate transactions ids
         following group_gtid_extracted so that we have the same exact
         ids that donor has.
      3) When joiner group_gtid_extracted and group_gtid_executed are
         equal, joiner switches to its regular ids generation mode,
         generating ids from group_gtid_executed.
  */
  bool certifying_already_applied_transactions;

  /*
    Sid map to store the GTIDs that are executed in the group.
  */
  Sid_map *group_gtid_sid_map;

  /*
    A Gtid_set containing the already executed for the group.
    This is used to support skip_gtid.
  */
  Gtid_set *group_gtid_executed;

  /**
    A Gtid_set which contains the gtid extracted from the certification info
    map of the donor. It is the set of transactions that is executed at the
    time of View_change_log_event at donor.
  */
  Gtid_set *group_gtid_extracted;

  /**
    The group GTID assignment block size.
  */
  uint64 gtid_assignment_block_size;

  /**
    List of free GTID intervals in group
  */
  std::list<Gtid_set::Interval> group_available_gtid_intervals;

  /**
    Extends the above to allow GTIDs to be assigned in blocks per member.
  */
  std::map<std::string, Gtid_set::Interval> member_gtids;
  ulonglong gtids_assigned_in_blocks_counter;

  /**
    Conflict detection is performed when:
     1) group is on multi-master mode;
     2) group is on single-primary mode and primary is applying
        relay logs with transactions from a previous primary.
  */
  bool conflict_detection_enable;

  mysql_mutex_t LOCK_members;

  /**
    Broadcast thread.
  */
  Certifier_broadcast_thread *broadcast_thread;

  /**
    Adds an item from transaction writeset to the certification DB.
    @param[in]  item             item in the writeset to be added to the
                                 Certification DB.
    @param[in]  snapshot_version Snapshot version of the incoming transaction
                                 which modified the above mentioned item.
    @param[out] item_previous_sequence_number
                                 The previous parallel applier sequence number
                                 for this item.

    @retval     False       successfully added to the map.
                True        otherwise.
  */
  bool add_item(const char *item, Gtid_set_ref *snapshot_version,
                int64 *item_previous_sequence_number);

  /**
    Find the snapshot_version corresponding to an item. Return if
    it exists, other wise return NULL;

    @param[in]  item          item for the snapshot version.
    @retval                   Gtid_set pointer if exists in the map.
                              Otherwise 0;
  */
  Gtid_set *get_certified_write_set_snapshot_version(const char *item);

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
    certified transaction on a particular group member.
  */
  void update_certified_transaction_count(bool result, bool local_transaction);
};

/*
 @class Gtid_Executed_Message

  Class to convey the serialized contents of the previously executed GTIDs
 */
class Gtid_Executed_Message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: variable
    PIT_GTID_EXECUTED = 1,

    // No valid type codes can appear after this one.
    PIT_MAX = 2
  };

  /**
   Gtid_Executed_Message constructor
   */
  Gtid_Executed_Message();
  ~Gtid_Executed_Message() override;

  /**
    Appends Gtid executed information in a raw format

   * @param[in] gtid_data encoded GTID data
   * @param[in] len GTID data length
   */
  void append_gtid_executed(uchar *gtid_data, size_t len);

 protected:
  /*
   Implementation of the template methods of Gcs_plugin_message
   */
  void encode_payload(std::vector<unsigned char> *buffer) const override;
  void decode_payload(const unsigned char *buffer,
                      const unsigned char *) override;

 private:
  std::vector<uchar> data;
};

#endif /* CERTIFIER_INCLUDE */
