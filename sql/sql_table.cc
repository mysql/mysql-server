/* Copyright (C) 2000-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* drop and alter of tables */

#include "mysql_priv.h"
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#include <hash.h>
#include <myisam.h>
#include <my_dir.h>

#ifdef __WIN__
#include <io.h>
#endif

const char *primary_key_name= "PRIMARY";

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static char *make_unique_key_name(const char *field_name,KEY *start,KEY *end);
static int copy_data_between_tables(TABLE *from,TABLE *to,
				    List<create_field> &create,
				    enum enum_duplicates handle_duplicates,
				    uint order_num, ORDER *order,
				    ha_rows *copied,ha_rows *deleted);

/*
 delete (drop) tables.

  SYNOPSIS
   mysql_rm_table()
   thd			Thread handle
   tables		List of tables to delete
   if_exists		If 1, don't give error if one table doesn't exists

  NOTES
    Will delete all tables that can be deleted and give a compact error
    messages for tables that could not be deleted.
    If a table is in use, we will wait for all users to free the table
    before dropping it

    Wait if global_read_lock (FLUSH TABLES WITH READ LOCK) is set.

  RETURN
    0		ok.  In this case ok packet is sent to user
    -1		Error  (Error message given but not sent to user)

*/

int mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists,
		   my_bool drop_temporary)
{
  int error= 0;
  DBUG_ENTER("mysql_rm_table");

  /* mark for close and remove all cached entries */

  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  VOID(pthread_mutex_lock(&LOCK_open));

  if (!drop_temporary && global_read_lock)
  {
    if (thd->global_read_lock)
    {
      my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,MYF(0),
	       tables->real_name);
      error= 1;
      goto err;
    }
    while (global_read_lock && ! thd->killed)
    {
      (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    }

  }
  error=mysql_rm_table_part2(thd,tables, if_exists, drop_temporary, 0);

 err:
  pthread_mutex_unlock(&LOCK_open);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  if (error)
    DBUG_RETURN(-1);
  send_ok(thd);
  DBUG_RETURN(0);
}


/*
 delete (drop) tables.

  SYNOPSIS
   mysql_rm_table_part2_with_lock()
   thd			Thread handle
   tables		List of tables to delete
   if_exists		If 1, don't give error if one table doesn't exists
   dont_log_query	Don't write query to log files

 NOTES
   Works like documented in mysql_rm_table(), but don't check
   global_read_lock and don't send_ok packet to server.

 RETURN
  0	ok
  1	error
*/

int mysql_rm_table_part2_with_lock(THD *thd,
				   TABLE_LIST *tables, bool if_exists,
				   bool drop_temporary, bool dont_log_query)
{
  int error;
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  VOID(pthread_mutex_lock(&LOCK_open));

  error=mysql_rm_table_part2(thd,tables, if_exists, drop_temporary,
			     dont_log_query);

  pthread_mutex_unlock(&LOCK_open);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  return error;
}


/*
  Execute the drop of a normal or temporary table

  SYNOPSIS
    mysql_rm_table_part2()
    thd			Thread handler
    tables		Tables to drop
    if_exists		If set, don't give an error if table doesn't exists.
			In this case we give an warning of level 'NOTE'
    drop_temporary	Only drop temporary tables
    dont_log_query	Don't log the query

  TODO:
    When logging to the binary log, we should log
    tmp_tables and transactional tables as separate statements if we
    are in a transaction;  This is needed to get these tables into the
    cached binary log that is only written on COMMIT.

   The current code only writes DROP statements that only uses temporary
   tables to the cache binary log.  This should be ok on most cases, but
   not all.

 RETURN
   0	ok
   1	Error
   -1	Thread was killed
*/

int mysql_rm_table_part2(THD *thd, TABLE_LIST *tables, bool if_exists,
			 bool drop_temporary, bool dont_log_query)
{
  TABLE_LIST *table;
  char	path[FN_REFLEN], *alias;
  String wrong_tables;
  int error;
  bool some_tables_deleted=0, tmp_table_deleted=0, foreign_key_error=0;
  DBUG_ENTER("mysql_rm_table_part2");

  if (lock_table_names(thd, tables))
    DBUG_RETURN(1);

  for (table=tables ; table ; table=table->next)
  {
    char *db=table->db;
    mysql_ha_close(thd, table, /*dont_send_ok*/ 1, /*dont_lock*/ 1);
    if (!close_temporary_table(thd, db, table->real_name))
    {
      tmp_table_deleted=1;
      continue;					// removed temporary table
    }

    error=0;
    if (!drop_temporary)
    {
      abort_locked_tables(thd,db,table->real_name);
      while (remove_table_from_cache(thd,db,table->real_name) && !thd->killed)
      {
	dropping_tables++;
	(void) pthread_cond_wait(&COND_refresh,&LOCK_open);
	dropping_tables--;
      }
      drop_locked_tables(thd,db,table->real_name);
      if (thd->killed)
	DBUG_RETURN(-1);
      alias= (lower_case_table_names == 2) ? table->alias : table->real_name;
      /* remove form file and isam files */
      strxmov(path, mysql_data_home, "/", db, "/", alias, reg_ext, NullS);
      (void) unpack_filename(path,path);
    }
    if (drop_temporary || access(path,F_OK))
    {
      if (if_exists)
	push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			    ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
			    table->real_name);
      else
	error= 1;
    }
    else
    {
      char *end;
      db_type table_type= get_table_type(path);
      *(end=fn_ext(path))=0;			// Remove extension for delete
      error=ha_delete_table(table_type, path);
      if (error == ENOENT && if_exists)
	error = 0;
      if (error == HA_ERR_ROW_IS_REFERENCED)
      {
	/* the table is referenced by a foreign key constraint */
	foreign_key_error=1;
      }
      if (!error || error == ENOENT)
      {
	/* Delete the table definition file */
	strmov(end,reg_ext);
	if (!(error=my_delete(path,MYF(MY_WME))))
	  some_tables_deleted=1;
      }
    }
    if (error)
    {
      if (wrong_tables.length())
	wrong_tables.append(',');
      wrong_tables.append(String(table->real_name,system_charset_info));
    }
  }
  thd->tmp_table_used= tmp_table_deleted;
  error= 0;
  if (wrong_tables.length())
  {
    if (!foreign_key_error)
      my_error(ER_BAD_TABLE_ERROR,MYF(0), wrong_tables.c_ptr());
    else
      my_error(ER_ROW_IS_REFERENCED, MYF(0));
    error= 1;
  }

  if (some_tables_deleted || tmp_table_deleted || !error)
  {
    query_cache_invalidate3(thd, tables, 0);
    if (!dont_log_query)
    {
      mysql_update_log.write(thd, thd->query,thd->query_length);
      if (mysql_bin_log.is_open())
      {
        if (!error)
          thd->clear_error();
	Query_log_event qinfo(thd, thd->query, thd->query_length,
			      tmp_table_deleted && !some_tables_deleted);
	mysql_bin_log.write(&qinfo);
      }
    }
  }

  unlock_table_names(thd, tables);
  DBUG_RETURN(error);
}


int quick_rm_table(enum db_type base,const char *db,
		   const char *table_name)
{
  char path[FN_REFLEN];
  int error=0;
  my_snprintf(path, sizeof(path), "%s/%s/%s%s",
	      mysql_data_home, db, table_name, reg_ext);
  unpack_filename(path,path);
  if (my_delete(path,MYF(0)))
    error=1; /* purecov: inspected */
  my_snprintf(path, sizeof(path), "%s/%s/%s", mysql_data_home, db, table_name);
  unpack_filename(path,path);
  return ha_delete_table(base,path) || error;
}

/*
  Sort keys in the following order:
  - PRIMARY KEY
  - UNIQUE keyws where all column are NOT NULL
  - Other UNIQUE keys
  - Normal keys
  - Fulltext keys

  This will make checking for duplicated keys faster and ensure that
  PRIMARY keys are prioritized.
*/

static int sort_keys(KEY *a, KEY *b)
{
  if (a->flags & HA_NOSAME)
  {
    if (!(b->flags & HA_NOSAME))
      return -1;
    if ((a->flags ^ b->flags) & (HA_NULL_PART_KEY | HA_END_SPACE_KEY))
    {
      /* Sort NOT NULL keys before other keys */
      return (a->flags & (HA_NULL_PART_KEY | HA_END_SPACE_KEY)) ? 1 : -1;
    }
    if (a->name == primary_key_name)
      return -1;
    if (b->name == primary_key_name)
      return 1;
  }
  else if (b->flags & HA_NOSAME)
    return 1;					// Prefer b

  if ((a->flags ^ b->flags) & HA_FULLTEXT)
  {
    return (a->flags & HA_FULLTEXT) ? 1 : -1;
  }
  /*
    Prefer original key order.	usable_key_parts contains here
    the original key position.
  */
  return ((a->usable_key_parts < b->usable_key_parts) ? -1 :
	  (a->usable_key_parts > b->usable_key_parts) ? 1 :
	  0);
}

/*
  Check TYPELIB (set or enum) for duplicates

  SYNOPSIS
    check_duplicates_in_interval()
    set_or_name   "SET" or "ENUM" string for warning message
    name	  name of the checked column
    typelib	  list of values for the column

  DESCRIPTION
    This function prints an warning for each value in list
    which has some duplicates on its right

  RETURN VALUES
    void
*/

void check_duplicates_in_interval(const char *set_or_name,
				  const char *name, TYPELIB *typelib)
{
  unsigned int old_count= typelib->count;
  const char **old_type_names= typelib->type_names;

  old_count= typelib->count;
  old_type_names= typelib->type_names;
  const char **cur_value= typelib->type_names;
  for ( ; typelib->count > 1; cur_value++)
  {
    typelib->type_names++;
    typelib->count--;
    if (find_type((char*)*cur_value,typelib,1))
    {
      push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_DUPLICATED_VALUE_IN_TYPE,
			  ER(ER_DUPLICATED_VALUE_IN_TYPE),
			  name,*cur_value,set_or_name);
    }
  }
  typelib->count= old_count;
  typelib->type_names= old_type_names;
}

/*
  Preparation for table creation

  SYNOPSIS
    mysql_prepare_table()
    thd			Thread object
    create_info		Create information (like MAX_ROWS)
    fields		List of fields to create
    keys		List of keys to create

  DESCRIPTION
    Prepares the table and key structures for table creation.

  RETURN VALUES
    0	ok
    -1	error
*/

int mysql_prepare_table(THD *thd, HA_CREATE_INFO *create_info,
			List<create_field> &fields,
			List<Key> &keys, bool tmp_table, uint &db_options,
			handler *file, KEY *&key_info_buffer,
			uint *key_count, int select_field_count)
{
  const char	*key_name;
  create_field	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns;
  ulong		pos;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  DBUG_ENTER("mysql_prepare_table");

  List_iterator<create_field> it(fields),it2(fields);
  select_field_pos=fields.elements - select_field_count;
  null_fields=blob_columns=0;

  for (field_no=0; (sql_field=it++) ; field_no++)
  {
    if (!sql_field->charset)
      sql_field->charset= create_info->default_table_charset;
    /*
      table_charset is set in ALTER TABLE if we want change character set
      for all varchar/char columns.
      But the table charset must not affect the BLOB fields, so don't
      allow to change my_charset_bin to somethig else.
    */
    if (create_info->table_charset && sql_field->charset != &my_charset_bin)
      sql_field->charset= create_info->table_charset;

    CHARSET_INFO *savecs= sql_field->charset;
    if ((sql_field->flags & BINCMP_FLAG) &&
	!(sql_field->charset= get_charset_by_csname(sql_field->charset->csname,
						    MY_CS_BINSORT,MYF(0))))
    {
      char tmp[64];
      strmake(strmake(tmp, savecs->csname, sizeof(tmp)-4), "_bin", 4);
      my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
      DBUG_RETURN(-1);
    }

    sql_field->create_length_to_internal_length();

    /* Don't pack keys in old tables if the user has requested this */
    if ((sql_field->flags & BLOB_FLAG) ||
	sql_field->sql_type == FIELD_TYPE_VAR_STRING &&
	create_info->row_type != ROW_TYPE_FIXED)
    {
      db_options|=HA_OPTION_PACK_RECORD;
    }
    if (!(sql_field->flags & NOT_NULL_FLAG))
      null_fields++;

    if (check_column_name(sql_field->field_name))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
      DBUG_RETURN(-1);
    }

    /* Check if we have used the same field name before */
    for (dup_no=0; (dup_field=it2++) != sql_field; dup_no++)
    {
      if (my_strcasecmp(system_charset_info,
			sql_field->field_name,
			dup_field->field_name) == 0)
      {
	/*
	  If this was a CREATE ... SELECT statement, accept a field
	  redefinition if we are changing a field in the SELECT part
	*/
	if (field_no < select_field_pos || dup_no >= select_field_pos)
	{
	  my_error(ER_DUP_FIELDNAME,MYF(0),sql_field->field_name);
	  DBUG_RETURN(-1);
	}
	else
	{
	  /* Field redefined */
	  sql_field->sql_type=		dup_field->sql_type;
	  sql_field->charset=		(dup_field->charset ?
					 dup_field->charset :
					 create_info->default_table_charset);
	  sql_field->length=		dup_field->length;
	  sql_field->pack_length=	dup_field->pack_length;
	  sql_field->create_length_to_internal_length();
	  sql_field->decimals=		dup_field->decimals;
	  sql_field->flags=		dup_field->flags;
	  sql_field->unireg_check=	dup_field->unireg_check;
	  it2.remove();			// Remove first (create) definition
	  select_field_pos--;
	  break;
	}
      }
    }
    it2.rewind();
  }
  /* If fixed row records, we need one bit to check for deleted rows */
  if (!(db_options & HA_OPTION_PACK_RECORD))
    null_fields++;
  pos=(null_fields+7)/8;

  it.rewind();
  while ((sql_field=it++))
  {
    DBUG_ASSERT(sql_field->charset);

    switch (sql_field->sql_type) {
    case FIELD_TYPE_BLOB:
    case FIELD_TYPE_MEDIUM_BLOB:
    case FIELD_TYPE_TINY_BLOB:
    case FIELD_TYPE_LONG_BLOB:
      sql_field->pack_flag=FIELDFLAG_BLOB |
	pack_length_to_packflag(sql_field->pack_length -
				portable_sizeof_char_ptr);
      if (sql_field->charset->state & MY_CS_BINSORT)
	sql_field->pack_flag|=FIELDFLAG_BINARY;
      sql_field->length=8;			// Unireg field length
      sql_field->unireg_check=Field::BLOB_FIELD;
      blob_columns++;
      break;
    case FIELD_TYPE_GEOMETRY:
#ifdef HAVE_SPATIAL
      if (!(file->table_flags() & HA_CAN_GEOMETRY))
      {
	my_printf_error(ER_CHECK_NOT_IMPLEMENTED, ER(ER_CHECK_NOT_IMPLEMENTED),
			MYF(0), "GEOMETRY");
	DBUG_RETURN(-1);
      }
      sql_field->pack_flag=FIELDFLAG_GEOM |
	pack_length_to_packflag(sql_field->pack_length -
				portable_sizeof_char_ptr);
      if (sql_field->charset->state & MY_CS_BINSORT)
	sql_field->pack_flag|=FIELDFLAG_BINARY;
      sql_field->length=8;			// Unireg field length
      sql_field->unireg_check=Field::BLOB_FIELD;
      blob_columns++;
      break;
#else
      my_printf_error(ER_FEATURE_DISABLED,ER(ER_FEATURE_DISABLED), MYF(0),
		      sym_group_geom.name, sym_group_geom.needed_define);
      DBUG_RETURN(-1);
#endif /*HAVE_SPATIAL*/
    case FIELD_TYPE_VAR_STRING:
    case FIELD_TYPE_STRING:
      sql_field->pack_flag=0;
      if (sql_field->charset->state & MY_CS_BINSORT)
	sql_field->pack_flag|=FIELDFLAG_BINARY;
      break;
    case FIELD_TYPE_ENUM:
      sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
	FIELDFLAG_INTERVAL;
      if (sql_field->charset->state & MY_CS_BINSORT)
	sql_field->pack_flag|=FIELDFLAG_BINARY;
      sql_field->unireg_check=Field::INTERVAL_FIELD;
      check_duplicates_in_interval("ENUM",sql_field->field_name,
				   sql_field->interval);
      break;
    case FIELD_TYPE_SET:
      sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
	FIELDFLAG_BITFIELD;
      if (sql_field->charset->state & MY_CS_BINSORT)
	sql_field->pack_flag|=FIELDFLAG_BINARY;
      sql_field->unireg_check=Field::BIT_FIELD;
      check_duplicates_in_interval("SET",sql_field->field_name,
				   sql_field->interval);
      break;
    case FIELD_TYPE_DATE:			// Rest of string types
    case FIELD_TYPE_NEWDATE:
    case FIELD_TYPE_TIME:
    case FIELD_TYPE_DATETIME:
    case FIELD_TYPE_NULL:
      sql_field->pack_flag=f_settype((uint) sql_field->sql_type);
      break;
    case FIELD_TYPE_TIMESTAMP:
      /* We should replace old TIMESTAMP fields with their newer analogs */
      if (sql_field->unireg_check == Field::TIMESTAMP_OLD_FIELD)
      {
	if (!timestamps)
	{
	  sql_field->unireg_check= Field::TIMESTAMP_DNUN_FIELD;
	  timestamps_with_niladic++;
	}
	else
	  sql_field->unireg_check= Field::NONE;
      }
      else if (sql_field->unireg_check != Field::NONE)
	timestamps_with_niladic++;

      timestamps++;
      /* fall-through */
    default:
      sql_field->pack_flag=(FIELDFLAG_NUMBER |
			    (sql_field->flags & UNSIGNED_FLAG ? 0 :
			     FIELDFLAG_DECIMAL) |
			    (sql_field->flags & ZEROFILL_FLAG ?
			     FIELDFLAG_ZEROFILL : 0) |
			    f_settype((uint) sql_field->sql_type) |
			    (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
      break;
    }
    if (!(sql_field->flags & NOT_NULL_FLAG))
      sql_field->pack_flag|=FIELDFLAG_MAYBE_NULL;
    sql_field->offset= pos;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    pos+=sql_field->pack_length;
  }
  if (timestamps_with_niladic > 1)
  {
    my_error(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment > 1)
  {
    my_error(ER_WRONG_AUTO_KEY,MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment &&
      (file->table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_error(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,MYF(0));
    DBUG_RETURN(-1);
  }

  if (blob_columns && (file->table_flags() & HA_NO_BLOBS))
  {
    my_error(ER_TABLE_CANT_HANDLE_BLOB,MYF(0));
    DBUG_RETURN(-1);
  }

  /* Create keys */

  List_iterator<Key> key_iterator(keys), key_iterator2(keys);
  uint key_parts=0, fk_key_count=0;
  bool primary_key=0,unique_key=0;
  Key *key, *key2;
  uint tmp, key_number;
  /* special marker for keys to be ignored */
  static char ignore_key[1];

  /* Calculate number of key segements */
  *key_count= 0;

  while ((key=key_iterator++))
  {
    if (key->type == Key::FOREIGN_KEY)
    {
      fk_key_count++;
      foreign_key *fk_key= (foreign_key*) key;
      if (fk_key->ref_columns.elements &&
	  fk_key->ref_columns.elements != fk_key->columns.elements)
      {
	my_error(ER_WRONG_FK_DEF, MYF(0), fk_key->name ? fk_key->name :
		 "foreign key without name",
		 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	DBUG_RETURN(-1);
      }
      continue;
    }
    (*key_count)++;
    tmp=file->max_key_parts();
    if (key->columns.elements > tmp)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      DBUG_RETURN(-1);
    }
    if (key->name && strlen(key->name) > NAME_LEN)
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), key->name);
      DBUG_RETURN(-1);
    }
    key_iterator2.rewind ();
    if (key->type != Key::FOREIGN_KEY)
    {
      while ((key2 = key_iterator2++) != key)
      {
	/*
          foreign_key_prefix(key, key2) returns 0 if key or key2, or both, is
          'generated', and a generated key is a prefix of the other key.
          Then we do not need the generated shorter key.
        */
        if ((key2->type != Key::FOREIGN_KEY &&
             key2->name != ignore_key &&
             !foreign_key_prefix(key, key2)))
        {
          /* TODO: issue warning message */
          /* mark that the generated key should be ignored */
          if (!key2->generated ||
              (key->generated && key->columns.elements <
               key2->columns.elements))
            key->name= ignore_key;
          else
          {
            key2->name= ignore_key;
            key_parts-= key2->columns.elements;
            (*key_count)--;
          }
          break;
        }
      }
    }
    if (key->name != ignore_key)
      key_parts+=key->columns.elements;
    else
      (*key_count)--;
    if (key->name && !tmp_table &&
	!my_strcasecmp(system_charset_info,key->name,primary_key_name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name);
      DBUG_RETURN(-1);
    }
  }
  tmp=file->max_keys();
  if (*key_count > tmp)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),tmp);
    DBUG_RETURN(-1);
  }

  key_info_buffer=key_info=(KEY*) sql_calloc(sizeof(KEY)* *key_count);
  key_part_info=(KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO)*key_parts);
  if (!key_info_buffer || ! key_part_info)
    DBUG_RETURN(-1);				// Out of memory

  key_iterator.rewind();
  key_number=0;
  for (; (key=key_iterator++) ; key_number++)
  {
    uint key_length=0;
    key_part_spec *column;

    if (key->name == ignore_key)
    {
      /* ignore redundant keys */
      do
	key=key_iterator++;
      while (key && key->name == ignore_key);
      if (!key)
	break;
    }

    switch(key->type){
    case Key::MULTIPLE:
	key_info->flags= 0;
	break;
    case Key::FULLTEXT:
	key_info->flags= HA_FULLTEXT;
	break;
    case Key::SPATIAL:
#ifdef HAVE_SPATIAL
	key_info->flags= HA_SPATIAL;
	break;
#else
	my_printf_error(ER_FEATURE_DISABLED,ER(ER_FEATURE_DISABLED),MYF(0),
			sym_group_geom.name, sym_group_geom.needed_define);
	DBUG_RETURN(-1);
#endif
    case Key::FOREIGN_KEY:
      key_number--;				// Skip this key
      continue;
    default:
      key_info->flags = HA_NOSAME;
      break;
    }
    if (key->generated)
      key_info->flags|= HA_GENERATED_KEY;

    key_info->key_parts=(uint8) key->columns.elements;
    key_info->key_part=key_part_info;
    key_info->usable_key_parts= key_number;
    key_info->algorithm=key->algorithm;

    if (key->type == Key::FULLTEXT)
    {
      if (!(file->table_flags() & HA_CAN_FULLTEXT))
      {
	my_error(ER_TABLE_CANT_HANDLE_FT, MYF(0));
	DBUG_RETURN(-1);
      }
    }
    /*
       Make SPATIAL to be RTREE by default
       SPATIAL only on BLOB or at least BINARY, this
       actually should be replaced by special GEOM type
       in near future when new frm file is ready
       checking for proper key parts number:
    */

    /* TODO: Add proper checks if handler supports key_type and algorithm */
    if (key_info->flags & HA_SPATIAL)
    {
      if (key_info->key_parts != 1)
      {
	my_printf_error(ER_WRONG_ARGUMENTS,
			ER(ER_WRONG_ARGUMENTS),MYF(0),"SPATIAL INDEX");
	DBUG_RETURN(-1);
      }
    }
    else
    if (key_info->algorithm == HA_KEY_ALG_RTREE)
    {
#ifdef HAVE_RTREE_KEYS
      if ((key_info->key_parts & 1) == 1)
      {
	my_printf_error(ER_WRONG_ARGUMENTS,
			ER(ER_WRONG_ARGUMENTS),MYF(0),"RTREE INDEX");
	DBUG_RETURN(-1);
      }
      /* TODO: To be deleted */
      my_printf_error(ER_NOT_SUPPORTED_YET, ER(ER_NOT_SUPPORTED_YET),
		      MYF(0), "RTREE INDEX");
      DBUG_RETURN(-1);
#else
      my_printf_error(ER_FEATURE_DISABLED,ER(ER_FEATURE_DISABLED),MYF(0),
		      sym_group_rtree.name, sym_group_rtree.needed_define);
      DBUG_RETURN(-1);
#endif
    }

    List_iterator<key_part_spec> cols(key->columns);
    CHARSET_INFO *ft_key_charset=0;  // for FULLTEXT
    for (uint column_nr=0 ; (column=cols++) ; column_nr++)
    {
      it.rewind();
      field=0;
      while ((sql_field=it++) &&
	     my_strcasecmp(system_charset_info,
			   column->field_name,
			   sql_field->field_name))
	field++;
      if (!sql_field)
      {
	my_printf_error(ER_KEY_COLUMN_DOES_NOT_EXITS,
			ER(ER_KEY_COLUMN_DOES_NOT_EXITS),MYF(0),
			column->field_name);
	DBUG_RETURN(-1);
      }
      /* for fulltext keys keyseg length is 1 for blobs (it's ignored in
	 ft code anyway, and 0 (set to column width later) for char's.
	 it has to be correct col width for char's, as char data are not
	 prefixed with length (unlike blobs, where ft code takes data length
	 from a data prefix, ignoring column->length).
      */
      if (key->type == Key::FULLTEXT)
      {
	if ((sql_field->sql_type != FIELD_TYPE_STRING &&
	     sql_field->sql_type != FIELD_TYPE_VAR_STRING &&
	     !f_is_blob(sql_field->pack_flag)) ||
	    sql_field->charset == &my_charset_bin ||
	    sql_field->charset->mbminlen > 1 || // ucs2 doesn't work yet
	    (ft_key_charset && sql_field->charset != ft_key_charset))
	{
	    my_printf_error(ER_BAD_FT_COLUMN,ER(ER_BAD_FT_COLUMN),MYF(0),
			    column->field_name);
	    DBUG_RETURN(-1);
	}
	ft_key_charset=sql_field->charset;
	/*
	  for fulltext keys keyseg length is 1 for blobs (it's ignored in ft
	  code anyway, and 0 (set to column width later) for char's. it has
	  to be correct col width for char's, as char data are not prefixed
	  with length (unlike blobs, where ft code takes data length from a
	  data prefix, ignoring column->length).
	*/
	column->length=test(f_is_blob(sql_field->pack_flag));
      }
      else
      {
	column->length*= sql_field->charset->mbmaxlen;

	if (f_is_blob(sql_field->pack_flag))
	{
	  if (!(file->table_flags() & HA_CAN_INDEX_BLOBS))
	  {
	    my_printf_error(ER_BLOB_USED_AS_KEY,ER(ER_BLOB_USED_AS_KEY),MYF(0),
			    column->field_name);
	    DBUG_RETURN(-1);
	  }
	  if (!column->length)
	  {
	    my_printf_error(ER_BLOB_KEY_WITHOUT_LENGTH,
			    ER(ER_BLOB_KEY_WITHOUT_LENGTH),MYF(0),
			    column->field_name);
	    DBUG_RETURN(-1);
	  }
	}
#ifdef HAVE_SPATIAL
	if (key->type  == Key::SPATIAL)
	{
	  if (!column->length )
	  {
	    /*
	    BAR: 4 is: (Xmin,Xmax,Ymin,Ymax), this is for 2D case
		 Lately we'll extend this code to support more dimensions
	    */
	    column->length=4*sizeof(double);
	  }
	}
#endif
	if (!(sql_field->flags & NOT_NULL_FLAG))
	{
	  if (key->type == Key::PRIMARY)
	  {
	    /* Implicitly set primary key fields to NOT NULL for ISO conf. */
	    sql_field->flags|= NOT_NULL_FLAG;
	    sql_field->pack_flag&= ~FIELDFLAG_MAYBE_NULL;
	  }
	  else
	     key_info->flags|= HA_NULL_PART_KEY;
	  if (!(file->table_flags() & HA_NULL_IN_KEY))
	  {
	    my_printf_error(ER_NULL_COLUMN_IN_INDEX,ER(ER_NULL_COLUMN_IN_INDEX),
			    MYF(0),column->field_name);
	    DBUG_RETURN(-1);
	  }
	  if (key->type == Key::SPATIAL)
	  {
	    my_error(ER_SPATIAL_CANT_HAVE_NULL, MYF(0));
	    DBUG_RETURN(-1);
	  }
	}
	if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
	{
	  if (column_nr == 0 || (file->table_flags() & HA_AUTO_PART_KEY))
	    auto_increment--;			// Field is used
	}
      }

      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16) sql_field->offset;
      key_part_info->key_type=sql_field->pack_flag;
      uint length=sql_field->pack_length;
      if (column->length)
      {
	if (f_is_blob(sql_field->pack_flag))
	{
	  if ((length=column->length) > file->max_key_length() ||
	      length > file->max_key_part_length())
	  {
	    length=min(file->max_key_length(), file->max_key_part_length());
	    if (key->type == Key::MULTIPLE)
	    {
	      /* not a critical problem */
	      char warn_buff[MYSQL_ERRMSG_SIZE];
	      my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
			  length);
	      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			   ER_TOO_LONG_KEY, warn_buff);
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY,MYF(0),length);
	      DBUG_RETURN(-1);
	    }
	  }
	}
	else if (!f_is_geom(sql_field->pack_flag) &&
		  (column->length > length ||
		   ((f_is_packed(sql_field->pack_flag) ||
		     ((file->table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
		      (key_info->flags & HA_NOSAME))) &&
		    column->length != length)))
	{
	  my_error(ER_WRONG_SUB_KEY,MYF(0));
	  DBUG_RETURN(-1);
	}
	else if (!(file->table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  length=column->length;
      }
      else if (length == 0)
      {
	my_printf_error(ER_WRONG_KEY_COLUMN, ER(ER_WRONG_KEY_COLUMN), MYF(0),
			column->field_name);
	  DBUG_RETURN(-1);
      }
      if (length > file->max_key_part_length())
      {
	length=file->max_key_part_length();
	if (key->type == Key::MULTIPLE)
	{
	  /* not a critical problem */
	  char warn_buff[MYSQL_ERRMSG_SIZE];
	  my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
		      length);
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		       ER_TOO_LONG_KEY, warn_buff);
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY,MYF(0),length);
	  DBUG_RETURN(-1);
	}
      }
      key_part_info->length=(uint16) length;
      /* Use packed keys for long strings on the first column */
      if (!(db_options & HA_OPTION_NO_PACK_KEYS) &&
	  (length >= KEY_DEFAULT_PACK_LENGTH &&
	   (sql_field->sql_type == FIELD_TYPE_STRING ||
	    sql_field->sql_type == FIELD_TYPE_VAR_STRING ||
	    sql_field->pack_flag & FIELDFLAG_BLOB)))
      {
	if (column_nr == 0 && (sql_field->pack_flag & FIELDFLAG_BLOB))
	  key_info->flags|= HA_BINARY_PACK_KEY;
	else
	  key_info->flags|= HA_PACK_KEY;
      }
      key_length+=length;
      key_part_info++;

      /* Create the key name based on the first column (if not given) */
      if (column_nr == 0)
      {
	if (key->type == Key::PRIMARY)
	{
	  if (primary_key)
	  {
	    my_error(ER_MULTIPLE_PRI_KEY,MYF(0));
	    DBUG_RETURN(-1);
	  }
	  key_name=primary_key_name;
	  primary_key=1;
	}
	else if (!(key_name = key->name))
	  key_name=make_unique_key_name(sql_field->field_name,
					key_info_buffer,key_info);
	if (check_if_keyname_exists(key_name,key_info_buffer,key_info))
	{
	  my_error(ER_DUP_KEYNAME,MYF(0),key_name);
	  DBUG_RETURN(-1);
	}
	key_info->name=(char*) key_name;
      }
    }
    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      DBUG_RETURN(-1);
    }
    if (!(key_info->flags & HA_NULL_PART_KEY))
      unique_key=1;
    key_info->key_length=(uint16) key_length;
    uint max_key_length= file->max_key_length();
    if (key_length > max_key_length && key->type != Key::FULLTEXT)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      DBUG_RETURN(-1);
    }
    key_info++;
  }
  if (!unique_key && !primary_key &&
      (file->table_flags() & HA_REQUIRE_PRIMARY_KEY))
  {
    my_error(ER_REQUIRES_PRIMARY_KEY,MYF(0));
    DBUG_RETURN(-1);
  }
  if (auto_increment > 0)
  {
    my_error(ER_WRONG_AUTO_KEY,MYF(0));
    DBUG_RETURN(-1);
  }
  /* Sort keys in optimized order */
  qsort((gptr) key_info_buffer, *key_count, sizeof(KEY),
	(qsort_cmp) sort_keys);

  DBUG_RETURN(0);
}


/*
  Create a table

  SYNOPSIS
    mysql_create_table()
    thd			Thread object
    db			Database
    table_name		Table name
    create_info		Create information (like MAX_ROWS)
    fields		List of fields to create
    keys		List of keys to create
    tmp_table		Set to 1 if this is an internal temporary table
			(From ALTER TABLE)

  DESCRIPTION
    If one creates a temporary table, this is automaticly opened

    no_log is needed for the case of CREATE ... SELECT,
    as the logging will be done later in sql_insert.cc
    select_field_count is also used for CREATE ... SELECT,
    and must be zero for standard create of table.

  RETURN VALUES
    0	ok
    -1	error
*/

int mysql_create_table(THD *thd,const char *db, const char *table_name,
		       HA_CREATE_INFO *create_info,
		       List<create_field> &fields,
		       List<Key> &keys,bool tmp_table,
		       uint select_field_count)
{
  char		path[FN_REFLEN];
  const char	*alias;
  int		error= -1;
  uint		db_options, key_count;
  KEY		*key_info_buffer;
  handler	*file;
  enum db_type	new_db_type;
  DBUG_ENTER("mysql_create_table");

  /* Check for duplicate fields and check type of table to create */
  if (!fields.elements)
  {
    my_error(ER_TABLE_MUST_HAVE_COLUMNS,MYF(0));
    DBUG_RETURN(-1);
  }
  if ((new_db_type= ha_checktype(create_info->db_type)) !=
      create_info->db_type)
  {
    create_info->db_type= new_db_type;
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_USING_OTHER_HANDLER,
			ER(ER_WARN_USING_OTHER_HANDLER),
			ha_get_storage_engine(new_db_type),
			table_name);
  }
  db_options=create_info->table_options;
  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    db_options|=HA_OPTION_PACK_RECORD;
  alias= table_case_name(create_info, table_name);
  file=get_new_handler((TABLE*) 0, create_info->db_type);

#ifdef NOT_USED
  /*
    if there is a technical reason for a handler not to have support
    for temp. tables this code can be re-enabled.
    Otherwise, if a handler author has a wish to prohibit usage of
    temporary tables for his handler he should implement a check in
    ::create() method
  */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      (file->table_flags() & HA_NO_TEMP_TABLES))
  {
    my_error(ER_ILLEGAL_HA,MYF(0),table_name);
    DBUG_RETURN(-1);
  }
#endif

  /*
    If the table character set was not given explicitely,
    let's fetch the database default character set and
    apply it to the table.
  */
  if (!create_info->default_table_charset)
  {
    HA_CREATE_INFO db_info;
    uint length;
    char  path[FN_REFLEN];
    strxmov(path, mysql_data_home, "/", db, NullS);
    length= unpack_dirname(path,path);             // Convert if not unix
    strmov(path+length, MY_DB_OPT_FILE);
    load_db_opt(thd, path, &db_info);
    create_info->default_table_charset= db_info.default_table_charset;
  }

  if (mysql_prepare_table(thd, create_info, fields,
			  keys, tmp_table, db_options, file,
			  key_info_buffer, &key_count,
			  select_field_count))
    DBUG_RETURN(-1);

      /* Check if table exists */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    my_snprintf(path, sizeof(path), "%s%s%lx_%lx_%x%s",
		mysql_tmpdir, tmp_file_prefix, current_pid, thd->thread_id,
		thd->tmp_table++, reg_ext);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, path);
    create_info->table_options|=HA_CREATE_DELAY_KEY_WRITE;
  }
  else
    my_snprintf(path, sizeof(path), "%s/%s/%s%s", mysql_data_home, db,
		alias, reg_ext);
  unpack_filename(path,path);
  /* Check if table already exists */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE)
      && find_temporary_table(thd,db,table_name))
  {
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      create_info->table_existed= 1;		// Mark that table existed
      DBUG_RETURN(0);
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alias);
    DBUG_RETURN(-1);
  }
  if (wait_if_global_read_lock(thd, 0, 1))
    DBUG_RETURN(error);
  VOID(pthread_mutex_lock(&LOCK_open));
  if (!tmp_table && !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    if (!access(path,F_OK))
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
      {
	create_info->table_existed= 1;		// Mark that table existed
	error= 0;
      }
      else
	my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto end;
    }
  }

  /*
    Check that table with given name does not already
    exist in any storage engine. In such a case it should
    be discovered and the error ER_TABLE_EXISTS_ERROR be returned
    unless user specified CREATE TABLE IF EXISTS
    The LOCK_open mutex has been locked to make sure no
    one else is attempting to discover the table. Since
    it's not on disk as a frm file, no one could be using it!
  */
  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    bool create_if_not_exists =
      create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS;
    if (!create_table_from_handler(db, table_name,
                                 create_if_not_exists))
    {
      DBUG_PRINT("info", ("Table already existed in handler"));

      if (create_if_not_exists)
      {
       create_info->table_existed= 1;   // Mark that table existed
       error= 0;
      }
      else
       my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto end;
    }
  }

  thd->proc_info="creating table";
  create_info->table_existed= 0;		// Mark that table is created

  if (thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE)
    create_info->data_file_name= create_info->index_file_name= 0;
  create_info->table_options=db_options;

  if (rea_create_table(thd, path, create_info, fields, key_count,
		       key_info_buffer))
  {
    /* my_error(ER_CANT_CREATE_TABLE,MYF(0),table_name,my_errno); */
    goto end;
  }
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    /* Open table and put in temporary table list */
    if (!(open_temporary_table(thd, path, db, table_name, 1)))
    {
      (void) rm_temporary_table(create_info->db_type, path);
      goto end;
    }
    thd->tmp_table_used= 1;
  }
  if (!tmp_table)
  {
    // Must be written before unlock
    mysql_update_log.write(thd,thd->query, thd->query_length);
    if (mysql_bin_log.is_open())
    {
      thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    test(create_info->options &
				 HA_LEX_CREATE_TMP_TABLE));
      mysql_bin_log.write(&qinfo);
    }
  }
  error=0;
end:
  VOID(pthread_mutex_unlock(&LOCK_open));
  start_waiting_global_read_lock(thd);
  thd->proc_info="After create";
  DBUG_RETURN(error);
}

/*
** Give the key name after the first field with an optional '_#' after
**/

static bool
check_if_keyname_exists(const char *name, KEY *start, KEY *end)
{
  for (KEY *key=start ; key != end ; key++)
    if (!my_strcasecmp(system_charset_info,name,key->name))
      return 1;
  return 0;
}


static char *
make_unique_key_name(const char *field_name,KEY *start,KEY *end)
{
  char buff[MAX_FIELD_NAME],*buff_end;

  if (!check_if_keyname_exists(field_name,start,end) &&
      my_strcasecmp(system_charset_info,field_name,primary_key_name))
    return (char*) field_name;			// Use fieldname
  buff_end=strmake(buff,field_name, sizeof(buff)-4);

  /*
    Only 3 chars + '\0' left, so need to limit to 2 digit
    This is ok as we can't have more than 100 keys anyway
  */
  for (uint i=2 ; i< 100; i++)
  {
    *buff_end= '_';
    int10_to_str(i, buff_end+1, 10);
    if (!check_if_keyname_exists(buff,start,end))
      return sql_strdup(buff);
  }
  return (char*) "not_specified";		// Should never happen
}


/****************************************************************************
** Create table from a list of fields and items
****************************************************************************/

TABLE *create_table_from_items(THD *thd, HA_CREATE_INFO *create_info,
			       const char *db, const char *name,
			       List<create_field> *extra_fields,
			       List<Key> *keys,
			       List<Item> *items,
			       MYSQL_LOCK **lock)
{
  TABLE tmp_table;		// Used during 'create_field()'
  TABLE *table;
  tmp_table.table_name=0;
  uint select_field_count= items->elements;
  Disable_binlog disable_binlog(thd);
  DBUG_ENTER("create_table_from_items");

  /* Add selected items to field list */
  List_iterator_fast<Item> it(*items);
  Item *item;
  Field *tmp_field;
  tmp_table.db_create_options=0;
  tmp_table.null_row=tmp_table.maybe_null=0;
  tmp_table.blob_ptr_size=portable_sizeof_char_ptr;
  tmp_table.db_low_byte_first= test(create_info->db_type == DB_TYPE_MYISAM ||
				    create_info->db_type == DB_TYPE_HEAP);

  while ((item=it++))
  {
    create_field *cr_field;
    Field *field;
    if (item->type() == Item::FUNC_ITEM)
      field=item->tmp_table_field(&tmp_table);
    else
      field=create_tmp_field(thd, &tmp_table, item, item->type(),
				  (Item ***) 0, &tmp_field,0,0);
    if (!field ||
	!(cr_field=new create_field(field,(item->type() == Item::FIELD_ITEM ?
					   ((Item_field *)item)->field :
					   (Field*) 0))))
      DBUG_RETURN(0);
    extra_fields->push_back(cr_field);
  }
  /* create and lock table */
  /* QQ: This should be done atomic ! */
  /* We don't log the statement, it will be logged later */
  if (mysql_create_table(thd,db,name,create_info,*extra_fields,
			 *keys,0,select_field_count))
    DBUG_RETURN(0);
  /*
    If this is a HEAP table, the automatic DELETE FROM which is written to the
    binlog when a HEAP table is opened for the first time since startup, must
    not be written: 1) it would be wrong (imagine we're in CREATE SELECT: we
    don't want to delete from it) 2) it would be written before the CREATE
    TABLE, which is a wrong order. So we keep binary logging disabled.
  */
  if (!(table=open_table(thd,db,name,name,(bool*) 0)))
  {
    quick_rm_table(create_info->db_type,db,table_case_name(create_info,name));
    DBUG_RETURN(0);
  }
  table->reginfo.lock_type=TL_WRITE;
  if (!((*lock)=mysql_lock_tables(thd,&table,1)))
  {
    VOID(pthread_mutex_lock(&LOCK_open));
    hash_delete(&open_cache,(byte*) table);
    VOID(pthread_mutex_unlock(&LOCK_open));
    quick_rm_table(create_info->db_type,db,table_case_name(create_info, name));
    DBUG_RETURN(0);
  }
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  DBUG_RETURN(table);
  /* Note that leaving the function resets binlogging properties */
}


/****************************************************************************
** Alter a table definition
****************************************************************************/

bool
mysql_rename_table(enum db_type base,
		   const char *old_db,
		   const char *old_name,
		   const char *new_db,
		   const char *new_name)
{
  char from[FN_REFLEN], to[FN_REFLEN];
  char tmp_from[NAME_LEN+1], tmp_to[NAME_LEN+1];
  handler *file=get_new_handler((TABLE*) 0, base);
  int error=0;
  DBUG_ENTER("mysql_rename_table");

  if (lower_case_table_names == 2 && !(file->table_flags() & HA_FILE_BASED))
  {
    /* Table handler expects to get all file names as lower case */
    strmov(tmp_from, old_name);
    my_casedn_str(files_charset_info, tmp_from);
    old_name= tmp_from;

    strmov(tmp_to, new_name);
    my_casedn_str(files_charset_info, tmp_to);
    new_name= tmp_to;
  }
  my_snprintf(from, sizeof(from), "%s/%s/%s",
	      mysql_data_home, old_db, old_name);
  my_snprintf(to, sizeof(to), "%s/%s/%s",
	      mysql_data_home, new_db, new_name);
  fn_format(from,from,"","",4);
  fn_format(to,to,    "","",4);

  if (!(error=file->rename_table((const char*) from,(const char *) to)))
  {
    if (rename_file_ext(from,to,reg_ext))
    {
      error=my_errno;
      /* Restore old file name */
      file->rename_table((const char*) to,(const char *) from);
    }
  }
  delete file;
  if (error)
    my_error(ER_ERROR_ON_RENAME, MYF(0), from, to, error);
  DBUG_RETURN(error != 0);
}


/*
  Force all other threads to stop using the table

  SYNOPSIS
    wait_while_table_is_used()
    thd			Thread handler
    table		Table to remove from cache
    function		HA_EXTRA_PREPARE_FOR_DELETE if table is to be deleted
			HA_EXTRA_FORCE_REOPEN if table is not be used
  NOTES
   When returning, the table will be unusable for other threads until
   the table is closed.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

static void wait_while_table_is_used(THD *thd,TABLE *table,
				     enum ha_extra_function function)
{
  DBUG_PRINT("enter",("table: %s", table->real_name));
  DBUG_ENTER("wait_while_table_is_used");
  safe_mutex_assert_owner(&LOCK_open);

  VOID(table->file->extra(function));
  /* Mark all tables that are in use as 'old' */
  mysql_lock_abort(thd, table);			// end threads waiting on lock

  /* Wait until all there are no other threads that has this table open */
  while (remove_table_from_cache(thd,table->table_cache_key,
				 table->real_name))
  {
    dropping_tables++;
    (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    dropping_tables--;
  }
  DBUG_VOID_RETURN;
}

/*
  Close a cached table

  SYNOPSIS
    close_cached_table()
    thd			Thread handler
    table		Table to remove from cache

  NOTES
    Function ends by signaling threads waiting for the table to try to
    reopen the table.

  PREREQUISITES
    Lock on LOCK_open
    Win32 clients must also have a WRITE LOCK on the table !
*/

static bool close_cached_table(THD *thd, TABLE *table)
{
  DBUG_ENTER("close_cached_table");

  wait_while_table_is_used(thd, table, HA_EXTRA_PREPARE_FOR_DELETE);
  /* Close lock if this is not got with LOCK TABLES */
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;			// Start locked threads
  }
  /* Close all copies of 'table'.  This also frees all LOCK TABLES lock */
  thd->open_tables=unlink_open_table(thd,thd->open_tables,table);

  /* When lock on LOCK_open is freed other threads can continue */
  pthread_cond_broadcast(&COND_refresh);
  DBUG_RETURN(0);
}

static int send_check_errmsg(THD *thd, TABLE_LIST* table,
			     const char* operator_name, const char* errmsg)

{
  Protocol *protocol= thd->protocol;
  protocol->prepare_for_resend();
  protocol->store(table->alias, system_charset_info);
  protocol->store((char*) operator_name, system_charset_info);
  protocol->store("error", 5, system_charset_info);
  protocol->store(errmsg, system_charset_info);
  thd->net.last_error[0]=0;
  if (protocol->write())
    return -1;
  return 1;
}


static int prepare_for_restore(THD* thd, TABLE_LIST* table,
			       HA_CHECK_OPT *check_opt)
{
  DBUG_ENTER("prepare_for_restore");

  if (table->table) // do not overwrite existing tables on restore
  {
    DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				  "table exists, will not overwrite on restore"
				  ));
  }
  else
  {
    char* backup_dir= thd->lex->backup_dir;
    char src_path[FN_REFLEN], dst_path[FN_REFLEN];
    char* table_name = table->real_name;
    char* db = thd->db ? thd->db : table->db;

    if (fn_format_relative_to_data_home(src_path, table_name, backup_dir,
					reg_ext))
      DBUG_RETURN(-1); // protect buffer overflow

    my_snprintf(dst_path, sizeof(dst_path), "%s/%s/%s",
		mysql_real_data_home, db, table_name);

    if (lock_and_wait_for_table_name(thd,table))
      DBUG_RETURN(-1);

    if (my_copy(src_path,
		fn_format(dst_path, dst_path,"", reg_ext, 4),
		MYF(MY_WME)))
    {
      pthread_mutex_lock(&LOCK_open);
      unlock_table_name(thd, table);
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				    "Failed copying .frm file"));
    }
    if (mysql_truncate(thd, table, 1))
    {
      pthread_mutex_lock(&LOCK_open);
      unlock_table_name(thd, table);
      pthread_mutex_unlock(&LOCK_open);
      DBUG_RETURN(send_check_errmsg(thd, table, "restore",
				    "Failed generating table from .frm file"));
    }
  }

  /*
    Now we should be able to open the partially restored table
    to finish the restore in the handler later on
  */
  if (!(table->table = reopen_name_locked_table(thd, table)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table);
    pthread_mutex_unlock(&LOCK_open);
  }
  DBUG_RETURN(0);
}


static int prepare_for_repair(THD* thd, TABLE_LIST *table_list,
			      HA_CHECK_OPT *check_opt)
{
  int error= 0;
  TABLE tmp_table, *table;
  DBUG_ENTER("prepare_for_repair");

  if (!(check_opt->sql_flags & TT_USEFRM))
    DBUG_RETURN(0);

  if (!(table= table_list->table))		/* if open_ltable failed */
  {
    char name[FN_REFLEN];
    strxmov(name, mysql_data_home, "/", table_list->db, "/",
	    table_list->real_name, NullS);
    if (openfrm(name, "", 0, 0, 0, &tmp_table))
      DBUG_RETURN(0);				// Can't open frm file
    table= &tmp_table;
  }

  /*
    User gave us USE_FRM which means that the header in the index file is
    trashed.
    In this case we will try to fix the table the following way:
    - Rename the data file to a temporary name
    - Truncate the table
    - Replace the new data file with the old one
    - Run a normal repair using the new index file and the old data file
  */

  char from[FN_REFLEN],tmp[FN_REFLEN+32];
  const char **ext= table->file->bas_ext();
  MY_STAT stat_info;

  /*
    Check if this is a table type that stores index and data separately,
    like ISAM or MyISAM
  */
  if (!ext[0] || !ext[1])
    goto end;					// No data file

  strxmov(from, table->path, ext[1], NullS);	// Name of data file
  if (!my_stat(from, &stat_info, MYF(0)))
    goto end;				// Can't use USE_FRM flag

  my_snprintf(tmp, sizeof(tmp), "%s-%lx_%lx",
	      from, current_pid, thd->thread_id);

  /* If we could open the table, close it */
  if (table_list->table)
  {
    pthread_mutex_lock(&LOCK_open);
    close_cached_table(thd, table);
    pthread_mutex_unlock(&LOCK_open);
  }
  if (lock_and_wait_for_table_name(thd,table_list))
  {
    error= -1;
    goto end;
  }
  if (my_rename(from, tmp, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed renaming data file");
    goto end;
  }
  if (mysql_truncate(thd, table_list, 1))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed generating table from .frm file");
    goto end;
  }
  if (my_rename(tmp, from, MYF(MY_WME)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
    error= send_check_errmsg(thd, table_list, "repair",
			     "Failed restoring .MYD file");
    goto end;
  }

  /*
    Now we should be able to open the partially repaired table
    to finish the repair in the handler later on.
  */
  if (!(table_list->table = reopen_name_locked_table(thd, table_list)))
  {
    pthread_mutex_lock(&LOCK_open);
    unlock_table_name(thd, table_list);
    pthread_mutex_unlock(&LOCK_open);
  }

end:
  if (table == &tmp_table)
    closefrm(table);				// Free allocated memory
  DBUG_RETURN(error);
}


static int mysql_admin_table(THD* thd, TABLE_LIST* tables,
			     HA_CHECK_OPT* check_opt,
			     const char *operator_name,
			     thr_lock_type lock_type,
			     bool open_for_modify,
			     uint extra_open_options,
			     int (*prepare_func)(THD *, TABLE_LIST *,
						 HA_CHECK_OPT *),
			     int (handler::*operator_func)
			     (THD *, HA_CHECK_OPT *))
{
  TABLE_LIST *table;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysql_admin_table");

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Op", 10));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_type", 10));
  item->maybe_null = 1;
  field_list.push_back(item = new Item_empty_string("Msg_text", 255));
  item->maybe_null = 1;
  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(-1);

  mysql_ha_close(thd, tables, /*dont_send_ok*/ 1, /*dont_lock*/ 1);
  for (table = tables; table; table = table->next)
  {
    char table_name[NAME_LEN*2+2];
    char* db = (table->db) ? table->db : thd->db;
    bool fatal_error=0;
    strxmov(table_name,db ? db : "",".",table->real_name,NullS);

    thd->open_options|= extra_open_options;
    table->table = open_ltable(thd, table, lock_type);
#ifdef EMBEDDED_LIBRARY
    thd->net.last_errno= 0;  // these errors shouldn't get client
#endif
    thd->open_options&= ~extra_open_options;

    if (prepare_func)
    {
      switch ((*prepare_func)(thd, table, check_opt)) {
	case  1: continue; // error, message written to net
	case -1: goto err; // error, message could be written to net
	default:	 ; // should be 0 otherwise
      }
    }

    if (!table->table)
    {
      const char *err_msg;
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store("error",5, system_charset_info);
      if (!(err_msg=thd->net.last_error))
	err_msg=ER(ER_CHECK_NO_SUCH_TABLE);
      protocol->store(err_msg, system_charset_info);
      thd->net.last_error[0]=0;
      if (protocol->write())
	goto err;
      continue;
    }
    table->table->pos_in_table_list= table;
    if ((table->table->db_stat & HA_READ_ONLY) && open_for_modify)
    {
      char buff[FN_REFLEN + MYSQL_ERRMSG_SIZE];
      protocol->prepare_for_resend();
      protocol->store(table_name, system_charset_info);
      protocol->store(operator_name, system_charset_info);
      protocol->store("error", 5, system_charset_info);
      my_snprintf(buff, sizeof(buff), ER(ER_OPEN_AS_READONLY), table_name);
      protocol->store(buff, system_charset_info);
      close_thread_tables(thd);
      table->table=0;				// For query cache
      if (protocol->write())
	goto err;
      continue;
    }

    /* Close all instances of the table to allow repair to rename files */
    if (lock_type == TL_WRITE && table->table->version)
    {
      pthread_mutex_lock(&LOCK_open);
      const char *old_message=thd->enter_cond(&COND_refresh, &LOCK_open,
					      "Waiting to get writelock");
      mysql_lock_abort(thd,table->table);
      while (remove_table_from_cache(thd, table->table->table_cache_key,
				     table->table->real_name) &&
	     ! thd->killed)
      {
	dropping_tables++;
	(void) pthread_cond_wait(&COND_refresh,&LOCK_open);
	dropping_tables--;
      }
      thd->exit_cond(old_message);
      if (thd->killed)
	goto err;
      open_for_modify=0;
    }

    int result_code = (table->table->file->*operator_func)(thd, check_opt);
#ifdef EMBEDDED_LIBRARY
    thd->net.last_errno= 0;  // these errors shouldn't get client
#endif
    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);
    protocol->store(operator_name, system_charset_info);

send_result_message:

    DBUG_PRINT("info", ("result_code: %d", result_code));
    switch (result_code) {
    case HA_ADMIN_NOT_IMPLEMENTED:
      {
	char buf[ERRMSGSIZE+20];
	uint length=my_snprintf(buf, ERRMSGSIZE,
				ER(ER_CHECK_NOT_IMPLEMENTED), operator_name);
	protocol->store("note", 4, system_charset_info);
	protocol->store(buf, length, system_charset_info);
      }
      break;

    case HA_ADMIN_OK:
      protocol->store("status", 6, system_charset_info);
      protocol->store("OK",2, system_charset_info);
      break;

    case HA_ADMIN_FAILED:
      protocol->store("status", 6, system_charset_info);
      protocol->store("Operation failed",16, system_charset_info);
      break;

    case HA_ADMIN_REJECT:
      protocol->store("status", 6, system_charset_info);
      protocol->store("Operation need committed state",30, system_charset_info);
      open_for_modify= FALSE;
      break;

    case HA_ADMIN_ALREADY_DONE:
      protocol->store("status", 6, system_charset_info);
      protocol->store("Table is already up to date", 27, system_charset_info);
      break;

    case HA_ADMIN_CORRUPT:
      protocol->store("error", 5, system_charset_info);
      protocol->store("Corrupt", 7, system_charset_info);
      fatal_error=1;
      break;

    case HA_ADMIN_INVALID:
      protocol->store("error", 5, system_charset_info);
      protocol->store("Invalid argument",16, system_charset_info);
      break;

    case HA_ADMIN_TRY_ALTER:
    {
      /*
        This is currently used only by InnoDB. ha_innobase::optimize() answers
        "try with alter", so here we close the table, do an ALTER TABLE,
        reopen the table and do ha_innobase::analyze() on it.
      */
      close_thread_tables(thd);
      TABLE_LIST *save_next= table->next;
      table->next= 0;
      result_code= mysql_recreate_table(thd, table, 0);
      close_thread_tables(thd);
      if (!result_code) // recreation went ok
      {
        if ((table->table= open_ltable(thd, table, lock_type)) &&
            ((result_code= table->table->file->analyze(thd, check_opt)) > 0))
          result_code= 0; // analyze went ok
      }
      result_code= result_code ? HA_ADMIN_FAILED : HA_ADMIN_OK;
      table->next= save_next;
      goto send_result_message;
    }

    default:				// Probably HA_ADMIN_INTERNAL_ERROR
      protocol->store("error", 5, system_charset_info);
      protocol->store("Unknown - internal error during operation", 41
		      , system_charset_info);
      fatal_error=1;
      break;
    }
    if (fatal_error)
      table->table->version=0;			// Force close of table
    else if (open_for_modify)
    {
      pthread_mutex_lock(&LOCK_open);
      remove_table_from_cache(thd, table->table->table_cache_key,
			      table->table->real_name);
      pthread_mutex_unlock(&LOCK_open);
      /* May be something modified consequently we have to invalidate cache */
      query_cache_invalidate3(thd, table->table, 0);
    }
    close_thread_tables(thd);
    table->table=0;				// For query cache
    if (protocol->write())
      goto err;
  }

  send_eof(thd);
  DBUG_RETURN(0);
 err:
  close_thread_tables(thd);			// Shouldn't be needed
  if (table)
    table->table=0;
  DBUG_RETURN(-1);
}


int mysql_backup_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_backup_table");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"backup", TL_READ, 0, 0, 0,
				&handler::backup));
}


int mysql_restore_table(THD* thd, TABLE_LIST* table_list)
{
  DBUG_ENTER("mysql_restore_table");
  DBUG_RETURN(mysql_admin_table(thd, table_list, 0,
				"restore", TL_WRITE, 1, 0,
				&prepare_for_restore,
				&handler::restore));
}


int mysql_repair_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("mysql_repair_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"repair", TL_WRITE, 1, HA_OPEN_FOR_REPAIR,
				&prepare_for_repair,
				&handler::repair));
}


int mysql_optimize_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
  DBUG_ENTER("mysql_optimize_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"optimize", TL_WRITE, 1,0,0,
				&handler::optimize));
}


/*
  Assigned specified indexes for a table into key cache

  SYNOPSIS
    mysql_assign_to_keycache()
    thd		Thread object
    tables	Table list (one table only)

  RETURN VALUES
    0	  ok
   -1	  error
*/

int mysql_assign_to_keycache(THD* thd, TABLE_LIST* tables,
			     LEX_STRING *key_cache_name)
{
  HA_CHECK_OPT check_opt;
  KEY_CACHE *key_cache;
  DBUG_ENTER("mysql_assign_to_keycache");

  check_opt.init();
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!(key_cache= get_key_cache(key_cache_name)))
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    my_error(ER_UNKNOWN_KEY_CACHE, MYF(0), key_cache_name->str);
    DBUG_RETURN(-1);
  }
  pthread_mutex_unlock(&LOCK_global_system_variables);
  check_opt.key_cache= key_cache;
  DBUG_RETURN(mysql_admin_table(thd, tables, &check_opt,
				"assign_to_keycache", TL_READ_NO_INSERT, 0,
				0, 0, &handler::assign_to_keycache));
}


/*
  Reassign all tables assigned to a key cache to another key cache

  SYNOPSIS
    reassign_keycache_tables()
    thd		Thread object
    src_cache	Reference to the key cache to clean up
    dest_cache	New key cache

  NOTES
    This is called when one sets a key cache size to zero, in which
    case we have to move the tables associated to this key cache to
    the "default" one.

    One has to ensure that one never calls this function while
    some other thread is changing the key cache. This is assured by
    the caller setting src_cache->in_init before calling this function.

    We don't delete the old key cache as there may still be pointers pointing
    to it for a while after this function returns.

 RETURN VALUES
    0	  ok
*/

int reassign_keycache_tables(THD *thd, KEY_CACHE *src_cache,
			     KEY_CACHE *dst_cache)
{
  DBUG_ENTER("reassign_keycache_tables");

  DBUG_ASSERT(src_cache != dst_cache);
  DBUG_ASSERT(src_cache->in_init);
  src_cache->param_buff_size= 0;		// Free key cache
  ha_resize_key_cache(src_cache);
  ha_change_key_cache(src_cache, dst_cache);
  DBUG_RETURN(0);
}


/*
  Preload specified indexes for a table into key cache

  SYNOPSIS
    mysql_preload_keys()
    thd		Thread object
    tables	Table list (one table only)

  RETURN VALUES
    0	  ok
   -1	  error
*/

int mysql_preload_keys(THD* thd, TABLE_LIST* tables)
{
  DBUG_ENTER("mysql_preload_keys");
  DBUG_RETURN(mysql_admin_table(thd, tables, 0,
				"preload_keys", TL_READ, 0, 0, 0,
				&handler::preload_keys));
}


/*
  Create a table identical to the specified table

  SYNOPSIS
    mysql_create_like_table()
    thd		Thread object
    table	Table list (one table only)
    create_info Create info
    table_ident Src table_ident

  RETURN VALUES
    0	  ok
    -1	error
*/

int mysql_create_like_table(THD* thd, TABLE_LIST* table,
			    HA_CREATE_INFO *create_info,
			    Table_ident *table_ident)
{
  TABLE **tmp_table;
  char src_path[FN_REFLEN], dst_path[FN_REFLEN];
  char *db= table->db;
  char *table_name= table->real_name;
  char *src_db= thd->db;
  char *src_table= table_ident->table.str;
  int  err, res= -1;
  TABLE_LIST src_tables_list;
  DBUG_ENTER("mysql_create_like_table");

  /*
    Validate the source table
  */
  if (table_ident->table.length > NAME_LEN ||
      (table_ident->table.length &&
       check_table_name(src_table,table_ident->table.length)) ||
      table_ident->db.str && check_db_name((src_db= table_ident->db.str)))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), src_table);
    DBUG_RETURN(-1);
  }

  src_tables_list.db= table_ident->db.str ? table_ident->db.str : thd->db;
  src_tables_list.real_name= table_ident->table.str;
  src_tables_list.next= 0;

  if (lock_and_wait_for_table_name(thd, &src_tables_list))
    goto err;

  if ((tmp_table= find_temporary_table(thd, src_db, src_table)))
    strxmov(src_path, (*tmp_table)->path, reg_ext, NullS);
  else
  {
    strxmov(src_path, mysql_data_home, "/", src_db, "/", src_table,
	    reg_ext, NullS);
    if (access(src_path, F_OK))
    {
      my_error(ER_BAD_TABLE_ERROR, MYF(0), src_table);
      goto err;
    }
  }

  /*
    Validate the destination table

    skip the destination table name checking as this is already
    validated.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (find_temporary_table(thd, db, table_name))
      goto table_exists;
    my_snprintf(dst_path, sizeof(dst_path), "%s%s%lx_%lx_%x%s",
		mysql_tmpdir, tmp_file_prefix, current_pid,
		thd->thread_id, thd->tmp_table++, reg_ext);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, dst_path);
    create_info->table_options|= HA_CREATE_DELAY_KEY_WRITE;
  }
  else
  {
    strxmov(dst_path, mysql_data_home, "/", db, "/", table_name,
	    reg_ext, NullS);
    if (!access(dst_path, F_OK))
      goto table_exists;
  }

  /*
    Create a new table by copying from source table
  */
  if (my_copy(src_path, dst_path, MYF(MY_WME|MY_DONT_OVERWRITE_FILE)))
    goto err;

  /*
    As mysql_truncate don't work on a new table at this stage of
    creation, instead create the table directly (for both normal
    and temporary tables).
  */
  *fn_ext(dst_path)= 0;
  err= ha_create_table(dst_path, create_info, 1);

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    if (err || !open_temporary_table(thd, dst_path, db, table_name, 1))
    {
      (void) rm_temporary_table(create_info->db_type,
				dst_path); /* purecov: inspected */
      goto err;     /* purecov: inspected */
    }
  }
  else if (err)
  {
    (void) quick_rm_table(create_info->db_type, db,
			  table_name); /* purecov: inspected */
    goto err;	    /* purecov: inspected */
  }

  // Must be written before unlock
  mysql_update_log.write(thd,thd->query, thd->query_length);
  if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    Query_log_event qinfo(thd, thd->query, thd->query_length,
			  test(create_info->options &
			       HA_LEX_CREATE_TMP_TABLE));
    mysql_bin_log.write(&qinfo);
  }
  res= 0;
  goto err;

table_exists:
  if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
  {
    char warn_buff[MYSQL_ERRMSG_SIZE];
    my_snprintf(warn_buff, sizeof(warn_buff),
		ER(ER_TABLE_EXISTS_ERROR), table_name);
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		 ER_TABLE_EXISTS_ERROR,warn_buff);
    res= 0;
  }
  else
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);

err:
  pthread_mutex_lock(&LOCK_open);
  unlock_table_name(thd, &src_tables_list);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(res);
}


int mysql_analyze_table(THD* thd, TABLE_LIST* tables, HA_CHECK_OPT* check_opt)
{
#ifdef OS2
  thr_lock_type lock_type = TL_WRITE;
#else
  thr_lock_type lock_type = TL_READ_NO_INSERT;
#endif

  DBUG_ENTER("mysql_analyze_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"analyze", lock_type, 1,0,0,
				&handler::analyze));
}


int mysql_check_table(THD* thd, TABLE_LIST* tables,HA_CHECK_OPT* check_opt)
{
#ifdef OS2
  thr_lock_type lock_type = TL_WRITE;
#else
  thr_lock_type lock_type = TL_READ_NO_INSERT;
#endif

  DBUG_ENTER("mysql_check_table");
  DBUG_RETURN(mysql_admin_table(thd, tables, check_opt,
				"check", lock_type,
				0, HA_OPEN_FOR_REPAIR, 0,
				&handler::check));
}

/* table_list should contain just one table */
int mysql_discard_or_import_tablespace(THD *thd,
		      TABLE_LIST *table_list,
		      enum tablespace_op_type tablespace_op)
{
  TABLE *table;
  my_bool discard;
  int error;
  DBUG_ENTER("mysql_discard_or_import_tablespace");

  /* Note that DISCARD/IMPORT TABLESPACE always is the only operation in an
  ALTER TABLE */

  thd->proc_info="discard_or_import_tablespace";

  if (tablespace_op == DISCARD_TABLESPACE)
    discard = TRUE;
  else
    discard = FALSE;

  thd->tablespace_op=TRUE; /* we set this flag so that ha_innobase::open
			   and ::external_lock() do not complain when we
			   lock the table */
  mysql_ha_close(thd, table_list, /*dont_send_ok*/ 1, /*dont_lock*/ 1);

  if (!(table=open_ltable(thd,table_list,TL_WRITE)))
  {
    thd->tablespace_op=FALSE;
    DBUG_RETURN(-1);
  }

  error=table->file->discard_or_import_tablespace(discard);

  thd->proc_info="end";

  if (error)
    goto err;

  /* The 0 in the call below means 'not in a transaction', which means
  immediate invalidation; that is probably what we wish here */
  query_cache_invalidate3(thd, table_list, 0);

  /* The ALTER TABLE is always in its own transaction */
  error = ha_commit_stmt(thd);
  if (ha_commit(thd))
    error=1;
  if (error)
    goto err;
  mysql_update_log.write(thd, thd->query,thd->query_length);
  if (mysql_bin_log.is_open())
  {
    Query_log_event qinfo(thd, thd->query, thd->query_length, 0);
    mysql_bin_log.write(&qinfo);
  }
err:
  close_thread_tables(thd);
  thd->tablespace_op=FALSE;
  if (error == 0)
  {
    send_ok(thd);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(error);
}


#ifdef NOT_USED
/*
  CREATE INDEX and DROP INDEX are implemented by calling ALTER TABLE with
  the proper arguments.  This isn't very fast but it should work for most
  cases.
  One should normally create all indexes with CREATE TABLE or ALTER TABLE.
*/

int mysql_create_indexes(THD *thd, TABLE_LIST *table_list, List<Key> &keys)
{
  List<create_field> fields;
  List<Alter_drop>   drop;
  List<Alter_column> alter;
  HA_CREATE_INFO     create_info;
  int		     rc;
  uint		     idx;
  uint		     db_options;
  uint		     key_count;
  TABLE		     *table;
  Field		     **f_ptr;
  KEY		     *key_info_buffer;
  char		     path[FN_REFLEN+1];
  DBUG_ENTER("mysql_create_index");

  /*
    Try to use online generation of index.
    This requires that all indexes can be created online.
    Otherwise, the old alter table procedure is executed.

    Open the table to have access to the correct table handler.
  */
  if (!(table=open_ltable(thd,table_list,TL_WRITE_ALLOW_READ)))
    DBUG_RETURN(-1);

  /*
    The add_index method takes an array of KEY structs for the new indexes.
    Preparing a new table structure generates this array.
    It needs a list with all fields of the table, which does not need to
    be correct in every respect. The field names are important.
  */
  for (f_ptr= table->field; *f_ptr; f_ptr++)
  {
    create_field *c_fld= new create_field(*f_ptr, *f_ptr);
    c_fld->unireg_check= Field::NONE; /*avoid multiple auto_increments*/
    fields.push_back(c_fld);
  }
  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  create_info.default_table_charset= thd->variables.collation_database;
  db_options= 0;
  if (mysql_prepare_table(thd, &create_info, fields,
			  keys, /*tmp_table*/ 0, db_options, table->file,
			  key_info_buffer, key_count,
			  /*select_field_count*/ 0))
    DBUG_RETURN(-1);

  /*
    Check if all keys can be generated with the add_index method.
    If anyone cannot, then take the old way.
  */
  for (idx=0; idx< key_count; idx++)
  {
    DBUG_PRINT("info", ("creating index %s", key_info_buffer[idx].name));
    if (!(table->file->index_ddl_flags(key_info_buffer+idx)&
	  (HA_DDL_ONLINE| HA_DDL_WITH_LOCK)))
      break ;
  }
  if ((idx < key_count)|| !key_count)
  {
    /* Re-initialize the create_info, which was changed by prepare table. */
    bzero((char*) &create_info,sizeof(create_info));
    create_info.db_type=DB_TYPE_DEFAULT;
    create_info.default_table_charset= thd->variables.collation_database;
    /* Cleanup the fields list. We do not want to create existing fields. */
    fields.delete_elements();
    if (real_alter_table(thd, table_list->db, table_list->real_name,
			 &create_info, table_list, table,
			 fields, keys, drop, alter, 0, (ORDER*)0,
			 ALTER_ADD_INDEX, DUP_ERROR))
      /* Don't need to free((gptr) key_info_buffer);*/
      DBUG_RETURN(-1);
  }
  else
  {
    if (table->file->add_index(table, key_info_buffer, key_count)||
	(my_snprintf(path, sizeof(path), "%s/%s/%s%s", mysql_data_home,
		     table_list->db, (lower_case_table_names == 2) ?
		     table_list->alias: table_list->real_name, reg_ext) >=
	 (int) sizeof(path)) ||
	! unpack_filename(path, path) ||
	mysql_create_frm(thd, path, &create_info,
			 fields, key_count, key_info_buffer, table->file))
      /* don't need to free((gptr) key_info_buffer);*/
      DBUG_RETURN(-1);
  }
  /* don't need to free((gptr) key_info_buffer);*/
  DBUG_RETURN(0);
}


int mysql_drop_indexes(THD *thd, TABLE_LIST *table_list,
		       List<Alter_drop> &drop)
{
  List<create_field> fields;
  List<Key>	     keys;
  List<Alter_column> alter;
  HA_CREATE_INFO     create_info;
  uint		     idx;
  uint		     db_options;
  uint		     key_count;
  uint		     *key_numbers;
  TABLE		     *table;
  Field		     **f_ptr;
  KEY		     *key_info;
  KEY		     *key_info_buffer;
  char		     path[FN_REFLEN];
  DBUG_ENTER("mysql_drop_index");

  /*
    Try to use online generation of index.
    This requires that all indexes can be created online.
    Otherwise, the old alter table procedure is executed.

    Open the table to have access to the correct table handler.
  */
  if (!(table=open_ltable(thd,table_list,TL_WRITE_ALLOW_READ)))
    DBUG_RETURN(-1);

  /*
    The drop_index method takes an array of key numbers.
    It cannot get more entries than keys in the table.
  */
  key_numbers= (uint*) thd->alloc(sizeof(uint*)*table->keys);
  key_count= 0;

  /*
    Get the number of each key and check if it can be created online.
  */
  List_iterator<Alter_drop> drop_it(drop);
  Alter_drop *drop_key;
  while ((drop_key= drop_it++))
  {
    /* Find the key in the table. */
    key_info=table->key_info;
    for (idx=0; idx< table->keys; idx++, key_info++)
    {
      if (!my_strcasecmp(system_charset_info, key_info->name, drop_key->name))
	break;
    }
    if (idx>= table->keys)
    {
      my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0), drop_key->name);
      /*don't need to free((gptr) key_numbers);*/
      DBUG_RETURN(-1);
    }
    /*
      Check if the key can be generated with the add_index method.
      If anyone cannot, then take the old way.
    */
    DBUG_PRINT("info", ("dropping index %s", table->key_info[idx].name));
    if (!(table->file->index_ddl_flags(table->key_info+idx)&
	  (HA_DDL_ONLINE| HA_DDL_WITH_LOCK)))
      break ;
    key_numbers[key_count++]= idx;
  }

  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  create_info.default_table_charset= thd->variables.collation_database;

  if ((drop_key)|| (drop.elements<= 0))
  {
    if (real_alter_table(thd, table_list->db, table_list->real_name,
			 &create_info, table_list, table,
			 fields, keys, drop, alter, 0, (ORDER*)0,
			 ALTER_DROP_INDEX, DUP_ERROR))
      /*don't need to free((gptr) key_numbers);*/
      DBUG_RETURN(-1);
  }
  else
  {
    db_options= 0;
    if (table->file->drop_index(table, key_numbers, key_count)||
	mysql_prepare_table(thd, &create_info, fields,
			    keys, /*tmp_table*/ 0, db_options, table->file,
			    key_info_buffer, key_count,
			    /*select_field_count*/ 0)||
	(snprintf(path, sizeof(path), "%s/%s/%s%s", mysql_data_home,
		  table_list->db, (lower_case_table_names == 2)?
		  table_list->alias: table_list->real_name, reg_ext)>=
	 (int)sizeof(path))||
	! unpack_filename(path, path)||
	mysql_create_frm(thd, path, &create_info,
			 fields, key_count, key_info_buffer, table->file))
      /*don't need to free((gptr) key_numbers);*/
      DBUG_RETURN(-1);
  }

  /*don't need to free((gptr) key_numbers);*/
  DBUG_RETURN(0);
}
#endif /* NOT_USED */


/*
  Alter table
*/

int mysql_alter_table(THD *thd,char *new_db, char *new_name,
		      HA_CREATE_INFO *create_info,
		      TABLE_LIST *table_list,
		      List<create_field> &fields, List<Key> &keys,
		      uint order_num, ORDER *order,
		      enum enum_duplicates handle_duplicates,
		      ALTER_INFO *alter_info, bool do_send_ok)
{
  TABLE *table,*new_table;
  int error;
  char tmp_name[80],old_name[32],new_name_buff[FN_REFLEN];
  char new_alias_buff[FN_REFLEN], *table_name, *db, *new_alias, *alias;
  char index_file[FN_REFLEN], data_file[FN_REFLEN];
  ha_rows copied,deleted;
  ulonglong next_insert_id;
  uint db_create_options, used_fields;
  enum db_type old_db_type,new_db_type;
  DBUG_ENTER("mysql_alter_table");

  thd->proc_info="init";
  table_name=table_list->real_name;
  alias= (lower_case_table_names == 2) ? table_list->alias : table_name;

  db=table_list->db;
  if (!new_db || !my_strcasecmp(table_alias_charset, new_db, db))
    new_db= db;
  used_fields=create_info->used_fields;

  mysql_ha_close(thd, table_list, /*dont_send_ok*/ 1, /*dont_lock*/ 1);

  /* DISCARD/IMPORT TABLESPACE is always alone in an ALTER TABLE */
  if (alter_info->tablespace_op != NO_TABLESPACE_OP)
    DBUG_RETURN(mysql_discard_or_import_tablespace(thd,table_list,
						   alter_info->tablespace_op));
  if (!(table=open_ltable(thd,table_list,TL_WRITE_ALLOW_READ)))
    DBUG_RETURN(-1);

  /* Check that we are not trying to rename to an existing table */
  if (new_name)
  {
    strmov(new_name_buff,new_name);
    strmov(new_alias= new_alias_buff, new_name);
    if (lower_case_table_names)
    {
      if (lower_case_table_names != 2)
      {
	my_casedn_str(files_charset_info, new_name_buff);
	new_alias= new_name;			// Create lower case table name
      }
      my_casedn_str(files_charset_info, new_name);
    }
    if (new_db == db &&
	!my_strcasecmp(table_alias_charset, new_name_buff, table_name))
    {
      /*
	Source and destination table names are equal: make later check
	easier.
      */
      new_alias= new_name= table_name;
    }
    else
    {
      if (table->tmp_table)
      {
	if (find_temporary_table(thd,new_db,new_name_buff))
	{
	  my_error(ER_TABLE_EXISTS_ERROR,MYF(0),new_name_buff);
	  DBUG_RETURN(-1);
	}
      }
      else
      {
	char dir_buff[FN_REFLEN];
	strxnmov(dir_buff, FN_REFLEN, mysql_real_data_home, new_db, NullS);
	if (!access(fn_format(new_name_buff,new_name_buff,dir_buff,reg_ext,0),
		    F_OK))
	{
	  /* Table will be closed in do_command() */
	  my_error(ER_TABLE_EXISTS_ERROR,MYF(0), new_alias);
	  DBUG_RETURN(-1);
	}
      }
    }
  }
  else
  {
    new_alias= (lower_case_table_names == 2) ? alias : table_name;
    new_name= table_name;
  }

  old_db_type=table->db_type;
  if (create_info->db_type == DB_TYPE_DEFAULT)
    create_info->db_type=old_db_type;
  if ((new_db_type= ha_checktype(create_info->db_type)) !=
      create_info->db_type)
  {
    create_info->db_type= new_db_type;
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_USING_OTHER_HANDLER,
			ER(ER_WARN_USING_OTHER_HANDLER),
			ha_get_storage_engine(new_db_type),
			new_name);
  }
  if (create_info->row_type == ROW_TYPE_NOT_USED)
    create_info->row_type=table->row_type;

  thd->proc_info="setup";
  if (alter_info->is_simple && !table->tmp_table)
  {
    error=0;
    if (new_name != table_name || new_db != db)
    {
      thd->proc_info="rename";
      VOID(pthread_mutex_lock(&LOCK_open));
      /* Then do a 'simple' rename of the table */
      error=0;
      if (!access(new_name_buff,F_OK))
      {
	my_error(ER_TABLE_EXISTS_ERROR,MYF(0),new_name);
	error= -1;
      }
      else
      {
	*fn_ext(new_name)=0;
	close_cached_table(thd, table);
	if (mysql_rename_table(old_db_type,db,table_name,new_db,new_alias))
	  error= -1;
      }
      VOID(pthread_mutex_unlock(&LOCK_open));
    }

    if (!error)
    {
      switch (alter_info->keys_onoff) {
      case LEAVE_AS_IS:
	break;
      case ENABLE:
	VOID(pthread_mutex_lock(&LOCK_open));
	wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
	VOID(pthread_mutex_unlock(&LOCK_open));
	error= table->file->enable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
	/* COND_refresh will be signaled in close_thread_tables() */
	break;
      case DISABLE:
	VOID(pthread_mutex_lock(&LOCK_open));
	wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN);
	VOID(pthread_mutex_unlock(&LOCK_open));
	error=table->file->disable_indexes(HA_KEY_SWITCH_NONUNIQ_SAVE);
	/* COND_refresh will be signaled in close_thread_tables() */
	break;
      }
    }
    if (error == HA_ERR_WRONG_COMMAND)
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
			  table->table_name);
      error=0;
    }
    if (!error)
    {
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	thd->clear_error();
	Query_log_event qinfo(thd, thd->query, thd->query_length, 0);
	mysql_bin_log.write(&qinfo);
      }
      if (do_send_ok)
        send_ok(thd);
    }
    else if (error > 0)
    {
      table->file->print_error(error, MYF(0));
      error= -1;
    }
    table_list->table=0;				// For query cache
    query_cache_invalidate3(thd, table_list, 0);
    DBUG_RETURN(error);
  }

  /* Full alter table */

  /* let new create options override the old ones */
  if (!(used_fields & HA_CREATE_USED_MIN_ROWS))
    create_info->min_rows=table->min_rows;
  if (!(used_fields & HA_CREATE_USED_MAX_ROWS))
    create_info->max_rows=table->max_rows;
  if (!(used_fields & HA_CREATE_USED_AVG_ROW_LENGTH))
    create_info->avg_row_length=table->avg_row_length;
  if (!(used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
    create_info->default_table_charset= table->table_charset;

  restore_record(table,default_values);		// Empty record for DEFAULT
  List_iterator<Alter_drop> drop_it(alter_info->drop_list);
  List_iterator<create_field> def_it(fields);
  List_iterator<Alter_column> alter_it(alter_info->alter_list);
  List<create_field> create_list;		// Add new fields here
  List<Key> key_list;				// Add new keys here
  create_field *def;

  /*
    First collect all fields from table which isn't in drop_list
  */

  Field **f_ptr,*field;
  for (f_ptr=table->field ; (field= *f_ptr) ; f_ptr++)
  {
    /* Check if field should be droped */
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::COLUMN &&
	  !my_strcasecmp(system_charset_info,field->field_name, drop->name))
      {
	/* Reset auto_increment value if it was dropped */
	if (MTYP_TYPENR(field->unireg_check) == Field::NEXT_NUMBER &&
	    !(used_fields & HA_CREATE_USED_AUTO))
	{
	  create_info->auto_increment_value=0;
	  create_info->used_fields|=HA_CREATE_USED_AUTO;
	}
	break;
      }
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }
    /* Check if field is changed */
    def_it.rewind();
    while ((def=def_it++))
    {
      if (def->change &&
	  !my_strcasecmp(system_charset_info,field->field_name, def->change))
	break;
    }
    if (def)
    {						// Field is changed
      def->field=field;
      if (!def->after)
      {
	create_list.push_back(def);
	def_it.remove();
      }
    }
    else
    {						// Use old field value
      create_list.push_back(def=new create_field(field,field));
      alter_it.rewind();			// Change default if ALTER
      Alter_column *alter;
      while ((alter=alter_it++))
      {
	if (!my_strcasecmp(system_charset_info,field->field_name, alter->name))
	  break;
      }
      if (alter)
      {
	if (def->sql_type == FIELD_TYPE_BLOB)
	{
	  my_error(ER_BLOB_CANT_HAVE_DEFAULT,MYF(0),def->change);
	  DBUG_RETURN(-1);
	}
	def->def=alter->def;			// Use new default
	alter_it.remove();
      }
    }
  }
  def_it.rewind();
  List_iterator<create_field> find_it(create_list);
  while ((def=def_it++))			// Add new columns
  {
    if (def->change && ! def->field)
    {
      my_error(ER_BAD_FIELD_ERROR,MYF(0),def->change,table_name);
      DBUG_RETURN(-1);
    }
    if (!def->after)
      create_list.push_back(def);
    else if (def->after == first_keyword)
      create_list.push_front(def);
    else
    {
      create_field *find;
      find_it.rewind();
      while ((find=find_it++))			// Add new columns
      {
	if (!my_strcasecmp(system_charset_info,def->after, find->field_name))
	  break;
      }
      if (!find)
      {
	my_error(ER_BAD_FIELD_ERROR,MYF(0),def->after,table_name);
	DBUG_RETURN(-1);
      }
      find_it.after(def);			// Put element after this
    }
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_BAD_FIELD_ERROR,MYF(0),alter_info->alter_list.head()->name,
	     table_name);
    DBUG_RETURN(-1);
  }
  if (!create_list.elements)
  {
    my_error(ER_CANT_REMOVE_ALL_FIELDS,MYF(0));
    DBUG_RETURN(-1);
  }

  /*
    Collect all keys which isn't in drop list. Add only those
    for which some fields exists.
  */

  List_iterator<Key> key_it(keys);
  List_iterator<create_field> field_it(create_list);
  List<key_part_spec> key_parts;

  KEY *key_info=table->key_info;
  for (uint i=0 ; i < table->keys ; i++,key_info++)
  {
    char *key_name= key_info->name;
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::KEY &&
	  !my_strcasecmp(system_charset_info,key_name, drop->name))
	break;
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }

    KEY_PART_INFO *key_part= key_info->key_part;
    key_parts.empty();
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (!key_part->field)
	continue;				// Wrong field (from UNIREG)
      const char *key_part_name=key_part->field->field_name;
      create_field *cfield;
      field_it.rewind();
      while ((cfield=field_it++))
      {
	if (cfield->change)
	{
	  if (!my_strcasecmp(system_charset_info, key_part_name,
			     cfield->change))
	    break;
	}
	else if (!my_strcasecmp(system_charset_info,
				key_part_name, cfield->field_name))
	  break;
      }
      if (!cfield)
	continue;				// Field is removed
      uint key_part_length=key_part->length;
      if (cfield->field)			// Not new field
      {						// Check if sub key
	if (cfield->field->type() != FIELD_TYPE_BLOB &&
	    (cfield->field->pack_length() == key_part_length ||
	     cfield->length <= key_part_length /
			       key_part->field->charset()->mbmaxlen))
	  key_part_length=0;			// Use whole field
      }
      key_part_length /= key_part->field->charset()->mbmaxlen;
      key_parts.push_back(new key_part_spec(cfield->field_name,
					    key_part_length));
    }
    if (key_parts.elements)
      key_list.push_back(new Key(key_info->flags & HA_SPATIAL ? Key::SPATIAL :
				 (key_info->flags & HA_NOSAME ?
				 (!my_strcasecmp(system_charset_info,
						 key_name, primary_key_name) ?
				  Key::PRIMARY	: Key::UNIQUE) :
				  (key_info->flags & HA_FULLTEXT ?
				   Key::FULLTEXT : Key::MULTIPLE)),
				 key_name,
				 key_info->algorithm,
                                 test(key_info->flags & HA_GENERATED_KEY),
				 key_parts));
  }
  {
    Key *key;
    while ((key=key_it++))			// Add new keys
    {
      if (key->type != Key::FOREIGN_KEY)
	key_list.push_back(key);
      if (key->name &&
	  !my_strcasecmp(system_charset_info,key->name,primary_key_name))
      {
	my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->name);
	DBUG_RETURN(-1);
      }
    }
  }

  if (alter_info->drop_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,MYF(0),
	     alter_info->drop_list.head()->name);
    goto err;
  }
  if (alter_info->alter_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,MYF(0),
	     alter_info->alter_list.head()->name);
    goto err;
  }

  db_create_options=table->db_create_options & ~(HA_OPTION_PACK_RECORD);
  my_snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  /* Safety fix for innodb */
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tmp_name);
  create_info->db_type=new_db_type;
  if (!create_info->comment)
    create_info->comment=table->comment;

  table->file->update_create_info(create_info);
  if ((create_info->table_options &
       (HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS)) ||
      (used_fields & HA_CREATE_USED_PACK_KEYS))
    db_create_options&= ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS);
  if (create_info->table_options &
      (HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM))
    db_create_options&= ~(HA_OPTION_CHECKSUM | HA_OPTION_NO_CHECKSUM);
  if (create_info->table_options &
      (HA_OPTION_DELAY_KEY_WRITE | HA_OPTION_NO_DELAY_KEY_WRITE))
    db_create_options&= ~(HA_OPTION_DELAY_KEY_WRITE |
			  HA_OPTION_NO_DELAY_KEY_WRITE);
  create_info->table_options|= db_create_options;

  if (table->tmp_table)
    create_info->options|=HA_LEX_CREATE_TMP_TABLE;

  /*
    Handling of symlinked tables:
    If no rename:
      Create new data file and index file on the same disk as the
      old data and index files.
      Copy data.
      Rename new data file over old data file and new index file over
      old index file.
      Symlinks are not changed.

   If rename:
      Create new data file and index file on the same disk as the
      old data and index files.  Create also symlinks to point at
      the new tables.
      Copy data.
      At end, rename temporary tables and symlinks to temporary table
      to final table name.
      Remove old table and old symlinks

    If rename is made to another database:
      Create new tables in new database.
      Copy data.
      Remove old table and symlinks.
  */

  if (!strcmp(db, new_db))		// Ignore symlink if db changed
  {
    if (create_info->index_file_name)
    {
      /* Fix index_file_name to have 'tmp_name' as basename */
      strmov(index_file, tmp_name);
      create_info->index_file_name=fn_same(index_file,
					   create_info->index_file_name,
					   1);
    }
    if (create_info->data_file_name)
    {
      /* Fix data_file_name to have 'tmp_name' as basename */
      strmov(data_file, tmp_name);
      create_info->data_file_name=fn_same(data_file,
					  create_info->data_file_name,
					  1);
    }
  }
  else
    create_info->data_file_name=create_info->index_file_name=0;
  {
    /* We don't log the statement, it will be logged later */
    Disable_binlog disable_binlog(thd);
    if ((error=mysql_create_table(thd, new_db, tmp_name,
                                  create_info,
                                  create_list,key_list,1,0)))
      DBUG_RETURN(error);
  }
  if (table->tmp_table)
    new_table=open_table(thd,new_db,tmp_name,tmp_name,0);
  else
  {
    char path[FN_REFLEN];
    my_snprintf(path, sizeof(path), "%s/%s/%s", mysql_data_home,
		new_db, tmp_name);
    fn_format(path,path,"","",4);
    new_table=open_temporary_table(thd, path, new_db, tmp_name,0);
  }
  if (!new_table)
  {
    VOID(quick_rm_table(new_db_type,new_db,tmp_name));
    goto err;
  }


  /*
    We don't want update TIMESTAMP fields during ALTER TABLE
    and copy_data_between_tables uses only write_row() for new_table so
    don't need to set up timestamp_on_update_now member.
  */
  new_table->timestamp_default_now= 0;
  new_table->next_number_field=new_table->found_next_number_field;
  thd->count_cuted_fields= CHECK_FIELD_WARN;	// calc cuted fields
  thd->cuted_fields=0L;
  thd->proc_info="copy to tmp table";
  next_insert_id=thd->next_insert_id;		// Remember for loggin
  copied=deleted=0;
  if (!new_table->is_view)
    error=copy_data_between_tables(table,new_table,create_list,
				   handle_duplicates,
				   order_num, order, &copied, &deleted);
  thd->last_insert_id=next_insert_id;		// Needed for correct log
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;

  if (table->tmp_table)
  {
    /* We changed a temporary table */
    if (error)
    {
      /*
	The following function call will free the new_table pointer,
	in close_temporary_table(), so we can safely directly jump to err
      */
      close_temporary_table(thd,new_db,tmp_name);
      goto err;
    }
    /* Close lock if this is a transactional table */
    if (thd->lock)
    {
      mysql_unlock_tables(thd, thd->lock);
      thd->lock=0;
    }
    /* Remove link to old table and rename the new one */
    close_temporary_table(thd,table->table_cache_key,table_name);
    if (rename_temporary_table(thd, new_table, new_db, new_alias))
    {						// Fatal error
      close_temporary_table(thd,new_db,tmp_name);
      my_free((gptr) new_table,MYF(0));
      goto err;
    }
    mysql_update_log.write(thd, thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length, 0);
      mysql_bin_log.write(&qinfo);
    }
    goto end_temporary;
  }

  intern_close_table(new_table);		/* close temporary table */
  my_free((gptr) new_table,MYF(0));
  VOID(pthread_mutex_lock(&LOCK_open));
  if (error)
  {
    VOID(quick_rm_table(new_db_type,new_db,tmp_name));
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }

  /*
    Data is copied.  Now we rename the old table to a temp name,
    rename the new one to the old name, remove all entries from the old table
    from the cash, free all locks, close the old table and remove it.
  */

  thd->proc_info="rename result table";
  my_snprintf(old_name, sizeof(old_name), "%s2-%lx-%lx", tmp_file_prefix,
	      current_pid, thd->thread_id);
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, old_name);
  if (new_name != table_name || new_db != db)
  {
    if (!access(new_name_buff,F_OK))
    {
      error=1;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),new_name_buff);
      VOID(quick_rm_table(new_db_type,new_db,tmp_name));
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }

#if (!defined( __WIN__) && !defined( __EMX__) && !defined( OS2))
  if (table->file->has_transactions())
#endif
  {
    /*
      Win32 and InnoDB can't drop a table that is in use, so we must
      close the original table at before doing the rename
    */
    table_name=thd->strdup(table_name);		// must be saved
    if (close_cached_table(thd, table))
    {						// Aborted
      VOID(quick_rm_table(new_db_type,new_db,tmp_name));
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
    table=0;					// Marker that table is closed
  }
#if (!defined( __WIN__) && !defined( __EMX__) && !defined( OS2))
  else
    table->file->extra(HA_EXTRA_FORCE_REOPEN);	// Don't use this file anymore
#endif


  error=0;
  if (mysql_rename_table(old_db_type,db,table_name,db,old_name))
  {
    error=1;
    VOID(quick_rm_table(new_db_type,new_db,tmp_name));
  }
  else if (mysql_rename_table(new_db_type,new_db,tmp_name,new_db,
			      new_alias))
  {						// Try to get everything back
    error=1;
    VOID(quick_rm_table(new_db_type,new_db,new_alias));
    VOID(quick_rm_table(new_db_type,new_db,tmp_name));
    VOID(mysql_rename_table(old_db_type,db,old_name,db,alias));
  }
  if (error)
  {
    /*
      This shouldn't happen.  We solve this the safe way by
      closing the locked table.
    */
    if (table)
      close_cached_table(thd,table);
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }
  if (thd->lock || new_name != table_name)	// True if WIN32
  {
    /*
      Not table locking or alter table with rename
      free locks and remove old table
    */
    if (table)
      close_cached_table(thd,table);
    VOID(quick_rm_table(old_db_type,db,old_name));
  }
  else
  {
    /*
      Using LOCK TABLES without rename.
      This code is never executed on WIN32!
      Remove old renamed table, reopen table and get new locks
    */
    if (table)
    {
      VOID(table->file->extra(HA_EXTRA_FORCE_REOPEN)); // Use new file
      remove_table_from_cache(thd,db,table_name); // Mark all in-use copies old
      mysql_lock_abort(thd,table);		 // end threads waiting on lock
    }
    VOID(quick_rm_table(old_db_type,db,old_name));
    if (close_data_tables(thd,db,table_name) ||
	reopen_tables(thd,1,0))
    {						// This shouldn't happen
      if (table)
	close_cached_table(thd,table);		// Remove lock for table
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }
  /* The ALTER TABLE is always in its own transaction */
  error = ha_commit_stmt(thd);
  if (ha_commit(thd))
    error=1;
  if (error)
  {
    VOID(pthread_mutex_unlock(&LOCK_open));
    VOID(pthread_cond_broadcast(&COND_refresh));
    goto err;
  }
  thd->proc_info="end";
  mysql_update_log.write(thd, thd->query,thd->query_length);
  if (mysql_bin_log.is_open())
  {
    thd->clear_error();
    Query_log_event qinfo(thd, thd->query, thd->query_length, 0);
    mysql_bin_log.write(&qinfo);
  }
  VOID(pthread_cond_broadcast(&COND_refresh));
  VOID(pthread_mutex_unlock(&LOCK_open));
#ifdef HAVE_BERKELEY_DB
  if (old_db_type == DB_TYPE_BERKELEY_DB)
  {
    /*
      For the alter table to be properly flushed to the logs, we
      have to open the new table.  If not, we get a problem on server
      shutdown.
    */
    char path[FN_REFLEN];
    my_snprintf(path, sizeof(path), "%s/%s/%s", mysql_data_home,
		new_db, table_name);
    fn_format(path,path,"","",4);
    table=open_temporary_table(thd, path, new_db, tmp_name,0);
    if (table)
    {
      intern_close_table(table);
      my_free((char*) table, MYF(0));
    }
    else
      sql_print_error("Warning: Could not open BDB table %s.%s after rename\n",
		      new_db,table_name);
    (void) berkeley_flush_logs();
  }
#endif
  table_list->table=0;				// For query cache
  query_cache_invalidate3(thd, table_list, 0);

end_temporary:
  my_snprintf(tmp_name, sizeof(tmp_name), ER(ER_INSERT_INFO),
	      (ulong) (copied + deleted), (ulong) deleted,
	      (ulong) thd->cuted_fields);
  if (do_send_ok)
    send_ok(thd,copied+deleted,0L,tmp_name);
  thd->some_tables_deleted=0;
  DBUG_RETURN(0);

 err:
  DBUG_RETURN(-1);
}


static int
copy_data_between_tables(TABLE *from,TABLE *to,
			 List<create_field> &create,
			 enum enum_duplicates handle_duplicates,
			 uint order_num, ORDER *order,
			 ha_rows *copied,
			 ha_rows *deleted)
{
  int error;
  Copy_field *copy,*copy_end;
  ulong found_count,delete_count;
  THD *thd= current_thd;
  uint length;
  SORT_FIELD *sortorder;
  READ_RECORD info;
  TABLE_LIST   tables;
  List<Item>   fields;
  List<Item>   all_fields;
  ha_rows examined_rows;
  bool auto_increment_field_copied= 0;
  DBUG_ENTER("copy_data_between_tables");

  if (!(copy= new Copy_field[to->fields]))
    DBUG_RETURN(-1);				/* purecov: inspected */

  to->file->external_lock(thd,F_WRLCK);
  from->file->info(HA_STATUS_VARIABLE);
  to->file->start_bulk_insert(from->file->records);

  List_iterator<create_field> it(create);
  create_field *def;
  copy_end=copy;
  for (Field **ptr=to->field ; *ptr ; ptr++)
  {
    def=it++;
    if (def->field)
    {
      if (*ptr == to->next_number_field)
        auto_increment_field_copied= TRUE;
      (copy_end++)->set(*ptr,def->field,0);
    }

  }

  found_count=delete_count=0;

  if (order)
  {
    from->sort.io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE),
					      MYF(MY_FAE | MY_ZEROFILL));
    bzero((char*) &tables,sizeof(tables));
    tables.table = from;
    tables.alias = tables.real_name= from->real_name;
    tables.db	 = from->table_cache_key;
    error=1;

    if (thd->lex->select_lex.setup_ref_array(thd, order_num) ||
	setup_order(thd, thd->lex->select_lex.ref_pointer_array,
		    &tables, fields, all_fields, order) ||
	!(sortorder=make_unireg_sortorder(order, &length)) ||
	(from->sort.found_records = filesort(thd, from, sortorder, length,
					     (SQL_SELECT *) 0, HA_POS_ERROR,
					     &examined_rows))
	== HA_POS_ERROR)
      goto err;
  };

  /*
    Turn off recovery logging since rollback of an alter table is to
    delete the new table so there is no need to log the changes to it.
  */
  error= ha_recovery_logging(thd,FALSE);
  if (error)
  {
    error= 1;
    goto err;
  }

  /* Handler must be told explicitly to retrieve all columns, because
     this function does not set field->query_id in the columns to the
     current query id */
  from->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
  init_read_record(&info, thd, from, (SQL_SELECT *) 0, 1,1);
  if (handle_duplicates == DUP_IGNORE ||
      handle_duplicates == DUP_REPLACE)
    to->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  thd->row_count= 0;
  while (!(error=info.read_record(&info)))
  {
    if (thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0));
      error= 1;
      break;
    }
    thd->row_count++;
    if (to->next_number_field)
    {
      if (auto_increment_field_copied)
        to->auto_increment_field_not_null= TRUE;
      else
        to->next_number_field->reset();
    }
    for (Copy_field *copy_ptr=copy ; copy_ptr != copy_end ; copy_ptr++)
    {
      copy_ptr->do_copy(copy_ptr);
    }
    if ((error=to->file->write_row((byte*) to->record[0])))
    {
      if ((handle_duplicates != DUP_IGNORE &&
	   handle_duplicates != DUP_REPLACE) ||
	  (error != HA_ERR_FOUND_DUPP_KEY &&
	   error != HA_ERR_FOUND_DUPP_UNIQUE))
      {
	to->file->print_error(error,MYF(0));
	break;
      }
      delete_count++;
    }
    else
      found_count++;
  }
  end_read_record(&info);
  free_io_cache(from);
  delete [] copy;				// This is never 0

  if (to->file->end_bulk_insert() && !error)
  {
    to->file->print_error(my_errno,MYF(0));
    error=1;
  }
  to->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  ha_recovery_logging(thd,TRUE);
  /*
    Ensure that the new table is saved properly to disk so that we
    can do a rename
  */
  if (ha_commit_stmt(thd))
    error=1;
  if (ha_commit(thd))
    error=1;
  if (to->file->external_lock(thd,F_UNLCK))
    error=1;
 err:
  free_io_cache(from);
  *copied= found_count;
  *deleted=delete_count;
  DBUG_RETURN(error > 0 ? -1 : 0);
}


/*
  Recreates tables by calling mysql_alter_table().

  SYNOPSIS
    mysql_recreate_table()
    thd			Thread handler
    tables		Tables to recreate
    do_send_ok          If we should send_ok() or leave it to caller

 RETURN
    Like mysql_alter_table().
*/
int mysql_recreate_table(THD *thd, TABLE_LIST *table_list,
                         bool do_send_ok)
{
  DBUG_ENTER("mysql_recreate_table");
  LEX *lex= thd->lex;
  HA_CREATE_INFO create_info;
  lex->create_list.empty();
  lex->key_list.empty();
  lex->col_list.empty();
  lex->alter_info.reset();
  lex->alter_info.is_simple= 0;                 // Force full recreate
  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  create_info.row_type=ROW_TYPE_DEFAULT;
  create_info.default_table_charset=default_charset_info;
  DBUG_RETURN(mysql_alter_table(thd, NullS, NullS, &create_info,
                                table_list, lex->create_list,
                                lex->key_list, 0, (ORDER *) 0,
                                DUP_ERROR, &lex->alter_info, do_send_ok));
}


int mysql_checksum_table(THD *thd, TABLE_LIST *tables, HA_CHECK_OPT *check_opt)
{
  TABLE_LIST *table;
  List<Item> field_list;
  Item *item;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysql_checksum_table");

  field_list.push_back(item = new Item_empty_string("Table", NAME_LEN*2));
  item->maybe_null= 1;
  field_list.push_back(item=new Item_int("Checksum",(longlong) 1,21));
  item->maybe_null= 1;
  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(-1);

  for (table= tables; table; table= table->next)
  {
    char table_name[NAME_LEN*2+2];
    TABLE *t;

    strxmov(table_name, table->db ,".", table->real_name, NullS);

    t= table->table= open_ltable(thd, table, TL_READ_NO_INSERT);
    thd->clear_error();			// these errors shouldn't get client

    protocol->prepare_for_resend();
    protocol->store(table_name, system_charset_info);

    if (!t)
    {
      /* Table didn't exist */
      protocol->store_null();
      thd->net.last_error[0]=0;
    }
    else
    {
      t->pos_in_table_list= table;

      if (t->file->table_flags() & HA_HAS_CHECKSUM &&
	  !(check_opt->flags & T_EXTEND))
	protocol->store((ulonglong)t->file->checksum());
      else if (!(t->file->table_flags() & HA_HAS_CHECKSUM) &&
	       (check_opt->flags & T_QUICK))
	protocol->store_null();
      else
      {
	/* calculating table's checksum */
	ha_checksum crc= 0;

	/* InnoDB must be told explicitly to retrieve all columns, because
	this function does not set field->query_id in the columns to the
	current query id */
	t->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);

	if (t->file->ha_rnd_init(1))
	  protocol->store_null();
	else
	{
	  while (!t->file->rnd_next(t->record[0]))
	  {
	    ha_checksum row_crc= 0;
	    if (t->record[0] != (byte*) t->field[0]->ptr)
	      row_crc= my_checksum(row_crc, t->record[0],
				   ((byte*) t->field[0]->ptr) - t->record[0]);

	    for (uint i= 0; i < t->fields; i++ )
	    {
	      Field *f= t->field[i];
	      if (f->type() == FIELD_TYPE_BLOB)
	      {
		String tmp;
		f->val_str(&tmp);
		row_crc= my_checksum(row_crc, (byte*) tmp.ptr(), tmp.length());
	      }
	      else
		row_crc= my_checksum(row_crc, (byte*) f->ptr,
				     f->pack_length());
	    }

	    crc+= row_crc;
	  }
	  protocol->store((ulonglong)crc);
          t->file->ha_rnd_end();
	}
      }
      thd->clear_error();
      close_thread_tables(thd);
      table->table=0;				// For query cache
    }
    if (protocol->write())
      goto err;
  }

  send_eof(thd);
  DBUG_RETURN(0);

 err:
  close_thread_tables(thd);			// Shouldn't be needed
  if (table)
    table->table=0;
  DBUG_RETURN(-1);
}
