/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

#undef USE_RAID
#define USE_RAID
#include "mysql_priv.h"
#include "sql_select.h"				// For select_describe
#include "sql_acl.h"
#include <my_dir.h>
extern "C" pthread_mutex_t THR_LOCK_keycache;

static const char *grant_names[]={
  "select","insert","update","delete","create","drop","reload","shutdown",
  "process","file","grant","references","index","alter"};

static TYPELIB grant_types = { sizeof(grant_names)/sizeof(char **),
			       "grant_types",
			       grant_names};

static int mysql_find_files(THD *thd,List<char> *files, const char *db,
			    const char *path, const char *wild, bool dir);

static int
store_create_info(THD *thd, TABLE *table, String* packet);

/****************************************************************************
** Send list of databases
** A database is a directory in the mysql_data_home directory
****************************************************************************/


int
mysqld_show_dbs(THD *thd,const char *wild)
{
  Item_string *field=new Item_string("",0);
  List<Item> field_list;
  char *end;
  List<char> files;
  char *file_name;
  DBUG_ENTER("mysqld_show_dbs");

  field->name=(char*) thd->alloc(20+ (wild ? (uint) strlen(wild)+4: 0));
  field->max_length=NAME_LEN;
  end=strmov(field->name,"Database");
  if (wild && wild[0])
    strxmov(end," (",wild,")",NullS);
  field_list.push_back(field);

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);
  if (mysql_find_files(thd,&files,NullS,mysql_data_home,wild,1))
    DBUG_RETURN(1);
  List_iterator<char> it(files);
  while ((file_name=it++))
  {
    thd->packet.length(0);
    net_store_data(&thd->packet,file_name);
    if (my_net_write(&thd->net,(char*) thd->packet.ptr(),thd->packet.length()))
      DBUG_RETURN(-1);
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}

/***************************************************************************
** List all tables in a database (fast version)
** A table is a .frm file in the current databasedir
***************************************************************************/

int mysqld_show_tables(THD *thd,const char *db,const char *wild)
{
  Item_string *field=new Item_string("",0);
  List<Item> field_list;
  char path[FN_LEN],*end;
  List<char> files;
  char *file_name;
  DBUG_ENTER("mysqld_show_tables");

  field->name=(char*) thd->alloc(20+(uint) strlen(db)+(wild ? (uint) strlen(wild)+4:0));
  end=strxmov(field->name,"Tables_in_",db,NullS);
  if (wild && wild[0])
    strxmov(end," (",wild,")",NullS);
  field->max_length=NAME_LEN;
  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  (void) unpack_dirname(path,path);
  field_list.push_back(field);
  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);
  if (mysql_find_files(thd,&files,db,path,wild,0))
    DBUG_RETURN(-1);
  List_iterator<char> it(files);
  while ((file_name=it++))
  {
    thd->packet.length(0);
    net_store_data(&thd->packet,file_name);
    if (my_net_write(&thd->net,(char*) thd->packet.ptr(),thd->packet.length()))
      DBUG_RETURN(-1);
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}


static int
mysql_find_files(THD *thd,List<char> *files, const char *db,const char *path,
		 const char *wild, bool dir)
{
  uint i;
  char *ext;
  MY_DIR *dirp;
  FILEINFO *file;
  uint col_access=thd->col_access;
  TABLE_LIST table_list;
  DBUG_ENTER("mysql_find_files");

  bzero((char*) &table_list,sizeof(table_list));

  if (!(dirp = my_dir(path,MYF(MY_WME | (dir ? MY_WANT_STAT : 0)))))
    DBUG_RETURN(-1);

  for (i=0 ; i < (uint) dirp->number_off_files	; i++)
  {
    file=dirp->dir_entry+i;
    if (dir)
    {						/* Return databases */
#ifdef USE_SYMDIR
      char *ext;
      if (my_use_symdir && !strcmp(ext=fn_ext(file->name), ".sym"))
	*ext=0;					/* Remove extension */
      else
#endif
      {
	if (file->name[0] == '.' || !MY_S_ISDIR(file->mystat.st_mode) ||
	    (wild && wild[0] && wild_compare(file->name,wild)))
	  continue;
      }
    }
    else
    {
	// Return only .frm files which isn't temp files.
      if (my_strcasecmp(ext=fn_ext(file->name),reg_ext) ||
	  is_prefix(file->name,tmp_file_prefix))	// Mysql temp table
	continue;
      *ext=0;
      if (wild && wild[0] && wild_compare(file->name,wild))
	continue;
    }
    /* Don't show tables where we don't have any privileges */
    if (db && !(col_access & TABLE_ACLS))
    {
      table_list.db= (char*) db;
      table_list.real_name=file->name;
      table_list.grant.privilege=col_access;
      if (check_grant(thd,TABLE_ACLS,&table_list,1))
	continue;
    }
    if (files->push_back(thd->strdup(file->name)))
    {
      my_dirend(dirp);
      DBUG_RETURN(-1);
    }
  }
  DBUG_PRINT("info",("found: %d files", files->elements));
  my_dirend(dirp);
  DBUG_RETURN(0);
}

/***************************************************************************
** Extended version of mysqld_show_tables
***************************************************************************/

int mysqld_extend_show_tables(THD *thd,const char *db,const char *wild)
{
  Item *item;
  List<char> files;
  List<Item> field_list;
  char path[FN_LEN];
  char *file_name;
  TABLE *table;
  String *packet= &thd->packet;
  DBUG_ENTER("mysqld_extend_show_tables");

  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  (void) unpack_dirname(path,path);

  field_list.push_back(item=new Item_empty_string("Name",NAME_LEN));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Type",10));
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
  field_list.push_back(item=new Item_empty_string("Create_options",255));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Comment",80));
  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  if (mysql_find_files(thd,&files,db,path,wild,0))
    DBUG_RETURN(-1);
  List_iterator<char> it(files);
  while ((file_name=it++))
  {
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    packet->length(0);
    net_store_data(packet,file_name);
    table_list.db=(char*) db;
    table_list.real_name=table_list.name=file_name;
    if (!(table = open_ltable(thd, &table_list, TL_READ)))
    {
      for (uint i=0 ; i < field_list.elements ; i++)
	net_store_null(packet);
      net_store_data(packet,thd->net.last_error);
      thd->net.last_error[0]=0;
    }
    else
    {
      struct tm tm_tmp;
      handler *file=table->file;
      file->info(HA_STATUS_VARIABLE | HA_STATUS_TIME | HA_STATUS_NO_LOCK);
      net_store_data(packet, file->table_type());
      net_store_data(packet,
		     (table->db_options_in_use & HA_OPTION_PACK_RECORD) ?
		     "Dynamic" :
		     (table->db_options_in_use & HA_OPTION_COMPRESS_RECORD)
		     ? "Compressed" : "Fixed");
      net_store_data(packet, (longlong) file->records);
      net_store_data(packet, (uint32) file->mean_rec_length);
      net_store_data(packet, (longlong) file->data_file_length);
      if (file->max_data_file_length)
	net_store_data(packet, (longlong) file->max_data_file_length);
      else
	net_store_null(packet);
      net_store_data(packet, (longlong) file->index_file_length);
      net_store_data(packet, (longlong) file->delete_length);
      if (table->found_next_number_field)
      {
	table->next_number_field=table->found_next_number_field;
	table->next_number_field->reset();
	file->update_auto_increment();
	net_store_data(packet, table->next_number_field->val_int());
	table->next_number_field=0;
      }
      else
	net_store_null(packet);
      if (!file->create_time)
	net_store_null(packet);
      else
      {
	localtime_r(&file->create_time,&tm_tmp);
	net_store_data(packet, &tm_tmp);
      }
      if (!file->update_time)
	net_store_null(packet);
      else
      {
	localtime_r(&file->update_time,&tm_tmp);
	net_store_data(packet, &tm_tmp);
      }
      if (!file->check_time)
	net_store_null(packet);
      else
      {
	localtime_r(&file->check_time,&tm_tmp);
	net_store_data(packet, &tm_tmp);
      }
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
	  ptr=strxmov(ptr, " format=", ha_row_type[(uint) table->row_type],
		      NullS);
	if (file->raid_type)
	{
	  char buff[100];
	  sprintf(buff," raid_type=%s raid_chunks=%d raid_chunksize=%ld",
		  my_raid_type(file->raid_type), file->raid_chunks, file->raid_chunksize/RAID_BLOCK_SIZE);
	  ptr=strmov(ptr,buff);
	}
	net_store_data(packet, option_buff+1,
		       (ptr == option_buff ? 0 : (uint) (ptr-option_buff)-1));
      }
      net_store_data(packet, table->comment);

      close_thread_tables(thd,0);
    }
    if (my_net_write(&thd->net,(char*) packet->ptr(),
		     packet->length()))
      DBUG_RETURN(-1);
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}



/***************************************************************************
** List all columns in a table
***************************************************************************/

int
mysqld_show_fields(THD *thd, TABLE_LIST *table_list,const char *wild)
{
  TABLE *table;
  handler *file;
  char tmp[MAX_FIELD_WIDTH];
  DBUG_ENTER("mysqld_show_fields");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
		      table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(&thd->net);
    DBUG_RETURN(1);
  }
  file=table->file;
  file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  (void) get_table_grant(thd, table_list);

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Field",NAME_LEN));
  field_list.push_back(new Item_empty_string("Type",40));
  field_list.push_back(new Item_empty_string("Null",1));
  field_list.push_back(new Item_empty_string("Key",3));
  field_list.push_back(new Item_empty_string("Default",NAME_LEN));
  field_list.push_back(new Item_empty_string("Extra",20));
  field_list.push_back(new Item_empty_string("Privileges",80));

	// Send first number of fields and records
  {
    char *pos;
    pos=net_store_length(tmp, (uint) field_list.elements);
    pos=net_store_length(pos,(ulonglong) file->records);
    (void) my_net_write(&thd->net,tmp,(uint) (pos-tmp));
  }

  if (send_fields(thd,field_list,0))
    DBUG_RETURN(1);
  restore_record(table,2);	// Get empty record

  Field **ptr,*field;
  for (ptr=table->field; (field= *ptr) ; ptr++)
  {
    if (!wild || !wild[0] || !wild_case_compare(field->field_name,wild))
    {
#ifdef NOT_USED
      if (thd->col_access & TABLE_ACLS ||
	  ! check_grant_column(thd,table,field->field_name,
			       (uint) strlen(field->field_name),1))
#endif
      {
	byte *pos;
	uint flags=field->flags;
	String *packet= &thd->packet,type(tmp,sizeof(tmp));
	uint col_access;
	bool null_default_value=0;

	packet->length(0);
	net_store_data(packet,field->field_name);
	field->sql_type(type);
	net_store_data(packet,type.ptr(),type.length());

	pos=(byte*) ((flags & NOT_NULL_FLAG) &&
		     field->type() != FIELD_TYPE_TIMESTAMP ?
		     "" : "YES");
	net_store_data(packet,(const char*) pos);
	pos=(byte*) ((field->flags & PRI_KEY_FLAG) ? "PRI" :
		     (field->flags & UNIQUE_KEY_FLAG) ? "UNI" :
		     (field->flags & MULTIPLE_KEY_FLAG) ? "MUL":"");
	net_store_data(packet,(char*) pos);

	if (field->type() == FIELD_TYPE_TIMESTAMP ||
	    field->unireg_check == Field::NEXT_NUMBER)
	  null_default_value=1;
	if (!null_default_value && !field->is_null())
	{						// Not null by default
	  type.set(tmp,sizeof(tmp));
	  field->val_str(&type,&type);
	  net_store_data(packet,type.ptr(),type.length());
	}
	else if (field->maybe_null() || null_default_value)
	  net_store_null(packet);			// Null as default
	else
	  net_store_data(packet,tmp,0);

	char *end=tmp;
	if (field->unireg_check == Field::NEXT_NUMBER)
	  end=strmov(tmp,"auto_increment");
	net_store_data(packet,tmp,(uint) (end-tmp));

	/* Add grant options */
	col_access= get_column_grant(thd,table_list,field) & COL_ACLS;
	end=tmp;
	for (uint bitnr=0; col_access ; col_access>>=1,bitnr++)
	{
	  if (col_access & 1)
	  {
	    *end++=',';
	    end=strmov(end,grant_types.type_names[bitnr]);
	  }
	}
	net_store_data(packet,tmp+1,end == tmp ? 0 : (uint) (end-tmp-1));
	if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
	  DBUG_RETURN(1);
      }
    }
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}

int
mysqld_show_create(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  DBUG_ENTER("mysqld_show_create");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
		      table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(&thd->net);
    DBUG_RETURN(1);
  }

  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Table",NAME_LEN));
  field_list.push_back(new Item_empty_string("Create Table",1024));

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);
  
  String *packet = &thd->packet;
  for(;table; table = table->next)
    {
      packet->length(0);
      net_store_data(packet, table->table_name);
      // a hack - we need to reserve some space for the length before
      // we know what it is - let's assume that the length of create table
      // statement will fit into 3 bytes ( 16 MB max :-) )
      ulong store_len_offset = packet->length();
      packet->length(store_len_offset + 4);
      if(store_create_info(thd, table, packet))
	DBUG_RETURN(-1);
      ulong create_len = packet->length() - store_len_offset - 4;
      if(create_len > 0x00ffffff) // better readable in HEX ...
	DBUG_RETURN(1);  // just in case somebody manages to create a table
      // with *that* much stuff in the definition

      // now we have to store the length in three bytes, even if it would fit
      // into fewer, so we cannot use net_store_data() anymore,
      // and do it ourselves
      char* p = (char*)packet->ptr() + store_len_offset;
      *p++ = (char) 253; // The client the length is stored using 3-bytes 
      int3store(p, create_len);

      // now we are in business :-)
      if(my_net_write(&thd->net, (char*)packet->ptr(), packet->length()))
	DBUG_RETURN(1);
    }

  send_eof(&thd->net);
  DBUG_RETURN(0);
}


int
mysqld_show_keys(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  char buff[256];
  DBUG_ENTER("mysqld_show_keys");
  DBUG_PRINT("enter",("db: %s  table: %s",table_list->db,
		      table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(&thd->net);
    DBUG_RETURN(1);
  }

  List<Item> field_list;
  Item *item;
  field_list.push_back(new Item_empty_string("Table",NAME_LEN));
  field_list.push_back(new Item_int("Non_unique",0,1));
  field_list.push_back(new Item_empty_string("Key_name",NAME_LEN));
  field_list.push_back(new Item_int("Seq_in_index",0,2));
  field_list.push_back(new Item_empty_string("Column_name",NAME_LEN));
  field_list.push_back(item=new Item_empty_string("Collation",1));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Cardinality",0,11));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("Sub_part",0,3));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("Packed",10));
  item->maybe_null=1;
  field_list.push_back(new Item_empty_string("Comment",255));
  item->maybe_null=1;

  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1);

  KEY *key_info=table->key_info;
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK | HA_STATUS_TIME);
  for (uint i=0 ; i < table->keys ; i++,key_info++)
  {
    KEY_PART_INFO *key_part= key_info->key_part;
    char *end;
    String *packet= &thd->packet;
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      packet->length(0);
      net_store_data(packet,table->table_name);
      net_store_data(packet,((key_info->flags & HA_NOSAME) ? "0" :"1"), 1);
      net_store_data(packet,key_info->name);
      end=int10_to_str((long) (j+1),(char*) buff,10);
      net_store_data(packet,buff,(uint) (end-buff));
      net_store_data(packet,key_part->field ? key_part->field->field_name :
		     "?unknown field?");
      if (table->file->option_flag() & HA_READ_ORDER)
	net_store_data(packet,((key_part->key_part_flag & HA_REVERSE_SORT)
			       ? "D" : "A"), 1);
      else
	net_store_null(packet); /* purecov: inspected */
      KEY *key=table->key_info+i;
      if (key->rec_per_key[j])
      {
	ulong records=(table->file->records / key->rec_per_key[j]);
	end=int10_to_str((long) records, buff, 10);
	net_store_data(packet,buff,(uint) (end-buff));
      }
      else
	net_store_null(packet);
      if (!key_part->field ||
	  key_part->length !=
	  table->field[key_part->fieldnr-1]->key_length())
      {
	end=int10_to_str((long) key_part->length, buff,10); /* purecov: inspected */
	net_store_data(packet,buff,(uint) (end-buff)); /* purecov: inspected */
      }
      else
	net_store_null(packet);
      net_store_null(packet);			// No pack_information yet
      net_store_null(packet);			// No comments yet
      if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
	DBUG_RETURN(1); /* purecov: inspected */
    }
  }
  send_eof(&thd->net);
  DBUG_RETURN(0);
}


/****************************************************************************
** Return only fields for API mysql_list_fields
** Use "show table wildcard" in mysql instead of this
****************************************************************************/

void
mysqld_list_fields(THD *thd, TABLE_LIST *table_list, const char *wild)
{
  TABLE *table;
  DBUG_ENTER("mysqld_list_fields");
  DBUG_PRINT("enter",("table: %s",table_list->real_name));

  if (!(table = open_ltable(thd, table_list, TL_UNLOCK)))
  {
    send_error(&thd->net);
    DBUG_VOID_RETURN;
  }
  List<Item> field_list;

  Field **ptr,*field;
  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    if (!wild || !wild[0] || !wild_case_compare(field->field_name,wild))
      field_list.push_back(new Item_field(field));
  }
  restore_record(table,2);		// Get empty record
  if (send_fields(thd,field_list,2))
    DBUG_VOID_RETURN;
  VOID(net_flush(&thd->net));
  DBUG_VOID_RETURN;
}

int
mysqld_dump_create_info(THD *thd, TABLE *table, int fd)
{
  DBUG_ENTER("mysqld_dump_create_info");
  DBUG_PRINT("enter",("table: %s",table->real_name));
  String* packet = &thd->packet;
  packet->length(0);
  
  if(store_create_info(thd,table,packet))
    DBUG_RETURN(-1);
  
  if(fd < 0)
    {
      if(my_net_write(&thd->net, (char*)packet->ptr(), packet->length()))
	DBUG_RETURN(-1);
      VOID(net_flush(&thd->net));
    }
  else
    {
      if(my_write(fd, (const byte*) packet->ptr(), packet->length(), 
		  MYF(MY_WME)))
	DBUG_RETURN(-1);
    }

  DBUG_RETURN(0);
}
  
static int
store_create_info(THD *thd, TABLE *table, String* packet)
{
  DBUG_ENTER("store_create_info");
  DBUG_PRINT("enter",("table: %s",table->real_name));

  restore_record(table,2); // Get empty record
  
  List<Item> field_list;
  char tmp[MAX_FIELD_WIDTH];
  String type(tmp, sizeof(tmp));
  packet->append("create table ", 13);
  packet->append(table->real_name);
  packet->append('(');
  
  Field **ptr,*field;
  for (ptr=table->field ; (field= *ptr); ptr++)
  {
    if(ptr != table->field)
      packet->append(',');
    
    uint flags = field->flags;
    packet->append(field->field_name);
    packet->append(' ');
    // check for surprises from the previous call to Field::sql_type()
    if(type.ptr() != tmp)
      type.set(tmp, sizeof(tmp));
    
    field->sql_type(type);
    packet->append(type.ptr(),type.length());
    
    bool null_default_value =  (field->type() == FIELD_TYPE_TIMESTAMP ||
				field->unireg_check == Field::NEXT_NUMBER);
    bool has_default = (field->type() != FIELD_TYPE_BLOB);
    
    if((flags & NOT_NULL_FLAG) && !null_default_value)
	packet->append(" not null", 9);
    

    if(has_default)
      {
        packet->append(" default ", 9);
	if (!null_default_value && !field->is_null())
	  {						// Not null by default
	    type.set(tmp,sizeof(tmp));
	    field->val_str(&type,&type);
	    packet->append('\'');
	    packet->append(type.ptr(),type.length());
	    packet->append('\'');
	  }
	else if (field->maybe_null() || null_default_value)
	  packet->append("NULL", 4);			// Null as default
	else
	  packet->append(tmp,0);
      }
    
    if (field->unireg_check == Field::NEXT_NUMBER)
	  packet->append(" auto_increment", 15 );

    
  }

  KEY *key_info=table->key_info;
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK | HA_STATUS_TIME);
  uint primary_key = table->primary_key;
  
  for (uint i=0 ; i < table->keys ; i++,key_info++)
  {
    packet->append(',');
    
    KEY_PART_INFO *key_part= key_info->key_part;
    if(i == primary_key)
      packet->append("primary", 7);
    else if(key_info->flags & HA_NOSAME)
      packet->append("unique", 6);
    packet->append(" key ", 5);

    if(i != primary_key)
     packet->append(key_info->name);
    
    packet->append('(');
    
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if(j)
	packet->append(',');
      
      if(key_part->field)
	packet->append(key_part->field->field_name);
      KEY *key=table->key_info+i;
      
      if (!key_part->field ||
	  key_part->length !=
	  table->field[key_part->fieldnr-1]->key_length())
      {
	char buff[64];
	buff[0] = '(';
	char* end=int10_to_str((long) key_part->length, buff + 1,10);
	*end++ = ')';
	packet->append(buff,(uint) (end-buff)); 
      }
    }

    packet->append(')');
  }

  packet->append(')');
  
  handler *file = table->file;
  packet->append(" type=", 6);
  packet->append(file->table_type());
  char buff[128];
  char* p;
  
  if(table->min_rows)
    {
      packet->append(" min_rows=");
      p = longlong10_to_str(table->min_rows, buff, 10);
      packet->append(buff, (uint) (p - buff));
    }

  if(table->max_rows)
    {
      packet->append(" max_rows=");
      p = longlong10_to_str(table->max_rows, buff, 10);
      packet->append(buff, (uint) (p - buff));
    }
  
  if (table->db_create_options & HA_OPTION_PACK_KEYS)
    packet->append(" pack_keys=1", 12);
  if (table->db_create_options & HA_OPTION_NO_PACK_KEYS)
    packet->append(" pack_keys=0", 12);
  if (table->db_create_options & HA_OPTION_CHECKSUM)
    packet->append(" checksum=1", 11);
  if (table->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
    packet->append(" delay_key_write=1",18);

  
  DBUG_RETURN(0);
}


/****************************************************************************
** Return info about all processes
** returns for each thread: thread id, user, host, db, command, info
****************************************************************************/

class thread_info :public ilink {
public:
  static void *operator new(size_t size) {return (void*) sql_alloc((uint) size); }
  static void operator delete(void *ptr __attribute__((unused)),
			      size_t size __attribute__((unused))) {} /*lint -e715 */

  ulong thread_id;
  time_t start_time;
  uint	 command;
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
  ulong max_query_length= verbose ? max_allowed_packet : PROCESS_LIST_WIDTH;
  DBUG_ENTER("mysqld_list_processes");

  field_list.push_back(new Item_int("Id",0,7));
  field_list.push_back(new Item_empty_string("User",16));
  field_list.push_back(new Item_empty_string("Host",64));
  field_list.push_back(field=new Item_empty_string("db",NAME_LEN));
  field->maybe_null=1;
  field_list.push_back(new Item_empty_string("Command",16));
  field_list.push_back(new Item_empty_string("Time",7));
  field_list.push_back(field=new Item_empty_string("State",30));
  field->maybe_null=1;
  field_list.push_back(field=new Item_empty_string("Info",max_query_length));
  field->maybe_null=1;
  if (send_fields(thd,field_list,1))
    DBUG_VOID_RETURN;

  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  if (!thd->killed)
  {
    I_List_iterator<THD> it(threads);
    THD *tmp;
    while ((tmp=it++))
    {
      if ((tmp->net.vio || tmp->system_thread) &&
	  (!user || (tmp->user && !strcmp(tmp->user,user))))
      {
	thread_info *thd_info=new thread_info;

	thd_info->thread_id=tmp->thread_id;
	thd_info->user=thd->strdup(tmp->user ? tmp->user : (tmp->system_thread ?
				  "system user" : "unauthenticated user"));
	thd_info->host=thd->strdup(tmp->host ? tmp->host : (tmp->ip ? tmp->ip :
				   (tmp->system_thread ? "none" : "connecting host")));
	if ((thd_info->db=tmp->db))		// Safe test
	  thd_info->db=thd->strdup(thd_info->db);
	thd_info->command=(int) tmp->command;
	if (tmp->mysys_var)
	  pthread_mutex_lock(&tmp->mysys_var->mutex);
	thd_info->proc_info= (char*) (tmp->killed ? "Killed" : 0);
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
	if (tmp->mysys_var)
	  pthread_mutex_unlock(&tmp->mysys_var->mutex);

#if !defined(DONT_USE_THR_ALARM) && ! defined(SCO)
	if (pthread_kill(tmp->real_id,0))
	  tmp->proc_info="*** DEAD ***";	// This shouldn't happen
#endif
	thd_info->start_time= tmp->start_time;
	thd_info->query=0;
	if (tmp->query)
	{
	  uint length=(uint) strlen(tmp->query);
	  if (length > max_query_length)
	    length=max_query_length;
	  thd_info->query=(char*) thd->memdup(tmp->query,length+1);
	  thd_info->query[length]=0;
	}
	thread_infos.append(thd_info);
      }
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  thread_info *thd_info;
  String *packet= &thd->packet;
  while ((thd_info=thread_infos.get()))
  {
    char buff[20],*end;
    packet->length(0);
    end=int10_to_str((long) thd_info->thread_id, buff,10);
    net_store_data(packet,buff,(uint) (end-buff));
    net_store_data(packet,thd_info->user);
    net_store_data(packet,thd_info->host);
    if (thd_info->db)
      net_store_data(packet,thd_info->db);
    else
      net_store_null(packet);
    if (thd_info->proc_info)
      net_store_data(packet,thd_info->proc_info);
    else
      net_store_data(packet,command_name[thd_info->command]);
    if (thd_info->start_time)
      net_store_data(packet,(uint32)
		     (time((time_t*) 0) - thd_info->start_time));
    else
      net_store_null(packet);
    if (thd_info->state_info)
      net_store_data(packet,thd_info->state_info);
    else
      net_store_null(packet);
    if (thd_info->query)
      net_store_data(packet,thd_info->query);
    else
      net_store_null(packet);
    if (my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
      break; /* purecov: inspected */
  }
  send_eof(&thd->net);
  DBUG_VOID_RETURN;
}


/*****************************************************************************
** Status functions
*****************************************************************************/


int mysqld_show(THD *thd, const char *wild, show_var_st *variables)
{
  uint i;
  char buff[8192];
  String packet2(buff,sizeof(buff));
  List<Item> field_list;
  DBUG_ENTER("mysqld_show");
  field_list.push_back(new Item_empty_string("Variable_name",30));
  field_list.push_back(new Item_empty_string("Value",256));
  if (send_fields(thd,field_list,1))
    DBUG_RETURN(1); /* purecov: inspected */

  pthread_mutex_lock(&THR_LOCK_keycache);
  pthread_mutex_lock(&LOCK_status);
  for (i=0; variables[i].name; i++)
  {
    if (!(wild && wild[0] && wild_compare(variables[i].name,wild)))
    {
      packet2.length(0);
      net_store_data(&packet2,variables[i].name);
      switch (variables[i].type){
      case SHOW_LONG:
      case SHOW_LONG_CONST:
	net_store_data(&packet2,(uint32) *(ulong*) variables[i].value);
	break;
      case SHOW_BOOL:
	net_store_data(&packet2,(ulong) *(bool*) variables[i].value ?
		       "ON" : "OFF");
	break;
      case SHOW_MY_BOOL:
	net_store_data(&packet2,(ulong) *(my_bool*) variables[i].value ?
		       "ON" : "OFF");
	break;
      case SHOW_INT_CONST:
      case SHOW_INT:
	net_store_data(&packet2,(uint32) *(int*) variables[i].value);
	break;
      case SHOW_CHAR:
	net_store_data(&packet2,variables[i].value);
	break;
      case SHOW_STARTTIME:
	net_store_data(&packet2,(uint32) (thd->query_start() - start_time));
	break;
      case SHOW_QUESTION:
	net_store_data(&packet2,(uint32) thd->query_id);
	break;
      case SHOW_OPENTABLES:
	net_store_data(&packet2,(uint32) cached_tables());
	break;
      case SHOW_CHAR_PTR:
	{
	  char *value= *(char**) variables[i].value;
	  net_store_data(&packet2,value ? value : "");
	  break;
	}
      }
      if (my_net_write(&thd->net, (char*) packet2.ptr(),packet2.length()))
	goto err;				/* purecov: inspected */
    }
  }
  pthread_mutex_unlock(&LOCK_status);
  pthread_mutex_unlock(&THR_LOCK_keycache);
  send_eof(&thd->net);
  DBUG_RETURN(0);

 err:
  pthread_mutex_unlock(&LOCK_status);
  pthread_mutex_unlock(&THR_LOCK_keycache);
  DBUG_RETURN(1);
}

#ifdef __GNUC__
template class List_iterator<char>;
template class List<char>;
#endif
