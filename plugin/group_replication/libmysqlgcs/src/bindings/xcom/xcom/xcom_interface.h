/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef XCOM_INTERFACE_H
#define XCOM_INTERFACE_H

#include "xcom/site_struct.h"
#include "xcom/xcom_cache.h"

void deliver_view_msg(site_def const *site);
void deliver_global_view_msg(site_def const *site, node_set const ns,
                             synode_no message_id);

/**
  Delivers the @c app payload to XCom's upper layer, e.g. GCS.

  Please note the following contract regarding the parameter of this function:
  If @c app_status is @c delivery_ok, then it means that XCom reached
  consensus on @c app. In this situation, @c pma MUST point to the Paxos
  instance (@c pax_machine) where @c app got consensus on. In other words, @c
  pma MUST NOT be NULL.

  In summary, this function is expected to be called as follows if a payload
  @c p reached consensus:

    deliver_to_app(<pointer to pax_machine where p was decided>, p, delivery_ok)

  This function is expected to be called as follows if a payload @c p did NOT
  reach consensus:

    deliver_to_app(_, p, delivery_failure)

  Where the @c pma argument may be a pointer to the pax_machine where @c p
  failed to get consensus, or NULL.

  Failure do adhere to this contract will result in an assert failure, at least
  on non-release builds.

  @param pma The Paxos instance where the @c app payload obtained consensus
  @param app The payload that was decided for the Paxos instance @c pma
  @param app_status The reason for @c app delivery: @c delivery_ok if the @c app
  payload reached consensus on the @c pma Paxos instance, or @c delivery_failure
  if XCom was unable to reach consensus
 */
void deliver_to_app(pax_machine *pma, app_data_ptr app,
                    delivery_status app_status);

void deliver_config(app_data_ptr a);

void deinit_xcom_interface();

#endif
