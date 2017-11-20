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

#include "sql/dd/dd.h"

#include "sql/dd/impl/cache/shared_dictionary_cache.h" // dd::cache::Shared_...
#include "sql/dd/impl/dictionary_impl.h"            // dd::Dictionary_impl
#include "sql/dd/impl/system_registry.h"            // dd::System_tables

namespace dd {

bool init(enum_dd_init_type dd_init)
{
  if (dd_init == enum_dd_init_type::DD_INITIALIZE ||
      dd_init == enum_dd_init_type::DD_RESTART_OR_UPGRADE)
  {
    cache::Shared_dictionary_cache::init();
    System_tables::instance()->init();
    System_views::instance()->init();
  }

  return Dictionary_impl::init(dd_init);
}

///////////////////////////////////////////////////////////////////////////

bool shutdown()
{
  cache::Shared_dictionary_cache::shutdown();
  return Dictionary_impl::shutdown();
}


Dictionary *get_dictionary()
{
  return Dictionary_impl::instance();
}


} // namespace dd
