/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "my_macros.h"
#include "mysql/psi/mysql_memory.h"

#include "sql/package/package.h"
#include "sql/package/package_common.h"
#include "sql/package/package_parse.h"
#include "sql/package/proc.h"
#include "sql/sp_head.h"

#ifndef NDEBUG
#include "sql/package/proc_dummy.h"
#endif
#ifdef HAVE_DUCKDB
#include "sql/duckdb/duckdb_proc.h"
#endif
#include "sql/package/show_native_procedure.h"

namespace im {

/* All package memory usage aggregation point */
PSI_memory_key key_memory_package;

const char *PACKAGE_SCHEMA = "mysql";

static bool package_inited = false;

#ifdef HAVE_PSI_INTERFACE
static PSI_memory_info package_memory[] = {
    {&key_memory_package, "im::package", 0, 0, PSI_DOCUMENT_ME}};

static void init_package_psi_key() {
  const char *category = "sql";
  int count;

  count = static_cast<int>(array_elements(package_memory));
  mysql_memory_register(category, package_memory, count);
}
#endif

/* Register all the native package element */
template <typename K, typename T>
static void register_package(const LEX_CSTRING &schema) {
  if (package_inited) {
    Package::instance()->register_element<K>(
        std::string(schema.str), T::instance()->str(), T::instance());
  }
}

/* Template of search package element */
template <typename T>
static const T *find_package_element(const std::string &schema_name,
                                     const std::string &element_name) {
  return Package::instance()->lookup_element<T>(schema_name, element_name);
}
/**
  whether exist native proc by schema_name and proc_name

  @retval       true              Exist
  @retval       false             Not exist
*/
bool exist_native_proc(const char *db, const char *name) {
  return find_package_element<Proc>(std::string(db), std::string(name)) ? true
                                                                        : false;
}
/**
  Find the native proc and evoke the parse tree root

  @param[in]    THD               Thread context
  @param[in]    sp_name           Proc name
  @param[in]    pt_expr_list      Parameters

  @retval       parse_tree_root   Parser structure
*/
Parse_tree_root *find_native_proc_and_evoke(THD *thd, sp_name *sp_name,
                                            PT_item_list *pt_expr_list) {
  const Proc *proc = find_package_element<Proc>(
      std::string(sp_name->m_db.str), std::string(sp_name->m_name.str));

  return proc ? proc->PT_evoke(thd, pt_expr_list, proc) : nullptr;
}

/**
  Initialize Package context.
*/
void package_context_init() {
#ifdef HAVE_PSI_INTERFACE
  init_package_psi_key();
#endif

  package_inited = true;

#ifndef NDEBUG
  register_package<Proc, Proc_dummy>(PROC_DUMMY_SCHEMA);
  register_package<Proc, Proc_dummy_2>(PROC_DUMMY_SCHEMA);
#endif

  /* dbms_duckdb.query() */
#ifdef HAVE_DUCKDB
  register_package<Proc, myduck::Duckdb_proc_query>(myduck::DUCKDB_PROC_SCHEMA);
#endif

  /* dbms_admin.show_native_procedure() */
  register_package<Proc, im::Show_native_procedure_proc>(im::ADMIN_PROC_SCHEMA);
}

} /* namespace im */
