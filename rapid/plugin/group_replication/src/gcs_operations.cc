/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_operations.h"
#include "plugin.h"
#include "plugin_log.h"

#include <vector>


const std::string Gcs_operations::gcs_engine= "xcom";


Gcs_operations::Gcs_operations()
  : gcs_interface(NULL),
    leave_coordination_leaving(false),
    leave_coordination_left(false),
    finalize_ongoing(false)
{
  gcs_operations_lock= new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_gcs_operations
#endif
  );
  finalize_ongoing_lock= new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_gcs_operations_finalize_ongoing
#endif
  );
}


Gcs_operations::~Gcs_operations()
{
  delete gcs_operations_lock;
  delete finalize_ongoing_lock;
}


int
Gcs_operations::initialize()
{
  DBUG_ENTER("Gcs_operations::initialize");
  int error= 0;
  gcs_operations_lock->wrlock();

  leave_coordination_leaving= false;
  leave_coordination_left= false;

  DBUG_ASSERT(gcs_interface == NULL);
  if ((gcs_interface=
           Gcs_interface_factory::get_interface_implementation(
               gcs_engine)) == NULL)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Failure in group communication engine '%s' initialization",
                gcs_engine.c_str());
    error= GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR;
    goto end;
    /* purecov: end */
  }

  if (gcs_interface->set_logger(&gcs_logger))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Unable to set the group communication engine logger");
    error= GROUP_REPLICATION_COMMUNICATION_LAYER_SESSION_ERROR;
    goto end;
    /* purecov: end */
  }

end:
  gcs_operations_lock->unlock();
  DBUG_RETURN(error);
}


void
Gcs_operations::finalize()
{
  DBUG_ENTER("Gcs_operations::finalize");
  finalize_ongoing_lock->wrlock();
  finalize_ongoing= true;
  gcs_operations_lock->wrlock();
  finalize_ongoing_lock->unlock();

  if (gcs_interface != NULL)
    gcs_interface->finalize();
  Gcs_interface_factory::cleanup(gcs_engine);
  gcs_interface= NULL;

  finalize_ongoing_lock->wrlock();
  finalize_ongoing= false;
  gcs_operations_lock->unlock();
  finalize_ongoing_lock->unlock();
  DBUG_VOID_RETURN;
}


enum enum_gcs_error
Gcs_operations::configure(const Gcs_interface_parameters& parameters)
{
  DBUG_ENTER("Gcs_operations::configure");
  enum enum_gcs_error error= GCS_NOK;
  gcs_operations_lock->wrlock();

  if (gcs_interface != NULL)
    error= gcs_interface->initialize(parameters);

  gcs_operations_lock->unlock();
  DBUG_RETURN(error);
}


enum enum_gcs_error
Gcs_operations::join(const Gcs_communication_event_listener& communication_event_listener,
                     const Gcs_control_event_listener& control_event_listener)
{
  DBUG_ENTER("Gcs_operations::join");
  enum enum_gcs_error error= GCS_NOK;
  gcs_operations_lock->wrlock();

  if (gcs_interface == NULL || !gcs_interface->is_initialized())
  {
    /* purecov: begin inspected */
    gcs_operations_lock->unlock();
    DBUG_RETURN(GCS_NOK);
    /* purecov: end */
  }

  std::string group_name(group_name_var);
  Gcs_group_identifier group_id(group_name);

  Gcs_communication_interface *gcs_communication=
      gcs_interface->get_communication_session(group_id);
  Gcs_control_interface *gcs_control=
      gcs_interface->get_control_session(group_id);

  if (gcs_communication == NULL || gcs_control == NULL)
  {
    /* purecov: begin inspected */
    gcs_operations_lock->unlock();
    DBUG_RETURN(GCS_NOK);
    /* purecov: end */
  }

  gcs_control->add_event_listener(control_event_listener);
  gcs_communication->add_event_listener(communication_event_listener);

  /*
    Fake a GCS join error by not invoking join(), the
    view_change_notifier will error out and return a error on
    START GROUP_REPLICATION command.
  */
  DBUG_EXECUTE_IF("group_replication_inject_gcs_join_error",
                  { gcs_operations_lock->unlock(); DBUG_RETURN(GCS_OK); };);

  error= gcs_control->join();

  gcs_operations_lock->unlock();
  DBUG_RETURN(error);
}


bool
Gcs_operations::belongs_to_group()
{
  DBUG_ENTER("Gcs_operations::belongs_to_group");
  bool res= false;
  gcs_operations_lock->rdlock();

  if (gcs_interface != NULL && gcs_interface->is_initialized())
  {
    std::string group_name(group_name_var);
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control=
        gcs_interface->get_control_session(group_id);

    if (gcs_control != NULL && gcs_control->belongs_to_group())
      res= true;
  }

  gcs_operations_lock->unlock();
  DBUG_RETURN(res);
}


Gcs_operations::enum_leave_state
Gcs_operations::leave()
{
  DBUG_ENTER("Gcs_operations::leave");
  enum_leave_state state= ERROR_WHEN_LEAVING;
  gcs_operations_lock->wrlock();

  if (leave_coordination_left)
  {
    state= ALREADY_LEFT;
    goto end;
  }
  if (leave_coordination_leaving)
  {
    state= ALREADY_LEAVING;
    goto end;
  }

  if (gcs_interface != NULL && gcs_interface->is_initialized())
  {
    std::string group_name(group_name_var);
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control=
        gcs_interface->get_control_session(group_id);

    if (gcs_control != NULL)
    {
      if (!gcs_control->leave())
      {
        state= NOW_LEAVING;
        leave_coordination_leaving= true;
        goto end;
      }
    }
    else
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "Error calling group communication interfaces while trying"
                  " to leave the group");
      goto end;
      /* purecov: end */
    }
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Error calling group communication interfaces while trying"
                " to leave the group");
    goto end;
  }

end:
  gcs_operations_lock->unlock();
  DBUG_RETURN(state);
}


void
Gcs_operations::leave_coordination_member_left()
{
  DBUG_ENTER("Gcs_operations::leave_coordination_member_left");

  /*
    If finalize method is ongoing, it means that GCS is waiting that
    all messages and views are delivered to GR, if we proceed with
    this method we will enter on the deadlock:
      1) leave view was not delivered before wait view timeout;
      2) finalize did start and acquired lock->wrlock();
      3) leave view was delivered, member_left is waiting to
         acquire lock->wrlock().
    So, if leaving, we just do nothing.
  */
  finalize_ongoing_lock->rdlock();
  if (finalize_ongoing)
  {
    finalize_ongoing_lock->unlock();
    DBUG_VOID_RETURN;
  }
  gcs_operations_lock->wrlock();
  finalize_ongoing_lock->unlock();

  leave_coordination_leaving= false;
  leave_coordination_left= true;

  gcs_operations_lock->unlock();
  DBUG_VOID_RETURN;
}


Gcs_view*
Gcs_operations::get_current_view()
{
  DBUG_ENTER("Gcs_operations::get_current_view");
  Gcs_view *view= NULL;
  gcs_operations_lock->rdlock();

  if (gcs_interface != NULL && gcs_interface->is_initialized())
  {
    std::string group_name(group_name_var);
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control=
        gcs_interface->get_control_session(group_id);

    if (gcs_control != NULL && gcs_control->belongs_to_group())
      view= gcs_control->get_current_view();
  }

  gcs_operations_lock->unlock();
  DBUG_RETURN(view);
}


int
Gcs_operations::get_local_member_identifier(std::string& identifier)
{
  DBUG_ENTER("Gcs_operations::get_local_member_identifier");
  int error= 1;
  gcs_operations_lock->rdlock();

  if (gcs_interface != NULL && gcs_interface->is_initialized())
  {
    std::string group_name(group_name_var);
    Gcs_group_identifier group_id(group_name);
    Gcs_control_interface *gcs_control=
        gcs_interface->get_control_session(group_id);

    if (gcs_control != NULL)
    {
      identifier.assign(gcs_control->get_local_member_identifier().get_member_id());
      error= 0;
    }
  }

  gcs_operations_lock->unlock();
  DBUG_RETURN(error);
}


enum enum_gcs_error
Gcs_operations::send_message(const Plugin_gcs_message& message,
                             bool skip_if_not_initialized)
{
  DBUG_ENTER("Gcs_operations::send");
  enum enum_gcs_error error= GCS_NOK;
  gcs_operations_lock->rdlock();

  /*
    Ensure that group communication interfaces are initialized
    and ready to use, since plugin can leave the group on errors
    but continue to be active.
  */
  if (gcs_interface == NULL || !gcs_interface->is_initialized())
  {
    gcs_operations_lock->unlock();
    DBUG_RETURN(skip_if_not_initialized ? GCS_OK : GCS_NOK);
  }

  std::string group_name(group_name_var);
  Gcs_group_identifier group_id(group_name);

  Gcs_communication_interface *gcs_communication=
      gcs_interface->get_communication_session(group_id);
  Gcs_control_interface *gcs_control=
      gcs_interface->get_control_session(group_id);

  if (gcs_communication == NULL || gcs_control == NULL)
  {
    /* purecov: begin inspected */
    gcs_operations_lock->unlock();
    DBUG_RETURN(skip_if_not_initialized ? GCS_OK : GCS_NOK);
    /* purecov: end */
  }

  std::vector<uchar> message_data;
  message.encode(&message_data);

  Gcs_member_identifier origin= gcs_control->get_local_member_identifier();
  Gcs_message gcs_message(origin, new Gcs_message_data(0, message_data.size()));
  gcs_message.get_message_data().append_to_payload(&message_data.front(),
                                                   message_data.size());
  error= gcs_communication->send_message(gcs_message);

  gcs_operations_lock->unlock();
  DBUG_RETURN(error);
}


int
Gcs_operations::force_members(const char* members)
{
  DBUG_ENTER("Gcs_operations::force_members");
  int error= 0;
  gcs_operations_lock->wrlock();

  if (gcs_interface == NULL || !gcs_interface->is_initialized())
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Member is OFFLINE, it is not possible to force a "
                "new group membership");
    error= 1;
    goto end;
    /* purecov: end */
  }

  if (local_member_info->get_recovery_status() == Group_member_info::MEMBER_ONLINE)
  {
    std::string group_id_str(group_name_var);
    Gcs_group_identifier group_id(group_id_str);
    Gcs_group_management_interface* gcs_management=
        gcs_interface->get_management_session(group_id);

    if (gcs_management == NULL)
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "Error calling group communication interfaces");
      error= 1;
      goto end;
      /* purecov: end */
    }

    view_change_notifier->start_injected_view_modification();

    Gcs_interface_parameters gcs_interface_parameters;
    gcs_interface_parameters.add_parameter("peer_nodes",
                                           std::string(members));
    enum_gcs_error result=
        gcs_management->modify_configuration(gcs_interface_parameters);
    if (result != GCS_OK)
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "Error setting group_replication_force_members "
                  "value '%s' on group communication interfaces", members);
      error= 1;
      goto end;
      /* purecov: end */
    }
    log_message(MY_INFORMATION_LEVEL,
                "The group_replication_force_members value '%s' "
                "was set in the group communication interfaces", members);
    if (view_change_notifier->wait_for_view_modification())
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "Timeout on wait for view after setting "
                  "group_replication_force_members value '%s' "
                  "into group communication interfaces", members);
      error= 1;
      goto end;
      /* purecov: end */
    }
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "Member is not ONLINE, it is not possible to force a "
                "new group membership");
    error= 1;
    goto end;
  }

end:
  gcs_operations_lock->unlock();
  DBUG_RETURN(error);
}

const std::string& Gcs_operations::get_gcs_engine()
{
  return gcs_engine;
}
