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


/* Function with list databases, tables or fields */

#include "mysql_priv.h"
#include "sql_select.h"                         // For select_describe
#include "sql_acl.h"
#include "repl_failsafe.h"
#include "sp_head.h"
#include <my_dir.h>

#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"			// For berkeley_show_logs
#endif

static const char *grant_names[]={
  "select","insert","update","delete","create","drop","reload","shutdown",
  "process","file","grant","references","index","alter"};

#ifndef NO_EMBEDDED_ACCESS_CHECKS
static TYPELIB grant_types = { sizeof(grant_names)/sizeof(char **),
                               "grant_types",
                               grant_names, NULL};
#endif

static int
store_create_info(THD *thd, TABLE *table, String *packet);
static int
view_store_create_info(THD *thd, TABLE_LIST *table, String *packet);


/***************************************************************************
  List all open tables in a database
***************************************************************************/

int mysqld_show_open_tables(THD *thd,const char *wild)
{
  List<Item> field_list;
  OPEN_TABLE_LIST *open_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_open_tables");

  field_list.push_back(new Item_empty_string("Database",NAME_LEN));
  field_list.push_back(new Item_empty_string("Table",NAME_LEN));
  field_list.push_back(new Item_return_int("In_use", 1, MYSQL_TYPE_TINY));
  field_list.push_back(new Item_return_int("Name_locked", 4, MYSQL_TYPE_TINY));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  if (!(open_list=list_open_tables(thd,wild)) && thd->is_fatal_error)
    DBUG_RETURN(-1);

  for (; open_list ; open_list=open_list->next)
  {
    protocol->prepare_for_resend();
    protocol->store(open_list->db, system_charset_info);
    protocol->store(open_list->table, system_charset_info);
    protocol->store_tiny((longlong) open_list->in_use);
    protocol->store_tiny((longlong) open_list->locked);
    if (protocol->write())
    {
      DBUG_RETURN(-1);
    }
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


/***************************************************************************
** List all table types supported 
***************************************************************************/

int mysqld_show_storage_engines(THD *thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_storage_engines");

  field_list.push_back(new Item_empty_string("Engine",10));
  field_list.push_back(new Item_empty_string("Support",10));
  field_list.push_back(new Item_empty_string("Comment",80));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  const char *default_type_name= 
    ha_get_storage_engine((enum db_type)thd->variables.table_type);

  show_table_type_st *types;
  for (types= sys_table_types; types->type; types++)
  {
    protocol->prepare_for_resend();
    protocol->store(types->type, system_charset_info);
    const char *option_name= show_comp_option_name[(int) *types->value];

    if (*types->value == SHOW_OPTION_YES &&
	!my_strcasecmp(system_charset_info, default_type_name, types->type))
      option_name= "DEFAULT";
    protocol->store(option_name, system_charset_info);
    protocol->store(types->comment, system_charset_info);
    if (protocol->write())
      DBUG_RETURN(-1);
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


/***************************************************************************
 List all privileges supported
***************************************************************************/

struct show_privileges_st {
  const char *privilege;
  const char *context;
  const char *comment;
};

static struct show_privileges_st sys_privileges[]=
{
  {"Alter", "Tables",  "To alter the table"},
  {"Create", "Databases,Tables,Indexes",  "To create new databases and tables"},
  {"Create temporary tables","Databases","To use CREATE TEMPORARY TABLE"},
  {"Create view", "Tables",  "To create new views"},
  {"Delete", "Tables",  "To delete existing rows"},
  {"Drop", "Databases,Tables", "To drop databases, tables, and views"},
  {"File", "File access on server",   "To read and write files on the server"},
  {"Grant option",  "Databases,Tables", "To give to other users those privileges you possess"},
  {"Index", "Tables",  "To create or drop indexes"},
  {"Insert", "Tables",  "To insert data into tables"},
  {"Lock tables","Databases","To use LOCK TABLES (together with SELECT privilege)"},
  {"Process", "Server Admin", "To view the plain text of currently executing queries"},
  {"References", "Databases,Tables", "To have references on tables"},
  {"Reload", "Server Admin", "To reload or refresh tables, logs and privileges"},
  {"Replication client","Server Admin","To ask where the slave or master servers are"},
  {"Replication slave","Server Admin","To read binary log events from the master"},
  {"Select", "Tables",  "To retrieve rows from table"},
  {"Show databases","Server Admin","To see all databases with SHOW DATABASES"},
  {"Show view","Tables","To see views with SHOW CREATE VIEW"},
  {"Shutdown","Server Admin", "To shut down the server"},
  {"Super","Server Admin","To use KILL thread, SET GLOBAL, CHANGE MASTER, etc."},
  {"Update", "Tables",  "To update existing rows"},
  {"Usage","Server Admin","No privileges - allow connect only"},
  {NullS, NullS, NullS}
};

int mysqld_show_privileges(THD *thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_privileges");

  field_list.push_back(new Item_empty_string("Privilege",10));
  field_list.push_back(new Item_empty_string("Context",15));
  field_list.push_back(new Item_empty_string("Comment",NAME_LEN));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  show_privileges_st *privilege= sys_privileges;
  for (privilege= sys_privileges; privilege->privilege ; privilege++)
  {
    protocol->prepare_for_resend();
    protocol->store(privilege->privilege, system_charset_info);
    protocol->store(privilege->context, system_charset_info);
    protocol->store(privilege->comment, system_charset_info);
    if (protocol->write())
      DBUG_RETURN(-1);
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


/***************************************************************************
  List all column types
***************************************************************************/

struct show_column_type_st
{
  const char *type;
  uint size;
  const char *min_value;
  const char *max_value;
  uint precision;
  uint scale;
  const char *nullable;
  const char *auto_increment;
  const char *unsigned_attr;
  const char *zerofill;
  const char *searchable;
  const char *case_sensitivity;
  const char *default_value;
  const char *comment;
};

/* TODO: Add remaning types */

static struct show_column_type_st sys_column_types[]=
{
  {"tinyint",
    1,  "-128",  "127",  0,  0,  "YES",  "YES",
    "NO",   "YES", "YES",  "NO",  "NULL,0",
    "A very small integer"},
  {"tinyint unsigned",
    1,  "0"   ,  "255",  0,  0,  "YES",  "YES",
    "YES",  "YES",  "YES",  "NO",  "NULL,0",
    "A very small integer"},
};

int mysqld_show_column_types(THD *thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_column_types");

  field_list.push_back(new Item_empty_string("Type",30));
  field_list.push_back(new Item_int("Size",(longlong) 1,21));
  field_list.push_back(new Item_empty_string("Min_Value",20));
  field_list.push_back(new Item_empty_string("Max_Value",20));
  field_list.push_back(new Item_return_int("Prec", 4, MYSQL_TYPE_SHORT));
  field_list.push_back(new Item_return_int("Scale", 4, MYSQL_TYPE_SHORT));
  field_list.push_back(new Item_empty_string("Nullable",4));
  field_list.push_back(new Item_empty_string("Auto_Increment",4));
  field_list.push_back(new Item_empty_string("Unsigned",4));
  field_list.push_back(new Item_empty_string("Zerofill",4));
  field_list.push_back(new Item_empty_string("Searchable",4));
  field_list.push_back(new Item_empty_string("Case_Sensitive",4));
  field_list.push_back(new Item_empty_string("Default",NAME_LEN));
  field_list.push_back(new Item_empty_string("Comment",NAME_LEN));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  /* TODO: Change the loop to not use 'i' */
  for (uint i=0; i < sizeof(sys_column_types)/sizeof(sys_column_types[0]); i++)
  {
    protocol->prepare_for_resend();
    protocol->store(sys_column_types[i].type, system_charset_info);
    protocol->store((ulonglong) sys_column_types[i].size);
    protocol->store(sys_column_types[i].min_value, system_charset_info);
    protocol->store(sys_column_types[i].max_value, system_charset_info);
    protocol->store_short((longlong) sys_column_types[i].precision);
    protocol->store_short((longlong) sys_column_types[i].scale);
    protocol->store(sys_column_types[i].nullable, system_charset_info);
    protocol->store(sys_column_types[i].auto_increment, system_charset_info);
    protocol->store(sys_column_types[i].unsigned_attr, system_charset_info);
    protocol->store(sys_column_types[i].zerofill, system_charset_info);
    protocol->store(sys_column_types[i].searchable, system_charset_info);
    protocol->store(sys_column_types[i].case_sensitivity, system_charset_info);
    protocol->store(sys_column_types[i].default_value, system_charset_info);
    protocol->store(sys_column_types[i].comment, system_charset_info);
    if (protocol->write())
      DBUG_RETURN(-1);
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


int
mysql_find_files(THD *thd,List<char> *files, const char *db,const char *path,
                 const char *wild, bool dir)
{
  uint i;
  char *ext;
  MY_DIR *dirp;
  FILEINFO *file;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint col_access=thd->col_access;
#endif
  TABLE_LIST table_list;
  DBUG_ENTER("mysql_find_files");

  if (wild && !wild[0])
    wild=0;

  bzero((char*) &table_list,sizeof(table_list));

  if (!(dirp = my_dir(path,MYF(MY_WME | (dir ? MY_WANT_STAT : 0)))))
    DBUG_RETURN(-1);

  for (i=0 ; i < (uint) dirp->number_off_files  ; i++)
  {
    file=dirp->dir_entry+i;
    if (dir)
    {                                           /* Return databases */
#ifdef USE_SYMDIR
      char *ext;
      char buff[FN_REFLEN];
      if (my_use_symdir && !strcmp(ext=fn_ext(file->name), ".sym"))
      {
	/* Only show the sym file if it points to a directory */
	char *end;
        *ext=0;                                 /* Remove extension */
	unpack_dirname(buff, file->name);
	end= strend(buff);
	if (end != buff && end[-1] == FN_LIBCHAR)
	  end[-1]= 0;				// Remove end FN_LIBCHAR
        if (!my_stat(buff, file->mystat, MYF(0)))
               continue;
       }
#endif
        if (file->name[0] == '.' || !MY_S_ISDIR(file->mystat->st_mode) ||
            (wild && wild_compare(file->name,wild,0)))
          continue;
    }
    else
    {
        // Return only .frm files which aren't temp files.
      if (my_strcasecmp(system_charset_info, ext=fn_ext(file->name),reg_ext) ||
          is_prefix(file->name,tmp_file_prefix))
        continue;
      *ext=0;
      if (wild)
      {
	if (lower_case_table_names)
	{
	  if (wild_case_compare(files_charset_info, file->name, wild))
	    continue;
	}
	else if (wild_compare(file->name,wild,0))
	  continue;
      }
    }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /* Don't show tables where we don't have any privileges */
    if (db && !(col_access & TABLE_ACLS))
    {
      table_list.db= (char*) db;
      table_list.real_name=file->name;
      table_list.grant.privilege=col_access;
      if (check_grant(thd, TABLE_ACLS, &table_list, 1, UINT_MAX, 1))
        continue;
    }
#endif
    if (files->push_back(thd->strdup(file->name)))
    {
      my_dirend(dirp);
      DBUG_RETURN(-1);
    }
  }
  DBUG_PRINT("info",("found: %d files", files->elements));
  my_dirend(dirp);

  VOID(ha_find_files(thd,db,path,wild,dir,files));

  DBUG_RETURN(0);
}


/***************************************************************************
** List all columns in a table_list->real_name
***************************************************************************/

int
mysqld_show_fields(THD *thd, TABLE_LIST *table_list,const char *wild,
		   bool verbose)
{
  TABLE *table;
  handler *file;
  char tmp[MAX_FIELD_WIDTH];
  char tmp1[MAX_FIELD_WIDTH];
  Item *item;
  Protocol *protocol= thd->protocol;
  int res;
  DBUG_ENTER("mysqld_show_fields");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
                      table_list->real_name));

  table_list->lock_type= TL_UNLOCK;
  if ((res= open_and_lock_tables(thd, table_list)))
  {
    if (res < 0)
      send_error(thd);
    DBUG_RETURN(1);
  }
  table= table_list->table;
  file=table->file;
  file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  (void) get_table_grant(thd, table_list);
#endif
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Field",NAME_LEN));
  field_list.push_back(new Item_empty_string("Type",40));
  if (verbose)
    field_list.push_back(new Item_empty_string("Collation",40));
  field_list.push_back(new Item_empty_string("Null",1));
  field_list.push_back(new Item_empty_string("Key",3));
  field_list.push_back(item=new Item_empty_string("Default",NAME_LEN));
  item->maybe_null=1;
  field_list.push_back(new Item_empty_string("Extra",20));
  if (verbose)
  {
    field_list.push_back(new Item_empty_string("Privileges",80));
    field_list.push_back(new Item_empty_string("Comment",255));
  }
        // Send first number of fields and records
  if (protocol->send_records_num(&field_list, (ulonglong)file->records) ||
      protocol->send_fields(&field_list, Protocol::SEND_EOF))
    DBUG_RETURN(1);
  restore_record(table,default_values);      // Get empty record

  Field **ptr,*field;
  for (ptr=table->field; (field= *ptr) ; ptr++)
  {
    if (!wild || !wild[0] || 
        !wild_case_compare(system_charset_info, field->field_name,wild))
    {
      {
        byte *pos;
        uint flags=field->flags;
        String type(tmp,sizeof(tmp), system_charset_info);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
        uint col_access;
#endif
	protocol->prepare_for_resend();
        protocol->store(field->field_name, system_charset_info);
        field->sql_type(type);
        protocol->store(type.ptr(), type.length(), system_charset_info);
	if (verbose)
	  protocol->store(field->has_charset() ? field->charset()->name : "NULL",
			system_charset_info);
        /*
          Even if TIMESTAMP field can't contain NULL as its value it
          will accept NULL if you will try to insert such value and will
          convert NULL value to current TIMESTAMP. So YES here means
          that NULL is allowed for assignment (but may be won't be
          returned).
        */
        pos=(byte*) ((flags & NOT_NULL_FLAG) &&
                     field->type() != FIELD_TYPE_TIMESTAMP ?
                     "" : "YES");
        protocol->store((const char*) pos, system_charset_info);
        pos=(byte*) ((field->flags & PRI_KEY_FLAG) ? "PRI" :
                     (field->flags & UNIQUE_KEY_FLAG) ? "UNI" :
                     (field->flags & MULTIPLE_KEY_FLAG) ? "MUL":"");
        protocol->store((char*) pos, system_charset_info);

        if (table->timestamp_field == field &&
            field->unireg_check != Field::TIMESTAMP_UN_FIELD)
        {
          /*
            We have NOW() as default value but we use CURRENT_TIMESTAMP form
            because it is more SQL standard compatible
          */
          protocol->store("CURRENT_TIMESTAMP", system_charset_info);
        }
        else if (field->unireg_check != Field::NEXT_NUMBER &&
                 !field->is_null() &&
                 !(field->flags & NO_DEFAULT_VALUE_FLAG))
        {                                               // Not null by default
          /*
            Note: we have to convert the default value into
            system_charset_info before sending.
            This is necessary for "SET NAMES binary":
            If the client character set is binary, we want to
            send metadata in UTF8 rather than in the column's
            character set.
            This conversion also makes "SHOW COLUMNS" and
            "SHOW CREATE TABLE" output consistent. Without
            this conversion the default values were displayed
            differently.
          */
          String def(tmp1,sizeof(tmp1), system_charset_info);
          type.set(tmp, sizeof(tmp), field->charset());
          field->val_str(&type);
          uint dummy_errors;
          def.copy(type.ptr(), type.length(), type.charset(), 
                   system_charset_info, &dummy_errors);
          protocol->store(def.ptr(), def.length(), def.charset());
        }
        else if (field->unireg_check == Field::NEXT_NUMBER ||
                 field->maybe_null())
          protocol->store_null();                       // Null as default
        else
          protocol->store("",0, system_charset_info);	// empty string

        char *end=tmp;
        if (field->unireg_check == Field::NEXT_NUMBER)
          end=strmov(tmp,"auto_increment");
        protocol->store(tmp,(uint) (end-tmp), system_charset_info);

	if (verbose)
	{
	  /* Add grant options & comments */
	  end=tmp;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
          col_access= get_column_grant(thd, &table_list->grant,
                                       table_list->db,
                                       table_list->real_name,
                                       field->field_name) & COL_ACLS;
	  for (uint bitnr=0; col_access ; col_access>>=1,bitnr++)
	  {
	    if (col_access & 1)
	    {
	      *end++=',';
	      end=strmov(end,grant_types.type_names[bitnr]);
	    }
	  }
#else
	  end=strmov(end,"");
#endif
	  protocol->store(tmp+1,end == tmp ? 0 : (uint) (end-tmp-1),
			  system_charset_info);
	  protocol->store(field->comment.str, field->comment.length,
			  system_charset_info);
	}
        if (protocol->write())
          DBUG_RETURN(1);
      }
    }
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


int
mysqld_show_create(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  Protocol *protocol= thd->protocol;
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  int res;
  DBUG_ENTER("mysqld_show_create");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
                      table_list->real_name));

  /* Only one table for now, but VIEW can involve several tables */
  if ((res= open_and_lock_tables(thd, table_list)))
  {
    if (res < 0)
      send_error(thd);
    DBUG_RETURN(1);
  }
  /* TODO: add environment variables show when it become possible */
  if (thd->lex->only_view && !table_list->view)
  {
    my_error(ER_WRONG_OBJECT, MYF(0), table_list->db,
             table_list->real_name, "VIEW");
    DBUG_RETURN(-1);
  }

  table= table_list->table;

  if ((table_list->view ?
       view_store_create_info(thd, table_list, &buffer) :
       store_create_info(thd, table, &buffer)))
    DBUG_RETURN(-1);

  List<Item> field_list;
  if (table_list->view)
  {
    field_list.push_back(new Item_empty_string("View",NAME_LEN));
    field_list.push_back(new Item_empty_string("Create View",
                                               max(buffer.length(),1024)));
  }
  else
  {
    field_list.push_back(new Item_empty_string("Table",NAME_LEN));
    // 1024 is for not to confuse old clients
    field_list.push_back(new Item_empty_string("Create Table",
                                               max(buffer.length(),1024)));
  }

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);
  protocol->prepare_for_resend();
  buffer.length(0);
  if (table_list->view)
  {
    protocol->store(table_list->view_name.str, system_charset_info);
    if (view_store_create_info(thd, table_list, &buffer))
      DBUG_RETURN(-1);
  }
  else
  {
    protocol->store(table->table_name, system_charset_info);
    if (store_create_info(thd, table, &buffer))
      DBUG_RETURN(-1);
  }
  protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->write())
    DBUG_RETURN(1);
  send_eof(thd);
  DBUG_RETURN(0);
}

int mysqld_show_create_db(THD *thd, char *dbname,
			  HA_CREATE_INFO *create_info)
{
  int length;
  char	path[FN_REFLEN];
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint db_access;
#endif
  bool found_libchar;
  HA_CREATE_INFO create;
  uint create_options = create_info ? create_info->options : 0;
  Protocol *protocol=thd->protocol;
  DBUG_ENTER("mysql_show_create_db");

  if (check_db_name(dbname))
  {
    net_printf(thd,ER_WRONG_DB_NAME, dbname);
    DBUG_RETURN(1);
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (test_all_bits(thd->master_access,DB_ACLS))
    db_access=DB_ACLS;
  else
    db_access= (acl_get(thd->host,thd->ip, thd->priv_user,dbname,0) |
		thd->master_access);
  if (!(db_access & DB_ACLS) && (!grant_option || check_grant_db(thd,dbname)))
  {
    net_printf(thd,ER_DBACCESS_DENIED_ERROR,
	       thd->priv_user, thd->host_or_ip, dbname);
    mysql_log.write(thd,COM_INIT_DB,ER(ER_DBACCESS_DENIED_ERROR),
		    thd->priv_user, thd->host_or_ip, dbname);
    DBUG_RETURN(1);
  }
#endif

  (void) sprintf(path,"%s/%s",mysql_data_home, dbname);
  length=unpack_dirname(path,path);		// Convert if not unix
  found_libchar= 0;
  if (length && path[length-1] == FN_LIBCHAR)
  {
    found_libchar= 1;
    path[length-1]=0;				// remove ending '\'
  }
  if (access(path,F_OK))
  {
    net_printf(thd,ER_BAD_DB_ERROR,dbname);
    DBUG_RETURN(1);
  }
  if (found_libchar)
    path[length-1]= FN_LIBCHAR;
  strmov(path+length, MY_DB_OPT_FILE);
  load_db_opt(thd, path, &create);

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Database",NAME_LEN));
  field_list.push_back(new Item_empty_string("Create Database",1024));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  protocol->prepare_for_resend();
  protocol->store(dbname, strlen(dbname), system_charset_info);
  buffer.length(0);
  buffer.append("CREATE DATABASE ", 16);
  if (create_options & HA_LEX_CREATE_IF_NOT_EXISTS)
    buffer.append("/*!32312 IF NOT EXISTS*/ ", 25);
  append_identifier(thd, &buffer, dbname, strlen(dbname));

  if (create.default_table_charset)
  {
    buffer.append(" /*!40100", 9);
    buffer.append(" DEFAULT CHARACTER SET ", 23);
    buffer.append(create.default_table_charset->csname);
    if (!(create.default_table_charset->state & MY_CS_PRIMARY))
    {
      buffer.append(" COLLATE ", 9);
      buffer.append(create.default_table_charset->name);
    }
    buffer.append(" */", 3);
  }
  protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->write())
    DBUG_RETURN(1);
  send_eof(thd);
  DBUG_RETURN(0);
}

int
mysqld_show_logs(THD *thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_logs");

  field_list.push_back(new Item_empty_string("File",FN_REFLEN));
  field_list.push_back(new Item_empty_string("Type",10));
  field_list.push_back(new Item_empty_string("Status",10));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

#ifdef HAVE_BERKELEY_DB
  if ((have_berkeley_db == SHOW_OPTION_YES) && berkeley_show_logs(protocol))
    DBUG_RETURN(-1);
#endif

  send_eof(thd);
  DBUG_RETURN(0);
}


/****************************************************************************
  Return only fields for API mysql_list_fields
  Use "show table wildcard" in mysql instead of this
****************************************************************************/

void
mysqld_list_fields(THD *thd, TABLE_LIST *table_list, const char *wild)
{
  TABLE *table;
  int res;
  DBUG_ENTER("mysqld_list_fields");
  DBUG_PRINT("enter",("table: %s",table_list->real_name));

  table_list->lock_type= TL_UNLOCK;
  if ((res= open_and_lock_tables(thd, table_list)))
  {
    if (res < 0)
      send_error(thd);
    DBUG_VOID_RETURN;
  }
  table= table_list->table;

  List<Item> field_list;

  Field **ptr,*field;
  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    if (!wild || !wild[0] || 
        !wild_case_compare(system_charset_info, field->field_name,wild))
      field_list.push_back(new Item_field(field));
  }
  restore_record(table,default_values);              // Get empty record
  if (thd->protocol->send_fields(&field_list, Protocol::SEND_DEFAULTS |
                                              Protocol::SEND_EOF))
    DBUG_VOID_RETURN;
  thd->protocol->flush();
  DBUG_VOID_RETURN;
}


int
mysqld_dump_create_info(THD *thd, TABLE *table, int fd)
{
  Protocol *protocol= thd->protocol;
  String *packet= protocol->storage_packet();
  DBUG_ENTER("mysqld_dump_create_info");
  DBUG_PRINT("enter",("table: %s",table->real_name));

  protocol->prepare_for_resend();
  if (store_create_info(thd, table, packet))
    DBUG_RETURN(-1);

  if (fd < 0)
  {
    if (protocol->write())
      DBUG_RETURN(-1);
    protocol->flush();
  }
  else
  {
    if (my_write(fd, (const byte*) packet->ptr(), packet->length(),
		 MYF(MY_WME)))
      DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/*
  Go through all character combinations and ensure that sql_lex.cc can
  parse it as an identifier.

  SYNOPSIS
  require_quotes()
  name			attribute name
  name_length		length of name

  RETURN
    #	Pointer to conflicting character
    0	No conflicting character
*/

static const char *require_quotes(const char *name, uint name_length)
{
  uint length;
  const char *end= name + name_length;

  for ( ; name < end ; name++)
  {
    uchar chr= (uchar) *name;
    length= my_mbcharlen(system_charset_info, chr);
    if (length == 1 && !system_charset_info->ident_map[chr])
      return name;
  }
  return 0;
}


void
append_identifier(THD *thd, String *packet, const char *name, uint length)
{
  const char *name_end;
  char quote_char;
  int q= get_quote_char_for_identifier(thd, name, length);

  if (q == EOF)
  {
    packet->append(name, length, system_charset_info);
    return;
  }

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  packet->reserve(length*2 + 2);
  quote_char= (char) q;
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end= name+length ; name < name_end ; name+= length)
  {
    uchar chr= (uchar) *name;
    length= my_mbcharlen(system_charset_info, chr);
    if (length == 1 && chr == (uchar) quote_char)
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(name, length, packet->charset());
  }
  packet->append(&quote_char, 1, system_charset_info);
}


/*
  Get the quote character for displaying an identifier.

  SYNOPSIS
    get_quote_char_for_identifier()
    thd		Thread handler
    name	name to quote
    length	length of name

  IMPLEMENTATION
    If name is a keyword or includes a special character, then force
    quoting.
    Otherwise identifier is quoted only if the option OPTION_QUOTE_SHOW_CREATE
    is set.

  RETURN
    EOF	  No quote character is needed
    #	  Quote character
*/

int get_quote_char_for_identifier(THD *thd, const char *name, uint length)
{
  if (!is_keyword(name,length) &&
      !require_quotes(name, length) &&
      !(thd->options & OPTION_QUOTE_SHOW_CREATE))
    return EOF;
  if (thd->variables.sql_mode & MODE_ANSI_QUOTES)
    return '"';
  return '`';
}


/* Append directory name (if exists) to CREATE INFO */

static void append_directory(THD *thd, String *packet, const char *dir_type,
			     const char *filename)
{
  if (filename && !(thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE))
  {
    uint length= dirname_length(filename);
    packet->append(' ');
    packet->append(dir_type);
    packet->append(" DIRECTORY='", 12);
    packet->append(filename, length);
    packet->append('\'');
  }
}


#define LIST_PROCESS_HOST_LEN 64

static int
store_create_info(THD *thd, TABLE *table, String *packet)
{
  List<Item> field_list;
  char tmp[MAX_FIELD_WIDTH], *for_str, buff[128], *end, *alias;
  String type(tmp, sizeof(tmp), system_charset_info);
  Field **ptr,*field;
  uint primary_key;
  KEY *key_info;
  handler *file= table->file;
  HA_CREATE_INFO create_info;
  my_bool foreign_db_mode=    (thd->variables.sql_mode & (MODE_POSTGRESQL |
							  MODE_ORACLE |
							  MODE_MSSQL |
							  MODE_DB2 |
							  MODE_MAXDB |
							  MODE_ANSI)) != 0;
  my_bool limited_mysql_mode= (thd->variables.sql_mode &
			       (MODE_NO_FIELD_OPTIONS | MODE_MYSQL323 |
				MODE_MYSQL40)) != 0;

  DBUG_ENTER("store_create_info");
  DBUG_PRINT("enter",("table: %s",table->real_name));

  restore_record(table,default_values); // Get empty record

  if (table->tmp_table)
    packet->append("CREATE TEMPORARY TABLE ", 23);
  else
    packet->append("CREATE TABLE ", 13);
  alias= (lower_case_table_names == 2 ? table->table_name :
	  table->real_name);
  append_identifier(thd, packet, alias, strlen(alias));
  packet->append(" (\n", 3);

  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    bool has_default;
    bool has_now_default;
    uint flags = field->flags;

    if (ptr != table->field)
      packet->append(",\n", 2);

    packet->append("  ", 2);
    append_identifier(thd,packet,field->field_name, strlen(field->field_name));
    packet->append(' ');
    // check for surprises from the previous call to Field::sql_type()
    if (type.ptr() != tmp)
      type.set(tmp, sizeof(tmp), system_charset_info);
    else
      type.set_charset(system_charset_info);

    field->sql_type(type);
    packet->append(type.ptr(), type.length(), system_charset_info);

    if (field->has_charset() && !limited_mysql_mode && !foreign_db_mode)
    {
      if (field->charset() != table->table_charset)
      {
	packet->append(" character set ", 15);
	packet->append(field->charset()->csname);
      }
      /* 
	For string types dump collation name only if 
	collation is not primary for the given charset
      */
      if (!(field->charset()->state & MY_CS_PRIMARY))
      {
	packet->append(" collate ", 9);
	packet->append(field->charset()->name);
      }
    }

    if (flags & NOT_NULL_FLAG)
      packet->append(" NOT NULL", 9);
    else if (field->type() == FIELD_TYPE_TIMESTAMP)
    {
      /*
        TIMESTAMP field require explicit NULL flag, because unlike
        all other fields they are treated as NOT NULL by default.
      */
      packet->append(" NULL", 5);
    }

    /* 
      Again we are using CURRENT_TIMESTAMP instead of NOW because it is
      more standard 
    */
    has_now_default= table->timestamp_field == field && 
                     field->unireg_check != Field::TIMESTAMP_UN_FIELD;
    
    has_default= (field->type() != FIELD_TYPE_BLOB &&
                  !(field->flags & NO_DEFAULT_VALUE_FLAG) &&
		  field->unireg_check != Field::NEXT_NUMBER &&
                  !((foreign_db_mode || limited_mysql_mode) &&
                    has_now_default));

    if (has_default)
    {
      packet->append(" default ", 9);
      if (has_now_default)
        packet->append("CURRENT_TIMESTAMP",17);
      else if (!field->is_null())
      {                                             // Not null by default
        type.set(tmp, sizeof(tmp), field->charset());
        field->val_str(&type);
	if (type.length())
	{
	  String def_val;
          uint dummy_errors;
	  /* convert to system_charset_info == utf8 */
	  def_val.copy(type.ptr(), type.length(), field->charset(),
		       system_charset_info, &dummy_errors);
          append_unescaped(packet, def_val.ptr(), def_val.length());
	}
        else
	  packet->append("''",2);
      }
      else if (field->maybe_null())
        packet->append("NULL", 4);                    // Null as default
      else
        packet->append(tmp);
    }

    if (!foreign_db_mode && !limited_mysql_mode &&
        table->timestamp_field == field && 
        field->unireg_check != Field::TIMESTAMP_DN_FIELD)
      packet->append(" on update CURRENT_TIMESTAMP",28);

    if (field->unireg_check == Field::NEXT_NUMBER && !foreign_db_mode)
      packet->append(" auto_increment", 15 );

    if (field->comment.length)
    {
      packet->append(" COMMENT ",9);
      append_unescaped(packet, field->comment.str, field->comment.length);
    }
  }

  key_info= table->key_info;
  file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK | HA_STATUS_TIME);
  bzero((char*) &create_info, sizeof(create_info));
  file->update_create_info(&create_info);
  primary_key= table->primary_key;

  for (uint i=0 ; i < table->keys ; i++,key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    bool found_primary=0;
    packet->append(",\n  ", 4);

    if (i == primary_key && !strcmp(key_info->name, primary_key_name))
    {
      found_primary=1;
      packet->append("PRIMARY ", 8);
    }
    else if (key_info->flags & HA_NOSAME)
      packet->append("UNIQUE ", 7);
    else if (key_info->flags & HA_FULLTEXT)
      packet->append("FULLTEXT ", 9);
    else if (key_info->flags & HA_SPATIAL)
      packet->append("SPATIAL ", 8);
    packet->append("KEY ", 4);

    if (!found_primary)
     append_identifier(thd, packet, key_info->name, strlen(key_info->name));

    if (!(thd->variables.sql_mode & MODE_NO_KEY_OPTIONS) &&
	!limited_mysql_mode && !foreign_db_mode)
    {
      if (table->db_type == DB_TYPE_HEAP &&
	  key_info->algorithm == HA_KEY_ALG_BTREE)
	packet->append(" TYPE BTREE", 11);
      
      // +BAR: send USING only in non-default case: non-spatial rtree
      if ((key_info->algorithm == HA_KEY_ALG_RTREE) &&
	  !(key_info->flags & HA_SPATIAL))
	packet->append(" TYPE RTREE", 11);
    }
    packet->append(" (", 2);

    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (j)
        packet->append(',');

      if (key_part->field)
        append_identifier(thd,packet,key_part->field->field_name,
			  strlen(key_part->field->field_name));
      if (!key_part->field ||
          (key_part->length !=
           table->field[key_part->fieldnr-1]->key_length() &&
           !(key_info->flags & HA_FULLTEXT)))
      {
        buff[0] = '(';
        char* end=int10_to_str((long) key_part->length / 
			       key_part->field->charset()->mbmaxlen,
			       buff + 1,10);
        *end++ = ')';
        packet->append(buff,(uint) (end-buff));
      }
    }
    packet->append(')');
  }

  /*
    Get possible foreign key definitions stored in InnoDB and append them
    to the CREATE TABLE statement
  */

  if ((for_str= file->get_foreign_key_create_info()))
  {
    packet->append(for_str, strlen(for_str));
    file->free_foreign_key_create_info(for_str);
  }

  packet->append("\n)", 2);
  if (!(thd->variables.sql_mode & MODE_NO_TABLE_OPTIONS) && !foreign_db_mode)
  {
    if (thd->variables.sql_mode & (MODE_MYSQL323 | MODE_MYSQL40))
      packet->append(" TYPE=", 6);
    else
      packet->append(" ENGINE=", 8);
    packet->append(file->table_type());
    
    if (table->table_charset &&
	!(thd->variables.sql_mode & MODE_MYSQL323) &&
	!(thd->variables.sql_mode & MODE_MYSQL40))
    {
      packet->append(" DEFAULT CHARSET=", 17);
      packet->append(table->table_charset->csname);
      if (!(table->table_charset->state & MY_CS_PRIMARY))
      {
	packet->append(" COLLATE=", 9);
	packet->append(table->table_charset->name);
      }
    }

    if (table->min_rows)
    {
      packet->append(" MIN_ROWS=", 10);
      end= longlong10_to_str(table->min_rows, buff, 10);
      packet->append(buff, (uint) (end- buff));
    }

    if (table->max_rows)
    {
      packet->append(" MAX_ROWS=", 10);
      end= longlong10_to_str(table->max_rows, buff, 10);
      packet->append(buff, (uint) (end - buff));
    }

    if (table->avg_row_length)
    {
      packet->append(" AVG_ROW_LENGTH=", 16);
      end= longlong10_to_str(table->avg_row_length, buff,10);
      packet->append(buff, (uint) (end - buff));
    }

    if (table->db_create_options & HA_OPTION_PACK_KEYS)
      packet->append(" PACK_KEYS=1", 12);
    if (table->db_create_options & HA_OPTION_NO_PACK_KEYS)
      packet->append(" PACK_KEYS=0", 12);
    if (table->db_create_options & HA_OPTION_CHECKSUM)
      packet->append(" CHECKSUM=1", 11);
    if (table->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      packet->append(" DELAY_KEY_WRITE=1",18);
    if (table->row_type != ROW_TYPE_DEFAULT)
    {
      packet->append(" ROW_FORMAT=",12);
      packet->append(ha_row_type[(uint) table->row_type]);
    }
    table->file->append_create_info(packet);
    if (table->comment && table->comment[0])
    {
      packet->append(" COMMENT=", 9);
      append_unescaped(packet, table->comment, strlen(table->comment));
    }
    if (file->raid_type)
    {
      uint length;
      length= my_snprintf(buff,sizeof(buff),
			  " RAID_TYPE=%s RAID_CHUNKS=%d RAID_CHUNKSIZE=%ld",
			  my_raid_type(file->raid_type), file->raid_chunks,
			  file->raid_chunksize/RAID_BLOCK_SIZE);
      packet->append(buff, length);
    }
    append_directory(thd, packet, "DATA",  create_info.data_file_name);
    append_directory(thd, packet, "INDEX", create_info.index_file_name);
  }
  DBUG_RETURN(0);
}


static int
view_store_create_info(THD *thd, TABLE_LIST *table, String *buff)
{
  my_bool foreign_db_mode= (thd->variables.sql_mode & (MODE_POSTGRESQL |
                                                       MODE_ORACLE |
                                                       MODE_MSSQL |
                                                       MODE_DB2 |
                                                       MODE_MAXDB |
                                                       MODE_ANSI)) != 0;
  buff->append("CREATE ", 7);
  if (!foreign_db_mode)
  {
    buff->append("ALGORITHM=", 10);
    switch(table->algorithm)
    {
    case VIEW_ALGORITHM_UNDEFINED:
      buff->append("UNDEFINED ", 10);
      break;
    case VIEW_ALGORITHM_TMPTABLE:
      buff->append("TEMPTABLE ", 10);
      break;
    case VIEW_ALGORITHM_MERGE:
      buff->append("MERGE ", 6);
      break;
    default:
	DBUG_ASSERT(0); // never should happen
    }
  }
  buff->append("VIEW ", 5);
  append_identifier(thd, buff, table->view_db.str, table->view_db.length);
  buff->append('.');
  append_identifier(thd, buff, table->view_name.str, table->view_name.length);
  buff->append(" AS ", 4);
  buff->append(table->query.str, table->query.length);
  if (table->with_check != VIEW_CHECK_NONE)
  {
    if (table->with_check == VIEW_CHECK_LOCAL)
      buff->append(" WITH LOCAL CHECK OPTION", 24);
    else
      buff->append(" WITH CASCADED CHECK OPTION", 27);
  }
  return 0;
}


/****************************************************************************
  Return info about all processes
  returns for each thread: thread id, user, host, db, command, info
****************************************************************************/

class thread_info :public ilink {
public:
  static void *operator new(size_t size) {return (void*) sql_alloc((uint) size); }
  static void operator delete(void *ptr __attribute__((unused)),
                              size_t size __attribute__((unused))) {} /*lint -e715 */

  ulong thread_id;
  time_t start_time;
  uint   command;
  const char *user,*host,*db,*proc_info,*state_info;
  char *query;
};

#ifdef __GNUC__
template class I_List<thread_info>;
#endif

void mysqld_list_processes(THD *thd,const char *user, bool verbose)
{
  Item *field;
  List<Item> field_list;
  I_List<thread_info> thread_infos;
  ulong max_query_length= (verbose ? thd->variables.max_allowed_packet :
			   PROCESS_LIST_WIDTH);
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_list_processes");

  field_list.push_back(new Item_int("Id",0,11));
  field_list.push_back(new Item_empty_string("User",16));
  field_list.push_back(new Item_empty_string("Host",LIST_PROCESS_HOST_LEN));
  field_list.push_back(field=new Item_empty_string("db",NAME_LEN));
  field->maybe_null=1;
  field_list.push_back(new Item_empty_string("Command",16));
  field_list.push_back(new Item_return_int("Time",7, FIELD_TYPE_LONG));
  field_list.push_back(field=new Item_empty_string("State",30));
  field->maybe_null=1;
  field_list.push_back(field=new Item_empty_string("Info",max_query_length));
  field->maybe_null=1;
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_VOID_RETURN;

  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  if (!thd->killed)
  {
    I_List_iterator<THD> it(threads);
    THD *tmp;
    while ((tmp=it++))
    {
      struct st_my_thread_var *mysys_var;
      if ((tmp->vio_ok() || tmp->system_thread) &&
          (!user || (tmp->user && !strcmp(tmp->user,user))))
      {
        thread_info *thd_info=new thread_info;

        thd_info->thread_id=tmp->thread_id;
        thd_info->user=thd->strdup(tmp->user ? tmp->user :
				   (tmp->system_thread ?
				    "system user" : "unauthenticated user"));
	if (tmp->peer_port && (tmp->host || tmp->ip) && thd->host_or_ip[0])
	{
	  if ((thd_info->host= thd->alloc(LIST_PROCESS_HOST_LEN+1)))
	    my_snprintf((char *) thd_info->host, LIST_PROCESS_HOST_LEN,
			"%s:%u", tmp->host_or_ip, tmp->peer_port);
	}
	else
	  thd_info->host= thd->strdup(tmp->host_or_ip);
        if ((thd_info->db=tmp->db))             // Safe test
          thd_info->db=thd->strdup(thd_info->db);
        thd_info->command=(int) tmp->command;
        if ((mysys_var= tmp->mysys_var))
          pthread_mutex_lock(&mysys_var->mutex);
        thd_info->proc_info= (char*) (tmp->killed == THD::KILL_CONNECTION? "Killed" : 0);
#ifndef EMBEDDED_LIBRARY
        thd_info->state_info= (char*) (tmp->locked ? "Locked" :
                                       tmp->net.reading_or_writing ?
                                       (tmp->net.reading_or_writing == 2 ?
                                        "Writing to net" :
                                        thd_info->command == COM_SLEEP ? "" :
                                        "Reading from net") :
                                       tmp->proc_info ? tmp->proc_info :
                                       tmp->mysys_var &&
                                       tmp->mysys_var->current_cond ?
                                       "Waiting on cond" : NullS);
#else
        thd_info->state_info= (char*)"Writing to net";
#endif
        if (mysys_var)
          pthread_mutex_unlock(&mysys_var->mutex);

#if !defined(DONT_USE_THR_ALARM) && ! defined(SCO)
        if (pthread_kill(tmp->real_id,0))
          tmp->proc_info="*** DEAD ***";        // This shouldn't happen
#endif
#ifdef EXTRA_DEBUG
        thd_info->start_time= tmp->time_after_lock;
#else
        thd_info->start_time= tmp->start_time;
#endif
        thd_info->query=0;
        if (tmp->query)
        {
	  /* 
            query_length is always set to 0 when we set query = NULL; see
	    the comment in sql_class.h why this prevents crashes in possible
            races with query_length
          */
          uint length= min(max_query_length, tmp->query_length);
          thd_info->query=(char*) thd->strmake(tmp->query,length);
        }
        thread_infos.append(thd_info);
      }
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  thread_info *thd_info;
  time_t now= time(0);
  while ((thd_info=thread_infos.get()))
  {
    protocol->prepare_for_resend();
    protocol->store((ulonglong) thd_info->thread_id);
    protocol->store(thd_info->user, system_charset_info);
    protocol->store(thd_info->host, system_charset_info);
    protocol->store(thd_info->db, system_charset_info);
    if (thd_info->proc_info)
      protocol->store(thd_info->proc_info, system_charset_info);
    else
      protocol->store(command_name[thd_info->command], system_charset_info);
    if (thd_info->start_time)
      protocol->store((uint32) (now - thd_info->start_time));
    else
      protocol->store_null();
    protocol->store(thd_info->state_info, system_charset_info);
    protocol->store(thd_info->query, system_charset_info);
    if (protocol->write())
      break; /* purecov: inspected */
  }
  send_eof(thd);
  DBUG_VOID_RETURN;
}

/*****************************************************************************
  Status functions
*****************************************************************************/

int mysqld_show(THD *thd, const char *wild, show_var_st *variables,
		enum enum_var_type value_type,
		pthread_mutex_t *mutex,
		struct system_status_var *status_var)
{
  char buff[1024];
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  LEX_STRING null_lex_str;
  DBUG_ENTER("mysqld_show");

  field_list.push_back(new Item_empty_string("Variable_name",30));
  field_list.push_back(new Item_empty_string("Value",256));
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1); /* purecov: inspected */
  null_lex_str.str= 0;				// For sys_var->value_ptr()
  null_lex_str.length= 0;

  pthread_mutex_lock(mutex);
  for (; variables->name; variables++)
  {
    if (!(wild && wild[0] && wild_case_compare(system_charset_info,
					       variables->name,wild)))
    {
      protocol->prepare_for_resend();
      protocol->store(variables->name, system_charset_info);
      SHOW_TYPE show_type=variables->type;
      char *value=variables->value;
      const char *pos, *end;
      long nr;

      if (show_type == SHOW_SYS)
      {
	show_type= ((sys_var*) value)->type();
	value=     (char*) ((sys_var*) value)->value_ptr(thd, value_type,
							 &null_lex_str);
      }

      pos= end= buff;
      switch (show_type) {
      case SHOW_LONG_STATUS:
      case SHOW_LONG_CONST_STATUS:
	value= ((char *) status_var + (ulong) value);
	  /* fall through */
      case SHOW_LONG:
      case SHOW_LONG_CONST:
	end= int10_to_str(*(long*) value, buff, 10);
        break;
      case SHOW_LONGLONG:
	end= longlong10_to_str(*(longlong*) value, buff, 10);
	break;
      case SHOW_HA_ROWS:
        end= longlong10_to_str((longlong) *(ha_rows*) value, buff, 10);
        break;
      case SHOW_BOOL:
	end= strmov(buff, *(bool*) value ? "ON" : "OFF");
        break;
      case SHOW_MY_BOOL:
	end= strmov(buff, *(my_bool*) value ? "ON" : "OFF");
        break;
      case SHOW_INT_CONST:
      case SHOW_INT:
	end= int10_to_str((long) *(uint32*) value, buff, 10);
        break;
      case SHOW_HAVE:
      {
	SHOW_COMP_OPTION tmp= *(SHOW_COMP_OPTION*) value;
	pos= show_comp_option_name[(int) tmp];
	end= strend(pos);
        break;
      }
      case SHOW_CHAR:
      {
        if (!(pos= value))
          pos= "";
        end= strend(pos);
        break;
       }
      case SHOW_STARTTIME:
	nr= (long) (thd->query_start() - start_time);
	end= int10_to_str(nr, buff, 10);
        break;
      case SHOW_QUESTION:
	end= int10_to_str((long) thd->query_id, buff, 10);
        break;
#ifdef HAVE_REPLICATION
      case SHOW_RPL_STATUS:
	end= strmov(buff, rpl_status_type[(int)rpl_status]);
	break;
      case SHOW_SLAVE_RUNNING:
      {
	pthread_mutex_lock(&LOCK_active_mi);
	end= strmov(buff, (active_mi->slave_running &&
			   active_mi->rli.slave_running) ? "ON" : "OFF");
	pthread_mutex_unlock(&LOCK_active_mi);
	break;
      }
#endif /* HAVE_REPLICATION */
      case SHOW_OPENTABLES:
	end= int10_to_str((long) cached_tables(), buff, 10);
        break;
      case SHOW_CHAR_PTR:
      {
        if (!(pos= *(char**) value))
          pos= "";
        end= strend(pos);
        break;
      }
      case SHOW_DOUBLE:
      {
        end= buff + sprintf(buff, "%f", *(double*) value);
        break;
      }
#ifdef HAVE_OPENSSL
	/* First group - functions relying on CTX */
      case SHOW_SSL_CTX_SESS_ACCEPT:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_accept(ssl_acceptor_fd->
						      ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_ACCEPT_GOOD:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_accept_good(ssl_acceptor_fd->
							   ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_CONNECT_GOOD:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_connect_good(ssl_acceptor_fd->
							    ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_ACCEPT_RENEGOTIATE:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_accept_renegotiate(ssl_acceptor_fd->ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_CONNECT_RENEGOTIATE:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_connect_renegotiate(ssl_acceptor_fd-> ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_CB_HITS:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_cb_hits(ssl_acceptor_fd->
						       ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_HITS:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_hits(ssl_acceptor_fd->
						    ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_CACHE_FULL:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_cache_full(ssl_acceptor_fd->
							  ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_MISSES:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_misses(ssl_acceptor_fd->
						      ssl_context)),
			  buff, 10);
        break;
      case SHOW_SSL_CTX_SESS_TIMEOUTS:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_timeouts(ssl_acceptor_fd->ssl_context)),
			  buff,10);
        break;
      case SHOW_SSL_CTX_SESS_NUMBER:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_number(ssl_acceptor_fd->ssl_context)),
			  buff,10);
        break;
      case SHOW_SSL_CTX_SESS_CONNECT:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_connect(ssl_acceptor_fd->ssl_context)),
			  buff,10);
        break;
      case SHOW_SSL_CTX_SESS_GET_CACHE_SIZE:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_sess_get_cache_size(ssl_acceptor_fd->ssl_context)),
				  buff,10);
        break;
      case SHOW_SSL_CTX_GET_VERIFY_MODE:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_get_verify_mode(ssl_acceptor_fd->ssl_context)),
			  buff,10);
        break;
      case SHOW_SSL_CTX_GET_VERIFY_DEPTH:
	end= int10_to_str((long) (!ssl_acceptor_fd ? 0 :
				  SSL_CTX_get_verify_depth(ssl_acceptor_fd->ssl_context)),
			  buff,10);
        break;
      case SHOW_SSL_CTX_GET_SESSION_CACHE_MODE:
	if (!ssl_acceptor_fd)
	{
	  pos= "NONE";
	  end= pos+4;
	  break;
	}
	switch (SSL_CTX_get_session_cache_mode(ssl_acceptor_fd->ssl_context))
	{
          case SSL_SESS_CACHE_OFF:
            pos= "OFF";
	    break;
          case SSL_SESS_CACHE_CLIENT:
            pos= "CLIENT";
	    break;
          case SSL_SESS_CACHE_SERVER:
            pos= "SERVER";
	    break;
          case SSL_SESS_CACHE_BOTH:
            pos= "BOTH";
	    break;
          case SSL_SESS_CACHE_NO_AUTO_CLEAR:
            pos= "NO_AUTO_CLEAR";
	    break;
          case SSL_SESS_CACHE_NO_INTERNAL_LOOKUP:
            pos= "NO_INTERNAL_LOOKUP";
	    break;
	  default:
            pos= "Unknown";
	    break;
	}
	end= strend(pos);
        break;
	/* First group - functions relying on SSL */
      case SHOW_SSL_GET_VERSION:
	pos= (thd->net.vio->ssl_arg ?
	      SSL_get_version((SSL*) thd->net.vio->ssl_arg) : "");
	end= strend(pos);
        break;
      case SHOW_SSL_SESSION_REUSED:
	end= int10_to_str((long) (thd->net.vio->ssl_arg ?
				  SSL_session_reused((SSL*) thd->net.vio->
						     ssl_arg) :
				  0),
			  buff, 10);
        break;
      case SHOW_SSL_GET_DEFAULT_TIMEOUT:
	end= int10_to_str((long) (thd->net.vio->ssl_arg ?
				  SSL_get_default_timeout((SSL*) thd->net.vio->
							  ssl_arg) :
				  0),
			  buff, 10);
        break;
      case SHOW_SSL_GET_VERIFY_MODE:
	end= int10_to_str((long) (thd->net.vio->ssl_arg ?
				  SSL_get_verify_mode((SSL*) thd->net.vio->
						      ssl_arg):
				  0),
			  buff, 10);
        break;
      case SHOW_SSL_GET_VERIFY_DEPTH:
	end= int10_to_str((long) (thd->net.vio->ssl_arg ?
				  SSL_get_verify_depth((SSL*) thd->net.vio->
						       ssl_arg):
				  0),
			  buff, 10);
        break;
      case SHOW_SSL_GET_CIPHER:
	pos= (thd->net.vio->ssl_arg ?
	      SSL_get_cipher((SSL*) thd->net.vio->ssl_arg) : "" );
	end= strend(pos);
	break;
      case SHOW_SSL_GET_CIPHER_LIST:
	if (thd->net.vio->ssl_arg)
	{
	  char *to= buff;
	  for (int i=0 ; i++ ;)
	  {
	    const char *p= SSL_get_cipher_list((SSL*) thd->net.vio->ssl_arg,i);
	    if (p == NULL) 
	      break;
	    to= strmov(to, p);
	    *to++= ':';
	  }
	  if (to != buff)
	    to--;				// Remove last ':'
	  end= to;
        }
        break;

#endif /* HAVE_OPENSSL */
      case SHOW_KEY_CACHE_LONG:
      case SHOW_KEY_CACHE_CONST_LONG:
	value= (value-(char*) &dflt_key_cache_var)+ (char*) sql_key_cache;
	end= int10_to_str(*(long*) value, buff, 10);
        break;
      case SHOW_UNDEF:				// Show never happen
      case SHOW_SYS:
	break;					// Return empty string
      default:
	break;
      }
      if (protocol->store(pos, (uint32) (end - pos), system_charset_info) ||
	  protocol->write())
        goto err;                               /* purecov: inspected */
    }
  }
  pthread_mutex_unlock(mutex);
  send_eof(thd);
  DBUG_RETURN(0);

 err:
  pthread_mutex_unlock(mutex);
  DBUG_RETURN(1);
}


/* collect status for all running threads */

void calc_sum_of_all_status(STATUS_VAR *to)
{
  DBUG_ENTER("calc_sum_of_all_status");

  /* Ensure that thread id not killed during loop */
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list

  I_List_iterator<THD> it(threads);
  THD *tmp;
  
  /* Get global values as base */
  *to= global_status_var;
  
  /* Add to this status from existing threads */
  while ((tmp= it++))
    add_to_status(to, &tmp->status_var);
  
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  DBUG_VOID_RETURN;
}


LEX_STRING *make_lex_string(THD *thd, LEX_STRING *lex_str,
                            const char* str, uint length,
                            bool allocate_lex_string)
{
  MEM_ROOT *mem= thd->mem_root;
  if (allocate_lex_string)
    lex_str= (LEX_STRING *)thd->alloc(sizeof(LEX_STRING));
  lex_str->str= strmake_root(mem, str, length);
  lex_str->length= length;
  return lex_str;
}


/* INFORMATION_SCHEMA name */
LEX_STRING information_schema_name= {(char*)"information_schema", 18};
extern ST_SCHEMA_TABLE schema_tables[];

typedef struct st_index_field_values
{
  const char *db_value, *table_value;
} INDEX_FIELD_VALUES;


void get_index_field_values(LEX *lex, INDEX_FIELD_VALUES *index_field_values)
{
  const char *wild= lex->wild ? lex->wild->ptr() : NullS;
  switch (lex->orig_sql_command) {
  case SQLCOM_SHOW_DATABASES:
    index_field_values->db_value= wild;
    break;
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TABLE_STATUS:
    index_field_values->db_value= lex->current_select->db;
    index_field_values->table_value= wild;
    break;
  default:
    index_field_values->db_value= NullS;
    index_field_values->table_value= NullS;
    break;
  }
}


int make_table_list(THD *thd, SELECT_LEX *sel,
                    char *db, char *table)
{
  Table_ident *table_ident;
  LEX_STRING ident_db, ident_table;
  ident_db.str= db; 
  ident_db.length= strlen(db);
  ident_table.str= table;
  ident_table.length= strlen(table);
  table_ident= new Table_ident(thd, ident_db, ident_table, 1);
  sel->init_query();
  if(!sel->add_table_to_list(thd, table_ident, 0, 0, TL_READ,
                             (List<String> *) 0, (List<String> *) 0))
    return 1;
  return 0;
}


bool uses_only_table_name_fields(Item *item, TABLE_LIST *table)
{
  if (item->type() == Item::FUNC_ITEM)
  {
    Item_func *item_func= (Item_func*)item;
    Item **child;
    Item **item_end= (item_func->arguments()) + item_func->argument_count();
    for (child= item_func->arguments(); child != item_end; child++)
      if (!uses_only_table_name_fields(*child, table))
        return 0;
    return 1;
  }
  else if (item->type() == Item::FIELD_ITEM)
  {
    Item_field *item_field= (Item_field*)item;
    CHARSET_INFO *cs= system_charset_info;
    ST_SCHEMA_TABLE *schema_table= table->schema_table;
    ST_FIELD_INFO *field_info= schema_table->fields_info;
    const char *field_name1= field_info[schema_table->idx_field1].field_name;
    const char *field_name2= field_info[schema_table->idx_field2].field_name;
    if(table->table != item_field->field->table ||
       (cs->coll->strnncollsp(cs, (uchar *) field_name1, strlen(field_name1),
                              (uchar *) item_field->field_name, 
                              strlen(item_field->field_name)) &&
        cs->coll->strnncollsp(cs, (uchar *) field_name2, strlen(field_name2),
                              (uchar *) item_field->field_name, 
                              strlen(item_field->field_name))))
      return 0;
    else
      return 1;
  }
  else
    return 1;
}


static COND * make_cond_for_info_schema(COND *cond, TABLE_LIST *table)
{
  if (!cond)
    return (COND*) 0;
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (COND*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_info_schema(item, table);
	if (fix)
	  new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (COND*) 0;
      case 1:
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
	return new_cond;
      }
    }
    else
    {						// Or list
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (COND*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_info_schema(item, table);
	if (!fix)
	  return (COND*) 0;
	new_cond->argument_list()->push_back(fix);
      }
      new_cond->quick_fix_field();
      new_cond->top_level_item();
      return new_cond;
    }
  }

  if (!uses_only_table_name_fields(cond, table))
    return (COND*) 0;
  return cond;
}


int get_all_tables(THD *thd, TABLE_LIST *tables, COND *cond)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  SELECT_LEX *select_lex= &lex->select_lex;
  ST_SCHEMA_TABLE *schema_table= tables->schema_table;
  DBUG_ENTER("fill_schema_tables");

  SELECT_LEX *lsel= tables->schema_select_lex;
  if (lsel)
  {
    TABLE *old_open_tables= thd->open_tables;
    TABLE_LIST *show_table_list= (TABLE_LIST*) lsel->table_list.first;
    lex->all_selects_list= lsel;
    int res= open_and_lock_tables(thd, show_table_list);
    if (schema_table->process_table(thd, show_table_list,
                                    table, res, show_table_list->db,
                                    show_table_list->real_name))
    {
      DBUG_RETURN(1);
    }
    close_thread_tables(thd, 0, 0, old_open_tables);
    show_table_list->table= 0;
    lex->all_selects_list= select_lex;
    DBUG_RETURN(0);
  }

  SELECT_LEX sel;
  INDEX_FIELD_VALUES idx_field_vals;
  char path[FN_REFLEN], *end, *base_name, *file_name;
  uint len;
  List<char> bases;
  lex->all_selects_list= &sel;
  enum enum_schema_tables schema_table_idx=
    (enum enum_schema_tables) (schema_table - &schema_tables[0]);
  thr_lock_type lock_type= TL_UNLOCK;
  if (schema_table_idx == SCH_TABLES)
    lock_type= TL_READ;
  get_index_field_values(lex, &idx_field_vals);
  if (mysql_find_files(thd, &bases, NullS, mysql_data_home,
		       idx_field_vals.db_value, 1))
    return 1;
  List_iterator_fast<char> it(bases);
  COND *partial_cond= make_cond_for_info_schema(cond, tables);
  while ((base_name=it++) ||
	 /*
	   generate error for non existing database.
	   (to save old behaviour for SHOW TABLES FROM db)
	 */
	 ((lex->orig_sql_command == SQLCOM_SHOW_TABLES ||
           lex->orig_sql_command == SQLCOM_SHOW_TABLE_STATUS) &&
	  (base_name= select_lex->db) && !bases.elements))
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (!check_access(thd,SELECT_ACL, base_name, &thd->col_access,0,1) ||
        thd->master_access & (DB_ACLS | SHOW_DB_ACL) ||
	acl_get(thd->host, thd->ip, thd->priv_user, base_name,0) ||
	(grant_option && !check_grant_db(thd, base_name)))
#endif
    {
      List<char> files;
      (void) sprintf(path,"%s/%s",mysql_data_home,base_name);
      end= path + (len= unpack_dirname(path,path));
      len= FN_LEN - len;
      if (mysql_find_files(thd, &files, base_name, 
                           path, idx_field_vals.table_value, 0))
	DBUG_RETURN(1);

      List_iterator_fast<char> it(files);
      while ((file_name=it++))
      {
	restore_record(table, default_values);
        table->field[schema_table->idx_field1]->
          store(base_name, strlen(base_name), system_charset_info);
        table->field[schema_table->idx_field2]->
          store(file_name, strlen(file_name),system_charset_info);
        if (!partial_cond || partial_cond->val_int())
        {
          if (schema_table_idx == SCH_TABLE_NAMES)
          {
            if (lex->verbose || lex->orig_sql_command == SQLCOM_END)
            {
              my_snprintf(end, len, "/%s%s", file_name, reg_ext);
              switch (mysql_frm_type(path))
              {
              case FRMTYPE_ERROR:
                table->field[3]->store("ERROR", 5, system_charset_info);
                break;
              case FRMTYPE_TABLE:
                table->field[3]->store("BASE TABLE", 10, system_charset_info);
                break;
              case FRMTYPE_VIEW:
                table->field[3]->store("VIEW", 4, system_charset_info);
                break;
              default:
                DBUG_ASSERT(0);
              }
            }
            table->file->write_row(table->record[0]);
          }
          else
          {
            int res;
            TABLE *old_open_tables= thd->open_tables;
            if (make_table_list(thd, &sel, base_name, file_name))
              DBUG_RETURN(1);
            TABLE_LIST *show_table_list= (TABLE_LIST*) sel.table_list.first;
            show_table_list->lock_type= lock_type;
            res= open_and_lock_tables(thd, show_table_list);
            if (schema_table->process_table(thd, show_table_list, table,
                                            res, base_name, file_name))
            {
              DBUG_RETURN(1);
            }
            close_thread_tables(thd, 0, 0, old_open_tables);
          }
        }
      }
    }
  }
  lex->all_selects_list= select_lex;
  DBUG_RETURN(0);
}


int fill_schema_shemata(THD *thd, TABLE_LIST *tables, COND *cond)
{
  char path[FN_REFLEN],*end;
  bool found_libchar;
  INDEX_FIELD_VALUES idx_field_vals;
  List<char> files;
  char *file_name;
  uint length;
  HA_CREATE_INFO create;
  TABLE *table= tables->table;

  get_index_field_values(thd->lex, &idx_field_vals);
  if (mysql_find_files(thd, &files, NullS, mysql_data_home,
                       idx_field_vals.db_value, 1))
    return 1;
  List_iterator_fast<char> it(files);
  while ((file_name=it++))
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (thd->master_access & (DB_ACLS | SHOW_DB_ACL) ||
	acl_get(thd->host, thd->ip, thd->priv_user, file_name,0) ||
	(grant_option && !check_grant_db(thd, file_name)))
#endif
    {
      (void) sprintf(path,"%s/%s",mysql_data_home, file_name);
      length=unpack_dirname(path,path);		// Convert if not unix
      found_libchar= 0;
      if (length && path[length-1] == FN_LIBCHAR)
      {
	found_libchar= 1;
	path[length-1]=0;			// remove ending '\'
      }

      if (found_libchar)
	path[length-1]= FN_LIBCHAR;
      strmov(path+length, MY_DB_OPT_FILE);
      load_db_opt(thd, path, &create);
      restore_record(table, default_values);
      table->field[1]->store(file_name, strlen(file_name), system_charset_info);
      table->field[2]->store(create.default_table_charset->name, 
			     strlen(create.default_table_charset->name),
			     system_charset_info);
      table->file->write_row(table->record[0]);
    }
  }
  return 0;
}


int get_schema_tables_record(THD *thd, struct st_table_list *tables,
                             TABLE *table, int res, 
                             const char *base_name, const char *file_name)
{
  const char *tmp_buff;
  TIME time;
  CHARSET_INFO *cs= system_charset_info;

  DBUG_ENTER("get_schema_tables_record");
  restore_record(table, default_values);
  if (res > 0)
  {
    DBUG_RETURN(1);
  }
  table->field[1]->store(base_name, strlen(base_name), cs);
  table->field[2]->store(file_name, strlen(file_name), cs);
  if (res < 0 || tables->view)
  {
    table->field[3]->store("VIEW", 4, cs);
    table->field[20]->store("view", 4, cs);
    if (res)
      thd->clear_error();
  }
  else
  {
    TABLE *show_table= tables->table;
    handler *file= show_table->file;
    file->info(HA_STATUS_VARIABLE | HA_STATUS_TIME | HA_STATUS_NO_LOCK);
    table->field[3]->store("BASE TABLE", 10, cs);
    for (int i= 4; i < 20; i++)
    {
      if ((i > 12 && i < 17) || i == 18)
        continue;
      table->field[i]->set_notnull();
    }
    tmp_buff= file->table_type();
    table->field[4]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[5]->store((longlong) show_table->frm_version);
    tmp_buff= ((show_table->db_options_in_use &
                HA_OPTION_COMPRESS_RECORD) ? "Compressed" :
               (show_table->db_options_in_use & HA_OPTION_PACK_RECORD) ?
               "Dynamic" : "Fixed");
    table->field[6]->store(tmp_buff, strlen(tmp_buff), cs);
    table->field[7]->store((longlong) file->records);
    table->field[8]->store((longlong) file->mean_rec_length);
    table->field[9]->store((longlong) file->data_file_length);
    if (file->max_data_file_length)
    {
      table->field[10]->store((longlong) file->max_data_file_length);
    }
    table->field[11]->store((longlong) file->index_file_length);
    table->field[12]->store((longlong) file->delete_length);
    if (table->found_next_number_field)
    {
      show_table->next_number_field=show_table->found_next_number_field;
      show_table->next_number_field->reset();
      file->update_auto_increment();
      table->field[13]->store((longlong) show_table->
                              next_number_field->val_int());
      table->field[13]->set_notnull();
      show_table->next_number_field=0;
    }
    if (file->create_time)
    {
      thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                file->create_time);
      table->field[14]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
      table->field[14]->set_notnull();
    }
    if (file->update_time)
    {
      thd->variables.time_zone->gmt_sec_to_TIME(&time,
                                                file->update_time);
      table->field[15]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
      table->field[15]->set_notnull();
    }
    if (file->check_time)
    {
      thd->variables.time_zone->gmt_sec_to_TIME(&time, file->check_time);
      table->field[16]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
      table->field[16]->set_notnull();
    }
    tmp_buff= (show_table->table_charset ? show_table->
               table_charset->name : "default");
    table->field[17]->store(tmp_buff, strlen(tmp_buff), cs);
    if (file->table_flags() & HA_HAS_CHECKSUM)
    {
      table->field[18]->store((longlong) file->checksum());
      table->field[18]->set_notnull();
    }

    char option_buff[350],*ptr;
    ptr=option_buff;
    if (show_table->min_rows)
    {
      ptr=strmov(ptr," min_rows=");
      ptr=longlong10_to_str(show_table->min_rows,ptr,10);
    }
    if (show_table->max_rows)
    {
      ptr=strmov(ptr," max_rows=");
      ptr=longlong10_to_str(show_table->max_rows,ptr,10);
    }
    if (show_table->avg_row_length)
    {
      ptr=strmov(ptr," avg_row_length=");
      ptr=longlong10_to_str(show_table->avg_row_length,ptr,10);
    }
    if (show_table->db_create_options & HA_OPTION_PACK_KEYS)
      ptr=strmov(ptr," pack_keys=1");
    if (show_table->db_create_options & HA_OPTION_NO_PACK_KEYS)
      ptr=strmov(ptr," pack_keys=0");
    if (show_table->db_create_options & HA_OPTION_CHECKSUM)
      ptr=strmov(ptr," checksum=1");
    if (show_table->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      ptr=strmov(ptr," delay_key_write=1");
    if (show_table->row_type != ROW_TYPE_DEFAULT)
      ptr=strxmov(ptr, " row_format=", 
                  ha_row_type[(uint) show_table->row_type],
                  NullS);
    if (file->raid_type)
    {
      char buff[100];
      sprintf(buff," raid_type=%s raid_chunks=%d raid_chunksize=%ld",
              my_raid_type(file->raid_type), file->raid_chunks,
              file->raid_chunksize/RAID_BLOCK_SIZE);
      ptr=strmov(ptr,buff);
    }
    table->field[19]->store(option_buff+1,
                            (ptr == option_buff ? 0 : 
                             (uint) (ptr-option_buff)-1), cs);

    char *comment= show_table->file->
      update_table_comment(show_table->comment);
    table->field[20]->store(comment, strlen(comment), cs);
    if (comment != show_table->comment)
      my_free(comment,MYF(0));
  }
  table->file->write_row(table->record[0]);
  DBUG_RETURN(0);
}


int get_schema_column_record(THD *thd, struct st_table_list *tables,
                             TABLE *table, int res, 
                             const char *base_name, const char *file_name)
{
  TIME time;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("get_schema_column_record");
  if (res)
  {
    DBUG_RETURN(1);
  }

  TABLE *show_table= tables->table;
  handler *file= show_table->file;
  file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  restore_record(show_table, default_values);
  Field **ptr,*field;
  int count= 0;
  for (ptr=show_table->field; (field= *ptr) ; ptr++)
  {
    if (!wild || !wild[0] || 
        !wild_case_compare(system_charset_info, field->field_name,wild))
    {
      uint tmp_length;
      char *tmp_buff;
      byte *pos;
      uint flags=field->flags;
      char tmp[MAX_FIELD_WIDTH];
      char tmp1[MAX_FIELD_WIDTH];
      String type(tmp,sizeof(tmp), system_charset_info);
      char tmp_buffer[128];
      count++;
      restore_record(table, default_values);
      table->field[1]->store(base_name, strlen(base_name), cs);
      table->field[2]->store(file_name, strlen(file_name), cs);
      table->field[3]->store(field->field_name, strlen(field->field_name),
                             cs);
      table->field[4]->store((longlong) count);
      field->sql_type(type);
      table->field[11]->store(type.ptr(), type.length(), cs);		
      tmp_buff= strchr(type.ptr(),'(');
      table->field[5]->store(type.ptr(),
                             (tmp_buff ? tmp_buff - type.ptr() :
                              type.length()), cs);

      if (show_table->timestamp_field == field &&
          field->unireg_check != Field::TIMESTAMP_UN_FIELD)
      {
        table->field[15]->store("CURRENT_TIMESTAMP", 17, cs);
        table->field[15]->set_notnull();
      }
      else if (field->unireg_check != Field::NEXT_NUMBER &&
               !field->is_null() &&
               !(field->flags & NO_DEFAULT_VALUE_FLAG))
      {
        String def(tmp1,sizeof(tmp1), cs);
        type.set(tmp, sizeof(tmp), field->charset());
        field->val_str(&type);
        uint dummy_errors;
        def.copy(type.ptr(), type.length(), type.charset(), cs, &dummy_errors);
        table->field[15]->store(def.ptr(), def.length(), def.charset());
        table->field[15]->set_notnull();
      }
      else if (field->unireg_check == Field::NEXT_NUMBER ||
               field->maybe_null())
        table->field[15]->set_null();                // Null as default
      else
      {
        table->field[15]->store("",0, cs);
        table->field[15]->set_notnull();
      }
		
      pos=(byte*) ((flags & NOT_NULL_FLAG) &&
                   field->type() != FIELD_TYPE_TIMESTAMP ?
                   "" : "YES");
      table->field[13]->store((const char*) pos,
                              strlen((const char*) pos), cs);
      if (field->has_charset())
      {
        table->field[6]->store((longlong) field->field_length/
                               field->charset()->mbmaxlen);
      }
      table->field[7]->store((longlong) field->field_length); 
      table->field[8]->store((longlong) field->pack_length());
      table->field[9]->store((longlong) field->decimals());
      if (field->has_charset())
      {
        pos=(byte*) field->charset()->csname;
        table->field[10]->store((const char*) pos,
                                strlen((const char*) pos), cs);
        table->field[10]->set_notnull();
        pos=(byte*) field->charset()->name;
        table->field[12]->store((const char*) pos,
                                strlen((const char*) pos), cs);
        table->field[12]->set_notnull();
      }
      pos=(byte*) ((field->flags & PRI_KEY_FLAG) ? "PRI" :
                   (field->flags & UNIQUE_KEY_FLAG) ? "UNI" :
                   (field->flags & MULTIPLE_KEY_FLAG) ? "MUL":"");
      table->field[14]->store((const char*) pos,
                              strlen((const char*) pos), cs);
      char *end=tmp;
      if (field->unireg_check == Field::NEXT_NUMBER)
        end=strmov(tmp,"auto_increment");
      table->field[16]->store(tmp, (uint) (end-tmp), cs);
      if (thd->lex->verbose)
      {
        end=tmp;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
        uint col_access;
        check_access(thd,SELECT_ACL | EXTRA_ACL, base_name,
                     &tables->grant.privilege, 0, 0);
        col_access= get_column_grant(thd, &tables->grant, tables->db,
                                     tables->real_name,
                                     field->field_name) & COL_ACLS;
        for (uint bitnr=0; col_access ; col_access>>=1,bitnr++)
        {
          if (col_access & 1)
          {
            *end++=',';
            end=strmov(end,grant_types.type_names[bitnr]);
          }
        }
#else
        end=strmov(end,"");
#endif
        table->field[17]->store(tmp+1,end == tmp ? 0 : (uint) (end-tmp-1), cs);
        table->field[18]->store(field->comment.str, field->comment.length, cs);
      }
      table->file->write_row(table->record[0]);
    }
  }
  DBUG_RETURN(0);
}



int fill_schema_charsets(THD *thd, TABLE_LIST *tables, COND *cond)
{
  CHARSET_INFO **cs;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;
  for ( cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO *tmp_cs= cs[0];
    if (tmp_cs && (tmp_cs->state & MY_CS_PRIMARY) && 
        (tmp_cs->state & MY_CS_AVAILABLE) &&
        !(wild && wild[0] &&
	  wild_case_compare(scs, tmp_cs->csname,wild)))
    {
      restore_record(table, default_values);
      table->field[0]->store(tmp_cs->csname, strlen(tmp_cs->csname), scs);
      table->field[1]->store(tmp_cs->comment ? tmp_cs->comment : "",
			     strlen(tmp_cs->comment ? tmp_cs->comment : ""),
                             scs);
      table->field[2]->store(tmp_cs->name, strlen(tmp_cs->name), scs);
      table->field[3]->store((longlong) tmp_cs->mbmaxlen);
      table->file->write_row(table->record[0]);
    }
  }
  return 0;
}


int fill_schema_collation(THD *thd, TABLE_LIST *tables, COND *cond)
{
  CHARSET_INFO **cs;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;
  for ( cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) || 
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for ( cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) || 
          !my_charset_same(tmp_cs, tmp_cl))
	continue;
      if (!(wild && wild[0] &&
	  wild_case_compare(scs, tmp_cl->name,wild)))
      {
	const char *tmp_buff;
	restore_record(table, default_values);
	table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
        table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
        table->field[2]->store((longlong) tmp_cl->number);
        tmp_buff= (tmp_cl->state & MY_CS_PRIMARY) ? "Yes" : "";
	table->field[3]->store(tmp_buff, strlen(tmp_buff), scs);
        tmp_buff= (tmp_cl->state & MY_CS_COMPILED)? "Yes" : "";
	table->field[4]->store(tmp_buff, strlen(tmp_buff), scs);
        table->field[5]->store((longlong) tmp_cl->strxfrm_multiply);
	table->file->write_row(table->record[0]);
      }
    }
  }
  return 0;
}


int fill_schema_coll_charset_app(THD *thd, TABLE_LIST *tables, COND *cond)
{
  CHARSET_INFO **cs;
  const char *wild= NullS;
  TABLE *table= tables->table;
  CHARSET_INFO *scs= system_charset_info;
  for ( cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) || 
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for ( cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) || 
          !my_charset_same(tmp_cs,tmp_cl))
	continue;
      restore_record(table, default_values);
      table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
      table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
      table->file->write_row(table->record[0]);
    }
  }
  return 0;
}


void store_schema_proc(THD *thd, TABLE *table,
		       TABLE *proc_table,
		       const char *wild)
{
  String tmp_string;
  TIME time;
  LEX *lex= thd->lex;
  CHARSET_INFO *cs= system_charset_info;
  restore_record(table, default_values);
  if (lex->orig_sql_command == SQLCOM_SHOW_STATUS_PROC &&
      proc_table->field[2]->val_int() == TYPE_ENUM_PROCEDURE ||
      lex->orig_sql_command == SQLCOM_SHOW_STATUS_FUNC &&
      proc_table->field[2]->val_int() == TYPE_ENUM_FUNCTION ||
      lex->orig_sql_command == SQLCOM_END)
  {
    tmp_string.length(0);
    get_field(thd->mem_root, proc_table->field[1], &tmp_string);
    if (!wild || !wild[0] || !wild_compare(tmp_string.ptr(), wild, 0))
    {
      table->field[3]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[0], &tmp_string);
      table->field[2]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[2], &tmp_string);
      table->field[4]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[3], &tmp_string);
      table->field[0]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[5], &tmp_string);
      table->field[11]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[6], &tmp_string);
      table->field[10]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[7], &tmp_string);
      table->field[15]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[9], &tmp_string);
      table->field[6]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[10], &tmp_string);
      table->field[8]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[11], &tmp_string);
      table->field[5]->store(tmp_string.ptr(), tmp_string.length(), cs);
      bzero((char *)&time, sizeof(time));
      ((Field_timestamp *) proc_table->field[12])->get_time(&time);
      table->field[14]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
      bzero((char *)&time, sizeof(time));
      ((Field_timestamp *) proc_table->field[13])->get_time(&time);
      table->field[13]->store_time(&time, MYSQL_TIMESTAMP_DATETIME);
      get_field(thd->mem_root, proc_table->field[14], &tmp_string);
      table->field[16]->store(tmp_string.ptr(), tmp_string.length(), cs);
      tmp_string.length(0);
      get_field(thd->mem_root, proc_table->field[15], &tmp_string);
      table->field[17]->store(tmp_string.ptr(), tmp_string.length(), cs);
      table->field[7]->store("SQL", 3, cs);
      table->field[9]->store("SQL", 3, cs);
      table->file->write_row(table->record[0]);
    }
  }
}


int fill_schema_proc(THD *thd, TABLE_LIST *tables, COND *cond)
{
  TABLE *proc_table;
  TABLE_LIST proc_tables;
  const char *wild= thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  int res= 0;
  TABLE *table= tables->table, *old_open_tables= thd->open_tables;
  DBUG_ENTER("fill_schema_proc");

  bzero((char*) &proc_tables,sizeof(proc_tables));
  proc_tables.db= (char*) "mysql";
  proc_tables.real_name= proc_tables.alias= (char*) "proc";
  proc_tables.lock_type= TL_READ;
  if (!(proc_table= open_ltable(thd, &proc_tables, TL_READ)))
  {
    DBUG_RETURN(1);
  }
  proc_table->file->ha_index_init(0);
  if ((res= proc_table->file->index_first(proc_table->record[0])))
  {
    res= (res == HA_ERR_END_OF_FILE) ? 0 : 1;
    goto err;
  }
  store_schema_proc(thd, table, proc_table, wild);
  while (!proc_table->file->index_next(proc_table->record[0]))
    store_schema_proc(thd, table, proc_table, wild);

err:
  proc_table->file->ha_index_end();
  close_thread_tables(thd, 0, 0, old_open_tables);
  DBUG_RETURN(res);
}


int get_schema_stat_record(THD *thd, struct st_table_list *tables,
                           TABLE *table, int res, 
                           const char *base_name, const char *file_name)
{
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("get_schema_stat_record");
  if (!res)
  {
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    show_table->file->info(HA_STATUS_VARIABLE | 
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);
    for (uint i=0 ; i < show_table->keys ; i++,key_info++)
    {
      KEY_PART_INFO *key_part= key_info->key_part;
      const char *str;
      for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
      {
        restore_record(table, default_values);
        table->field[1]->store(base_name, strlen(base_name), cs);
        table->field[2]->store(file_name, strlen(file_name), cs);
        table->field[3]->store((longlong) ((key_info->flags & 
                                            HA_NOSAME) ? 0 :1));
        table->field[4]->store(base_name, strlen(base_name), cs);
        table->field[5]->store(key_info->name, strlen(key_info->name), cs);
        table->field[6]->store((longlong) (j+1));
        str=(key_part->field ? key_part->field->field_name :
             "?unknown field?");
        table->field[7]->store(str, strlen(str), cs);
        if (show_table->file->index_flags(i, j, 0) & HA_READ_ORDER)
        {
          table->field[8]->store(((key_part->key_part_flag &
                                   HA_REVERSE_SORT) ?
                                  "D" : "A"), 1, cs);
          table->field[8]->set_notnull();
        }
        KEY *key=show_table->key_info+i;
        if (key->rec_per_key[j])
        {
          ha_rows records=(show_table->file->records /
                           key->rec_per_key[j]);
          table->field[9]->store((longlong) records);
          table->field[9]->set_notnull();
        }
        if (!(key_info->flags & HA_FULLTEXT) && 
            (!key_part->field ||
             key_part->length != 
             show_table->field[key_part->fieldnr-1]->key_length()))
        {
          table->field[10]->store((longlong) key_part->length);
          table->field[10]->set_notnull();
        }
        uint flags= key_part->field ? key_part->field->flags : 0;
        const char *pos=(char*) ((flags & NOT_NULL_FLAG) ? "" : "YES");
        table->field[12]->store(pos, strlen(pos), cs);
        pos= show_table->file->index_type(i);
        table->field[13]->store(pos, strlen(pos), cs);
        if (!show_table->keys_in_use.is_set(i))
          table->field[14]->store("disabled", 8, cs);
        else
          table->field[14]->store("", 0, cs);
        table->field[14]->set_notnull();
        table->file->write_row(table->record[0]);
      }
    }
  }
  DBUG_RETURN(0);
}


int get_schema_views_record(THD *thd, struct st_table_list *tables,
                            TABLE *table, int res, 
                            const char *base_name, const char *file_name)
{
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("get_schema_views_record");
  if (!res)
  {
    if (tables->view)
    {
      restore_record(table, default_values);
      table->field[1]->store(tables->view_db.str, tables->view_db.length, cs);
      table->field[2]->store(tables->view_name.str,tables->view_name.length,cs);
      table->field[3]->store(tables->query.str, tables->query.length, cs);
      table->field[4]->store("NONE", 4, cs);
      if (tables->updatable_view)
        table->field[5]->store("YES", 3, cs);
      else
        table->field[5]->store("NO", 2, cs);
      table->file->write_row(table->record[0]);
    }
  }
  DBUG_RETURN(0);
}


int get_schema_constarints_record(THD *thd, struct st_table_list *tables,
                                  TABLE *table, int res, 
                                  const char *base_name, const char *file_name)
{
  CHARSET_INFO *cs= system_charset_info;
  DBUG_ENTER("get_schema_constarints_record");
  if (!res)
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint primary_key= show_table->primary_key;
    show_table->file->info(HA_STATUS_VARIABLE | 
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);
    for (uint i=0 ; i < show_table->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
        continue;              
      restore_record(table, default_values);
      table->field[1]->store(base_name, strlen(base_name), cs);
      table->field[2]->store(key_info->name, strlen(key_info->name), cs);
      table->field[3]->store(base_name, strlen(base_name), cs);
      table->field[4]->store(file_name, strlen(file_name), cs);
      if (i == primary_key && !strcmp(key_info->name, primary_key_name))
        table->field[5]->store("PRIMARY", 7, cs);
      else if (key_info->flags & HA_NOSAME)
        table->field[5]->store("UNIQUE", 6, cs);
      table->file->write_row(table->record[0]);
    }

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info=it++))
    {
      restore_record(table, default_values);
      table->field[1]->store(base_name, strlen(base_name), cs);
      table->field[2]->store(f_key_info->forein_id->str,
                             f_key_info->forein_id->length, cs);
      table->field[3]->store(base_name, strlen(base_name), cs);
      table->field[4]->store(file_name, strlen(file_name), cs);
      table->field[5]->store("FOREIGN", 7, system_charset_info);
      table->field[6]->store(f_key_info->constraint_method->str,
                             f_key_info->constraint_method->length, cs);
      table->field[6]->set_notnull();
      table->file->write_row(table->record[0]);
    }
  }
  DBUG_RETURN(0);
}


int get_schema_key_column_usage_record(THD *thd, struct st_table_list *tables,
                                       TABLE *table, int res,
                                       const char *base_name,
                                       const char *file_name)
{
  DBUG_ENTER("get_schema_key_column_usage_record");
  CHARSET_INFO *cs= system_charset_info;
  if (!res)
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    TABLE *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint primary_key= show_table->primary_key;
    show_table->file->info(HA_STATUS_VARIABLE | 
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);
    for (uint i=0 ; i < show_table->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
        continue;              
      uint f_idx= 0;
      KEY_PART_INFO *key_part= key_info->key_part;
      for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
      {
        uint f_idx= 0;
        if (key_part->field)
        {
          f_idx++;
          restore_record(table, default_values);
          table->field[1]->store(base_name, strlen(base_name), cs);
          table->field[2]->store(key_info->name, strlen(key_info->name), cs);
          table->field[3]->store(base_name, strlen(base_name), cs);
          table->field[4]->store(file_name, strlen(file_name), cs);
          table->field[5]->store(key_part->field->field_name, 
                                 strlen(key_part->field->field_name), cs);
          table->field[6]->store((longlong) f_idx);
          table->file->write_row(table->record[0]);
        }
      }
    }

    show_table->file->get_foreign_key_list(thd, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info= it++))
    {
      LEX_STRING *f_info, *r_info;
      List_iterator_fast<LEX_STRING> it(f_key_info->foreign_fields),
        it1(f_key_info->referenced_fields);
      uint f_idx= 0;
      while ((f_info= it++))
      {
        r_info= it1++;
        f_idx++;
        restore_record(table, default_values);
        table->field[1]->store(base_name, strlen(base_name), cs);
        table->field[2]->store(f_key_info->forein_id->str,
                               f_key_info->forein_id->length, cs);
        table->field[3]->store(base_name, strlen(base_name), cs);
        table->field[4]->store(file_name, strlen(file_name), cs);
        table->field[5]->store(f_info->str, f_info->length, cs);
        table->field[6]->store((longlong) f_idx);
        table->field[7]->store(f_key_info->referenced_db->str,
                               f_key_info->referenced_db->length, cs);
        table->field[7]->set_notnull();
        table->field[8]->store(f_key_info->referenced_table->str,
                               f_key_info->referenced_table->length, cs);
        table->field[8]->set_notnull();
        table->field[9]->store(r_info->str, r_info->length, cs);
        table->field[9]->set_notnull();
        table->file->write_row(table->record[0]);
      }
    }
  }
  DBUG_RETURN(0);
}


/*
  Find schema_tables elment by name

  SYNOPSIS
    find_schema_table()
    thd                 thread handler
    table_name          table name

  RETURN
    0	table not found
    #   pointer to 'shema_tables' element
*/

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char* table_name)
{
  ST_SCHEMA_TABLE *schema_table= schema_tables;
  for ( ; schema_table->table_name; schema_table++)
  {
    if (!my_strcasecmp(system_charset_info,
                       schema_table->table_name,
                       table_name))
      return schema_table;
  }
  return 0;
}


ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx)
{
  return &schema_tables[schema_table_idx];
}


/*
  Create information_schema table using schema_table data

  SYNOPSIS
    create_schema_table()
    thd	       	          thread handler
    schema_table          pointer to 'shema_tables' element

  RETURN
    #	                  Pointer to created table
    0	                  Can't create table
*/

TABLE *create_schema_table(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  int field_count= 0;
  Item *item;
  TABLE *table;
  List<Item> field_list;
  ST_FIELD_INFO *fields_info= schema_table->fields_info;
  DBUG_ENTER("create_schema_table");

  for ( ; fields_info->field_name; fields_info++)
  {
    switch (fields_info->field_type) {
    case MYSQL_TYPE_LONG:
      if (!(item= new Item_int(fields_info->field_name,
                               fields_info->value,
                               fields_info->field_length)))
      {
        DBUG_RETURN(0);
      }
      break;
    case MYSQL_TYPE_TIMESTAMP:
      if (!(item=new Item_datetime(fields_info->field_name)))
      {
        DBUG_RETURN(0);
      }
      break;
    default:
      CHARSET_INFO *cs= system_charset_info;
      if (fields_info->utf8)
	cs= thd->charset();
      if (!(item= new Item_string("", fields_info->field_length, cs)))
      {
        DBUG_RETURN(0);
      }
      item->set_name(fields_info->field_name, 
		     strlen(fields_info->field_name), cs);
      break;
    }
    field_list.push_back(item);
    item->maybe_null= fields_info->maybe_null;
    field_count++;
  }
  TMP_TABLE_PARAM *tmp_table_param =
    (TMP_TABLE_PARAM*) (thd->calloc(sizeof(TMP_TABLE_PARAM)));
  tmp_table_param->init();
  tmp_table_param->field_count= field_count;
  SELECT_LEX *select_lex= thd->lex->current_select;
  if (!(table= create_tmp_table(thd, tmp_table_param,
                                field_list, (ORDER*) 0, 0, 0, 
                                (select_lex->options | thd->options |
                                 TMP_TABLE_ALL_COLUMNS),
                                HA_POS_ERROR,
                                (char *) schema_table->table_name)))
    DBUG_RETURN(0);
  DBUG_RETURN(table);
}


/*
  For old SHOW compatibility. It is used when
  old SHOW doesn't have generated column names
  Make list of fields for SHOW

  SYNOPSIS
    make_old_format()
    thd			thread handler
    schema_table        pointer to 'schema_tables' element

  RETURN
   -1	errror
    0	success
*/

int make_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  ST_FIELD_INFO *field_info= schema_table->fields_info;
  for ( ; field_info->field_name; field_info++)
  {
    if (field_info->old_name)
    {
      Item_field *field= new Item_field(NullS, NullS, field_info->field_name);
      if (field)
      {
        field->set_name(field_info->old_name,
                        strlen(field_info->old_name),
                        system_charset_info);
        if (add_item_to_list(thd, field))
          return 1;
      }
    }
  }
  return 0;
}


int make_schemata_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  char tmp[128];
  LEX *lex= thd->lex;
  SELECT_LEX *sel= lex->current_select;

  if (!sel->item_list.elements)
  {
    ST_FIELD_INFO *field_info= &schema_table->fields_info[1];
    String buffer(tmp,sizeof(tmp), system_charset_info);
    Item_field *field= new Item_field(NullS, NullS, field_info->field_name);
    if (!field || add_item_to_list(thd, field))
      return 1;
    buffer.length(0);
    buffer.append(field_info->old_name);
    if (lex->wild && lex->wild->ptr())
    {
      buffer.append(" (");
      buffer.append(lex->wild->ptr());
      buffer.append(")");
    }
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  }
  return 0;
}


int make_table_names_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  char tmp[128];
  String buffer(tmp,sizeof(tmp), thd->charset());
  LEX *lex= thd->lex;

  ST_FIELD_INFO *field_info= &schema_table->fields_info[2];
  buffer.length(0);
  buffer.append(field_info->old_name);
  buffer.append(lex->select_lex.db);
  if (lex->wild && lex->wild->ptr())
  {
    buffer.append(" (");
    buffer.append(lex->wild->ptr());
    buffer.append(")");
  }
  Item_field *field= new Item_field(NullS, NullS, field_info->field_name);
  if (add_item_to_list(thd, field))
    return 1;
  field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  if (thd->lex->verbose)
  {
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
    field_info= &schema_table->fields_info[3];
    field= new Item_field(NullS, NullS, field_info->field_name);
    if (add_item_to_list(thd, field))
      return 1;
    field->set_name(field_info->old_name, strlen(field_info->old_name),
                    system_charset_info);
  }
  return 0;
}


int make_columns_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table)
{
  ST_FIELD_INFO *field_info= &schema_table->fields_info[3];
  int count= 2;
  for ( ; field_info->field_name; field_info++)
  {
    count++;
    if (field_info->old_name)
    {
      if (!thd->lex->verbose && (count == 12 ||count == 17 || count == 18))
	continue;
      Item_field *field= new Item_field(NullS, NullS, field_info->field_name);
      if (field)
      {
        field->set_name(field_info->old_name,
                        strlen(field_info->old_name),
                        system_charset_info);
        if (add_item_to_list(thd, field))
          return 1;
      }
    }
  }
  return 0;
}


/*
  Create information_schema table

  SYNOPSIS
  mysql_schema_table()
    thd                thread handler
    lex                pointer to LEX
    table_list         pointer to table_list

  RETURN
    0	success
    1   error
*/

int mysql_schema_table(THD *thd, LEX *lex, TABLE_LIST *table_list)
{
  TABLE *table;
  DBUG_ENTER("mysql_schema_table");
  if (!(table= table_list->schema_table->
	create_table(thd, table_list->schema_table)))
  {
    DBUG_RETURN(1);
  }
  table->tmp_table= TMP_TABLE;
  table->grant.privilege= SELECT_ACL;
  table_list->real_name= table->real_name;
  table_list->table= table;
  table->next= thd->derived_tables;
  thd->derived_tables= table;
  table_list->select_lex->options |= OPTION_SCHEMA_TABLE;
  DBUG_RETURN(0);
}


/*
  Generate select from information_schema table

  SYNOPSIS
    make_schema_select()
    thd                  thread handler
    sel                  pointer to SELECT_LEX
    schema_table_idx     index of 'schema_tables' element

  RETURN
    0	success
    1   error
*/

int make_schema_select(THD *thd, SELECT_LEX *sel,
		       enum enum_schema_tables schema_table_idx)
{
  ST_SCHEMA_TABLE *schema_table= get_schema_table(schema_table_idx);
  LEX_STRING db, table;
  DBUG_ENTER("mysql_schema_select");
  /* 
     We have to make non const db_name & table_name
     because of lower_case_table_names
  */
  make_lex_string(thd, &db, information_schema_name.str,
                  information_schema_name.length, 0);
  make_lex_string(thd, &table, schema_table->table_name,
                  strlen(schema_table->table_name), 0);
  if (!sel->item_list.elements &&          /* Handle old syntax */
      schema_table->old_format(thd, schema_table) ||
      !sel->add_table_to_list(thd, new Table_ident(thd, db, table, 0),
                              0, 0, TL_READ, (List<String> *) 0,
                              (List<String> *) 0))
  {
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Fill temporaty schema tables before SELECT

  SYNOPSIS
    get_schema_tables_result()
    join  join which use schema tables

  RETURN
    0	success
    1   error
*/

int get_schema_tables_result(JOIN *join)
{
  DBUG_ENTER("get_schema_tables_result");
  JOIN_TAB *tmp_join_tab= join->join_tab+join->tables;
  THD *thd= join->thd;
  for (JOIN_TAB *tab= join->join_tab; tab < tmp_join_tab; tab++)
  {  
    if (!tab->table || !tab->table->pos_in_table_list)
      break;
    TABLE_LIST *table_list= tab->table->pos_in_table_list;
    if (table_list->schema_table && !thd->only_prepare())
    {
      TABLE *old_derived_tables= thd->derived_tables;
      thd->derived_tables= 0;
      thd->lex->sql_command= SQLCOM_SHOW_FIELDS;
      table_list->table->file->records= 0;
      MYSQL_LOCK *sql_lock= thd->lock;
      thd->lock=0;
      if (table_list->schema_table->fill_table(thd, table_list,
                                               tab->select_cond))
      {
        thd->derived_tables= old_derived_tables;
        thd->lock= sql_lock;
        DBUG_RETURN(-1);
      }
      thd->lock= sql_lock;
      thd->lex->sql_command= SQLCOM_SELECT;
      thd->derived_tables= old_derived_tables;
    }
  }
  DBUG_RETURN(0);
}


ST_FIELD_INFO schema_fields_info[]=
{
  {"CATALOG_NAME", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"SCHEMA_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Database"},
  {"DEFAULT_CHARACTER_SET_NAME", 60, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO tables_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA",NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Name"},
  {"TABLE_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"ENGINE", NAME_LEN, MYSQL_TYPE_STRING, 0, 1, 1, "Engine"},
  {"VERSION", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Version"},
  {"ROW_FORMAT", 10, MYSQL_TYPE_STRING, 0, 1, 1, "Row_format"},
  {"ROWS", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Rows"},
  {"AVG_ROW_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Avg_row_length"},
  {"DATA_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Data_length"},
  {"MAX_DATA_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Max_data_length"},
  {"INDEX_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Index_length"},
  {"DATA_FREE", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Data_free"},
  {"AUTO_INCREMENT", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Auto_increment"},
  {"CREATE_TIME", 0, MYSQL_TYPE_TIMESTAMP, 0, 1, 0, "Create_time"},
  {"UPDATE_TIME", 0, MYSQL_TYPE_TIMESTAMP, 0, 1, 0, "Update_time"},
  {"CHECK_TIME", 0, MYSQL_TYPE_TIMESTAMP, 0, 1, 0, "Check_time"},
  {"COLLATION", 60, MYSQL_TYPE_STRING, 0, 1, 1, "Collation"},
  {"CHECKSUM", 21 , MYSQL_TYPE_LONG, 0, 1, 0, "Checksum"},
  {"CREATE_OPTIONS", 255, MYSQL_TYPE_STRING, 0, 1, 1, "Create_options"},
  {"COMMENT", 80, MYSQL_TYPE_STRING, 0, 0, 1, "Comment"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO columns_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"COLUMN_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Field"},
  {"ORDINAL_POSITION", 21 , MYSQL_TYPE_LONG, 0, 0, 0, 0},
  {"DATA_TYPE", 40, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"CHARACTER_MAXIMUM_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 0, 0, 0},
  {"CHARACTER_OCTET_LENGTH", 21 , MYSQL_TYPE_LONG, 0, 0, 0, 0},
  {"NUMERIC_PRECISION", 21 , MYSQL_TYPE_LONG, 0, 0, 0, 0},
  {"NUMERIC_SCALE", 21 , MYSQL_TYPE_LONG, 0, 0, 0, 0},
  {"CHARACTER_SET_NAME", 40, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TYPE", 40, MYSQL_TYPE_STRING, 0, 0, 1, "Type"},
  {"COLLATION_NAME", 40, MYSQL_TYPE_STRING, 0, 1, 1, "Collation"},
  {"IS_NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, "Null"},
  {"KEY", 3, MYSQL_TYPE_STRING, 0, 0, 1, "Key"},
  {"COLUMN_DEFAULT", NAME_LEN, MYSQL_TYPE_STRING, 0, 1, 1, "Default"},
  {"EXTRA", 20, MYSQL_TYPE_STRING, 0, 0, 1, "Extra"},
  {"PRIVILIGES", 80, MYSQL_TYPE_STRING, 0, 0, 1, "Privileges"},
  {"COMMENT", 255, MYSQL_TYPE_STRING, 0, 0, 1, "Comment"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO charsets_fields_info[]=
{
  {"CHARACTER_SET_NAME", 30, MYSQL_TYPE_STRING, 0, 0, 1, "Charset"},
  {"Description", 60, MYSQL_TYPE_STRING, 0, 0, 1, "Description"},
  {"DEFAULT_COLLATE_NAME", 60, MYSQL_TYPE_STRING, 0, 0, 1, "Default collation"},
  {"Maxlen", 3 ,MYSQL_TYPE_LONG, 0, 0, 0, "Maxlen"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO collation_fields_info[]=
{
  {"COLLATION_NAME", 30, MYSQL_TYPE_STRING, 0, 0, 1, "Collation"},
  {"Charset", 30, MYSQL_TYPE_STRING, 0, 0, 1, "Charset"},
  {"Id", 11, MYSQL_TYPE_LONG, 0, 0, 0, "Id"},
  {"Default", 30 ,MYSQL_TYPE_STRING, 0, 0, 1, "Default"},
  {"Compiled", 30 ,MYSQL_TYPE_STRING, 0, 0, 1, "Compiled"},
  {"Sortlen", 3 ,MYSQL_TYPE_LONG, 0, 0, 0, "Sortlen"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO coll_charset_app_fields_info[]=
{
  {"COLLATION_NAME", 30, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"CHARACTER_SET_NAME", 30, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO proc_fields_info[]=
{
  {"SPECIFIC_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"ROUTINE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"ROUTINE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Db"},
  {"ROUTINE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Name"},
  {"ROUTINE_TYPE", 9, MYSQL_TYPE_STRING, 0, 0, 1, "Type"},
  {"DEFINER", 77, MYSQL_TYPE_STRING, 0, 0, 1, "Definer"},
  {"DTD_IDENTIFIER", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"ROUTINE_BODY", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"ROUTINE_DEFINITION", 65535, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"PARAMETER_STYLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"IS_DETERMINISTIC", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"SQL_DATA_ACCESS", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"SQL_PATH", NAME_LEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"LAST_ALTERED", 0, MYSQL_TYPE_TIMESTAMP, 0, 0, 0, "Modified"},
  {"CREATED", 0, MYSQL_TYPE_TIMESTAMP, 0, 0, 1, "Created"},
  {"SECURITY_TYPE", 7, MYSQL_TYPE_STRING, 0, 0, 1, "Security_type"},
  {"SQL_MODE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"ROUTINE_COMMENT", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Comment"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO stat_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Table"},
  {"NON_UNIQUE", 1, MYSQL_TYPE_LONG, 0, 0, 0, "Non_unique"},
  {"INDEX_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"INDEX_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Key_name"},
  {"SEQ_IN_INDEX", 2, MYSQL_TYPE_LONG, 0, 0, 0, "Seq_in_index"},
  {"COLUMN_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Column_name"},
  {"COLLATION", 1, MYSQL_TYPE_STRING, 0, 1, 1, "Collation"},
  {"CARDINALITY", 21, MYSQL_TYPE_LONG, 0, 1, 0, "Cardinality"},
  {"SUB_PART", 3, MYSQL_TYPE_LONG, 0, 1, 0, "Sub_part"},
  {"PACKED", 10, MYSQL_TYPE_STRING, 0, 1, 1, "Packed"},
  {"NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, "Null"},
  {"INDEX_TYPE", 16, MYSQL_TYPE_STRING, 0, 0, 1, "Index_type"},
  {"COMMENT", 16, MYSQL_TYPE_STRING, 0, 1, 1, "Comment"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO view_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"VIEW_DEFINITION", 65535, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"CHECK_OPTION", 4, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"IS_UPDATABLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO user_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"PRIVILEGE_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO schema_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"PRIVILEGE_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO table_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"PRIVILEGE_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO column_privileges_fields_info[]=
{
  {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"COLUMN_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"PRIVILEGE_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO table_constraints_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"CONSTRAINT_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"CONSTRAINT_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"CONSTRAINT_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"CONSTRAINT_METHOD", 20, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO key_column_usage_fields_info[]=
{
  {"CONSTRAINT_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"CONSTRAINT_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"CONSTRAINT_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"COLUMN_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"ORDINAL_POSITION", 10 ,MYSQL_TYPE_LONG, 0, 0, 0, 0},
  {"REFERENCED_TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"REFERENCED_TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"REFERENCED_COLUMN_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


ST_FIELD_INFO table_names_fields_info[]=
{
  {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 1, 1, 0},
  {"TABLE_SCHEMA",NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, 0},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Tables_in_"},
  {"TABLE_TYPE", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 1, "Table_type"},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};


/*
  Description of ST_FIELD_INFO in table.h
*/

ST_SCHEMA_TABLE schema_tables[]=
{
  {"SCHEMATA", schema_fields_info, create_schema_table,
   fill_schema_shemata, make_schemata_old_format, 0, 1, -1},
  {"TABLES", tables_fields_info, create_schema_table, 
   get_all_tables, make_old_format, get_schema_tables_record, 1, 2},
  {"COLUMNS", columns_fields_info, create_schema_table, 
   get_all_tables, make_columns_old_format, get_schema_column_record, 1, 2},
  {"CHARACTER_SETS", charsets_fields_info, create_schema_table, 
   fill_schema_charsets, make_old_format, 0, -1, -1},
  {"COLLATIONS", collation_fields_info, create_schema_table, 
   fill_schema_collation, make_old_format, 0, -1, -1},
  {"COLLATION_CHARACTER_SET_APPLICABILITY", coll_charset_app_fields_info,
   create_schema_table, fill_schema_coll_charset_app, 0, 0, -1, -1},
  {"ROUTINES", proc_fields_info, create_schema_table, 
    fill_schema_proc, make_old_format, 0, -1, -1},
  {"STATISTICS", stat_fields_info, create_schema_table, 
    get_all_tables, make_old_format, get_schema_stat_record, 1, 2},
  {"VIEWS", view_fields_info, create_schema_table, 
    get_all_tables, 0, get_schema_views_record, 1, 2},
  {"USER_PRIVILEGES", user_privileges_fields_info, create_schema_table, 
    fill_schema_user_privileges, 0, 0, -1, -1},
  {"SCHEMA_PRIVILEGES", schema_privileges_fields_info, create_schema_table,
    fill_schema_schema_privileges, 0, 0, -1, -1},
  {"TABLE_PRIVILEGES", table_privileges_fields_info, create_schema_table,
    fill_schema_table_privileges, 0, 0, -1, -1},
  {"COLUMN_PRIVILEGES", column_privileges_fields_info, create_schema_table,
    fill_schema_column_privileges, 0, 0, -1, -1},
  {"TABLE_CONSTRAINTS", table_constraints_fields_info, create_schema_table,
    get_all_tables, 0, get_schema_constarints_record, 3, 4},
  {"KEY_COLUMN_USAGE", key_column_usage_fields_info, create_schema_table,
    get_all_tables, 0, get_schema_key_column_usage_record, 3, 4},
  {"TABLE_NAMES", table_names_fields_info, create_schema_table,
   get_all_tables, make_table_names_old_format, 0, 1, 2},
  {0, 0, 0, 0, 0, 0, 0, 0}
};


#ifdef __GNUC__
template class List_iterator_fast<char>;
template class List<char>;
#endif
