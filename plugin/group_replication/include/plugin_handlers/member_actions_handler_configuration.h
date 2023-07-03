/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef MEMBER_ACTIONS_HANDLER_CONFIGURATION_INCLUDED
#define MEMBER_ACTIONS_HANDLER_CONFIGURATION_INCLUDED

#include "plugin/group_replication/generated/protobuf_lite/replication_group_member_actions.pb.h"
#include "plugin/group_replication/include/configuration_propagation.h"
#include "sql/rpl_sys_table_access.h"

/**
  @class Member_actions_handler_configuration

  The member actions table configuration abstraction layer.
*/
class Member_actions_handler_configuration {
 public:
  /**
    Constructor.

    @param[in]  configuration_propagation
                            the object to call to propagate configuration
  */
  Member_actions_handler_configuration(
      Configuration_propagation *configuration_propagation);

  virtual ~Member_actions_handler_configuration();

  /**
    Enable/disable a member action.

    @param[in]  name        the action name
    @param[in]  event       the action event
    @param[in]  enable      true to enable the action, false to disable

    @return std::pair<bool, std::string> where each element has the
            following meaning:
              first element of the pair is the function error value:
                false  Successful
                true   Error
              second element of the pair is the error message.
  */
  std::pair<bool, std::string> enable_disable_action(const std::string &name,
                                                     const std::string &event,
                                                     bool enable);

  /**
    Retrieve member actions configured to trigger on a given event.

    @param[out] action_list member actions list
    @param[in]  event       the event to filter the actions

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool get_actions_for_event(
      protobuf_replication_group_member_actions::ActionList &action_list,
      const std::string &event);

  /**
    Retrieve member actions configuration in the serialized
    format.

    @param[out] serialized_configuration
                            the serialized configuration
    @param[in]  set_force_update
                            if true enables the force_update flag, which will
                            make the sent configuration to override other
                            members configuration

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool get_all_actions(std::string &serialized_configuration,
                       bool set_force_update);

  /**
    Update member actions configuration with a given configuration.
    If the given configuration has a version lower than the local
    configuration, the update is skipped.

    @param[in]  action_list member actions list

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool update_all_actions(
      const protobuf_replication_group_member_actions::ActionList &action_list);

  /**
    Replace member actions configuration with a given configuration,
    even if the given configuration has a version lower than the local
    configuration.

    @param[in]  action_list member actions list

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool replace_all_actions(
      const protobuf_replication_group_member_actions::ActionList &action_list);

  /**
    Reset member actions to the default configuration.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool reset_to_default_actions_configuration();

 private:
  void field_store(Field *field, const std::string &value);
  void field_store(Field *field, uint value);

  /**
    Commit and propagate the local member actions configuration.

    @param[in]  table_op    the table that persists the configuration

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  std::pair<bool, std::string> commit_and_propagate_changes(
      Rpl_sys_table_access &table_op);

  /**
    Retrieve member actions configuration in the serialized
    format.

    @param[in]  table_op    the table that persists the configuration
    @param[out] action_list member actions list

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool get_all_actions_internal(
      Rpl_sys_table_access &table_op,
      protobuf_replication_group_member_actions::ActionList &action_list);

  /**
    Update member actions configuration with a given configuration.
    If the given configuration has a version lower than the local
    configuration, the update is skipped.

    @param[in]  action_list member actions list
    @param[in]  ignore_version
                            if false, the local configuration is updated
                                      if the version of the given configuration
                                      is higher than the local one
                               true, there is no version checking
    @param[in]  ignore_global_read_lock
                            if true,  global_read_lock is ignored on commit
                               false, otherwise

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool update_all_actions_internal(
      const protobuf_replication_group_member_actions::ActionList &action_list,
      bool ignore_version, bool ignore_global_read_lock);

  const std::string s_schema_name{"mysql"};
  const std::string s_table_name{"replication_group_member_actions"};
  const uint s_fields_number{6};

  /**
    The pointer to the object to call to propagate configuration.
  */
  Configuration_propagation *m_configuration_propagation{nullptr};
};

#endif /* MEMBER_ACTIONS_HANDLER_CONFIGURATION_INCLUDED */
