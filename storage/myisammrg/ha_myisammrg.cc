/* Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


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
  children. This list is attached to the parent TABLE object as
  TABLE::child_l. The end of the children list is saved in
  TABLE::child_last_l.

  Back in open_tables(), add_merge_table_list() is called. It updates
  each list member with the lock type and a back pointer to the parent
  TABLE_LIST object TABLE_LIST::parent_l. The list is then inserted in
  the list of tables to open, right behind the parent. Consequently,
  open_tables() opens the children, one after the other. The TABLE
  references of the TABLE_LIST objects are implicitly set to the open
  tables. The children are opened as independent MyISAM tables, right as
  if they are used by the SQL statement.

  TABLE_LIST::parent_l is required to find the parent 1. when the last
  child has been opened and children are to be attached, and 2. when an
  error happens during child open and the child list must be removed
  from the queuery list. In these cases the current child does not have
  TABLE::parent set or does not have a TABLE at all respectively.

  When the last child is open, attach_merge_children() is called. It
  removes the list of children from the open list. Then the children are
  "attached" to the parent. All required references between parent and
  children are set up.

  The MERGE storage engine sets up an array with references to the
  low-level MyISAM table objects (MI_INFO). It remembers the state of
  the table in MYRG_INFO::children_attached.

  Every child TABLE::parent references the parent TABLE object. That way
  TABLE objects belonging to a MERGE table can be identified.
  TABLE::parent is required because the parent and child TABLE objects
  can live longer than the parent TABLE_LIST object. So the path
  child->pos_in_table_list->parent_l->table can be broken.

  If necessary, the compatibility of parent and children is checked.
  This check is necessary when any of the objects are reopend. This is
  detected by comparing the current table def version against the
  remembered child def version. On parent open, the list members are
  initialized to an "impossible"/"undefined" version value. So the check
  is always executed on the first attach.

  The version check is done in myisammrg_attach_children_callback(),
  which is called for every child. ha_myisammrg::attach_children()
  initializes 'need_compat_check' to FALSE and
  myisammrg_attach_children_callback() sets it ot TRUE if a table
  def version mismatches the remembered child def version.

  Finally the parent TABLE::children_attached is set.

  ---

  On parent open the storage engine structures are allocated and initialized.
  They stay with the open table until its final close.


*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "mysql_priv.h"
#include <mysql/plugin.h>
#include <m_ctype.h>
#include "../myisam/ha_myisam.h"
#include "ha_myisammrg.h"
#include "myrg_def.h"


static handler *myisammrg_create_handler(handlerton *hton,
                                         TABLE_SHARE *table,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_myisammrg(hton, table);
}


/**
  @brief Constructor
*/

ha_myisammrg::ha_myisammrg(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg), file(0), is_cloned(0)
{}


/**
  @brief Destructor
*/

ha_myisammrg::~ha_myisammrg(void)
{}


static const char *ha_myisammrg_exts[] = {
  ".MRG",
  NullS
};
extern int table2myisam(TABLE *table_arg, MI_KEYDEF **keydef_out,
                        MI_COLUMNDEF **recinfo_out, uint *records_out);
extern int check_definition(MI_KEYDEF *t1_keyinfo, MI_COLUMNDEF *t1_recinfo,
                            uint t1_keys, uint t1_recs,
                            MI_KEYDEF *t2_keyinfo, MI_COLUMNDEF *t2_recinfo,
                            uint t2_keys, uint t2_recs, bool strict,
                            TABLE *table_arg);
static void split_file_name(const char *file_name,
			    LEX_STRING *db, LEX_STRING *name);


extern "C" void myrg_print_wrong_table(const char *table_name)
{
  LEX_STRING db= {NULL, 0}, name;
  char buf[FN_REFLEN];
  split_file_name(table_name, &db, &name);
  memcpy(buf, db.str, db.length);
  buf[db.length]= '.';
  memcpy(buf + db.length + 1, name.str, name.length);
  buf[db.length + name.length + 1]= 0;
  push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                      ER_ADMIN_WRONG_MRG_TABLE, ER(ER_ADMIN_WRONG_MRG_TABLE),
                      buf);
}


const char **ha_myisammrg::bas_ext() const
{
  return ha_myisammrg_exts;
}


const char *ha_myisammrg::index_type(uint key_number)
{
  return ((table->key_info[key_number].flags & HA_FULLTEXT) ? 
	  "FULLTEXT" :
	  (table->key_info[key_number].flags & HA_SPATIAL) ?
	  "SPATIAL" :
	  (table->key_info[key_number].algorithm == HA_KEY_ALG_RTREE) ?
	  "RTREE" :
	  "BTREE");
}


/**
  @brief Callback function for open of a MERGE parent table.

  @detail This function adds a TABLE_LIST object for a MERGE child table
    to a list of tables of the parent TABLE object. It is called for
    each child table.

    The list of child TABLE_LIST objects is kept in the TABLE object of
    the parent for the whole life time of the MERGE table. It is
    inserted in the statement list behind the MERGE parent TABLE_LIST
    object when the MERGE table is opened. It is removed from the
    statement list after the last child is opened.

    All memeory used for the child TABLE_LIST objects and the strings
    referred by it are taken from the parent TABLE::mem_root. Thus they
    are all freed implicitly at the final close of the table.

    TABLE::child_l -> TABLE_LIST::next_global -> TABLE_LIST::next_global
    #                 #               ^          #               ^
    #                 #               |          #               |
    #                 #               +--------- TABLE_LIST::prev_global
    #                 #                                          |
    #           |<--- TABLE_LIST::prev_global                    |
    #                                                            |
    TABLE::child_last_l -----------------------------------------+

  @param[in]    callback_param  data pointer as given to myrg_parent_open()
  @param[in]    filename        file name of MyISAM table
                                without extension.

  @return status
    @retval     0               OK
    @retval     != 0            Error
*/

static int myisammrg_parent_open_callback(void *callback_param,
                                          const char *filename)
{
  ha_myisammrg  *ha_myrg;
  TABLE         *parent;
  TABLE_LIST    *child_l;
  const char    *db;
  const char    *table_name;
  size_t        dirlen;
  char          dir_path[FN_REFLEN];
  DBUG_ENTER("myisammrg_parent_open_callback");

  /* Extract child table name and database name from filename. */
  dirlen= dirname_length(filename);
  if (dirlen >= FN_REFLEN)
  {
    /* purecov: begin inspected */
    DBUG_PRINT("error", ("name too long: '%.64s'", filename));
    my_errno= ENAMETOOLONG;
    DBUG_RETURN(1);
    /* purecov: end */
  }
  table_name= filename + dirlen;
  dirlen--; /* Strip off trailing '/'. */
  memcpy(dir_path, filename, dirlen);
  dir_path[dirlen]= '\0';
  db= base_name(dir_path);
  dirlen-= db - dir_path; /* This is now the length of 'db'. */
  DBUG_PRINT("myrg", ("open: '%s'.'%s'", db, table_name));

  ha_myrg= (ha_myisammrg*) callback_param;
  parent= ha_myrg->table_ptr();

  /* Get a TABLE_LIST object. */
  if (!(child_l= (TABLE_LIST*) alloc_root(&parent->mem_root,
                                          sizeof(TABLE_LIST))))
  {
    /* purecov: begin inspected */
    DBUG_PRINT("error", ("my_malloc error: %d", my_errno));
    DBUG_RETURN(1);
    /* purecov: end */
  }
  bzero((char*) child_l, sizeof(TABLE_LIST));

  /* Set database (schema) name. */
  child_l->db_length= dirlen;
  child_l->db= strmake_root(&parent->mem_root, db, dirlen);
  /* Set table name. */
  child_l->table_name_length= strlen(table_name);
  child_l->table_name= strmake_root(&parent->mem_root, table_name,
                                    child_l->table_name_length);
  /* Convert to lowercase if required. */
  if (lower_case_table_names && child_l->table_name_length)
    child_l->table_name_length= my_casedn_str(files_charset_info,
                                              child_l->table_name);
  /* Set alias. */
  child_l->alias= child_l->table_name;

  /* Initialize table map to 'undefined'. */
  child_l->init_child_def_version();

  /* Link TABLE_LIST object into the parent list. */
  if (!parent->child_last_l)
  {
    /* Initialize parent->child_last_l when handling first child. */
    parent->child_last_l= &parent->child_l;
  }
  *parent->child_last_l= child_l;
  child_l->prev_global= parent->child_last_l;
  parent->child_last_l= &child_l->next_global;

  DBUG_RETURN(0);
}


/**
  @brief Callback function for attaching a MERGE child table.

  @detail This function retrieves the MyISAM table handle from the
    next child table. It is called for each child table.

  @param[in]    callback_param      data pointer as given to
                                    myrg_attach_children()

  @return       pointer to open MyISAM table structure
    @retval     !=NULL                  OK, returning pointer
    @retval     NULL, my_errno == 0     Ok, no more child tables
    @retval     NULL, my_errno != 0     error
*/

static MI_INFO *myisammrg_attach_children_callback(void *callback_param)
{
  ha_myisammrg  *ha_myrg;
  TABLE         *parent;
  TABLE         *child;
  TABLE_LIST    *child_l;
  MI_INFO       *myisam;
  DBUG_ENTER("myisammrg_attach_children_callback");

  my_errno= 0;
  ha_myrg= (ha_myisammrg*) callback_param;
  parent= ha_myrg->table_ptr();

  /* Get child list item. */
  child_l= ha_myrg->next_child_attach;
  if (!child_l)
  {
    DBUG_PRINT("myrg", ("No more children to attach"));
    DBUG_RETURN(NULL);
  }
  child= child_l->table;
  DBUG_PRINT("myrg", ("child table: '%s'.'%s' 0x%lx", child->s->db.str,
                      child->s->table_name.str, (long) child));
  /*
    Prepare for next child. Used as child_l in next call to this function.
    We cannot rely on a NULL-terminated chain.
  */
  if (&child_l->next_global == parent->child_last_l)
  {
    DBUG_PRINT("myrg", ("attaching last child"));
    ha_myrg->next_child_attach= NULL;
  }
  else
    ha_myrg->next_child_attach= child_l->next_global;

  /* Set parent reference. */
  child->parent= parent;

  /*
    Do a quick compatibility check. The table def version is set when
    the table share is created. The child def version is copied
    from the table def version after a sucessful compatibility check.
    We need to repeat the compatibility check only if a child is opened
    from a different share than last time it was used with this MERGE
    table.
  */
  DBUG_PRINT("myrg", ("table_def_version last: %lu  current: %lu",
                      (ulong) child_l->get_child_def_version(),
                      (ulong) child->s->get_table_def_version()));
  if (child_l->get_child_def_version() != child->s->get_table_def_version())
    ha_myrg->need_compat_check= TRUE;

  /*
    If parent is temporary, children must be temporary too and vice
    versa. This check must be done for every child on every open because
    the table def version can overlap between temporary and
    non-temporary tables. We need to detect the case where a
    non-temporary table has been replaced with a temporary table of the
    same version. Or vice versa. A very unlikely case, but it could
    happen.
  */
  if (child->s->tmp_table != parent->s->tmp_table)
  {
    DBUG_PRINT("error", ("temporary table mismatch parent: %d  child: %d",
                         parent->s->tmp_table, child->s->tmp_table));
    my_errno= HA_ERR_WRONG_MRG_TABLE_DEF;
    goto err;
  }

  /* Extract the MyISAM table structure pointer from the handler object. */
  if ((child->file->ht->db_type != DB_TYPE_MYISAM) ||
      !(myisam= ((ha_myisam*) child->file)->file_ptr()))
  {
    DBUG_PRINT("error", ("no MyISAM handle for child table: '%s'.'%s' 0x%lx",
                         child->s->db.str, child->s->table_name.str,
                         (long) child));
    my_errno= HA_ERR_WRONG_MRG_TABLE_DEF;
  }
  DBUG_PRINT("myrg", ("MyISAM handle: 0x%lx  my_errno: %d",
                      my_errno ? NULL : (long) myisam, my_errno));

 err:
  DBUG_RETURN(my_errno ? NULL : myisam);
}


/**
  @brief Open a MERGE parent table, not its children.

  @detail This function initializes the MERGE storage engine structures
    and adds a child list of TABLE_LIST to the parent TABLE.

  @param[in]    name            MERGE table path name
  @param[in]    mode            read/write mode, unused
  @param[in]    test_if_locked  open flags

  @return       status
    @retval     0               OK
    @retval     -1              Error, my_errno gives reason
*/

int ha_myisammrg::open(const char *name, int mode __attribute__((unused)),
                       uint test_if_locked)
{
  DBUG_ENTER("ha_myisammrg::open");
  DBUG_PRINT("myrg", ("name: '%s'  table: 0x%lx", name, (long) table));
  DBUG_PRINT("myrg", ("test_if_locked: %u", test_if_locked));

  /* Save for later use. */
  this->test_if_locked= test_if_locked;

  /* retrieve children table list. */
  my_errno= 0;
  if (is_cloned)
  {
    /*
      Open and attaches the MyISAM tables,that are under the MERGE table 
      parent, on the MyISAM storage engine interface directly within the
      MERGE engine. The new MyISAM table instances, as well as the MERGE 
      clone itself, are not visible in the table cache. This is not a 
      problem because all locking is handled by the original MERGE table
      from which this is cloned of.
    */
    if (!(file= myrg_open(table->s->normalized_path.str, table->db_stat, 
                                       HA_OPEN_IGNORE_IF_LOCKED)))
    {
      DBUG_PRINT("error", ("my_errno %d", my_errno));
      DBUG_RETURN(my_errno ? my_errno : -1); 
    }

    file->children_attached= TRUE;

    info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  }
  else if (!(file= myrg_parent_open(name, myisammrg_parent_open_callback, this)))
  {
    DBUG_PRINT("error", ("my_errno %d", my_errno));
    DBUG_RETURN(my_errno ? my_errno : -1);
  }
  DBUG_PRINT("myrg", ("MYRG_INFO: 0x%lx", (long) file));
  DBUG_RETURN(0);
}

/**
   Returns a cloned instance of the current handler.

   @return A cloned handler instance.
 */
handler *ha_myisammrg::clone(MEM_ROOT *mem_root)
{
  MYRG_TABLE    *u_table,*newu_table;
  ha_myisammrg *new_handler= 
    (ha_myisammrg*) get_new_handler(table->s, mem_root, table->s->db_type());
  if (!new_handler)
    return NULL;
  
  /* Inform ha_myisammrg::open() that it is a cloned handler */
  new_handler->is_cloned= TRUE;
  /*
    Allocate handler->ref here because otherwise ha_open will allocate it
    on this->table->mem_root and we will not be able to reclaim that memory 
    when the clone handler object is destroyed.
  */
  if (!(new_handler->ref= (uchar*) alloc_root(mem_root, ALIGN_SIZE(ref_length)*2)))
  {
    delete new_handler;
    return NULL;
  }

  if (new_handler->ha_open(table, table->s->normalized_path.str, table->db_stat,
                            HA_OPEN_IGNORE_IF_LOCKED))
  {
    delete new_handler;
    return NULL;
  }
 
  /*
    Iterate through the original child tables and
    copy the state into the cloned child tables.
    We need to do this because all the child tables
    can be involved in delete.
  */
  newu_table= new_handler->file->open_tables;
  for (u_table= file->open_tables; u_table < file->end_table; u_table++)
  {
    newu_table->table->state= u_table->table->state;
    newu_table++;
  }

  return new_handler;
 }


/**
  @brief Attach children to a MERGE table.

  @detail Let the storage engine attach its children through a callback
    function. Check table definitions for consistency.

  @note Special thd->open_options may be in effect. We can make use of
    them in attach. I.e. we use HA_OPEN_FOR_REPAIR to report the names
    of mismatching child tables. We cannot transport these options in
    ha_myisammrg::test_if_locked because they may change after the
    parent is opened. The parent is kept open in the table cache over
    multiple statements and can be used by other threads. Open options
    can change over time.

  @return status
    @retval     0               OK
    @retval     != 0            Error, my_errno gives reason
*/

int ha_myisammrg::attach_children(void)
{
  MYRG_TABLE    *u_table;
  MI_COLUMNDEF  *recinfo;
  MI_KEYDEF     *keyinfo;
  uint          recs;
  uint          keys= table->s->keys;
  int           error;
  DBUG_ENTER("ha_myisammrg::attach_children");
  DBUG_PRINT("myrg", ("table: '%s'.'%s' 0x%lx", table->s->db.str,
                      table->s->table_name.str, (long) table));
  DBUG_PRINT("myrg", ("test_if_locked: %u", this->test_if_locked));
  DBUG_ASSERT(!this->file->children_attached);

  /*
    Initialize variables that are used, modified, and/or set by
    myisammrg_attach_children_callback().
    'next_child_attach' traverses the chain of TABLE_LIST objects
    that has been compiled during myrg_parent_open(). Every call
    to myisammrg_attach_children_callback() moves the pointer to
    the next object.
    'need_compat_check' is set by myisammrg_attach_children_callback()
    if a child fails the table def version check.
    'my_errno' is set by myisammrg_attach_children_callback() in
    case of an error.
  */
  next_child_attach= table->child_l;
  need_compat_check= FALSE;
  my_errno= 0;

  if (myrg_attach_children(this->file, this->test_if_locked |
                           current_thd->open_options,
                           myisammrg_attach_children_callback, this,
                           (my_bool *) &need_compat_check))
  {
    DBUG_PRINT("error", ("my_errno %d", my_errno));
    DBUG_RETURN(my_errno ? my_errno : -1);
  }
  DBUG_PRINT("myrg", ("calling myrg_extrafunc"));
  myrg_extrafunc(file, query_cache_invalidate_by_MyISAM_filename_ref);
  if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
	test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
    myrg_extra(file,HA_EXTRA_NO_WAIT_LOCK,0);
  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
  if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
    myrg_extra(file,HA_EXTRA_WAIT_LOCK,0);

  /*
    The compatibility check is required only if one or more children do
    not match their table def version from the last check. This will
    always happen at the first attach because the reference child def
    version is initialized to 'undefined' at open.
  */
  DBUG_PRINT("myrg", ("need_compat_check: %d", need_compat_check));
  if (need_compat_check)
  {
    TABLE_LIST *child_l;

    if (table->s->reclength != stats.mean_rec_length && stats.mean_rec_length)
    {
      DBUG_PRINT("error",("reclength: %lu  mean_rec_length: %lu",
                          table->s->reclength, stats.mean_rec_length));
      if (test_if_locked & HA_OPEN_FOR_REPAIR)
        myrg_print_wrong_table(file->open_tables->table->filename);
      error= HA_ERR_WRONG_MRG_TABLE_DEF;
      goto err;
    }
    /*
      Both recinfo and keyinfo are allocated by my_multi_malloc(), thus
      only recinfo must be freed.
    */
    if ((error= table2myisam(table, &keyinfo, &recinfo, &recs)))
    {
      /* purecov: begin inspected */
      DBUG_PRINT("error", ("failed to convert TABLE object to MyISAM "
                           "key and column definition"));
      goto err;
      /* purecov: end */
    }
    for (u_table= file->open_tables; u_table < file->end_table; u_table++)
    {
      if (check_definition(keyinfo, recinfo, keys, recs,
                           u_table->table->s->keyinfo, u_table->table->s->rec,
                           u_table->table->s->base.keys,
                           u_table->table->s->base.fields, false, NULL))
      {
        DBUG_PRINT("error", ("table definition mismatch: '%s'",
                             u_table->table->filename));
        error= HA_ERR_WRONG_MRG_TABLE_DEF;
        if (!(this->test_if_locked & HA_OPEN_FOR_REPAIR))
        {
          my_free((uchar*) recinfo, MYF(0));
          goto err;
        }
        myrg_print_wrong_table(u_table->table->filename);
      }
    }
    my_free((uchar*) recinfo, MYF(0));
    if (error == HA_ERR_WRONG_MRG_TABLE_DEF)
      goto err;

    /* All checks passed so far. Now update child def version. */
    for (child_l= table->child_l; ; child_l= child_l->next_global)
    {
      child_l->set_child_def_version(
        child_l->table->s->get_table_def_version());

      if (&child_l->next_global == table->child_last_l)
        break;
    }
  }
#if !defined(BIG_TABLES) || SIZEOF_OFF_T == 4
  /* Merge table has more than 2G rows */
  if (table->s->crashed)
  {
    DBUG_PRINT("error", ("MERGE table marked crashed"));
    error= HA_ERR_WRONG_MRG_TABLE_DEF;
    goto err;
  }
#endif
  DBUG_RETURN(0);

err:
  myrg_detach_children(file);
  DBUG_RETURN(my_errno= error);
}


/**
  @brief Detach all children from a MERGE table.

  @note Detach must not touch the children in any way.
    They may have been closed at ths point already.
    All references to the children should be removed.

  @return status
    @retval     0               OK
    @retval     != 0            Error, my_errno gives reason
*/

int ha_myisammrg::detach_children(void)
{
  DBUG_ENTER("ha_myisammrg::detach_children");
  DBUG_ASSERT(this->file && this->file->children_attached);

  if (myrg_detach_children(this->file))
  {
    /* purecov: begin inspected */
    DBUG_PRINT("error", ("my_errno %d", my_errno));
    DBUG_RETURN(my_errno ? my_errno : -1);
    /* purecov: end */
  }
  DBUG_RETURN(0);
}


/**
  @brief Close a MERGE parent table, not its children.

  @note The children are expected to be closed separately by the caller.

  @return status
    @retval     0               OK
    @retval     != 0            Error, my_errno gives reason
*/

int ha_myisammrg::close(void)
{
  int rc;
  DBUG_ENTER("ha_myisammrg::close");
  /*
    Children must not be attached here. Unless the MERGE table has no
    children or the handler instance has been cloned. In these cases 
    children_attached is always true. 
  */
  DBUG_ASSERT(!this->file->children_attached || !this->file->tables || this->is_cloned);
  rc= myrg_close(file);
  file= 0;
  DBUG_RETURN(rc);
}

int ha_myisammrg::write_row(uchar * buf)
{
  DBUG_ENTER("ha_myisammrg::write_row");
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_write_count);

  if (file->merge_insert_method == MERGE_INSERT_DISABLED || !file->tables)
    DBUG_RETURN(HA_ERR_TABLE_READONLY);

  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
    table->timestamp_field->set_time();
  if (table->next_number_field && buf == table->record[0])
  {
    int error;
    if ((error= update_auto_increment()))
      DBUG_RETURN(error); /* purecov: inspected */
  }
  DBUG_RETURN(myrg_write(file,buf));
}

int ha_myisammrg::update_row(const uchar * old_data, uchar * new_data)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_update_count);
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  return myrg_update(file,old_data,new_data);
}

int ha_myisammrg::delete_row(const uchar * buf)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_delete_count);
  return myrg_delete(file,buf);
}

int ha_myisammrg::index_read_map(uchar * buf, const uchar * key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=myrg_rkey(file,buf,active_index, key, keypart_map, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_read_idx_map(uchar * buf, uint index, const uchar * key,
                                     key_part_map keypart_map,
                                     enum ha_rkey_function find_flag)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=myrg_rkey(file,buf,index, key, keypart_map, find_flag);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_read_last_map(uchar *buf, const uchar *key,
                                      key_part_map keypart_map)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_key_count);
  int error=myrg_rkey(file,buf,active_index, key, keypart_map,
		      HA_READ_PREFIX_LAST);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_next(uchar * buf)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_next_count);
  int error=myrg_rnext(file,buf,active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_prev(uchar * buf)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_prev_count);
  int error=myrg_rprev(file,buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_first(uchar * buf)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_first_count);
  int error=myrg_rfirst(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_last(uchar * buf)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_last_count);
  int error=myrg_rlast(file, buf, active_index);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_myisammrg::index_next_same(uchar * buf,
                                  const uchar *key __attribute__((unused)),
                                  uint length __attribute__((unused)))
{
  int error;
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_next_count);
  do
  {
    error= myrg_rnext_same(file,buf);
  } while (error == HA_ERR_RECORD_DELETED);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}


int ha_myisammrg::rnd_init(bool scan)
{
  DBUG_ASSERT(this->file->children_attached);
  return myrg_reset(file);
}


int ha_myisammrg::rnd_next(uchar *buf)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  int error=myrg_rrnd(file, buf, HA_OFFSET_ERROR);
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}


int ha_myisammrg::rnd_pos(uchar * buf, uchar *pos)
{
  DBUG_ASSERT(this->file->children_attached);
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  int error=myrg_rrnd(file, buf, my_get_ptr(pos,ref_length));
  table->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_myisammrg::position(const uchar *record)
{
  DBUG_ASSERT(this->file->children_attached);
  ulonglong row_position= myrg_position(file);
  my_store_ptr(ref, ref_length, (my_off_t) row_position);
}


ha_rows ha_myisammrg::records_in_range(uint inx, key_range *min_key,
                                       key_range *max_key)
{
  DBUG_ASSERT(this->file->children_attached);
  return (ha_rows) myrg_records_in_range(file, (int) inx, min_key, max_key);
}


int ha_myisammrg::info(uint flag)
{
  MYMERGE_INFO mrg_info;
  DBUG_ASSERT(this->file->children_attached);
  (void) myrg_status(file,&mrg_info,flag);
  /*
    The following fails if one has not compiled MySQL with -DBIG_TABLES
    and one has more than 2^32 rows in the merge tables.
  */
  stats.records = (ha_rows) mrg_info.records;
  stats.deleted = (ha_rows) mrg_info.deleted;
#if !defined(BIG_TABLES) || SIZEOF_OFF_T == 4
  if ((mrg_info.records >= (ulonglong) 1 << 32) ||
      (mrg_info.deleted >= (ulonglong) 1 << 32))
    table->s->crashed= 1;
#endif
  stats.data_file_length= mrg_info.data_file_length;
  if (mrg_info.errkey >= (int) table_share->keys)
  {
    /*
     If value of errkey is higher than the number of keys
     on the table set errkey to MAX_KEY. This will be
     treated as unknown key case and error message generator
     won't try to locate key causing segmentation fault.
    */
    mrg_info.errkey= MAX_KEY;
  }
  table->s->keys_in_use.set_prefix(table->s->keys);
  stats.mean_rec_length= mrg_info.reclength;
  
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
  stats.block_size= 0;
  if (file->tables)
    stats.block_size= myisam_block_size / file->tables;
  
  stats.update_time= 0;
#if SIZEOF_OFF_T > 4
  ref_length=6;					// Should be big enough
#else
  ref_length=4;					// Can't be > than my_off_t
#endif
  if (flag & HA_STATUS_CONST)
  {
    if (table->s->key_parts && mrg_info.rec_per_key)
    {
#ifdef HAVE_purify
      /*
        valgrind may be unhappy about it, because optimizer may access values
        between file->keys and table->key_parts, that will be uninitialized.
        It's safe though, because even if opimizer will decide to use a key
        with such a number, it'll be an error later anyway.
      */
      bzero((char*) table->key_info[0].rec_per_key,
            sizeof(table->key_info[0].rec_per_key[0]) * table->s->key_parts);
#endif
      memcpy((char*) table->key_info[0].rec_per_key,
	     (char*) mrg_info.rec_per_key,
             sizeof(table->key_info[0].rec_per_key[0]) *
             min(file->keys, table->s->key_parts));
    }
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    errkey= mrg_info.errkey;
    my_store_ptr(dup_ref, ref_length, mrg_info.dupp_key_pos);
  }
  return 0;
}


int ha_myisammrg::extra(enum ha_extra_function operation)
{
  if (operation == HA_EXTRA_ATTACH_CHILDREN)
  {
    int rc= attach_children();
    if (!rc)
      (void) extra(HA_EXTRA_NO_READCHECK); // Not needed in SQL
    return(rc);
  }
  else if (operation == HA_EXTRA_DETACH_CHILDREN)
  {
    /*
      Note that detach must not touch the children in any way.
      They may have been closed at ths point already.
    */
    int rc= detach_children();
    return(rc);
  }

  /* As this is just a mapping, we don't have to force the underlying
     tables to be closed */
  if (operation == HA_EXTRA_FORCE_REOPEN ||
      operation == HA_EXTRA_PREPARE_FOR_DROP)
    return 0;
  return myrg_extra(file,operation,0);
}

int ha_myisammrg::reset(void)
{
  return myrg_reset(file);
}

/* To be used with WRITE_CACHE, EXTRA_CACHE and BULK_INSERT_BEGIN */

int ha_myisammrg::extra_opt(enum ha_extra_function operation, ulong cache_size)
{
  DBUG_ASSERT(this->file->children_attached);
  if ((specialflag & SPECIAL_SAFE_MODE) && operation == HA_EXTRA_WRITE_CACHE)
    return 0;
  return myrg_extra(file, operation, (void*) &cache_size);
}

int ha_myisammrg::external_lock(THD *thd, int lock_type)
{
  DBUG_ASSERT(this->file->children_attached);
  return myrg_lock_database(file,lock_type);
}

uint ha_myisammrg::lock_count(void) const
{
  /*
    Return the real lock count even if the children are not attached.
    This method is used for allocating memory. If we would return 0
    to another thread (e.g. doing FLUSH TABLE), and attach the children
    before the other thread calls store_lock(), then we would return
    more locks in store_lock() than we claimed by lock_count(). The
    other tread would overrun its memory.
  */
  return file->tables;
}


THR_LOCK_DATA **ha_myisammrg::store_lock(THD *thd,
					 THR_LOCK_DATA **to,
					 enum thr_lock_type lock_type)
{
  MYRG_TABLE *open_table;

  /*
    This method can be called while another thread is attaching the
    children. If the processor reorders instructions or write to memory,
    'children_attached' could be set before 'open_tables' has all the
    pointers to the children. Use of a mutex here and in
    myrg_attach_children() forces consistent data.
  */
  pthread_mutex_lock(&this->file->mutex);

  /*
    When MERGE table is open, but not yet attached, other threads
    could flush it, which means call mysql_lock_abort_for_thread()
    on this threads TABLE. 'children_attached' is FALSE in this
    situaton. Since the table is not locked, return no lock data.
  */
  if (!this->file->children_attached)
    goto end; /* purecov: tested */

  for (open_table=file->open_tables ;
       open_table != file->end_table ;
       open_table++)
  {
    *(to++)= &open_table->table->lock;
    if (lock_type != TL_IGNORE && open_table->table->lock.type == TL_UNLOCK)
      open_table->table->lock.type=lock_type;
  }

 end:
  pthread_mutex_unlock(&this->file->mutex);
  return to;
}


/* Find out database name and table name from a filename */

static void split_file_name(const char *file_name,
			    LEX_STRING *db, LEX_STRING *name)
{
  size_t dir_length, prefix_length;
  char buff[FN_REFLEN];

  db->length= 0;
  strmake(buff, file_name, sizeof(buff)-1);
  dir_length= dirname_length(buff);
  if (dir_length > 1)
  {
    /* Get database */
    buff[dir_length-1]= 0;			// Remove end '/'
    prefix_length= dirname_length(buff);
    db->str= (char*) file_name+ prefix_length;
    db->length= dir_length - prefix_length -1;
  }
  name->str= (char*) file_name+ dir_length;
  name->length= (uint) (fn_ext(name->str) - name->str);
}


void ha_myisammrg::update_create_info(HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_myisammrg::update_create_info");

  if (!(create_info->used_fields & HA_CREATE_USED_UNION))
  {
    MYRG_TABLE *open_table;
    THD *thd=current_thd;

    create_info->merge_list.next= &create_info->merge_list.first;
    create_info->merge_list.elements=0;

    for (open_table=file->open_tables ;
	 open_table != file->end_table ;
	 open_table++)
    {
      TABLE_LIST *ptr;
      LEX_STRING db, name;
      LINT_INIT(db.str);

      if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
	goto err;
      split_file_name(open_table->table->filename, &db, &name);
      if (!(ptr->table_name= thd->strmake(name.str, name.length)))
	goto err;
      if (db.length && !(ptr->db= thd->strmake(db.str, db.length)))
	goto err;

      create_info->merge_list.elements++;
      (*create_info->merge_list.next) = (uchar*) ptr;
      create_info->merge_list.next= (uchar**) &ptr->next_local;
    }
    *create_info->merge_list.next=0;
  }
  if (!(create_info->used_fields & HA_CREATE_USED_INSERT_METHOD))
  {
    create_info->merge_insert_method = file->merge_insert_method;
  }
  DBUG_VOID_RETURN;

err:
  create_info->merge_list.elements=0;
  create_info->merge_list.first=0;
  DBUG_VOID_RETURN;
}


int ha_myisammrg::create(const char *name, register TABLE *form,
			 HA_CREATE_INFO *create_info)
{
  char buff[FN_REFLEN];
  const char **table_names, **pos;
  TABLE_LIST *tables= (TABLE_LIST*) create_info->merge_list.first;
  THD *thd= current_thd;
  size_t dirlgt= dirname_length(name);
  DBUG_ENTER("ha_myisammrg::create");

  /* Allocate a table_names array in thread mem_root. */
  if (!(table_names= (const char**)
        thd->alloc((create_info->merge_list.elements+1) * sizeof(char*))))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  /* Create child path names. */
  for (pos= table_names; tables; tables= tables->next_local)
  {
    const char *table_name;

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
    uint length= build_table_filename(buff, sizeof(buff),
                                      tables->db, tables->table_name, "", 0);
    /*
      If a MyISAM table is in the same directory as the MERGE table,
      we use the table name without a path. This means that the
      DATADIR can easily be moved even for an embedded server as long
      as the MyISAM tables are from the same database as the MERGE table.
    */
    if ((dirname_length(buff) == dirlgt) && ! memcmp(buff, name, dirlgt))
      table_name= tables->table_name;
    else
      if (! (table_name= thd->strmake(buff, length)))
        DBUG_RETURN(HA_ERR_OUT_OF_MEM); /* purecov: inspected */

    *pos++= table_name;
  }
  *pos=0;

  /* Create a MERGE meta file from the table_names array. */
  DBUG_RETURN(myrg_create(fn_format(buff,name,"","",
                                    MY_RESOLVE_SYMLINKS|
                                    MY_UNPACK_FILENAME|MY_APPEND_EXT),
			  table_names,
                          create_info->merge_insert_method,
                          (my_bool) 0));
}


void ha_myisammrg::append_create_info(String *packet)
{
  const char *current_db;
  size_t db_length;
  THD *thd= current_thd;
  MYRG_TABLE *open_table, *first;

  if (file->merge_insert_method != MERGE_INSERT_DISABLED)
  {
    packet->append(STRING_WITH_LEN(" INSERT_METHOD="));
    packet->append(get_type(&merge_insert_method,file->merge_insert_method-1));
  }
  /*
    There is no sence adding UNION clause in case there is no underlying
    tables specified.
  */
  if (file->open_tables == file->end_table)
    return;
  packet->append(STRING_WITH_LEN(" UNION=("));

  current_db= table->s->db.str;
  db_length=  table->s->db.length;

  for (first=open_table=file->open_tables ;
       open_table != file->end_table ;
       open_table++)
  {
    LEX_STRING db, name;
    LINT_INIT(db.str);

    split_file_name(open_table->table->filename, &db, &name);
    if (open_table != first)
      packet->append(',');
    /* Report database for mapped table if it isn't in current database */
    if (db.length &&
	(db_length != db.length ||
	 strncmp(current_db, db.str, db.length)))
    {
      append_identifier(thd, packet, db.str, db.length);
      packet->append('.');
    }
    append_identifier(thd, packet, name.str, name.length);
  }
  packet->append(')');
}


bool ha_myisammrg::check_if_incompatible_data(HA_CREATE_INFO *info,
					      uint table_changes)
{
  /*
    For myisammrg, we should always re-generate the mapping file as this
    is trivial to do
  */
  return COMPATIBLE_DATA_NO;
}


int ha_myisammrg::check(THD* thd, HA_CHECK_OPT* check_opt)
{
  return HA_ADMIN_OK;
}


ha_rows ha_myisammrg::records()
{
  return myrg_records(file);
}


extern int myrg_panic(enum ha_panic_function flag);
int myisammrg_panic(handlerton *hton, ha_panic_function flag)
{
  return myrg_panic(flag);
}

static int myisammrg_init(void *p)
{
  handlerton *myisammrg_hton;

  myisammrg_hton= (handlerton *)p;

  myisammrg_hton->db_type= DB_TYPE_MRG_MYISAM;
  myisammrg_hton->create= myisammrg_create_handler;
  myisammrg_hton->panic= myisammrg_panic;
  myisammrg_hton->flags= HTON_NO_PARTITION;

  return 0;
}

struct st_mysql_storage_engine myisammrg_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(myisammrg)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &myisammrg_storage_engine,
  "MRG_MYISAM",
  "MySQL AB",
  "Collection of identical MyISAM tables",
  PLUGIN_LICENSE_GPL,
  myisammrg_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  0x0100, /* 1.0 */
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;
