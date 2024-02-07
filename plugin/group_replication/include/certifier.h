/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef CERTIFIER_INCLUDE
#define CERTIFIER_INCLUDE

#include <assert.h>
#include <mysql/group_replication_priv.h>
#include <atomic>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/certification/certified_gtid.h"
#include "plugin/group_replication/include/certification/gtid_generator.h"
#include "plugin/group_replication/include/certification_result.h"
#include "plugin/group_replication/include/certifier_stats_interface.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/gr_compression.h"
#include "plugin/group_replication/include/gr_decompression.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/pipeline_interfaces.h"
#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message.h"
#include "plugin/group_replication/include/plugin_utils.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_interface.h"

#include "plugin/group_replication/generated/protobuf_lite/replication_group_recovery_metadata.pb.h"

#include "mysql/utils/return_status.h"

/**
  While sending Recovery Metadata the Certification Information is divided into
  several small packets of MAX_COMPRESSED_PACKET_SIZE before sending it to
  group for Recovery.
  The compressed packet size is choosen as 10MB so that multiple threads can
  process (serialize and compress or unserialize and decompress) packets
  simultaneously without consuming too much memory.
*/
#define MAX_COMPRESSED_PACKET_SIZE 10485760

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
  Gtid_set_ref(Tsid_map *tsid_map, int64 parallel_applier_sequence_number)
      : Gtid_set(tsid_map),
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
  virtual bool get_certification_info_recovery_metadata(
      Recovery_metadata_message *recovery_metadata_message) = 0;
  virtual int set_certification_info(
      std::map<std::string, std::string> *cert_info) = 0;
  virtual bool set_certification_info_recovery_metadata(
      Recovery_metadata_message *recovery_metadata_message) = 0;
  virtual bool initialize_server_gtid_set_after_distributed_recovery() = 0;
  virtual void garbage_collect(Gtid_set *executed_gtid_set = nullptr,
                               bool on_member_join = false) = 0;
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

  typedef protobuf_replication_group_recovery_metadata::
      CertificationInformationMap ProtoCertificationInformationMap;

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
    @param is_gtid_specified  True in case GTID is specified for this trx
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
  gr::Certified_gtid certify(Gtid_set *snapshot_version,
                             std::list<const char *> *write_set,
                             bool is_gtid_specified, const char *member_uuid,
                             Gtid_log_event *gle, bool local_transaction);

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
    Retrieves the current certification info.

    @note if concurrent access is introduce to these variables,
    locking is needed in this method

    @param[out] recovery_metadata_message  Retrieves the metadata message

    @return the operation status
      @retval false      OK
      @retval true       Error
  */
  bool get_certification_info_recovery_metadata(
      Recovery_metadata_message *recovery_metadata_message) override;

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
    The received certification info from Recovery Metadata is decoded,
    compressed and added to the member certification info for certification.

    @note if concurrent access is introduce to these variables,
    locking is needed in this method

    @param[in]  recovery_metadata_message  the pointer to
                                           Recovery_metadata_message.

    @return the operation status
      @retval false      OK
      @retval true       Error
  */
  bool set_certification_info_recovery_metadata(
      Recovery_metadata_message *recovery_metadata_message) override;
  /**
    Initializes the gtid_executed set.

    @return the operation status
      @retval false      OK
      @retval true       Error
  */
  bool initialize_server_gtid_set_after_distributed_recovery() override;

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
    @return Generated gtid and Gtid generation result
    @see Return_status
  */
  std::pair<Gtid, mysql::utils::Return_status>
  generate_view_change_group_gtid();

  /**
    Public method to add the given GTID value in the group_gtid_executed set
    which is used to support skip gtid functionality.

    @param[in] gtid GTID to be added

    @retval  1  error during addition.
    @retval  0  success.
  */
  int add_gtid_to_group_gtid_executed(const Gtid &gtid);

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

  /**
    Compute GTID intervals.
  */
  void gtid_intervals_computation();

  /**
    Validates if garbage collect should run against the intersection of the
    received transactions stable sets.

    @param executed_gtid_set intersection gtid set
    @param on_member_join    call due to member joining
   */
  void garbage_collect(Gtid_set *executed_gtid_set = nullptr,
                       bool on_member_join = false) override;

 private:
  /**
   Key used to store group_gtid_executed on certification
   info on View_change_log_event.
  */
  static const std::string GTID_EXTRACTED_NAME;

  /**
    Is certifier initialized.
  */
  std::atomic<bool> initialized{false};

  /**
    Variable to store the sidno used for transactions which will be logged
    with the group_uuid.
  */
  rpl_sidno group_gtid_tsid_map_group_sidno;

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

  /// @brief Returns group_executed_gtid_set or group_extracted_gtid_set while
  /// certifying already applied transactions from the donor
  /// @returns Pointer to the 'correct' group_gtid_set
  const Gtid_set *get_group_gtid_set() const;

  /// @brief Returns group_executed_gtid_set or group_extracted_gtid_set while
  /// certifying already applied transactions from the donor
  /// @returns Pointer to the 'correct' group_gtid_set
  Gtid_set *get_group_gtid_set();

  /// @brief This function determines three sidnos for a specific TSID
  /// based on information obtained from the Gtid_log_event.
  /// @param gle Gtid_log_event from which tsid will be extracted
  /// @param is_gtid_specified True in case GTID is specified
  /// @param snapshot_gtid_set Snapshot GTIDs
  /// @param group_gtid_set Current GTID set
  /// @return A tuple of:
  ///   - group_sidno Sidno relative to the group sid map
  ///   - gtid_snapshot_sidno Sidno relative to the snapshot sid map
  ///   - gtid_global_sidno Sidno relative to the global sid map
  ///   - return status
  /// @details
  /// We need to ensure that group sidno does exist on snapshot
  /// version due to the following scenario:
  ///   1) Member joins the group.
  ///   2) Goes through recovery procedure, view change is queued to
  ///       apply, member is marked ONLINE. This requires
  ///         --group_replication_recovery_complete_at=TRANSACTIONS_CERTIFIED
  ///       to happen.
  ///   3) Despite the view change log event is still being applied,
  ///       since the member is already ONLINE it can execute
  ///       transactions. The first transaction from this member will
  ///       not include any group GTID, since no group transaction is
  ///       yet applied.
  ///   4) As a result of this sequence snapshot_version will not
  ///       contain any group GTID and the below instruction
  ///         snapshot_version->_add_gtid(group_sidno, result);
  ///       would fail because of that
  std::tuple<rpl_sidno, rpl_sidno, rpl_sidno, mysql::utils::Return_status>
  extract_sidno(Gtid_log_event &gle, bool is_gtid_specified,
                Gtid_set &snapshot_gtid_set, Gtid_set &group_gtid_set);

  /// @brief Internal helper method for ending certification, determination
  /// of final GTID values after certification according to certification result
  /// @param[in] gtid_server_sidno SIDNO for transaction GTID as represented in
  /// the server (global sid map)
  /// @param[in] gtid_group_sidno SIDNO for transaction GTID as represented in
  /// the group
  /// @param[in] generated_gno GNO generated for the transaction
  /// @param[in] is_gtid_specified True if GTID was specified
  /// @param[in] local_transaction True in case this transaction originates
  /// from the this server
  /// @param[in] certification_result Determined certification result
  gr::Certified_gtid end_certification_result(
      const rpl_sidno &gtid_server_sidno, const rpl_sidno &gtid_group_sidno,
      const rpl_gno &generated_gno, bool is_gtid_specified,
      bool local_transaction,
      const gr::Certification_result &certification_result);

  /// @brief Adds the transaction's write set to certification info.
  /// @param[out] transaction_last_committed The transaction's logical
  /// timestamps used for MTS
  /// @param[in,out] snapshot_version   The incoming transaction snapshot
  /// version.
  /// @param[in, out] write_set          The incoming transaction write set.
  /// @param[in] local_transaction True in case this transaction originates
  /// from the this server
  gr::Certification_result add_writeset_to_certification_info(
      int64 &transaction_last_committed, Gtid_set *snapshot_version,
      std::list<const char *> *write_set, bool local_transaction);

  /// @brief Updates parallel applier indexes in GLE
  /// @param gle Gle currently processed
  /// @param has_write_set True in case transaction write set is not empty
  /// @param has_write_set_large_size True in case number of write sets in
  /// transactions is greater than
  /// group_replication_preemptive_garbage_collection_rows_threshold
  /// @param transaction_last_committed The transaction's logical timestamps
  /// used for MTS
  void update_transaction_dependency_timestamps(
      Gtid_log_event &gle, bool has_write_set, bool has_write_set_large_size,
      int64 transaction_last_committed);

  bool inline is_initialized() { return initialized; }

  /**
    This shall serialize the certification info stored in protobuf map format,
    and then compress provided serialized string. The compressed payload is
    stored into multiple buffer containers of the output list.

    @param[in]  cert_info        the certification info stored in protobuf map.
    @param[out] uncompresssed_buffer the buffer for uncompressed data.
    @param[out] compressor_list  the certification info in compressed form
                                 splitted into multiple container of list.
    @param[in] compression_type the type of compression used

    @return the operation status
      @retval false      OK
      @retval true       Error
  */
  bool compress_packet(ProtoCertificationInformationMap &cert_info,
                       unsigned char **uncompresssed_buffer,
                       std::vector<GR_compress *> &compressor_list,
                       GR_compress::enum_compression_type compression_type);

  /**
    Sets the certification info according to the given value.
    This shall uncompress and then convert uncompressed string into the protobuf
    map format storing certification info. This certification info is added to
    certifier's certification info.

    @note if concurrent access is introduce to these variables,
    locking is needed in this method

    @param[in]  compression_type  the compression type
    @param[in]  buffer         the compressed  certification info retrieved from
                               recovery procedure.
    @param[in]  buffer_length  the size of the compressed retrieved
                               certification info.
    @param[in]  uncompressed_buffer_length the size of the uncompressed
                                           certification info before it was
                                           compressed.

    @return the operation status
      @retval false      OK
      @retval true       Error
  */
  bool set_certification_info_part(
      GR_compress::enum_compression_type compression_type,
      const unsigned char *buffer, unsigned long long buffer_length,
      unsigned long long uncompressed_buffer_length);

  /**
    Empties certification info.
  */
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
  Tsid_map *certification_info_tsid_map;

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
  Tsid_map *stable_tsid_map;
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
  Tsid_map *group_gtid_tsid_map;

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

  /// Object responsible for generation of the GTIDs for transactions with
  /// gtid_next equal to AUTOMATIC (tagged/untagged)
  gr::Gtid_generator gtid_generator;

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
    Clear incoming queue.
  */
  void clear_incoming();

  /*
    Update method to store the count of the positively and negatively
    certified transaction on a particular group member.
  */
  void update_certified_transaction_count(bool result, bool local_transaction);

  /*
    The first remote transaction certified does need to reset
    replication_group_applier channel previous transaction
    sequence_number.
  */
  bool is_first_remote_transaction_certified{true};

  /**
    Removes the intersection of the received transactions stable
    sets from certification database.

    @param intersection_gtid_set intersection gtid set
    @param preemptive            is a preemptive run
   */
  void garbage_collect_internal(Gtid_set *intersection_gtid_set,
                                bool preemptive = false);

  /**
    Computes intersection between all sets received, so that we
    have the already applied transactions on all servers.

    @return the operation status
      @retval false  it did not run garbage_collect
      @retval true   it did run garbage_collect
  */
  bool intersect_members_gtid_executed_and_garbage_collect();

  enum enum_update_status {
    // stable set successfully updated
    STABLE_SET_UPDATED,
    // stable set already contains set
    STABLE_SET_ALREADY_CONTAINED,
    // not able to update due error
    STABLE_SET_ERROR
  };

  /**
   * Update stable set with set if not already contained.
   *
   * @param set Gtid to add to stable set
   *
   * @return status of operation
   */
  enum enum_update_status update_stable_set(const Gtid_set &set);
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

    // Length of the payload item: 8 bytes
    PIT_SENT_TIMESTAMP = 2,

    // No valid type codes can appear after this one.
    PIT_MAX = 3
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

  /**
    Return the time at which the message contained in the buffer was sent.
    @see Metrics_handler::get_current_time()

    @param[in] buffer            the buffer to decode from.
    @param[in] length            the buffer length

    @return the time on which the message was sent.
  */
  static uint64_t get_sent_timestamp(const unsigned char *buffer,
                                     size_t length);

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
