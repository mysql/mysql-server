/* Copyright (c) 2020 Justin Swanhart

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

#include "storage/warp/ha_warp.h"

//static WARP_SHARE *get_share(const char *table_name, TABLE *table);
//static int free_share(WARP_SHARE *share);

/* Stuff for shares */
mysql_mutex_t warp_mutex;
static std::unique_ptr<collation_unordered_multimap<std::string, WARP_SHARE *>>
    warp_open_tables;


static handler *warp_create_handler(handlerton *hton, TABLE_SHARE *table,
                                    bool partitioned, MEM_ROOT *mem_root);

static handler *warp_create_handler(handlerton *hton, TABLE_SHARE *table, bool,
                                    MEM_ROOT *mem_root) {
  return new (mem_root) ha_warp(hton, table);
}

/*****************************************************************************
 ** WARP tables
 *****************************************************************************/

#ifdef HAVE_PSI_INTERFACE
static PSI_memory_key warp_key_memory_warp_share;
static PSI_memory_key warp_key_memory_row;
static PSI_memory_key warp_key_memory_blobroot;

static PSI_mutex_key warp_key_mutex_warp, warp_key_mutex_WARP_SHARE_mutex;

static PSI_mutex_info all_warp_mutexes[] = {
    {&warp_key_mutex_warp, "warp", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&warp_key_mutex_WARP_SHARE_mutex, "WARP_SHARE::mutex", 0, 0,
     PSI_DOCUMENT_ME}};

static PSI_memory_info all_warp_memory[] = {
    {&warp_key_memory_warp_share, "WARP_SHARE", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&warp_key_memory_blobroot, "blobroot", 0, 0, PSI_DOCUMENT_ME},
    {&warp_key_memory_row, "row", 0, 0, PSI_DOCUMENT_ME}};

/*
static PSI_file_key warp_key_file_metadata, warp_key_file_data,
    warp_key_file_update;
*/

static void init_warp_psi_keys(void) {
  const char *category = "warp";
  int count;

  count = static_cast<int>(array_elements(all_warp_mutexes));
  mysql_mutex_register(category, all_warp_mutexes, count);

  count = static_cast<int>(array_elements(all_warp_memory));
  mysql_memory_register(category, all_warp_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */

/*
  If frm_error() is called in table.cc this is called to find out what file
  extensions exist for this handler.  Our table name, however, is not a set
  of files in the schema directory, but instead is a directory itself which
  contains files which respresent each column and also the table metadata.
*/
static const char *ha_warp_exts[] = {NullS};

static int warp_init_func(void *p) {
  DBUG_ENTER("warp_init_func");
  handlerton *warp_hton;
  ibis::fileManager::adjustCacheSize(1U<<31);
  ibis::init(NULL, "/tmp/fastbit.log");
  ibis::util::setVerboseLevel(5);

#ifdef HAVE_PSI_INTERFACE
  init_warp_psi_keys();
#endif

  warp_hton = (handlerton *)p;
  mysql_mutex_init(warp_key_mutex_warp, &warp_mutex, MY_MUTEX_INIT_FAST);
  warp_open_tables.reset(new collation_unordered_multimap<std::string, WARP_SHARE *>(
      system_charset_info, warp_key_memory_warp_share));
  warp_hton->state = SHOW_OPTION_YES;
  warp_hton->db_type = DB_TYPE_UNKNOWN;
  warp_hton->create = warp_create_handler;
  warp_hton->flags =
      (HTON_CAN_RECREATE | HTON_NO_PARTITION);
  warp_hton->file_extensions = ha_warp_exts;
  warp_hton->rm_tmp_tables = default_rm_tmp_tables;

  DBUG_RETURN(0);
}

static int warp_done_func(void *) {
  warp_open_tables.reset();
  mysql_mutex_destroy(&warp_mutex);

  return 0;
}

/* Construct the warp handler */
ha_warp::ha_warp(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg),
      //reader(NULL),
      base_table(NULL),
      filtered_table(NULL),
      cursor(NULL),
      writer(NULL),
      current_rowid(0),
      blobroot(warp_key_memory_blobroot, BLOB_MEMROOT_ALLOC_SIZE)
      { 
        
      }

void ha_warp::get_auto_increment	(	
  ulonglong 	,
  ulonglong 	,
  ulonglong 	,
  ulonglong * 	first_value,
  ulonglong * 	nb_reserved_values 
)	{
    
  *first_value = stats.auto_increment_value ? stats.auto_increment_value : 1;
  *nb_reserved_values = ULLONG_MAX;
    
}

int ha_warp::encode_quote(uchar *) {
  char attribute_buffer[1024];
  String attribute(attribute_buffer, sizeof(attribute_buffer), &my_charset_bin);
  buffer.length(0);

  for (Field **field = table->field; *field; field++) {
    const char *ptr;
    const char *end_ptr;
    
    /* See the comment below about the NULL bitmap.  Note that
       for both strings and numeric types, the value of a NULL
       column in the database is 0. This value isn't ever used
       as it is just a placeholder. The associated NULL bitmap
       is marked as 1 to indicate the actual value is garbage.
    */
    if((*field)->is_null()) {
      buffer.append("0,1,");
      continue;
    } 

    /* Convert the value to string */
    bool no_quote = false;
    switch((*field)->real_type()) {
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:    
      case MYSQL_TYPE_YEAR:
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_NEWDATE: 
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_DATETIME2: 
      case MYSQL_TYPE_TIME2:  
        attribute.length(0);
        attribute.append(std::to_string((*field)->val_int()).c_str());
        no_quote = true;
      break;
      
      default:
        (*field)->val_str(&attribute, &attribute);
      break; 
    }
    
    if ((*field)->str_needs_quotes() && !no_quote) {
      ptr = attribute.ptr();
      end_ptr = attribute.length() + ptr;

      buffer.append('"');

      for (; ptr < end_ptr; ptr++) {
        if (*ptr == '"') {
          buffer.append('\\');
          buffer.append('"');
        } else if (*ptr == '\r') {
          buffer.append('\\');
          buffer.append('r');
        } else if (*ptr == '\\') {
          buffer.append('\\');
          buffer.append('\\');
        } else if (*ptr == '\n') {
          buffer.append('\\');
          buffer.append('n');
        } else
          buffer.append(*ptr);
      }
      buffer.append('"');
    } else {
      buffer.append(attribute);
    }
    
    /* A NULL bitmap (for example the column n0 for column c0) is 
       marked as zero when the value is not NULL. The NULL bitmap 
       column is always included in a fetch for the corresponding
       cX column. NOT NULL columns do not have an associated NULL
       bitmap.  Note the trailing comma (also above).
    */
    if((*field)->real_maybe_null()) {
      buffer.append(",0,");
    } else {
      buffer.append(',');
    }

  }
  
  // Remove that pesky trailing comma
  buffer.length(buffer.length() - 1);

  return (buffer.length());
}

/*
  Simple lock controls.
*/
static WARP_SHARE *get_share(const char *table_name, TABLE *) {
  DBUG_ENTER("ha_warp::get_share");
  WARP_SHARE *share;
  char *tmp_name;
  uint length;
  length = (uint)strlen(table_name);

  mysql_mutex_lock(&warp_mutex);

  /*
    If share is not present in the hash, create a new share and
    initialize its members.
  */
  const auto it = warp_open_tables->find(table_name);
  if (it == warp_open_tables->end()) {
    if (!my_multi_malloc(warp_key_memory_warp_share, MYF(MY_WME | MY_ZEROFILL),
                         &share, sizeof(*share), &tmp_name, length + 1,
                         NullS)) {
      mysql_mutex_unlock(&warp_mutex);
      return NULL;
    }

    share->use_count = 0;
    share->table_name.assign(table_name, length);
    fn_format(share->data_dir_name, table_name, "", ".data",
              MY_REPLACE_EXT | MY_UNPACK_FILENAME);
    
    warp_open_tables->emplace(table_name, share);
    thr_lock_init(&share->lock);
    mysql_mutex_init(warp_key_mutex_WARP_SHARE_mutex, &share->mutex,
                     MY_MUTEX_INIT_FAST);

  } else {
    share = it->second;
  }

  share->use_count++;
  mysql_mutex_unlock(&warp_mutex);

  DBUG_RETURN(share);
}

bool ha_warp::check_and_repair(THD *thd) {
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_warp::check_and_repair");

  check_opt.init();

  DBUG_RETURN(repair(thd, &check_opt));
}

bool ha_warp::is_crashed() const {
  DBUG_ENTER("ha_warp::is_crashed");
  DBUG_RETURN(0);
}

/*
  Free lock controls.
*/
static int free_share(WARP_SHARE *share) {
  DBUG_ENTER("ha_warp::free_share");
  mysql_mutex_lock(&warp_mutex);
  int result_code = 0;
  if (!--share->use_count) {
    warp_open_tables->erase(share->table_name.c_str());
    thr_lock_delete(&share->lock);
    mysql_mutex_destroy(&share->mutex);
    my_free(share);
  }
  mysql_mutex_unlock(&warp_mutex);

  DBUG_RETURN(result_code);
}



/*
  Populates the comma separarted list of all the columns that need to be read from 
  the storage engine for this query.
*/
int ha_warp::set_column_set() {
  bool read_all;
  DBUG_ENTER("ha_warp::set_column_set");
  column_set = "";
 
  /* We must read all columns in case a table is opened for update */
  read_all = !bitmap_is_clear_all(table->write_set);
  int count = 0;
  for (Field **field = table->field; *field; field++) {
    if (read_all || bitmap_is_set(table->read_set, (*field)->field_index)) {
      ++count;
      
      /* this column must be read from disk */
      column_set += std::string("c") + std::to_string((*field)->field_index);
      
      /* Add the NULL bitmap for the column if the column is NULLable */
      if((*field)->real_maybe_null()) {
        column_set += "," + std::string("n") + std::to_string((*field)->field_index);
      }
      column_set += ",";
    }    
  }
  
  if(count==0) {
    column_set = "c0";
    DBUG_PRINT("ha_warp::set_column_set", ("NOTE: forcing read of first column for scan..."));
  } else {
    //remove the trailing comma
    column_set = column_set.substr(0,column_set.length()-1);
  }
  
  DBUG_PRINT("ha_warp::set_column_set", ("column_list=%s", column_set.c_str()));

  DBUG_RETURN(count+1);
}

/*
  Populates the comma separarted list of all the columns that need to be read from 
  the storage engine for this query for a given index.
*/
int ha_warp::set_column_set(uint32_t idxno) {
  DBUG_ENTER("ha_warp::set_column_set");
  index_column_set = "";
  uint32_t count=0;

  for(uint32_t i=0; i < table->key_info[idxno].actual_key_parts;++i) {
    uint16_t fieldno = table->key_info[idxno].key_part[i].field->field_index;
    bool may_be_null = table->key_info[idxno].key_part[i].field->real_maybe_null();
    ++count;
    
    /* this column must be read from disk */
    index_column_set += std::string("c") + std::to_string(fieldno);
    
    /* Add the NULL bitmap for the column if the column is NULLable */
    if(may_be_null) {
      index_column_set += "," + std::string("n") + std::to_string(fieldno);
    }
    index_column_set += ",";    
  }
  
  index_column_set = index_column_set.substr(0,index_column_set.length()-1);
  
  DBUG_PRINT("ha_warp::set_column_set", ("column_list=%s", index_column_set.c_str()));

  DBUG_RETURN(count+1);
}

/* store the binary data for each returned value into the MySQL buffer
   using field->store()
*/

int ha_warp::find_current_row(uchar *buf) {
  DBUG_ENTER("ha_warp::find_current_row");
  int rc = 0; 
  memset(buf,0,table->s->null_bytes);

  // Clear BLOB data from the previous row.
  blobroot.ClearForReuse();

  /* Avoid asserts in ::store() for columns that are not going to be updated */
  my_bitmap_map* org_bitmap(dbug_tmp_use_all_columns(table, table->write_set));

  /* Read all columns when a table is opened for update */
  bool read_all = !bitmap_is_clear_all(table->write_set);

  /* First step: gather stats about the table and the resultset.
     These are needed to size the NULL bitmap for the row and to
     properly format that bitmap.  DYNAMIC rows resultsets (those 
     that contain variable length fields) reserve an extra bit in
     the NULL bitmap...
  */
  for (Field **field = table->field; *field; field++) {
    buffer.length(0);
    DBUG_PRINT("ha_warp::find_current_row", ("Getting value for field: %s",(*field)->field_name));
    if (read_all || bitmap_is_set(table->read_set, (*field)->field_index)) {
      bool is_unsigned = (*field)->flags & UNSIGNED_FLAG;
      std::string cname = "c" + std::to_string((*field)->field_index);
      std::string nname = "n" + std::to_string((*field)->field_index);
      
      if((*field)->real_maybe_null()) {
        unsigned char is_null=0;
        
        rc = cursor->getColumnAsUByte(nname.c_str(),is_null);
        
        /* This column value is NULL */
        if(is_null != 0) {
          (*field)->set_null();
          rc = 0;
          continue;
        } 
      }  
      
      switch((*field)->real_type()) {
        case MYSQL_TYPE_TINY: {
          if(is_unsigned) { 
            unsigned char tmp=0;
            rc = cursor->getColumnAsUByte(cname.c_str(),tmp);
            rc = (*field)->store(tmp, true);
          } else {
            char tmp=0;
            rc = cursor->getColumnAsByte(cname.c_str(),tmp);
            rc = (*field)->store(tmp, false);
          }
          break;
        }
        case MYSQL_TYPE_SHORT: {
          if(is_unsigned) { 
            uint16_t tmp=0;
            rc = cursor->getColumnAsUShort(cname.c_str(),tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int16_t tmp=0;
            rc = cursor->getColumnAsShort(cname.c_str(),tmp);
            rc = (*field)->store(tmp, false);
          }
        }
        break;

        case MYSQL_TYPE_LONG: {
          
          if(is_unsigned) { 
            uint32_t tmp = 0;
            rc = cursor->getColumnAsUInt(cname.c_str(),tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int32_t tmp = 0;
            rc = cursor->getColumnAsInt(cname.c_str(),tmp);
            rc = (*field)->store(tmp, false);
          }
        }
        break;

        case MYSQL_TYPE_LONGLONG: {
          uint64_t tmp=0;
          if(is_unsigned) { 
            rc = cursor->getColumnAsULong(cname.c_str(),tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int64_t tmp=0;
            rc = cursor->getColumnAsLong(cname.c_str(),tmp);
            rc = (*field)->store(tmp, false);
          }
        }
        break;

        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_JSON: {
          std::string tmp;
          rc = cursor->getColumnAsString(cname.c_str(), tmp);
          if ((*field)->store(tmp.c_str(), tmp.length(), (*field)->charset(),CHECK_FIELD_WARN)) {
            rc = HA_ERR_CRASHED_ON_USAGE;
            goto err;
          }
          if ((*field)->flags & BLOB_FLAG) {
            Field_blob *blob_field = down_cast<Field_blob *>(*field);
            //size_t length = blob_field->get_length(blob_field->ptr);
            size_t length = blob_field->get_length();
            // BLOB data is not stored inside buffer. It only contains a
            // pointer to it. Copy the BLOB data into a separate memory
            // area so that it is not overwritten by subsequent calls to
            // Field::store() after moving the offset.
            if (length > 0) {
              const unsigned char *old_blob;
              //blob_field->get_ptr(&old_blob);
              old_blob = blob_field->get_ptr();
              unsigned char *new_blob = new (&blobroot) unsigned char[length];
              if (new_blob == nullptr) DBUG_RETURN(HA_ERR_OUT_OF_MEM);
              memcpy(new_blob, old_blob, length);
              blob_field->set_ptr(length, new_blob);
              
            }
          }
        }
        
        break;
        
        case MYSQL_TYPE_FLOAT: {
          float_t tmp;
          rc = cursor->getColumnAsFloat(cname.c_str(),tmp);
          rc=(*field)->store(tmp);
        }
        break;
        
        case MYSQL_TYPE_DOUBLE: {
          double_t tmp;
          rc = cursor->getColumnAsDouble(cname.c_str(),tmp);
          rc=(*field)->store(tmp);
        }
        break;
        
        case MYSQL_TYPE_INT24: {
          uint32_t tmp;
          if(is_unsigned) { 
            rc = cursor->getColumnAsUInt(cname.c_str(),tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int32_t tmp;
            rc = cursor->getColumnAsInt(cname.c_str(),tmp);
            rc = (*field)->store(tmp, false);
          }
        }
        break;

        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL: {     
          uint64_t tmp;
          rc = cursor->getColumnAsULong(cname.c_str(),tmp);
          rc= (*field)->store(tmp, is_unsigned);
        }
        break;
        
        case MYSQL_TYPE_YEAR:{
          int16_t tmp;
          rc = cursor->getColumnAsShort(cname.c_str(),tmp);
          rc= (*field)->store(tmp, false);
        }
        break;

        case MYSQL_TYPE_NEWDATE: 
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_TIME2: 
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_TIMESTAMP2:
        case MYSQL_TYPE_DATETIME2: {
          int64_t tmp;
          rc = cursor->getColumnAsLong(cname.c_str(),tmp);
          rc= (*field)->store(tmp, false);
          break;
        }

        case MYSQL_TYPE_NULL:
        case MYSQL_TYPE_BIT:
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_GEOMETRY: {
          std::string tmp;
          rc = cursor->getColumnAsString(cname.c_str(), tmp);
          if ((*field)->store(tmp.c_str(), tmp.length(), (*field)->charset(),CHECK_FIELD_WARN)) {
            rc = HA_ERR_CRASHED_ON_USAGE;
            goto err;
          }
        }
        break; 
        
        default: {
          std::string errmsg = "Unsupported data type for column: " + 
            std::string((*field)->field_name); 
          my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), errmsg.c_str());
          rc = HA_ERR_UNSUPPORTED;
          goto err;
          break;
        }
      
      }

      if(rc != 0) {
        goto err;
      }
    }   
  }
  
err:
  dbug_tmp_restore_column_map(table->write_set, org_bitmap);

  DBUG_RETURN(rc);
}

int ha_warp::reset_table() {
  DBUG_ENTER("ha_warp::reset_table");
  DBUG_RETURN(0);
}

void ha_warp::update_row_count() {
  DBUG_ENTER("ha_warp::row_count");
  if(base_table) {
    
    delete base_table;
  }
  
  base_table = new ibis::mensa(share->data_dir_name);
  stats.records = base_table->nRows();
  
  delete base_table;
  base_table = NULL;
  DBUG_VOID_RETURN;
}

/*
  Open a database file. Keep in mind that tables are caches, so
  this will not be called for every request. Any sort of positions
  that need to be reset should be kept in the ::extra() call.
*/
int ha_warp::open(const char *name, int, uint, const dd::Table *) {
  DBUG_ENTER("ha_warp::open");
  if (!(share = get_share(name, table))) DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  
  update_row_count();
  
  /*
    Init locking. Pass handler object to the locking routines,
    so that they could save/update local_saved_data_file_length value
    during locking. This is needed to enable concurrent inserts.

    FIXME: support concurrent insert
    -----------------------------------------------------------------
  */
  thr_lock_data_init(&share->lock, &lock, (void *)this);
  ref_length = sizeof(my_off_t);

/*
  share->lock.get_status = warp_get_status;
  share->lock.update_status = warp_update_status;
  share->lock.check_status = warp_check_status;
*/

  /* reserve space for the buffer for INSERT statements */
  buffer.alloc(65535);

  DBUG_RETURN(0);
}

/*
  Close a database file. We remove ourselves from the shared strucutre.
  If it is empty we destroy it.
*/
int ha_warp::close(void) {
  DBUG_ENTER("ha_warp::close");
  
  if(writer) delete writer;
  writer = NULL;

  if(cursor) delete cursor;
  cursor = NULL;

  if(filtered_table) delete filtered_table;
  filtered_table = NULL;

  if(base_table) delete base_table;
  base_table = NULL;

  DBUG_RETURN(free_share(share));
}

/*
  This is an INSERT.  The row data is converted to CSV (just like the CSV engine)
  and this is passed to the storage layer for processing.  It would be more 
  efficient to construct a vector of rows to insert (to also support bulk insert). 
*/
int ha_warp::write_row(uchar *buf) {
  DBUG_ENTER("ha_warp::write_row");
    create_writer(table);

  if (table->next_number_field && buf == table->record[0]) {
    int error;
    if ((error = update_auto_increment())) DBUG_RETURN(error);
  }
  ha_statistic_increment(&System_status_var::ha_write_count);
  ha_warp::encode_quote(buf);
  
  DBUG_PRINT("ha_warp::write_row", ("row_data=%s", buffer.c_ptr()));
    writer->appendRow(buffer.c_ptr(), ",");

  mysql_mutex_lock(&share->mutex);
  mysql_mutex_unlock(&share->mutex);
  stats.records++;
  DBUG_RETURN(0);
}


/*
  This is called for an update.
  Make sure you put in code to increment the auto increment.
  Currently auto increment is not being
  fixed since autoincrements have yet to be added to this table handler.
  This will be called in a table scan right before the previous ::rnd_next()
  call.
*/
int ha_warp::update_row(const uchar *, uchar *new_data) {
  DBUG_ENTER("ha_warp::update_row");
  my_error(ER_INTERNAL_ERROR, MYF(0), "Update is not supported");
  *new_data+=0; /* to get rid of warning*/
  DBUG_RETURN(-1);

  ha_statistic_increment(&System_status_var::ha_update_count);
}

/*
  Deletes a row. First the database will find the row, and then call this
  method. In the case of a table scan, the previous call to this will be
  the ::rnd_next() that found this row.
  The exception to this is an ORDER BY. This will cause the table handler
  to walk the table noting the positions of all rows that match a query.
  The table will then be deleted/positioned based on the ORDER (so RANDOM,
  DESC, ASC).
*/
int ha_warp::delete_row(const uchar *) {
  DBUG_ENTER("ha_warp::delete_row");
  
  deleted_rows.push_back(current_rowid);

  ha_statistic_increment(&System_status_var::ha_delete_count);
  stats.records--;
  DBUG_RETURN(0);
}

int ha_warp::delete_table(const char *table_name, const dd::Table *) {
  DBUG_ENTER("ha_warp::delete_table");
  //my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "DROP is not supported)");
  //DBUG_RETURN(HA_ERR_UNSUPPORTED);
  
  //FIXME: this needs to be safer
  //system((std::string("rm -rf ") + std::string(share->data_dir_name)).c_str());
  std::string cmdline = std::string("rm -rf ") + std::string(table_name) + ".data/";
  int rc = system(cmdline.c_str());
  ha_statistic_increment(&System_status_var::ha_delete_count);
  DBUG_RETURN(rc != 0);
}

/*
  ::info() is used to return information to the optimizer.
  Currently this table handler doesn't implement most of the fields
  really needed. SHOW also makes use of this data
*/
int ha_warp::info(uint) {
  DBUG_ENTER("ha_warp::info");
  /* This is a lie, but you don't want the optimizer to see zero or 1 */
  if (!records_is_known && stats.records < 2) stats.records = 2;

  stats.mean_rec_length = 0;
  for (Field **field = table->s->field; *field; field++) {
    switch((*field)->real_type()) {
      case MYSQL_TYPE_TINY:
        stats.mean_rec_length += 1;
        break;

      case MYSQL_TYPE_SHORT:
        stats.mean_rec_length += 2;
        break;

      case MYSQL_TYPE_INT24:
        stats.mean_rec_length += 3;
        break;

      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_FLOAT:
        stats.mean_rec_length += 4;
        break;

      case MYSQL_TYPE_LONGLONG:
        stats.mean_rec_length += 8;
        break;

      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_DOUBLE:
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:         
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_YEAR:
      case MYSQL_TYPE_NEWDATE: 
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_NULL:
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_DATETIME2: 
      case MYSQL_TYPE_TIME2:   
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_GEOMETRY:
      default:
        /* this is a total lie but this is just an estimate */
        stats.mean_rec_length += 8;
        break; 
	 
    }    

    stats.auto_increment_value = stats.records;
  }

  /* estimate the data size from the record count and average record size */
  stats.data_file_length = stats.mean_rec_length * stats.records;

  DBUG_RETURN(0);
}

/* The ::extra function is called a bunch of times before and after various
   storage engine operations. I think it is used as a hint for faster alter
   for example. Right now, in warp if there are any dirty rows buffered in 
   the writer object, flush them to disk when ::extra is called.   Seems to
   work.
*/
int ha_warp::extra(enum ha_extra_function) {
  if(deleted_rows.size() > 0) {
    if(base_table) {
      delete base_table;
    }
    base_table=new ibis::mensa(share->data_dir_name);
    ibis::partList parts;
    ibis::util::gatherParts(parts, share->data_dir_name);
    
    std::vector<uint32_t> rowids;
     
    /* skip through the partitions until we find the one containing the row */
    uint64_t deleted_count = 0;
    uint64_t cumulative_rowcount = 0;
    /* i is used to iterate over the rows to delete. note the comment below 
       about why i is initialized here and not inside the for loop where it
       would usually be done.
    */
    uint64_t i=0;
    for(auto itp = parts.begin();itp < parts.end();++itp) {
      uint32_t part_rowcount = (*itp)->nRows();
      cumulative_rowcount += part_rowcount;
      /* This loop doesn't start from the beginning when it breaks out
         and iterates the for loop.  Each row must be deleted just one
         time, and it must be deleted from the proper partition.  Each
         partition removes rows relative to the start of the partition      
         but the cursor returns logical values greater than a the size
         of a partition.  Thus the "current_rowid" values here must be 
         adjusted to reconcile the difference between the two offsets.
      */
      while(i<deleted_rows.size()) {
        if(deleted_rows[i] > cumulative_rowcount) { 
          /* move on to the next partition*/
          break;
        }

        uint32_t rowid = deleted_rows[i]+1;
        rowids.push_back(rowid);
        ++i;
      }
      
      if(rowids.size() >0) {
        (*itp)->deactivate(rowids);
        deleted_count += rowids.size();
        rowids.clear();  
      }
    }
    
    for(uint16_t idx = 0; idx < table->s->keys ; ++idx) {
       std::string cname = "c" + std::to_string(table->key_info[idx].key_part[0].field->field_index);

       // a field can have an index= comment that indicates an index spec.  
       //See here: https://sdm.lbl.gov/fastbit/doc/indexSpec.html
        
       std::string comment(table->key_info[idx].key_part[0].field->comment.str,
         table->key_info[idx].key_part[0].field->comment.length);
       if(comment != "" && comment.substr(0,6) != "index=") {
         comment = "";
       }
       base_table->buildIndex(cname.c_str(),comment.c_str());
    }

    delete base_table;
    base_table = NULL;
    deleted_rows.clear();
    stats.records -= deleted_count;
  }
  
  if(writer != NULL && writer->mRows() > 0) {
     int rowcount = writer->write(share->data_dir_name);
     writer->clearData();
     if(base_table == NULL) {
       base_table=new ibis::mensa(share->data_dir_name);
     }
           
     /* rebuild any indexed columns */
     
     for(uint16_t idx = 0; idx < table->s->keys ; ++idx) {
       std::string cname = "c" + std::to_string(table->key_info[idx].key_part[0].field->field_index);

       // a field can have an index= comment that indicates an index spec.  
       //See here: https://sdm.lbl.gov/fastbit/doc/indexSpec.html
        
       std::string comment(table->key_info[idx].key_part[0].field->comment.str,
         table->key_info[idx].key_part[0].field->comment.length);
       if(comment != "" && comment.substr(0,6) != "index=") {
         comment = "";
       }
       base_table->buildIndex(cname.c_str(),comment.c_str());
     }
     
     //base_table->buildIndexes("");
     delete base_table;
     base_table = NULL;
     stats.records += rowcount; 
  }
  //}
 //DBUG_RETURN(0); 
 return 0;
}

int ha_warp::repair(THD *, HA_CHECK_OPT *) {
  DBUG_ENTER("ha_warp::repair");
  my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "REPAIR is not supported");
  DBUG_RETURN(HA_ERR_UNSUPPORTED);

  //DBUG_RETURN(HA_ADMIN_OK);
}

/*
  DELETE without WHERE calls this
*/
/*int ha_warp::delete_all_rows() {
  DBUG_ENTER("ha_warp::delete_all_rows");
  my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "TRUNCATE is not supported");
  DBUG_RETURN(HA_ERR_UNSUPPORTED);
}
*/

/*
  Called by the database to lock the table. Keep in mind that this
  is an internal lock.
*/
THR_LOCK_DATA **ha_warp::store_lock(THD *, THR_LOCK_DATA **to,
                                    enum thr_lock_type lock_type) {
  DBUG_ENTER("ha_warp::store_lock");
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) lock.type = lock_type;
  *to++ = &lock;
  DBUG_RETURN(to);
}

int file_exists(const char * file_name)
{
	struct stat buf;
	return(stat(file_name, &buf) == 0);
}

int file_exists(std::string file_name)
{
  return file_exists(file_name.c_str());
}

void ha_warp::create_writer(TABLE* table_arg) {
  if(writer != NULL) {
     //delete writer;
     return;
  }
  /* 
  Add the columns of the table to the writer object.
  
  MySQL types map to IBIS types:
  -----------------------------------------------------
  UNKNOWN_TYPE, OID, UDT, CATEGORY, BIT, BLOB
  BYTE, UBYTE , SHORT , USHORT, INT, UINT, LONG, ULONG 	
  FLOAT, DOUBLE 	
  TEXT 
  */
  int column_count= 0;
  
  /* Create an empty writer.  Columns are added to it */
  writer = ibis::tablex::create();
  
  for(Field **field = table_arg->s->field; *field; field++) {
    std::string name  = "c" + std::to_string(column_count);
    std::string nname = "n" + std::to_string(column_count);
    ++column_count;

    ibis::TYPE_T datatype = ibis::UNKNOWN_TYPE;
    bool is_unsigned = (*field)->flags & UNSIGNED_FLAG;
    bool is_nullable = (*field)->real_maybe_null();

    /* create a tablex object to create the metadata for the table */
    switch((*field)->real_type()) {
      case MYSQL_TYPE_TINY:
        if(is_unsigned) {
          datatype = ibis::UBYTE; 
        } else {
          datatype = ibis::BYTE;
        }
        break;

      case MYSQL_TYPE_SHORT:
        if(is_unsigned) {
          datatype = ibis::USHORT; 
        } else {
          datatype = ibis::SHORT;
        }
        break;

      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_LONG:
        if(is_unsigned) {
          datatype = ibis::UINT; 
        } else {
          datatype = ibis::INT;
        }
        break;

      case MYSQL_TYPE_LONGLONG:
        if(is_unsigned) {
          datatype = ibis::ULONG; 
        } else {
          datatype = ibis::LONG;
        }
        break;

      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_JSON:
        datatype = ibis::TEXT;
        break;

      case MYSQL_TYPE_FLOAT:
        datatype = ibis::FLOAT;
        break;

      case MYSQL_TYPE_DOUBLE:
        datatype = ibis::DOUBLE;
        break;

      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:    
        datatype = ibis::ULONG;
        break;

      case MYSQL_TYPE_YEAR:
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
        datatype = ibis::SHORT;
        break;

      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_NEWDATE: 
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_DATETIME2:
      case MYSQL_TYPE_TIME2:   
        datatype = ibis::LONG;
        break;

      case MYSQL_TYPE_ENUM:
        datatype = ibis::CATEGORY;
        break;

      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_NULL:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_GEOMETRY:
        datatype = ibis::TEXT;
      break;

      /* UNSUPPORTED TYPES */
      default:
        std::string errmsg = "Unsupported data type for column: " + std::string((*field)->field_name); 
        my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), errmsg.c_str());
        datatype = ibis::UNKNOWN_TYPE;
        break;
    }
    /* Fastbit supports numerous bitmap index options.  You can place these options in the comment
       string of a column.  When an index is created on a column, then the indexing options used
       are taken from the comment.  In the future, the comment will support more options for 
       compression, etc.
    */
    std::string comment((*field)->comment.str, (*field)->comment.length);
    std::string index_spec = "";
    if(comment.substr(0,6) != "index=") {
      comment = "";
      index_spec = comment;
    }
    writer->addColumn(name.c_str(),datatype,comment.c_str(),index_spec.c_str());
    
    /* Columns which are NULLable have a NULL marker.  A better approach might to have one NULL bitmap
       stored as a separate column instead of one byte per NULLable column, but that makes query 
       processing a bit more complex so this simpler approach is taken for now.  Also, once compression
       is implemented, these columns will shrink quite a bit.
    */
    if(is_nullable) {
      //writer->addColumn(nname.c_str(),ibis::BIT,"NULL bitmap the correspondingly numbered column");
      writer->addColumn(nname.c_str(),ibis::UBYTE,"NULL bitmap the correspondingly numbered column");
    }

  }

  /* FIXME: this should be an SE variable */
  writer->reserveBuffer(25000);

  /* FIXME: should be a table option and should be able to be set in size not just count*/
  writer->setPartitionMax(1<<30);

}

/*
  Create a table. You do not want to leave the table open after a call to
  this (the database will call ::open() if it needs to).

  Note that the internal Fastbit columns are named after the field numbers
  in the MySQL table.
*/
int ha_warp::create(const char *name, TABLE *table_arg, HA_CREATE_INFO *,
                    dd::Table *) {
  DBUG_ENTER("ha_warp::create");
  int rc = 0;
  if (!(share = get_share(name, table))) DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  /* create the writer object from the list of columns in the table */
  create_writer(table_arg);

	char errbuf[MYSYS_STRERROR_SIZE];
  DBUG_PRINT("ha_warp::create", ("creating table data directory=%s", share->data_dir_name));
	if(!file_exists(share->data_dir_name)) {
		if(mkdir(share->data_dir_name, S_IRWXU | S_IXOTH) == -1) {
      my_error(ER_INTERNAL_ERROR, MYF(0), my_strerror(errbuf, sizeof(errbuf), errno));
			DBUG_RETURN(-1);
		}
	} else {
      my_error(ER_INTERNAL_ERROR, MYF(0), "on-disk table data already exists");
      DBUG_RETURN(-1);
	}
  
  /* Write the metadata to disk (returns 1 on success but this function returns 0 on success...)
     Be nice and try to clean up if metadata write failed (out of disk space for example)
  */
  DBUG_PRINT("ha_warp::create", ("Writing table metadata"));
  if(!writer->writeMetaData((std::string(share->data_dir_name)).c_str())) {
    if(file_exists(share->data_dir_name)) {
      rmdir(share->data_dir_name);
    }
    rc = -1;
  }

  DBUG_RETURN(rc);
}

int ha_warp::check(THD *, HA_CHECK_OPT *) {
  DBUG_ENTER("ha_warp::check");
  //old_proc_info = thd_proc_info(thd, "Checking table");

//  blobroot.Clear();

  //thd_proc_info(thd, old_proc_info);

  /*if ((rc != HA_ERR_END_OF_FILE) || count) {
    share->crashed = true;
    DBUG_RETURN(HA_ADMIN_CORRUPT);
  }*/

  DBUG_RETURN(HA_ADMIN_OK);
}

bool ha_warp::check_if_incompatible_data(HA_CREATE_INFO *, uint) {
  return COMPATIBLE_DATA_YES;
}

struct st_mysql_storage_engine warp_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

mysql_declare_plugin(warp){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &warp_storage_engine,
    "WARP",
    "Justin Swanhart",
    "WARP columnar storage engine(using FastBit 2.0.3 storage)",
    PLUGIN_LICENSE_GPL,
    warp_init_func, /* Plugin Init */
    NULL,           /* Plugin check uninstall */
    warp_done_func, /* Plugin Deinit */
    0x203 /* Based on Fastbit 2.0.3 */,
    NULL, /* status variables                */
    NULL, /* system variables                */
    NULL, /* config options                  */
    0,    /* flags                           */
} mysql_declare_plugin_end;
/* Copyright (c) 2020 Justin Swanhart

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

#include "storage/warp/ha_warp.h"

int ha_warp::rnd_init(bool) {
  DBUG_ENTER("ha_warp::rnd_init");
  current_rowid = 0;
  
  /* This is the a big part of the MAGIC (outside of bitmap indexes)...
     This figures out which columns this query is reading so we only read
     the necesssary columns from disk.  This is the primary advantage
     of a column store...
  */
  set_column_set();
  
  /* This second table object is a new relation which is a projection of all
     the rows and columns in the table.
  */
  base_table = new ibis::mensa(share->data_dir_name);
  filtered_table = base_table->select(column_set.c_str(), "1=1");
  if(filtered_table != NULL) {
    /* Allocate a cursor for any queries that actually fetch columns */
    cursor = filtered_table->createCursor();
  }
  
  DBUG_RETURN(0);
}

int ha_warp::rnd_next(uchar *buf) {
  DBUG_ENTER("ha_warp::rnd_next");
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);
    
  if(cursor == NULL || cursor->nRows() <= 0) {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  
  if(cursor->fetch() != 0) {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  current_rowid=cursor->getCurrentRowNumber();

  find_current_row(buf);

  DBUG_RETURN(0);

}

/*
  In the case of an order by rows will need to be sorted.
  ::position() is called after each call to ::rnd_next(),
  the data it stores is to a byte array. You can store this
  data via my_store_ptr(). ref_length is a variable defined to the
  class that is the sizeof() of position being stored. In our case
  its just a position. Look at the bdb code if you want to see a case
  where something other then a number is stored.
*/
void ha_warp::position(const uchar *) {
  DBUG_ENTER("ha_warp::position");
  my_store_ptr(ref, ref_length, current_rowid);
  DBUG_VOID_RETURN;
}

/*
  Used to fetch a row from a posiion stored with ::position().
  my_get_ptr() retrieves the data for you.
*/

int ha_warp::rnd_pos(uchar *buf, uchar *pos) {
  int rc;
  DBUG_ENTER("ha_warp::rnd_pos");
  ha_statistic_increment(&System_status_var::ha_read_rnd_count);
  current_rowid = my_get_ptr(pos, ref_length);
  cursor->fetch(current_rowid);
  rc = find_current_row(buf);
  DBUG_RETURN(rc);
}

/*
  Called after each table scan. In particular after deletes,
  and updates. In the last case we employ chain of deleted
  slots to clean up all of the dead space we have collected while
  performing deletes/updates.
*/
int ha_warp::rnd_end() {
  DBUG_ENTER("ha_warp::rnd_end");
  blobroot.Clear();
  
  delete cursor;
  cursor = NULL;
  delete filtered_table;
  filtered_table = NULL;
  delete base_table;
  base_table = NULL;
  
  DBUG_RETURN(0);
}

ulong ha_warp::index_flags(uint, uint, bool) const {
  DBUG_ENTER("ha_warp::index_flags");
  //DBUG_RETURN(HA_READ_NEXT | HA_READ_RANGE | HA_KEYREAD_ONLY | HA_DO_INDEX_COND_PUSHDOWN);
  DBUG_RETURN(HA_READ_NEXT | HA_READ_RANGE | HA_KEYREAD_ONLY);
}

/* FIXME: quite sure this is not implemented correctly, but I always want to 
   prefer indexes over table scans right now, and prefer unique indexes
   over non-unique indexes.  Everything is going to be pushed down with ECP
   anyway...
*/
  ha_rows ha_warp::records_in_range(uint idxno, key_range *, key_range *) {
  /* UNIQUE indexes return 1 row, all other indexes return 2 rows */
    if(table->key_info[idxno].flags & HA_NOSAME) { 
      return 1;
    } else {
      return 2;
    }
  }

  void ha_warp::get_auto_increment	(	
    ulonglong 	offset,
    ulonglong 	increment,
    ulonglong 	nb_desired_values,
    ulonglong * 	first_value,
    ulonglong * 	nb_reserved_values 
  );

  // bitmap indexes are not sorted 
  int ha_warp::index_init(uint idxno, bool sorted) {
    DBUG_ENTER("ha_warp::index_init");
    DBUG_PRINT("ha_warp::index_init",("Key #%d, sorted:%d",idxno, sorted));
    if(sorted) DBUG_RETURN(-1);
    DBUG_RETURN(index_init(idxno));
  }

  int ha_warp::index_init(uint idxno) { 
    DBUG_ENTER("ha_warp::index_init(uint)");
    active_index=idxno; 
    if(base_table == NULL) {
      base_table = new ibis::mensa(share->data_dir_name);
    }
    
    if(column_set == "") { 
      set_column_set();
    }

    DBUG_RETURN(0); 
  }

  int ha_warp::index_next(uchar * buf) {
    DBUG_ENTER("ha_warp::index_next");
    if(cursor->fetch() != 0) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    current_rowid = cursor->getCurrentRowNumber();
    find_current_row(buf);
    DBUG_RETURN(0);
  }

  int ha_warp::index_first(uchar * buf) {
    DBUG_ENTER("ha_warp::index_first");
    set_column_set();
    filtered_table = base_table->select(column_set.c_str(), "1=1");
    if(filtered_table == NULL) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    cursor = filtered_table->createCursor();
    if(cursor->fetch() != 0) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    current_rowid = cursor->getCurrentRowNumber();
    find_current_row(buf);

    DBUG_RETURN(0);
  }

  int ha_warp::index_end() {
    DBUG_ENTER("ha_warp::index_end");
    if(cursor) delete cursor;
    cursor = NULL;
    if(filtered_table) delete filtered_table;
    filtered_table = NULL;
    if(base_table) delete base_table;
    base_table = NULL;
    DBUG_RETURN(0);
  }

  int ha_warp::make_where_clause(const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag, std::string& where_clause, uint32_t idxno) {
    DBUG_ENTER("ha_warp::make_where_clause");
    where_clause = "";
    int index_to_use = idxno ? idxno : active_index;
    /* If the bit is set, then the part is being used.  Unfortunately MySQL will only 
       consider prefixes so we need to use ECP for magical performance.
    */
    auto key_offset = key;
    for (uint16_t partno = 0; partno < table->key_info[index_to_use].actual_key_parts; partno++ ) {
      /* given index (a,b,c) and where a=1 quit when we reach the b key part
         given a=1 and b=2 then quit when we reach the c part
      */
      if(!(keypart_map & (1<<partno))){
        DBUG_RETURN(0);
      }
      /* What field is this? */
      Field* f = table->key_info[index_to_use].key_part[partno].field;
      
      if(partno >0) where_clause += " AND ";

      /* Which column number does this correspond to? */
      where_clause += "c" + std::to_string(table->key_info[index_to_use].key_part[partno].field->field_index);
      
      switch(find_flag) {
        case HA_READ_AFTER_KEY:
          where_clause += " > ";
          break;
        case HA_READ_BEFORE_KEY:
          where_clause += " < ";
          break;
        case HA_READ_KEY_EXACT:
          where_clause += " = ";
          break;
        case HA_READ_KEY_OR_NEXT:
          where_clause += ">=";
          break;
    
        case HA_READ_KEY_OR_PREV:
          where_clause += "<=";
          break;
    
        case HA_READ_PREFIX:
        case HA_READ_PREFIX_LAST:
        case HA_READ_PREFIX_LAST_OR_PREV:
        default:
          DBUG_RETURN(-1);
      }

      bool is_unsigned = f->flags & UNSIGNED_FLAG;
    
      switch(f->real_type()) {
        case MYSQL_TYPE_TINY:
          if(is_unsigned) {
            where_clause += std::to_string((uint8_t)(key_offset[0]));
          } else {
            where_clause += std::to_string((int8_t)(key_offset[0]));
          }
          key_offset += 1;
          break;

        case MYSQL_TYPE_SHORT:
          if(is_unsigned) {
            where_clause += std::to_string((uint16_t)__bswap_16(*key_offset));
          } else {
            where_clause += std::to_string((int16_t)__bswap_16(*key_offset));
          }
          key_offset += 2;
          break;

        case MYSQL_TYPE_INT24:
          if(is_unsigned) {
            where_clause += std::to_string((uint32_t)(key_offset[3]<<0 | key_offset[1]<<8 | key_offset[2]<<16));
          } else {
            where_clause += std::to_string((int32_t)(key_offset[3]<<0 | key_offset[1]<<8 | key_offset[2]<<16));
          }
          key_offset += 3;
          break;
        
        case MYSQL_TYPE_LONG:
          if(is_unsigned) {
            where_clause += std::to_string((uint32_t)(key_offset[0]<<0 | key_offset[1]<<8 | key_offset[2]<<16) | key_offset[3]<<24);
          } else {
            where_clause += std::to_string((int32_t)(key_offset[0]<<0 | key_offset[1]<<8 | key_offset[2]<<16) | key_offset[3]<<24);
          }
          key_offset += 4;
          break;     
        
        case MYSQL_TYPE_LONGLONG:
          if(is_unsigned) {
            where_clause += std::to_string((uint64_t)__bswap_64(*key_offset));
          } else {
            where_clause += std::to_string((int64_t)__bswap_64(*key_offset));
          }
          key_offset += 8;
          break;

        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_JSON: {
          // for strings, the key buffer is fixed width, and there is a two byte prefix
          // which lists the string length
          // FIXME: different data types probably have different prefix lengths
          uint16_t strlen = (uint16_t)(*key_offset);
          
          where_clause += "'" + std::string((const char*)key_offset+2, strlen) + "'";  
          key_offset += table->key_info[index_to_use].key_part[partno].store_length;
          break;
        }

        case MYSQL_TYPE_FLOAT:
          where_clause += std::to_string((float_t)*(key_offset));
          key_offset += 4;
          break;

        case MYSQL_TYPE_DOUBLE:
          where_clause += std::to_string((double_t)*(key_offset));
          key_offset += 8;
          break;
        
        // Support lookups for these types
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:         
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_YEAR:
        case MYSQL_TYPE_NEWDATE: 
        case MYSQL_TYPE_BIT:
        case MYSQL_TYPE_NULL:
        case MYSQL_TYPE_TIMESTAMP2:
        case MYSQL_TYPE_DATETIME2: 
        case MYSQL_TYPE_TIME2:   
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_GEOMETRY:
	default:
          DBUG_RETURN(-1);
          break;
      }

      /* exclude NULL columns */
      //if(f->real_maybe_null()) {
      //  where_clause += " and n" + std::to_string(f->field_index) + " = 0";
      //}
    }
    std::cout << "MADE WHERE CLAUSE: " << where_clause << "\n";
    DBUG_RETURN(0);
  }
  
  int ha_warp::index_read_map (uchar *buf, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag) {
      DBUG_ENTER("ha_warp::index_read_map");
      //DBUG_RETURN(HA_ERR_WRONG_COMMAND);
      std::string where_clause;
      set_column_set();

      make_where_clause(key, keypart_map, find_flag, where_clause);
      
      if(!cursor) {
        base_table = new ibis::mensa(share->data_dir_name);
        if(!base_table){
          DBUG_RETURN(-1);
        }
        filtered_table = base_table->select(column_set.c_str(), where_clause.c_str());
        if(filtered_table == NULL) {
          DBUG_RETURN(HA_ERR_END_OF_FILE);
        }

        cursor = filtered_table->createCursor();
        if(!cursor) {
          DBUG_RETURN(-1);
        } 
        current_rowid = cursor->getCurrentRowNumber();
      }   
      
      if(cursor->fetch() != 0) {
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }
      current_rowid = cursor->getCurrentRowNumber();
      find_current_row(buf);

      DBUG_RETURN(0);

    }

  int ha_warp::index_read_idx_map (uchar *buf, uint idxno, const uchar *key, key_part_map keypart_map, enum ha_rkey_function find_flag) {
    DBUG_ENTER("ha_warp::index_read_idx_map");
    index_init(idxno);
    std::string where_clause = "";
    int rc = index_read_map(buf, key, keypart_map, find_flag);
    index_end();
    DBUG_RETURN(rc);
  }
