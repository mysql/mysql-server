/* Copyright (C) 2000 MySQL AB

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
                               grant_names};
#endif

static int
store_create_info(THD *thd, TABLE *table, String *packet);


/*
  Report list of databases
  A database is a directory in the mysql_data_home directory
*/

int
mysqld_show_dbs(THD *thd,const char *wild)
{
  Item_string *field=new Item_string("",0,thd->charset());
  List<Item> field_list;
  char *end;
  List<char> files;
  char *file_name;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_dbs");

  field->name=(char*) thd->alloc(20+ (wild ? (uint) strlen(wild)+4: 0));
  field->max_length=NAME_LEN;
  end=strmov(field->name,"Database");
  if (wild && wild[0])
    strxmov(end," (",wild,")",NullS);
  field_list.push_back(field);

  if (protocol->send_fields(&field_list,1))
    DBUG_RETURN(1);
  if (mysql_find_files(thd,&files,NullS,mysql_data_home,wild,1))
    DBUG_RETURN(1);
  List_iterator_fast<char> it(files);

  while ((file_name=it++))
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (thd->master_access & (DB_ACLS | SHOW_DB_ACL) ||
	acl_get(thd->host, thd->ip, thd->priv_user, file_name,0) ||
	(grant_option && !check_grant_db(thd, file_name)))
#endif
    {
      protocol->prepare_for_resend();
      protocol->store(file_name, system_charset_info);
      if (protocol->write())
	DBUG_RETURN(-1);
    }
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


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

  if (protocol->send_fields(&field_list,1))
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
** List all tables in a database (fast version)
** A table is a .frm file in the current databasedir
***************************************************************************/

int mysqld_show_tables(THD *thd,const char *db,const char *wild)
{
  Item_string *field=new Item_string("",0,thd->charset());
  List<Item> field_list;
  char path[FN_LEN],*end;
  List<char> files;
  char *file_name;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_tables");

  field->name=(char*) thd->alloc(20+(uint) strlen(db)+
				 (wild ? (uint) strlen(wild)+4:0));
  end=strxmov(field->name,"Tables_in_",db,NullS);
  if (wild && wild[0])
    strxmov(end," (",wild,")",NullS);
  field->max_length=NAME_LEN;
  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  (void) unpack_dirname(path,path);
  field_list.push_back(field);
  if (protocol->send_fields(&field_list,1))
    DBUG_RETURN(1);
  if (mysql_find_files(thd,&files,db,path,wild,0))
    DBUG_RETURN(-1);
  List_iterator_fast<char> it(files);
  while ((file_name=it++))
  {
    protocol->prepare_for_resend();
    protocol->store(file_name, system_charset_info);
    if (protocol->write())
      DBUG_RETURN(-1);
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

  if (protocol->send_fields(&field_list,1))
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
  {"Create temporary tables","Databases","To use CREATE TEMPORARY TABLE"},
  {"Create", "Databases,Tables,Indexes",  "To create new databases and tables"},
  {"Delete", "Tables",  "To delete existing rows"},
  {"Drop", "Databases,Tables", "To drop databases and tables"},
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
  {"Shutdown","Server Admin", "To shutdown the server"},
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

  if (protocol->send_fields(&field_list,1))
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

  if (protocol->send_fields(&field_list,1))
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
      if (my_use_symdir && !strcmp(ext=fn_ext(file->name), ".sym"))
      {
	/* Only show the sym file if it points to a directory */
	char buff[FN_REFLEN], *end;
	MY_STAT status;
        *ext=0;                                 /* Remove extension */
	unpack_dirname(buff, file->name);
	end= strend(buff);
	if (end != buff && end[-1] == FN_LIBCHAR)
	  end[-1]= 0;				// Remove end FN_LIBCHAR
	if (!my_stat(buff, &status, MYF(0)) ||
	    !MY_S_ISDIR(status.st_mode))
	  continue;
      }
      else
#endif
      {
        if (file->name[0] == '.' || !MY_S_ISDIR(file->mystat->st_mode) ||
            (wild && wild_compare(file->name,wild,0)))
          continue;
      }
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
 Extended version of mysqld_show_tables
***************************************************************************/

int mysqld_extend_show_tables(THD *thd,const char *db,const char *wild)
{
  Item *item;
  List<char> files;
  List<Item> field_list;
  char path[FN_LEN];
  char *file_name;
  TABLE *table;
  Protocol *protocol= thd->protocol;
  TIME time;
  DBUG_ENTER("mysqld_extend_show_tables");

  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  (void) unpack_dirname(path,path);
  field_list.push_back(item=new Item_empty_string("Name",NAME_LEN));
  field_list.push_back(item=new Item_empty_string("Engine",10));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Version", (longlong) 0, 21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Row_format",10));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Rows",(longlong) 1,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Avg_row_length",(int32) 0,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Data_length",(longlong) 1,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Max_data_length",(longlong) 1,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Index_length",(longlong) 1,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Data_free",(longlong) 1,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Auto_increment",(longlong) 1,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_datetime("Create_time"));
  item->maybe_null=1;
  field_list.push_back(item=new Item_datetime("Update_time"));
  item->maybe_null=1;
  field_list.push_back(item=new Item_datetime("Check_time"));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Collation",32));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Checksum",(longlong) 1,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Create_options",255));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Comment",80));
  item->maybe_null=1;
  if (protocol->send_fields(&field_list,1))
    DBUG_RETURN(1);

  if (mysql_find_files(thd,&files,db,path,wild,0))
    DBUG_RETURN(-1);
  List_iterator_fast<char> it(files);
  while ((file_name=it++))
  {
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    protocol->prepare_for_resend();
    protocol->store(file_name, system_charset_info);
    table_list.db=(char*) db;
    table_list.real_name= table_list.alias= file_name;
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, file_name);
    if (!(table = open_ltable(thd, &table_list, TL_READ)))
    {
      for (uint i=2 ; i < field_list.elements ; i++)
        protocol->store_null();
      // Send error to Comment field
      protocol->store(thd->net.last_error, system_charset_info);
      thd->clear_error();
    }
    else
    {
      const char *str;
      handler *file=table->file;
      file->info(HA_STATUS_VARIABLE | HA_STATUS_TIME | HA_STATUS_NO_LOCK);
      protocol->store(file->table_type(), system_charset_info);
      protocol->store((ulonglong) table->frm_version);
      str= ((table->db_options_in_use & HA_OPTION_COMPRESS_RECORD) ?
	    "Compressed" :
	    (table->db_options_in_use & HA_OPTION_PACK_RECORD) ?
	    "Dynamic" : "Fixed");
      protocol->store(str, system_charset_info);
      protocol->store((ulonglong) file->records);
      protocol->store((ulonglong) file->mean_rec_length);
      protocol->store((ulonglong) file->data_file_length);
      if (file->max_data_file_length)
        protocol->store((ulonglong) file->max_data_file_length);
      else
        protocol->store_null();
      protocol->store((ulonglong) file->index_file_length);
      protocol->store((ulonglong) file->delete_length);
      if (table->found_next_number_field)
      {
        table->next_number_field=table->found_next_number_field;
        table->next_number_field->reset();
        file->update_auto_increment();
        protocol->store(table->next_number_field->val_int());
        table->next_number_field=0;
      }
      else
        protocol->store_null();
      if (!file->create_time)
        protocol->store_null();
      else
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time, file->create_time);
        protocol->store(&time);
      }
      if (!file->update_time)
        protocol->store_null();
      else
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time, file->update_time);
        protocol->store(&time);
      }
      if (!file->check_time)
        protocol->store_null();
      else
      {
        thd->variables.time_zone->gmt_sec_to_TIME(&time, file->check_time);
        protocol->store(&time);
      }
      str= (table->table_charset ? table->table_charset->name : "default");
      protocol->store(str, system_charset_info);
      if (file->table_flags() & HA_HAS_CHECKSUM)
        protocol->store((ulonglong)file->checksum());
      else
        protocol->store_null(); // Checksum
      {
        char option_buff[350],*ptr;
        ptr=option_buff;
        if (table->min_rows)
        {
          ptr=strmov(ptr," min_rows=");
          ptr=longlong10_to_str(table->min_rows,ptr,10);
        }
        if (table->max_rows)
        {
          ptr=strmov(ptr," max_rows=");
          ptr=longlong10_to_str(table->max_rows,ptr,10);
        }
        if (table->avg_row_length)
        {
          ptr=strmov(ptr," avg_row_length=");
          ptr=longlong10_to_str(table->avg_row_length,ptr,10);
        }
        if (table->db_create_options & HA_OPTION_PACK_KEYS)
          ptr=strmov(ptr," pack_keys=1");
        if (table->db_create_options & HA_OPTION_NO_PACK_KEYS)
          ptr=strmov(ptr," pack_keys=0");
        if (table->db_create_options & HA_OPTION_CHECKSUM)
          ptr=strmov(ptr," checksum=1");
        if (table->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
          ptr=strmov(ptr," delay_key_write=1");
        if (table->row_type != ROW_TYPE_DEFAULT)
          ptr=strxmov(ptr, " row_format=", ha_row_type[(uint) table->row_type],
                      NullS);
        if (file->raid_type)
        {
          char buff[100];
          sprintf(buff," raid_type=%s raid_chunks=%d raid_chunksize=%ld",
                  my_raid_type(file->raid_type), file->raid_chunks, file->raid_chunksize/RAID_BLOCK_SIZE);
          ptr=strmov(ptr,buff);
        }
        protocol->store(option_buff+1,
			(ptr == option_buff ? 0 : (uint) (ptr-option_buff)-1)
			, system_charset_info);
      }
      {
	char *comment=table->file->update_table_comment(table->comment);
	protocol->store(comment, system_charset_info);
	if (comment != table->comment)
	  my_free(comment,MYF(0));
      }
      close_thread_tables(thd,0);
    }
    if (protocol->write())
      DBUG_RETURN(-1);
  }
  send_eof(thd);
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
  DBUG_ENTER("mysqld_show_fields");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
                      table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(thd);
    DBUG_RETURN(1);
  }
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
      protocol->send_fields(&field_list,0))
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
          Altough TIMESTAMP fields can't contain NULL as its value they
          will accept NULL if you will try to insert such value and will
          convert it to current TIMESTAMP. So YES here means that NULL 
          is allowed for assignment but can't be returned.
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
            because it is more SQL standard comatible
          */
          protocol->store("CURRENT_TIMESTAMP", system_charset_info);
        }
        else if (field->unireg_check != Field::NEXT_NUMBER &&
                 !field->is_null())
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
          def.copy(type.ptr(), type.length(), type.charset(), 
                   system_charset_info);
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
	  col_access= get_column_grant(thd,table_list,field) & COL_ACLS;
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
  DBUG_ENTER("mysqld_show_create");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
                      table_list->real_name));

  /* Only one table for now */
  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(thd);
    DBUG_RETURN(1);
  }

  if (store_create_info(thd, table, &buffer))
    DBUG_RETURN(-1);

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Table",NAME_LEN));
  // 1024 is for not to confuse old clients
  field_list.push_back(new Item_empty_string("Create Table",
					     max(buffer.length(),1024)));

  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(1);
  protocol->prepare_for_resend();
  protocol->store(table->table_name, system_charset_info);
  buffer.length(0);
  if (store_create_info(thd, table, &buffer))
    DBUG_RETURN(-1);
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

  if (protocol->send_fields(&field_list,1))
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

  if (protocol->send_fields(&field_list,1))
    DBUG_RETURN(1);

#ifdef HAVE_BERKELEY_DB
  if ((have_berkeley_db == SHOW_OPTION_YES) && berkeley_show_logs(protocol))
    DBUG_RETURN(-1);
#endif

  send_eof(thd);
  DBUG_RETURN(0);
}


int
mysqld_show_keys(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysqld_show_keys");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
                      table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(thd);
    DBUG_RETURN(1);
  }

  List<Item> field_list;
  Item *item;
  field_list.push_back(new Item_empty_string("Table",NAME_LEN));
  field_list.push_back(new Item_return_int("Non_unique",1, MYSQL_TYPE_TINY));
  field_list.push_back(new Item_empty_string("Key_name",NAME_LEN));
  field_list.push_back(new Item_return_int("Seq_in_index",2, MYSQL_TYPE_TINY));
  field_list.push_back(new Item_empty_string("Column_name",NAME_LEN));
  field_list.push_back(item=new Item_empty_string("Collation",1));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Cardinality",0,21));
  item->maybe_null=1;
  field_list.push_back(item=new Item_return_int("Sub_part",3,
						MYSQL_TYPE_TINY));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Packed",10));
  item->maybe_null=1;
  field_list.push_back(new Item_empty_string("Null",3));
  field_list.push_back(new Item_empty_string("Index_type",16));
  field_list.push_back(new Item_empty_string("Comment",255));
  item->maybe_null=1;

  if (protocol->send_fields(&field_list,1))
    DBUG_RETURN(1);

  KEY *key_info=table->key_info;
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK | HA_STATUS_TIME);
  for (uint i=0 ; i < table->keys ; i++,key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    const char *str;
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      protocol->prepare_for_resend();
      protocol->store(table->table_name, system_charset_info);
      protocol->store_tiny((longlong) ((key_info->flags & HA_NOSAME) ? 0 :1));
      protocol->store(key_info->name, system_charset_info);
      protocol->store_tiny((longlong) (j+1));
      str=(key_part->field ? key_part->field->field_name :
	   "?unknown field?");
      protocol->store(str, system_charset_info);
      if (table->file->index_flags(i, j, 0) & HA_READ_ORDER)
        protocol->store(((key_part->key_part_flag & HA_REVERSE_SORT) ?
			 "D" : "A"), 1, system_charset_info);
      else
        protocol->store_null(); /* purecov: inspected */
      KEY *key=table->key_info+i;
      if (key->rec_per_key[j])
      {
        ha_rows records=(table->file->records / key->rec_per_key[j]);
        protocol->store((ulonglong) records);
      }
      else
        protocol->store_null();

      /* Check if we have a key part that only uses part of the field */
      if (!(key_info->flags & HA_FULLTEXT) && (!key_part->field ||
          key_part->length != table->field[key_part->fieldnr-1]->key_length()))
        protocol->store_tiny((longlong) key_part->length);
      else
        protocol->store_null();
      protocol->store_null();                   // No pack_information yet

      /* Null flag */
      uint flags= key_part->field ? key_part->field->flags : 0;
      char *pos=(char*) ((flags & NOT_NULL_FLAG) ? "" : "YES");
      protocol->store((const char*) pos, system_charset_info);
      protocol->store(table->file->index_type(i), system_charset_info);
      /* Comment */
      if (!table->keys_in_use.is_set(i))
	protocol->store("disabled",8, system_charset_info);
      else
        protocol->store("", 0, system_charset_info);
      if (protocol->write())
        DBUG_RETURN(1); /* purecov: inspected */
    }
  }
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
  DBUG_ENTER("mysqld_list_fields");
  DBUG_PRINT("enter",("table: %s",table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(thd);
    DBUG_VOID_RETURN;
  }
  List<Item> field_list;

  Field **ptr,*field;
  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    if (!wild || !wild[0] || 
        !wild_case_compare(system_charset_info, field->field_name,wild))
      field_list.push_back(new Item_field(field));
  }
  restore_record(table,default_values);              // Get empty record
  if (thd->protocol->send_fields(&field_list,2))
    DBUG_VOID_RETURN;
  net_flush(&thd->net);
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

  //if (protocol->convert)
  //  protocol->convert->convert((char*) packet->ptr(), packet->length());
  if (fd < 0)
  {
    if (protocol->write())
      DBUG_RETURN(-1);
    net_flush(&thd->net);
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
  parse it as an identifer.

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


static void append_quoted_simple_identifier(String *packet, char quote_char,
					    const char *name, uint length)
{
  packet->append(&quote_char, 1, system_charset_info);
  packet->append(name, length, system_charset_info);
  packet->append(&quote_char, 1, system_charset_info);
}  


void
append_identifier(THD *thd, String *packet, const char *name, uint length)
{
  const char *name_end;
  char quote_char;

  if (thd->variables.sql_mode & MODE_ANSI_QUOTES)
    quote_char= '\"';
  else
    quote_char= '`';

  if (is_keyword(name,length))
  {
    append_quoted_simple_identifier(packet, quote_char, name, length);
    return;
  }

  if (!require_quotes(name, length))
  {
    if (!(thd->options & OPTION_QUOTE_SHOW_CREATE))
      packet->append(name, length, system_charset_info);
    else
      append_quoted_simple_identifier(packet, quote_char, name, length);
    return;
  }

  /* The identifier must be quoted as it includes a quote character */

  packet->reserve(length*2 + 2);
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end= name+length ; name < name_end ; name+= length)
  {
    char chr= *name;
    length= my_mbcharlen(system_charset_info, chr);
    if (length == 1 && chr == quote_char)
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(name, length, packet->charset());
  }
  packet->append(&quote_char, 1, system_charset_info);
}


/* Append directory name (if exists) to CREATE INFO */

static void append_directory(THD *thd, String *packet, const char *dir_type,
			     const char *filename)
{
  uint length;
  if (filename && !(thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE))
  {
    length= dirname_length(filename);
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


    /* 
      Again we are using CURRENT_TIMESTAMP instead of NOW because it is
      more standard 
    */
    has_now_default= table->timestamp_field == field && 
                     field->unireg_check != Field::TIMESTAMP_UN_FIELD;
    
    has_default= (field->type() != FIELD_TYPE_BLOB &&
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
	  /* convert to system_charset_info == utf8 */
	  def_val.copy(type.ptr(), type.length(), field->charset(),
		       system_charset_info);
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
  if (protocol->send_fields(&field_list,1))
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
        thd_info->proc_info= (char*) (tmp->killed ? "Killed" : 0);
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

static bool write_collation(Protocol *protocol, CHARSET_INFO *cs)
{
  protocol->prepare_for_resend();
  protocol->store(cs->name, system_charset_info);
  protocol->store(cs->csname, system_charset_info);
  protocol->store_short((longlong) cs->number);
  protocol->store((cs->state & MY_CS_PRIMARY) ? "Yes" : "",system_charset_info);
  protocol->store((cs->state & MY_CS_COMPILED)? "Yes" : "",system_charset_info);
  protocol->store_short((longlong) cs->strxfrm_multiply);
  return protocol->write();
}

int mysqld_show_collations(THD *thd, const char *wild)
{
  char buff[8192];
  String packet2(buff,sizeof(buff),thd->charset());
  List<Item> field_list;
  CHARSET_INFO **cs;
  Protocol *protocol= thd->protocol;

  DBUG_ENTER("mysqld_show_charsets");

  field_list.push_back(new Item_empty_string("Collation",30));
  field_list.push_back(new Item_empty_string("Charset",30));
  field_list.push_back(new Item_return_int("Id",11, FIELD_TYPE_SHORT));
  field_list.push_back(new Item_empty_string("Default",30));
  field_list.push_back(new Item_empty_string("Compiled",30));
  field_list.push_back(new Item_return_int("Sortlen",3, FIELD_TYPE_SHORT));

  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(1);

  for ( cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    if (!cs[0] || !(cs[0]->state & MY_CS_AVAILABLE) || 
        !(cs[0]->state & MY_CS_PRIMARY))
      continue;
    for ( cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      if (!cl[0] || !(cl[0]->state & MY_CS_AVAILABLE) || 
          !my_charset_same(cs[0],cl[0]))
	continue;
      if (!(wild && wild[0] &&
	  wild_case_compare(system_charset_info,cl[0]->name,wild)))
      {
        if (write_collation(protocol, cl[0]))
	  goto err;
      }
    }
  }
  send_eof(thd); 
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

static bool write_charset(Protocol *protocol, CHARSET_INFO *cs)
{
  protocol->prepare_for_resend();
  protocol->store(cs->csname, system_charset_info);
  protocol->store(cs->comment ? cs->comment : "", system_charset_info);
  protocol->store(cs->name, system_charset_info);
  protocol->store_short((longlong) cs->mbmaxlen);
  return protocol->write();
}

int mysqld_show_charsets(THD *thd, const char *wild)
{
  char buff[8192];
  String packet2(buff,sizeof(buff),thd->charset());
  List<Item> field_list;
  CHARSET_INFO **cs;
  Protocol *protocol= thd->protocol;

  DBUG_ENTER("mysqld_show_charsets");

  field_list.push_back(new Item_empty_string("Charset",30));
  field_list.push_back(new Item_empty_string("Description",60));
  field_list.push_back(new Item_empty_string("Default collation",60));
  field_list.push_back(new Item_return_int("Maxlen",3, FIELD_TYPE_SHORT));

  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(1);

  for ( cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    if (cs[0] && (cs[0]->state & MY_CS_PRIMARY) && 
        (cs[0]->state & MY_CS_AVAILABLE) &&
        !(wild && wild[0] &&
        wild_case_compare(system_charset_info,cs[0]->csname,wild)))
    {
      if (write_charset(protocol, cs[0]))
	goto err;
    }
  }
  send_eof(thd); 
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

  

int mysqld_show(THD *thd, const char *wild, show_var_st *variables,
		enum enum_var_type value_type,
		pthread_mutex_t *mutex)
{
  char buff[1024];
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  LEX_STRING null_lex_str;
  DBUG_ENTER("mysqld_show");

  field_list.push_back(new Item_empty_string("Variable_name",30));
  field_list.push_back(new Item_empty_string("Value",256));
  if (protocol->send_fields(&field_list,1))
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

#ifdef __GNUC__
template class List_iterator_fast<char>;
template class List<char>;
#endif
