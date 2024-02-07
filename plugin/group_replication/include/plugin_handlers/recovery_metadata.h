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

#ifndef GR_RECOVERY_METADATA_INCLUDE
#define GR_RECOVERY_METADATA_INCLUDE

#include <map>
#include <string>

#include "plugin/group_replication/include/plugin_handlers/recovery_metadata_joiner_information.h"
#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

/**
  @class Recovery_metadata_module
  This class handles the recovery metadata.
  For safety, processing of add or delete recovery metadata or any processing of
  recovery metadata like removing group members should be done from pipeline.
  This class also handles some joiner related information, which is processed
  from GCS thread directly. This is temporary information and is deleted as soon
  as recovery metadata is received.
*/
class Recovery_metadata_module {
 public:
  /**
    Recovery_metadata_module constructor
  */
  Recovery_metadata_module();

  /**
    Recovery_metadata_module Destructor
  */
  virtual ~Recovery_metadata_module();

  /**
    Inserts a new recovery_view_metadata entry for the given VIEW_ID.

    @param  view_id       View ID of metadata

    @return the insert success status
      @retval true   returns a pair of an iterator to the newly inserted
                     element and a value of true
      @retval false  returns a pair of an iterator pointing to map end()
                     and a value of false
  */
  std::pair<std::map<const std::string, Recovery_metadata_message *>::iterator,
            bool>
  add_recovery_view_metadata(const std::string &view_id);

  /**
    Deletes all recovery_view_metadata_table elements.
  */
  void delete_all_recovery_view_metadata();

  /**
    Remove members not present in given list from all the stored
    recovery_view_metadata_table records.
    If post cleanup there is no valid sender or joiner for the view-id this
    function will do the cleanup for such view-ids.
    Also, the function does the cleanup of the view-ids mentioned in the
    parameter view_id_delete_list.

    @param  members_left  remove members left from the joining member
                          and online member list.
    @param  view_id_delete_list List of VIEW-IDs of which metadata is no longer
    required
  */
  void
  delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left(
      std::vector<Gcs_member_identifier> &members_left,
      std::vector<std::string> &view_id_delete_list);

  /**
    Broadcast message to group members for particular VIEW_ID.

    @param  recovery_metadata_msg  pointer to Recovery metadata message store.

    @return the send status
      @retval true   Error
      @retval false  Success
  */
  enum_gcs_error send_recovery_metadata(
      Recovery_metadata_message *recovery_metadata_msg);

  /**
    Broadcast recovery metadata error message to the group members for
    particular VIEW_ID.
    This message means recovery metadata message cannot be successfully provided
    to the joiner, so joiner should leave the group and valid senders should
    cleanup the recovery metadata if saved.

    @param  view_id  View-id for which error has to be send.

    @return the send status
      @retval true   Error
      @retval false  Success
  */
  enum_gcs_error send_error_message(const std::string &view_id);

  // Joiner related information
  /**
    Store view-id in which joiner joined along with all the members that were
    ONLINE at the time of joining.

    @param  view_id  view-id in which joiner joined the group
    @param  valid_senders  ONLINE members at the time of joining
  */
  void store_joiner_view_id_and_valid_senders(
      const std::string &view_id,
      const std::vector<Gcs_member_identifier> &valid_senders);

  /**
    Deleted the joiner information i.e. view-id and valid sender list.
    m_recovery_metadata_joiner_information object will be deleted.
  */
  void delete_joiner_view_id();

  /**
    When member leaves the group, we need to cleanup those members and remove
    them from valid sender list. If post cleanup there are no members that have
    joiner recovery metadata, joiner leaves the group.

    @param  leaving  list of members that have left the group
    @param  is_leaving  if joiner is also leaving the group
  */
  void delete_leaving_members_from_joiner_and_leave_group_if_no_valid_sender(
      const std::vector<Gcs_member_identifier> &leaving, bool is_leaving);

  /**
    Returns if recovery metadata view belongs to the joiner.

    @param  view_id  view-id of which metadata has been received

    @return the send status
      @retval true   Yes, joiner metadata
      @retval false  No, other joiner metadata
  */
  bool is_joiner_recovery_metadata(const std::string &view_id);

 private:
  /**
    Broadcast error message to group members for particular VIEW_ID.

    @param  recovery_metadata_msg  pointer to Recovery metadata message store.

    @return the send status
      @retval true   Error
      @retval false  Success
  */
  enum_gcs_error send_error_message_internal(
      Recovery_metadata_message *recovery_metadata_msg);

  /**
    Remove members that have left the group from valid-sender and valid-joiner
    list.
    This function is called from
    delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left.
    If post cleanup there is no valid sender or joiner for the view-id, that
    view is added to the list of view_id_delete_list for cleanup.

    @param  members_left  remove members left from the joining member
                          and online member list.
    @param  view_id_delete_list List of VIEW-IDs of which metadata is no longer
    required
  */
  void delete_members_from_all_recovery_view_metadata_internal(
      std::vector<Gcs_member_identifier> &members_left,
      std::vector<std::string> &view_id_delete_list);

  /**
    Remove recovery_view_metadata_table element for the given VIEW_ID.
    This function is called from
    delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left.

    @param  view_id       View ID of metadata

    @return the key delete status
      @retval true   Key was not found
      @retval false  Key was found and deleted
  */
  bool delete_recovery_view_metadata_internal(const std::string view_id);

  /**
    Leaves the group.

    @param  err_msg  Details of why member is leaving the group.
  */
  void leave_the_group_internal(std::string err_msg);

 private:
  /**
    Stores recovery_view_metadata entry for the given view_id.
    The record is kept on sender till it is successfully broadcasted.
  */
  std::map<const std::string, Recovery_metadata_message *>
      recovery_view_metadata_table;

  /**
    Stores the joiner related information i.e. view-id and the list of all the
    members that were ONLINE at the time of joining. The scope of this object is
    limited, when member joins the group it needs to store it's metadata.
    Because when recovery metadata is being received joiner needs to identify
    which recovery metadata it can use to come ONLINE. So joiner temporary
    stores it view-id and valid sender list in this object till the time it
    receives it metadata. Post receiving the metadata, this information is not
    relevant for the joiner, so joiner immediately deletes this information.
  */
  Recovery_metadata_joiner_information *m_recovery_metadata_joiner_information{
      nullptr};
};

#endif /* GR_RECOVERY_METADATA_INCLUDE */
