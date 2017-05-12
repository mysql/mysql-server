/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#include "sql_import.h"

#include "mdl.h"                   // MDL_request
#include "my_dir.h"                // my_dir
#include "my_sys.h"                // dirname_part
#include "mysqld.h"                // is_secure_file_path
#include "mysqld_error.h"          // my_error
#include "prealloced_array.h"      // Prealloced_array
#include "psi_memory_key.h"        // key_memory_DD_import
#include "sql_class.h"             // THD
#include "sql_table.h"             // write_bin_log
#include "transaction.h"           // trans_rollback_stmt
#include "mysql/psi/mysql_file.h"  // mysql_file_x

#include "dd/string_type.h"        // dd::String_type
#include "dd/sdi_api.h"            // dd::sdi::Import_target
#include "dd/sdi_file.h"           // dd::sdi_file::expand_pattern

#include "dd/cache/dictionary_client.h" // dd::cache::Dictionary_client::Auto_releaser
#include "dd/types/table.h"        // dd::Table
#include "dd/impl/sdi_utils.h"     // dd::sdi_utils::handle_errors

namespace {

typedef Prealloced_array<dd::sdi::Import_target, 5> Targets_type;

template <typename P_TYPE, typename CLOS_TYPE>
std::unique_ptr<P_TYPE, CLOS_TYPE> make_guard(P_TYPE *p, CLOS_TYPE &&clos)
{
  return std::unique_ptr<P_TYPE, CLOS_TYPE>(p, std::forward<CLOS_TYPE>(clos));
}
} // namepspace


Sql_cmd_import_table::Sql_cmd_import_table(const Sdi_patterns_type
                                           &sdi_patterns)
  : m_sdi_patterns(sdi_patterns)
{}

bool Sql_cmd_import_table::execute(THD *thd)
{
  DBUG_ASSERT(!m_sdi_patterns.empty());

  auto rbgrd= make_guard(thd, [] (THD *thd) {
      trans_rollback_stmt(thd);
      trans_rollback(thd);
    });

  // Need to keep this alive until after commit/rollback has been done
  dd::cache::Dictionary_client::Auto_releaser ar{thd->dd_client()};

  if (check_access(thd, FILE_ACL, nullptr, nullptr, nullptr, FALSE, FALSE))
  {
    return true;
  }

  // Convert supplied sdi patterns into path,in_datadir pairs
  dd::sdi_file::Paths_type paths{key_memory_DD_import};
  paths.reserve(m_sdi_patterns.size());
  for (auto &pattern : m_sdi_patterns)
  {
    if (thd->charset() == files_charset_info)
    {
      if (dd::sdi_file::expand_pattern(thd, pattern, &paths))
      {
        return true;
      }
      continue;
    }

    LEX_STRING converted;
    if (thd->convert_string(&converted, files_charset_info,
                            pattern.str, pattern.length, thd->charset()))
    {
      return true;
    }

    if (dd::sdi_file::expand_pattern(thd, converted, &paths))
    {
      return true;
    }
  }

  Targets_type targets{key_memory_DD_import};

  auto tgtgrd=
    make_guard(thd, [&] (THD*)
               {
                 for (auto &tgt : targets)
                 {
                   tgt.rollback();
                 }
               });

  for (auto &p : paths)
  {
    // Move the path string from paths to avoid copy - paths is now
    // empty shell
    targets.emplace_back(std::move(p.first), p.second);
  }
  // Have a valid list of sdi files to import

  dd::String_type shared_buffer;
  MDL_request_list mdl_requests;
  for (auto &t : targets)
  {
    if (t.load(thd, &shared_buffer))
    {
      return true;
    }

    if (check_privileges(thd, t))
    {
      return true;
    }

    mdl_requests.push_front(mdl_request(t, thd->mem_root));
  }
  // Table objects and their schema names have been loaded, privileges
  // checked and EXCLUSIVE MDL requests for the tables been added to
  // mdl_requests.

  std::vector<dd::String_type> schema_names;
  schema_names.reserve(targets.size());
  for (auto &t : targets)
  {
    schema_names.push_back(*t.can_schema_name());
  }
  std::sort(schema_names.begin(), schema_names.end());
  std::unique(schema_names.begin(), schema_names.end());

  for (auto &sn : schema_names)
  {
    MDL_request *r= new (thd->mem_root) MDL_request;
    MDL_REQUEST_INIT(r, MDL_key::SCHEMA, sn.c_str(), "",
                     MDL_INTENTION_EXCLUSIVE, MDL_TRANSACTION);
    mdl_requests.push_front(r);
  }

  if (thd->mdl_context.acquire_locks(&mdl_requests,
                                     thd->variables.lock_wait_timeout))
  {
    return true;
  }
  // Now we have MDL on all schemas and tables involved

  for (auto &t : targets)
  {
    if (t.store_in_dd(thd))
    {
      return true;
    }
  }

  rbgrd.release();
  tgtgrd.release();

  // Downgrade failing delete_file errors to warning, and
  // allow the transaction to commit.
  dd::sdi_utils::handle_errors
    (thd,
     [] (uint, const char*,
         Sql_condition::enum_severity_level *level, const char*)
     {
       (*level)= Sql_condition::SL_WARNING;
       return false;
     },
     [&] () {
       for (auto &tgt : targets)
       {
         (void) tgt.commit();
       }
       return false;
     });

  my_ok(thd);
  return trans_commit_stmt(thd) || trans_commit(thd);
}

enum_sql_command Sql_cmd_import_table::sql_command_code() const
{
  return SQLCOM_IMPORT;
}
