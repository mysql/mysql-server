/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MEMBER_INFO_INCLUDE
#define MEMBER_INFO_INCLUDE

/*
  The file contains declarations relevant to Member state and
  its identification by the Protocol Client.
*/

/*
  Since this file is used on unit tests includes must set here and
  not through plugin_server_include.h.
*/

#include <list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "my_sys.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/member_version.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "plugin/group_replication/include/services/notification/notification.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

/*
  Encoding of the group_replication_enforce_update_everywhere_checks
  config value in the member info structure.
*/
#define CNF_ENFORCE_UPDATE_EVERYWHERE_CHECKS_F 0x1

/*
  Encoding of the group_replication_single_primary_mode config value
  in the member info structure.
*/
#define CNF_SINGLE_PRIMARY_MODE_F 0x2

/*
  Valid values of lower_case_table_names are 0 - 2.
  So when member has DEFAULT_NOT_RECEIVED value, it means its
  lower_case_table_names value is not known.
*/
#define DEFAULT_NOT_RECEIVED_LOWER_CASE_TABLE_NAMES 65540
#ifndef DBUG_OFF
#define SKIP_ENCODING_LOWER_CASE_TABLE_NAMES 65541
#endif

/*
  @class Group_member_info

  Describes all the properties of a group member
*/
class Group_member_info : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: variable
    PIT_HOSTNAME = 1,

    // Length of the payload item: 2 bytes
    PIT_PORT = 2,

    // Length of the payload item: variable
    PIT_UUID = 3,

    // Length of the payload item: variable
    PIT_GCS_ID = 4,

    // Length of the payload item: 1 byte
    PIT_STATUS = 5,

    // Length of the payload item: 4 bytes
    PIT_VERSION = 6,

    // Length of the payload item: 2 bytes
    PIT_WRITE_SET_EXTRACTION_ALGORITHM = 7,

    // Length of the payload item: variable
    PIT_EXECUTED_GTID = 8,

    // Length of the payload item: variable
    PIT_RETRIEVED_GTID = 9,

    // Length of the payload item: 8 bytes
    PIT_GTID_ASSIGNMENT_BLOCK_SIZE = 10,

    // length of the role item: 1 byte
    PIT_MEMBER_ROLE = 11,

    // length of the configuration flags: 4 bytes
    PIT_CONFIGURATION_FLAGS = 12,

    // length of the conflict detection enabled: 1 byte
    PIT_CONFLICT_DETECTION_ENABLE = 13,

    // Length of the payload item: 2 bytes
    PIT_MEMBER_WEIGHT = 14,

    // Length of the payload item: 2 bytes
    PIT_LOWER_CASE_TABLE_NAME = 15,

    // Length of the payload item: 1 bytes
    PIT_GROUP_ACTION_RUNNING = 16,

    // Length of the payload item: 1 bytes
    PIT_PRIMARY_ELECTION_RUNNING = 17,

    // Length of the payload item: 1 bytes
    PIT_DEFAULT_TABLE_ENCRYPTION = 18,

    // Length of the payload item: variable
    PIT_PURGED_GTID = 19,

    // No valid type codes can appear after this one.
    PIT_MAX = 20
  };

  /*
   @enum Member_recovery_status

   This enumeration describes all the states that a member can assume while in a
   group.
   */
  typedef enum {
    MEMBER_ONLINE = 1,
    MEMBER_OFFLINE,
    MEMBER_IN_RECOVERY,
    MEMBER_ERROR,
    MEMBER_UNREACHABLE,
    MEMBER_END  // the end of the enum
  } Group_member_status;

  /*
    @enum Group_member_role

    This enumeration describes all the roles a server can have.
  */
  typedef enum {
    MEMBER_ROLE_PRIMARY = 1,
    MEMBER_ROLE_SECONDARY,
    MEMBER_ROLE_END
  } Group_member_role;

  /**
    Group_member_info constructor

    @param[in] hostname_arg                           member hostname
    @param[in] port_arg                               member port
    @param[in] uuid_arg                               member uuid
    @param[in] write_set_extraction_algorithm         write set extraction
    algorithm
    @param[in] gcs_member_id_arg                      member GCS member
    identifier
    @param[in] status_arg                             member Recovery status
    @param[in] member_version_arg                     member version
    @param[in] gtid_assignment_block_size_arg         member gtid assignment
    block size
    @param[in] role_arg                               member role within the
    group
    @param[in] in_single_primary_mode                 is member in single mode
    @param[in] has_enforces_update_everywhere_checks  has member enforce update
    check
    @param[in] member_weight_arg                      member_weight
    @param[in] lower_case_table_names_arg             lower case table names
    @param[in] psi_mutex_key_arg                      mutex key
    @param[in] default_table_encryption_arg           default_table_encryption
   */
  Group_member_info(const char *hostname_arg, uint port_arg,
                    const char *uuid_arg, int write_set_extraction_algorithm,
                    const std::string &gcs_member_id_arg,
                    Group_member_info::Group_member_status status_arg,
                    Member_version &member_version_arg,
                    ulonglong gtid_assignment_block_size_arg,
                    Group_member_info::Group_member_role role_arg,
                    bool in_single_primary_mode,
                    bool has_enforces_update_everywhere_checks,
                    uint member_weight_arg, uint lower_case_table_names_arg,
                    bool default_table_encryption_arg,
                    PSI_mutex_key psi_mutex_key_arg =
                        key_GR_LOCK_group_member_info_update_lock);

  /**
    Copy constructor

    @param other source of the copy
   */
  Group_member_info(Group_member_info &other);

  /**
   * Group_member_info raw data constructor
   *
   * @param[in] data raw data
   * @param[in] len raw data length
   * @param[in] psi_mutex_key_arg                      mutex key
   */
  Group_member_info(const uchar *data, size_t len,
                    PSI_mutex_key psi_mutex_key_arg =
                        key_GR_LOCK_group_member_info_update_lock);

  /**
    Destructor
   */
  virtual ~Group_member_info();

  /**
    Update Group_member_info.

    @param[in] hostname_arg                           member hostname
    @param[in] port_arg                               member port
    @param[in] uuid_arg                               member uuid
    @param[in] write_set_extraction_algorithm         write set extraction
    algorithm
    @param[in] gcs_member_id_arg                      member GCS member
    identifier
    @param[in] status_arg                             member Recovery status
    @param[in] member_version_arg                     member version
    @param[in] gtid_assignment_block_size_arg         member gtid assignment
    block size
    @param[in] role_arg                               member role within the
    group
    @param[in] in_single_primary_mode                 is member in single mode
    @param[in] has_enforces_update_everywhere_checks  has member enforce update
    check
    @param[in] member_weight_arg                      member_weight
    @param[in] lower_case_table_names_arg             lower case table names
    @param[in] default_table_encryption_arg           default table encryption
   */
  void update(char *hostname_arg, uint port_arg, char *uuid_arg,
              int write_set_extraction_algorithm,
              const std::string &gcs_member_id_arg,
              Group_member_info::Group_member_status status_arg,
              Member_version &member_version_arg,
              ulonglong gtid_assignment_block_size_arg,
              Group_member_info::Group_member_role role_arg,
              bool in_single_primary_mode,
              bool has_enforces_update_everywhere_checks,
              uint member_weight_arg, uint lower_case_table_names_arg,
              bool default_table_encryption_arg);

  /**
    @return the member hostname
   */
  std::string get_hostname();

  /**
    @return the member port
   */
  uint get_port();

  /**
    @return the member uuid
   */
  std::string get_uuid();

  /**
    @return the member identifier in the GCS layer
   */
  Gcs_member_identifier get_gcs_member_id();

  /**
    @return the member recovery status
   */
  Group_member_status get_recovery_status();

  /**
    @return the member role type code.
   */
  Group_member_role get_role();

  /**
    @return the member role type code in string
   */
  const char *get_member_role_string();

  /**
    @return the member plugin version
   */
  Member_version get_member_version();

  /**
    @return the member GTID_EXECUTED set
   */
  std::string get_gtid_executed();

  /**
    @return the member GTID_PURGED set
   */
  std::string get_gtid_purged();

  /**
    @return the member GTID_RETRIEVED set for the applier channel
  */
  std::string get_gtid_retrieved();

  /**
    @return the member algorithm for extracting write sets
  */
  uint get_write_set_extraction_algorithm();

  /**
    @return the member gtid assignment block size
  */
  ulonglong get_gtid_assignment_block_size();

  /**
    @return the member configuration flags
  */
  uint32 get_configuration_flags();

  /**
    Set the primary flag
    @param in_primary_mode is the member in primary mode
  */
  void set_primary_mode_flag(bool in_primary_mode);

  /**
    Set the enforces_update_everywhere_checks flag
    @param enforce_everywhere_checks are the update everywhere checks active or
    not
  */
  void set_enforces_update_everywhere_checks_flag(
      bool enforce_everywhere_checks);

  /**
    @return the global-variable lower case table names value
  */
  uint get_lower_case_table_names();

  /**
    @return the global-variable lower case table names value
  */
  bool get_default_table_encryption();

  /**
    @return the member state of system variable
            group_replication_single_primary_mode
  */
  bool in_primary_mode();

  /**
    @return the member state of system variable
            group_replication_enforce_update_everywhere_checks
  */
  bool has_enforces_update_everywhere_checks();

  /**
    Updates this object recovery status

    @param[in] new_status the status to set
   */
  void update_recovery_status(Group_member_status new_status);

  /**
    Updates this object GTID sets

    @param[in] executed_gtids the status to set
    @param[in] purged_gtids   the status to set
    @param[in] retrieve_gtids the status to set
   */
  void update_gtid_sets(std::string &executed_gtids, std::string &purged_gtids,
                        std::string &retrieve_gtids);

  /**
    Updates this object member role.

    @param[in] new_role the role to set.
   */
  void set_role(Group_member_role new_role);

  /**
    @return the member status as string.
   */
  static const char *get_member_status_string(Group_member_status status);

  /**
    @return configuration flag as string
   */
  static const char *get_configuration_flag_string(
      const uint32 configuation_flag);

  /**
    @return the member configuration flags as string
   */
  static std::string get_configuration_flags_string(
      const uint32 configuation_flags);

  /**
    @return Compare two members using member version
   */
  static bool comparator_group_member_version(Group_member_info *m1,
                                              Group_member_info *m2);

  /**
    @return Compare two members using server uuid
   */
  static bool comparator_group_member_uuid(Group_member_info *m1,
                                           Group_member_info *m2);

  /**
    @return Compare two members using member weight
    @note if the weight is same, the member is sorted in
          lexicographical order using its uuid.
   */
  static bool comparator_group_member_weight(Group_member_info *m1,
                                             Group_member_info *m2);

  /**
    Return true if member version is higher than other member version
   */
  bool has_greater_version(Group_member_info *other);

  /**
    Return true if server uuid is lower than other member server uuid
   */
  bool has_lower_uuid(Group_member_info *other);

  /**
    Return true if member weight is higher than other member weight
   */
  bool has_greater_weight(Group_member_info *other);

  /**
    Redefinition of operate ==, which operate upon the uuid
   */
  bool operator==(Group_member_info &other);

  /**
    Sets this member as unreachable.
   */
  void set_unreachable();

  /**
    Sets this member as reachable.
   */
  void set_reachable();

  /**
    Return true if this has been flagged as unreachable.
   */
  bool is_unreachable();

  /**
    Update this member conflict detection to true
   */
  void enable_conflict_detection();

  /**
    Update this member conflict detection to false
   */
  void disable_conflict_detection();

  /**
    Return true if conflict detection is enable on this member
   */
  bool is_conflict_detection_enabled();

  /**
    Update member weight

    @param[in] new_member_weight  new member_weight to set
   */
  void set_member_weight(uint new_member_weight);

  /**
    Return member weight
   */
  uint get_member_weight();

  /**
    @return is a group action running in this member
  */
  bool is_group_action_running();

  /**
    Sets if the member is currently running a group action
    @param is_running is an action running
  */
  void set_is_group_action_running(bool is_running);

  /**
    @return is a primary election running in this member
  */
  bool is_primary_election_running();

  /**
    Sets if the member is currently running a primary election
    @param is_running is an election running
  */
  void set_is_primary_election_running(bool is_running);

 protected:
  void encode_payload(std::vector<unsigned char> *buffer) const;
  void decode_payload(const unsigned char *buffer, const unsigned char *);

 private:
  /**
    Internal method without concurrency control.

    @return the member state of system variable
            group_replication_single_primary_mode
  */
  bool in_primary_mode_internal();

  /**
    Return true if server uuid is lower than other member server uuid
    Internal method without concurrency control.
   */
  bool has_lower_uuid_internal(Group_member_info *other);

  mysql_mutex_t update_lock;
  std::string hostname;
  uint port;
  std::string uuid;
  Group_member_status status;
  Gcs_member_identifier *gcs_member_id;
  Member_version *member_version;
  std::string executed_gtid_set;
  std::string purged_gtid_set;
  std::string retrieved_gtid_set;
  uint write_set_extraction_algorithm;
  uint64 gtid_assignment_block_size;
  bool unreachable;
  Group_member_role role;
  uint32 configuration_flags;
  bool conflict_detection_enable;
  uint member_weight;
  uint lower_case_table_names;
  bool default_table_encryption;
  bool group_action_running;
  bool primary_election_running;
#ifndef DBUG_OFF
 public:
  bool skip_encode_default_table_encryption;
#endif
  // Allow use copy constructor on unit tests.
  PSI_mutex_key psi_mutex_key;
};

/*
  @interface Group_member_info_manager_interface

  Defines the set of operations that a Group_member_info_manager should provide.
  This is a component that lies on top of the GCS, on the application level,
  providing richer and relevant information to the plugin.
 */
class Group_member_info_manager_interface {
 public:
  virtual ~Group_member_info_manager_interface() {}

  virtual size_t get_number_of_members() = 0;

  /**
    Is the member present in the group info

    @param[in] uuid uuid to check
    @return true if present, false otherwise
  */
  virtual bool is_member_info_present(const std::string &uuid) = 0;

  /**
    Retrieves a registered Group member by its uuid

    @param[in] uuid uuid to retrieve
    @return reference to a copy of Group_member_info. NULL if not managed.
            The return value must deallocated by the caller.
   */
  virtual Group_member_info *get_group_member_info(const std::string &uuid) = 0;

  /**
    Retrieves a registered Group member by an index function.
    One is free to determine the index function. Nevertheless, it should have
    the same result regardless of the member of the group where it is called

    @param[in] idx the index
    @return reference to a Group_member_info. NULL if not managed
   */
  virtual Group_member_info *get_group_member_info_by_index(int idx) = 0;

  /**
    Return lowest member version.

    @return group lowest version, if used at place where member can be OFFLINE
            or in ERROR state, version 0xFFFFFF may be returned(not found)
   */
  virtual Member_version get_group_lowest_online_version() = 0;

  /**
    Retrieves a registered Group member by its backbone GCS identifier

    @param[in] idx the GCS identifier
    @return reference to a copy of Group_member_info. NULL if not managed.
            The return value must deallocated by the caller.
   */
  virtual Group_member_info *get_group_member_info_by_member_id(
      Gcs_member_identifier idx) = 0;

  /**
    Retrieves all Group members managed by this site

    @return a vector with copies to all managed Group_member_info
   */
  virtual std::vector<Group_member_info *> *get_all_members() = 0;

  /**
    Retrieves all ONLINE Group members managed by this site, or
    NULL if any group member version is from a version lower than
    #TRANSACTION_WITH_GUARANTEES_VERSION.

    @return  list of all ONLINE members, if all members have version
             equal or greater than #TRANSACTION_WITH_GUARANTEES_VERSION
             otherwise  NULL

    @note the memory allocated for the list ownership belongs to the
          caller
   */
  virtual std::list<Gcs_member_identifier> *get_online_members_with_guarantees(
      const Gcs_member_identifier &exclude_member) = 0;

  /**
    Adds a new member to be managed by this Group manager

    @param[in] new_member new group member
   */
  virtual void add(Group_member_info *new_member) = 0;

  /**
    Removes all members of the group and update new local member.

    @param[in] update_local_member new Group member
   */
  virtual void update(Group_member_info *update_local_member) = 0;

  /**
    Updates all members of the group. Typically used after a view change.

    @param[in] new_members new Group members
   */
  virtual void update(std::vector<Group_member_info *> *new_members) = 0;

  /**
    Updates the status of a single member

    @param[in] uuid        member uuid
    @param[in] new_status  status to change to
    @param[in,out] ctx     The notification context to update.
   */
  virtual void update_member_status(
      const std::string &uuid,
      Group_member_info::Group_member_status new_status,
      Notification_context &ctx) = 0;

  /**
    Updates the GTID sets on a single member


    @param[in] uuid            member uuid
    @param[in] gtid_executed   the member executed GTID set
    @param[in] purged_gtids    the server purged GTID set
    @param[in] gtid_retrieved  the member retrieved GTID set for the applier
  */
  virtual void update_gtid_sets(const std::string &uuid,
                                std::string &gtid_executed,
                                std::string &purged_gtids,
                                std::string &gtid_retrieved) = 0;
  /**
    Updates the role of a single member

    @param[in] uuid        member uuid
    @param[in] new_role    role to change to
    @param[in,out] ctx     The notification context to update.
   */
  virtual void update_member_role(const std::string &uuid,
                                  Group_member_info::Group_member_role new_role,
                                  Notification_context &ctx) = 0;

  /**
   Updates the primary/secondary roles of the group.
   This method allows for all roles to be updated at once in the same method

   @param[in] uuid        the primary member uuid
   @param[in,out] ctx     The notification context to update.
  */
  virtual void update_group_primary_roles(const std::string &uuid,
                                          Notification_context &ctx) = 0;

  /**
  Updates the weight of a single member

  @param[in] uuid        member uuid
  @param[in] member_weight  the new weight
*/
  virtual void update_member_weight(const std::string &uuid,
                                    uint member_weight) = 0;

  /**
    Changes the primary flag on all members
    @param in_primary_mode is the member in primary mode
  */
  virtual void update_primary_member_flag(bool in_primary_mode) = 0;

  /**
    Set the enforces_update_everywhere_checks flag on all members
    @param enforce_everywhere are the update everywhere checks active or not
  */
  virtual void update_enforce_everywhere_checks_flag(
      bool enforce_everywhere) = 0;

  /**
    Encodes this object to send via the network

    @param[out] to_encode out parameter to receive the encoded data
   */
  virtual void encode(std::vector<uchar> *to_encode) = 0;

  /**
    Decodes the raw format of this object

    @param[in] to_decode raw encoded data
    @param[in] length    raw encoded data length
    @return a vector of Group_member_info references
   */
  virtual std::vector<Group_member_info *> *decode(const uchar *to_decode,
                                                   size_t length) = 0;

  /**
    Check if some member of the group has the conflict detection enable

    @return true if at least one member has  conflict detection enabled
  */
  virtual bool is_conflict_detection_enabled() = 0;

  /**
    Return the uuid for the for the primary

    @param[out] primary_member_uuid the uuid of the primary will be assigned
    here.

    @note If there is no primary or the member is on error state, the returned
    uuid is "UNDEFINED". If not on primary mode it returns an empty string.

    @return true if the member is in primary mode, false if it is not.
  */
  virtual bool get_primary_member_uuid(std::string &primary_member_uuid) = 0;

  /**
    Return the group member info for the current group primary

    @note the returned reference must be deallocated by the caller.

    @return reference to a Group_member_info. NULL if not managed
  */
  virtual Group_member_info *get_primary_member_info() = 0;

  /**
    Check if majority of the group is unreachable

    This approach is optimistic, right after return the majority can be
    reestablish or go away.

    @return true if majority of the group is unreachable
  */
  virtual bool is_majority_unreachable() = 0;

  /**
    Check if an unreachable member exists

    This approach is optimistic, right after return a member can be marked as
    rechable/unreachable

    @return true if an unreachable member exists
  */
  virtual bool is_unreachable_member_present() = 0;

  /**
    Check if a member in recovery exists in the group

    This approach is optimistic, right after return a member can enter the group

    @return true if a member in recovery exists
  */
  virtual bool is_recovering_member_present() = 0;

  /**
    This method returns all ONLINE and RECOVERING members comma separated
    host and port in string format.

    @return hosts and port of all ONLINE and RECOVERING members
  */
  virtual std::string get_string_current_view_active_hosts() const = 0;

  /**
    This method returns the update lock for consistent read of member state.

    @return update_lock reference
  */
  virtual mysql_mutex_t *get_update_lock() = 0;
};

/**
  @class Group_member_info_manager

  Implementation of the interface Group_member_info_manager_interface
 */
class Group_member_info_manager : public Group_member_info_manager_interface {
 public:
  Group_member_info_manager(
      Group_member_info *local_member_info,
      PSI_mutex_key psi_mutex_key =
          key_GR_LOCK_group_member_info_manager_update_lock);

  virtual ~Group_member_info_manager();

  size_t get_number_of_members();

  bool is_member_info_present(const std::string &uuid);

  Group_member_info *get_group_member_info(const std::string &uuid);

  Group_member_info *get_group_member_info_by_index(int idx);

  Member_version get_group_lowest_online_version();

  Group_member_info *get_group_member_info_by_member_id(
      Gcs_member_identifier idx);

  std::vector<Group_member_info *> *get_all_members();

  std::list<Gcs_member_identifier> *get_online_members_with_guarantees(
      const Gcs_member_identifier &exclude_member);

  void add(Group_member_info *new_member);

  void update(Group_member_info *update_local_member);

  void update(std::vector<Group_member_info *> *new_members);

  void update_member_status(const std::string &uuid,
                            Group_member_info::Group_member_status new_status,
                            Notification_context &ctx);

  void update_gtid_sets(const std::string &uuid, std::string &gtid_executed,
                        std::string &purged_gtids, std::string &gtid_retrieved);

  void update_member_role(const std::string &uuid,
                          Group_member_info::Group_member_role new_role,
                          Notification_context &ctx);

  void update_group_primary_roles(const std::string &uuid,
                                  Notification_context &ctx);

  void update_member_weight(const std::string &uuid, uint member_weight);

  void update_primary_member_flag(bool in_primary_mode);

  void update_enforce_everywhere_checks_flag(bool enforce_everywhere);

  void encode(std::vector<uchar> *to_encode);

  std::vector<Group_member_info *> *decode(const uchar *to_decode,
                                           size_t length);

  bool is_conflict_detection_enabled();

  bool get_primary_member_uuid(std::string &primary_member_uuid);

  Group_member_info *get_primary_member_info();

  bool is_majority_unreachable();

  bool is_unreachable_member_present();

  bool is_recovering_member_present();

  std::string get_string_current_view_active_hosts() const;

  mysql_mutex_t *get_update_lock() { return &update_lock; }

 private:
  void clear_members();

  std::map<std::string, Group_member_info *> *members;
  Group_member_info *local_member_info;

  mysql_mutex_t update_lock;
};

/**
 This is the Group_member_info_manager message.
 It is composed by a fixed header and 1 or more Group_member_info messages.
 Each Group_member_info message does have its own fixed header.

 The on-the-wire representation of the message is:

  +-------------------+-----------+--------------------------------------+
  | field             | wire size | description                          |
  +===================+===========+======================================+
  | version           |   4 bytes | protocol version                     |
  | fixed_hdr_len     |   2 bytes | length of the fixed header           |
  | message_len       |   8 bytes | length of the message                |
  | cargo_type        |   2 bytes | the cargo type in the payload        |
  +-------------------+-----------+--------------------------------------+
  | payload_item_type |   2 bytes | PIT_MEMBERS_NUMBER                   |
  | payload_item_len  |   8 bytes | size of PIT_MEMBERS_NUMBER value     |
  | payload_item      |   X bytes | number of members                    |
  +-------------------+-----------+--------------------------------------+
  | payload_item_type |   2 bytes | PIT_MEMBER_DATA                      |
  | payload_item_len  |   8 bytes | size of CT_MEMBER_INFO_MESSAGE data  |
  | payload_item      |   X bytes | CT_MEMBER_INFO_MESSAGE data          |
  +-------------------+-----------+--------------------------------------+

 The last tree lines occur the number of times specified on
 PIT_MEMBERS_NUMBER.
*/
class Group_member_info_manager_message : public Plugin_gcs_message {
 public:
  enum enum_payload_item_type {
    // This type should not be used anywhere.
    PIT_UNKNOWN = 0,

    // Length of the payload item: 2 bytes
    PIT_MEMBERS_NUMBER = 1,

    // Length of the payload item: variable
    PIT_MEMBER_DATA = 2,

    // No valid type codes can appear after this one.
    PIT_MAX = 3
  };

  /**
    Group_member_info_manager_message constructor.
   */
  Group_member_info_manager_message();

  /**
    Group_member_info_manager_message constructor.

    @param[in] group_info  Group_member_info_manager members information
   */
  Group_member_info_manager_message(Group_member_info_manager &group_info);

  /**
    Group_member_info_manager_message constructor.

    @param[in] member_info  Group_member_info one member information
   */
  Group_member_info_manager_message(Group_member_info *member_info);

  /**
    Group_member_info_manager_message destructor.
   */
  virtual ~Group_member_info_manager_message();

  /**
    Retrieves all Group members on this message.

    @return a vector with copies to all members.
   */
  std::vector<Group_member_info *> *get_all_members();

 protected:
  void encode_payload(std::vector<unsigned char> *buffer) const;
  void decode_payload(const unsigned char *buffer, const unsigned char *end);

 private:
  /**
    Clear members and its allocated memory.
  */
  void clear_members();

  std::vector<Group_member_info *> *members;
};

#endif /* MEMBER_INFO_INCLUDE */
