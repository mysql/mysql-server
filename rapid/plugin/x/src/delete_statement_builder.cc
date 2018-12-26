/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "delete_statement_builder.h"
#include "ngs_common/protocol_protobuf.h"


void xpl::Delete_statement_builder::build(const Delete &msg) const
{
  m_builder.put("DELETE FROM ");
  add_collection(msg.collection());
  add_filter(msg.criteria());
  add_order(msg.order());
  add_limit(msg.limit(), true);
}
