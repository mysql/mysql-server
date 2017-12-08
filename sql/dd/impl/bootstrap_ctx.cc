/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/bootstrap_ctx.h"        // dd::DD_bootstrap_ctx
#include "sql/dd/impl/tables/dd_properties.h" // dd::tables::DD_properties

namespace dd {
namespace bootstrap {

DD_bootstrap_ctx &DD_bootstrap_ctx::instance()
{
  static DD_bootstrap_ctx s_instance;
  return s_instance;
}

bool DD_bootstrap_ctx::is_above_minor_downgrade_threshold(THD *thd) const
{
  uint minor_downgrade_threshold= 0;
  bool exists= false;
  /*
    If we successfully get hold of the threshold, and it exists, and
    the target DD version is above or equal to the threshold, then we
    return true.
  */
  return (!dd::tables::DD_properties::instance().get(thd,
          "MINOR_DOWNGRADE_THRESHOLD",
          &minor_downgrade_threshold, &exists) &&
          exists &&
          dd::DD_VERSION >= minor_downgrade_threshold);
}

}
}