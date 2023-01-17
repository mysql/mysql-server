/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef MEMBER_ACTIONS_HANDLER_INCLUDED
#define MEMBER_ACTIONS_HANDLER_INCLUDED

#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_message_service.h>
#include <string>
#include <utility>
#include "plugin/group_replication/generated/protobuf_lite/replication_group_member_actions.pb.h"
#include "plugin/group_replication/include/configuration_propagation.h"
#include "plugin/group_replication/include/thread/mysql_thread.h"

class Member_actions_handler_configuration;

/**
  @class Member_actions

  The list of events on which a member action can be triggered.
*/
class Member_actions {
 public:
  virtual ~Member_actions() {}

  enum enum_action_event { AFTER_PRIMARY_ELECTION = 0 };

  /**
    Return a string representation of a member action event.

    @return string representation
      @retval !=""   Successful
      @retval ""     invalid event
  */
  static const std::string get_event_name(enum_action_event event) {
    switch (event) {
      case AFTER_PRIMARY_ELECTION:
        return "AFTER_PRIMARY_ELECTION";
      default:
        /* purecov: begin inspected */
        assert(0);
        return "";
        /* purecov: end */
    }
  }
};

/**
  @class Member_actions_trigger_parameters

  The event on which a member action is triggered, which will be
  a trigger parameter.
*/
class Member_actions_trigger_parameters : public Mysql_thread_body_parameters {
 public:
  Member_actions_trigger_parameters(Member_actions::enum_action_event event)
      : m_event(event) {}

  Member_actions_trigger_parameters(const Member_actions_trigger_parameters &o)
      : m_event(o.m_event) {}

  virtual ~Member_actions_trigger_parameters() {}

  Member_actions::enum_action_event get_event() { return m_event; }

 private:
  Member_actions::enum_action_event m_event;
};

/**
  @class Member_actions_handler

  Handles member actions configuration and trigger.
*/
class Member_actions_handler : public Mysql_thread_body,
                               public Configuration_propagation {
 public:
  Member_actions_handler();

  virtual ~Member_actions_handler() override;

  /**
    Initialize the handler.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool init();

  /**
    De-initialize the handler.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool deinit();

  /**
    Acquires a reference to the
    `group_replication_message_service_send` service.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool acquire_send_service();

  /**
    Releases the reference to the
    `group_replication_message_service_send` service.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool release_send_service();

  /**
    Delivery method for the `group_replication_message_service_recv`
    service.
    A error on this method will move the member into ERROR state and
    follow the --group_replication_exit_state_action option.

    @param[in]  tag         the message tag
    @param[in]  data        the message data
    @param[in]  data_length the message data length

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool receive(const char *tag, const unsigned char *data, size_t data_length);

  /**
    Enable a member action for a given event.

    @param[in]  name        the action name
    @param[in]  event       the event on which the action will be triggered

    @return std::pair<bool, std::string> where each element has the
            following meaning:
              first element of the pair is the function error value:
                false  Successful
                true   Error
              second element of the pair is the error message.
  */
  std::pair<bool, std::string> enable_action(const std::string &name,
                                             const std::string &event);

  /**
    Disable a member action for a given event.

    @param[in]  name        the action name
    @param[in]  event       the event on which the action will be triggered

    @return std::pair<bool, std::string> where each element has the
            following meaning:
              first element of the pair is the function error value:
                false  Successful
                true   Error
              second element of the pair is the error message.
  */
  std::pair<bool, std::string> disable_action(const std::string &name,
                                              const std::string &event);

  /**
    Reset member actions to the default configuration.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool reset_to_default_actions_configuration();

  /**
    Replace member actions configuration with the given one.

    @param[in]  exchanged_members_actions_serialized_configuration
                            the serialized configuration

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool replace_all_actions(
      const std::vector<std::string>
          &exchanged_members_actions_serialized_configuration);

  /**
    Retrieve member actions configuration in the serialized
    format.

    @param[out] serialized_configuration
                            the serialized configuration

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool get_all_actions(std::string &serialized_configuration);

  /**
    Propagate the local member actions configuration to the group,
    enabling the force_update flag, which will make the sent
    configuration to override other members configuration.

    @return Operation status
      @retval false  Successful
      @retval true   Error
  */
  bool force_my_actions_configuration_on_all_members();

  /**
    Trigger the actions configured to run on the given event.

    @param[in] event  the event that did trigger the member actions
  */
  void trigger_actions(Member_actions::enum_action_event event);

  /**
    Run the actions that were triggered.

    @param[in] parameters  the actions parameters
  */
  void run(Mysql_thread_body_parameters *parameters) override;

 private:
  /**
    Propagate the serialized configuration to the group.

    @param[in] serialized_configuration
                            the serialized configuration

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  bool propagate_serialized_configuration(
      const std::string &serialized_configuration) override;

  /**
    Run a INTERNAL action.

    @param[in] action      action configuration

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  int run_internal_action(
      const protobuf_replication_group_member_actions::Action &action);

  /**
    The tag used on the messages sent by this class.
  */
  const char *m_message_tag{"mysql_replication_group_member_actions"};

  /**
    The name of the message service listener.
  */
  const char *m_message_service_listener_name{
      "group_replication_message_service_recv.replication_group_member_"
      "actions"};

  /**
    The table configuration abstraction layer.
  */
  Member_actions_handler_configuration *m_configuration{nullptr};

  /**
    Single thread executor.
  */
  Mysql_thread *m_mysql_thread{nullptr};

  /**
    Pointer to the `group_replication_message_service_send` service.
  */
  SERVICE_TYPE_NO_CONST(group_replication_message_service_send) *
      m_group_replication_message_service_send{nullptr};
};

#endif /* MEMBER_ACTIONS_HANDLER_INCLUDED */
