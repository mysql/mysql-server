/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/components/services/group_member_status_listener.h>
#include <mysql/components/services/group_membership_listener.h>
#include <mysql/components/services/log_builtins.h>

#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/services/notification/notification.h"

enum SvcTypes { kGroupMembership = 0, kGroupMemberStatus };

typedef int (*svc_notify_func)(Notification_context &, my_h_service);

static int notify_group_membership(Notification_context &ctx,
                                   my_h_service svc) {
  int svc_ko = 0;
  const char *view_id = ctx.get_view_id().c_str();
  SERVICE_TYPE(group_membership_listener) *listener = NULL;

  /* now that we have the handler for it, notify */
  listener = reinterpret_cast<SERVICE_TYPE(group_membership_listener) *>(svc);

  if (ctx.get_view_changed()) {
    svc_ko = svc_ko + listener->notify_view_change(view_id);
  }

  if (ctx.get_quorum_lost()) {
    svc_ko = svc_ko + listener->notify_quorum_loss(view_id);
  }

  return svc_ko;
}

static int notify_group_member_status(Notification_context &ctx,
                                      my_h_service svc) {
  int svc_ko = 0;
  const char *view_id = ctx.get_view_id().c_str();
  SERVICE_TYPE(group_member_status_listener) *listener = NULL;

  /* now that we have the handler for it, notify */
  listener =
      reinterpret_cast<SERVICE_TYPE(group_member_status_listener) *>(svc);

  if (ctx.get_member_state_changed()) {
    svc_ko = svc_ko + listener->notify_member_state_change(view_id);
  }

  if (ctx.get_member_role_changed()) {
    svc_ko = svc_ko + listener->notify_member_role_change(view_id);
  }

  return svc_ko;
}

/**
  Auxiliary function to engage the service registry to
  notify a set of listeners.

  @param svc_type The service name.
  @param ctx The events context

  @return false on success, true otherwise.
 */
static bool notify(SvcTypes svc_type, Notification_context &ctx) {
  SERVICE_TYPE(registry) *r = NULL;
  SERVICE_TYPE(registry_query) *rq = NULL;
  my_h_service_iterator h_ret_it = NULL;
  my_h_service h_listener_svc = NULL;
  my_h_service h_listener_default_svc = NULL;
  bool res = false;
  bool default_notified = false;
  std::string svc_name;
  svc_notify_func notify_func_ptr;

  if (!registry_module || !(r = registry_module->get_registry_handle()) ||
      !(rq = registry_module->get_registry_query_handle()))
    goto err; /* purecov: inspected */

  /*
    Decides which listener service to notify, based on the
    service type. It also checks whether the service should
    be notified indeed, based on the event context.

    If the event is not to be notified, the function returns
    immediately.
   */
  switch (svc_type) {
    case kGroupMembership:
      notify_func_ptr = notify_group_membership;
      svc_name = Registry_module_interface::SVC_NAME_MEMBERSHIP;
      break;
    case kGroupMemberStatus:
      notify_func_ptr = notify_group_member_status;
      svc_name = Registry_module_interface::SVC_NAME_STATUS;
      break;
    default:
      DBUG_ASSERT(false); /* purecov: inspected */
      /* production builds default to membership */
      svc_name = Registry_module_interface::SVC_NAME_MEMBERSHIP;
      notify_func_ptr = notify_group_membership;
      break;
  }

  /* acquire the default service */
  if (r->acquire(svc_name.c_str(), &h_listener_default_svc) ||
      !h_listener_default_svc)
    /* no listener registered, skip */
    goto end;

  /*
    create iterator to navigate notification GMS change
    notification listeners
  */
  if (rq->create(svc_name.c_str(), &h_ret_it)) {
    goto err; /* purecov: inspected */
  }

  /* notify all listeners */
  while (h_ret_it != NULL &&
         /* is_valid returns false on success */
         rq->is_valid(h_ret_it) == false) {
    int svc_ko = 0;
    const char *next_svc_name = NULL;

    /* get next registered listener */
    if (rq->get(h_ret_it, &next_svc_name)) goto err; /* purecov: inspected */

    /*
      The iterator currently contains more service implementations than
      those named after the given service name. The spec says that the
      name given is used to position the iterator start on the first
      registered service implementation prefixed with that name. We need
      to iterate until the next element in the iterator (service implementation)
      has a different service name.
    */
    std::string s(next_svc_name);
    if (s.find(svc_name, 0) == std::string::npos) break;

    /* acquire next listener */
    if (r->acquire(next_svc_name, &h_listener_svc))
      goto err; /* purecov: inspected */

    /* don't notify the default service twice */
    if (h_listener_svc != h_listener_default_svc || !default_notified) {
      if (notify_func_ptr(ctx, h_listener_svc))
        LogPluginErr(WARNING_LEVEL,
                     ER_GRP_RPL_FAILED_TO_NOTIFY_GRP_MEMBERSHIP_EVENT,
                     next_svc_name); /* purecov: inspected */

      default_notified =
          default_notified || (h_listener_svc == h_listener_default_svc);
    }

    /* release the listener service */
    if (r->release(h_listener_svc) || svc_ko) goto err; /* purecov: inspected */

    /* update iterator */
    if (rq->next(h_ret_it)) goto err; /* purecov: inspected */
  }

end:
  /* release the iterator */
  if (h_ret_it) rq->release(h_ret_it);

  /* release the default service */
  if (h_listener_default_svc)
    if (r->release(h_listener_default_svc)) res = true; /* purecov: inspected */

  return res;

err:
  res = true; /* purecov: inspected */
  goto end;
}

/* Public Functions */

bool notify_and_reset_ctx(Notification_context &ctx) {
  bool res = false;

  if (ctx.get_view_changed() || ctx.get_quorum_lost()) {
    /* notify membership events listeners. */
    if (notify(kGroupMembership, ctx)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FAILED_TO_BROADCAST_GRP_MEMBERSHIP_NOTIFICATION);
      /* purecov: end */
      res = true; /* purecov: inspected */
    }
  }

  if (ctx.get_member_state_changed() || ctx.get_member_role_changed()) {
    /* notify member status events listeners. */
    if (notify(kGroupMemberStatus, ctx)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_FAILED_TO_BROADCAST_MEMBER_STATUS_NOTIFICATION);
      /* purecov: end */
      res = true; /* purecov: inspected */
    }
  }

  ctx.reset();
  return res;
}
