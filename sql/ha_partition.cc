/* Copyright (C) 2005 MySQL AB

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

/*
  This handler was developed by Mikael Ronström for version 5.1 of MySQL.
  It is an abstraction layer on top of other handlers such as MyISAM,
  InnoDB, Federated, Berkeley DB and so forth. Partitioned tables can also
  be handled by a storage engine. The current example of this is NDB
  Cluster that has internally handled partitioning. This have benefits in
  that many loops needed in the partition handler can be avoided.

  Partitioning has an inherent feature which in some cases is positive and
  in some cases is negative. It splits the data into chunks. This makes
  the data more manageable, queries can easily be parallelised towards the
  parts and indexes are split such that there are less levels in the
  index trees. The inherent disadvantage is that to use a split index
  one has to scan all index parts which is ok for large queries but for
  small queries it can be a disadvantage.

  Partitioning lays the foundation for more manageable databases that are
  extremely large. It does also lay the foundation for more parallelism
  in the execution of queries. This functionality will grow with later
  versions of MySQL.

  You can enable it in your buld by doing the following during your build
  process:
  ./configure --with-partition

  The partition is setup to use table locks. It implements an partition "SHARE"
  that is inserted into a hash by table name. You can use this to store
  information of state that any partition handler object will be able to see
  if it is using the same table.

  Please read the object definition in ha_partition.h before reading the rest
  if this file.
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include <mysql_priv.h>

#ifdef HAVE_PARTITION_DB
#include "ha_partition.h"

static const char *ha_par_ext= ".par";
#ifdef NOT_USED
static int free_share(PARTITION_SHARE * share);
static PARTITION_SHARE *get_share(const char *table_name, TABLE * table);
#endif

/****************************************************************************
                MODULE create/delete handler object
****************************************************************************/

static handlerton partition_hton = {
  "partition",
  0, /* slot */
  0, /* savepoint size */
  NULL /*ndbcluster_close_connection*/,
  NULL, /* savepoint_set */
  NULL, /* savepoint_rollback */
  NULL, /* savepoint_release */
  NULL /*ndbcluster_commit*/,
  NULL /*ndbcluster_rollback*/,
  NULL, /* prepare */
  NULL, /* recover */
  NULL, /* commit_by_xid */
  NULL, /* rollback_by_xid */
  NULL,
  NULL,
  NULL,
  HTON_NO_FLAGS
};

ha_partition::ha_partition(TABLE *table)
  :handler(&partition_hton, table), m_part_info(NULL), m_create_handler(FALSE),
   m_is_sub_partitioned(0)
{
  DBUG_ENTER("ha_partition::ha_partition(table)");
  init_handler_variables();
  if (table)
  {
    if (table->s->part_info)
    {
      m_part_info= table->s->part_info;
      m_is_sub_partitioned= is_sub_partitioned(m_part_info);
    }
  }
  DBUG_VOID_RETURN;
}


ha_partition::ha_partition(partition_info *part_info)
  :handler(&partition_hton, NULL), m_part_info(part_info), m_create_handler(TRUE),
   m_is_sub_partitioned(is_sub_partitioned(m_part_info))

{
  DBUG_ENTER("ha_partition::ha_partition(part_info)");
  init_handler_variables();
  DBUG_ASSERT(m_part_info);
  DBUG_VOID_RETURN;
}


void ha_partition::init_handler_variables()
{
  active_index= MAX_KEY;
  m_file_buffer= NULL;
  m_name_buffer_ptr= NULL;
  m_engine_array= NULL;
  m_file= NULL;
  m_tot_parts= 0;
  m_has_transactions= 0;
  m_pkey_is_clustered= 0;
  m_lock_type= F_UNLCK;
  m_part_spec.start_part= NO_CURRENT_PART_ID;
  m_scan_value= 2;
  m_ref_length= 0;
  m_part_spec.end_part= NO_CURRENT_PART_ID;
  m_index_scan_type= partition_no_index_scan;
  m_start_key.key= NULL;
  m_start_key.length= 0;
  m_myisam= FALSE;
  m_innodb= FALSE;
  m_extra_cache= FALSE;
  m_extra_cache_size= 0;
  m_table_flags= HA_FILE_BASED | HA_REC_NOT_IN_SEQ;
  m_low_byte_first= 1;
  m_part_field_array= NULL;
  m_ordered_rec_buffer= NULL;
  m_top_entry= NO_CURRENT_PART_ID;
  m_rec_length= 0;
  m_last_part= 0;
  m_rec0= 0;
  m_curr_key_info= 0;

#ifdef DONT_HAVE_TO_BE_INITALIZED
  m_start_key.flag= 0;
  m_ordered= TRUE;
#endif
}


ha_partition::~ha_partition()
{
  DBUG_ENTER("ha_partition::~ha_partition()");
  if (m_file != NULL)
  {
    uint i;
    for (i= 0; i < m_tot_parts; i++)
      delete m_file[i];
  }
  my_free((char*) m_ordered_rec_buffer, MYF(MY_ALLOW_ZERO_PTR));

  clear_handler_file();
  DBUG_VOID_RETURN;
}


/*
  The partition handler is only a layer on top of other engines. Thus it
  can't really perform anything without the underlying handlers. Thus we
  add this method as part of the allocation of a handler object.

  1) Allocation of underlying handlers
     If we have access to the partition info we will allocate one handler
     instance for each partition.
  2) Allocation without partition info
     The cases where we don't have access to this information is when called
     in preparation for delete_table and rename_table and in that case we
     only need to set HA_FILE_BASED. In that case we will use the .par file
     that contains information about the partitions and their engines and
     the names of each partition.
  3) Table flags initialisation
     We need also to set table flags for the partition handler. This is not
     static since it depends on what storage engines are used as underlying
     handlers.
     The table flags is set in this routine to simulate the behaviour of a
     normal storage engine
     The flag HA_FILE_BASED will be set independent of the underlying handlers
  4) Index flags initialisation
     When knowledge exists on the indexes it is also possible to initialise the
     index flags. Again the index flags must be initialised by using the under-
     lying handlers since this is storage engine dependent.
     The flag HA_READ_ORDER will be reset for the time being to indicate no
     ordered output is available from partition handler indexes. Later a merge
     sort will be performed using the underlying handlers.
  5) primary_key_is_clustered, has_transactions and low_byte_first is
     calculated here.
*/

int ha_partition::ha_initialise()
{
  handler **file_array, *file;
  DBUG_ENTER("ha_partition::set_up_constants");

  if (m_part_info)
  {
    m_tot_parts= get_tot_partitions(m_part_info);
    DBUG_ASSERT(m_tot_parts > 0);
    if (m_create_handler)
    {
      if (new_handlers_from_part_info())
	DBUG_RETURN(1);
    }
    else if (get_from_handler_file(table->s->path))
    {
      my_error(ER_OUTOFMEMORY, MYF(0), 129); //Temporary fix TODO print_error
      DBUG_RETURN(1);
    }
    /*
      We create all underlying table handlers here. We only do it if we have
      access to the partition info. We do it in this special method to be
      able to report allocation errors.
    */
    /*
      Set up table_flags, low_byte_first, primary_key_is_clustered and
      has_transactions since they are called often in all kinds of places,
      other parameters are calculated on demand.
      HA_FILE_BASED is always set for partition handler since we use a
      special file for handling names of partitions, engine types.
      HA_CAN_GEOMETRY, HA_CAN_FULLTEXT, HA_CAN_SQL_HANDLER,
      HA_CAN_INSERT_DELAYED is disabled until further investigated.
    */
    m_table_flags= m_file[0]->table_flags();
    m_low_byte_first= m_file[0]->low_byte_first();
    m_has_transactions= TRUE;
    m_pkey_is_clustered= TRUE;
    file_array= m_file;
    do
    {
      file= *file_array;
      if (m_low_byte_first != file->low_byte_first())
      {
        // Cannot have handlers with different endian
        my_error(ER_MIX_HANDLER_ERROR, MYF(0));
	DBUG_RETURN(1);
      }
      if (!file->has_transactions())
	m_has_transactions= FALSE;
      if (!file->primary_key_is_clustered())
	m_pkey_is_clustered= FALSE;
      m_table_flags&= file->table_flags();
    } while (*(++file_array));
    m_table_flags&= ~(HA_CAN_GEOMETRY & HA_CAN_FULLTEXT &
                      HA_CAN_SQL_HANDLER & HA_CAN_INSERT_DELAYED);
    /*
      TODO RONM:
      Make sure that the tree works without partition defined, compiles
      and goes through mysql-test-run.
    */
  }
  m_table_flags|= HA_FILE_BASED | HA_REC_NOT_IN_SEQ;
  DBUG_RETURN(0);
}

/****************************************************************************
                MODULE meta data changes
****************************************************************************/
/*
  This method is used to calculate the partition name, service routine to
  the del_ren_cre_table method.
*/

static void create_partition_name(char *out, const char *in1, const char *in2)
{
  strxmov(out, in1, "_", in2, NullS);
}

/*
  This method is used to calculate the partition name, service routine to
  the del_ren_cre_table method.
*/

static void create_subpartition_name(char *out, const char *in1,
                                     const char *in2, const char *in3)
{
  strxmov(out, in1, "_", in2, "_", in3, NullS);
}


/*
  Used to delete a table. By the time delete_table() has been called all
  opened references to this table will have been closed (and your globally
  shared references released. The variable name will just be the name of
  the table. You will need to remove any files you have created at this
  point.

  If you do not implement this, the default delete_table() is called from
  handler.cc and it will delete all files with the file extentions returned
  by bas_ext().

  Called from handler.cc by delete_table and  ha_create_table(). Only used
  during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
  the storage engine.
*/

int ha_partition::delete_table(const char *name)
{
  int error;
  DBUG_ENTER("ha_partition::delete_table");
  if ((error= del_ren_cre_table(name, NULL, NULL, NULL)))
    DBUG_RETURN(error);
  DBUG_RETURN(handler::delete_table(name));
}


/*
  Renames a table from one name to another from alter table call.

  If you do not implement this, the default rename_table() is called from
  handler.cc and it will delete all files with the file extentions returned
  by bas_ext().

  Called from sql_table.cc by mysql_rename_table().
*/

int ha_partition::rename_table(const char *from, const char *to)
{
  int error;
  DBUG_ENTER("ha_partition::rename_table");
  if ((error= del_ren_cre_table(from, to, NULL, NULL)))
    DBUG_RETURN(error);
  DBUG_RETURN(handler::rename_table(from, to));
}


/*
  create_handler_files is called to create any handler specific files
  before opening the file with openfrm to later call ::create on the
  file object.
  In the partition handler this is used to store the names of partitions
  and types of engines in the partitions.
*/

int ha_partition::create_handler_files(const char *name)
{
  DBUG_ENTER("ha_partition::create_handler_files()");

  /*
    We need to update total number of parts since we might write the handler
    file as part of a partition management command
  */
  m_tot_parts= get_tot_partitions(m_part_info);
  if (create_handler_file(name))
  {
    my_error(ER_CANT_CREATE_HANDLER_FILE, MYF(0));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  create() is called to create a table. The variable name will have the name
  of the table. When create() is called you do not need to worry about
  opening the table. Also, the FRM file will have already been created so
  adjusting create_info will not do you any good. You can overwrite the frm
  file at this point if you wish to change the table definition, but there
  are no methods currently provided for doing that.

  Called from handle.cc by ha_create_table().
*/

int ha_partition::create(const char *name, TABLE *table_arg,
			 HA_CREATE_INFO *create_info)
{
  char t_name[FN_REFLEN];
  DBUG_ENTER("ha_partition::create");

  strmov(t_name, name);
  *fn_ext(t_name)= 0;
  if (del_ren_cre_table(t_name, NULL, table_arg, create_info))
  {
    handler::delete_table(t_name);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

int ha_partition::drop_partitions(const char *path)
{
  List_iterator<partition_element> part_it(m_part_info->partitions);
  char part_name_buff[FN_REFLEN];
  uint no_parts= m_part_info->no_parts;
  uint no_subparts= m_part_info->no_subparts, i= 0;
  int error= 1;
  DBUG_ENTER("ha_partition::drop_partitions()");

  do
  {
    partition_element *part_elem= part_it++;
    if (part_elem->part_state == PART_IS_DROPPED)
    {
      /*
        This part is to be dropped, meaning the part or all its subparts.
      */
      if (is_sub_partitioned(m_part_info))
      {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        uint j= 0, part;
        do
        {
          partition_element *sub_elem= sub_it++;
          create_subpartition_name(part_name_buff, path,
                                   part_elem->partition_name,
                                   sub_elem->partition_name);
          part= i * no_subparts + j;
          DBUG_PRINT("info", ("Drop subpartition %s", part_name_buff));
          error= m_file[part]->delete_table((const char *) part_name_buff);
        } while (++j < no_subparts);
      }
      else
      {
        create_partition_name(part_name_buff, path,
                              part_elem->partition_name);
        DBUG_PRINT("info", ("Drop partition %s", part_name_buff));
        error= m_file[i]->delete_table((const char *) part_name_buff);
      }
    }
  } while (++i < no_parts);
  DBUG_RETURN(error);
}

void ha_partition::update_create_info(HA_CREATE_INFO *create_info)
{
  return;
}


char *ha_partition::update_table_comment(const char *comment)
{
  return (char*) comment;                       // Nothing to change
}



/*
  Common routine to handle delete_table and rename_table.
  The routine uses the partition handler file to get the
  names of the partition instances. Both these routines
  are called after creating the handler without table
  object and thus the file is needed to discover the
  names of the partitions and the underlying storage engines.
*/

uint ha_partition::del_ren_cre_table(const char *from,
				     const char *to,
				     TABLE *table_arg,
				     HA_CREATE_INFO *create_info)
{
  int save_error= 0, error;
  char from_buff[FN_REFLEN], to_buff[FN_REFLEN];
  char *name_buffer_ptr;
  uint i;
  handler **file;
  DBUG_ENTER("del_ren_cre_table()");

  if (get_from_handler_file(from))
    DBUG_RETURN(TRUE);
  DBUG_ASSERT(m_file_buffer);
  name_buffer_ptr= m_name_buffer_ptr;
  file= m_file;
  i= 0;
  do
  {
    create_partition_name(from_buff, from, name_buffer_ptr);
    if (to != NULL)
    {						// Rename branch
      create_partition_name(to_buff, to, name_buffer_ptr);
      error= (*file)->rename_table((const char*) from_buff,
				   (const char*) to_buff);
    }
    else if (table_arg == NULL)			// delete branch
      error= (*file)->delete_table((const char*) from_buff);
    else
    {
      set_up_table_before_create(table_arg, create_info, i);
      error= (*file)->create(from_buff, table_arg, create_info);
    }
    name_buffer_ptr= strend(name_buffer_ptr) + 1;
    if (error)
      save_error= error;
    i++;
  } while (*(++file));
  DBUG_RETURN(save_error);
}


partition_element *ha_partition::find_partition_element(uint part_id)
{
  uint i;
  uint curr_part_id= 0;
  List_iterator_fast < partition_element > part_it(m_part_info->partitions);

  for (i= 0; i < m_part_info->no_parts; i++)
  {
    partition_element *part_elem;
    part_elem= part_it++;
    if (m_is_sub_partitioned)
    {
      uint j;
      List_iterator_fast <partition_element> sub_it(part_elem->subpartitions);
      for (j= 0; j < m_part_info->no_subparts; j++)
      {
	part_elem= sub_it++;
	if (part_id == curr_part_id++)
	  return part_elem;
      }
    }
    else if (part_id == curr_part_id++)
      return part_elem;
  }
  DBUG_ASSERT(0);
  current_thd->fatal_error();                   // Abort
  return NULL;
}


void ha_partition::set_up_table_before_create(TABLE *table,
					      HA_CREATE_INFO *info,
					      uint part_id)
{
  /*
    Set up
    1) Comment on partition
    2) MAX_ROWS, MIN_ROWS on partition
    3) Index file name on partition
    4) Data file name on partition
  */
  partition_element *part_elem= find_partition_element(part_id);
  if (!part_elem)
    return;                                     // Fatal error
  table->s->max_rows= part_elem->part_max_rows;
  table->s->min_rows= part_elem->part_min_rows;
  info->index_file_name= part_elem->index_file_name;
  info->data_file_name= part_elem->data_file_name;
}


/*
  Routine used to add two names with '_' in between then. Service routine
  to create_handler_file
  Include the NULL in the count of characters since it is needed as separator
  between the partition names.
*/

static uint name_add(char *dest, const char *first_name, const char *sec_name)
{
  return (uint) (strxmov(dest, first_name, "_", sec_name, NullS) -dest) + 1;
}


/*
  Method used to create handler file with names of partitions, their
  engine types and the number of partitions.
*/

bool ha_partition::create_handler_file(const char *name)
{
  partition_element *part_elem, *subpart_elem;
  uint i, j, part_name_len, subpart_name_len;
  uint tot_partition_words, tot_name_len;
  uint tot_len_words, tot_len_byte, chksum, tot_name_words;
  char *name_buffer_ptr;
  uchar *file_buffer, *engine_array;
  bool result= TRUE;
  char file_name[FN_REFLEN];
  File file;
  List_iterator_fast < partition_element > part_it(m_part_info->partitions);
  DBUG_ENTER("create_handler_file");

  DBUG_PRINT("info", ("table name = %s", name));
  tot_name_len= 0;
  for (i= 0; i < m_part_info->no_parts; i++)
  {
    part_elem= part_it++;
    part_name_len= strlen(part_elem->partition_name);
    if (!m_is_sub_partitioned)
      tot_name_len+= part_name_len + 1;
    else
    {
      List_iterator_fast<partition_element> sub_it(part_elem->subpartitions);
      for (j= 0; j < m_part_info->no_subparts; j++)
      {
	subpart_elem= sub_it++;
	subpart_name_len= strlen(subpart_elem->partition_name);
	tot_name_len+= part_name_len + subpart_name_len + 2;
      }
    }
  }
  /*
     File format:
     Length in words              4 byte
     Checksum                     4 byte
     Total number of partitions   4 byte
     Array of engine types        n * 4 bytes where
     n = (m_tot_parts + 3)/4
     Length of name part in bytes 4 bytes
     Name part                    m * 4 bytes where
     m = ((length_name_part + 3)/4)*4

     All padding bytes are zeroed
  */
  tot_partition_words= (m_tot_parts + 3) / 4;
  tot_name_words= (tot_name_len + 3) / 4;
  tot_len_words= 4 + tot_partition_words + tot_name_words;
  tot_len_byte= 4 * tot_len_words;
  if (!(file_buffer= (uchar *) my_malloc(tot_len_byte, MYF(MY_ZEROFILL))))
    DBUG_RETURN(TRUE);
  engine_array= (file_buffer + 12);
  name_buffer_ptr= (char*) (file_buffer + ((4 + tot_partition_words) * 4));
  part_it.rewind();
  for (i= 0; i < m_part_info->no_parts; i++)
  {
    part_elem= part_it++;
    if (!m_is_sub_partitioned)
    {
      name_buffer_ptr= strmov(name_buffer_ptr, part_elem->partition_name)+1;
      *engine_array= (uchar) part_elem->engine_type;
      DBUG_PRINT("info", ("engine: %u", *engine_array));
      engine_array++;
    }
    else
    {
      List_iterator_fast<partition_element> sub_it(part_elem->subpartitions);
      for (j= 0; j < m_part_info->no_subparts; j++)
      {
	subpart_elem= sub_it++;
	name_buffer_ptr+= name_add(name_buffer_ptr,
				    part_elem->partition_name,
				    subpart_elem->partition_name);
	*engine_array= (uchar) part_elem->engine_type;
	engine_array++;
      }
    }
  }
  chksum= 0;
  int4store(file_buffer, tot_len_words);
  int4store(file_buffer + 8, m_tot_parts);
  int4store(file_buffer + 12 + (tot_partition_words * 4), tot_name_len);
  for (i= 0; i < tot_len_words; i++)
    chksum^= uint4korr(file_buffer + 4 * i);
  int4store(file_buffer + 4, chksum);
  /*
    Remove .frm extension and replace with .par
    Create and write and close file
    to be used at open, delete_table and rename_table
  */
  fn_format(file_name, name, "", ".par", MYF(MY_REPLACE_EXT));
  if ((file= my_create(file_name, CREATE_MODE, O_RDWR | O_TRUNC,
		       MYF(MY_WME))) >= 0)
  {
    result= my_write(file, (byte *) file_buffer, tot_len_byte,
			   MYF(MY_WME | MY_NABP));
    VOID(my_close(file, MYF(0)));
  }
  else
    result= TRUE;
  my_free((char*) file_buffer, MYF(0));
  DBUG_RETURN(result);
}


void ha_partition::clear_handler_file()
{
  my_free((char*) m_file_buffer, MYF(MY_ALLOW_ZERO_PTR));
  m_file_buffer= NULL;
  m_name_buffer_ptr= NULL;
  m_engine_array= NULL;
}


bool ha_partition::create_handlers()
{
  uint i;
  uint alloc_len= (m_tot_parts + 1) * sizeof(handler*);
  DBUG_ENTER("create_handlers");

  if (!(m_file= (handler **) sql_alloc(alloc_len)))
    DBUG_RETURN(TRUE);
  bzero(m_file, alloc_len);
  for (i= 0; i < m_tot_parts; i++)
  {
    if (!(m_file[i]= get_new_handler(table, (enum db_type) m_engine_array[i])))
      DBUG_RETURN(TRUE);
    DBUG_PRINT("info", ("engine_type: %u", m_engine_array[i]));
  }
  m_file[m_tot_parts]= 0;
  /* For the moment we only support partition over the same table engine */
  if (m_engine_array[0] == (uchar) DB_TYPE_MYISAM)
  {
    DBUG_PRINT("info", ("MyISAM"));
    m_myisam= TRUE;
  }
  else if (m_engine_array[0] == (uchar) DB_TYPE_INNODB)
  {
    DBUG_PRINT("info", ("InnoDB"));
    m_innodb= TRUE;
  }
  DBUG_RETURN(FALSE);
}


bool ha_partition::new_handlers_from_part_info()
{
  uint i, j;
  partition_element *part_elem;
  uint alloc_len= (m_tot_parts + 1) * sizeof(handler*);
  List_iterator_fast <partition_element> part_it(m_part_info->partitions);
  DBUG_ENTER("ha_partition::new_handlers_from_part_info");

  if (!(m_file= (handler **) sql_alloc(alloc_len)))
    goto error;
  bzero(m_file, alloc_len);
  DBUG_ASSERT(m_part_info->no_parts > 0);

  i= 0;
  /*
    Don't know the size of the underlying storage engine, invent a number of
    bytes allocated for error message if allocation fails
  */
  alloc_len= 128; 
  do
  {
    part_elem= part_it++;
    if (!(m_file[i]= get_new_handler(table, part_elem->engine_type)))
      goto error;
    DBUG_PRINT("info", ("engine_type: %u", (uint) part_elem->engine_type));
    if (m_is_sub_partitioned)
    {
      for (j= 0; j < m_part_info->no_subparts; j++)
      {
	if (!(m_file[i]= get_new_handler(table, part_elem->engine_type)))
          goto error;
	DBUG_PRINT("info", ("engine_type: %u", (uint) part_elem->engine_type));
      }
    }
  } while (++i < m_part_info->no_parts);
  if (part_elem->engine_type == DB_TYPE_MYISAM)
  {
    DBUG_PRINT("info", ("MyISAM"));
    m_myisam= TRUE;
  }
  DBUG_RETURN(FALSE);
error:
  my_error(ER_OUTOFMEMORY, MYF(0), alloc_len);
  DBUG_RETURN(TRUE);
}


/*
  Open handler file to get partition names, engine types and number of
  partitions.
*/

bool ha_partition::get_from_handler_file(const char *name)
{
  char buff[FN_REFLEN], *address_tot_name_len;
  File file;
  char *file_buffer, *name_buffer_ptr;
  uchar *engine_array;
  uint i, len_bytes, len_words, tot_partition_words, tot_name_words, chksum;
  DBUG_ENTER("ha_partition::get_from_handler_file");
  DBUG_PRINT("enter", ("table name: '%s'", name));

  if (m_file_buffer)
    DBUG_RETURN(FALSE);
  fn_format(buff, name, "", ha_par_ext, MYF(0));

  /* Following could be done with my_stat to read in whole file */
  if ((file= my_open(buff, O_RDONLY | O_SHARE, MYF(0))) < 0)
    DBUG_RETURN(TRUE);
  if (my_read(file, (byte *) & buff[0], 8, MYF(MY_NABP)))
    goto err1;
  len_words= uint4korr(buff);
  len_bytes= 4 * len_words;
  if (!(file_buffer= my_malloc(len_bytes, MYF(0))))
    goto err1;
  VOID(my_seek(file, 0, MY_SEEK_SET, MYF(0)));
  if (my_read(file, (byte *) file_buffer, len_bytes, MYF(MY_NABP)))
    goto err2;

  chksum= 0;
  for (i= 0; i < len_words; i++)
    chksum ^= uint4korr((file_buffer) + 4 * i);
  if (chksum)
    goto err2;
  m_tot_parts= uint4korr((file_buffer) + 8);
  tot_partition_words= (m_tot_parts + 3) / 4;
  engine_array= (uchar *) ((file_buffer) + 12);
  address_tot_name_len= file_buffer + 12 + 4 * tot_partition_words;
  tot_name_words= (uint4korr(address_tot_name_len) + 3) / 4;
  if (len_words != (tot_partition_words + tot_name_words + 4))
    goto err2;
  name_buffer_ptr= file_buffer + 16 + 4 * tot_partition_words;
  VOID(my_close(file, MYF(0)));
  m_file_buffer= file_buffer;          // Will be freed in clear_handler_file()
  m_name_buffer_ptr= name_buffer_ptr;
  m_engine_array= engine_array;
  if (!m_file && create_handlers())
  {
    clear_handler_file();
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);

err2:
  my_free(file_buffer, MYF(0));
err1:
  VOID(my_close(file, MYF(0)));
  DBUG_RETURN(TRUE);
}

/****************************************************************************
                MODULE open/close object
****************************************************************************/
/*
  Used for opening tables. The name will be the name of the file.
  A table is opened when it needs to be opened. For instance
  when a request comes in for a select on the table (tables are not
  open and closed for each request, they are cached).

  Called from handler.cc by handler::ha_open(). The server opens all tables
  by calling ha_open() which then calls the handler specific open().
*/

int ha_partition::open(const char *name, int mode, uint test_if_locked)
{
  int error;
  char name_buff[FN_REFLEN];
  char *name_buffer_ptr= m_name_buffer_ptr;
  handler **file;
  uint alloc_len;
  DBUG_ENTER("ha_partition::open");

  ref_length= 0;
  m_part_field_array= m_part_info->full_part_field_array;
  if (get_from_handler_file(name))
    DBUG_RETURN(1);
  m_start_key.length= 0;
  m_rec0= table->record[0];
  m_rec_length= table->s->reclength;
  alloc_len= m_tot_parts * (m_rec_length + PARTITION_BYTES_IN_POS); 
  alloc_len+= table->s->max_key_length;
  if (!m_ordered_rec_buffer)
  {
    if (!(m_ordered_rec_buffer= my_malloc(alloc_len, MYF(MY_WME))))
    {
      DBUG_RETURN(1);
    }
    {
      /*
        We set-up one record per partition and each record has 2 bytes in
        front where the partition id is written. This is used by ordered
        index_read.
        We also set-up a reference to the first record for temporary use in
        setting up the scan.
      */
      char *ptr= m_ordered_rec_buffer;
      uint i= 0;
      do
      {
        int2store(ptr, i);
        ptr+= m_rec_length + PARTITION_BYTES_IN_POS;
      } while (++i < m_tot_parts);
      m_start_key.key= ptr;
    }
  }
  file= m_file;
  do
  {
    create_partition_name(name_buff, name, name_buffer_ptr);
    if ((error= (*file)->ha_open((const char*) name_buff, mode,
                                 test_if_locked)))
      goto err_handler;
    name_buffer_ptr+= strlen(name_buffer_ptr) + 1;
    set_if_bigger(ref_length, ((*file)->ref_length));
  } while (*(++file));
  /*
    Add 2 bytes for partition id in position ref length.
    ref_length=max_in_all_partitions(ref_length) + PARTITION_BYTES_IN_POS
  */
  ref_length+= PARTITION_BYTES_IN_POS;
  m_ref_length= ref_length;
  /*
    Release buffer read from .par file. It will not be reused again after
    being opened once.
  */
  clear_handler_file();
  /*
    Initialise priority queue, initialised to reading forward.
  */
  if ((error= init_queue(&queue, m_tot_parts, (uint) PARTITION_BYTES_IN_POS,
                         0, key_rec_cmp, (void*)this)))
    goto err_handler;
  /*
    Some handlers update statistics as part of the open call. This will in
    some cases corrupt the statistics of the partition handler and thus
    to ensure we have correct statistics we call info from open after
    calling open on all individual handlers.
  */
  info(HA_STATUS_VARIABLE | HA_STATUS_CONST);
  DBUG_RETURN(0);

err_handler:
  while (file-- != m_file)
    (*file)->close();
  DBUG_RETURN(error);
}

/*
  Closes a table. We call the free_share() function to free any resources
  that we have allocated in the "shared" structure.

  Called from sql_base.cc, sql_select.cc, and table.cc.
  In sql_select.cc it is only used to close up temporary tables or during
  the process where a temporary table is converted over to being a
  myisam table.
  For sql_base.cc look at close_data_tables().
*/

int ha_partition::close(void)
{
  handler **file;
  DBUG_ENTER("ha_partition::close");
  file= m_file;
  do
  {
    (*file)->close();
  } while (*(++file));
  DBUG_RETURN(0);
}


/****************************************************************************
                MODULE start/end statement
****************************************************************************/
/*
  A number of methods to define various constants for the handler. In
  the case of the partition handler we need to use some max and min
  of the underlying handlers in most cases.
*/

/*
  First you should go read the section "locking functions for mysql" in
  lock.cc to understand this.
  This create a lock on the table. If you are implementing a storage engine
  that can handle transactions look at ha_berkely.cc to see how you will
  want to goo about doing this. Otherwise you should consider calling
  flock() here.
  Originally this method was used to set locks on file level to enable
  several MySQL Servers to work on the same data. For transactional
  engines it has been "abused" to also mean start and end of statements
  to enable proper rollback of statements and transactions. When LOCK
  TABLES has been issued the start_stmt method takes over the role of
  indicating start of statement but in this case there is no end of
  statement indicator(?).

  Called from lock.cc by lock_external() and unlock_external(). Also called
  from sql_table.cc by copy_data_between_tables().
*/

int ha_partition::external_lock(THD *thd, int lock_type)
{
  uint error;
  handler **file;
  DBUG_ENTER("ha_partition::external_lock");
  file= m_file;
  do
  {
    if ((error= (*file)->external_lock(thd, lock_type)))
    {
      if (lock_type != F_UNLCK)
	goto err_handler;
    }
  } while (*(++file));
  m_lock_type= lock_type;                       // For the future (2009?)
  DBUG_RETURN(0);

err_handler:
  while (file-- != m_file)
    (*file)->external_lock(thd, F_UNLCK);
  DBUG_RETURN(error);
}


/*
  The idea with handler::store_lock() is the following:

  The statement decided which locks we should need for the table
  for updates/deletes/inserts we get WRITE locks, for SELECT... we get
  read locks.

  Before adding the lock into the table lock handler (see thr_lock.c)
  mysqld calls store lock with the requested locks.  Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all) or add locks
  for many tables (like we do when we are using a MERGE handler).

  Berkeley DB for partition  changes all WRITE locks to TL_WRITE_ALLOW_WRITE
  (which signals that we are doing WRITES, but we are still allowing other
  reader's and writer's.

  When releasing locks, store_lock() are also called. In this case one
  usually doesn't have to do anything.

  store_lock is called when holding a global mutex to ensure that only
  one thread at a time changes the locking information of tables.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time).  In the future we will probably try to remove this.

  Called from lock.cc by get_lock_data().
*/

THR_LOCK_DATA **ha_partition::store_lock(THD *thd,
					 THR_LOCK_DATA **to,
					 enum thr_lock_type lock_type)
{
  handler **file;
  DBUG_ENTER("ha_partition::store_lock");
  file= m_file;
  do
  {
    to= (*file)->store_lock(thd, to, lock_type);
  } while (*(++file));
  DBUG_RETURN(to);
}


int ha_partition::start_stmt(THD *thd)
{
  int error= 0;
  handler **file;
  DBUG_ENTER("ha_partition::start_stmt");
  file= m_file;
  do
  {
    if ((error= (*file)->start_stmt(thd)))
      break;
  } while (*(++file));
  DBUG_RETURN(error);
}


/*
  Returns the number of store locks needed in call to store lock.
  We return number of partitions since we call store_lock on each
  underlying handler. Assists the above functions in allocating
  sufficient space for lock structures.
*/

uint ha_partition::lock_count() const
{
  DBUG_ENTER("ha_partition::lock_count");
  DBUG_RETURN(m_tot_parts);
}


/*
  Record currently processed was not in the result set of the statement
  and is thus unlocked. Used for UPDATE and DELETE queries.
*/

void ha_partition::unlock_row()
{
  m_file[m_last_part]->unlock_row();
  return;
}


/****************************************************************************
                MODULE change record
****************************************************************************/

/*
  write_row() inserts a row. buf() is a byte array of data, normally record[0].

  You can use the field information to extract the data from the native byte
  array type.

  Example of this would be:
  for (Field **field=table->field ; *field ; field++)
  {
    ...
  }

  See ha_tina.cc for an partition of extracting all of the data as strings.
  ha_berekly.cc has an partition of how to store it intact by "packing" it
  for ha_berkeley's own native storage type.

  See the note for update_row() on auto_increments and timestamps. This
  case also applied to write_row().

  Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.

  ADDITIONAL INFO:

  Most handlers set timestamp when calling write row if any such fields
  exists. Since we are calling an underlying handler we assume the´
  underlying handler will assume this responsibility.

  Underlying handlers will also call update_auto_increment to calculate
  the new auto increment value. We will catch the call to
  get_auto_increment and ensure this increment value is maintained by
  only one of the underlying handlers.
*/

int ha_partition::write_row(byte * buf)
{
  uint32 part_id;
  int error;
#ifdef NOT_NEEDED
  byte *rec0= m_rec0;
#endif
  DBUG_ENTER("ha_partition::write_row");
  DBUG_ASSERT(buf == m_rec0);

#ifdef NOT_NEEDED
  if (likely(buf == rec0))
#endif
    error= m_part_info->get_partition_id(m_part_info, &part_id);
#ifdef NOT_NEEDED
  else
  {
    set_field_ptr(m_part_field_array, buf, rec0);
    error= m_part_info->get_partition_id(m_part_info, &part_id);
    set_field_ptr(m_part_field_array, rec0, buf);
  }
#endif
  if (unlikely(error))
    DBUG_RETURN(error);
  m_last_part= part_id;
  DBUG_PRINT("info", ("Insert in partition %d", part_id));
  DBUG_RETURN(m_file[part_id]->write_row(buf));
}


/*
  Yes, update_row() does what you expect, it updates a row. old_data will
  have the previous row record in it, while new_data will have the newest
  data in it.
  Keep in mind that the server can do updates based on ordering if an
  ORDER BY clause was used. Consecutive ordering is not guarenteed.

  Currently new_data will not have an updated auto_increament record, or
  and updated timestamp field. You can do these for partition by doing these:
  if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
    table->timestamp_field->set_time();
  if (table->next_number_field && record == table->record[0])
    update_auto_increment();

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.
  new_data is always record[0]
  old_data is normally record[1] but may be anything

*/

int ha_partition::update_row(const byte *old_data, byte *new_data)
{
  uint32 new_part_id, old_part_id;
  int error;
  DBUG_ENTER("ha_partition::update_row");

  if ((error= get_parts_for_update(old_data, new_data, table->record[0],
                                  m_part_info, &old_part_id, &new_part_id)))
  {
    DBUG_RETURN(error);
  }

  /*
    TODO:
      set_internal_auto_increment=
        max(set_internal_auto_increment, new_data->auto_increment)
  */
  m_last_part= new_part_id;
  if (new_part_id == old_part_id)
  {
    DBUG_PRINT("info", ("Update in partition %d", new_part_id));
    DBUG_RETURN(m_file[new_part_id]->update_row(old_data, new_data));
  }
  else
  {
    DBUG_PRINT("info", ("Update from partition %d to partition %d",
			old_part_id, new_part_id));
    if ((error= m_file[new_part_id]->write_row(new_data)))
      DBUG_RETURN(error);
    if ((error= m_file[old_part_id]->delete_row(old_data)))
    {
#ifdef IN_THE_FUTURE
      (void) m_file[new_part_id]->delete_last_inserted_row(new_data);
#endif
      DBUG_RETURN(error);
    }
  }
  DBUG_RETURN(0);
}


/*
  This will delete a row. buf will contain a copy of the row to be deleted.
  The server will call this right after the current row has been read
  (from either a previous rnd_xxx() or index_xxx() call).
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier.
  Keep in mind that the server does no guarentee consecutive deletions.
  ORDER BY clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table information.
  Called in sql_delete.cc, sql_insert.cc, and sql_select.cc. In sql_select
  it is used for removing duplicates while in insert it is used for REPLACE
  calls.

  buf is either record[0] or record[1]

*/

int ha_partition::delete_row(const byte *buf)
{
  uint32 part_id;
  int error;
  DBUG_ENTER("ha_partition::delete_row");

  if ((error= get_part_for_delete(buf, m_rec0, m_part_info, &part_id)))
  {
    DBUG_RETURN(error);
  }
  m_last_part= part_id;
  DBUG_RETURN(m_file[part_id]->delete_row(buf));
}


/*
  Used to delete all rows in a table. Both for cases of truncate and
  for cases where the optimizer realizes that all rows will be
  removed as a result of a SQL statement.

  Called from item_sum.cc by Item_func_group_concat::clear(),
  Item_sum_count_distinct::clear(), and Item_func_group_concat::clear().
  Called from sql_delete.cc by mysql_delete().
  Called from sql_select.cc by JOIN::reinit().
  Called from sql_union.cc by st_select_lex_unit::exec().
*/

int ha_partition::delete_all_rows()
{
  int error;
  handler **file;
  DBUG_ENTER("ha_partition::delete_all_rows");
  file= m_file;
  do
  {
    if ((error= (*file)->delete_all_rows()))
      DBUG_RETURN(error);
  } while (*(++file));
  DBUG_RETURN(0);
}

/*
  rows == 0 means we will probably insert many rows
*/

void ha_partition::start_bulk_insert(ha_rows rows)
{
  handler **file;
  DBUG_ENTER("ha_partition::start_bulk_insert");
  if (!rows)
  {
    /* Avoid allocation big caches in all underlaying handlers */
    DBUG_VOID_RETURN;
  }
  rows= rows/m_tot_parts + 1;
  file= m_file;
  do
  {
    (*file)->start_bulk_insert(rows);
  } while (*(++file));
  DBUG_VOID_RETURN;
}


int ha_partition::end_bulk_insert()
{
  int error= 0;
  handler **file;
  DBUG_ENTER("ha_partition::end_bulk_insert");

  file= m_file;
  do
  {
    int tmp;
    /* We want to execute end_bulk_insert() on all handlers */
    if ((tmp= (*file)->end_bulk_insert()))
      error= tmp;
  } while (*(++file));
  DBUG_RETURN(error);
}

/****************************************************************************
                MODULE full table scan
****************************************************************************/
/*
  Initialize engine for random reads

  SYNOPSIS
    ha_partition::rnd_init()
    scan	0  Initialize for random reads through rnd_pos()
		1  Initialize for random scan through rnd_next()

  NOTES
  rnd_init() is called when the server wants the storage engine to do a
  table scan or when the server wants to access data through rnd_pos.

  When scan is used we will scan one handler partition at a time.
  When preparing for rnd_pos we will init all handler partitions.
  No extra cache handling is needed when scannning is not performed.

  Before initialising we will call rnd_end to ensure that we clean up from
  any previous incarnation of a table scan.
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_partition::rnd_init(bool scan)
{
  int error;
  handler **file;
  DBUG_ENTER("ha_partition::rnd_init");

  include_partition_fields_in_used_fields();
  if (scan)
  {
    /*
      rnd_end() is needed for partitioning to reset internal data if scan
      is already in use
    */

    rnd_end();
    if (partition_scan_set_up(rec_buf(0), FALSE))
    {
      /*
        The set of partitions to scan is empty. We return success and return
        end of file on first rnd_next.
      */
      DBUG_RETURN(0);
    }
    /*
      We will use the partition set in our scan, using the start and stop
      partition and checking each scan before start dependent on bittfields.
    */
    late_extra_cache(m_part_spec.start_part);
    DBUG_PRINT("info", ("rnd_init on partition %d",m_part_spec.start_part));
    error= m_file[m_part_spec.start_part]->ha_rnd_init(1);
    m_scan_value= 1;                            // Scan active
    if (error)
      m_scan_value= 2;                          // No scan active
    DBUG_RETURN(error);
  }
  file= m_file;
  do
  {
    if ((error= (*file)->ha_rnd_init(0)))
      goto err;
  } while (*(++file));
  m_scan_value= 0;
  DBUG_RETURN(0);

err:
  while (file--)
    (*file)->ha_rnd_end();
  DBUG_RETURN(error);
}


int ha_partition::rnd_end()
{
  handler **file;
  DBUG_ENTER("ha_partition::rnd_end");
  switch (m_scan_value) {
  case 2:                                       // Error
    break;
  case 1:                                       // Table scan
    if (m_part_spec.start_part != NO_CURRENT_PART_ID)
    {
      late_extra_no_cache(m_part_spec.start_part);
      m_file[m_part_spec.start_part]->ha_rnd_end();
    }
    break;
  case 0:
    file= m_file;
    do
    {
      (*file)->ha_rnd_end();
    } while (*(++file));
    break;
  }
  m_part_spec.start_part= NO_CURRENT_PART_ID;
  m_scan_value= 2;
  DBUG_RETURN(0);
}


/*
  read next row during full table scan (scan in random row order)

  SYNOPSIS
    rnd_next()
    buf		buffer that should be filled with data

  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.
*/

int ha_partition::rnd_next(byte *buf)
{
  DBUG_ASSERT(m_scan_value);
  uint part_id= m_part_spec.start_part;         // Cache of this variable
  handler *file= m_file[part_id];
  int result= HA_ERR_END_OF_FILE;
  DBUG_ENTER("ha_partition::rnd_next");

  DBUG_ASSERT(m_scan_value == 1);

  if (part_id > m_part_spec.end_part)
  {
    /*
      The original set of partitions to scan was empty and thus we report
      the result here.
    */
    goto end;
  }
  while (TRUE)
  {
    if ((result= file->rnd_next(buf)))
    {
      if (result == HA_ERR_RECORD_DELETED)
        continue;                               // Probably MyISAM

      if (result != HA_ERR_END_OF_FILE)
        break;                                  // Return error

      /* End current partition */
      late_extra_no_cache(part_id);
      DBUG_PRINT("info", ("rnd_end on partition %d", part_id));
      if ((result= file->ha_rnd_end()))
        break;
      /* Shift to next partition */
      if (++part_id > m_part_spec.end_part)
      {
        result= HA_ERR_END_OF_FILE;
        break;
      }
      file= m_file[part_id];
      DBUG_PRINT("info", ("rnd_init on partition %d", part_id));
      if ((result= file->ha_rnd_init(1)))
        break;
      late_extra_cache(part_id);
    }
    else
    {
      m_part_spec.start_part= part_id;
      m_last_part= part_id;
      table->status= 0;
      DBUG_RETURN(0);
    }
  }

end:
  m_part_spec.start_part= NO_CURRENT_PART_ID;
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(result);
}


inline void store_part_id_in_pos(byte *pos, uint part_id)
{
  int2store(pos, part_id);
}

inline uint get_part_id_from_pos(const byte *pos)
{
  return uint2korr(pos);
}

/*
  position() is called after each call to rnd_next() if the data needs
  to be ordered. You can do something like the following to store
  the position:
  ha_store_ptr(ref, ref_length, current_position);

  The server uses ref to store data. ref_length in the above case is
  the size needed to store current_position. ref is just a byte array
  that the server will maintain. If you are using offsets to mark rows, then
  current_position should be the offset. If it is a primary key like in
  BDB, then it needs to be a primary key.

  Called from filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc.
*/

void ha_partition::position(const byte *record)
{
  handler *file= m_file[m_last_part];
  DBUG_ENTER("ha_partition::position");
  file->position(record);
  store_part_id_in_pos(ref, m_last_part);
  memcpy((ref + PARTITION_BYTES_IN_POS), file->ref,
	 (ref_length - PARTITION_BYTES_IN_POS));

#ifdef SUPPORTING_PARTITION_OVER_DIFFERENT_ENGINES
#ifdef HAVE_purify
  bzero(ref + PARTITION_BYTES_IN_POS + ref_length, max_ref_length-ref_length);
#endif /* HAVE_purify */
#endif
  DBUG_VOID_RETURN;
}

/*
  This is like rnd_next, but you are given a position to use
  to determine the row. The position will be of the type that you stored in
  ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
  or position you saved when position() was called.
  Called from filesort.cc records.cc sql_insert.cc sql_select.cc
  sql_update.cc.
*/

int ha_partition::rnd_pos(byte * buf, byte *pos)
{
  uint part_id;
  handler *file;
  DBUG_ENTER("ha_partition::rnd_pos");

  part_id= get_part_id_from_pos((const byte *) pos);
  DBUG_ASSERT(part_id < m_tot_parts);
  file= m_file[part_id];
  m_last_part= part_id;
  DBUG_RETURN(file->rnd_pos(buf, (pos + PARTITION_BYTES_IN_POS)));
}


/****************************************************************************
                MODULE index scan
****************************************************************************/
/*
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index.

  There are loads of optimisations possible here for the partition handler.
  The same optimisations can also be checked for full table scan although
  only through conditions and not from index ranges.
  Phase one optimisations:
    Check if the fields of the partition function are bound. If so only use
    the single partition it becomes bound to.
  Phase two optimisations:
    If it can be deducted through range or list partitioning that only a
    subset of the partitions are used, then only use those partitions.
*/

/*
  index_init is always called before starting index scans (except when
  starting through index_read_idx and using read_range variants).
*/

int ha_partition::index_init(uint inx, bool sorted)
{
  int error= 0;
  handler **file;
  DBUG_ENTER("ha_partition::index_init");

  active_index= inx;
  m_part_spec.start_part= NO_CURRENT_PART_ID;
  m_start_key.length= 0;
  m_ordered= sorted;
  m_curr_key_info= table->key_info+inx;
  include_partition_fields_in_used_fields();

  file= m_file;
  do
  {
    /* TODO RONM: Change to index_init() when code is stable */
    if ((error= (*file)->ha_index_init(inx, sorted)))
    {
      DBUG_ASSERT(0);                           // Should never happen
      break;
    }
  } while (*(++file));
  DBUG_RETURN(error);
}


/*
  index_end is called at the end of an index scan to clean up any
  things needed to clean up.
*/

int ha_partition::index_end()
{
  int error= 0;
  handler **file;
  DBUG_ENTER("ha_partition::index_end");

  active_index= MAX_KEY;
  m_part_spec.start_part= NO_CURRENT_PART_ID;
  file= m_file;
  do
  {
    int tmp;
    /* We want to execute index_end() on all handlers */
    /* TODO RONM: Change to index_end() when code is stable */
    if ((tmp= (*file)->ha_index_end()))
      error= tmp;
  } while (*(++file));
  DBUG_RETURN(error);
}


/*
  index_read starts a new index scan using a start key. The MySQL Server
  will check the end key on its own. Thus to function properly the
  partitioned handler need to ensure that it delivers records in the sort
  order of the MySQL Server.
  index_read can be restarted without calling index_end on the previous
  index scan and without calling index_init. In this case the index_read
  is on the same index as the previous index_scan. This is particularly
  used in conjuntion with multi read ranges.
*/

int ha_partition::index_read(byte * buf, const byte * key,
			     uint key_len, enum ha_rkey_function find_flag)
{
  DBUG_ENTER("ha_partition::index_read");
  end_range= 0;
  DBUG_RETURN(common_index_read(buf, key, key_len, find_flag));
}


int ha_partition::common_index_read(byte *buf, const byte *key, uint key_len,
				    enum ha_rkey_function find_flag)
{
  int error;
  DBUG_ENTER("ha_partition::common_index_read");

  memcpy((void*)m_start_key.key, key, key_len);
  m_start_key.length= key_len;
  m_start_key.flag= find_flag;
  m_index_scan_type= partition_index_read;

  if ((error= partition_scan_set_up(buf, TRUE)))
  {
    DBUG_RETURN(error);
  }

  if (!m_ordered_scan_ongoing ||
      (find_flag == HA_READ_KEY_EXACT &&
       (key_len >= m_curr_key_info->key_length ||
	key_len == 0)))
  {
    /*
      We use unordered index scan either when read_range is used and flag
      is set to not use ordered or when an exact key is used and in this
      case all records will be sorted equal and thus the sort order of the
      resulting records doesn't matter.
      We also use an unordered index scan when the number of partitions to
      scan is only one.
      The unordered index scan will use the partition set created.
      Need to set unordered scan ongoing since we can come here even when
      it isn't set.
    */
    m_ordered_scan_ongoing= FALSE;
    error= handle_unordered_scan_next_partition(buf);
  }
  else
  {
    /*
      In all other cases we will use the ordered index scan. This will use
      the partition set created by the get_partition_set method.
    */
    error= handle_ordered_index_scan(buf);
  }
  DBUG_RETURN(error);
}


/*
  index_first() asks for the first key in the index.
  This is similar to index_read except that there is no start key since
  the scan starts from the leftmost entry and proceeds forward with
  index_next.

  Called from opt_range.cc, opt_sum.cc, sql_handler.cc,
  and sql_select.cc.
*/

int ha_partition::index_first(byte * buf)
{
  DBUG_ENTER("ha_partition::index_first");
  end_range= 0;
  m_index_scan_type= partition_index_first;
  DBUG_RETURN(common_first_last(buf));
}


/*
  index_last() asks for the last key in the index.
  This is similar to index_read except that there is no start key since
  the scan starts from the rightmost entry and proceeds forward with
  index_prev.

  Called from opt_range.cc, opt_sum.cc, sql_handler.cc,
  and sql_select.cc.
*/

int ha_partition::index_last(byte * buf)
{
  DBUG_ENTER("ha_partition::index_last");
  m_index_scan_type= partition_index_last;
  DBUG_RETURN(common_first_last(buf));
}

int ha_partition::common_first_last(byte *buf)
{
  int error;
  if ((error= partition_scan_set_up(buf, FALSE)))
    return error;
  if (!m_ordered_scan_ongoing)
    return handle_unordered_scan_next_partition(buf);
  return handle_ordered_index_scan(buf);
}

/*
  Positions an index cursor to the index specified in key. Fetches the
  row if any.  This is only used to read whole keys.
  TODO: Optimise this code to avoid index_init and index_end
*/

int ha_partition::index_read_idx(byte * buf, uint index, const byte * key,
				 uint key_len,
                                 enum ha_rkey_function find_flag)
{
  int res;
  DBUG_ENTER("ha_partition::index_read_idx");
  index_init(index, 0);
  res= index_read(buf, key, key_len, find_flag);
  index_end();
  DBUG_RETURN(res);
}

/*
  This is used in join_read_last_key to optimise away an ORDER BY.
  Can only be used on indexes supporting HA_READ_ORDER
*/

int ha_partition::index_read_last(byte *buf, const byte *key, uint keylen)
{
  DBUG_ENTER("ha_partition::index_read_last");
  m_ordered= TRUE;				// Safety measure
  DBUG_RETURN(index_read(buf, key, keylen, HA_READ_PREFIX_LAST));
}


/*
  Used to read forward through the index.
*/

int ha_partition::index_next(byte * buf)
{
  DBUG_ENTER("ha_partition::index_next");
  /*
    TODO(low priority):
    If we want partition to work with the HANDLER commands, we
    must be able to do index_last() -> index_prev() -> index_next()
  */
  DBUG_ASSERT(m_index_scan_type != partition_index_last);
  if (!m_ordered_scan_ongoing)
  {
    DBUG_RETURN(handle_unordered_next(buf, FALSE));
  }
  DBUG_RETURN(handle_ordered_next(buf, FALSE));
}


/*
  This routine is used to read the next but only if the key is the same
  as supplied in the call.
*/

int ha_partition::index_next_same(byte *buf, const byte *key, uint keylen)
{
  DBUG_ENTER("ha_partition::index_next_same");
  DBUG_ASSERT(keylen == m_start_key.length);
  DBUG_ASSERT(m_index_scan_type != partition_index_last);
  if (!m_ordered_scan_ongoing)
    DBUG_RETURN(handle_unordered_next(buf, TRUE));
  DBUG_RETURN(handle_ordered_next(buf, TRUE));
}

/*
  Used to read backwards through the index.
*/

int ha_partition::index_prev(byte * buf)
{
  DBUG_ENTER("ha_partition::index_prev");
  /* TODO: read comment in index_next */
  DBUG_ASSERT(m_index_scan_type != partition_index_first);
  DBUG_RETURN(handle_ordered_prev(buf));
}


/*
  We reimplement read_range_first since we don't want the compare_key
  check at the end. This is already performed in the partition handler.
  read_range_next is very much different due to that we need to scan
  all underlying handlers.
*/

int ha_partition::read_range_first(const key_range *start_key,
				   const key_range *end_key,
				   bool eq_range_arg, bool sorted)
{
  int error;
  DBUG_ENTER("ha_partition::read_range_first");
  m_ordered= sorted;
  eq_range= eq_range_arg;
  end_range= 0;
  if (end_key)
  {
    end_range= &save_end_range;
    save_end_range= *end_key;
    key_compare_result_on_equal=
      ((end_key->flag == HA_READ_BEFORE_KEY) ? 1 :
       (end_key->flag == HA_READ_AFTER_KEY) ? -1 : 0);
  }
  range_key_part= m_curr_key_info->key_part;

  if (!start_key)				// Read first record
  {
    m_index_scan_type= partition_index_first;
    error= common_first_last(m_rec0);
  }
  else
  {
    error= common_index_read(m_rec0,
			     start_key->key,
			     start_key->length, start_key->flag);
  }
  DBUG_RETURN(error);
}


int ha_partition::read_range_next()
{
  DBUG_ENTER("ha_partition::read_range_next");
  if (m_ordered)
  {
    DBUG_RETURN(handler::read_range_next());
  }
  DBUG_RETURN(handle_unordered_next(m_rec0, eq_range));
}


int ha_partition::partition_scan_set_up(byte * buf, bool idx_read_flag)
{
  DBUG_ENTER("ha_partition::partition_scan_set_up");

  if (idx_read_flag)
    get_partition_set(table,buf,active_index,&m_start_key,&m_part_spec);
  else
    get_partition_set(table, buf, MAX_KEY, 0, &m_part_spec);
  if (m_part_spec.start_part > m_part_spec.end_part)
  {
    /*
      We discovered a partition set but the set was empty so we report
      key not found.
    */
    DBUG_PRINT("info", ("scan with no partition to scan"));
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  if (m_part_spec.start_part == m_part_spec.end_part)
  {
    /*
      We discovered a single partition to scan, this never needs to be
      performed using the ordered index scan.
    */
    DBUG_PRINT("info", ("index scan using the single partition %d",
			m_part_spec.start_part));
    m_ordered_scan_ongoing= FALSE;
  }
  else
  {
    /*
      Set m_ordered_scan_ongoing according how the scan should be done
    */
    m_ordered_scan_ongoing= m_ordered;
  }
  DBUG_ASSERT(m_part_spec.start_part < m_tot_parts &&
              m_part_spec.end_part < m_tot_parts);
  DBUG_RETURN(0);
}


/****************************************************************************
  Unordered Index Scan Routines
****************************************************************************/
/*
  These routines are used to scan partitions without considering order.
  This is performed in two situations.
  1) In read_multi_range this is the normal case
  2) When performing any type of index_read, index_first, index_last where
  all fields in the partition function is bound. In this case the index
  scan is performed on only one partition and thus it isn't necessary to
  perform any sort.
*/

int ha_partition::handle_unordered_next(byte *buf, bool next_same)
{
  handler *file= file= m_file[m_part_spec.start_part];
  int error;
  DBUG_ENTER("ha_partition::handle_unordered_next");

  /*
    We should consider if this should be split into two functions as
    next_same is alwas a local constant
  */
  if (next_same)
  {
    if (!(error= file->index_next_same(buf, m_start_key.key,
                                       m_start_key.length)))
    {
      m_last_part= m_part_spec.start_part;
      DBUG_RETURN(0);
    }
  }
  else if (!(error= file->index_next(buf)))
  {
    if (compare_key(end_range) <= 0)
    {
      m_last_part= m_part_spec.start_part;
      DBUG_RETURN(0);                           // Row was in range
    }
    error= HA_ERR_END_OF_FILE;
  }

  if (error == HA_ERR_END_OF_FILE)
  {
    m_part_spec.start_part++;                    // Start using next part
    error= handle_unordered_scan_next_partition(buf);
  }
  DBUG_RETURN(error);
}


/*
  This routine is used to start the index scan on the next partition.
  Both initial start and after completing scan on one partition.
*/

int ha_partition::handle_unordered_scan_next_partition(byte * buf)
{
  uint i;
  DBUG_ENTER("ha_partition::handle_unordered_scan_next_partition");

  for (i= m_part_spec.start_part; i <= m_part_spec.end_part; i++)
  {
    int error;
    handler *file= m_file[i];

    m_part_spec.start_part= i;
    switch (m_index_scan_type) {
    case partition_index_read:
      DBUG_PRINT("info", ("index_read on partition %d", i));
      error= file->index_read(buf, m_start_key.key,
			      m_start_key.length,
			      m_start_key.flag);
      break;
    case partition_index_first:
      DBUG_PRINT("info", ("index_first on partition %d", i));
      error= file->index_first(buf);
      break;
    default:
      DBUG_ASSERT(FALSE);
      DBUG_RETURN(1);
    }
    if (!error)
    {
      if (compare_key(end_range) <= 0)
      {
        m_last_part= i;
	DBUG_RETURN(0);
      }
      error= HA_ERR_END_OF_FILE;
    }
    if ((error != HA_ERR_END_OF_FILE) && (error != HA_ERR_KEY_NOT_FOUND))
      DBUG_RETURN(error);
    DBUG_PRINT("info", ("HA_ERR_END_OF_FILE on partition %d", i));
  }
  m_part_spec.start_part= NO_CURRENT_PART_ID;
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


/*
  This part contains the logic to handle index scans that require ordered
  output. This includes all except those started by read_range_first with
  the flag ordered set to FALSE. Thus most direct index_read and all
  index_first and index_last.

  We implement ordering by keeping one record plus a key buffer for each
  partition. Every time a new entry is requested we will fetch a new
  entry from the partition that is currently not filled with an entry.
  Then the entry is put into its proper sort position.

  Returning a record is done by getting the top record, copying the
  record to the request buffer and setting the partition as empty on
  entries.
*/

int ha_partition::handle_ordered_index_scan(byte *buf)
{
  uint i, j= 0;
  bool found= FALSE;
  bool reverse_order= FALSE;
  DBUG_ENTER("ha_partition::handle_ordered_index_scan");

  m_top_entry= NO_CURRENT_PART_ID;
  queue_remove_all(&queue);
  for (i= m_part_spec.start_part; i <= m_part_spec.end_part; i++)
  {
    int error;
    byte *rec_buf_ptr= rec_buf(i);
    handler *file= m_file[i];

    switch (m_index_scan_type) {
    case partition_index_read:
      error= file->index_read(rec_buf_ptr,
			      m_start_key.key,
			      m_start_key.length,
			      m_start_key.flag);
      reverse_order= FALSE;
      break;
    case partition_index_first:
      error= file->index_first(rec_buf_ptr);
      reverse_order= FALSE;
      break;
    case partition_index_last:
      error= file->index_last(rec_buf_ptr);
      reverse_order= TRUE;
      break;
    default:
      DBUG_ASSERT(FALSE);
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    if (!error)
    {
      found= TRUE;
      /*
        Initialise queue without order first, simply insert
      */
      queue_element(&queue, j++)= (byte*)queue_buf(i);
    }
    else if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    {
      DBUG_RETURN(error);
    }
  }
  if (found)
  {
    /*
      We found at least one partition with data, now sort all entries and
      after that read the first entry and copy it to the buffer to return in.
    */
    queue_set_max_at_top(&queue, reverse_order);
    queue_set_cmp_arg(&queue, (void*)m_curr_key_info);
    queue.elements= j;
    queue_fix(&queue);
    return_top_record(buf);
    DBUG_PRINT("info", ("Record returned from partition %d", m_top_entry));
    DBUG_RETURN(0);
  }
  DBUG_RETURN(HA_ERR_END_OF_FILE);
}


void ha_partition::return_top_record(byte *buf)
{
  uint part_id;
  byte *key_buffer= queue_top(&queue);
  byte *rec_buffer= key_buffer + PARTITION_BYTES_IN_POS;
  part_id= uint2korr(key_buffer);
  memcpy(buf, rec_buffer, m_rec_length);
  m_last_part= part_id;
  m_top_entry= part_id;
}


int ha_partition::handle_ordered_next(byte *buf, bool next_same)
{
  int error;
  uint part_id= m_top_entry;
  handler *file= m_file[part_id];
  DBUG_ENTER("ha_partition::handle_ordered_next");

  if (!next_same)
    error= file->index_next(rec_buf(part_id));
  else
    error= file->index_next_same(rec_buf(part_id), m_start_key.key,
				 m_start_key.length);
  if (error)
  {
    if (error == HA_ERR_END_OF_FILE)
    {
      /* Return next buffered row */
      queue_remove(&queue, (uint) 0);
      if (queue.elements)
      {
         DBUG_PRINT("info", ("Record returned from partition %u (2)",
                     m_top_entry));
         return_top_record(buf);
         error= 0;
      }
    }
    DBUG_RETURN(error);
  }
  queue_replaced(&queue);
  return_top_record(buf);
  DBUG_PRINT("info", ("Record returned from partition %u", m_top_entry));
  DBUG_RETURN(0);
}


int ha_partition::handle_ordered_prev(byte *buf)
{
  int error;
  uint part_id= m_top_entry;
  handler *file= m_file[part_id];
  DBUG_ENTER("ha_partition::handle_ordered_prev");
  if ((error= file->index_prev(rec_buf(part_id))))
  {
    if (error == HA_ERR_END_OF_FILE)
    {
      queue_remove(&queue, (uint) 0);
      if (queue.elements)
      {
	return_top_record(buf);
	DBUG_PRINT("info", ("Record returned from partition %d (2)",
			    m_top_entry));
        error= 0;
      }
    }
    DBUG_RETURN(error);
  }
  queue_replaced(&queue);
  return_top_record(buf);
  DBUG_PRINT("info", ("Record returned from partition %d", m_top_entry));
  DBUG_RETURN(0);
}


void ha_partition::include_partition_fields_in_used_fields()
{
  DBUG_ENTER("ha_partition::include_partition_fields_in_used_fields");
  Field **ptr= m_part_field_array;
  do
  {
    ha_set_bit_in_read_set((*ptr)->fieldnr);
  } while (*(++ptr));
  DBUG_VOID_RETURN;
}


/****************************************************************************
                MODULE information calls
****************************************************************************/

/*
  These are all first approximations of the extra, info, scan_time
  and read_time calls
*/

/*
  ::info() is used to return information to the optimizer.
  Currently this table handler doesn't implement most of the fields
  really needed. SHOW also makes use of this data
  Another note, if your handler doesn't proved exact record count,
  you will probably want to have the following in your code:
  if (records < 2)
    records = 2;
  The reason is that the server will optimize for cases of only a single
  record. If in a table scan you don't know the number of records
  it will probably be better to set records to two so you can return
  as many records as you need.

  Along with records a few more variables you may wish to set are:
    records
    deleted
    data_file_length
    index_file_length
    delete_length
    check_time
  Take a look at the public variables in handler.h for more information.

  Called in:
    filesort.cc
    ha_heap.cc
    item_sum.cc
    opt_sum.cc
    sql_delete.cc
    sql_delete.cc
    sql_derived.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_select.cc
    sql_show.cc
    sql_show.cc
    sql_show.cc
    sql_show.cc
    sql_table.cc
    sql_union.cc
    sql_update.cc

  Some flags that are not implemented
    HA_STATUS_POS:
      This parameter is never used from the MySQL Server. It is checked in a
      place in MyISAM so could potentially be used by MyISAM specific programs.
    HA_STATUS_NO_LOCK:
    This is declared and often used. It's only used by MyISAM.
    It means that MySQL doesn't need the absolute latest statistics
    information. This may save the handler from doing internal locks while
    retrieving statistics data.
*/

void ha_partition::info(uint flag)
{
  handler *file, **file_array;
  DBUG_ENTER("ha_partition:info");

  if (flag & HA_STATUS_AUTO)
  {
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    /*
      The auto increment value is only maintained by the first handler
      so we will only call this.
    */
    m_file[0]->info(HA_STATUS_AUTO);
  }
  if (flag & HA_STATUS_VARIABLE)
  {
    DBUG_PRINT("info", ("HA_STATUS_VARIABLE"));
    /*
      Calculates statistical variables
      records:           Estimate of number records in table
      We report sum (always at least 2)
      deleted:           Estimate of number holes in the table due to
      deletes
      We report sum
      data_file_length:  Length of data file, in principle bytes in table
      We report sum
      index_file_length: Length of index file, in principle bytes in
      indexes in the table
      We report sum
      mean_record_length:Mean record length in the table
      We calculate this
      check_time:        Time of last check (only applicable to MyISAM)
      We report last time of all underlying handlers
    */
    records= 0;
    deleted= 0;
    data_file_length= 0;
    index_file_length= 0;
    check_time= 0;
    file_array= m_file;
    do
    {
      file= *file_array;
      file->info(HA_STATUS_VARIABLE);
      records+= file->records;
      deleted+= file->deleted;
      data_file_length+= file->data_file_length;
      index_file_length+= file->index_file_length;
      if (file->check_time > check_time)
	check_time= file->check_time;
    } while (*(++file_array));
    if (records < 2 &&
        m_table_flags & HA_NOT_EXACT_COUNT)
      records= 2;
    if (records > 0)
      mean_rec_length= (ulong) (data_file_length / records);
    else
      mean_rec_length= 1; //? What should we set here 
  }
  if (flag & HA_STATUS_CONST)
  {
    DBUG_PRINT("info", ("HA_STATUS_CONST"));
    /*
      Recalculate loads of constant variables. MyISAM also sets things
      directly on the table share object.

      Check whether this should be fixed since handlers should not
      change things directly on the table object.

      Monty comment: This should NOT be changed!  It's the handlers
      responsibility to correct table->s->keys_xxxx information if keys
      have been disabled.

      The most important parameters set here is records per key on
      all indexes. block_size and primar key ref_length.

      For each index there is an array of rec_per_key.
      As an example if we have an index with three attributes a,b and c
      we will have an array of 3 rec_per_key.
      rec_per_key[0] is an estimate of number of records divided by
      number of unique values of the field a.
      rec_per_key[1] is an estimate of the number of records divided
      by the number of unique combinations of the fields a and b.
      rec_per_key[2] is an estimate of the number of records divided
      by the number of unique combinations of the fields a,b and c.

      Many handlers only set the value of rec_per_key when all fields
      are bound (rec_per_key[2] in the example above).

      If the handler doesn't support statistics, it should set all of the
      above to 0.

      We will allow the first handler to set the rec_per_key and use
      this as an estimate on the total table.

      max_data_file_length:     Maximum data file length
      We ignore it, is only used in
      SHOW TABLE STATUS
      max_index_file_length:    Maximum index file length
      We ignore it since it is never used
      block_size:               Block size used
      We set it to the value of the first handler
      sortkey:                  Never used at any place so ignored
      ref_length:               We set this to the value calculated
      and stored in local object
      raid_type:                Set by first handler (MyISAM)
      raid_chunks:              Set by first handler (MyISAM)
      raid_chunksize:           Set by first handler (MyISAM)
      create_time:              Creation time of table
      Set by first handler

      So we calculate these constants by using the variables on the first
      handler.
    */

    file= m_file[0];
    file->info(HA_STATUS_CONST);
    create_time= file->create_time;
    raid_type= file->raid_type;
    raid_chunks= file->raid_chunks;
    raid_chunksize= file->raid_chunksize;
    ref_length= m_ref_length;
  }
  if (flag & HA_STATUS_ERRKEY)
  {
    handler *file= m_file[m_last_part];
    DBUG_PRINT("info", ("info: HA_STATUS_ERRKEY"));
    /*
      This flag is used to get index number of the unique index that
      reported duplicate key
      We will report the errkey on the last handler used and ignore the rest
    */
    file->info(HA_STATUS_ERRKEY);
    if (file->errkey != (uint) -1)
      errkey= file->errkey;
  }
  if (flag & HA_STATUS_TIME)
  {
    DBUG_PRINT("info", ("info: HA_STATUS_TIME"));
    /*
      This flag is used to set the latest update time of the table.
      Used by SHOW commands
      We will report the maximum of these times
    */
    update_time= 0;
    file_array= m_file;
    do
    {
      file= *file_array;
      file->info(HA_STATUS_TIME);
      if (file->update_time > update_time)
	update_time= file->update_time;
    } while (*(++file_array));
  }
  DBUG_VOID_RETURN;
}


/*
  extra() is called whenever the server wishes to send a hint to
  the storage engine. The MyISAM engine implements the most hints.

  We divide the parameters into the following categories:
  1) Parameters used by most handlers
  2) Parameters used by some non-MyISAM handlers
  3) Parameters used only by MyISAM
  4) Parameters only used by temporary tables for query processing
  5) Parameters only used by MyISAM internally
  6) Parameters not used at all

  The partition handler need to handle category 1), 2) and 3).

  1) Parameters used by most handlers
  -----------------------------------
  HA_EXTRA_RESET:
    This option is used by most handlers and it resets the handler state
    to the same state as after an open call. This includes releasing
    any READ CACHE or WRITE CACHE or other internal buffer used.

    It is called from the reset method in the handler interface. There are
    three instances where this is called.
    1) After completing a INSERT ... SELECT ... query the handler for the
       table inserted into is reset
    2) It is called from close_thread_table which in turn is called from
       close_thread_tables except in the case where the tables are locked
       in which case ha_commit_stmt is called instead.
       It is only called from here if flush_version hasn't changed and the
       table is not an old table when calling close_thread_table.
       close_thread_tables is called from many places as a general clean up
       function after completing a query.
    3) It is called when deleting the QUICK_RANGE_SELECT object if the
       QUICK_RANGE_SELECT object had its own handler object. It is called
       immediatley before close of this local handler object.
  HA_EXTRA_KEYREAD:
  HA_EXTRA_NO_KEYREAD:
    These parameters are used to provide an optimisation hint to the handler.
    If HA_EXTRA_KEYREAD is set it is enough to read the index fields, for
    many handlers this means that the index-only scans can be used and it
    is not necessary to use the real records to satisfy this part of the
    query. Index-only scans is a very important optimisation for disk-based
    indexes. For main-memory indexes most indexes contain a reference to the
    record and thus KEYREAD only says that it is enough to read key fields.
    HA_EXTRA_NO_KEYREAD disables this for the handler, also HA_EXTRA_RESET
    will disable this option.
    The handler will set HA_KEYREAD_ONLY in its table flags to indicate this
    feature is supported.
  HA_EXTRA_FLUSH:
    Indication to flush tables to disk, called at close_thread_table to
    ensure disk based tables are flushed at end of query execution.

  2) Parameters used by some non-MyISAM handlers
  ----------------------------------------------
  HA_EXTRA_RETRIEVE_ALL_COLS:
    Many handlers have implemented optimisations to avoid fetching all
    fields when retrieving data. In certain situations all fields need
    to be retrieved even though the query_id is not set on all field
    objects.

    It is called from copy_data_between_tables where all fields are
    copied without setting query_id before calling the handlers.
    It is called from UPDATE statements when the fields of the index
    used is updated or ORDER BY is used with UPDATE.
    And finally when calculating checksum of a table using the CHECKSUM
    command.
  HA_EXTRA_RETRIEVE_PRIMARY_KEY:
    In some situations it is mandatory to retrieve primary key fields
    independent of the query id's. This extra flag specifies that fetch
    of primary key fields is mandatory.
  HA_EXTRA_KEYREAD_PRESERVE_FIELDS:
    This is a strictly InnoDB feature that is more or less undocumented.
    When it is activated InnoDB copies field by field from its fetch
    cache instead of all fields in one memcpy. Have no idea what the
    purpose of this is.
    Cut from include/my_base.h:
    When using HA_EXTRA_KEYREAD, overwrite only key member fields and keep
    other fields intact. When this is off (by default) InnoDB will use memcpy
    to overwrite entire row.
  HA_EXTRA_IGNORE_DUP_KEY:
  HA_EXTRA_NO_IGNORE_DUP_KEY:
    Informs the handler to we will not stop the transaction if we get an
    duplicate key errors during insert/upate.
    Always called in pair, triggered by INSERT IGNORE and other similar
    SQL constructs.
    Not used by MyISAM.

  3) Parameters used only by MyISAM
  ---------------------------------
  HA_EXTRA_NORMAL:
    Only used in MyISAM to reset quick mode, not implemented by any other
    handler. Quick mode is also reset in MyISAM by HA_EXTRA_RESET.

    It is called after completing a successful DELETE query if the QUICK
    option is set.

  HA_EXTRA_QUICK:
    When the user does DELETE QUICK FROM table where-clause; this extra
    option is called before the delete query is performed and
    HA_EXTRA_NORMAL is called after the delete query is completed.
    Temporary tables used internally in MySQL always set this option

    The meaning of quick mode is that when deleting in a B-tree no merging
    of leafs is performed. This is a common method and many large DBMS's
    actually only support this quick mode since it is very difficult to
    merge leaves in a tree used by many threads concurrently.

  HA_EXTRA_CACHE:
    This flag is usually set with extra_opt along with a cache size.
    The size of this buffer is set by the user variable
    record_buffer_size. The value of this cache size is the amount of
    data read from disk in each fetch when performing a table scan.
    This means that before scanning a table it is normal to call
    extra with HA_EXTRA_CACHE and when the scan is completed to call
    HA_EXTRA_NO_CACHE to release the cache memory.

    Some special care is taken when using this extra parameter since there
    could be a write ongoing on the table in the same statement. In this
    one has to take special care since there might be a WRITE CACHE as
    well. HA_EXTRA_CACHE specifies using a READ CACHE and using
    READ CACHE and WRITE CACHE at the same time is not possible.

    Only MyISAM currently use this option.

    It is set when doing full table scans using rr_sequential and
    reset when completing such a scan with end_read_record
    (resetting means calling extra with HA_EXTRA_NO_CACHE).

    It is set in filesort.cc for MyISAM internal tables and it is set in
    a multi-update where HA_EXTRA_CACHE is called on a temporary result
    table and after that ha_rnd_init(0) on table to be updated
    and immediately after that HA_EXTRA_NO_CACHE on table to be updated.

    Apart from that it is always used from init_read_record but not when
    used from UPDATE statements. It is not used from DELETE statements
    with ORDER BY and LIMIT but it is used in normal scan loop in DELETE
    statements. The reason here is that DELETE's in MyISAM doesn't move
    existings data rows.

    It is also set in copy_data_between_tables when scanning the old table
    to copy over to the new table.
    And it is set in join_init_read_record where quick objects are used
    to perform a scan on the table. In this case the full table scan can
    even be performed multiple times as part of the nested loop join.

    For purposes of the partition handler it is obviously necessary to have
    special treatment of this extra call. If we would simply pass this
    extra call down to each handler we would allocate
    cache size * no of partitions amount of memory and this is not
    necessary since we will only scan one partition at a time when doing
    full table scans.

    Thus we treat it by first checking whether we have MyISAM handlers in
    the table, if not we simply ignore the call and if we have we will
    record the call but will not call any underlying handler yet. Then
    when performing the sequential scan we will check this recorded value
    and call extra_opt whenever we start scanning a new partition.

    monty: Neads to be fixed so that it's passed to all handlers when we
    move to another partition during table scan.

  HA_EXTRA_NO_CACHE:
    When performing a UNION SELECT HA_EXTRA_NO_CACHE is called from the
    flush method in the select_union class.
    It is used to some extent when insert delayed inserts.
    See HA_EXTRA_RESET_STATE for use in conjunction with delete_all_rows().

    It should be ok to call HA_EXTRA_NO_CACHE on all underlying handlers
    if they are MyISAM handlers. Other handlers we can ignore the call
    for. If no cache is in use they will quickly return after finding
    this out. And we also ensure that all caches are disabled and no one
    is left by mistake.
    In the future this call will probably be deleted an we will instead call
    ::reset();

  HA_EXTRA_WRITE_CACHE:
    See above, called from various places. It is mostly used when we
    do INSERT ... SELECT
    No special handling to save cache space is developed currently.

  HA_EXTRA_PREPARE_FOR_UPDATE:
    This is called as part of a multi-table update. When the table to be
    updated is also scanned then this informs MyISAM handler to drop any
    caches if dynamic records are used (fixed size records do not care
    about this call). We pass this along to all underlying MyISAM handlers
    and ignore it for the rest.

  HA_EXTRA_PREPARE_FOR_DELETE:
    Only used by MyISAM, called in preparation for a DROP TABLE.
    It's used mostly by Windows that cannot handle dropping an open file.
    On other platforms it has the same effect as HA_EXTRA_FORCE_REOPEN.

  HA_EXTRA_READCHECK:
  HA_EXTRA_NO_READCHECK:
    Only one call to HA_EXTRA_NO_READCHECK from ha_open where it says that
    this is not needed in SQL. The reason for this call is that MyISAM sets
    the READ_CHECK_USED in the open call so the call is needed for MyISAM
    to reset this feature.
    The idea with this parameter was to inform of doing/not doing a read
    check before applying an update. Since SQL always performs a read before
    applying the update No Read Check is needed in MyISAM as well.

    This is a cut from Docs/myisam.txt
     Sometimes you might want to force an update without checking whether
     another user has changed the record since you last read it. This is
     somewhat dangerous, so it should ideally not be used. That can be
     accomplished by wrapping the mi_update() call in two calls to mi_extra(),
     using these functions:
     HA_EXTRA_NO_READCHECK=5                 No readcheck on update
     HA_EXTRA_READCHECK=6                    Use readcheck (def)

  HA_EXTRA_FORCE_REOPEN:
    Only used by MyISAM, called when altering table, closing tables to
    enforce a reopen of the table files.

  4) Parameters only used by temporary tables for query processing
  ----------------------------------------------------------------
  HA_EXTRA_RESET_STATE:
    Same as HA_EXTRA_RESET except that buffers are not released. If there is
    a READ CACHE it is reinit'ed. A cache is reinit'ed to restart reading
    or to change type of cache between READ CACHE and WRITE CACHE.

    This extra function is always called immediately before calling
    delete_all_rows on the handler for temporary tables.
    There are cases however when HA_EXTRA_RESET_STATE isn't called in
    a similar case for a temporary table in sql_union.cc and in two other
    cases HA_EXTRA_NO_CACHE is called before and HA_EXTRA_WRITE_CACHE
    called afterwards.
    The case with HA_EXTRA_NO_CACHE and HA_EXTRA_WRITE_CACHE means
    disable caching, delete all rows and enable WRITE CACHE. This is
    used for temporary tables containing distinct sums and a
    functional group.

    The only case that delete_all_rows is called on non-temporary tables
    is in sql_delete.cc when DELETE FROM table; is called by a user.
    In this case no special extra calls are performed before or after this
    call.

    The partition handler should not need to bother about this one. It
    should never be called.

  HA_EXTRA_NO_ROWS:
    Don't insert rows indication to HEAP and MyISAM, only used by temporary
    tables used in query processing.
    Not handled by partition handler.

  5) Parameters only used by MyISAM internally
  --------------------------------------------
  HA_EXTRA_REINIT_CACHE:
    This call reinitialises the READ CACHE described above if there is one
    and otherwise the call is ignored.

    We can thus safely call it on all underlying handlers if they are
    MyISAM handlers. It is however never called so we don't handle it at all.
  HA_EXTRA_FLUSH_CACHE:
    Flush WRITE CACHE in MyISAM. It is only from one place in the code.
    This is in sql_insert.cc where it is called if the table_flags doesn't
    contain HA_DUPP_POS. The only handler having the HA_DUPP_POS set is the
    MyISAM handler and so the only handler not receiving this call is MyISAM.
    Thus in effect this call is called but never used. Could be removed
    from sql_insert.cc
  HA_EXTRA_NO_USER_CHANGE:
    Only used by MyISAM, never called.
    Simulates lock_type as locked.
  HA_EXTRA_WAIT_LOCK:
  HA_EXTRA_WAIT_NOLOCK:
    Only used by MyISAM, called from MyISAM handler but never from server
    code on top of the handler.
    Sets lock_wait on/off
  HA_EXTRA_NO_KEYS:
    Only used MyISAM, only used internally in MyISAM handler, never called
    from server level.
  HA_EXTRA_KEYREAD_CHANGE_POS:
  HA_EXTRA_REMEMBER_POS:
  HA_EXTRA_RESTORE_POS:
  HA_EXTRA_PRELOAD_BUFFER_SIZE:
  HA_EXTRA_CHANGE_KEY_TO_DUP:
  HA_EXTRA_CHANGE_KEY_TO_UNIQUE:
    Only used by MyISAM, never called.

  6) Parameters not used at all
  -----------------------------
  HA_EXTRA_KEY_CACHE:
  HA_EXTRA_NO_KEY_CACHE:
    This parameters are no longer used and could be removed.
*/

int ha_partition::extra(enum ha_extra_function operation)
{
  DBUG_ENTER("ha_partition:extra");
  DBUG_PRINT("info", ("operation: %d", (int) operation));

  switch (operation) {
    /* Category 1), used by most handlers */
  case HA_EXTRA_KEYREAD:
  case HA_EXTRA_NO_KEYREAD:
  case HA_EXTRA_FLUSH:
    DBUG_RETURN(loop_extra(operation));

    /* Category 2), used by non-MyISAM handlers */
  case HA_EXTRA_IGNORE_DUP_KEY:
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
  case HA_EXTRA_RETRIEVE_ALL_COLS:
  case HA_EXTRA_RETRIEVE_PRIMARY_KEY:
  case HA_EXTRA_KEYREAD_PRESERVE_FIELDS:
  {
    if (!m_myisam)
      DBUG_RETURN(loop_extra(operation));
    break;
  }

  /* Category 3), used by MyISAM handlers */
  case HA_EXTRA_NORMAL:
  case HA_EXTRA_QUICK:
  case HA_EXTRA_NO_READCHECK:
  case HA_EXTRA_PREPARE_FOR_UPDATE:
  case HA_EXTRA_PREPARE_FOR_DELETE:
  case HA_EXTRA_FORCE_REOPEN:
  {
    if (m_myisam)
      DBUG_RETURN(loop_extra(operation));
    break;
  }
  case HA_EXTRA_CACHE:
  {
    prepare_extra_cache(0);
    break;
  }
  case HA_EXTRA_NO_CACHE:
  {
    m_extra_cache= FALSE;
    m_extra_cache_size= 0;
    DBUG_RETURN(loop_extra(operation));
  }
  default:
  {
    /* Temporary crash to discover what is wrong */
    DBUG_ASSERT(0);
    break;
  }
  }
  DBUG_RETURN(0);
}


/*
  This will in the future be called instead of extra(HA_EXTRA_RESET) as this
  is such a common call
*/

int ha_partition::reset(void)
{
  int result= 0, tmp;
  handler **file;
  DBUG_ENTER("ha_partition::reset");
  file= m_file;
  do
  {
    if ((tmp= (*file)->reset()))
      result= tmp;
  } while (*(++file));
  DBUG_RETURN(result);
}


int ha_partition::extra_opt(enum ha_extra_function operation, ulong cachesize)
{
  DBUG_ENTER("ha_partition::extra_opt()");
  DBUG_ASSERT(HA_EXTRA_CACHE == operation);
  prepare_extra_cache(cachesize);
  DBUG_RETURN(0);
}


void ha_partition::prepare_extra_cache(uint cachesize)
{
  DBUG_ENTER("ha_partition::prepare_extra_cache()");

  m_extra_cache= TRUE;
  m_extra_cache_size= cachesize;
  if (m_part_spec.start_part != NO_CURRENT_PART_ID)
  {
    DBUG_ASSERT(m_part_spec.start_part == 0);
    late_extra_cache(0);
  }
  DBUG_VOID_RETURN;
}


int ha_partition::loop_extra(enum ha_extra_function operation)
{
  int result= 0, tmp;
  handler **file;
  DBUG_ENTER("ha_partition::loop_extra()");
  for (file= m_file; *file; file++)
  {
    if ((tmp= (*file)->extra(operation)))
      result= tmp;
  }
  DBUG_RETURN(result);
}


void ha_partition::late_extra_cache(uint partition_id)
{
  handler *file;
  DBUG_ENTER("ha_partition::late_extra_cache");
  if (!m_extra_cache)
    DBUG_VOID_RETURN;
  file= m_file[partition_id];
  if (m_extra_cache_size == 0)
    VOID(file->extra(HA_EXTRA_CACHE));
  else
    VOID(file->extra_opt(HA_EXTRA_CACHE, m_extra_cache_size));
  DBUG_VOID_RETURN;
}


void ha_partition::late_extra_no_cache(uint partition_id)
{
  handler *file;
  DBUG_ENTER("ha_partition::late_extra_no_cache");
  if (!m_extra_cache)
    DBUG_VOID_RETURN;
  file= m_file[partition_id];
  VOID(file->extra(HA_EXTRA_NO_CACHE));
  DBUG_VOID_RETURN;
}


/****************************************************************************
                MODULE optimiser support
****************************************************************************/

const key_map *ha_partition::keys_to_use_for_scanning()
{
  DBUG_ENTER("ha_partition::keys_to_use_for_scanning");
  DBUG_RETURN(m_file[0]->keys_to_use_for_scanning());
}

double ha_partition::scan_time()
{
  double scan_time= 0;
  handler **file;
  DBUG_ENTER("ha_partition::scan_time");

  for (file= m_file; *file; file++)
    scan_time+= (*file)->scan_time();
  DBUG_RETURN(scan_time);
}


/*
  This will be optimised later to include whether or not the index can
  be used with partitioning. To achieve we need to add another parameter
  that specifies how many of the index fields that are bound in the ranges.
  Possibly added as a new call to handlers.
*/

double ha_partition::read_time(uint index, uint ranges, ha_rows rows)
{
  DBUG_ENTER("ha_partition::read_time");
  DBUG_RETURN(m_file[0]->read_time(index, ranges, rows));
}

/*
  Given a starting key, and an ending key estimate the number of rows that
  will exist between the two. end_key may be empty which in case determine
  if start_key matches any rows.

  Called from opt_range.cc by check_quick_keys().

  monty: MUST be called for each range and added.
	 Note that MySQL will assume that if this returns 0 there is no
         matching rows for the range!
*/

ha_rows ha_partition::records_in_range(uint inx, key_range *min_key,
				       key_range *max_key)
{
  ha_rows in_range= 0;
  handler **file;
  DBUG_ENTER("ha_partition::records_in_range");

  file= m_file;
  do
  {
    in_range+= (*file)->records_in_range(inx, min_key, max_key);
  } while (*(++file));
  DBUG_RETURN(in_range);
}


ha_rows ha_partition::estimate_rows_upper_bound()
{
  ha_rows rows, tot_rows= 0;
  handler **file;
  DBUG_ENTER("ha_partition::estimate_rows_upper_bound");

  file= m_file;
  do
  {
    rows= (*file)->estimate_rows_upper_bound();
    if (rows == HA_POS_ERROR)
      DBUG_RETURN(HA_POS_ERROR);
    tot_rows+= rows;
  } while (*(++file));
  DBUG_RETURN(tot_rows);
}


uint8 ha_partition::table_cache_type()
{
  DBUG_ENTER("ha_partition::table_cache_type");
  DBUG_RETURN(m_file[0]->table_cache_type());
}


/****************************************************************************
                MODULE print messages
****************************************************************************/

const char *ha_partition::index_type(uint inx)
{
  DBUG_ENTER("ha_partition::index_type");
  DBUG_RETURN(m_file[0]->index_type(inx));
}


void ha_partition::print_error(int error, myf errflag)
{
  DBUG_ENTER("ha_partition::print_error");
  /* Should probably look for my own errors first */
  /* monty: needs to be called for the last used partition ! */
  m_file[0]->print_error(error, errflag);
  DBUG_VOID_RETURN;
}


bool ha_partition::get_error_message(int error, String *buf)
{
  DBUG_ENTER("ha_partition::get_error_message");
  /* Should probably look for my own errors first */
  /* monty: needs to be called for the last used partition ! */
  DBUG_RETURN(m_file[0]->get_error_message(error, buf));
}


/****************************************************************************
                MODULE handler characteristics
****************************************************************************/
/*
  If frm_error() is called then we will use this to to find out what file
  extensions exist for the storage engine. This is also used by the default
  rename_table and delete_table method in handler.cc.
*/

static const char *ha_partition_ext[]=
{
  ha_par_ext, NullS
};

const char **ha_partition::bas_ext() const
{ return ha_partition_ext; }


uint ha_partition::min_of_the_max_uint(uint (handler::*operator_func)(void) const) const
{
  handler **file;
  uint min_of_the_max= ((*m_file)->*operator_func)();

  for (file= m_file+1; *file; file++)
  {
    uint tmp= ((*file)->*operator_func)();
    set_if_smaller(min_of_the_max, tmp);
  }
  return min_of_the_max;
}


uint ha_partition::max_supported_key_parts() const
{
  return min_of_the_max_uint(&handler::max_supported_key_parts);
}


uint ha_partition::max_supported_key_length() const
{
  return min_of_the_max_uint(&handler::max_supported_key_length);
}


uint ha_partition::max_supported_key_part_length() const
{
  return min_of_the_max_uint(&handler::max_supported_key_part_length);
}


uint ha_partition::max_supported_record_length() const
{
  return min_of_the_max_uint(&handler::max_supported_record_length);
}


uint ha_partition::max_supported_keys() const
{
  return min_of_the_max_uint(&handler::max_supported_keys);
}


uint ha_partition::extra_rec_buf_length() const
{
  handler **file;
  uint max= (*m_file)->extra_rec_buf_length();
  for (file= m_file, file++; *file; file++)
    if (max < (*file)->extra_rec_buf_length())
      max= (*file)->extra_rec_buf_length();
  return max;
}


uint ha_partition::min_record_length(uint options) const
{
  handler **file;
  uint max= (*m_file)->min_record_length(options);
  for (file= m_file, file++; *file; file++)
    if (max < (*file)->min_record_length(options))
      max= (*file)->min_record_length(options);
  return max;
}


/****************************************************************************
                MODULE compare records
****************************************************************************/
/*
  We get two references and need to check if those records are the same.
  If they belong to different partitions we decide that they are not
  the same record. Otherwise we use the particular handler to decide if
  they are the same. Sort in partition id order if not equal.
*/

int ha_partition::cmp_ref(const byte *ref1, const byte *ref2)
{
  uint part_id;
  my_ptrdiff_t diff1, diff2;
  handler *file;
  DBUG_ENTER("ha_partition::cmp_ref");
  if ((ref1[0] == ref2[0]) && (ref1[1] == ref2[1]))
  {
    part_id= get_part_id_from_pos(ref1);
    file= m_file[part_id];
    DBUG_ASSERT(part_id < m_tot_parts);
    DBUG_RETURN(file->cmp_ref((ref1 + PARTITION_BYTES_IN_POS),
			      (ref2 + PARTITION_BYTES_IN_POS)));
  }
  diff1= ref2[1] - ref1[1];
  diff2= ref2[0] - ref1[0];
  if (diff1 > 0)
  {
    DBUG_RETURN(-1);
  }
  if (diff1 < 0)
  {
    DBUG_RETURN(+1);
  }
  if (diff2 > 0)
  {
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(+1);
}


/****************************************************************************
                MODULE auto increment
****************************************************************************/

void ha_partition::restore_auto_increment()
{
  DBUG_ENTER("ha_partition::restore_auto_increment");
  DBUG_VOID_RETURN;
}


/*
  This method is called by update_auto_increment which in turn is called
  by the individual handlers as part of write_row. We will always let
  the first handler keep track of the auto increment value for all
  partitions.
*/

ulonglong ha_partition::get_auto_increment()
{
  DBUG_ENTER("ha_partition::get_auto_increment");
  DBUG_RETURN(m_file[0]->get_auto_increment());
}


/****************************************************************************
                MODULE initialise handler for HANDLER call
****************************************************************************/

void ha_partition::init_table_handle_for_HANDLER()
{
  return;
}


/****************************************************************************
                MODULE Partition Share
****************************************************************************/
/*
  Service routines for ... methods.
-------------------------------------------------------------------------
  Variables for partition share methods. A hash used to track open tables.
  A mutex for the hash table and an init variable to check if hash table
  is initialised.
  There is also a constant ending of the partition handler file name.
*/

#ifdef NOT_USED
static HASH partition_open_tables;
static pthread_mutex_t partition_mutex;
static int partition_init= 0;


/*
  Function we use in the creation of our hash to get key.
*/
static byte *partition_get_key(PARTITION_SHARE *share, uint *length,
			       my_bool not_used __attribute__ ((unused)))
{
  *length= share->table_name_length;
  return (byte *) share->table_name;
}

/*
  Example of simple lock controls. The "share" it creates is structure we
  will pass to each partition handler. Do you have to have one of these?
  Well, you have pieces that are used for locking, and they are needed to
  function.
*/


static PARTITION_SHARE *get_share(const char *table_name, TABLE *table)
{
  PARTITION_SHARE *share;
  uint length;
  char *tmp_name;

  /*
    So why does this exist? There is no way currently to init a storage
    engine.
    Innodb and BDB both have modifications to the server to allow them to
    do this. Since you will not want to do this, this is probably the next
    best method.
  */
  if (!partition_init)
  {
    /* Hijack a mutex for init'ing the storage engine */
    pthread_mutex_lock(&LOCK_mysql_create_db);
    if (!partition_init)
    {
      partition_init++;
      VOID(pthread_mutex_init(&partition_mutex, MY_MUTEX_INIT_FAST));
      (void) hash_init(&partition_open_tables, system_charset_info, 32, 0, 0,
		       (hash_get_key) partition_get_key, 0, 0);
    }
    pthread_mutex_unlock(&LOCK_mysql_create_db);
  }
  pthread_mutex_lock(&partition_mutex);
  length= (uint) strlen(table_name);

  if (!(share= (PARTITION_SHARE *) hash_search(&partition_open_tables,
					       (byte *) table_name, length)))
  {
    if (!(share= (PARTITION_SHARE *)
	  my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
			  &share, sizeof(*share),
			  &tmp_name, length + 1, NullS)))
    {
      pthread_mutex_unlock(&partition_mutex);
      return NULL;
    }

    share->use_count= 0;
    share->table_name_length= length;
    share->table_name= tmp_name;
    strmov(share->table_name, table_name);
    if (my_hash_insert(&partition_open_tables, (byte *) share))
      goto error;
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
  }
  share->use_count++;
  pthread_mutex_unlock(&partition_mutex);

  return share;

error:
  pthread_mutex_unlock(&partition_mutex);
  my_free((gptr) share, MYF(0));

  return NULL;
}


/*
  Free lock controls. We call this whenever we close a table. If the table
  had the last reference to the share then we free memory associated with
  it.
*/

static int free_share(PARTITION_SHARE *share)
{
  pthread_mutex_lock(&partition_mutex);
  if (!--share->use_count)
  {
    hash_delete(&partition_open_tables, (byte *) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&partition_mutex);

  return 0;
}
#endif /* NOT_USED */
#endif						/* HAVE_PARTITION_DB */
