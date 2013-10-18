/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include <gcs_protocol.h>
#include <gcs_corosync.h>
#include <gcs_protocol_factory.h>
#include "my_global.h"

namespace GCS
{

/*
  The current factory implementaion is limitted with
  just one possible instance.
  Consider a container if more instances will be required.
*/
static Protocol* single_instance= NULL;


Protocol*
Protocol_factory::create_protocol(Protocol_type type,
                                  Stats& stats_collector)
{
  DBUG_ENTER("GCS::Protocol_factory::create_protocol");

  switch (type) {
  case PROTO_COROSYNC:

    DBUG_ASSERT(!single_instance); // an creator can't do it 2nd time.

    DBUG_RETURN(single_instance= new Protocol_corosync(stats_collector));

  default:
    DBUG_ASSERT(0);
  }

  DBUG_RETURN(NULL);
}

Protocol* Protocol_factory::get_instance()
{
  DBUG_ASSERT(single_instance);
  return single_instance;
}

} // namespace

