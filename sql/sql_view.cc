/* Copyright (C) 2004 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_select.h"
#include "parse_file.h"
#include "sp.h"

static int mysql_register_view(THD *thd, TABLE_LIST *view,
			       enum_view_create_mode mode);

const char *sql_updatable_view_key_names[]= { "NO", "YES", "LIMIT1", NullS };
TYPELIB sql_updatable_view_key_typelib=
{
  array_elements(sql_updatable_view_key_names)-1, "",
  sql_updatable_view_key_names
};


/*
  Creating/altering VIEW procedure

  SYNOPSIS
    mysql_create_view()
    thd		- thread handler
    mode	- VIEW_CREATE_NEW, VIEW_ALTER, VIEW_CREATE_OR_REPLACE

  RETURN VALUE
     0	OK
    -1	Error
     1	Error and error message given
*/
int mysql_create_view(THD *thd,
		      enum_view_create_mode mode)
{
  LEX *lex= thd->lex;
  bool link_to_local;
  /* first table in list is target VIEW name => cut off it */
  TABLE_LIST *view= lex->unlink_first_table(&link_to_local);
  TABLE_LIST *tables= lex->query_tables;
  TABLE_LIST *tbl;
  SELECT_LEX *select_lex= &lex->select_lex, *sl;
  SELECT_LEX_UNIT *unit= &lex->unit;
  int res= 0;
  DBUG_ENTER("mysql_create_view");

  if (lex->proc_list.first ||
      lex->result)
  {
    my_error(ER_VIEW_SELECT_CLAUSE, MYF(0), (lex->result ?
                                             "INTO" :
                                             "PROCEDURE"));
    res= -1;
    goto err;
  }
  if (lex->derived_tables ||
      lex->variables_used || lex->param_list.elements)
  {
    my_error((lex->derived_tables ?
              ER_VIEW_SELECT_DERIVED :
              ER_VIEW_SELECT_VARIABLE), MYF(0));
    res= -1;
    goto err;
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (check_access(thd, CREATE_VIEW_ACL, view->db, &view->grant.privilege,
                   0, 0) ||
      grant_option && check_grant(thd, CREATE_VIEW_ACL, view, 0, 1, 0))
    DBUG_RETURN(1);
  for (sl= select_lex; sl; sl= sl->next_select())
  {
    for (tbl= sl->get_table_list(); tbl; tbl= tbl->next_local)
    {
      /*
        Ensure that we have some privilage on this table, more strict check
        will be done on column level after preparation,

        SELECT_ACL will be checked for sure for all fields because it is
        listed first (if we have not rights to SELECT from whole table this
        right will be written as tbl->grant.want_privilege and will be checked
        later (except fields which need any privilege and can be updated).
      */
      if ((check_access(thd, SELECT_ACL, tbl->db,
                        &tbl->grant.privilege, 0, 1) ||
           grant_option && check_grant(thd, SELECT_ACL, tbl, 0, 1, 1)) &&
          (check_access(thd, INSERT_ACL, tbl->db,
                        &tbl->grant.privilege, 0, 1) ||
           grant_option && check_grant(thd, INSERT_ACL, tbl, 0, 1, 1)) &&
          (check_access(thd, DELETE_ACL, tbl->db,
                        &tbl->grant.privilege, 0, 1) ||
           grant_option && check_grant(thd, DELETE_ACL, tbl, 0, 1, 1)) &&
          (check_access(thd, UPDATE_ACL, tbl->db,
                        &tbl->grant.privilege, 0, 1) ||
           grant_option && check_grant(thd, UPDATE_ACL, tbl, 0, 1, 1))
         )
      {
        my_printf_error(ER_TABLEACCESS_DENIED_ERROR,
                        ER(ER_TABLEACCESS_DENIED_ERROR),
                        MYF(0),
                        "ANY",
                        thd->priv_user,
                        thd->host_or_ip,
                        tbl->real_name);
        DBUG_RETURN(-1);
      }
      /* mark this table as table which will be checked after preparation */
      tbl->table_in_first_from_clause= 1;

      /*
        We need to check only SELECT_ACL for all normal fields, fields
        where we need any privilege will be pmarked later
      */
      tbl->grant.want_privilege= SELECT_ACL;
      /*
        Make sure that all rights are loaded to table 'grant' field.

        tbl->real_name will be correct name of table because VIEWs are
        not opened yet.
      */
      fill_effective_table_privileges(thd, &tbl->grant, tbl->db,
                                      tbl->real_name);
    }
  }

  if (&lex->select_lex != lex->all_selects_list)
  {
    /* check tables of subqueries */
    for (tbl= tables; tbl; tbl= tbl->next_global)
    {
      if (!tbl->table_in_first_from_clause)
      {
        if (check_access(thd, SELECT_ACL, tbl->db,
                         &tbl->grant.privilege, 0, 0) ||
            grant_option && check_grant(thd, SELECT_ACL, tbl, 0, 1, 0))
        {
          res= 1;
          goto err;
        }
      }
    }
  }
  /*
    Mark fields for special privilege check (any privilege)
  */
  for (sl= select_lex; sl; sl= sl->next_select())
  {
    List_iterator_fast<Item> it(sl->item_list);
    Item *item;
    while ((item= it++))
    {
      if (item->type() == Item::FIELD_ITEM)
        ((Item_field *)item)->any_privileges= 1;
    }
  }
#endif

  if ((res= open_and_lock_tables(thd, tables)))
    DBUG_RETURN(res);

  /* check that tables are not temporary */
  for (tbl= tables; tbl; tbl= tbl->next_global)
  {
    if (tbl->table->tmp_table != NO_TMP_TABLE && !test(tbl->view))
    {
      my_error(ER_VIEW_SELECT_TMPTABLE, MYF(0), tbl->alias);
      res= -1;
      goto err;
    }

    /*
      Copy privileges of underlaying VIEWs which was filled by
      fill_effective_table_privileges
      (they was not copied in derived tables processing)
    */
    tbl->table->grant.privilege= tbl->grant.privilege;
  }

  // prepare select to resolve all fields
  lex->view_prepare_mode= 1;
  if (unit->prepare(thd, 0, 0))
  {
    /*
      some errors from prepare are reported to user, if is not then
      it will be checked after err: label
    */
    res= 1;
    goto err;
  }

  /* view list (list of view fields names) */
  if (lex->view_list.elements)
  {
    if (lex->view_list.elements != select_lex->item_list.elements)
    {
      my_message(ER_VIEW_WRONG_LIST, ER(ER_VIEW_WRONG_LIST), MYF(0));
      goto err;
    }
    List_iterator_fast<Item> it(select_lex->item_list);
    List_iterator_fast<LEX_STRING> nm(lex->view_list);
    Item *item;
    LEX_STRING *name;
    while((item= it++, name= nm++))
    {
      item->set_name(name->str, name->length, system_charset_info);
    }
  }

  /* Test absence of duplicates names */
  {
    Item *item;
    List_iterator_fast<Item> it(select_lex->item_list);
    it++;
    while((item= it++))
    {
      Item *check;
      List_iterator_fast<Item> itc(select_lex->item_list);
      while((check= itc++) && check != item)
      {
        if (strcmp(item->name, check->name) == 0)
        {
          my_error(ER_DUP_FIELDNAME, MYF(0), item->name);
          DBUG_RETURN(-1);
        }
      }
    }
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /*
    Compare/check grants on view with grants of underlaying tables
  */
  for (sl= select_lex; sl; sl= sl->next_select())
  {
    char *db= view->db ? view->db : thd->db;
    List_iterator_fast<Item> it(sl->item_list);
    Item *item;
    fill_effective_table_privileges(thd, &view->grant, db,
                                    view->real_name);
    while((item= it++))
    {
      uint priv= (get_column_grant(thd, &view->grant, db,
                                    view->real_name, item->name) &
                  VIEW_ANY_ACL);
      if (item->type() == Item::FIELD_ITEM)
      {
        Item_field *fld= (Item_field *)item;
        /*
          There are no any privileges on VIWE column or there are
          some other privileges then we have for underlaying table
        */
        if (priv == 0 || test(~fld->have_privileges & priv))
        {
          /* VIEW column has more privileges */
          my_printf_error(ER_COLUMNACCESS_DENIED_ERROR,
                          ER(ER_COLUMNACCESS_DENIED_ERROR),
                          MYF(0),
                          "create view",
                          thd->priv_user,
                          thd->host_or_ip,
                          item->name,
                          view->real_name);
          DBUG_RETURN(-1);
        }
      }
      else
      {
        if (!test(priv & SELECT_ACL))
        {
          /* user have not privilege to SELECT expression */
          my_printf_error(ER_COLUMNACCESS_DENIED_ERROR,
                          ER(ER_COLUMNACCESS_DENIED_ERROR),
                          MYF(0),
                          "select",
                          thd->priv_user,
                          thd->host_or_ip,
                          item->name,
                          view->real_name);
          DBUG_RETURN(-1);
        }
      }
    }
  }
#endif

  if (wait_if_global_read_lock(thd, 0))
  {
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }
  VOID(pthread_mutex_lock(&LOCK_open));
  if ((res= mysql_register_view(thd, view, mode)))
  {
    VOID(pthread_mutex_unlock(&LOCK_open));
    start_waiting_global_read_lock(thd);
    goto err;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
  start_waiting_global_read_lock(thd);

  send_ok(thd);
  lex->link_first_table_back(view, link_to_local);
  return 0;

err:
  thd->proc_info= "end";
  lex->link_first_table_back(view, link_to_local);
  unit->cleanup();
  if (thd->net.report_error)
    res= -1;
  DBUG_RETURN(res);
}


/* index of revision number in following table */
static const int revision_number_position= 5;
/* index of last required parameter for making view */
static const int last_parameter= 8;

static char *view_field_names[]=
{
  (char*)"query",
  (char*)"md5",
  (char*)"updatable",
  (char*)"algorithm",
  (char*)"syscharset",
  (char*)"revision",
  (char*)"timestamp",
  (char*)"create-version",
  (char*)"source"
};

// table of VIEW .frm field descriprors
static File_option view_parameters[]=
{{{view_field_names[0], 5},	offsetof(TABLE_LIST, query),
  FILE_OPTIONS_STRING},
 {{view_field_names[1], 3},	offsetof(TABLE_LIST, md5),
  FILE_OPTIONS_STRING},
 {{view_field_names[2], 9},	offsetof(TABLE_LIST, updatable_view),
  FILE_OPTIONS_ULONGLONG},
 {{view_field_names[3], 9},	offsetof(TABLE_LIST, algorithm),
  FILE_OPTIONS_ULONGLONG},
 {{view_field_names[4], 10},    offsetof(TABLE_LIST, syscharset),
  FILE_OPTIONS_STRING},
 {{view_field_names[5], 8},	offsetof(TABLE_LIST, revision),
  FILE_OPTIONS_REV},
 {{view_field_names[6], 9},	offsetof(TABLE_LIST, timestamp),
  FILE_OPTIONS_TIMESTAMP},
 {{view_field_names[7], 14},	offsetof(TABLE_LIST, file_version),
  FILE_OPTIONS_ULONGLONG},
 {{view_field_names[8], 6},	offsetof(TABLE_LIST, source),
  FILE_OPTIONS_ESTRING},
 {{NULL, 0},			0,
  FILE_OPTIONS_STRING}
};

static LEX_STRING view_file_type[]= {{(char*)"VIEW", 4}};


/*
  Register VIEW (write .frm & process .frm's history backups)

  SYNOPSIS
    mysql_register_view()
    thd		- thread handler
    view	- view description
    mode	- VIEW_CREATE_NEW, VIEW_ALTER, VIEW_CREATE_OR_REPLACE

  RETURN
     0	OK
    -1	Error
     1	Error and error message given
*/
static int mysql_register_view(THD *thd, TABLE_LIST *view,
			       enum_view_create_mode mode)
{
  char buff[4096];
  String str(buff,(uint32) sizeof(buff), system_charset_info);
  char md5[33];
  bool can_be_merged;
  char dir_buff[FN_REFLEN], file_buff[FN_REFLEN];
  LEX_STRING dir, file;
  DBUG_ENTER("mysql_register_view");

  // print query
  str.length(0);
  {
    ulong sql_mode= thd->variables.sql_mode & MODE_ANSI_QUOTES;
    thd->variables.sql_mode&= ~MODE_ANSI_QUOTES;
    thd->lex->unit.print(&str);
    thd->variables.sql_mode|= sql_mode;
  }
  str.append('\0');
  DBUG_PRINT("VIEW", ("View: %s", str.ptr()));

  // print file name
  (void) my_snprintf(dir_buff, FN_REFLEN, "%s/%s/",
		     mysql_data_home, view->db);
  unpack_filename(dir_buff, dir_buff);
  dir.str= dir_buff;
  dir.length= strlen(dir_buff);

  file.str= file_buff;
  file.length= my_snprintf(file_buff, FN_REFLEN, "%s%s",
			   view->real_name, reg_ext);
  /* init timestamp */
  if (!test(view->timestamp.str))
    view->timestamp.str= view->timestamp_buffer;

  // check old .frm
  {
    char path_buff[FN_REFLEN];
    LEX_STRING path;
    path.str= path_buff;
    fn_format(path_buff, file.str, dir.str, 0, MY_UNPACK_FILENAME);
    path.length= strlen(path_buff);

    if (!access(path.str, F_OK))
    {
      if (mode == VIEW_CREATE_NEW)
      {
	my_error(ER_TABLE_EXISTS_ERROR, MYF(0), view->alias);
	DBUG_RETURN(1);
      }

      File_parser *parser= sql_parse_prepare(&path, &thd->mem_root, 0);
      if (parser)
      {
	if(parser->ok() &&
	   !strncmp("VIEW", parser->type()->str, parser->type()->length))
	{
	  /*
	    read revision number

	    TODO: read dependense list, too, to process cascade/restrict
	    TODO: special cascade/restrict procedure for alter?
	  */
	  if (parser->parse((gptr)view, &thd->mem_root,
			    view_parameters + revision_number_position, 1))
	  {
	    DBUG_RETURN(1);
	  }
	}
	else
	{
          my_error(ER_WRONG_OBJECT, MYF(0), (view->db?view->db:thd->db),
                   view->real_name, "VIEW");
	  DBUG_RETURN(1);
	}
      }
      else
      {
	DBUG_RETURN(1);
      }
    }
    else
    {
      if (mode == VIEW_ALTER)
      {
	my_error(ER_NO_SUCH_TABLE, MYF(0), view->db, view->alias);
	DBUG_RETURN(1);
      }
    }
  }
  // fill structure
  view->query.str= (char*)str.ptr();
  view->query.length= str.length()-1; // we do not need last \0
  view->source.str= thd->query;
  view->source.length= thd->query_length;
  view->syscharset.str= (char *)system_charset_info->csname;
  view->syscharset.length= strlen(view->syscharset.str);
  view->file_version= 1;
  view->calc_md5(md5);
  view->md5.str= md5;
  view->md5.length= 32;
  can_be_merged= thd->lex->can_be_merged();
  if (thd->lex->create_view_algorithm == VIEW_ALGORITHM_MERGE &&
      !thd->lex->can_be_merged())
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_VIEW_MERGE,
                 ER(ER_WARN_VIEW_MERGE));
    thd->lex->create_view_algorithm= VIEW_ALGORITHM_UNDEFINED;
  }
  view->algorithm= thd->lex->create_view_algorithm;
  if ((view->updatable_view= (can_be_merged &&
                              view->algorithm != VIEW_ALGORITHM_TMEPTABLE)))
  {
    // TODO: change here when we will support UNIONs
    for (TABLE_LIST *tbl= (TABLE_LIST *)thd->lex->select_lex.table_list.first;
         tbl;
         tbl= tbl->next_local)
    {
      if (tbl->view != 0 && !tbl->updatable_view)
      {
        view->updatable_view= 0;
        break;
      }
    }
  }
  if (sql_create_definition_file(&dir, &file, view_file_type,
				 (gptr)view, view_parameters, 3))
  {
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  read VIEW .frm and create structures

  SYNOPSIS
    mysql_make_view()
    parser		- parser object;
    table		- TABLE_LIST structure for filling

  RETURN
    TRUE  OK
    FALSE error
*/
my_bool
mysql_make_view(File_parser *parser, TABLE_LIST *table)
{
  bool include_proc_table= 0;
  DBUG_ENTER("mysql_make_view");

  if (table->view)
  {
    DBUG_PRINT("info",
               ("VIEW %s.%s is already processed on previos PS/SP execution",
                table->view_db.str, table->view_name.str));
    DBUG_RETURN(0);
  }

  TABLE_LIST *old_next, *tbl_end, *tbl_next;
  SELECT_LEX *end;
  THD *thd= current_thd;
  LEX *old_lex= thd->lex, *lex;
  int res= 0;

  /*
    For now we assume that tables will not be changed during PS life (it
    will be TRUE as far as we make new table cache).
  */
  Item_arena *arena= thd->current_arena, backup;
  if (arena)
    thd->set_n_backup_item_arena(arena, &backup);

  /* init timestamp */
  if (!test(table->timestamp.str))
    table->timestamp.str= table->timestamp_buffer;
  /*
    TODO: when VIEWs will be stored in cache, table mem_root should
    be used here
  */
  if (parser->parse((gptr)table, &thd->mem_root, view_parameters,
                    last_parameter))
    goto err;

  /*
    Save VIEW parameters, which will be wiped out by derived table
    processing
  */
  table->view_db.str= table->db;
  table->view_db.length= table->db_length;
  table->view_name.str= table->real_name;
  table->view_name.length= table->real_name_length;

  /*TODO: md5 test here and warning if it is differ */

  /*
    TODO: TABLE mem root should be used here when VIEW will be stored in
    TABLE cache

    now Lex placed in statement memory
  */
  table->view= lex= thd->lex= (LEX*) new(&thd->mem_root) st_lex_local;
  mysql_init_query(thd, (uchar*)table->query.str, table->query.length, TRUE);
  lex->select_lex.select_number= ++thd->select_number;
  old_lex->derived_tables|= DERIVED_VIEW;
  {
    ulong options= thd->options;
    /* switch off modes which can prevent normal parsing of VIEW
      - MODE_REAL_AS_FLOAT            affect only CREATE TABLE parsing
      + MODE_PIPES_AS_CONCAT          affect expression parsing
      + MODE_ANSI_QUOTES              affect expression parsing
      + MODE_IGNORE_SPACE             affect expression parsing
      - MODE_NOT_USED                 not used :)
      * MODE_ONLY_FULL_GROUP_BY       affect execution
      * MODE_NO_UNSIGNED_SUBTRACTION  affect execution
      - MODE_NO_DIR_IN_CREATE         affect table creation only
      - MODE_POSTGRESQL               compounded from other modes
      - MODE_ORACLE                   compounded from other modes
      - MODE_MSSQL                    compounded from other modes
      - MODE_DB2                      compounded from other modes
      - MODE_MAXDB                    affect only CREATE TABLE parsing
      - MODE_NO_KEY_OPTIONS           affect only SHOW
      - MODE_NO_TABLE_OPTIONS         affect only SHOW
      - MODE_NO_FIELD_OPTIONS         affect only SHOW
      - MODE_MYSQL323                 affect only SHOW
      - MODE_MYSQL40                  affect only SHOW
      - MODE_ANSI                     compounded from other modes
                                      (+ transaction mode)
      ? MODE_NO_AUTO_VALUE_ON_ZERO    affect UPDATEs
      + MODE_NO_BACKSLASH_ESCAPES     affect expression parsing
    */
    thd->options&= ~(MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
                     MODE_IGNORE_SPACE | MODE_NO_BACKSLASH_ESCAPES);
    CHARSET_INFO *save_cs= thd->variables.character_set_client;
    if (!table->syscharset.length)
      thd->variables.character_set_client= system_charset_info;
    else
    {
      if (!(thd->variables.character_set_client=
            get_charset_by_csname(table->syscharset.str,
                                  MY_CS_PRIMARY, MYF(MY_WME))))
      {
        thd->variables.character_set_client= save_cs;
        goto err;
      }
    }
    res= yyparse((void *)thd);
    thd->variables.character_set_client= save_cs;
    thd->options= options;
  }
  if (!res && !thd->is_fatal_error)
  {
    TABLE_LIST *top_view= (table->belong_to_view ?
                           table->belong_to_view :
                           table);

    if (lex->spfuns.records)
    {
      /* move SP to main LEX */
      sp_merge_funs(old_lex, lex);
      if (lex->spfuns.array.buffer)
        hash_free(&lex->spfuns);
      if (old_lex->proc_table == 0 &&
          (old_lex->proc_table=
           (TABLE_LIST*)thd->calloc(sizeof(TABLE_LIST))) != 0)
      {
        TABLE_LIST *table= old_lex->proc_table;
        table->db= (char*)"mysql";
        table->db_length= 5;
        table->real_name= table->alias= (char*)"proc";
        table->real_name_length= 4;
        table->cacheable_table= 1;
        include_proc_table= 1;
      }
    }

    old_next= table->next_global;
    if ((table->next_global= lex->query_tables))
      table->next_global->prev_global= &table->next_global;

    /* mark to avoid temporary table using and put view reference*/
    for (TABLE_LIST *tbl= table->next_global; tbl; tbl= tbl->next_global)
    {
      tbl->skip_temporary= 1;
      tbl->belong_to_view= top_view;
    }

    /*
      check rights to run commands (EXPLAIN SELECT & SHOW CREATE) which show
      underlaying tables
    */
    if ((old_lex->sql_command == SQLCOM_SELECT && old_lex->describe) ||
        old_lex->sql_command == SQLCOM_SHOW_CREATE)
    {
      if (check_table_access(thd, SELECT_ACL, table->next_global, 1) &&
          check_table_access(thd, SHOW_VIEW_ACL, table->next_global, 1))
      {
        my_error(ER_VIEW_NO_EXPLAIN, MYF(0));
        goto err;
      }
    }

    /* move SQL_NO_CACHE & Co to whole query */
    old_lex->safe_to_cache_query= (old_lex->safe_to_cache_query &&
				   lex->safe_to_cache_query);
    /* move SQL_CACHE to whole query */
    if (lex->select_lex.options & OPTION_TO_QUERY_CACHE)
      old_lex->select_lex.options|= OPTION_TO_QUERY_CACHE;

    /*
      check MERGE algorithm ability
      - algorithm is not explicit TEMPORARY TABLE
      - VIEW SELECT allow marging
      - VIEW used in subquery or command support MERGE algorithm
    */
    if (table->algorithm != VIEW_ALGORITHM_TMEPTABLE &&
	lex->can_be_merged() &&
        (table->select_lex->master_unit() != &old_lex->unit ||
         old_lex->can_use_merged()) &&
        !old_lex->can_not_use_merged())
    {
      /*
        TODO: support multi tables substitutions

        table->next_global should be the same as
        (TABLE_LIST *)lex->select_lex.table_list.first;
      */
      TABLE_LIST *view_table= table->next_global;
      /* lex should contain at least one table */
      DBUG_ASSERT(view_table != 0);

      table->effective_algorithm= VIEW_ALGORITHM_MERGE;
      DBUG_PRINT("info", ("algorithm: MERGE"));
      table->updatable= (table->updatable_view != 0);

      if (old_next)
      {
	if ((view_table->next_global= old_next))
          old_next->prev_global= &view_table->next_global;
      }
      table->ancestor= view_table;
      // next table should include SELECT_LEX under this table SELECT_LEX
      table->ancestor->select_lex= table->select_lex;
      /*
        move lock type (TODO: should we issue error in case of TMPTABLE
        algorithm and non-read locking)?
      */
      view_table->lock_type= table->lock_type;

      /* Store WHERE clause for postprocessing in setup_ancestor */
      table->where= lex->select_lex.where;

      /*
	This SELECT_LEX will be linked in global SELECT_LEX list
	to make it processed by mysql_handle_derived(),
	but it will not be included to SELECT_LEX tree, because it
	will not be executed
      */
      goto ok;
    }

    table->effective_algorithm= VIEW_ALGORITHM_TMEPTABLE;
    DBUG_PRINT("info", ("algorithm: TEMPORARY TABLE"));
    lex->select_lex.linkage= DERIVED_TABLE_TYPE;
    table->updatable= 0;

    /* SELECT tree link */
    lex->unit.include_down(table->select_lex);
    lex->unit.slave= &lex->select_lex; // fix include_down initialisation

    if (old_next)
    {
      if ((tbl_end= table->next_global))
      {
	for (; (tbl_next= tbl_end->next_global); tbl_end= tbl_next);
	if ((tbl_end->next_global= old_next))
          tbl_end->next_global->prev_global= &tbl_end->next_global;
      }
      else
      {
        /* VIEW do not contain tables */
        table->next_global= old_next;
      }
    }

    table->derived= &lex->unit;
  }
  else
    goto err;

ok:
  if (arena)
    thd->restore_backup_item_arena(arena, &backup);
  /* global SELECT list linking */
  end= &lex->select_lex;	// primary SELECT_LEX is always last
  end->link_next= old_lex->all_selects_list;
  old_lex->all_selects_list->link_prev= &end->link_next;
  old_lex->all_selects_list= lex->all_selects_list;
  lex->all_selects_list->link_prev=
    (st_select_lex_node**)&old_lex->all_selects_list;

  if (include_proc_table)
  {
    TABLE_LIST *proc= old_lex->proc_table;
    if((proc->next_global= table->next_global))
    {
      table->next_global->prev_global= &proc->next_global;
    }
    proc->prev_global= &table->next_global;
    table->next_global= proc;
  }

  thd->lex= old_lex;
  DBUG_RETURN(0);

err:
  if (arena)
    thd->restore_backup_item_arena(arena, &backup);
  table->view= 0;	// now it is not VIEW placeholder
  thd->lex= old_lex;
  DBUG_RETURN(1);
}


/*
  drop view

  SYNOPSIS
    mysql_drop_view()
    thd		- thread handler
    views	- views to delete
    drop_mode	- cascade/check

  RETURN VALUE
     0	OK
    -1	Error
     1	Error and error message given
*/
int mysql_drop_view(THD *thd, TABLE_LIST *views, enum_drop_mode drop_mode)
{
  DBUG_ENTER("mysql_drop_view");
  char path[FN_REFLEN];
  TABLE_LIST *view;
  bool type= 0;

  for (view= views; view; view= view->next_local)
  {
    strxmov(path, mysql_data_home, "/", view->db, "/", view->real_name,
	    reg_ext, NullS);
    (void) unpack_filename(path, path);
    VOID(pthread_mutex_lock(&LOCK_open));
    if (access(path, F_OK) || (type= (mysql_frm_type(path) != FRMTYPE_VIEW)))
    {
      char name[FN_REFLEN];
      my_snprintf(name, sizeof(name), "%s.%s", view->db, view->real_name);
      if (thd->lex->drop_if_exists)
      {
	push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			    ER_BAD_TABLE_ERROR, ER(ER_BAD_TABLE_ERROR),
			    name);
	VOID(pthread_mutex_unlock(&LOCK_open));
	continue;
      }
      if (type)
        my_error(ER_WRONG_OBJECT, MYF(0), view->db, view->real_name, "VIEW");
      else
        my_error(ER_BAD_TABLE_ERROR, MYF(0), name);
      goto err;
    }
    if (my_delete(path, MYF(MY_WME)))
      goto err;
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  send_ok(thd);
  DBUG_RETURN(0);

err:
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(-1);

}


/*
  Check type of .frm if we are not going to parse it

  SYNOPSIS
    mysql_frm_type()
    path	path to file

  RETURN
    FRMTYPE_ERROR	error
    FRMTYPE_TABLE	table
    FRMTYPE_VIEW	view
*/

frm_type_enum mysql_frm_type(char *path)
{
  File file;
  char header[10];	//"TYPE=VIEW\n" it is 10 characters
  DBUG_ENTER("mysql_frm_type");

  if ((file= my_open(path, O_RDONLY | O_SHARE, MYF(MY_WME))) < 0)
  {
    DBUG_RETURN(FRMTYPE_ERROR);
  }
  if (my_read(file, (byte*) header, 10, MYF(MY_WME)) == MY_FILE_ERROR)
  {
    my_close(file, MYF(MY_WME));
    DBUG_RETURN(FRMTYPE_ERROR);
  }
  my_close(file, MYF(MY_WME));
  if (strncmp(header, "TYPE=VIEW\n", 10) != 0)
    DBUG_RETURN(FRMTYPE_TABLE);
  DBUG_RETURN(FRMTYPE_VIEW);
}


/*
  check of key (primary or unique) presence in updatable view

  SYNOPSIS
    check_key_in_view()
    thd     thread handler
    view    view for check with opened table

  RETURN
    FALSE   OK
    TRUE    view do not contain key or all fields
*/

bool check_key_in_view(THD *thd, TABLE_LIST *view)
{
  DBUG_ENTER("check_key_in_view");
  if (!view->view)
    DBUG_RETURN(FALSE); /* it is normal table */

  TABLE *table= view->table;
  Item **trans= view->field_translation;
  KEY *key_info= table->key_info;
  uint primary_key= table->primary_key;
  uint num= view->view->select_lex.item_list.elements;
  DBUG_ASSERT(view->table != 0 && view->field_translation != 0);

  /* try to find key */
  for (uint i=0; i < table->keys; i++, key_info++)
  {
    if (i == primary_key && !strcmp(key_info->name, primary_key_name) ||
        key_info->flags & HA_NOSAME)
    {
      KEY_PART_INFO *key_part= key_info->key_part;
      bool found= 1;
      for (uint j=0; j < key_info->key_parts && found; j++, key_part++)
      {
        found= 0;
        for (uint k= 0; k < num; k++)
        {
          if (trans[k]->type() == Item::FIELD_ITEM &&
              ((Item_field *)trans[k])->field == key_part->field &&
              (key_part->field->flags & NOT_NULL_FLAG))
          {
            found= 1;
            break;
          }
        }
      }
      if (found)
        DBUG_RETURN(FALSE);
    }
  }

  /* check all fields presence */
  {
    Field **field_ptr= table->field;
    for (; *field_ptr; ++field_ptr)
    {
      uint i= 0;
      for (; i < num; i++)
      {
        if (trans[i]->type() == Item::FIELD_ITEM &&
            ((Item_field *)trans[i])->field == *field_ptr)
          break;
      }
      if (i >= num)
      {
        ulong mode= thd->variables.sql_updatable_view_key;
        /* 1 == YES, 2 == LIMIT1 */
        if (mode == 1 ||
            (mode == 2 &&
             thd->lex->select_lex.select_limit == 1))
        {
          DBUG_RETURN(TRUE);
        }
        else
        {
          push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                       ER_WARN_VIEW_WITHOUT_KEY, ER(ER_WARN_VIEW_WITHOUT_KEY));
          DBUG_RETURN(FALSE);
        }
      }
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  insert fields from VIEW (MERGE algorithm) into given list

  SYNOPSIS
    insert_view_fields()
    list      list for insertion
    view      view for processing
*/

void insert_view_fields(List<Item> *list, TABLE_LIST *view)
{
  uint num= view->view->select_lex.item_list.elements;
  Item **trans= view->field_translation;
  DBUG_ENTER("insert_view_fields");
  if (!trans)
    DBUG_VOID_RETURN;

  for (uint i= 0; i < num; i++)
  {
    if (trans[i]->type() == Item::FIELD_ITEM)
    {
      list->push_back(trans[i]);
    }
  }
  DBUG_VOID_RETURN;
}
