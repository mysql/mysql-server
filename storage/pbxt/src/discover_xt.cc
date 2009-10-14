/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 * Derived from code Copyright (C) 2000-2004 MySQL AB
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  Created by Leslie on 8/27/08.
 *
 */

#include "xt_config.h"

#ifndef DRIZZLED
#include "mysql_priv.h"
#include "item_create.h"
#include <m_ctype.h>
#else
#include <drizzled/session.h>
#include <drizzled/server_includes.h>
#include <drizzled/sql_base.h>
#endif

#include "strutil_xt.h"
#include "ha_pbxt.h"
#include "discover_xt.h"
#include "ha_xtsys.h"

#ifndef DRIZZLED
#if MYSQL_VERSION_ID > 60005
#define DOT_STR(x)			x.str
#else
#define DOT_STR(x)			x
#endif
#endif

#ifndef DRIZZLED
#define LOCK_OPEN_HACK_REQUIRED
#endif // DRIZZLED

#ifdef LOCK_OPEN_HACK_REQUIRED
///////////////////////////////
/*
 * Unfortunately I cannot use the standard mysql_create_table_no_lock() because it will lock "LOCK_open"
 * which has already been locked while the server is performing table discovery. So I have added this hack 
 * in here to create my own version. The following macros will make the changes I need to get it to work.
 * The actual function code has been copied here without changes.
 *
 * Its almost enough to make you want to cry. :(
*/
//-----------------------------

#ifdef pthread_mutex_lock
#undef pthread_mutex_lock
#endif

#ifdef pthread_mutex_unlock
#undef pthread_mutex_unlock
#endif

#define mysql_create_table_no_lock hacked_mysql_create_table_no_lock
#define pthread_mutex_lock(l)
#define pthread_mutex_unlock(l)

#define check_engine(t, n, c) (0)
#define set_table_default_charset(t, c, d)

void calculate_interval_lengths(CHARSET_INFO *cs, TYPELIB *interval,
                                uint32 *max_length, uint32 *tot_length);

uint build_tmptable_filename(THD* thd, char *buff, size_t bufflen);
uint build_table_filename(char *buff, size_t bufflen, const char *db,
                          const char *table_name, const char *ext, uint flags);

//////////////////////////////////////////////////////////
////// START OF CUT AND PASTES FROM  sql_table.cc ////////
//////////////////////////////////////////////////////////

// sort_keys() cut and pasted directly from sql_table.cc. 
static int sort_keys(KEY *a, KEY *b)
{
  ulong a_flags= a->flags, b_flags= b->flags;
  
  if (a_flags & HA_NOSAME)
  {
    if (!(b_flags & HA_NOSAME))
      return -1;
    if ((a_flags ^ b_flags) & (HA_NULL_PART_KEY | HA_END_SPACE_KEY))
    {
      /* Sort NOT NULL keys before other keys */
      return (a_flags & (HA_NULL_PART_KEY | HA_END_SPACE_KEY)) ? 1 : -1;
    }
    if (a->name == primary_key_name)
      return -1;
    if (b->name == primary_key_name)
      return 1;
    /* Sort keys don't containing partial segments before others */
    if ((a_flags ^ b_flags) & HA_KEY_HAS_PART_KEY_SEG)
      return (a_flags & HA_KEY_HAS_PART_KEY_SEG) ? 1 : -1;
  }
  else if (b_flags & HA_NOSAME)
    return 1;					// Prefer b

  if ((a_flags ^ b_flags) & HA_FULLTEXT)
  {
    return (a_flags & HA_FULLTEXT) ? 1 : -1;
  }
  /*
    Prefer original key order.	usable_key_parts contains here
    the original key position.
  */
  return ((a->usable_key_parts < b->usable_key_parts) ? -1 :
	  (a->usable_key_parts > b->usable_key_parts) ? 1 :
	  0);
}

// check_if_keyname_exists() cut and pasted directly from sql_table.cc. 
static bool
check_if_keyname_exists(const char *name, KEY *start, KEY *end)
{
  for (KEY *key=start ; key != end ; key++)
    if (!my_strcasecmp(system_charset_info,name,key->name))
      return 1;
  return 0;
}

// make_unique_key_name() cut and pasted directly from sql_table.cc. 
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


// prepare_blob_field() cut and pasted directly from sql_table.cc. 
static bool prepare_blob_field(THD *thd, Create_field *sql_field)
{
  DBUG_ENTER("prepare_blob_field");

  if (sql_field->length > MAX_FIELD_VARCHARLENGTH &&
      !(sql_field->flags & BLOB_FLAG))
  {
    /* Convert long VARCHAR columns to TEXT or BLOB */
    char warn_buff[MYSQL_ERRMSG_SIZE];

    if (sql_field->def || (thd->variables.sql_mode & (MODE_STRICT_TRANS_TABLES |
                                                      MODE_STRICT_ALL_TABLES)))
    {
      my_error(ER_TOO_BIG_FIELDLENGTH, MYF(0), sql_field->field_name,
               MAX_FIELD_VARCHARLENGTH / sql_field->charset->mbmaxlen);
      DBUG_RETURN(1);
    }
    sql_field->sql_type= MYSQL_TYPE_BLOB;
    sql_field->flags|= BLOB_FLAG;
    sprintf(warn_buff, ER(ER_AUTO_CONVERT), sql_field->field_name,
            (sql_field->charset == &my_charset_bin) ? "VARBINARY" : "VARCHAR",
            (sql_field->charset == &my_charset_bin) ? "BLOB" : "TEXT");
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_AUTO_CONVERT,
                 warn_buff);
  }
    
  if ((sql_field->flags & BLOB_FLAG) && sql_field->length)
  {
    if (sql_field->sql_type == MYSQL_TYPE_BLOB)
    {
      /* The user has given a length to the blob column */
      sql_field->sql_type= get_blob_type_from_length(sql_field->length);
      sql_field->pack_length= calc_pack_length(sql_field->sql_type, 0);
    }
    sql_field->length= 0;
  }
  DBUG_RETURN(0);
}

//////////////////////////////
// mysql_prepare_create_table() cut and pasted directly from sql_table.cc.
static int
mysql_prepare_create_table(THD *thd, HA_CREATE_INFO *create_info,
                           Alter_info *alter_info,
                           bool tmp_table,
                           uint *db_options,
                           handler *file, KEY **key_info_buffer,
                           uint *key_count, int select_field_count)
{
  const char	*key_name;
  Create_field	*sql_field,*dup_field;
  uint		field,null_fields,blob_columns,max_key_length;
  ulong		record_offset= 0;
  KEY		*key_info;
  KEY_PART_INFO *key_part_info;
  int		timestamps= 0, timestamps_with_niladic= 0;
  int		field_no,dup_no;
  int		select_field_pos,auto_increment=0;
  List_iterator<Create_field> it(alter_info->create_list);
  List_iterator<Create_field> it2(alter_info->create_list);
  uint total_uneven_bit_length= 0;
  DBUG_ENTER("mysql_prepare_create_table");

  select_field_pos= alter_info->create_list.elements - select_field_count;
  null_fields=blob_columns=0;
  create_info->varchar= 0;
  max_key_length= file->max_key_length();

  for (field_no=0; (sql_field=it++) ; field_no++)
  {
    CHARSET_INFO *save_cs;

    /*
      Initialize length from its original value (number of characters),
      which was set in the parser. This is necessary if we're
      executing a prepared statement for the second time.
    */
    sql_field->length= sql_field->char_length;
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

    save_cs= sql_field->charset;
    if ((sql_field->flags & BINCMP_FLAG) &&
	!(sql_field->charset= get_charset_by_csname(sql_field->charset->csname,
						    MY_CS_BINSORT,MYF(0))))
    {
      char tmp[64];
      strmake(strmake(tmp, save_cs->csname, sizeof(tmp)-4),
              STRING_WITH_LEN("_bin"));
      my_error(ER_UNKNOWN_COLLATION, MYF(0), tmp);
      DBUG_RETURN(TRUE);
    }

    /*
      Convert the default value from client character
      set into the column character set if necessary.
    */
    if (sql_field->def && 
        save_cs != sql_field->def->collation.collation &&
        (sql_field->sql_type == MYSQL_TYPE_VAR_STRING ||
         sql_field->sql_type == MYSQL_TYPE_STRING ||
         sql_field->sql_type == MYSQL_TYPE_SET ||
         sql_field->sql_type == MYSQL_TYPE_ENUM))
    {
      /*
        Starting from 5.1 we work here with a copy of Create_field
        created by the caller, not with the instance that was
        originally created during parsing. It's OK to create
        a temporary item and initialize with it a member of the
        copy -- this item will be thrown away along with the copy
        at the end of execution, and thus not introduce a dangling
        pointer in the parsed tree of a prepared statement or a
        stored procedure statement.
      */
      sql_field->def= sql_field->def->safe_charset_converter(save_cs);

      if (sql_field->def == NULL)
      {
        /* Could not convert */
        my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
        DBUG_RETURN(TRUE);
      }
    }

    if (sql_field->sql_type == MYSQL_TYPE_SET ||
        sql_field->sql_type == MYSQL_TYPE_ENUM)
    {
      uint32 dummy;
      CHARSET_INFO *cs= sql_field->charset;
      TYPELIB *interval= sql_field->interval;

      /*
        Create typelib from interval_list, and if necessary
        convert strings from client character set to the
        column character set.
      */
      if (!interval)
      {
        /*
          Create the typelib in runtime memory - we will free the
          occupied memory at the same time when we free this
          sql_field -- at the end of execution.
        */
        interval= sql_field->interval= typelib(thd->mem_root,
                                               sql_field->interval_list);
        List_iterator<String> int_it(sql_field->interval_list);
        String conv, *tmp;
        char comma_buf[2];
        int comma_length= cs->cset->wc_mb(cs, ',', (uchar*) comma_buf,
                                          (uchar*) comma_buf + 
                                          sizeof(comma_buf));
        DBUG_ASSERT(comma_length > 0);
        for (uint i= 0; (tmp= int_it++); i++)
        {
          uint lengthsp;
          if (String::needs_conversion(tmp->length(), tmp->charset(),
                                       cs, &dummy))
          {
            uint cnv_errs;
            conv.copy(tmp->ptr(), tmp->length(), tmp->charset(), cs, &cnv_errs);
            interval->type_names[i]= strmake_root(thd->mem_root, conv.ptr(),
                                                  conv.length());
            interval->type_lengths[i]= conv.length();
          }

          // Strip trailing spaces.
          lengthsp= cs->cset->lengthsp(cs, interval->type_names[i],
                                       interval->type_lengths[i]);
          interval->type_lengths[i]= lengthsp;
          ((uchar *)interval->type_names[i])[lengthsp]= '\0';
          if (sql_field->sql_type == MYSQL_TYPE_SET)
          {
            if (cs->coll->instr(cs, interval->type_names[i], 
                                interval->type_lengths[i], 
                                comma_buf, comma_length, NULL, 0))
            {
              my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "set", tmp->ptr());
              DBUG_RETURN(TRUE);
            }
          }
        }
        sql_field->interval_list.empty(); // Don't need interval_list anymore
      }

      if (sql_field->sql_type == MYSQL_TYPE_SET)
      {
        uint32 field_length;
        if (sql_field->def != NULL)
        {
          char *not_used;
          uint not_used2;
          bool not_found= 0;
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(TRUE);
            }

            /* else, NULL is an allowed value */
            (void) find_set(interval, NULL, 0,
                            cs, &not_used, &not_used2, &not_found);
          }
          else /* not NULL */
          {
            (void) find_set(interval, def->ptr(), def->length(),
                            cs, &not_used, &not_used2, &not_found);
          }

          if (not_found)
          {
            my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
            DBUG_RETURN(TRUE);
          }
        }
        calculate_interval_lengths(cs, interval, &dummy, &field_length);
        sql_field->length= field_length + (interval->count - 1);
      }
      else  /* MYSQL_TYPE_ENUM */
      {
        uint32 field_length;
        DBUG_ASSERT(sql_field->sql_type == MYSQL_TYPE_ENUM);
        if (sql_field->def != NULL)
        {
          String str, *def= sql_field->def->val_str(&str);
          if (def == NULL) /* SQL "NULL" maps to NULL */
          {
            if ((sql_field->flags & NOT_NULL_FLAG) != 0)
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(TRUE);
            }

            /* else, the defaults yield the correct length for NULLs. */
          } 
          else /* not NULL */
          {
            def->length(cs->cset->lengthsp(cs, def->ptr(), def->length()));
            if (find_type2(interval, def->ptr(), def->length(), cs) == 0) /* not found */
            {
              my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
              DBUG_RETURN(TRUE);
            }
          }
        }
        calculate_interval_lengths(cs, interval, &field_length, &dummy);
        sql_field->length= field_length;
      }
      set_if_smaller(sql_field->length, MAX_FIELD_WIDTH-1);
    }

    if (sql_field->sql_type == MYSQL_TYPE_BIT)
    { 
      sql_field->pack_flag= FIELDFLAG_NUMBER;
      if (file->ha_table_flags() & HA_CAN_BIT_FIELD)
        total_uneven_bit_length+= sql_field->length & 7;
      else
        sql_field->pack_flag|= FIELDFLAG_TREAT_BIT_AS_CHAR;
    }

    sql_field->create_length_to_internal_length();
    if (prepare_blob_field(thd, sql_field))
      DBUG_RETURN(TRUE);

    if (!(sql_field->flags & NOT_NULL_FLAG))
      null_fields++;

    if (check_column_name(sql_field->field_name))
    {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), sql_field->field_name);
      DBUG_RETURN(TRUE);
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
	  my_error(ER_DUP_FIELDNAME, MYF(0), sql_field->field_name);
	  DBUG_RETURN(TRUE);
	}
	else
	{
	  /* Field redefined */
	  sql_field->def=		dup_field->def;
	  sql_field->sql_type=		dup_field->sql_type;
	  sql_field->charset=		(dup_field->charset ?
					 dup_field->charset :
					 create_info->default_table_charset);
	  sql_field->length=		dup_field->char_length;
          sql_field->pack_length=	dup_field->pack_length;
          sql_field->key_length=	dup_field->key_length;
	  sql_field->decimals=		dup_field->decimals;
	  sql_field->create_length_to_internal_length();
	  sql_field->unireg_check=	dup_field->unireg_check;
          /* 
            We're making one field from two, the result field will have
            dup_field->flags as flags. If we've incremented null_fields
            because of sql_field->flags, decrement it back.
          */
          if (!(sql_field->flags & NOT_NULL_FLAG))
            null_fields--;
	  sql_field->flags=		dup_field->flags;
          sql_field->interval=          dup_field->interval;
	  it2.remove();			// Remove first (create) definition
	  select_field_pos--;
	  break;
	}
      }
    }
    /* Don't pack rows in old tables if the user has requested this */
    if ((sql_field->flags & BLOB_FLAG) ||
	(sql_field->sql_type == MYSQL_TYPE_VARCHAR &&
         create_info->row_type != ROW_TYPE_FIXED))
      (*db_options)|= HA_OPTION_PACK_RECORD;
    it2.rewind();
  }

  /* record_offset will be increased with 'length-of-null-bits' later */
  record_offset= 0;
  null_fields+= total_uneven_bit_length;

  it.rewind();
  while ((sql_field=it++))
  {
    DBUG_ASSERT(sql_field->charset != 0);

    if (prepare_create_field(sql_field, &blob_columns, 
			     &timestamps, &timestamps_with_niladic,
			     file->ha_table_flags()))
      DBUG_RETURN(TRUE);
    if (sql_field->sql_type == MYSQL_TYPE_VARCHAR)
      create_info->varchar= TRUE;
    sql_field->offset= record_offset;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    record_offset+= sql_field->pack_length;
  }
  if (timestamps_with_niladic > 1)
  {
    my_message(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,
               ER(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS), MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (auto_increment > 1)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (auto_increment &&
      (file->ha_table_flags() & HA_NO_AUTO_INCREMENT))
  {
    my_message(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT,
               ER(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT), MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (blob_columns && (file->ha_table_flags() & HA_NO_BLOBS))
  {
    my_message(ER_TABLE_CANT_HANDLE_BLOB, ER(ER_TABLE_CANT_HANDLE_BLOB),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  /* Create keys */

  List_iterator<Key> key_iterator(alter_info->key_list);
  List_iterator<Key> key_iterator2(alter_info->key_list);
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
    DBUG_PRINT("info", ("key name: '%s'  type: %d", key->DOT_STR(name) ? key->DOT_STR(name) :
                        "(none)" , key->type));
    LEX_STRING key_name_str;
    if (key->type == Key::FOREIGN_KEY)
    {
      fk_key_count++;
      Foreign_key *fk_key= (Foreign_key*) key;
      if (fk_key->ref_columns.elements &&
	  fk_key->ref_columns.elements != fk_key->columns.elements)
      {
        my_error(ER_WRONG_FK_DEF, MYF(0),
                 (fk_key->DOT_STR(name) ?  fk_key->DOT_STR(name) : "foreign key without name"),
                 ER(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
	DBUG_RETURN(TRUE);
      }
      continue;
    }
    (*key_count)++;
    tmp=file->max_key_parts();
    if (key->columns.elements > tmp)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),tmp);
      DBUG_RETURN(TRUE);
    }
    key_name_str.str= (char*) key->DOT_STR(name);
    key_name_str.length= key->DOT_STR(name) ? strlen(key->DOT_STR(name)) : 0;
    if (check_string_char_length(&key_name_str, "", NAME_CHAR_LEN,
                                 system_charset_info, 1))
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), key->DOT_STR(name));
      DBUG_RETURN(TRUE);
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
             key2->DOT_STR(name) != ignore_key &&
             !foreign_key_prefix(key, key2)))
        {
          /* TODO: issue warning message */
          /* mark that the generated key should be ignored */
          if (!key2->generated ||
              (key->generated && key->columns.elements <
               key2->columns.elements))
            key->DOT_STR(name)= ignore_key;
          else
          {
            key2->DOT_STR(name)= ignore_key;
            key_parts-= key2->columns.elements;
            (*key_count)--;
          }
          break;
        }
      }
    }
    if (key->DOT_STR(name) != ignore_key)
      key_parts+=key->columns.elements;
    else
      (*key_count)--;
    if (key->DOT_STR(name) && !tmp_table && (key->type != Key::PRIMARY) &&
	!my_strcasecmp(system_charset_info,key->DOT_STR(name),primary_key_name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key->DOT_STR(name));
      DBUG_RETURN(TRUE);
    }
  }
  tmp=file->max_keys();
  if (*key_count > tmp)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),tmp);
    DBUG_RETURN(TRUE);
  }

  (*key_info_buffer)= key_info= (KEY*) sql_calloc(sizeof(KEY) * (*key_count));
  key_part_info=(KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO)*key_parts);
  if (!*key_info_buffer || ! key_part_info)
    DBUG_RETURN(TRUE);				// Out of memory

  key_iterator.rewind();
  key_number=0;
  for (; (key=key_iterator++) ; key_number++)
  {
    uint key_length=0;
    Key_part_spec *column;

    if (key->DOT_STR(name) == ignore_key)
    {
      /* ignore redundant keys */
      do
	key=key_iterator++;
      while (key && key->DOT_STR(name) == ignore_key);
      if (!key)
	break;
    }

    switch (key->type) {
    case Key::MULTIPLE:
	key_info->flags= 0;
	break;
    case Key::FULLTEXT:
	key_info->flags= HA_FULLTEXT;
	if ((key_info->parser_name= &key->key_create_info.parser_name)->str)
          key_info->flags|= HA_USES_PARSER;
        else
          key_info->parser_name= 0;
	break;
    case Key::SPATIAL:
#ifdef HAVE_SPATIAL
	key_info->flags= HA_SPATIAL;
	break;
#else
	my_error(ER_FEATURE_DISABLED, MYF(0),
                 sym_group_geom.name, sym_group_geom.needed_define);
	DBUG_RETURN(TRUE);
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
    key_info->algorithm= key->key_create_info.algorithm;

    if (key->type == Key::FULLTEXT)
    {
      if (!(file->ha_table_flags() & HA_CAN_FULLTEXT))
      {
	my_message(ER_TABLE_CANT_HANDLE_FT, ER(ER_TABLE_CANT_HANDLE_FT),
                   MYF(0));
	DBUG_RETURN(TRUE);
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
      if (!(file->ha_table_flags() & HA_CAN_RTREEKEYS))
      {
        my_message(ER_TABLE_CANT_HANDLE_SPKEYS, ER(ER_TABLE_CANT_HANDLE_SPKEYS),
                   MYF(0));
        DBUG_RETURN(TRUE);
      }
      if (key_info->key_parts != 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "SPATIAL INDEX");
	DBUG_RETURN(TRUE);
      }
    }
    else if (key_info->algorithm == HA_KEY_ALG_RTREE)
    {
#ifdef HAVE_RTREE_KEYS
      if ((key_info->key_parts & 1) == 1)
      {
	my_error(ER_WRONG_ARGUMENTS, MYF(0), "RTREE INDEX");
	DBUG_RETURN(TRUE);
      }
      /* TODO: To be deleted */
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "RTREE INDEX");
      DBUG_RETURN(TRUE);
#else
      my_error(ER_FEATURE_DISABLED, MYF(0),
               sym_group_rtree.name, sym_group_rtree.needed_define);
      DBUG_RETURN(TRUE);
#endif
    }

    /* Take block size from key part or table part */
    /*
      TODO: Add warning if block size changes. We can't do it here, as
      this may depend on the size of the key
    */
    key_info->block_size= (key->key_create_info.block_size ?
                           key->key_create_info.block_size :
                           create_info->key_block_size);

    if (key_info->block_size)
      key_info->flags|= HA_USES_BLOCK_SIZE;

    List_iterator<Key_part_spec> cols(key->columns), cols2(key->columns);
    CHARSET_INFO *ft_key_charset=0;  // for FULLTEXT
    for (uint column_nr=0 ; (column=cols++) ; column_nr++)
    {
      uint length;
      Key_part_spec *dup_column;

      it.rewind();
      field=0;
      while ((sql_field=it++) &&
	     my_strcasecmp(system_charset_info,
			   column->DOT_STR(field_name),
			   sql_field->field_name))
	field++;
      if (!sql_field)
      {
	my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name);
	DBUG_RETURN(TRUE);
      }
      while ((dup_column= cols2++) != column)
      {
        if (!my_strcasecmp(system_charset_info,
	     	           column->DOT_STR(field_name), dup_column->DOT_STR(field_name)))
	{
	  my_printf_error(ER_DUP_FIELDNAME,
			  ER(ER_DUP_FIELDNAME),MYF(0),
			  column->field_name);
	  DBUG_RETURN(TRUE);
	}
      }
      cols2.rewind();
      if (key->type == Key::FULLTEXT)
      {
	if ((sql_field->sql_type != MYSQL_TYPE_STRING &&
	     sql_field->sql_type != MYSQL_TYPE_VARCHAR &&
	     !f_is_blob(sql_field->pack_flag)) ||
	    sql_field->charset == &my_charset_bin ||
	    sql_field->charset->mbminlen > 1 || // ucs2 doesn't work yet
	    (ft_key_charset && sql_field->charset != ft_key_charset))
	{
	    my_error(ER_BAD_FT_COLUMN, MYF(0), column->field_name);
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

        if (key->type == Key::SPATIAL && column->length)
        {
          my_error(ER_WRONG_SUB_KEY, MYF(0));
	  DBUG_RETURN(TRUE);
	}

	if (f_is_blob(sql_field->pack_flag) ||
            (f_is_geom(sql_field->pack_flag) && key->type != Key::SPATIAL))
	{
	  if (!(file->ha_table_flags() & HA_CAN_INDEX_BLOBS))
	  {
	    my_error(ER_BLOB_USED_AS_KEY, MYF(0), column->field_name);
	    DBUG_RETURN(TRUE);
	  }
          if (f_is_geom(sql_field->pack_flag) && sql_field->geom_type ==
              Field::GEOM_POINT)
            column->length= 25;
	  if (!column->length)
	  {
	    my_error(ER_BLOB_KEY_WITHOUT_LENGTH, MYF(0), column->field_name);
	    DBUG_RETURN(TRUE);
	  }
	}
#ifdef HAVE_SPATIAL
	if (key->type == Key::SPATIAL)
	{
	  if (!column->length)
	  {
	    /*
              4 is: (Xmin,Xmax,Ymin,Ymax), this is for 2D case
              Lately we'll extend this code to support more dimensions
	    */
	    column->length= 4*sizeof(double);
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
            null_fields--;
	  }
	  else
          {
            key_info->flags|= HA_NULL_PART_KEY;
            if (!(file->ha_table_flags() & HA_NULL_IN_KEY))
            {
              my_error(ER_NULL_COLUMN_IN_INDEX, MYF(0), column->field_name);
              DBUG_RETURN(TRUE);
            }
            if (key->type == Key::SPATIAL)
            {
              my_message(ER_SPATIAL_CANT_HAVE_NULL,
                         ER(ER_SPATIAL_CANT_HAVE_NULL), MYF(0));
              DBUG_RETURN(TRUE);
            }
          }
	}
	if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
	{
	  if (column_nr == 0 || (file->ha_table_flags() & HA_AUTO_PART_KEY))
	    auto_increment--;			// Field is used
	}
      }

      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16) sql_field->offset;
      key_part_info->key_type=sql_field->pack_flag;
      length= sql_field->key_length;

      if (column->length)
      {
	if (f_is_blob(sql_field->pack_flag))
	{
	  if ((length=column->length) > max_key_length ||
	      length > file->max_key_part_length())
	  {
	    length=min(max_key_length, file->max_key_part_length());
	    if (key->type == Key::MULTIPLE)
	    {
	      /* not a critical problem */
	      char warn_buff[MYSQL_ERRMSG_SIZE];
	      my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
			  length);
	      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			   ER_TOO_LONG_KEY, warn_buff);
              /* Align key length to multibyte char boundary */
              length-= length % sql_field->charset->mbmaxlen;
	    }
	    else
	    {
	      my_error(ER_TOO_LONG_KEY,MYF(0),length);
	      DBUG_RETURN(TRUE);
	    }
	  }
	}
	else if (!f_is_geom(sql_field->pack_flag) &&
		  (column->length > length ||
                   !Field::type_can_have_key_part (sql_field->sql_type) ||
		   ((f_is_packed(sql_field->pack_flag) ||
		     ((file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS) &&
		      (key_info->flags & HA_NOSAME))) &&
		    column->length != length)))
	{
	  my_message(ER_WRONG_SUB_KEY, ER(ER_WRONG_SUB_KEY), MYF(0));
	  DBUG_RETURN(TRUE);
	}
	else if (!(file->ha_table_flags() & HA_NO_PREFIX_CHAR_KEYS))
	  length=column->length;
      }
      else if (length == 0)
      {
	my_error(ER_WRONG_KEY_COLUMN, MYF(0), column->field_name);
	  DBUG_RETURN(TRUE);
      }
      if (length > file->max_key_part_length() && key->type != Key::FULLTEXT)
      {
        length= file->max_key_part_length();
	if (key->type == Key::MULTIPLE)
	{
	  /* not a critical problem */
	  char warn_buff[MYSQL_ERRMSG_SIZE];
	  my_snprintf(warn_buff, sizeof(warn_buff), ER(ER_TOO_LONG_KEY),
		      length);
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		       ER_TOO_LONG_KEY, warn_buff);
          /* Align key length to multibyte char boundary */
          length-= length % sql_field->charset->mbmaxlen;
	}
	else
	{
	  my_error(ER_TOO_LONG_KEY,MYF(0),length);
	  DBUG_RETURN(TRUE);
	}
      }
      key_part_info->length=(uint16) length;
      /* Use packed keys for long strings on the first column */
      if (!((*db_options) & HA_OPTION_NO_PACK_KEYS) &&
	  (length >= KEY_DEFAULT_PACK_LENGTH &&
	   (sql_field->sql_type == MYSQL_TYPE_STRING ||
	    sql_field->sql_type == MYSQL_TYPE_VARCHAR ||
	    sql_field->pack_flag & FIELDFLAG_BLOB)))
      {
	if ((column_nr == 0 && (sql_field->pack_flag & FIELDFLAG_BLOB)) ||
            sql_field->sql_type == MYSQL_TYPE_VARCHAR)
	  key_info->flags|= HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY;
	else
	  key_info->flags|= HA_PACK_KEY;
      }
      /* Check if the key segment is partial, set the key flag accordingly */
      if (length != sql_field->key_length)
        key_info->flags|= HA_KEY_HAS_PART_KEY_SEG;

      key_length+=length;
      key_part_info++;

      /* Create the key name based on the first column (if not given) */
      if (column_nr == 0)
      {
	if (key->type == Key::PRIMARY)
	{
	  if (primary_key)
	  {
	    my_message(ER_MULTIPLE_PRI_KEY, ER(ER_MULTIPLE_PRI_KEY),
                       MYF(0));
	    DBUG_RETURN(TRUE);
	  }
	  key_name=primary_key_name;
	  primary_key=1;
	}
	else if (!(key_name = key->DOT_STR(name)))
	  key_name=make_unique_key_name(sql_field->field_name,
					*key_info_buffer, key_info);
	if (check_if_keyname_exists(key_name, *key_info_buffer, key_info))
	{
	  my_error(ER_DUP_KEYNAME, MYF(0), key_name);
	  DBUG_RETURN(TRUE);
	}
	key_info->name=(char*) key_name;
      }
    }
    if (!key_info->name || check_column_name(key_info->name))
    {
      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key_info->name);
      DBUG_RETURN(TRUE);
    }
    if (!(key_info->flags & HA_NULL_PART_KEY))
      unique_key=1;
    key_info->key_length=(uint16) key_length;
    if (key_length > max_key_length && key->type != Key::FULLTEXT)
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),max_key_length);
      DBUG_RETURN(TRUE);
    }
    key_info++;
  }
  if (!unique_key && !primary_key &&
      (file->ha_table_flags() & HA_REQUIRE_PRIMARY_KEY))
  {
    my_message(ER_REQUIRES_PRIMARY_KEY, ER(ER_REQUIRES_PRIMARY_KEY), MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (auto_increment > 0)
  {
    my_message(ER_WRONG_AUTO_KEY, ER(ER_WRONG_AUTO_KEY), MYF(0));
    DBUG_RETURN(TRUE);
  }
  /* Sort keys in optimized order */
  my_qsort((uchar*) *key_info_buffer, *key_count, sizeof(KEY),
	   (qsort_cmp) sort_keys);
  create_info->null_bits= null_fields;

  /* Check fields. */
  it.rewind();
  while ((sql_field=it++))
  {
    Field::utype type= (Field::utype) MTYP_TYPENR(sql_field->unireg_check);

    if (thd->variables.sql_mode & MODE_NO_ZERO_DATE &&
        !sql_field->def &&
        sql_field->sql_type == MYSQL_TYPE_TIMESTAMP &&
        (sql_field->flags & NOT_NULL_FLAG) &&
        (type == Field::NONE || type == Field::TIMESTAMP_UN_FIELD))
    {
      /*
        An error should be reported if:
          - NO_ZERO_DATE SQL mode is active;
          - there is no explicit DEFAULT clause (default column value);
          - this is a TIMESTAMP column;
          - the column is not NULL;
          - this is not the DEFAULT CURRENT_TIMESTAMP column.

        In other words, an error should be reported if
          - NO_ZERO_DATE SQL mode is active;
          - the column definition is equivalent to
            'column_name TIMESTAMP DEFAULT 0'.
      */

      my_error(ER_INVALID_DEFAULT, MYF(0), sql_field->field_name);
      DBUG_RETURN(TRUE);
    }
  }

  DBUG_RETURN(FALSE);
}

//////////////////////////////
// mysql_create_table_no_lock() cut and pasted directly from sql_table.cc. (I did make is static after copying it.)

static bool mysql_create_table_no_lock(THD *thd,
                                const char *db, const char *table_name,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                bool internal_tmp_table,
                                uint select_field_count)
{
  char			path[FN_REFLEN];
  uint          path_length;
  const char	*alias;
  uint			db_options, key_count;
  KEY			*key_info_buffer;
  handler		*file;
  bool			error= TRUE;
  DBUG_ENTER("mysql_create_table_no_lock");
  DBUG_PRINT("enter", ("db: '%s'  table: '%s'  tmp: %d",
                       db, table_name, internal_tmp_table));


  /* Check for duplicate fields and check type of table to create */
  if (!alter_info->create_list.elements)
  {
    my_message(ER_TABLE_MUST_HAVE_COLUMNS, ER(ER_TABLE_MUST_HAVE_COLUMNS),
               MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (check_engine(thd, table_name, create_info))
    DBUG_RETURN(TRUE);
  db_options= create_info->table_options;
  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    db_options|=HA_OPTION_PACK_RECORD;
  alias= table_case_name(create_info, table_name);

  /* PMC - Done to avoid getting the partition handler by mistake! */
  if (!(file= new (thd->mem_root) ha_xtsys(pbxt_hton, NULL)))
  {
    mem_alloc_error(sizeof(handler));
    DBUG_RETURN(TRUE);
  }

  set_table_default_charset(thd, create_info, (char*) db);

  if (mysql_prepare_create_table(thd, create_info, alter_info,
                                 internal_tmp_table,
                                 &db_options, file,
                                 &key_info_buffer, &key_count,
                                 select_field_count))
    goto err;

      /* Check if table exists */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    path_length= build_tmptable_filename(thd, path, sizeof(path));
    create_info->table_options|=HA_CREATE_DELAY_KEY_WRITE;
  }
  else  
  {
 #ifdef FN_DEVCHAR
    /* check if the table name contains FN_DEVCHAR when defined */
    if (strchr(alias, FN_DEVCHAR))
    {
      my_error(ER_WRONG_TABLE_NAME, MYF(0), alias);
      DBUG_RETURN(TRUE);
    }
#endif
    path_length= build_table_filename(path, sizeof(path), db, alias, reg_ext,
                                      internal_tmp_table ? FN_IS_TMP : 0);
  }

  /* Check if table already exists */
  if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      find_temporary_table(thd, db, table_name))
  {
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      create_info->table_existed= 1;		// Mark that table existed
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          alias);
      error= 0;
      goto err;
    }
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), alias);
    goto err;
  }

  pthread_mutex_lock(&LOCK_open);
  if (!internal_tmp_table && !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
  {
    if (!access(path,F_OK))
    {
      if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
        goto warn;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      goto unlock_and_end;
    }
    /*
      We don't assert here, but check the result, because the table could be
      in the table definition cache and in the same time the .frm could be
      missing from the disk, in case of manual intervention which deletes
      the .frm file. The user has to use FLUSH TABLES; to clear the cache.
      Then she could create the table. This case is pretty obscure and
      therefore we don't introduce a new error message only for it.
    */
    if (get_cached_table_share(db, alias))
    {
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), table_name);
      goto unlock_and_end;
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
    int retcode = ha_table_exists_in_engine(thd, db, table_name);
    DBUG_PRINT("info", ("exists_in_engine: %u",retcode));
    switch (retcode)
    {
      case HA_ERR_NO_SUCH_TABLE:
        /* Normal case, no table exists. we can go and create it */
        break;
      case HA_ERR_TABLE_EXIST:
        DBUG_PRINT("info", ("Table existed in handler"));

        if (create_if_not_exists)
          goto warn;
        my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
        goto unlock_and_end;
        break;
      default:
        DBUG_PRINT("info", ("error: %u from storage engine", retcode));
        my_error(retcode, MYF(0),table_name);
        goto unlock_and_end;
    }
  }

  thd_proc_info(thd, "creating table");
  create_info->table_existed= 0;		// Mark that table is created

  create_info->table_options=db_options;

  path[path_length - reg_ext_length]= '\0'; // Remove .frm extension
  if (rea_create_table(thd, path, db, table_name,
                       create_info, alter_info->create_list,
                       key_count, key_info_buffer, file))
    goto unlock_and_end;

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
  {
    /* Open table and put in temporary table list */
#if MYSQL_VERSION_ID > 60005
    if (!(open_temporary_table(thd, path, db, table_name, 1, OTM_OPEN)))
#else
    if (!(open_temporary_table(thd, path, db, table_name, 1)))
#endif
    {
#if MYSQL_VERSION_ID > 60005
      (void) rm_temporary_table(create_info->db_type, path, false);
#else
      (void) rm_temporary_table(create_info->db_type, path);
#endif
      goto unlock_and_end;
    }
    thd->thread_specific_used= TRUE;
  }

  /*
    Don't write statement if:
    - It is an internal temporary table,
    - Row-based logging is used and it we are creating a temporary table, or
    - The binary log is not open.
    Otherwise, the statement shall be binlogged.
   */
  if (!internal_tmp_table &&
      (!thd->current_stmt_binlog_row_based ||
       (thd->current_stmt_binlog_row_based &&
        !(create_info->options & HA_LEX_CREATE_TMP_TABLE))))
    write_bin_log(thd, TRUE, thd->query, thd->query_length);
  error= FALSE;
unlock_and_end:
  pthread_mutex_unlock(&LOCK_open);

err:
  thd_proc_info(thd, "After create");
  delete file;
  DBUG_RETURN(error);

warn:
  error= FALSE;
  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                      ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                      alias);
  create_info->table_existed= 1;		// Mark that table existed
  goto unlock_and_end;
}

////////////////////////////////////////////////////////
////// END OF CUT AND PASTES FROM  sql_table.cc ////////
////////////////////////////////////////////////////////

#endif // LOCK_OPEN_HACK_REQUIRED

//------------------------------
int xt_create_table_frm(handlerton *hton, THD* thd, const char *db, const char *name, DT_FIELD_INFO *info, DT_KEY_INFO *XT_UNUSED(keys), xtBool skip_existing)
{
#ifdef DRIZZLED
    drizzled::message::Table table_proto;

	static const char *ext = ".dfe";
	static const int ext_len = 4;
#else
	static const char *ext = ".frm";
	static const int ext_len = 4;
#endif
	int err = 1;
	//HA_CREATE_INFO create_info = {0};
	//Alter_info alter_info;
	char field_length_buffer[12], *field_length_ptr;
	LEX  *save_lex= thd->lex, mylex;
	
	memset(&mylex.create_info, 0, sizeof(HA_CREATE_INFO));

	thd->lex = &mylex;
    lex_start(thd);
	
	/* setup the create info */
	mylex.create_info.db_type = hton;
#ifndef DRIZZLED 
	mylex.create_info.frm_only = 1;
#endif
 	mylex.create_info.default_table_charset = system_charset_info;
	
	/* setup the column info. */
	while (info->field_name) {		
		 LEX_STRING field_name, comment;		 
		 field_name.str = (char*)(info->field_name);
		 field_name.length = strlen(info->field_name);
		 
		 comment.str = (char*)(info->comment);
		 comment.length = strlen(info->comment);
		 			
		 if (info->field_length) {
			sprintf(field_length_buffer, "%d", info->field_length);
			field_length_ptr = field_length_buffer;
		 } else 
			field_length_ptr = NULL;

#ifdef DRIZZLED
		if (add_field_to_list(thd, &field_name, info->field_type, field_length_ptr, info->field_decimal_length,
			info->field_flags,
            COLUMN_FORMAT_TYPE_FIXED,
		    NULL /*default_value*/, NULL /*on_update_value*/, &comment, NULL /*change*/,
            NULL /*interval_list*/, info->field_charset))
#else
		if (add_field_to_list(thd, &field_name, info->field_type, field_length_ptr, info->field_decimal_length,
			info->field_flags,
#if MYSQL_VERSION_ID > 60005
				HA_SM_DISK,
				COLUMN_FORMAT_TYPE_FIXED,
#endif
		       NULL /*default_value*/, NULL /*on_update_value*/, &comment, NULL /*change*/, 
		       NULL /*interval_list*/, info->field_charset, 0 /*uint_geom_type*/)) 
#endif
			goto error;


		info++;
	}

	if (skip_existing) {
		size_t db_len = strlen(db);
		size_t name_len = strlen(name);
		size_t len = db_len + 1 + name_len + ext_len + 1;
		char *path = (char *)xt_malloc_ns(len);
		memcpy(path, db, db_len);
		memcpy(path + db_len + 1, name, name_len);
		memcpy(path + db_len + 1 + name_len, ext, ext_len);
		path[db_len] = XT_DIR_CHAR;
		path[len - 1] = '\0';
		xtBool exists = xt_fs_exists(path);
		xt_free_ns(path);
		if (exists)
			goto noerror;
	}
	
	/* Create an internal temp table */
#ifdef DRIZZLED
    table_proto.set_name(name);
    table_proto.set_type(drizzled::message::Table::STANDARD);

	if (mysql_create_table_no_lock(thd, db, name, &mylex.create_info, &table_proto, &mylex.alter_info, 1, 0, false)) 
		goto error;
#else
	if (mysql_create_table_no_lock(thd, db, name, &mylex.create_info, &mylex.alter_info, 1, 0)) 
		goto error;
#endif

	noerror:
	err = 0;

	error:
	lex_end(&mylex);
	thd->lex = save_lex;
	return err;
}

