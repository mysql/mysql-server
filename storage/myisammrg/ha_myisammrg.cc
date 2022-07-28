/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/*
  MyISAM MERGE tables

  A MyISAM MERGE table is kind of a union of zero or more MyISAM tables.

  Besides the normal form file (.frm) a MERGE table has a meta file
  (.MRG) with a list of tables. These are paths to the MyISAM table
  files. The last two components of the path contain the database name
  and the table name respectively.

  When a MERGE table is open, there exists an TABLE object for the MERGE
  table itself and a TABLE object for each of the MyISAM tables. For
  abbreviated writing, I call the MERGE table object "parent" and the
  MyISAM table objects "children".

  A MERGE table is almost always opened through open_and_lock_tables()
  and hence through open_tables(). When the parent appears in the list
  of tables to open, the initial open of the handler does nothing but
  read the meta file and collect a list of TABLE_LIST objects for the
  children. This list is attached to the handler object as
  ha_myisammrg::children_l. The end of the children list is saved in
  ha_myisammrg::children_last_l.

  Back in open_tables(), handler::extra(HA_EXTRA_ADD_CHILDREN_LIST) is
  called. It updates each list member with the lock type and a back
  pointer to the parent TABLE_LIST object TABLE_LIST::parent_l. The list
  is then inserted in the list of tables to open, right behind the
  parent. Consequently, open_tables() opens the children, one after the
  other. The TABLE references of the TABLE_LIST objects are implicitly
  set to the open tables by open_table(). The children are opened as
  independent MyISAM tables, right as if they are used by the SQL
  statement.

  When all tables from the statement query list are open,
  handler::extra(HA_EXTRA_ATTACH_CHILDREN) is called. It "attaches" the
  children to the parent. All required references between parent and
  children are set up.

  The MERGE storage engine sets up an array with references to the
  low-level MyISAM table objects (MI_INFO). It remembers the state of
  the table in MYRG_INFO::children_attached.

  If necessary, the compatibility of parent and children is checked.
  This check is necessary when any of the objects are reopend. This is
  detected by comparing the current table def version against the
  remembered child def version. On parent open, the list members are
  initialized to an "impossible"/"undefined" version value. So the check
  is always executed on the first attach.

  The version check is done in myisammrg_attach_children_callback(),
  which is called for every child. ha_myisammrg::attach_children()
  initializes 'need_compat_check' to false and
  myisammrg_attach_children_callback() sets it to true if a table
  def version mismatches the remembered child def version.

  The children chain remains in the statement query list until the table
  is closed or the children are detached. This is done so that the
  children are locked by lock_tables().

  At statement end the children are detached. At the next statement
  begin the open-add-attach sequence repeats. There is no exception for
  LOCK TABLES. The fresh establishment of the parent-child relationship
  before every statement catches numerous cases of ALTER/FLUSH/DROP/etc
  of parent or children during LOCK TABLES.

  ---

  On parent open the storage engine structures are allocated and initialized.
  They stay with the open table until its final close.
*/

#define MYSQL_SERVER 1
#include "storage/myisammrg/ha_myisammrg.h"

#include "my_config.h"

#include <mysql/plugin.h>
#include <algorithm>

#include "m_ctype.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_pointer_arithmetic.h"
#include "my_psi_config.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"
#include "sql/sql_show.h"    // append_identifier
#include "sql/sql_table.h"   // build_table_filename
#include "sql/thr_malloc.h"  // int_sql_alloc
#include "storage/myisam/ha_myisam.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisammrg/myrg_def.h"
#include "typelib.h"

using std::max;
using std::min;

static handler *myisammrg_create_handler(handlerton *hton, TABLE_SHARE *table,
                                         bool, MEM_ROOT *mem_root) {
  return new (mem_root) ha_myisammrg(hton, table);
}

/**
  @brief Constructor
*/

ha_myisammrg::ha_myisammrg(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg), file(nullptr), is_cloned(false) {}

static const char *ha_myisammrg_exts[] = {".MRG", NullS};
static void split_file_name(const char *file_name, LEX_CSTRING *db,
                            LEX_CSTRING *name);

void myrg_print_wrong_table(const char *table_name) {
  LEX_CSTRING db = {nullptr, 0}, name;
  char buf[FN_REFLEN];
  split_file_name(table_name, &db, &name);
  memcpy(buf, db.str, db.length);
  buf[db.length] = '.';
  memcpy(buf + db.length + 1, name.str, name.length);
  buf[db.length + name.length + 1] = 0;
  /*
    Push an error to be reported as part of CHECK/REPAIR result-set.
    Note that calling my_error() from handler is a hack which is kept
    here to avoid refactoring. Normally engines should report errors
    through return value which will be interpreted by caller using
    handler::print_error() call.
  */
  my_error(ER_ADMIN_WRONG_MRG_TABLE, MYF(0), buf);
}

namespace {

/**
  Callback function for open of a MERGE parent table.

  @param[in]    callback_param  data pointer as given to myrg_parent_open()
                                this is used to pass the handler handle
  @param[in]    filename        file name of MyISAM table
                                without extension.

  @return status
    @retval     0               OK
    @retval     != 0            Error

    This function adds a TABLE_LIST object for a MERGE child table to a
    list of tables in the parent handler object. It is called for each
    child table.

    The list of child TABLE_LIST objects is kept in the handler object
    of the parent for the whole life time of the MERGE table. It is
    inserted in the statement query list behind the MERGE parent
    TABLE_LIST object when the MERGE table is opened. It is removed from
    the statement query list at end of statement or at children detach.

    All memory used for the child TABLE_LIST objects and the strings
    referred by it are taken from the parent
    ha_myisammrg::children_mem_root. Thus they are all freed implicitly at
    the final close of the table.

    children_l -> TABLE_LIST::next_global -> TABLE_LIST::next_global
    #             #               ^          #               ^
    #             #               |          #               |
    #             #               +--------- TABLE_LIST::prev_global
    #             #                                          |
    #       |<--- TABLE_LIST::prev_global                    |
    #                                                        |
    children_last_l -----------------------------------------+
*/

extern "C" int myisammrg_parent_open_callback(void *callback_param,
                                              const char *filename) {
  ha_myisammrg *ha_myrg = (ha_myisammrg *)callback_param;
  TABLE *parent = ha_myrg->table_ptr();
  Mrg_child_def *mrg_child_def;
  char *db;
  char *table_name;
  size_t dirlen;
  size_t db_length;
  size_t table_name_length;
  char dir_path[FN_REFLEN];
  char name_buf[NAME_LEN];
  DBUG_TRACE;

  /*
    Depending on MySQL version, filename may be encoded by table name to
    file name encoding or not. Always encoded if parent table is created
    by 5.1.46+. Encoded if parent is created by 5.1.6+ and child table is
    in different database.
  */
  if (!has_path(filename)) {
    /* Child is in the same database as parent. */
    db_length = parent->s->db.length;
    db =
        strmake_root(&ha_myrg->children_mem_root, parent->s->db.str, db_length);
    /* Child table name is encoded in parent dot-MRG starting with 5.1.46. */
    if (parent->s->mysql_version >= 50146) {
      table_name_length =
          filename_to_tablename(filename, name_buf, sizeof(name_buf));
      table_name = strmake_root(&ha_myrg->children_mem_root, name_buf,
                                table_name_length);
    } else {
      table_name_length = strlen(filename);
      table_name = strmake_root(&ha_myrg->children_mem_root, filename,
                                table_name_length);
    }
  } else {
    assert(strlen(filename) < sizeof(dir_path));
    fn_format(dir_path, filename, "", "", 0);
    /* Extract child table name and database name from filename. */
    dirlen = dirname_length(dir_path);
    /* Child db/table name is encoded in parent dot-MRG starting with 5.1.6. */
    if (parent->s->mysql_version >= 50106) {
      table_name_length =
          filename_to_tablename(dir_path + dirlen, name_buf, sizeof(name_buf));
      table_name = strmake_root(&ha_myrg->children_mem_root, name_buf,
                                table_name_length);
      dir_path[dirlen - 1] = 0;
      dirlen = dirname_length(dir_path);
      db_length =
          filename_to_tablename(dir_path + dirlen, name_buf, sizeof(name_buf));
      db = strmake_root(&ha_myrg->children_mem_root, name_buf, db_length);
    } else {
      table_name_length = strlen(dir_path + dirlen);
      table_name = strmake_root(&ha_myrg->children_mem_root, dir_path + dirlen,
                                table_name_length);
      dir_path[dirlen - 1] = 0;
      dirlen = dirname_length(dir_path);
      db_length = strlen(dir_path + dirlen);
      db = strmake_root(&ha_myrg->children_mem_root, dir_path + dirlen,
                        db_length);
    }
  }

  if (!db || !table_name) return 1;

  DBUG_PRINT("myrg", ("open: '%.*s'.'%.*s'", (int)db_length, db,
                      (int)table_name_length, table_name));

  /* Convert to lowercase if required. */
  if (lower_case_table_names && table_name_length) {
    /* purecov: begin tested */
    table_name_length = my_casedn_str(files_charset_info, table_name);
    /* purecov: end */
  }

  mrg_child_def = new (&ha_myrg->children_mem_root)
      Mrg_child_def(db, db_length, table_name, table_name_length);

  if (!mrg_child_def || ha_myrg->child_def_list.push_back(
                            mrg_child_def, &ha_myrg->children_mem_root)) {
    return 1;
  }
  return 0;
}

}  // namespace

/**
  Open a MERGE parent table, but not its children.

  @param[in]    name               MERGE table path name
  @param[in]    mode               read/write mode, unused
  @param[in]    test_if_locked_arg open flags
  @param[in]    table_def          dd::Table object describing
                                   table to be opened.

  @return       status
  @retval     0                    OK
  @retval     -1                   Error, my_errno gives reason

  @details
  This function initializes the MERGE storage engine structures
  and adds a child list of TABLE_LIST to the parent handler.
*/

int ha_myisammrg::open(const char *name, int mode [[maybe_unused]],
                       uint test_if_locked_arg,
                       const dd::Table *table_def [[maybe_unused]]) {
  DBUG_TRACE;
  DBUG_PRINT("myrg", ("name: '%s'  table: %p", name, table));
  DBUG_PRINT("myrg", ("test_if_locked_arg: %u", test_if_locked_arg));

  /* Must not be used when table is open. */
  assert(!this->file);

  /* Save for later use. */
  test_if_locked = test_if_locked_arg;

  /* In case this handler was open and closed before, free old data. */
  this->children_mem_root.ClearForReuse();

  /*
    Initialize variables that are used, modified, and/or set by
    myisammrg_parent_open_callback().
    'children_l' is the head of the children chain.
    'children_last_l' points to the end of the children chain.
    'my_errno' is set by myisammrg_parent_open_callback() in
    case of an error.
  */
  children_l = nullptr;
  children_last_l = nullptr;
  child_def_list.clear();
  set_my_errno(0);

  /* retrieve children table list. */
  if (is_cloned) {
    DEBUG_SYNC(current_thd, "before_myrg_open");
    /*
      Open and attaches the MyISAM tables,that are under the MERGE table
      parent, on the MyISAM storage engine interface directly within the
      MERGE engine. The new MyISAM table instances, as well as the MERGE
      clone itself, are not visible in the table cache. This is not a
      problem because all locking is handled by the original MERGE table
      from which this is cloned of.
    */
    if (!(file = myrg_open(name, table->db_stat, HA_OPEN_IGNORE_IF_LOCKED))) {
      DBUG_PRINT("error", ("my_errno %d", my_errno()));
      return my_errno() ? my_errno() : -1;
    }

    file->children_attached = true;

    info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
    /*
      There may arise a scenario where it might end up with two different
      MYMERGE_INFO data if any one of the child table is updated in
      between myrg_open() and the last ha_myisammrg::info(). So we need make
      sure that the MYMERGE_INFO data are in sync.
    */
    table->file->info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  } else if (!(file = myrg_parent_open(name, myisammrg_parent_open_callback,
                                       this))) {
    /* purecov: begin inspected */
    DBUG_PRINT("error", ("my_errno %d", my_errno()));
    return my_errno() ? my_errno() : -1;
    /* purecov: end */
  }
  DBUG_PRINT("myrg", ("MYRG_INFO: %p  child tables: %u", file, file->tables));
  return 0;
}

/**
  Add list of MERGE children to a TABLE_LIST chain.

  @return status
    @retval     0               OK
    @retval     != 0            Error

  @details
    When a MERGE parent table has just been opened, insert the
    TABLE_LIST chain from the MERGE handler into the table list used for
    opening tables for this statement. This lets the children be opened
    too.
*/

int ha_myisammrg::add_children_list(void) {
  TABLE_LIST *parent_l = this->table->pos_in_table_list;
  THD *thd = table->in_use;
  List_iterator_fast<Mrg_child_def> it(child_def_list);
  Mrg_child_def *mrg_child_def;
  DBUG_TRACE;
  DBUG_PRINT("myrg", ("table: '%s'.'%s' %p", this->table->s->db.str,
                      this->table->s->table_name.str, this->table));

  /* Must call this with open table. */
  assert(this->file);

  /* Ignore this for empty MERGE tables (UNION=()). */
  if (!this->file->tables) {
    DBUG_PRINT("myrg", ("empty merge table union"));
    goto end;
  }

  /* Must not call this with attached children. */
  assert(!this->file->children_attached);

  /* Must not call this with children list in place. */
  assert(this->children_l == nullptr);

  /*
    Prevent inclusion of another MERGE table, which could make infinite
    recursion.
  */
  if (parent_l->parent_l) {
    my_error(ER_ADMIN_WRONG_MRG_TABLE, MYF(0), parent_l->alias);
    return 1;
  }

  while ((mrg_child_def = it++)) {
    TABLE_LIST *child_l;
    char *db;
    char *table_name;

    db = (char *)thd->memdup(mrg_child_def->db.str,
                             mrg_child_def->db.length + 1);
    table_name = (char *)thd->memdup(mrg_child_def->name.str,
                                     mrg_child_def->name.length + 1);

    if (db == nullptr || table_name == nullptr) return 1;

    child_l = new (thd->mem_root) TABLE_LIST(
        db, mrg_child_def->db.length, table_name, mrg_child_def->name.length,
        table_name, parent_l->lock_descriptor().type);

    if (child_l == nullptr) return 1;

    /* Set parent reference. Used to detect MERGE in children list. */
    child_l->parent_l = parent_l;
    /* Copy query_block. Used in unique_table() at least. */
    child_l->query_block = parent_l->query_block;
    /* Set the expected table version, to not cause spurious re-prepare. */
    child_l->set_table_ref_id(mrg_child_def->get_child_table_ref_type(),
                              mrg_child_def->get_child_def_version());
    /*
      Copy parent's prelocking attribute to allow opening of child
      temporary residing in the prelocking list.
    */
    child_l->prelocking_placeholder = parent_l->prelocking_placeholder;
    /*
      For ALTER TABLE statements, which acquire a SU metadata lock on a
      parent table and then later try to upgrade it first to SNW and then
      to X lock, SNW locks should be also taken on the children tables.

      Otherwise, we end up in a situation where the thread trying to upgrade
      SU to SNW/SNW to X lock on the parent also holds a SR metadata lock
      and a read thr_lock.c lock on the child. As a result, another thread
      might be blocked on the thr_lock.c lock for the child after successfully
      acquiring a SR or SW metadata lock on it. If at the same time this second
      thread has a shared metadata lock on the parent table or there is some
      other thread which has a shared metadata lock on the parent and is waiting
      for this second thread, we get a deadlock. This deadlock cannot be
      properly detected by the MDL subsystem as part of the waiting happens
      within thr_lock.c. By taking SNW locks on the child tables we ensure that
      any thread which waits for a thread doing upgrade, does this within the
      MDL subsystem and thus potential deadlocks are exposed to the deadlock
      detector.

      We don't do similar thing for SNRW locks as only S locks associated with
      open HANDLER statements can cause issue with SNRW -> X upgrade. But this
      issue is solved thanks to notification mechanism in MDL subsystem.

      SRO locks don't require similar handling since they are never upgraded and
      underlying tables are always properly protected by thr_lock.c locks.
    */
    if (!thd->locked_tables_mode &&
        parent_l->mdl_request.type == MDL_SHARED_UPGRADABLE)
      child_l->mdl_request.set_type(MDL_SHARED_NO_WRITE);
    /* Link TABLE_LIST object into the children list. */
    if (this->children_last_l)
      child_l->prev_global = this->children_last_l;
    else {
      /* Initialize children_last_l when handling first child. */
      this->children_last_l = &this->children_l;
    }
    *this->children_last_l = child_l;
    this->children_last_l = &child_l->next_global;
  }

  /* Insert children into the table list. */
  if (parent_l->next_global)
    parent_l->next_global->prev_global = this->children_last_l;
  *this->children_last_l = parent_l->next_global;
  parent_l->next_global = this->children_l;
  this->children_l->prev_global = &parent_l->next_global;
  /*
    We have to update LEX::query_tables_last if children are added to
    the tail of the table list in order to be able correctly add more
    elements to it (e.g. as part of prelocking process).
  */
  if (thd->lex->query_tables_last == &parent_l->next_global)
    thd->lex->query_tables_last = this->children_last_l;
  /*
    The branch below works only when re-executing a prepared
    statement or a stored routine statement:
    We've just modified query_tables_last. Keep it in sync with
    query_tables_last_own, if it was set by the prelocking code.
    This ensures that the check that prohibits double updates (*)
    can correctly identify what tables belong to the main statement.
    (*) A double update is, e.g. when a user issues UPDATE t1 and
    t1 has an AFTER UPDATE trigger that also modifies t1.
  */
  if (thd->lex->query_tables_own_last == &parent_l->next_global)
    thd->lex->query_tables_own_last = this->children_last_l;

end:
  return 0;
}

/**
  A context of myrg_attach_children() callback.
*/

class Mrg_attach_children_callback_param {
 public:
  /**
    'need_compat_check' is set by myisammrg_attach_children_callback()
    if a child fails the table def version check.
  */
  bool need_compat_check;
  /** TABLE_LIST identifying this merge parent. */
  TABLE_LIST *parent_l;
  /** Iterator position, the current child to attach. */
  TABLE_LIST *next_child_attach;
  List_iterator_fast<Mrg_child_def> def_it;
  Mrg_child_def *mrg_child_def;

 public:
  Mrg_attach_children_callback_param(TABLE_LIST *parent_l_arg,
                                     TABLE_LIST *first_child,
                                     List<Mrg_child_def> &child_def_list)
      : need_compat_check(false),
        parent_l(parent_l_arg),
        next_child_attach(first_child),
        def_it(child_def_list),
        mrg_child_def(def_it++) {}
  void next() {
    next_child_attach = next_child_attach->next_global;
    if (next_child_attach && next_child_attach->parent_l != parent_l)
      next_child_attach = nullptr;
    if (mrg_child_def) mrg_child_def = def_it++;
  }
};

namespace {

/**
  Callback function for attaching a MERGE child table.

  @param[in]    callback_param  data pointer as given to myrg_attach_children()
                                this is used to pass the handler handle

  @return       pointer to open MyISAM table structure
    @retval     !=NULL                  OK, returning pointer
    @retval     NULL,                   Error.

  @details
    This function retrieves the MyISAM table handle from the
    next child table. It is called for each child table.
*/

extern "C" MI_INFO *myisammrg_attach_children_callback(void *callback_param) {
  Mrg_attach_children_callback_param *param =
      (Mrg_attach_children_callback_param *)callback_param;
  TABLE *parent = param->parent_l->table;
  TABLE *child;
  TABLE_LIST *child_l = param->next_child_attach;
  Mrg_child_def *mrg_child_def = param->mrg_child_def;
  MI_INFO *myisam = nullptr;
  DBUG_TRACE;

  /*
    Number of children in the list and MYRG_INFO::tables_count,
    which is used by caller of this function, should always match.
  */
  assert(child_l);

  child = child_l->table;

  /* Prepare for next child. */
  param->next();

  if (!child) {
    DBUG_PRINT("error", ("failed to open underlying table '%s'.'%s'",
                         child_l->db, child_l->table_name));
    goto end;
  }

  /*
    Do a quick compatibility check. The table def version is set when
    the table share is created. The child def version is copied
    from the table def version after a successful compatibility check.
    We need to repeat the compatibility check only if a child is opened
    from a different share than last time it was used with this MERGE
    table.
  */
  DBUG_PRINT("myrg", ("table_def_version last: %llu  current: %llu",
                      mrg_child_def->get_child_def_version(),
                      child->s->get_table_def_version()));
  if (mrg_child_def->get_child_def_version() !=
      child->s->get_table_def_version())
    param->need_compat_check = true;

  /*
    If child is temporary, parent must be temporary as well. Other
    parent/child combinations are allowed. This check must be done for
    every child on every open because the table def version can overlap
    between temporary and non-temporary tables. We need to detect the
    case where a non-temporary table has been replaced with a temporary
    table of the same version. Or vice versa. A very unlikely case, but
    it could happen. (Note that the condition was different from
    5.1.23/6.0.4(Bug#19627) to 5.5.6 (Bug#36171): child->s->tmp_table !=
    parent->s->tmp_table. Tables were required to have the same status.)
  */
  if (child->s->tmp_table && !parent->s->tmp_table) {
    DBUG_PRINT("error", ("temporary table mismatch parent: %d  child: %d",
                         parent->s->tmp_table, child->s->tmp_table));
    goto end;
  }

  /* Extract the MyISAM table structure pointer from the handler object. */
  if ((child->file->ht->db_type != DB_TYPE_MYISAM) ||
      !(myisam = ((ha_myisam *)child->file)->file_ptr())) {
    DBUG_PRINT("error", ("no MyISAM handle for child table: '%s'.'%s' %p",
                         child->s->db.str, child->s->table_name.str, child));
  }

  DBUG_PRINT("myrg", ("MyISAM handle: %p", myisam));

end:

  if (!myisam && (current_thd->open_options & HA_OPEN_FOR_REPAIR)) {
    char buf[2 * NAME_LEN + 1 + 1];
    strxnmov(buf, sizeof(buf) - 1, child_l->db, ".", child_l->table_name, NULL);
    /*
      Push an error to be reported as part of CHECK/REPAIR result-set.
      Note that calling my_error() from handler is a hack which is kept
      here to avoid refactoring. Normally engines should report errors
      through return value which will be interpreted by caller using
      handler::print_error() call.
    */
    my_error(ER_ADMIN_WRONG_MRG_TABLE, MYF(0), buf);
  }

  return myisam;
}

}  // namespace

/**
   Returns a cloned instance of the current handler.

   @return A cloned handler instance.
 */
handler *ha_myisammrg::clone(const char *name, MEM_ROOT *mem_root) {
  MYRG_TABLE *u_table, *newu_table;
  ha_myisammrg *new_handler = (ha_myisammrg *)get_new_handler(
      table->s, false, mem_root, table->s->db_type());
  if (!new_handler) return nullptr;

  /* Inform ha_myisammrg::open() that it is a cloned handler */
  new_handler->is_cloned = true;
  /*
    Allocate handler->ref here because otherwise ha_open will allocate it
    on this->table->mem_root and we will not be able to reclaim that memory
    when the clone handler object is destroyed.
  */
  if (!(new_handler->ref =
            (uchar *)mem_root->Alloc(ALIGN_SIZE(ref_length) * 2))) {
    destroy(new_handler);
    return nullptr;
  }

  if (new_handler->ha_open(table, name, table->db_stat,
                           HA_OPEN_IGNORE_IF_LOCKED, nullptr)) {
    destroy(new_handler);
    return nullptr;
  }

  /*
    Iterate through the original child tables and
    copy the state into the cloned child tables.
    We need to do this because all the child tables
    can be involved in delete.
  */
  newu_table = new_handler->file->open_tables;
  for (u_table = file->open_tables; u_table < file->end_table; u_table++) {
    newu_table->table->state = u_table->table->state;
    newu_table++;
  }

  return new_handler;
}

/**
  Attach children to a MERGE table.

  @return status
    @retval     0               OK
    @retval     != 0            Error, my_errno gives reason

  @details
    Let the storage engine attach its children through a callback
    function. Check table definitions for consistency.

  @note
    Special thd->open_options may be in effect. We can make use of
    them in attach. I.e. we use HA_OPEN_FOR_REPAIR to report the names
    of mismatching child tables. We cannot transport these options in
    ha_myisammrg::test_if_locked because they may change after the
    parent is opened. The parent is kept open in the table cache over
    multiple statements and can be used by other threads. Open options
    can change over time.
*/

int ha_myisammrg::attach_children(void) {
  MYRG_TABLE *u_table;
  MI_COLUMNDEF *recinfo;
  MI_KEYDEF *keyinfo;
  uint recs;
  uint keys = table->s->keys;
  TABLE_LIST *parent_l = table->pos_in_table_list;
  int error;
  Mrg_attach_children_callback_param param(parent_l, this->children_l,
                                           child_def_list);
  DBUG_TRACE;
  DBUG_PRINT("myrg", ("table: '%s'.'%s' %p", table->s->db.str,
                      table->s->table_name.str, table));
  DBUG_PRINT("myrg", ("test_if_locked: %u", this->test_if_locked));

  /* Must call this with open table. */
  assert(this->file);

  /*
    A MERGE table with no children (empty union) is always seen as
    attached internally.
  */
  if (!this->file->tables) {
    DBUG_PRINT("myrg", ("empty merge table union"));
    goto end;
  }
  DBUG_PRINT("myrg", ("child tables: %u", this->file->tables));

  /* Must not call this with attached children. */
  assert(!this->file->children_attached);

  DEBUG_SYNC(current_thd, "before_myisammrg_attach");
  /* Must call this with children list in place. */
  assert(this->table->pos_in_table_list->next_global == this->children_l);

  if (myrg_attach_children(this->file,
                           this->test_if_locked | current_thd->open_options,
                           myisammrg_attach_children_callback, &param,
                           (bool *)&param.need_compat_check)) {
    error = my_errno();
    goto err;
  }
  DBUG_PRINT("myrg", ("calling myrg_extrafunc"));
  if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
        test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
    myrg_extra(file, HA_EXTRA_NO_WAIT_LOCK, nullptr);
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    myrg_extra(file, HA_EXTRA_WAIT_LOCK, nullptr);

  /*
    The compatibility check is required only if one or more children do
    not match their table def version from the last check. This will
    always happen at the first attach because the reference child def
    version is initialized to 'undefined' at open.
  */
  DBUG_PRINT("myrg", ("need_compat_check: %d", param.need_compat_check));
  if (param.need_compat_check) {
    TABLE_LIST *child_l;

    if (table->s->reclength != stats.mean_rec_length && stats.mean_rec_length) {
      DBUG_PRINT("error", ("reclength: %lu  mean_rec_length: %lu",
                           table->s->reclength, stats.mean_rec_length));
      if (test_if_locked & HA_OPEN_FOR_REPAIR) {
        /* purecov: begin inspected */
        myrg_print_wrong_table(file->open_tables->table->filename);
        /* purecov: end */
      }
      error = HA_ERR_WRONG_MRG_TABLE_DEF;
      goto err;
    }
    /*
      Both recinfo and keyinfo are allocated by my_multi_malloc(), thus
      only recinfo must be freed.
    */
    if ((error = table2myisam(table, &keyinfo, &recinfo, &recs))) {
      /* purecov: begin inspected */
      DBUG_PRINT("error", ("failed to convert TABLE object to MyISAM "
                           "key and column definition"));
      goto err;
      /* purecov: end */
    }
    for (u_table = file->open_tables; u_table < file->end_table; u_table++) {
      if (check_definition(keyinfo, recinfo, keys, recs,
                           u_table->table->s->keyinfo, u_table->table->s->rec,
                           u_table->table->s->base.keys,
                           u_table->table->s->base.fields, false)) {
        DBUG_PRINT("error", ("table definition mismatch: '%s'",
                             u_table->table->filename));
        error = HA_ERR_WRONG_MRG_TABLE_DEF;
        if (!(this->test_if_locked & HA_OPEN_FOR_REPAIR)) {
          my_free(recinfo);
          goto err;
        }
        /* purecov: begin inspected */
        myrg_print_wrong_table(u_table->table->filename);
        /* purecov: end */
      }
    }
    my_free(recinfo);
    if (error == HA_ERR_WRONG_MRG_TABLE_DEF) goto err; /* purecov: inspected */

    List_iterator_fast<Mrg_child_def> def_it(child_def_list);
    assert(this->children_l);
    for (child_l = this->children_l;; child_l = child_l->next_global) {
      Mrg_child_def *mrg_child_def = def_it++;
      mrg_child_def->set_child_def_version(
          child_l->table->s->get_table_ref_type(),
          child_l->table->s->get_table_def_version());

      if (&child_l->next_global == this->children_last_l) break;
    }
  }

end:
  return 0;

err:
  DBUG_PRINT("error", ("attaching MERGE children failed: %d", error));
  print_error(error, MYF(0));
  detach_children();
  set_my_errno(error);
  return error;
}

/**
  Detach all children from a MERGE table and from the query list of tables.

  @return status
    @retval     0               OK
    @retval     != 0            Error, my_errno gives reason

  @note
    Detach must not touch the child TABLE objects in any way.
    They may have been closed at this point already.
    All references to the children should be removed.
*/

int ha_myisammrg::detach_children(void) {
  TABLE_LIST *child_l;
  DBUG_TRACE;

  /* Must call this with open table. */
  assert(this->file);

  /* A MERGE table with no children (empty union) cannot be detached. */
  if (!this->file->tables) {
    DBUG_PRINT("myrg", ("empty merge table union"));
    goto end;
  }

  if (this->children_l) {
    THD *thd = table->in_use;

    /* Clear TABLE references. */
    for (child_l = this->children_l;; child_l = child_l->next_global) {
      /*
        Do not assert(child_l->table); open_tables might be
        incomplete.

        Clear the table reference.
      */
      child_l->table = nullptr;
      /* Similarly, clear the ticket reference. */
      child_l->mdl_request.ticket = nullptr;

      /* Break when this was the last child. */
      if (&child_l->next_global == this->children_last_l) break;
    }
    /*
      Remove children from the table list. This won't fail if called
      twice. The list is terminated after removal.

      If the parent is LEX::query_tables_own_last and pre-locked tables
      follow (tables used by stored functions or triggers), the children
      are inserted behind the parent and before the pre-locked tables. But
      we do not adjust LEX::query_tables_own_last. The pre-locked tables
      could have chopped off the list by clearing
      *LEX::query_tables_own_last. This did also chop off the children. If
      we would copy the reference from *this->children_last_l in this
      case, we would put the chopped off pre-locked tables back to the
      list. So we refrain from copying it back, if the destination has
      been set to NULL meanwhile.
    */
    if (this->children_l->prev_global && *this->children_l->prev_global)
      *this->children_l->prev_global = *this->children_last_l;
    if (*this->children_last_l)
      (*this->children_last_l)->prev_global = this->children_l->prev_global;

    /*
      If table elements being removed are at the end of table list we
      need to adjust LEX::query_tables_last member to point to the
      new last element of the list.
    */
    if (thd->lex->query_tables_last == this->children_last_l)
      thd->lex->query_tables_last = this->children_l->prev_global;

    /*
      If the statement requires prelocking, and prelocked
      tables were added right after merge children, modify the
      last own table pointer to point at prev_global of the merge
      parent.
    */
    if (thd->lex->query_tables_own_last == this->children_last_l)
      thd->lex->query_tables_own_last = this->children_l->prev_global;

    /* Terminate child list. So it cannot be tried to remove again. */
    *this->children_last_l = nullptr;
    this->children_l->prev_global = nullptr;

    /* Forget about the children, we don't own their memory. */
    this->children_l = nullptr;
    this->children_last_l = nullptr;
  }

  if (!this->file->children_attached) {
    DBUG_PRINT("myrg", ("merge children are already detached"));
    goto end;
  }

  if (myrg_detach_children(this->file)) {
    /* purecov: begin inspected */
    print_error(my_errno(), MYF(0));
    return my_errno() ? my_errno() : -1;
    /* purecov: end */
  }

end:
  return 0;
}

/**
  Close a MERGE parent table, but not its children.

  @return status
    @retval     0               OK
    @retval     != 0            Error, my_errno gives reason

  @note
    The children are expected to be closed separately by the caller.
*/

int ha_myisammrg::close(void) {
  int rc;
  DBUG_TRACE;
  /*
    There are cases where children are not explicitly detached before
    close. detach_children() protects itself against double detach.
  */
  if (!is_cloned) detach_children();

  rc = myrg_close(file);
  file = nullptr;
  return rc;
}

int ha_myisammrg::write_row(uchar *buf) {
  DBUG_TRACE;
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_write_count);

  if (file->merge_insert_method == MERGE_INSERT_DISABLED || !file->tables)
    return HA_ERR_TABLE_READONLY;

  if (table->next_number_field && buf == table->record[0]) {
    int error;
    if ((error = update_auto_increment()))
      return error; /* purecov: inspected */
  }
  return myrg_write(file, buf);
}

int ha_myisammrg::update_row(const uchar *old_data, uchar *new_data) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_update_count);
  return myrg_update(file, old_data, new_data);
}

int ha_myisammrg::delete_row(const uchar *buf) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_delete_count);
  return myrg_delete(file, buf);
}

int ha_myisammrg::index_read_map(uchar *buf, const uchar *key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error = myrg_rkey(file, buf, active_index, key, keypart_map, find_flag);
  return error;
}

int ha_myisammrg::index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                     key_part_map keypart_map,
                                     enum ha_rkey_function find_flag) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error = myrg_rkey(file, buf, index, key, keypart_map, find_flag);
  return error;
}

int ha_myisammrg::index_read_last_map(uchar *buf, const uchar *key,
                                      key_part_map keypart_map) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error =
      myrg_rkey(file, buf, active_index, key, keypart_map, HA_READ_PREFIX_LAST);
  return error;
}

int ha_myisammrg::index_next(uchar *buf) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  int error = myrg_rnext(file, buf, active_index);
  return error;
}

int ha_myisammrg::index_prev(uchar *buf) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_prev_count);
  int error = myrg_rprev(file, buf, active_index);
  return error;
}

int ha_myisammrg::index_first(uchar *buf) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_first_count);
  int error = myrg_rfirst(file, buf, active_index);
  return error;
}

int ha_myisammrg::index_last(uchar *buf) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_last_count);
  int error = myrg_rlast(file, buf, active_index);
  return error;
}

int ha_myisammrg::index_next_same(uchar *buf, const uchar *key [[maybe_unused]],
                                  uint length [[maybe_unused]]) {
  int error;
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  do {
    error = myrg_rnext_same(file, buf);
  } while (error == HA_ERR_RECORD_DELETED);
  return error;
}

int ha_myisammrg::rnd_init(bool) {
  assert(this->file->children_attached);
  return myrg_reset(file);
}

int ha_myisammrg::rnd_next(uchar *buf) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);
  int error = myrg_rrnd(file, buf, HA_OFFSET_ERROR);
  return error;
}

int ha_myisammrg::rnd_pos(uchar *buf, uchar *pos) {
  assert(this->file->children_attached);
  ha_statistic_increment(&System_status_var::ha_read_rnd_count);
  int error = myrg_rrnd(file, buf, my_get_ptr(pos, ref_length));
  return error;
}

void ha_myisammrg::position(const uchar *) {
  assert(this->file->children_attached);
  ulonglong row_position = myrg_position(file);
  my_store_ptr(ref, ref_length, (my_off_t)row_position);
}

ha_rows ha_myisammrg::records_in_range(uint inx, key_range *min_key,
                                       key_range *max_key) {
  assert(this->file->children_attached);
  return (ha_rows)myrg_records_in_range(file, (int)inx, min_key, max_key);
}

int ha_myisammrg::truncate(dd::Table *) {
  int err = 0;
  MYRG_TABLE *my_table;
  DBUG_TRACE;

  for (my_table = file->open_tables; my_table != file->end_table; my_table++) {
    if ((err = mi_delete_all_rows(my_table->table))) break;
  }

  return err;
}

int ha_myisammrg::info(uint flag) {
  MYMERGE_INFO mrg_info;
  assert(this->file->children_attached);
  (void)myrg_status(file, &mrg_info, flag);
  /*
    The following fails if one has not compiled MySQL with -DBIG_TABLES
    and one has more than 2^32 rows in the merge tables.
  */
  stats.records = (ha_rows)mrg_info.records;
  stats.deleted = (ha_rows)mrg_info.deleted;
  stats.data_file_length = mrg_info.data_file_length;
  if (mrg_info.errkey >= (int)table_share->keys) {
    /*
     If value of errkey is higher than the number of keys
     on the table set errkey to MAX_KEY. This will be
     treated as unknown key case and error message generator
     won't try to locate key causing segmentation fault.
    */
    mrg_info.errkey = MAX_KEY;
  }
  table->s->keys_in_use.set_prefix(table->s->keys);
  stats.mean_rec_length = mrg_info.reclength;

  /*
    The handler::block_size is used all over the code in index scan cost
    calculations. It is used to get number of disk seeks required to
    retrieve a number of index tuples.
    If the merge table has N underlying tables, then (assuming underlying
    tables have equal size, the only "simple" approach we can use)
    retrieving X index records from a merge table will require N times more
    disk seeks compared to doing the same on a MyISAM table with equal
    number of records.
    In the edge case (file_tables > myisam_block_size) we'll get
    block_size==0, and index calculation code will act as if we need one
    disk seek to retrieve one index tuple.

    TODO: In 5.2 index scan cost calculation will be factored out into a
    virtual function in class handler and we'll be able to remove this hack.
  */
  stats.block_size = 0;
  if (file->tables) stats.block_size = myisam_block_size / file->tables;

  stats.update_time = 0;
  ref_length = 6;  // Should be big enough
  if (flag & HA_STATUS_CONST) {
    if (table->s->key_parts && mrg_info.rec_per_key) {
      memcpy((char *)table->key_info[0].rec_per_key,
             (char *)mrg_info.rec_per_key,
             sizeof(table->key_info[0].rec_per_key[0]) *
                 min(file->keys, table->s->key_parts));
    }
  }
  if (flag & HA_STATUS_ERRKEY) {
    errkey = mrg_info.errkey;
    my_store_ptr(dup_ref, ref_length, mrg_info.dupp_key_pos);
  }
  return 0;
}

int ha_myisammrg::extra(enum ha_extra_function operation) {
  if (operation == HA_EXTRA_ADD_CHILDREN_LIST) {
    int rc = add_children_list();
    return (rc);
  } else if (operation == HA_EXTRA_ATTACH_CHILDREN) {
    int rc = attach_children();
    if (!rc) (void)extra(HA_EXTRA_NO_READCHECK);  // Not needed in SQL
    return (rc);
  } else if (operation == HA_EXTRA_IS_ATTACHED_CHILDREN) {
    /* For the upper layer pretend empty MERGE union is never attached. */
    return (file && file->tables && file->children_attached);
  } else if (operation == HA_EXTRA_DETACH_CHILDREN) {
    /*
      Note that detach must not touch the children in any way.
      They may have been closed at this point already.
    */
    int rc = detach_children();
    return (rc);
  }

  /* As this is just a mapping, we don't have to force the underlying
     tables to be closed */
  if (operation == HA_EXTRA_FORCE_REOPEN ||
      operation == HA_EXTRA_PREPARE_FOR_DROP)
    return 0;
  return myrg_extra(file, operation, nullptr);
}

int ha_myisammrg::reset(void) {
  /* This is normally called with detached children. */
  return myrg_reset(file);
}

/* To be used with WRITE_CACHE, EXTRA_CACHE and BULK_INSERT_BEGIN */

int ha_myisammrg::extra_opt(enum ha_extra_function operation,
                            ulong cache_size) {
  assert(this->file->children_attached);
  return myrg_extra(file, operation, (void *)&cache_size);
}

int ha_myisammrg::external_lock(THD *, int lock_type) {
  /*
    This can be called with no children attached. E.g. FLUSH TABLES
    unlocks and re-locks tables under LOCK TABLES, but it does not open
    them first. So they are detached all the time. But locking of the
    children should work anyway because thd->open_tables is not changed
    during FLUSH TABLES.

    If this handler instance has been cloned, we still must call
    myrg_lock_database().
  */
  if (is_cloned) return myrg_lock_database(file, lock_type);
  return 0;
}

uint ha_myisammrg::lock_count(void) const { return 0; }

THR_LOCK_DATA **ha_myisammrg::store_lock(THD *, THR_LOCK_DATA **to,
                                         enum thr_lock_type) {
  return to;
}

/* Find out database name and table name from a filename */

static void split_file_name(const char *file_name, LEX_CSTRING *db,
                            LEX_CSTRING *name) {
  size_t dir_length, prefix_length;
  char buff[FN_REFLEN];

  db->length = 0;
  strmake(buff, file_name, sizeof(buff) - 1);
  dir_length = dirname_length(buff);
  if (dir_length > 1) {
    /* Get database */
    buff[dir_length - 1] = 0;  // Remove end '/'
    prefix_length = dirname_length(buff);
    db->str = file_name + prefix_length;
    db->length = dir_length - prefix_length - 1;
  }
  name->str = file_name + dir_length;
  name->length = (uint)(fn_ext(name->str) - name->str);
}

void ha_myisammrg::update_create_info(HA_CREATE_INFO *create_info) {
  DBUG_TRACE;

  if (!(create_info->used_fields & HA_CREATE_USED_UNION)) {
    TABLE_LIST *child_table;
    THD *thd = current_thd;

    create_info->merge_list.next = &create_info->merge_list.first;
    create_info->merge_list.elements = 0;

    if (children_l != nullptr) {
      for (child_table = children_l;; child_table = child_table->next_global) {
        TABLE_LIST *ptr;

        if (!(ptr = (TABLE_LIST *)thd->mem_calloc(sizeof(TABLE_LIST))))
          goto err;

        if (!(ptr->table_name = thd->strmake(child_table->table_name,
                                             child_table->table_name_length)))
          goto err;
        if (child_table->db &&
            !(ptr->db = thd->strmake(child_table->db, child_table->db_length)))
          goto err;

        create_info->merge_list.elements++;
        (*create_info->merge_list.next) = ptr;
        create_info->merge_list.next = &ptr->next_local;

        if (&child_table->next_global == children_last_l) break;
      }
    }
    *create_info->merge_list.next = nullptr;
  }
  if (!(create_info->used_fields & HA_CREATE_USED_INSERT_METHOD)) {
    create_info->merge_insert_method = file->merge_insert_method;
  }
  return;

err:
  create_info->merge_list.elements = 0;
  create_info->merge_list.first = nullptr;
}

int ha_myisammrg::create(const char *name, TABLE *, HA_CREATE_INFO *create_info,
                         dd::Table *) {
  char buff[FN_REFLEN];
  const char **table_names, **pos;
  TABLE_LIST *tables = create_info->merge_list.first;
  THD *thd = current_thd;
  size_t dirlgt = dirname_length(name);
  DBUG_TRACE;

  /* Allocate a table_names array in thread mem_root. */
  if (!(table_names = (const char **)thd->alloc(
            (create_info->merge_list.elements + 1) * sizeof(char *))))
    return HA_ERR_OUT_OF_MEM; /* purecov: inspected */

  /* Create child path names. */
  for (pos = table_names; tables; tables = tables->next_local) {
    const char *table_name = buff;

    /*
      Construct the path to the MyISAM table. Try to meet two conditions:
      1.) Allow to include MyISAM tables from different databases, and
      2.) allow for moving DATADIR around in the file system.
      The first means that we need paths in the .MRG file. The second
      means that we should not have absolute paths in the .MRG file.
      The best, we can do, is to use 'mysql_data_home', which is '.'
      in mysqld and may be an absolute path in an embedded server.
      This means that it might not be possible to move the DATADIR of
      an embedded server without changing the paths in the .MRG file.

      Do the same even for temporary tables. MERGE children are now
      opened through the table cache. They are opened by db.table_name,
      not by their path name.
    */
    size_t length = build_table_filename(buff, sizeof(buff), tables->db,
                                         tables->table_name, "", 0);
    /*
      If a MyISAM table is in the same directory as the MERGE table,
      we use the table name without a path. This means that the
      DATADIR can easily be moved even for an embedded server as long
      as the MyISAM tables are from the same database as the MERGE table.
    */
    if ((dirname_length(buff) == dirlgt) && !memcmp(buff, name, dirlgt)) {
      table_name += dirlgt;
      length -= dirlgt;
    }
    if (!(table_name = thd->strmake(table_name, length)))
      return HA_ERR_OUT_OF_MEM; /* purecov: inspected */

    *pos++ = table_name;
  }
  *pos = nullptr;

  /* Create a MERGE meta file from the table_names array. */
  return myrg_create(
      fn_format(buff, name, "", "",
                MY_RESOLVE_SYMLINKS | MY_UNPACK_FILENAME | MY_APPEND_EXT),
      table_names, create_info->merge_insert_method, false);
}

void ha_myisammrg::append_create_info(String *packet) {
  const char *current_db;
  size_t db_length;
  const THD *thd = current_thd;
  TABLE_LIST *open_table, *first;

  if (file->merge_insert_method != MERGE_INSERT_DISABLED) {
    packet->append(STRING_WITH_LEN(" INSERT_METHOD="));
    packet->append(
        get_type(&merge_insert_method, file->merge_insert_method - 1));
  }
  /*
    There is no sense adding UNION clause in case there are no underlying
    tables specified.
  */
  if (file->open_tables == file->end_table) return;
  packet->append(STRING_WITH_LEN(" UNION=("));

  current_db = table->s->db.str;
  db_length = table->s->db.length;

  for (first = open_table = children_l;; open_table = open_table->next_global) {
    LEX_STRING db = {const_cast<char *>(open_table->db), open_table->db_length};

    if (open_table != first) packet->append(',');
    /* Report database for mapped table if it isn't in current database */
    if (db.length &&
        (db_length != db.length || strncmp(current_db, db.str, db.length))) {
      append_identifier(thd, packet, db.str, db.length);
      packet->append('.');
    }
    append_identifier(thd, packet, open_table->table_name,
                      open_table->table_name_length);
    if (&open_table->next_global == children_last_l) break;
  }
  packet->append(')');
}

bool ha_myisammrg::check_if_incompatible_data(HA_CREATE_INFO *, uint) {
  /*
    For myisammrg, we should always re-generate the mapping file as this
    is trivial to do
  */
  return COMPATIBLE_DATA_NO;
}

int ha_myisammrg::check(THD *, HA_CHECK_OPT *) {
  return this->file->children_attached ? HA_ADMIN_OK : HA_ADMIN_CORRUPT;
}

int ha_myisammrg::records(ha_rows *num_rows) {
  *num_rows = myrg_records(file);
  return 0;
}

static int myisammrg_panic(handlerton *, ha_panic_function flag) {
  return myrg_panic(flag);
}

static int myisammrg_init(void *p) {
  handlerton *myisammrg_hton;

  myisammrg_hton = (handlerton *)p;

#ifdef HAVE_PSI_INTERFACE
  init_myisammrg_psi_keys();
#endif

  myisammrg_hton->db_type = DB_TYPE_MRG_MYISAM;
  myisammrg_hton->create = myisammrg_create_handler;
  myisammrg_hton->panic = myisammrg_panic;
  myisammrg_hton->flags = HTON_NO_PARTITION;
  myisammrg_hton->file_extensions = ha_myisammrg_exts;
  myisammrg_hton->rm_tmp_tables = default_rm_tmp_tables;

  return 0;
}

struct st_mysql_storage_engine myisammrg_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

mysql_declare_plugin(myisammrg){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &myisammrg_storage_engine,
    "MRG_MYISAM",
    PLUGIN_AUTHOR_ORACLE,
    "Collection of identical MyISAM tables",
    PLUGIN_LICENSE_GPL,
    myisammrg_init, /* Plugin Init */
    nullptr,        /* Plugin Deinit */
    nullptr,        /* Plugin Check uninstall */
    0x0100,         /* 1.0 */
    nullptr,        /* status variables                */
    nullptr,        /* system variables                */
    nullptr,        /* config options                  */
    0,              /* flags                           */
} mysql_declare_plugin_end;
