/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__SDI_UTILS_INCLUDED
#define DD__SDI_UTILS_INCLUDED

#include "current_thd.h"             // inline_current_thd
#include "dd/string_type.h"          // dd::String_type
#include "mdl.h"                     // MDL_request
#include "my_dbug.h"
#include "my_global.h"
#include "sql_class.h"               // THD

/**
  @file
  @ingroup sdi

  Inline utility functions used in different
  TUs. Declared inline in header to avoid any overhead as they are
  only a syntactic convnience (macro replacement).
*/

namespace dd {
namespace sdi_utils {

/**
  In debug mode, check that a true argument is paired with
  thd->is_error() or thd->killed being set. In optimized mode it turns into
  a noop.
  @param[in] ret return value to check
  @return same as argument passed in
 */
inline bool checked_return(bool ret)
{
#ifndef DBUG_OFF
  THD *cthd= inline_current_thd();
  DBUG_ASSERT(!ret || cthd->is_error() || cthd->killed);
#endif /*!DBUG_OFF*/
  return ret;
}


/**
  Convenience function for obtaint an MDL. Sets up the MDL_request
  struct and populates it, before calling Mdl_context::acquire_lock.

  @param thd
  @param ns MDL key namespace
  @param schema_name
  @param object_name
  @param mt MDL type
  @param md MDL duration
  @return value from Mdl_context::acquire_lock
 */
inline bool mdl_lock(THD *thd, MDL_key::enum_mdl_namespace ns,
                     const String_type &schema_name,
                     const String_type &object_name,
                     enum_mdl_type mt = MDL_EXCLUSIVE,
                     enum_mdl_duration md = MDL_TRANSACTION)
{
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, ns, schema_name.c_str(), object_name.c_str(),
                   mt, md);
  return checked_return
    (thd->mdl_context.acquire_lock(&mdl_request,
                                   thd->variables.lock_wait_timeout));
}
} // namespace sdi_utils
} // namespace dd
#endif // DD__SDI_UTILS_INCLUDED
