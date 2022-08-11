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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA 

  ------
  
  This software uses MySQL which is also available under the above GPL2 
  license.
  
  ------

  This software uses Fastbit:
  "FastBit, Copyright (c) 2014, The Regents of the University of
  California, through Lawrence Berkeley National Laboratory (subject to
  receipt of any required approvals from the U.S. Dept. of Energy).  All
  rights reserved."

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  (1) Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  (2) Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  (3) Neither the name of the University of California, Lawrence Berkeley
  National Laboratory, U.S. Dept. of Energy nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
  PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  You are under no obligation whatsoever to provide any bug fixes,
  patches, or upgrades to the features, functionality or performance of
  the source code ("Enhancements") to anyone; however, if you choose to
  make your Enhancements available either publicly, or directly to
  Lawrence Berkeley National Laboratory, without imposing a separate
  written license agreement for such Enhancements, then you hereby grant
  the following license: a non-exclusive, royalty-free perpetual license
  to install, use, modify, prepare derivative works, incorporate into
  other computer software, distribute, and sublicense such enhancements or
  derivative works thereof, in binary and source code form.
*/
#define WARP_BITMAP_DEBUG
#include "ha_warp.h"
#ifdef WARP_USE_SIMD_INTERSECTION
#include "include/lemire-sorted-simd/codecfactory.h"
#include "include/lemire-sorted-simd/intersection.h"
using namespace SIMDCompressionLib;
#endif

int warp_push_to_engine(THD *, AccessPath *, JOIN *);
// Stuff for shares */
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

struct st_mysql_storage_engine warp_storage_engine = { MYSQL_HANDLERTON_INTERFACE_VERSION };
static int warp_init_func(void *p);
static int warp_done_func(void *p);

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
  NULL,             /* status variables                */
  system_variables, /* system variables    */
  NULL,             /* config options                  */
  0,                /* flags                           */
} mysql_declare_plugin_end;

 
static int warp_init_func(void *p) {
  DBUG_ENTER("warp_init_func");
  sql_print_information("WARP storage engine initialization started");
  handlerton *warp_hton;
  if(my_cache_size>0) {
    ibis::fileManager::adjustCacheSize(my_cache_size);
  }

  ibis::init(NULL, "/tmp/fastbit.log");
  ibis::util::setVerboseLevel(0);
#ifdef HAVE_PSI_INTERFACE
  init_warp_psi_keys();
#endif
  
  warp_hton = (handlerton *)p;
  mysql_mutex_init(warp_key_mutex_warp, &warp_mutex, MY_MUTEX_INIT_FAST);
  warp_open_tables.reset(
      new collation_unordered_multimap<std::string, WARP_SHARE *>(
          system_charset_info, warp_key_memory_warp_share));
  warp_hton->state = SHOW_OPTION_YES;
  warp_hton->db_type = DB_TYPE_UNKNOWN;
  warp_hton->create = warp_create_handler;
  warp_hton->flags = (HTON_CAN_RECREATE | HTON_NO_PARTITION);
  warp_hton->file_extensions = ha_warp_exts;
  warp_hton->rm_tmp_tables = default_rm_tmp_tables;
  warp_hton->commit = warp_commit;
  warp_hton->rollback = warp_rollback;
  warp_hton->push_to_engine = warp_push_to_engine;
  
  // starts the database and reads in the database state, upgrades
  // tables and does crash recovery
  warp_state = new warp_global_data();
  
  assert(warp_state != NULL);
  sql_print_information("WARP storage engine initialization completed");
  DBUG_RETURN(0);
}

static int warp_done_func(void *) {
  sql_print_information("WARP storage engine shutdown started");
  warp_open_tables.reset();

  // destroying warp_state writes the state to disk
  delete warp_state;
  mysql_mutex_destroy(&warp_mutex);
  sql_print_information("WARP storage engine shutdown completed");
  return 0;
}

/* Construct the warp handler */
ha_warp::ha_warp(handlerton *hton, TABLE_SHARE *table_arg) 
: handler(hton, table_arg),
  base_table(NULL),
  filtered_table(NULL),
  cursor(NULL),
  writer(NULL),
  current_rowid(0),
  blobroot(warp_key_memory_blobroot, BLOB_MEMROOT_ALLOC_SIZE) 
{
  warp_hton = hton;
}

const char **ha_warp::bas_ext() const {  
  return ha_warp_exts;
}

int ha_warp::rename_table(const char * from, const char * to, const dd::Table* , dd::Table* ) {
  DBUG_ENTER("ha_example::rename_table ");
  std::string cmd = "mv " + std::string(from) + ".data/ " + std::string(to) + ".data/";
  
  system(cmd.c_str()); 
  DBUG_RETURN(0);
}

bool ha_warp::is_deleted(uint64_t rownum) {
  return warp_state->delete_bitmap->is_set(rownum);
}

/*
void ha_warp::get_auto_increment(ulonglong, ulonglong, ulonglong,
                                 ulonglong *first_value,
                                 ulonglong *nb_reserved_values) {
  *first_value = stats.auto_increment_value ? stats.auto_increment_value : 1;
  *nb_reserved_values = ULLONG_MAX;
}
*/

int ha_warp::encode_quote(uchar *) {
  char attribute_buffer[1024];
  String attribute(attribute_buffer, sizeof(attribute_buffer), &my_charset_bin);
  buffer.length(0);

  for (Field **field = table->field; *field; field++) {
    const char *ptr;
    const char *end_ptr;

    /* For both strings and numeric types, the value of a NULL
       column in the database is 0. This value isn't ever used
       as it is just a placeholder. The associated NULL marker
       is marked as 1.  There are no NULL markers for columns
       which are NOT NULLable.

       This side effect must be handled by condition pushdown
       because comparisons for the value zero must take into
       account the NULL marker and it is also used to handle
       IS NULL/IS NOT NULL too.
    */
    if((*field)->is_null()) {
      buffer.append("0,1,");
      continue;
    }

    /* Convert the value to string */
    bool no_quote = false;
    attribute.length(0);
    switch((*field)->real_type()) {
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
        (*field)->val_str(&attribute, &attribute);
        break;

      case MYSQL_TYPE_YEAR:
        attribute.append((*field)->data_ptr()[0]);

        no_quote = true;
        break;

      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_NEWDATE:
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_DATETIME2:
      case MYSQL_TYPE_TIME2: 
      {
        MYSQL_TIME tmp_ltime;
        (*field)->get_date(&tmp_ltime, 6);
        auto tmp = TIME_to_longlong_datetime_packed(tmp_ltime);
        attribute.append(std::to_string(tmp).c_str());
        no_quote = true;
      }

      break;

      default:
        (*field)->val_str(&attribute, &attribute);
        break;
    }

    /* MySQL is going to tell us that the date and time types need quotes
       in string form, but they are being written into the storage engine
       in integer format the quotes are not needed in this encapsulation.
    */
    if((*field)->str_needs_quotes() && !no_quote) {
      ptr = attribute.ptr();
      end_ptr = attribute.length() + ptr;

      buffer.append('"');

      for (; ptr < end_ptr; ptr++) {
        if(*ptr == '"') {
          buffer.append('\\');
          buffer.append('"');
        } else if(*ptr == '\r') {
          buffer.append('\\');
          buffer.append('r');
        } else if(*ptr == '\\') {
          buffer.append('\\');
          buffer.append('\\');
        } else if(*ptr == '\n') {
          buffer.append('\\');
          buffer.append('n');
        } else if(*ptr == 0) {
          buffer.append('\\');
          buffer.append('0');
        } else {
          buffer.append(*ptr);
        }
      }
      buffer.append('"');
    } else {
      buffer.append(attribute);
    }

    /* A NULL marker (for example the column n0 for column c0) is
       marked as zero when the value is not NULL. The NULL marker
       column is always included in a fetch for the corresponding
       cX column. NOT NULL columns do not have an associated NULL
       marker.  Note the trailing comma (also above).
    */
    if((*field)->is_nullable()) {
      buffer.append(",0,");
    } else {
      buffer.append(',');
    }
  }

  /* the RID column is at the end of every table */
  buffer.append(std::to_string(current_rowid).c_str());
    
  /* add the transaction identifier */
  auto current_trx=warp_get_trx(warp_hton, table->in_use);
  assert(current_trx != NULL);
  buffer.append(",");
  buffer.append(std::to_string(current_trx->trx_id).c_str());
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
  if(it == warp_open_tables->end()) {
    if(!my_multi_malloc(warp_key_memory_warp_share, MYF(MY_WME | MY_ZEROFILL),
                         &share, sizeof(*share), &tmp_name, length + 1,
                         NullS)) {
      mysql_mutex_unlock(&warp_mutex);
      return NULL;
    }

    share->use_count = 0;
    share->table_name.assign(table_name, length);
    /* This is where the WARP data is actually stored.  It is usually 
       something like /var/lib/mysql/dbname/tablename.data
    */
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

bool ha_warp::check_and_repair(THD *) {
  HA_CHECK_OPT check_opt;
  DBUG_ENTER("ha_warp::check_and_repair");
  /*
  check_opt.init();

  DBUG_RETURN(repair(thd, &check_opt));
  */
  DBUG_RETURN(-1);
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
  if(!--share->use_count) {
    warp_open_tables->erase(share->table_name.c_str());
    thr_lock_delete(&share->lock);
    mysql_mutex_destroy(&share->mutex);
    my_free(share);
  }
  mysql_mutex_unlock(&warp_mutex);
  
  DBUG_RETURN(result_code);
}


int ha_warp::set_column_set() {
  DBUG_ENTER("ha_warp::set_column_set");
  column_set = "";

  int count = 0;
  for (Field **field = table->field; *field; field++) {
    if(bitmap_is_set(table->read_set, (*field)->field_index()) || current_thd->lex->sql_command == SQLCOM_UPDATE || current_thd->lex->sql_command == SQLCOM_UPDATE_MULTI || current_thd->lex->sql_command == SQLCOM_DELETE || current_thd->lex->sql_command == SQLCOM_DELETE_MULTI ) {
      ++count;

      /* this column must be read from disk */
      column_set += std::string("c") + std::to_string((*field)->field_index());

      /* Add the NULL bitmap for the column if the column is NULLable */
      if((*field)->is_nullable()) {
        column_set +=
            "," + std::string("n") + std::to_string((*field)->field_index());
      }
      column_set += ",";
    }
  }

  /* The RID column (r) needs to be read always in order to support UPDATE and
     DELETE. For queries that neither SELECT nor PROJECT columns, the RID column
     will be projected regardless.  The RID column is never included in the
     result set.

     The TRX_ID column (t) must be read for transaction visibility and to 
     exclude rows that were not commited.
  */
  column_set += "r,t";

  update_column_set.clear();
  nullable_column_set.clear();
  count=0;
  for (Field **field = table->field; *field; field++) {
    if(bitmap_is_set(table->write_set, (*field)->field_index())) { 
      update_column_set.push_back((*field)->field_index());
    }
    if((*field)->is_nullable()) {
        nullable_column_set.push_back(1);
    } else {
       nullable_column_set.push_back(0);
    }
  }

  DBUG_RETURN(count + 1);
}

/* store the binary data for each returned value into the MySQL buffer
   using field->store()
*/
int ha_warp::find_current_row(uchar *buf, ibis::table::cursor *cursor) {
  DBUG_ENTER("ha_warp::find_current_row");
  int rc = 0;
  memset(buf, 0, table->s->null_bytes);
  
  // Clear BLOB data from the previous row.
  blobroot.ClearForReuse();

  /* Avoid asserts in ::store() for columns that are not going to be updated */
  my_bitmap_map *org_bitmap(dbug_tmp_use_all_columns(table, table->write_set));
  
  /* Read all columns when a table is opened for update */
  

  for (Field **field = table->field; *field; field++) {
    buffer.length(0);
    if(bitmap_is_set(table->read_set, (*field)->field_index()) || current_thd->lex->sql_command == SQLCOM_UPDATE ||  current_thd->lex->sql_command == SQLCOM_UPDATE_MULTI  || current_thd->lex->sql_command == SQLCOM_DELETE || current_thd->lex->sql_command == SQLCOM_DELETE_MULTI ) {
      
      bool is_unsigned = (*field)->all_flags() & UNSIGNED_FLAG;
      std::string cname = "c" + std::to_string((*field)->field_index());
      std::string nname = "n" + std::to_string((*field)->field_index());

      if((*field)->is_nullable()) {
        unsigned char is_null = 0;

        rc = cursor->getColumnAsUByte(nname.c_str(), is_null);

        /* This column value is NULL */
        if(is_null != 0) {
          (*field)->set_null();
          rc = 0;
          continue;
        }
      }

      switch((*field)->real_type()) {
        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_YEAR: {
          if(is_unsigned) {
            unsigned char tmp = 0;
            rc = cursor->getColumnAsUByte(cname.c_str(), tmp);
            rc = (*field)->store(tmp, true);
          } else {
            char tmp = 0;
            rc = cursor->getColumnAsByte(cname.c_str(), tmp);
            rc = (*field)->store(tmp, false);
          }
          break;
        }
        case MYSQL_TYPE_SHORT: {
          if(is_unsigned) {
            uint16_t tmp = 0;
            rc = cursor->getColumnAsUShort(cname.c_str(), tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int16_t tmp = 0;
            rc = cursor->getColumnAsShort(cname.c_str(), tmp);
            rc = (*field)->store(tmp, false);
          }
        } break;

        case MYSQL_TYPE_LONG: {
          if(is_unsigned) {
            uint32_t tmp = 0;
            rc = cursor->getColumnAsUInt(cname.c_str(), tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int32_t tmp = 0;
            rc = cursor->getColumnAsInt(cname.c_str(), tmp);
            rc = (*field)->store(tmp, false);
          }
        } break;

        case MYSQL_TYPE_LONGLONG: {
          uint64_t tmp = 0;
          if(is_unsigned) {
            rc = cursor->getColumnAsULong(cname.c_str(), tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int64_t tmp = 0;
            rc = cursor->getColumnAsLong(cname.c_str(), tmp);
            rc = (*field)->store(tmp, false);
          }
        } break;

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
          if((*field)->store(tmp.c_str(), tmp.length(), (*field)->charset(),
                              CHECK_FIELD_WARN)) {
            rc = HA_ERR_CRASHED_ON_USAGE;
            goto err;
          }
          if((*field)->all_flags() & BLOB_FLAG) {
            Field_blob *blob_field = down_cast<Field_blob *>(*field);
            
            size_t length = blob_field->get_length();
            // BLOB data is not stored inside buffer. It only contains a
            // pointer to it. Copy the BLOB data into a separate memory
            // area so that it is not overwritten by subsequent calls to
            // Field::store() after moving the offset.
            if(length > 0) {
              const unsigned char *old_blob;
              old_blob = blob_field->data_ptr();
              unsigned char *new_blob = new (&blobroot) unsigned char[length];
              
              if(new_blob == nullptr) DBUG_RETURN(HA_ERR_OUT_OF_MEM);
              memcpy(new_blob, old_blob, length);
              blob_field->set_ptr(length, new_blob);

            }
          }
        }

        break;

        case MYSQL_TYPE_FLOAT: {
          float_t tmp;
          rc = cursor->getColumnAsFloat(cname.c_str(), tmp);
          rc = (*field)->store(tmp);
        } break;

        case MYSQL_TYPE_DOUBLE: {
          double_t tmp;
          rc = cursor->getColumnAsDouble(cname.c_str(), tmp);
          rc = (*field)->store(tmp);
        } break;

        case MYSQL_TYPE_INT24: {
          uint32_t tmp;
          if(is_unsigned) {
            rc = cursor->getColumnAsUInt(cname.c_str(), tmp);
            rc = (*field)->store(tmp, true);
          } else {
            int32_t tmp;
            rc = cursor->getColumnAsInt(cname.c_str(), tmp);
            rc = (*field)->store(tmp, false);
          }
        } break;

        case MYSQL_TYPE_NEWDATE:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_TIME2:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_TIMESTAMP2:
        case MYSQL_TYPE_DATETIME2: {
          
          uint64_t tmp;
          rc = cursor->getColumnAsULong(cname.c_str(), tmp);
          rc = (*field)->store_packed(tmp);
        } break;
        /* the following are stored as strings in Fastbit */
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
        case MYSQL_TYPE_NULL:
        case MYSQL_TYPE_BIT:
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_GEOMETRY: {
          std::string tmp;
          rc = cursor->getColumnAsString(cname.c_str(), tmp);
          if((*field)->store(tmp.c_str(), tmp.length(), (*field)->charset(),
                              CHECK_FIELD_WARN)) {
            rc = HA_ERR_CRASHED_ON_USAGE;
            goto err;
          }
        } break;

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
  /* ECP is reset here */
  push_where_clause = "";

  DBUG_RETURN(0);
}

void ha_warp::update_row_count() {
  DBUG_ENTER("ha_warp::row_count");
  if(base_table == NULL) {
    base_table = new ibis::mensa(share->data_dir_name);
  }

  stats.records = base_table->nRows();

  //delete base_table;
  //base_table = NULL;
  DBUG_VOID_RETURN;
}

int ha_warp::open(const char *name, int, uint, const dd::Table *) {
  DBUG_ENTER("ha_warp::open");
  if(!(share = get_share(name, table))) DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  update_row_count();

  //  FIXME: support concurrent insert for LDI
  thr_lock_data_init(&share->lock, &lock, (void *)this);
  ref_length = sizeof(my_off_t);

  /* These closures are used to allow concurrent insert.  It isn't
     working with LOAD DATA INFILE though.  LDI sends 0 for the
     concurrent_insert parameter and requests a TL_WRITE lock.
     INSERT INTO ... however sends 1 and requests a
     TL_WRITE_CONCURRENT_INSERT lock and concurent insert works. I
     need to figure out how to get MySQL to allow concurrent
     insert for LDI
  */

  auto get_status = [](void *, int) { return; };

  auto update_status = [](void *) { return; };

  auto check_status = [](void *) { return false; };

  share->lock.get_status = get_status;
  share->lock.update_status = update_status;
  share->lock.check_status = check_status;

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
  if(writer) {
    writer->clearData();
    delete writer;
  } 
  writer = NULL;
  if(cursor) delete cursor;
  cursor = NULL;
  if(filtered_table) delete filtered_table;
  filtered_table = NULL;
  if(base_table) delete base_table;
  base_table = NULL;

  DBUG_RETURN(free_share(share));
}

void ha_warp::start_bulk_insert(ha_rows) {
}

int ha_warp::end_bulk_insert() {
  if(writer != NULL) {
    /* foreground write actually because it is not executed in a different `thread */
    write_buffered_rows_to_disk();
  }

  return 0;
}

std::string ha_warp::get_writer_partition() {
  auto parts = new ibis::partList ;
  int partition_count = ibis::util::gatherParts(*parts, share->data_dir_name);
  std::string retval;
  write_mutex.lock();
  // if there is only one partition, the table is empty and a new p0 must be created
  if(partition_count == 1) {
    retval =  std::string(share->data_dir_name) + std::string("/p0");
    goto done;
  }
  
  for (auto it = parts->begin(); it < parts->end(); ++it) {
    // skip the top level partition
    if(std::string((*it)->currentDataDir()) == std::string(share->data_dir_name)) {
      continue;
    }
    // find the partition with the least number of rows (top level partition is excluded above)
    if(writer->mRows() + (*it)->nRows() <= my_partition_max_rows) {
      retval = std::string((*it)->currentDataDir());
      goto done;
    }
  }   
  
  retval = std::string(share->data_dir_name) + std::string("/p") + (std::to_string(parts->size()-1));
  done:
  write_mutex.unlock();
  
  for(auto it=parts->begin();it!=parts->end();++it) {
    delete *it;
    *it=NULL;
  }
  delete parts;
  return retval;
}

/* write the rows and destroy the writer*/
void ha_warp::write_buffered_rows_to_disk() {
  mysql_mutex_lock(&share->mutex);

  std::string part_dir = get_writer_partition();
  auto part_dir_copy = strdup(part_dir.c_str());
  auto part_name = basename(part_dir_copy);
  writer->write(part_dir.c_str(), part_name);
  writer->clearData();
  //maintain_indexes(part_dir.c_str());
  free(part_dir_copy);
  //delete writer;
  //writer = NULL;
  
  mysql_mutex_unlock(&share->mutex);
};

/*
  This is an INSERT.  The row data is converted to CSV (just like the CSV
  engine) and this is passed to the storage layer for processing.  It would be
  more efficient to construct a vector of rows to insert (to also support bulk
  insert).
*/
int ha_warp::write_row(uchar *buf) {
  DBUG_ENTER("ha_warp::write_row");
  ha_statistic_increment(&System_status_var::ha_write_count);
  
  mysql_mutex_lock(&share->mutex);
  if(share->next_rowid == 0 || share->rowids_generated >= WARP_ROWID_BATCH_SIZE) {
    share->next_rowid = warp_state->get_next_rowid_batch();
    share->rowids_generated = 0;
  } 
  current_rowid = share->next_rowid--;
  share->rowids_generated++;
  mysql_mutex_unlock(&share->mutex);
    
  /* This will return a cached writer unless a background
    write was started on the last insert.  In that case
    a new writer is constructed because the old one will
    still be background writing.
  */
  create_writer(table);
  mysql_mutex_lock(&share->mutex);
  mysql_mutex_unlock(&share->mutex);
  /* The auto_increment value isn't being properly persisted between
    restarts at the moment.  AUTO_INCREMENT should definitely be
    considered and ALPHA level feature.
  */
  if(table->next_number_field && buf == table->record[0]) {
    int error;
    if((error = update_auto_increment())) DBUG_RETURN(error);
  }

  /* This encodes the data from the row buffer into a CSV string which
    is processed by Fastbit...  It is probably faster to construct a
    Fastbit row object but this is fast enough for now/ALPHA release.
  */
  ha_warp::encode_quote(buf);

  /* The writer object caches rows in memory.  Memory is reserved
    for a given number of rows, which defaults to 1 million.  The
    Fastbit cache size must be greater than or equal to this value
    or an allocation failure will happen.
  */
  writer->appendRow(buffer.c_ptr(), ",");
  stats.records++;
  
  /* In order to check for duplicate keys in a single insert
     statement the writer has to be flushed for each insert
     statement, which is not optimal - maybe there is a 
     better solution
  */
  /*if(unique_check_where_clause != "") {
    foreground_write();
    current_trx->write_insert_log_rowid(current_rowid);
  } else
  */
  if(writer->mRows() >= my_write_cache_size) {
    // write the rows to disk and destroy the writer (a new one will be created)
    write_buffered_rows_to_disk();
  }
  DBUG_RETURN(0);
}

// Updating a row in WARP is a bit weird.  A new version of the row is 
// written into the table and a LOCK_EX is taken on the row.  The
// delete bitmap isn't written until the transaction commits. The
// deleted row is written into the transaction log and it gets set
// when the log is read at commit which is quite different from
// InnoDB.  A history lock is also taken on the row.  During future
// scans this verion of the row will not be visible to this or 
// newer transactions and will be visible to older transactions.
int ha_warp::update_row(const uchar *, uchar *new_data) {
  is_update=true;
  DBUG_ENTER("ha_warp::update_row");
  auto current_trx = warp_get_trx(warp_hton, table->in_use);
  assert(current_trx != NULL);
  
  /*delete cursor;
  delete filtered_table;
  delete base_table;
  cursor = NULL;
  filtered_table = NULL;
  base_table = NULL;*/


  int lock_taken = warp_state->create_lock(current_rowid, current_trx, LOCK_EX);
  /* if deadlock or lock timeout return the error*/
  if(lock_taken != LOCK_EX) {
    DBUG_RETURN(lock_taken);
  }

  // current_rowid will be changed by write_row so save the 
  // value now
  uint64_t deleted_rowid = current_rowid;
  
  // if the write fails (for example due to duplicate key then
  // the statement will be rolled back and the deleted row 
  // will be 
  int retval = write_row(new_data);
  
  if(retval == 0) {
    // only log the delete and create the history lock 
    // if the write completed successfully.  The EX_LOCK
    // will still be held so the update can be retried
    // without having to lock the row again. 
    current_trx->write_delete_log_rowid(deleted_rowid);
    
    warp_state->create_lock(deleted_rowid, current_trx, LOCK_HISTORY);
  }
  
  ha_statistic_increment(&System_status_var::ha_update_count);
  is_update=false;

  
  DBUG_RETURN(retval);
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
  
  auto current_trx = warp_get_trx(warp_hton, table->in_use);
  assert(current_trx != NULL);
  int lock_taken = warp_state->create_lock(current_rowid, current_trx, LOCK_EX);
  /* if deadlock or lock timeout return the error*/
  if(lock_taken != LOCK_EX) {
    DBUG_RETURN(lock_taken);
  }
  warp_state->create_lock(current_rowid, current_trx, LOCK_HISTORY);
  current_trx->write_delete_log_rowid(current_rowid);
  
  ha_statistic_increment(&System_status_var::ha_delete_count);
  stats.records--;
  DBUG_RETURN(0);
}

int ha_warp::delete_table(const char *table_name, const dd::Table *) {
  DBUG_ENTER("ha_warp::delete_table");

  // FIXME: this needs to be safer
  std::string cmdline =
      std::string("rm -rf ") + std::string(table_name) + ".data/";
  int rc = system(cmdline.c_str());
  ha_statistic_increment(&System_status_var::ha_delete_count);
  DBUG_RETURN(rc != 0);

}

// int ha_warp::truncate(dd::Table *dd) {
int ha_warp::delete_all_rows() {
  int rc = delete_table(share->table_name.c_str(), NULL);
  if(rc) return rc;
  return create(share->table_name.c_str(), table, NULL, NULL);
}

WARP_SHARE* ha_warp::get_warp_share() {
  return share;
}
/*
  ::info() is used to return information to the optimizer.
  Currently this table handler doesn't implement most of the fields
  really needed. SHOW also makes use of this data
*/
int ha_warp::info(uint) {
  DBUG_ENTER("ha_warp::info");
  std::unordered_map<std::string, bool> query_tables;
  close_in_extra = true;
  
  auto table_counts = get_table_counts_in_schema(share->data_dir_name);

  auto thd = current_thd;
  auto cur_table = thd->open_tables;
  
  while(cur_table != NULL) {
    auto handler = (ha_warp*)(cur_table->file);
    auto other_share = handler->get_warp_share();
    query_tables.emplace(std::make_pair(other_share->data_dir_name, true));
    cur_table=cur_table->next;
  }
  
  const char* table_with_most_rows = get_table_with_most_rows(&table_counts, query_tables);
  uint64_t least_row_count = get_least_row_count(&table_counts);
  assert(table_with_most_rows != NULL);
  bool is_fact_table = false;

  // list the tables in the query
  // if this is the fact table (largest table in schema) set the records to the smallest possible value
  // which is 2 (otherwise const evaluation will be used)
  if(strstr(share->data_dir_name, table_with_most_rows) != NULL) {
    is_fact_table = true;
    if(THDVAR(table->in_use, adjust_table_stats_for_joins)) {
      stats.records = least_row_count+2;
    }
  } 

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
  
  /* register the table for condition pushdown..  ::info is always called before 
     ::engine_push so this ensures the table information for hybrid join is
     available when we get there
  */
  warp_pushdown_information* pushdown_info = get_or_create_pushdown_info(table->in_use, table->alias, share->data_dir_name);
  pushdown_info->fields = table->s->field;
  set_column_set();

  pushdown_info->column_set = column_set;

  if(is_fact_table) {
    pushdown_info->is_fact_table = true;
  }

  DBUG_RETURN(0);
}

int file_exists(const char *file_name) {
  struct stat buf;
  return (stat(file_name, &buf) == 0);
}

int file_exists(std::string file_name) {
  return file_exists(file_name.c_str());
}

void index_builder(ibis::table* tbl, const char* cname, const char* comment) {
  tbl->buildIndex(cname, comment);
}
/* Fastbit will normally maintain the indexes automatically, but if the type
   of bitmap index is to be set manually, the comment on the field will be
   taken into account. */
void ha_warp::maintain_indexes(const char *datadir) {
  
  ibis::table::stringArray columns;
  std::string opt = "";
  
  auto tbl = new ibis::part(datadir);
  for (Field **field = table->field; *field; field++) {
    std::string columnIndexFilename = std::string(datadir) + "/c" + std::to_string((*field)->field_index()) + ".idx";
    std::string columnIndexNullFilename = std::string(datadir) + "/n" + std::to_string((*field)->field_index());
    if(file_exists(columnIndexFilename)) {
      ibis::fileManager::instance().flushFile(columnIndexFilename.c_str());
      auto col = tbl->getColumn((*field)->field_index());
      if(col->hasIndex()) {
        col->loadIndex();
        if (col->indexedRows() != tbl->nRows() ) {
          // update the index if the existing one does not
          // have the same number of rows as the current data
          // partition
          col->unloadIndex();
          //col->purgeIndexFile();
          auto idx = ibis::index::create(col, NULL);
          delete idx;
        }
        col->unloadIndex();
      }
    }
  }
}

/* The ::extra function is called a bunch of times before and after various
   storage engine operations. I think it is used as a hint for faster alter
   for example. Right now, in warp if there are any dirty rows buffered in
   the writer object, flush them to disk when ::extra is called.   Seems to
   work.
*/
int ha_warp::extra(enum ha_extra_function) {
  /* if not bulk insert, and there are buffered inserts, write them out
     to disk.  This will destroy the writer.
  */
  /*if(writer != NULL) {
    write_buffered_rows_to_disk();
  }*/
  return 0;
}

void ha_warp::cleanup_pushdown_info() {
  // free up memory used for pushdown filters
  auto pushdown_info = get_pushdown_info(table->in_use, table->alias);
  
  pushdown_mtx.lock();
  auto it = pd_info.find(table->in_use);
  if(it != pd_info.end()) {
  
    // remove the pushdown info for this table
    auto it2 = it->second->find(table->alias);
    if(it2 != it->second->end()) {
      it->second->erase(it2);
    }
    // if all of the tables are removed delete the pushdown info completely
    if(it->second->size() == 0) {
      delete it->second;
      pd_info.erase(it);
    }

  }
  
  delete pushdown_info;
  pushdown_mtx.unlock();
  fact_table_filters.clear();
  pushdown_table_count = 0;
  bitmap_merge_join_executed = false;
}  

int ha_warp::repair(THD *, HA_CHECK_OPT *) {
  DBUG_ENTER("ha_warp::repair");
  my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "REPAIR is not supported");
  DBUG_RETURN(HA_ERR_UNSUPPORTED);
}

/*
  Called by the database to lock the table. Keep in mind that this
  is an internal lock.
*/
THR_LOCK_DATA **ha_warp::store_lock(THD *, THR_LOCK_DATA **to,
                                    enum thr_lock_type lock_type) {
  DBUG_ENTER("ha_warp::store_lock");

  lock_in_share_mode = false;
  lock_for_update = false;

  if(lock_type == TL_READ_WITH_SHARED_LOCKS) {
    lock_in_share_mode = true;
  }

  if(lock_type == TL_WRITE) {
    lock_for_update = true;
  }

  if(lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {
    lock.type = lock_type;
  }

  DBUG_RETURN(to);
}

void ha_warp::create_writer(TABLE *table_arg) {
  if(writer != NULL) {
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
  int column_count = 0;
  const char *index_spec=NULL;
  /* Create an empty writer.  Columns are added to it */
  writer = ibis::tablex::create();

  for (Field **field = table_arg->s->field; *field; field++) {
    std::string name = "c" + std::to_string(column_count);
    std::string nname = "n" + std::to_string(column_count);
    ++column_count;

    ibis::TYPE_T datatype = ibis::UNKNOWN_TYPE;
    bool is_unsigned = (*field)->all_flags() & UNSIGNED_FLAG;
    bool is_nullable = (*field)->is_nullable();

    /* create a tablex object to create the metadata for the table */
    switch((*field)->real_type()) {
      case MYSQL_TYPE_TINY:
        index_spec="<binning none/><encoding interval/>";
        if(is_unsigned) {
          datatype = ibis::UBYTE;
        } else {
          datatype = ibis::BYTE;
        }
        break;

      case MYSQL_TYPE_SHORT:
        index_spec="<binning none/><encoding interval-equality/>";
        if(is_unsigned) {
          datatype = ibis::USHORT;
        } else {
          datatype = ibis::SHORT;
        }
        break;

      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_LONG:
        index_spec="<binning none><encoding binary/>";
        if(is_unsigned) {
          datatype = ibis::UINT;
        } else {
          datatype = ibis::INT;
        }
        break;

      case MYSQL_TYPE_LONGLONG:
        index_spec="<binning none/><encoding binary/>";
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
        index_spec="<binning none/><encoding binary/>";
        datatype = ibis::TEXT;
        break;

      case MYSQL_TYPE_FLOAT:
        index_spec="<binning precision=2/><encoding interval-equality/>";
        datatype = ibis::FLOAT;
        break;

      case MYSQL_TYPE_DOUBLE:
        index_spec="<binning precision=2/><encoding interval-equality/>";
        datatype = ibis::DOUBLE;
        break;

      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
        index_spec="<binning none/><encoding binary/>";
        datatype = ibis::TEXT;
        break;

      case MYSQL_TYPE_YEAR:
        index_spec="<binning none/><encoding interval-equality/>";
        datatype = ibis::UBYTE;
        break;

      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_NEWDATE:
     /*   index_spec="<binning none/><encoding interval-equality/>";
        datatype = ibis::UINT;
        break;*/

      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_TIME2:
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_DATETIME2:
        index_spec="<binning none/><encoding interval-equality/>";
        datatype = ibis::ULONG;
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
        std::string errmsg = "Unsupported data type for column: " +
                             std::string((*field)->field_name);
        my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), errmsg.c_str());
        datatype = ibis::UNKNOWN_TYPE;
        break;
    }
    /* Fastbit supports numerous bitmap index options.  You can place these
       options in the comment string of a column.  When an index is created on a
       column, then the indexing options used are taken from the comment.  In
       the future, the comment will support more options for compression, etc.
    */
    const char *custom_spec;
    if((custom_spec = strstr((*field)->comment.str, "index=")) != NULL) {
      index_spec = custom_spec+6;  
      if(index_spec[0] != '<') {
        std::string tmp_spec = "<" + std::string(custom_spec) + "/>";
        index_spec = tmp_spec.c_str();
      }
    } else {
      if(index_spec == NULL) { 
        index_spec = "<binary/>";
      }
    }
    
    writer->addColumn(name.c_str(), datatype, NULL, index_spec);

    /* Columns which are NULLable have a NULL marker.  A better approach might
       to have one NULL bitmap stored as a separate column instead of one byte
       per NULLable column, but that makes query processing a bit more complex
       so this simpler approach is taken for now.  Also, once compression is
       implemented, these columns will shrink quite a bit.
    */
    if(is_nullable) {
      // correspondingly numbered column");
      writer->addColumn(nname.c_str(), ibis::UBYTE,
                        "NULL marker for the correspondingly numbered column",
                        "<binning none/><encoding equality/>");
                        //"none");
    }
  }
  /* This is the psuedo-rowid which is used for deletes and updates */
  writer->addColumn("r", ibis::ULONG, "WARP rowid");

    /* This is the psuedo-rowid which is used for deletes and updates */
  writer->addColumn("t", ibis::ULONG, "WARP transaction identifier");

  /* This is the memory buffer for writes*/
  //writer->reserveBuffer(my_write_cache_size > my_partition_max_rows
  //                          ? my_partition_max_rows
  //                          : my_write_cache_size);

  /* FIXME: should be a table option and should be able to be set in size not
   * just count*/
  writer->setPartitionMax(my_partition_max_rows);
  mysql_mutex_lock(&share->mutex);
  mysql_mutex_unlock(&share->mutex);
}

/*
  Create a table. You do not want to leave the table open after a call to
  this (the database will call ::open() if it needs to).

  Note that the internal Fastbit columns are named after the field numbers
  in the MySQL table.
*/
// int ha_warp::create(const char *name, TABLE *table_arg, HA_CREATE_INFO *info,
int ha_warp::create(const char *name, TABLE *table_arg, HA_CREATE_INFO *,
                    dd::Table *) {
  DBUG_ENTER("ha_warp::create");
  int rc = 0;
  if(!(share = get_share(name, table))) DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  /* create the writer object from the list of columns in the table */
  create_writer(table_arg);

  char errbuf[MYSYS_STRERROR_SIZE];
  DBUG_PRINT("ha_warp::create",
             ("creating table data directory=%s", share->data_dir_name));
  if(file_exists(share->data_dir_name)) {
    delete_table(name, NULL);
  }
  if(mkdir(share->data_dir_name, S_IRWXU | S_IXOTH) == -1) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             my_strerror(errbuf, sizeof(errbuf), errno));
    DBUG_RETURN(-1);
  }

  /* Write the metadata to disk (returns 1 on success but this function returns
     0 on success...) Be nice and try to clean up if metadata write failed (out
     of disk space for example)
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
  DBUG_RETURN(HA_ADMIN_OK);
}

bool ha_warp::check_if_incompatible_data(HA_CREATE_INFO *, uint) {
  return COMPATIBLE_DATA_YES;
}

/* This is where table scans happen.  While most storage engines
   scan ALL rows in this function, the WARP engine supports
   engine condition pushdown.  This means that the WHERE clause in
   the SQL statement is made available to the WARP engine for
   processing during the scan.

   This has MAJOR performance implications.

   Fastbit can evaluate and satisfy with indexes many complex
   conditions that MySQL itself can not support efficiently
   (or at all) with btree or hash indexes.

   These include conditions such as:
   col1 = 1 OR col2 = 1
   col1 < 10 and col2 between 1 and 2
   (col1 = 1 or col2 = 1) and col3 = 1

   Fastbit evaluation will bitmap intersect the index results
   for each evaluated expression.  Fastbit will automatically
   construct indexes for these evaluations when appropriate.
*/
int ha_warp::rnd_init(bool) {
  DBUG_ENTER("ha_warp::rnd_init");
  fetch_count = 0;
  auto pushdown_info = get_pushdown_info(table->in_use, table->alias);
  char* partition_filter = THDVAR(table->in_use, partition_filter);
  full_partition_scan = false;
  /* extract/use the partition filter if provided*/
  uint partition_filter_len = strlen(partition_filter);
  partition_filter_alias = "";
  partition_filter_partition_name = "";
  // partition filter is of form alias: pX 
  // minimum alias is one char, plus two char delim, plus two chars for partition = 5 chars
  
  if(partition_filter_len >= 5) {
    for(uint delim_at = 0;delim_at<partition_filter_len;delim_at++) {
      if(partition_filter[delim_at] == ':' && partition_filter[delim_at+1] == ' ') {
        partition_filter_alias.assign(partition_filter, delim_at);
      
        if(partition_filter_alias == table->alias) {
          partition_filter_partition_name.assign(partition_filter + delim_at+2);
        }
        break;
      }
    }
  }
  
  current_rowid = 0;
  /* When scanning this is used to skip evaluation of transactions
     that have already been evaluated
  */  
  last_trx_id = 0;

  /* This is the a big part of the performance advantage of WARP outside of
     the bitmap indexes.  This figures out which columns this query is reading
     so we only read the necesssary columns from disk.

     This is the primary advantage of a column store.
  */
  set_column_set();

  /* push_where_clause is populated in ha_warp::cond_push() which is the
     handler function invoked by engine condition pushdown.  When ECP is
     used, then push_where_clause will be a non-empty string.  If it
     isn't used, then the WHERE clause is set such that Fastbit will
     return all rows.
  */
  if(push_where_clause == "") {
    push_where_clause = "1=1";
  }

  if(pushdown_info->base_table != NULL) {
    partitions = NULL;
    base_table = pushdown_info->base_table;
    filtered_table = pushdown_info->filtered_table;
    if(filtered_table != NULL && pushdown_info->cursor != NULL) {
      cursor = pushdown_info->cursor;
    } else {
      if(filtered_table != NULL) {
        cursor = filtered_table->createCursor();
        pushdown_info->cursor=cursor; 
      }
    }
 
    for(auto filter_it = pushdown_info->fact_table_filters->begin(); filter_it != pushdown_info->fact_table_filters->end(); ++filter_it) {
 
      if((filter_it->first)->dim_alias == std::string(table->alias)) {
        auto tmp = (filter_it->first);
        current_matching_dim_ridset_it = tmp->get_rownums()->begin();
 
        current_matching_dim_ridset = tmp->get_rownums();
        
        break;
      }
    }
    rownum = 0;
    
  } else {
    base_table = NULL;
    if( (get_pushdown_info_count(current_thd) > 1 && pushdown_info->is_fact_table) || partition_filter_partition_name != "" ) {
      partitions = new ibis::partList;
    
      // read all partitions unless a filter is set
      if(partition_filter_partition_name == "") {
        ibis::util::gatherParts(*partitions, share->data_dir_name, true);
        part_it = partitions->begin();
      } else {
        // only read one partition if filter is set
        std::string tmpstr = std::string(share->data_dir_name);
        tmpstr += "/" + partition_filter_partition_name;
        ibis::util::gatherParts(*partitions, tmpstr.c_str(), true);
        part_it = partitions->begin();
      }
    } else {
      partitions = new ibis::partList;
      full_partition_scan = true;
      ibis::util::gatherParts(*partitions, share->data_dir_name, true);
      part_it = partitions->begin();
    }
  }
  
  DBUG_RETURN(0);
}

void filter_fact_column(
  ibis::query* column_query, 
  fact_table_filter::iterator fact_filter, 
  std::vector<uint32_t>* matching_rids, 
  std::set<uint64_t>* matching_dim_rids,
  uint32_t* running_filter_threads,
  std::mutex* fact_filter_mutex ) 
  { 
    auto column_vals = column_query->getQualifiedLongs((fact_filter->first)->fact_column.c_str());
    
    uint32_t rownum = 1;
    std::vector<uint64_t> matching_dim_rowids;
    matching_dim_rowids.clear();
    for(auto column_it = column_vals->begin(); column_it != column_vals->end(); ++column_it) {
      ++rownum;
      
      auto find_it = fact_filter->second->find(*column_it);
      if( find_it != fact_filter->second->end() ) {
        matching_rids->push_back(rownum);
        matching_dim_rids->insert(find_it->second);
      }
  }
  delete column_vals;
  column_vals = NULL;
  
  fact_filter_mutex->lock();
  --(*running_filter_threads);
  fact_filter_mutex->unlock();
  
}
// this is expensive because it has to try to insert many existing entries
// into the existing set.  This presents a synchronization point in the 
// join process to it is moved into a dedicated thread
// there should be one running threads per dimension because a
// mutex is held as each dimension is processed

void merge_dimension_keys(
  fact_table_filter::iterator filter_it, 
  std::set<uint64_t>* matching_dim_rowids, 
  uint32_t* running_dimension_merges, 
  std::mutex* dimension_merge_mutex ) {
 
  filter_it->first->mtx.lock();
  for(auto insert_it = matching_dim_rowids->begin(); insert_it != matching_dim_rowids->end(); ++insert_it) {
    filter_it->first->add_matching_rownum(*insert_it);
  }
  filter_it->first->mtx.unlock();   
  delete matching_dim_rowids;
  
  dimension_merge_mutex->lock();

  (*running_dimension_merges)--;
  dimension_merge_mutex->unlock();
}

void exec_pushdown_join(
  ibis::query* column_query, 
  ibis::partList::iterator part_it, 
  fact_table_filter* fact_table_filters,
  std::unordered_map<std::string, std::vector<uint32_t>*>* matching_ridset,
  uint32_t* running_join_threads, 
  std::mutex* parallel_join_mutex,
  uint32_t* running_dimension_merges,
  std::mutex* dimension_merge_mutex ) {
          
  std::unordered_map<uint32_t, uint8_t> tmp_matching_rids;
  auto rid_it = tmp_matching_rids.begin();

  uint8_t filter_exec_count = 0;
  auto filter_it = fact_table_filters->begin();
  
  for ( filter_exec_count = 1; filter_it != fact_table_filters->end(); ++filter_it,++filter_exec_count) {
    auto column_vals = column_query->getQualifiedLongs((filter_it->first)->fact_column.c_str());
    if(!column_vals) {
      tmp_matching_rids.clear();
      break;
    }
    
    uint32_t rownum =0;
    auto matching_dim_rowids = new std::set<uint64_t> ;

    for(auto column_it = column_vals->begin(); column_it != column_vals->end(); ++column_it) {
      
      ++rownum;
      if( filter_exec_count > 1 ) {
        /* if this is the second or later pass over column data
           if this rownum did not already match, then it does not
           have to be looked up again.  Lookups into filter_it->second
           are 64 bit while lookups into matching_rids are 32 bit.  That
           makes this lookup considerably faster than the column lookup.
        */
        rid_it = tmp_matching_rids.find(rownum);
        if( rid_it == tmp_matching_rids.end() ) {
          continue;
        }

        if( rid_it->second != filter_exec_count - 1 ) {
          continue;
        }

      }  
      auto find_it = filter_it->second->find(*column_it);
      if( find_it == filter_it->second->end() ) {
        continue;
      }
      if( filter_exec_count == 1 ) {
        tmp_matching_rids.insert(std::make_pair(rownum,1)); 
      } else {
        rid_it->second++;
      }
     
      matching_dim_rowids->insert(find_it->second);
    
    }
    // free up columnar values
    delete column_vals;

    dimension_merge_mutex->lock();
    ++running_dimension_merges;
    dimension_merge_mutex->unlock();

    //deletes matching_dim_rowids
    std::thread(merge_dimension_keys, filter_it, matching_dim_rowids, running_dimension_merges, dimension_merge_mutex).detach();

    #if 0
    filter_it->first->mtx.lock();
              
    for(auto insert_it = matching_dim_rowids.begin(); insert_it != matching_dim_rowids.end(); ++insert_it) {
      filter_it->first->add_matching_rownum(*insert_it);
    }
    
    filter_it->first->mtx.unlock();    
    #endif

  }
  
  if( tmp_matching_rids.size() > 0 ) {
    
    auto tmp = std::string((*part_it)->currentDataDir());
    auto find_it = matching_ridset->find(tmp);
    assert(find_it != matching_ridset->end());

    auto filtered_matching_ids = new std::vector<uint32_t>;
    for(auto tmp_it = tmp_matching_rids.begin(); tmp_it != tmp_matching_rids.end(); ++tmp_it) {
      if( tmp_it->second == fact_table_filters->size() ) {
        filtered_matching_ids->push_back(tmp_it->first);
      }
    }
    
    if(filtered_matching_ids->size() > 0) {
      find_it->second = filtered_matching_ids;
    } else {
      find_it->second = NULL;
    }
  }


  parallel_join_mutex->lock();
  (*running_join_threads)--;
  parallel_join_mutex->unlock();

  delete column_query;
}

int ha_warp::rnd_next(uchar *buf) {
  DBUG_ENTER("ha_warp::rnd_next");
  
  // transaction id of the current row
  uint64_t row_trx_id = 0;
fetch_again:  
  
  if( !full_partition_scan && partitions != NULL && bitmap_merge_join_executed == false ) {
    if( std::string((*part_it)->currentDataDir()) == std::string(share->data_dir_name) ) {
      ++part_it;
    }
  
    while( part_it != partitions->end() ) {        
      //verify that the partition is valid / not empty
      base_table = ibis::table::create((*part_it)->currentDataDir());
      rownum = 0;
      if(!base_table) {
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }
      
      delete base_table;
      base_table = NULL;
      filtered_table = NULL;
      
      auto column_query = new ibis::query((const char*)(0), (*part_it), (const char*)(0));
      if( push_where_clause == "" ) {
        push_where_clause = "1=1";
      }
      
      column_query->addConditions(push_where_clause.c_str());
      column_query->evaluate();
      if( column_query->getNumHits() != 0 ) {
        // nothing happens if this function is called more than once during query evaluation
        // but it must be executed at least once when parallel hash join is being used
        bitmap_merge_join();
        
        // this is zero if join optimization is not being used
        if(fact_table_filters.size() == 0) {
        
          if( push_where_clause == "" ) {
            push_where_clause = "1=1";
          }
          
        } else {
        
          if( matching_ridset.size() == 0 ) {
            for(auto part_it2 = partitions->begin();part_it2 != partitions->end();++part_it2) {
              matching_ridset.emplace(std::make_pair(std::string((*part_it2)->currentDataDir()),(std::vector<uint32_t>*)NULL));
            }
          }  
          
          while(1) {
            parallel_join_mutex.lock();
            auto tmp = running_join_threads;
            parallel_join_mutex.unlock();
            
            if(tmp >= THDVAR(table->in_use, max_degree_of_parallelism) ) {
              
              struct timespec sleep_time;
              struct timespec remaining_time;
              sleep_time.tv_sec = (time_t)0;
              sleep_time.tv_nsec = 100000000L; // sleep a millisecond
              
              nanosleep(&sleep_time, &remaining_time);
              continue;
            } 
            
            parallel_join_mutex.lock();
            ++running_join_threads;
            
            parallel_join_mutex.unlock();
            std::thread(exec_pushdown_join,column_query, part_it, &fact_table_filters, &matching_ridset, &running_join_threads, &parallel_join_mutex, &running_dimension_merges, &dimension_merge_mutex).detach();
            break;

          }  
        }
      } 
      ++part_it;
      
    } // jobs for joining all the partitions involved in the query have been scheduled   
    
    part_it = partitions->begin();
    current_matching_ridset = NULL;
  } 
  
  // wait for scheduled join to complete
  while(all_jobs_completed == false) {
    parallel_join_mutex.lock();
    if( running_join_threads == 0 ) {
      parallel_join_mutex.unlock();
      // next time the mutex won't be held!
      all_jobs_completed = true;
      rownum = 0;
      break;
    }
    
    struct timespec sleep_time;
    struct timespec remaining_time;
    sleep_time.tv_sec = (time_t)0;
    sleep_time.tv_nsec = 100000000L; // sleep a bit
    parallel_join_mutex.unlock();
    nanosleep(&sleep_time, &remaining_time);
  }

  // there may be dimension merging still going on!
  while(all_dimension_merges_completed == false) {
    dimension_merge_mutex.lock();
    if( running_dimension_merges == 0 ) {
      dimension_merge_mutex.unlock();
      all_dimension_merges_completed = true;  
      continue;
    }
    dimension_merge_mutex.unlock();
    
    struct timespec sleep_time;
    struct timespec remaining_time;
    sleep_time.tv_sec = (time_t)0;
    sleep_time.tv_nsec = 100000000L; // sleep a bit
    parallel_join_mutex.unlock();
    nanosleep(&sleep_time, &remaining_time);
  }
  
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);
  
  next_ridset:

  if( matching_ridset.size() > 0 ) {
    if(part_it == partitions->end()) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    
    if( current_matching_ridset == NULL ) {
      auto find_it = matching_ridset.find(std::string((*part_it)->currentDataDir()));
      
      if( find_it == matching_ridset.end() ) {
        ++part_it;
        goto next_ridset;
      }
      
      if( find_it->second == NULL ) {
        ++part_it;
        goto next_ridset;
      } 
    
      if(base_table != NULL) {
        delete cursor;
        cursor = NULL;
        delete filtered_table;
        filtered_table = NULL;
        delete base_table;
        base_table = NULL;
      }
    
      current_matching_ridset = find_it->second;
      current_matching_ridset_it = find_it->second->begin();
      
      base_table = ibis::table::create(find_it->first.c_str());
      assert(base_table != NULL);
            
      // this will do some IO to read in projected columns that where not used for filters
      maintain_indexes(find_it->first.c_str());
      filtered_table = base_table->select(column_set.c_str(), push_where_clause.c_str());
      
      if(!filtered_table) {
        ++part_it;
        goto fetch_again;
      }
      
      cursor = filtered_table->createCursor();

    }
    
  } else {
    
    // table scan (possibly with filters) without any joins
    if(cursor == NULL) {
      if( std::string((*part_it)->currentDataDir()) == std::string(share->data_dir_name) ) {
      
        ++part_it;
        if(part_it == partitions->end()) {
          DBUG_RETURN(HA_ERR_END_OF_FILE);
        }
      }
      
      base_table = ibis::table::create((*part_it)->currentDataDir());
      assert(base_table != NULL);
      
      maintain_indexes((*part_it)->currentDataDir());
      filtered_table = base_table->select(column_set.c_str(), push_where_clause.c_str());
      
      if(filtered_table==NULL) {
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }
      
      cursor = filtered_table->createCursor();  
      rownum = 0;
    } 
  }
  
  if( !cursor ) {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  
  // will remain 10 if we hit the end of current_matching_ridset
  // otherwise is the result of the fetch.  if there is no 
  // current_matching_ridset the next row is fetched and if the
  // end of the resultset is reached, res will end up non-zero 
  int res = 10;
  if(matching_ridset.size() >0 ) {
    
    if(current_matching_ridset_it != current_matching_ridset->end()) {
      rownum = (*current_matching_ridset_it);
      res = cursor->fetch(rownum-1);
      ++current_matching_ridset_it;
    } 
    // if end of ridset res still = 10 here and the fetch failure
    // is handled below, objects are free'd etc..
  } else {
    
    // during pushdown joins the dimensions have a set of buffered rowids
    // this is a scan of one of the dimension tables (because current_matching_ridset_it )
    if( current_matching_dim_ridset != NULL ) {  
      if( current_matching_dim_ridset_it != current_matching_dim_ridset->end() ) {
        rownum = *current_matching_dim_ridset_it;
        res = cursor->fetch((*current_matching_dim_ridset_it)-1);
        ++current_matching_dim_ridset_it;
      } else {
        res = -1;
      }
    } else {
      res = cursor->fetch();
      ++rownum;
    }
  }
  
  if( res != 0 ) {
    fetch_count = 0;
    if( partitions != NULL && fact_table_filters.size() > 0 ) {
      // free up the memory used for buffering the matching rowids
      if( current_matching_ridset != NULL ) {
        delete current_matching_ridset;
        auto find_it = matching_ridset.find(std::string((*part_it)->currentDataDir()));
        find_it->second = NULL;
        current_matching_ridset = NULL;
      }
      
      // move to the next partition
      ++part_it;
      goto next_ridset;
    }
       
    if(current_matching_dim_ridset != NULL) {
      delete current_matching_dim_ridset;
      current_matching_dim_ridset = NULL;
    }
    
    if(partitions != NULL) {
      ++part_it;
      if(part_it == partitions->end()) {
        DBUG_RETURN(HA_ERR_END_OF_FILE); 
      }
      delete cursor;
      cursor = NULL;
      delete filtered_table;
      filtered_table=NULL;
      delete base_table;
      base_table=NULL;
      
      goto fetch_again;
    }
       
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }  
  
  ++fetch_count;
  
  cursor->getColumnAsULong("r", current_rowid);
  cursor->getColumnAsULong("t", row_trx_id);
  
  /* This sets is_trx_visible handler variable!
     If we already checked this trx_id in the last iteration
     then it does not have to be checked again and the
     is_trx_visible variable does not change. This function
     also sets last_trx_id to the transaction being c
     checked if the value is not the same as this 
     transaction.
  */

  
  is_trx_visible_to_read(row_trx_id);
  
  if(!is_trx_visible) {
    goto fetch_again;
  }
  
  // if the row would be visible due to row_trx_id it might not
  // be visible if it has been changed in a future transaction.
  // because the delete_rows bitmap has bits possibly committed
  // from future transaction, a history lock is created to 
  // maintain row visiblity
  if(!is_row_visible_to_read(current_rowid)) {
    goto fetch_again;
  }
  
  
  // Lock rows during a read if requested
  auto current_trx = warp_get_trx(warp_hton, table->in_use);
  int lock_taken = 0;
  if( lock_in_share_mode ) {
    lock_taken = warp_state->create_lock(current_rowid, current_trx, LOCK_SH);
    // row is exclusive locked so it has been deleted but this row should 
    // have already been skipped because it has a history lock
    if( lock_taken == LOCK_EX ) {
      goto fetch_again;
    }
  
    if( lock_taken != LOCK_SH && lock_taken != WRITE_INTENTION ) {
        // some sort of error happened like DEADLOCK or LOCK_WAIT_TIMEOUt
      DBUG_RETURN(lock_taken);
    }
  } else {
    if( lock_for_update ) {
      lock_taken = warp_state->create_lock(current_rowid, current_trx, WRITE_INTENTION);
      if( lock_taken != WRITE_INTENTION ) {
        DBUG_RETURN(lock_taken);
      }
    }  
  }
  find_current_row(buf, cursor);

  DBUG_RETURN(0);
}

/*
  Called after each table scan.
*/
int ha_warp::rnd_end() {
  DBUG_ENTER("ha_warp::rnd_end");
  
  blobroot.Clear();
   
  push_where_clause = "";

  if(cursor) delete cursor;
  if(filtered_table) delete filtered_table;
  if(base_table) delete base_table;

  if(partitions) {
    for(auto it=partitions->begin();it!=partitions->end();++it) {
      delete *it;
      *it=NULL;
    } 
    delete partitions;
  }
  
  if(writer != NULL) {
    write_buffered_rows_to_disk();
  }
  ibis::fileManager::instance().flushDir(share->data_dir_name);

  base_table = NULL;
  filtered_table = NULL;
  cursor = NULL;
  partitions = NULL;
  // these have to be reset for consecutive execution of queries on this 
  // THD / handle to continue working properly (ie not crash)
  matching_ridset.clear();
  fact_table_filters.clear();
  all_dimension_merges_completed = false;
  all_jobs_completed = false;
  current_matching_ridset = NULL;
  buffer.length(0);
  DBUG_RETURN(0);
}

/*
  This records the current position *in the active cursor* for the current row.
  This is a logical reference to the row which doesn't have any meaning outside
  of this scan because scans will have different row numbers when the pushed
  conditions are different.

  For similar reasons, deletions must mark the physical rowid of the row in the
  deleted RID map.
*/
void ha_warp::position(const uchar *) {
  DBUG_ENTER("ha_warp::position");
  my_store_ptr(ref, ref_length, current_rowid);
  DBUG_VOID_RETURN;
}

/*
  Used to seek to a logical posiion stored with ::position().
*/
int ha_warp::rnd_pos(uchar *buf, uchar *pos) {
  int rc;
  DBUG_ENTER("ha_warp::rnd_pos");
  
  ha_statistic_increment(&System_status_var::ha_read_rnd_count);
  current_rowid = my_get_ptr(pos, ref_length);
  base_table = ibis::mensa::create(share->data_dir_name);
  filtered_table = base_table->select(column_set.c_str(), ("r=" + std::to_string(current_rowid)).c_str());
  cursor = filtered_table->createCursor();
  
  rc = find_current_row(buf, cursor);
   
  delete cursor;
  delete filtered_table;
  delete base_table;
  cursor = NULL;
  filtered_table = NULL;
  base_table = NULL;
  DBUG_RETURN(rc);
}



/*
ulong ha_warp::index_flags(uint, uint, bool) const {
  // return(HA_READ_NEXT | HA_READ_RANGE | HA_KEYREAD_ONLY |
  // HA_DO_INDEX_COND_PUSHDOWN);
  //return (HA_READ_NEXT | HA_READ_RANGE | HA_KEYREAD_ONLY);
  return (HA_READ_NEXT | HA_READ_RANGE | HA_KEYREAD_ONLY);
}

ha_rows ha_warp::records_in_range(uint, key_range *, key_range *) {
  close_in_extra = true;
  auto pushdown_info = get_pushdown_info(table->in_use, table->alias);
  auto estimator = ibis::mensa::create(pushdown_info->datadir);
  uint64_t min=0;
  uint64_t max=0;
  ha_rows cnt = 0;

  // always scan the fact table
  if(pushdown_info->is_fact_table == true) {
    cnt = -1ULL;
  } else {
    if(pushdown_info->filter == "") {
      cnt = estimator->nRows();  
    } else {
      estimator->estimate(pushdown_info->filter.c_str(), min, max);
    }
  }
  delete estimator;
  return cnt > 0 ? cnt : max - min;
}

int ha_warp::index_init(uint idxno, bool) {
  DBUG_ENTER("ha_warp::index_init");
  // just prevents unused variable warning
  //if(sorted) sorted = sorted;

  //FIXME: bitmap indexes are not sorted so figure out what the sorted arg
  //means..
  //assert(!sorted);

  //DBUG_PRINT("ha_warp::index_init", ("Key #%d, sorted:%d", idxno, sorted));
  DBUG_RETURN(index_init(idxno));
}


int ha_warp::index_init(uint idxno) {
  active_index = idxno;
  last_trx_id = 0;
  current_trx = NULL;

  if(column_set == "") {
    set_column_set();
  }

  auto pushdown_info = get_pushdown_info(table->in_use, table->alias);
  if(pushdown_info->base_table != NULL) {
    base_table = pushdown_info->base_table;
    idx_filtered_table = pushdown_info->filtered_table;
    if(idx_filtered_table != NULL && pushdown_info->cursor != NULL) {
      idx_cursor = pushdown_info->cursor;
    } else {
      if(idx_filtered_table != NULL) {
        idx_cursor = idx_filtered_table->createCursor();
        pushdown_info->cursor=idx_cursor; 
      }
    }
  } else {
    if(base_table == NULL) {
      base_table = new ibis::mensa(share->data_dir_name);
      idx_filtered_table =
        base_table->select(column_set.c_str(), push_where_clause.c_str());
      if(idx_filtered_table != NULL) {
        // Allocate a cursor for any queries that actually fetch columns 
        idx_cursor = idx_filtered_table->createCursor();
      }
      pushdown_info->base_table = base_table;
      pushdown_info->filtered_table = idx_filtered_table;
      pushdown_info->cursor = idx_cursor;
    }
  }
    
  //idx_filtered_table_with_pushdown =
  //   base_table->select(column_set.c_str(), push_where_clause.c_str());
  return (0);
}

int ha_warp::index_next(uchar *buf) {
  DBUG_ENTER("ha_warp::index_next");
  uint64_t row_trx_id;
  
fetch_again:
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  if(idx_cursor->fetch() != 0) {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  idx_cursor->getColumnAsULong("t", row_trx_id);
  if(!is_trx_visible_to_read(row_trx_id)) {
    goto fetch_again;
  }

  idx_cursor->getColumnAsULong("r", current_rowid);
  if(!is_row_visible_to_read(current_rowid)) {
    goto fetch_again;
  }   
  find_current_row(buf, idx_cursor);
  DBUG_RETURN(0);
}

int ha_warp::index_first(uchar *buf) {
  DBUG_ENTER("ha_warp::index_first");
  ha_statistic_increment(&System_status_var::ha_read_first_count);
  uint64_t row_trx_id;
  last_trx_id = 0;
  current_trx = NULL;
  
  set_column_set();
  std::string where_clause;

  if(idx_filtered_table == NULL) {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }

fetch_again:
  if(idx_cursor->fetch() != 0) {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  idx_cursor->getColumnAsULong("t", row_trx_id);
  if(!is_trx_visible_to_read(row_trx_id)) {
    goto fetch_again;
  }
  
  idx_cursor->getColumnAsULong("r", current_rowid);
  if(!is_row_visible_to_read(current_rowid)) {
    goto fetch_again;
  } 
  find_current_row(buf, idx_cursor);

  DBUG_RETURN(0);
}

int ha_warp::index_end() {
  DBUG_ENTER("ha_warp::index_end");
  idx_cursor = NULL;
  idx_filtered_table = NULL;
  base_table = NULL;
  idx_where_clause = "";
  push_where_clause = "";
  DBUG_RETURN(0);
}


uint64_t ha_warp::lookup_in_hash_index(const uchar *key, key_part_map keypart_map,
                               enum ha_rkey_function find_flag) {
  DBUG_ENTER("ha_warp::lookup_in_hash_index");
  Field *f = table->key_info[active_index].key_part[0].field;
  
  uint64_t uintval = 0;
  int64_t intval = 0;
  double dblval = 0;
  std::string strval;

  bool is_unsigned = f->all_flags() & UNSIGNED_FLAG;
  bool is_int = false;
  bool is_uint = false;
  bool is_double = false;
  bool is_string = false;

  switch(f->real_type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
      if(is_unsigned) {
        is_uint = true;
        uintval = f->val_int();
      } else {
        is_int = true;
        intval = f->val_int();
      }
    break;

    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      is_double = true;
      dblval = f->val_real();
    break;

    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIME2: 
      is_unsigned = true;
      uintval = f->val_int();
    break;

    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET: 
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_NULL:
    case MYSQL_TYPE_GEOMETRY:
    {
      is_string = true;
      // for strings, the key buffer is fixed width, and there is a two byte
      // prefix which lists the string length
      String tmpval;
      tmpval.reserve(8192);
      f->val_str(&tmpval, &tmpval);
      std::string strval;
      strval.assign(tmpval.ptr(), tmpval.length());
    }
    break;
    default:
    break;
  }

  auto pushdown_info = get_pushdown_info(table->in_use, table->alias);
  
  if(is_uint) {
    auto it = pushdown_info->uint_to_row_map.find(uintval);
    if(it !=  pushdown_info->uint_to_row_map.end()) {
      return it->second;
    }
  } 

  if(is_int) {
    auto it=pushdown_info->int_to_row_map.find(intval);
    if(it !=  pushdown_info->int_to_row_map.end()) {
      return it->second;
    }
  } 

  if(is_double) {
    auto it=pushdown_info->double_to_row_map.find(dblval);
    if(it !=  pushdown_info->double_to_row_map.end()) {
      return it->second;
    }
  } 

  if(is_string) {
    auto it=pushdown_info->string_to_row_map.find(strval);
    if(it !=  pushdown_info->string_to_row_map.end()) {
      return it->second;
    }
  }

  // not found returns maximum ulonglong
  return -1ULL;

  DBUG_RETURN(0);
}


int ha_warp::index_read_map(uchar *buf, const uchar *key,
                            key_part_map keypart_map,
                            enum ha_rkey_function find_flag) {
  DBUG_ENTER("ha_warp::index_read_map");
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  // DBUG_RETURN(HA_ERR_WRONG_COMMAND);
  
  uint64_t row_trx_id;
  last_trx_id = 0;
  current_trx = NULL;
  auto pushdown_info = get_pushdown_info(table->in_use, table->alias);
  if(!idx_cursor) {
    base_table = pushdown_info->base_table;
    idx_filtered_table = pushdown_info->filtered_table;
  }
  if(idx_filtered_table == NULL) {
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  
  if(pushdown_info->cursor) {
    idx_cursor = pushdown_info->cursor;
  } else {
    idx_cursor = idx_filtered_table->createCursor();
  }  
  assert(idx_cursor != NULL);

  
fetch_again:
  if(idx_cursor->fetch() != 0) {
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
 
  idx_cursor->getColumnAsULong("t", row_trx_id);
  if(!is_trx_visible_to_read(row_trx_id)) {
    goto fetch_again;
  }
  
  idx_cursor->getColumnAsULong("r", current_rowid);
  if(!is_row_visible_to_read(current_rowid)) {
    goto fetch_again;
  } 

  find_current_row(buf, idx_cursor);
   DBUG_RETURN(0);
}

int ha_warp::index_read_idx_map(uchar *buf, uint idxno, const uchar *key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag) {
  DBUG_ENTER("ha_warp::index_read_idx_map");
  auto save_idx = active_index;
  active_index = idxno;
  int rc = index_read_map(buf, key, keypart_map, find_flag);
  active_index = save_idx;
  DBUG_RETURN(rc);
}
*/

/**
 * This function replaces (for SELECT queries) the handler::cond_push
 * function.  Instead of using an array of ITEM* it uses an
 * abstract query plan.
 * This function now calls ha_warp::cond_push to do the work that it used
 * to do in 8.0.20
 * @param table_aqp The specific table in the join plan to examine.
 * @return Possible error code, '0' if no errors.
 */
int warp_push_to_engine(THD * thd , AccessPath * root_path, JOIN * join) {
  AQP::Join_plan query_plan(thd, root_path, join);
  const uint count = query_plan.get_access_count();
  
  for (uint i = 0; i < count; i++) {
    AQP::Table_access *table_access = query_plan.get_table_access(i);

    const Item *cond = table_access->get_condition();
    const TABLE *table = table_access->get_table();
    QEP_TAB *qep_tab = table->reginfo.qep_tab;
    ha_warp *const ha = (ha_warp*)(table->file);
    
    if (cond == nullptr && join->where_cond == nullptr) { 
	    continue; 
    }
    
    auto share = ha->get_warp_share();
    auto pushdown_info = get_or_create_pushdown_info(table->in_use, table->alias, share->data_dir_name);
    assert(pushdown_info != nullptr);
    ha->push_where_clause = "";
    const Item* remainder=nullptr;
    const Item* remainder1=nullptr;
    if(cond) {
      remainder = ha->cond_push(cond,true);
    }
    auto save_where = ha->push_where_clause;
    if(join->where_cond) {
      ha->push_where_clause = "";
      remainder1 = ha->cond_push(join->where_cond, true);
    }
    if(ha->push_where_clause != "") {
	    ha->push_where_clause += " AND ";
    }
    ha->push_where_clause += save_where;
    
    assert(root_path->filter().condition != nullptr);
    if(remainder) 
	    root_path->filter().condition = const_cast<Item *>(remainder);
    // To get correct explain output: (Does NOT affect what is executed)
    // Need to set the QEP_TAB condition as well. Note that QEP_TABs
    //are not 'executed' any longer -> affects only explain output.
    // Can be removed when/if the 'traditional' explain is rewritten
    // to not use the QEP_TAB's
    if (qep_tab != nullptr) {
      // The Hypergraph-optimizer do not construct QEP_TAB's
      qep_tab->set_condition(const_cast<Item *>(remainder));
      qep_tab->set_condition_optim();
    }

    pushdown_info->filter = ha->push_where_clause;
  }
  
  return 0;
  
}

/* This is the ECP (engine condition pushdown) handler code.  This is where the
   WARP magic really happens from a MySQL standpoint, since it allows index
   usage that MySQL would not normally support and provides automatic indexing
   for filter conditions.

   This code is called from ha_warp::engine_push in 8.0.20+
*/
const Item *ha_warp::cond_push(const Item *cond, bool other_tbls_ok) {
  
  static int depth=0;
  static int unpushed_condition_count = 0;
  static int condition_count = 0;
  static std::string where_clause = "";

  // reset the variables when called at depth 0
  if(depth == 0) {
    condition_count = 0;
    unpushed_condition_count = 0;
    where_clause = "";
  }

  /* A simple comparison without conjuction or disjunction */
  if(cond->type() == Item::Type::FUNC_ITEM) {
    condition_count++;
    
    int rc = append_column_filter(cond, where_clause);
    if(rc != 1) {
      unpushed_condition_count++;
      where_clause += "1=1";
      return cond;
    }
    /* List of connected simple conditions */
  } else if(cond->type() == Item::Type::COND_ITEM) {
    auto item_cond = (dynamic_cast<Item_cond *>(const_cast<Item *>(cond)));
    List<Item> items = *(item_cond->argument_list());
    auto cnt = items.size();
    where_clause += "(";
    
    for (uint i = 0; i < cnt; ++i) {
      auto item = items.pop();
      condition_count++;
      if(i > 0) {
        if(item_cond->functype() == Item_func::Functype::COND_AND_FUNC) {
          where_clause += " AND ";
        } else if(item_cond->functype() == Item_func::Functype::COND_OR_FUNC) {
          where_clause += " OR ";
        } else {
          where_clause += "1=1";
          unpushed_condition_count++;
          /* not handled */
          return cond;
        }
      }
      /* recurse to print the field and other items.  This should be a
         FUNC_ITEM. if it isn't, then the item will be returned by this function
         and pushdown evaluation will be abandoned.
      */
      ++depth;
      
      if(cond_push(item, other_tbls_ok) != NULL) {
        unpushed_condition_count++;
        //items->push_back(item);
      }
      //items->push_front(item);
      --depth;
    }
    
    where_clause += ")";
  }
  
  // only push a where clause if there were condtiions that were actually pushed
  if(depth == 0 && (unpushed_condition_count != condition_count)){
    push_where_clause += where_clause;
  }
  
  if(unpushed_condition_count>0) {
     return cond;
  }
  return NULL;
}

/* return 1 if this clause could not be processed (will be processed by MySQL)*/
int ha_warp::append_column_filter(const Item *cond,
                                   std::string &where_clause) {
  bool field_may_be_null = false;
  bool is_between = false;
  bool is_in = false;
  bool is_is_null = false;
  bool is_isnot_null = false;
  bool is_eq = false;
  std::string build_where_clause = "";
  if(cond->type() == Item::Type::FUNC_ITEM) {

    Item_func *tmp = dynamic_cast<Item_func *>(const_cast<Item *>(cond));
    std::string op = "";

    /* There are only a small number of options currently available for
       filtering at the WARP SE level.  The basic numeric filters are presented
       here.
    */
    switch(tmp->functype()) {
      /* when op = " " there is special handling below because the
         syntax of the given function differs from the "regular"
         functions.
      */
      case Item_func::Functype::BETWEEN:
        is_between = true;
        break;

      case Item_func::Functype::IN_FUNC:
        is_in = true;
        break;

      case Item_func::Functype::ISNULL_FUNC:
        is_is_null = true;
        break;

      case Item_func::Functype::ISNOTNULL_FUNC:
        is_isnot_null = true;
        break;

      /* normal arg0 OP arg1 type operators */
      case Item_func::Functype::EQ_FUNC:
      case Item_func::Functype::EQUAL_FUNC:
        op = " = ";
        is_eq = true;
        break;

      case Item_func::Functype::LIKE_FUNC:
        op = " LIKE ";
        break;

      case Item_func::Functype::LT_FUNC:
        op = " < ";
        break;

      case Item_func::Functype::GT_FUNC:
        op = " > ";
        break;

      case Item_func::Functype::GE_FUNC:
        op = " >= ";
        break;

      case Item_func::Functype::LE_FUNC:
        op = " <= ";
        break;

      case Item_func::Functype::NE_FUNC:
        op = " != ";
        break;

      default:
        return 0;
    }

    Item **arg = tmp->arguments();
    //This is a fix for queries that have CONST filters on more than one table conjoined in an AND or an OR
    //when this happens, the field item will have a different alias from the table we are currently working
    //on (table->alias).  
    //For example, a TPC-H query contains the following:
    //and and ( (n1.n_name = 'JORDAN' and n2.n_name = 'BRAZIL') or (n1.n_name = 'BRAZIL' and n2.n_name = 'JORDAN') )                         
    //notice that there are AND conditions that compare constants in diffrent tables.
    for(size_t arg_num = 0; arg_num < tmp->arg_count-1; ++arg_num) {
      // if a field item refers to another field, then this is a join, and it is handled below in JOIN PUSHDOWN
      if( ( arg[arg_num]->type() == Item::Type::FIELD_ITEM && arg[arg_num+1]->type() != Item::Type::FIELD_ITEM ) )  {
        
        if(arg[arg_num]->used_tables() == 0) continue;
        
        auto str = ItemToString(arg[arg_num]);
        const char* dot_pos = strstr(str.c_str(), ".");
        const char* dot_pos2 = strstr(dot_pos+1, ".");
        std::string alias;
        //ssb.lineorder.LO_Quantity
        if(dot_pos2 != NULL) {
          alias=str.substr(dot_pos - str.c_str()+1, dot_pos2 - dot_pos -1);
        } else {
          alias=str.substr(0, dot_pos - str.c_str());
        }
        if(std::string(table->alias) != alias) {
          return 0;
        }
      }
    }
    /* JOIN PUSHDOWN
       ***********************************************************
       This detects where two fields are compared to each other in
       different tables which is a join condition.  The pushdown
       information is retrieved for both tables and pushdown conditions
       are attached to the larger table.  Note that nothing is pushed
       down right now, this just computes the structures for it to
       happen when a scan is initiated.
    */
    
    if(tmp->arg_count == 2 && arg[0]->type() == Item::Type::FIELD_ITEM &&
        arg[0]->type() == arg[1]->type()
    ) {
      
      // only support equijoin right now
      if(!is_eq) {
        return 0;
      }      
      
      Item_field* f0 = (Item_field *)(arg[0]);
      Item_field* f1 = (Item_field *)(arg[1]);

      // Get the pushdown information - something is quite broken if these are NULL
      auto f0_info = get_pushdown_info(table->in_use, f0->table_ref->alias);
      auto f1_info = get_pushdown_info(table->in_use, f1->table_ref->alias);
      
      if(f0_info == NULL || f1_info == NULL) {
        return 0;
      }
      
      bool this_is_dim_table = true;
      if(f1_info->datadir != share->data_dir_name) {
        this_is_dim_table = false; 
      }
      const char* dim_field_mysql_name;
      const char* fact_field_mysql_name;
      const char* dim_alias;
      //const char* fact_alias;
      
      // Used later translate the MySQL table column names into
      // WARP ordinal column names.  These are attched to the
      // join_info member in the pushdown structure.
      // join_info[fact_field] -> dim_info{dim_alias, dim_field}
      Field** dim_field;
      Field** fact_field;
      warp_join_info dim_info;

      // which table do we attach the join to?
      warp_pushdown_information* dim_table = NULL;
      warp_pushdown_information* fact_table = NULL;

      if(this_is_dim_table) {
        fact_table = f0_info;
        fact_field_mysql_name = f0->field_name;
        //fact_alias = f0->table_name;

        dim_table = f1_info;
        dim_field_mysql_name = f1->field_name;
        dim_alias = f1->table_name;
      } else {
        fact_table = f1_info;
        fact_field_mysql_name = f1->field_name;
        //fact_alias = f1->table_name;
        
        dim_table = f0_info;
        dim_field_mysql_name = f0->field_name;
        dim_alias = f0->table_name;
      }

      // find the field in the fact table
      for(fact_field = fact_table->fields; *fact_field; fact_field++) {
        //if((*fact_field)->field_name == fact_field_mysql_name) {
        //  break;
        //}
        if(strcasecmp((*fact_field)->field_name, fact_field_mysql_name) == 0) {
	  break;
        }
      }
      // have to find the field in the fact table or there was a serious error
      assert(*fact_field != NULL);

      // find the field in the dimension table
      for(dim_field = dim_table->fields; *dim_field; dim_field++) {
        //if((*dim_field)->field_name == dim_field_mysql_name) {    
        //   break;
        //}
        if(strcasecmp((*dim_field)->field_name, dim_field_mysql_name) == 0) {    
          break;
	}
      } 
      assert(*dim_field != NULL);

      dim_info.alias = dim_alias;
      dim_info.field = *dim_field;

      // attach the join to the fact table.  The actual pushdown will happen
      // when the table is first scanned (ie, ::rnd_init or ::index_init)
      fact_table->join_info.emplace(std::pair<Field*, warp_join_info>(*fact_field, dim_info));
      
      return 2;
    }
    
    /* BETWEEN AND IN() need some special syntax handling */
    for (uint i = 0; i < tmp->arg_count; ++i, ++arg) {
      if(i > 0) {
        if(!is_between && !is_in) { /* normal <, >, =, LIKE, etc */
          build_where_clause += op;
        } else {
          if(is_between) {
            if(i == 1) {
              build_where_clause += " BETWEEN ";
            } else {
              build_where_clause += " AND ";
            }
          } else {
            if(is_in) {
              if(i == 1) {
                build_where_clause += " IN (";
              } else {
                build_where_clause += ", ";
              }
            }
          }
        }
      }

      /* For most operators, only the column ordinal position is output here,
         but there is special handling for IS NULL and IS NOT NULL comparisons
         here too, because those functions only have one argument which is the
         field. These things only have meaning on NULLable columns of course,
         so there is special handling if the column is NOT NULL.
      */
      if((*arg)->type() == Item::Type::FIELD_ITEM) {
        auto field_index = ((Item_field *)(*arg))->field->field_index();
        field_may_be_null = ((Item_field *)(*arg))->field->is_nullable();

        /* this is the common case, where just the ordinal position is emitted
         */
        if(!is_is_null && !is_isnot_null) {
          /* If the field may be NULL it is necessary to check that that the
             NULL marker is zero because otherwise searching for 0 in a NULLable
             field would return true for NULL rows...
          */
          if(build_where_clause != "") {
            build_where_clause += " AND ";
          }
          if(field_may_be_null) {
            build_where_clause +=
                "(n" + std::to_string(field_index) + " = 0 AND ";
          }
          build_where_clause += "c" + std::to_string(field_index);
        } else {
          /* Handle IS NULL and IS NOT NULL, depending on NULLability */
          if(field_may_be_null) {
            if(is_is_null) {
              /* the NULL marker will be one if the value is NULL */
              build_where_clause += "(n" + std::to_string(field_index) + " = 1";
            } else if(is_isnot_null) {
              /* the NULL marker will be zero if the value is NOT NULL */
              build_where_clause += "(n" + std::to_string(field_index) + " = 0";
            }
          } else {
            if(is_is_null) {
              /* NOT NULL field is being compared with IS NULL so no rows can
               * match */
              build_where_clause += " 1=0 ";
            } else if(is_isnot_null) {
              /* NOT NULL field is being compared with IS NOT NULL so all rows
               * match */
              build_where_clause += " 1=1 ";
            }
          }
        }
        continue;
      }
      
      /* TODO:
         While there are some Fastbit functions that could be pushed down
         we don't handle that yet, but put this here as a reminder that it
         can be done at some point, as it will speed things up.  
         
         Special note: TEMPORAL values are passed down as an 
         Item_func::DATE_FUNC and the date is extracted from it.
      */
      if((*arg)->type() == Item::Type::CACHE_ITEM) {
        String str;
        //fixme: max_packet_length?
        str.reserve(1024*1024);
        (*arg)->print(current_thd, &str, QT_ORDINARY);
        if(
          memcmp("date",str.c_ptr()+9,4) == 0 || 
          strcasestr(str.c_ptr(), "interval ") != NULL ||
          strcasestr(str.c_ptr(), " as date") != NULL
          ) {
          auto t=(*arg)->val_temporal_by_field_type();
          build_where_clause += std::to_string(t);
          continue;        
        }
        // only date_sub date_add etc are supported right now
        return 0;
      }   

      if((*arg)->type() == Item::Type::FUNC_ITEM) {
        auto func_item = dynamic_cast<Item_func *>(*arg);
        
        switch(func_item->functype()) {
          case Item_func::DATE_FUNC:
          case Item_func::ADDTIME_FUNC:
            {  
            auto t=(*arg)->val_temporal_by_field_type();
            build_where_clause += std::to_string(t);
            }
            continue;
          break;
          default:
            return 0;
        }
      }
      
      if((*arg)->type() == Item::Type::INT_ITEM) {
        build_where_clause += std::to_string((*arg)->val_int());
        continue;
      }

      if((*arg)->type() == Item::Type::NULL_ITEM) {
        build_where_clause += " NULL ";
        continue;
      }

      // can't push down decimal comparisons as they are stored
      // as strings
      if((*arg)->type() == Item::Type::DECIMAL_ITEM) {
        //where_clause += "1=1";
        return 0;
      }

      if((*arg)->type() == Item::Type::REAL_ITEM) {
        String s;
        String *val = (*arg)->val_str(&s);
        build_where_clause += std::string(val->c_ptr());
        continue;
      }

      if((*arg)->type() == Item::Type::STRING_ITEM ||
         (*arg)->type() == Item::Type::VARBIN_ITEM) 
      {
        if(!is_eq) {
          return 0;
        }
        String s;
        String *val = (*arg)->val_str(&s);
        std::string escaped;
        char *ptr = val->c_ptr();
        for (unsigned int i = 0; i < val->length(); ++i) {
          char c = *(ptr + i);
          if(c == '\'') {
            escaped += '\\';
            escaped += '\'';
          } else if(c == 0) {
            escaped += '\\';
            escaped += '0';
          } else if(c == '\\') {
            escaped += "\\\\";
          } else {
            escaped += c;
          }
        }
        build_where_clause += "'" + escaped + "'";
        continue;
      }
    }

    if(is_in) {
      build_where_clause += ')';
    }

    if(field_may_be_null) {
      build_where_clause += ')';
    }
  }
  
  where_clause += build_where_clause;
  
  // clause was pushed down successfully 
  return 1;
}

int ha_warp::bitmap_merge_join() {
  if(bitmap_merge_join_executed != false) {
    return 0;
  }
  bitmap_merge_join_executed = true;
  auto fact_pushdown_info=get_pushdown_info(table->in_use, table->alias);
  
  if(fact_pushdown_info == NULL) {
    return 0;
  }
  
  std::string dim_pushdown_clause = "";
  
  for(auto join_it = fact_pushdown_info->join_info.begin(); join_it != fact_pushdown_info->join_info.end(); ++join_it) {
    
    Field* fact_field = join_it->first;
    // don't try to push down blob or JSON columns for joins 
    if(fact_field->real_type() == MYSQL_TYPE_TINY_BLOB ||
       fact_field->real_type() == MYSQL_TYPE_MEDIUM_BLOB ||
       fact_field->real_type() ==  MYSQL_TYPE_BLOB ||
       fact_field->real_type() ==  MYSQL_TYPE_LONG_BLOB ||
       fact_field->real_type() ==  MYSQL_TYPE_JSON) {
      continue;
    }
    
    Field* dim_field = join_it->second.field;
    auto dim_pushdown_info = get_pushdown_info(table->in_use, join_it->second.alias);
    if(dim_pushdown_info == NULL) {
      continue;
    }
    
    std::string fact_colname = std::string("c") + std::to_string(fact_field->field_index());
    std::string fact_nullname = std::string("n") + std::to_string(fact_field->field_index());
    std::string dim_colname = std::string("c") + std::to_string(dim_field->field_index());
    std::string dim_nullname = std::string("n") + std::to_string(dim_field->field_index());
    std::string dim_alias = join_it->second.alias;

    //FIXME: this is going to be needed to properly support outer joins
    //bool fact_is_nullable = fact_field->is_nullable();
    bool dim_is_nullable = dim_field->is_nullable();

    if(dim_pushdown_info->filter == "") {
	    continue;
	    //dim_pushdown_info->filter="1=1";
    } 
    
    dim_pushdown_clause = dim_pushdown_info->filter;
    if(dim_is_nullable) {
      dim_pushdown_clause += " AND " + dim_nullname + "=0";
    } 
    /* open the dimension table to read the data - the pointers are stored on the pushdown
        info structure so that they can be re-used in the scan
    */
    dim_pushdown_info->base_table = ibis::mensa::create(dim_pushdown_info->datadir);
    if(dim_pushdown_info->base_table == NULL) {
      continue;
    }
    
    dim_pushdown_info->filtered_table = 
    dim_pushdown_info->base_table->select(dim_pushdown_info->column_set.c_str(), dim_pushdown_clause.c_str());
    
    if(dim_pushdown_info->filtered_table == NULL) {
      continue;
    }

    auto dim_cursor = dim_pushdown_info->filtered_table->createCursor();      
    if(dim_cursor == NULL) {
      continue;
    }
  
    switch(dim_field->real_type()) {
      case MYSQL_TYPE_NULL:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_GEOMETRY:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_TYPED_ARRAY:
        continue;
        break;
  
    }
    
    auto matches = new std::unordered_map<uint64_t, uint64_t>;
    uint64_t rownum = 0;
    while(dim_cursor->fetch() == 0) {
      
      ++rownum;   
  
      bool is_unsigned = fact_field->is_unsigned();
      int rc=0;
      switch(dim_field->real_type()) {

        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_YEAR: {
          if(is_unsigned) {
            unsigned char tmp = 0;
            rc = dim_cursor->getColumnAsUByte(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));
          } else {
            char tmp = 0;
            rc = dim_cursor->getColumnAsByte(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));            
          }
          
        } break;

        case MYSQL_TYPE_SHORT: {
          if(is_unsigned) {
            uint16_t tmp = 0;
            rc = dim_cursor->getColumnAsUShort(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));

          } else {
            int16_t tmp = 0;
            rc = dim_cursor->getColumnAsShort(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));
          }
        } break;

        case MYSQL_TYPE_LONG: {
          if(is_unsigned) {
            uint32_t tmp = 0;
            rc = dim_cursor->getColumnAsUInt(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));

          } else {
            int32_t tmp = 0;
            rc = dim_cursor->getColumnAsInt(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));

          }
        } break;

        case MYSQL_TYPE_LONGLONG: {
          uint64_t tmp = 0;
          if(is_unsigned) {
            rc = dim_cursor->getColumnAsULong(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));

          } else {
            int64_t tmp = 0;
            rc = dim_cursor->getColumnAsLong(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));

          }
        } break;

        case MYSQL_TYPE_INT24: {
          if(is_unsigned) {
            uint32_t tmp;
            rc = dim_cursor->getColumnAsUInt(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));

          } else {
            int32_t tmp;
            rc = dim_cursor->getColumnAsInt(dim_colname.c_str(), tmp);
            matches->insert(std::make_pair(tmp, rownum));

          }
        } break;

        case MYSQL_TYPE_NEWDATE:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_TIME2:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_TIMESTAMP2:
        case MYSQL_TYPE_DATETIME2: {
          uint64_t tmp;
          rc = dim_cursor->getColumnAsULong(dim_colname.c_str(), tmp); 
          matches->insert(std::make_pair(tmp, rownum));

        } break;

        /* this should never happen but is here to avoid a warning */  
        default:
          continue;
      }
      if(rc != 0) {
        return -1;
      }
    } // end of fetch loop
    if( matches->size() > 0 ) {
      auto filter_info = new warp_filter_info(fact_colname, dim_alias, dim_colname);
      fact_table_filters.insert(std::make_pair(filter_info, matches));
    } else {
      delete matches;
    }
    dim_pushdown_info->fact_table_filters = &fact_table_filters;

  } // end of dim tables loop  
  
  return 0;
}

//FIXME: maybe this is supposed to do something?
//everything seems to work with it just returning zero
//but some unexpected bugs around transactions might be
//lurking.  Need to double check storage engine 
//interface documentation.  Note: Those docs are outdated and
//I think this function is no longer used...
//external_lock seems to handle it?
int ha_warp::start_stmt(THD *, thr_lock_type) {
  return 0;
}

int ha_warp::register_trx_with_mysql(THD* thd, warp_trx* trx) {
  long long all_trx = thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN | OPTION_TABLE_LOCK);
  if(all_trx && !trx->registered) {
    trx->registered = true;
    trans_register_ha(thd, true, warp_hton, const_cast<ulonglong*>(&(trx->trx_id)));
  }
  trans_register_ha(thd, false, warp_hton, const_cast<ulonglong*>(&(trx->trx_id)));
  return 0;
}

int ha_warp::external_lock(THD *thd, int lock_type){ 
    
  if (lock_type != F_UNLCK)  {
    auto current_trx = warp_get_trx(warp_hton, table->in_use);
    if(current_trx == NULL) current_trx = create_trx(table->in_use);
    assert(current_trx != NULL);
  
    register_trx_with_mysql(thd, current_trx);
    current_trx->lock_count++;

    if(lock_type == F_WRLCK) {
      current_trx->for_update = true;
    } else {
      current_trx->for_update = false;
    }

    /* serializable isolation level takes shared locks on all visible rows traveresed
      and so does LOCK IN SHARE MODE
    */
    if (current_trx->isolation_level == ISO_SERIALIZABLE) {
      current_trx->lock_in_share_mode=true;
    }

    enum_sql_command sql_command = (enum_sql_command)thd_sql_command(thd);
    if(sql_command == SQLCOM_UPDATE || sql_command == SQLCOM_UPDATE_MULTI ||
      sql_command == SQLCOM_INSERT ||
      sql_command == SQLCOM_REPLACE ||
      sql_command == SQLCOM_DELETE || sql_command == SQLCOM_DELETE_MULTI ||
      sql_command == SQLCOM_INSERT_SELECT ||
      sql_command == SQLCOM_LOAD ||
      sql_command == SQLCOM_ALTER_TABLE ||
      sql_command == SQLCOM_CREATE_TABLE)  {  
      // the first time a data modification statement is encountered
      // the transaction is marked dirty.  Registering the open
      // transaction prevents a transaction from seeing inserts
      // that are not visible to it and to still find duplicate
      // keys in transactions doing concurrent inserts
      if(!current_trx->dirty) {
        warp_state->register_open_trx(current_trx->trx_id);
        current_trx->dirty = true;
      }
    } 
  } else {
    // unlock the table
    cleanup_pushdown_info();
  }

  return 0;
}

warp_trx* ha_warp::create_trx(THD* thd) {
  trx_mutex.lock();
  auto trx = new warp_trx;
  trx->isolation_level = thd_get_trx_isolation(thd);
  thd->get_ha_data(warp_hton->slot)->ha_ptr = (void*)trx;
  trx->begin();
  trx->open_log();
  trx->autocommit = !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN | OPTION_TABLE_LOCK);
  trx_mutex.unlock();
  return trx;
}

void warp_trx::open_log() {
  if(log == NULL) {
    log_filename = std::to_string(trx_id) + std::string(".txlog");
    log=fopen(log_filename.c_str(), "w+");
    if(log == NULL) {
      sql_print_error("Could not open transaction log %s", log_filename.c_str());
      assert(false);
    }
  }
}

void warp_trx::write_insert_log_rowid(uint64_t rowid) {
  int sz = 0;
  sz = fwrite(&insert_marker, sizeof(insert_marker), 1, log);
  if(sz == 0 || ferror(log) != 0) {
    sql_print_error("failed to write rowid into insert log: %s", log_filename.c_str());
    assert(false);
  }
  sz = fwrite(&rowid, sizeof(uint64_t), 1, log);
  if(sz == 0 || ferror(log) != 0) {
    sql_print_error("failed to write rowid into insert log: %s", log_filename.c_str());
    assert(false);
  }
}

void warp_trx::write_delete_log_rowid(uint64_t rowid) {
  int sz = 0;
  //sql_print_warning("Writing delete rowid %d", rowid);
  sz = fwrite(&delete_marker, sizeof(delete_marker), 1, log);
  if(sz == 0 || ferror(log) != 0) {
    sql_print_error("failed to write rowid into insert log: %s", log_filename.c_str());
    assert(false);
  }
  sz = fwrite(&rowid, sizeof(uint64_t), 1, log);
  if(sz == 0 || ferror(log) != 0) {
    sql_print_error("failed to write rowid into insert log: %s", log_filename.c_str());
    assert(false);
  }
}


int warp_trx::begin() {
  int retval = 0;
  if(trx_id == 0) {
    trx_id = warp_state->get_next_trx_id();
  } else {
    retval = 1;
  }
  return retval;
}

// This causes some visibility problems - leave commented for now
// revisit in BETA 3.
void warp_global_data::cleanup_history_locks() {
#ifdef WARP_CLEANUP_HISTORY_LOCKS  
  commit_mtx.lock();
  history_lock_mtx.lock();

  uint64_t oldest_open_trx_id = 0;
  auto trx_it = commit_list.begin();
  while(trx_it != commit_list.end()) {
    dbug("trx_id: " << trx_it->first << ", state: " << trx_it->second);
    if(trx_it->second == WARP_UNCOMMITTED_TRX) {
      if(oldest_open_trx_id < trx_it->first) {
        oldest_open_trx_id = trx_it->first;
      }
    }
    ++trx_it;
  }
  dbug("Oldest open trx: " << oldest_open_trx_id);
     
  if(!oldest_open_trx_id) {
    history_lock_mtx.unlock();
    commit_mtx.unlock();
    return;
  };

  
  
  auto history_lock_it = history_locks.begin();
  while(history_lock_it != history_locks.end()) {
    if(history_lock_it->second < oldest_open_trx_id) {
      dbug("Removing HISTORY lock on trx: " << oldest_open_trx_id << " for rowid: " << history_lock_it->first);
      history_locks.erase(history_lock_it);
    }
    ++history_lock_it;
  }
  
  // remove the rolled back trx from the commit list
  trx_it = commit_list.begin();
  while(trx_it != commit_list.end()) {
    if(trx_it->first < oldest_open_trx_id && trx_it->second == WARP_ROLLED_BACK_TRX) {
      commit_list.erase(trx_it);
    }
  }

  history_lock_mtx.unlock();
  commit_mtx.unlock();
#endif
}

// used when a transaction commits
// not called when statements commit
void warp_trx::commit() {
  commit_mtx.lock();
  int sz = 0;
  uint64_t rowid = 0;
  char marker;
  
  auto commit_it = warp_state->commit_list.find(trx_id);
  if(dirty) {
  
    if(commit_it == warp_state->commit_list.end()) {
      sql_print_error("Open transaction not in commit list");
      assert(false);
    }

    sz = fwrite(&commit_marker, sizeof(commit_marker), 1, log);
    if(sz != 1) {
      sql_print_error("failed to write commit marker into transaction log");
    }
    fflush(log);
    fsync(fileno(log));
    fseek(log, 0, SEEK_SET);
    while( (sz = fread(&marker, sizeof(marker), 1, log) == 1) ) {
      switch(marker) {
        case savepoint_marker:
          continue;
          break;

         case commit_marker:
          continue;
          break;
      
        case insert_marker:
          // insertions are already written to disk
          fseek(log, sizeof(uint64_t), SEEK_CUR);
          break;

        case delete_marker:
          fread(&rowid, sizeof(uint64_t), 1, log);
          if(sz != 1) {
            sql_print_error("transaction log read failed");
            assert(false);
          }
          
          warp_state->delete_bitmap->set_bit(rowid);
          continue;
          break;

        default:
          sql_print_error("transaction log read failed");
          assert(false);
          break;
      }
    } 

    // commit the deletes
    if(warp_state->delete_bitmap->commit() != 0) {
      sql_print_error("Failed to commit delete bitmap %s", warp_state->delete_bitmap->get_fname().c_str());
      assert(false);
    }
    
    // mark the transaction committed
    int sz = fwrite(&trx_id, sizeof(trx_id), 1, warp_state->commit_file);
    if(sz != 1) {
      sql_print_error("Failed to write to commits file");
      assert(false);
    }
    // ensure commit marker is on disk
    fflush(warp_state->commit_file);
    fsync(fileno(warp_state->commit_file));
    
    commit_it->second = WARP_COMMITTED_TRX;
    
    
  }
  if(log) fclose(log);
  log = NULL;
  unlink(log_filename.c_str());
  
  commit_mtx.unlock();
}

// used when a transaction or statement rolls back
void warp_trx::rollback(bool all) {
  commit_mtx.lock();
  auto commit_it = warp_state->commit_list.find(trx_id);
  size_t savepoint_at = 0;
  uint64_t rowid = 0;
  char marker;
  if(dirty) {
    if(commit_it == warp_state->commit_list.end()) {
      sql_print_error("Open transaction not in commit list");
      assert(false);
    }
    int sz;
    if(all != ROLLBACK_STATEMENT) {
      sz = fwrite(&rollback_marker, sizeof(rollback_marker), 1, log);
      if(sz != 1) {
        sql_print_error("failed to write rollback marker into transaction log");
      }
    } 
    fflush(log);
    fsync(fileno(log));
    fseek(log, 0, SEEK_SET);
    clearerr(log);
  
    while((sz = fread(&marker, sizeof(marker), 1, log))) {
      switch(marker) {
        case rollback_marker:
          // nothing to do - end of log
        break;

        case savepoint_marker:
          savepoint_at = ftell(log) - sizeof(marker);
          continue;
        break;

        case insert_marker:
          if(all == ROLLBACK_STATEMENT && savepoint_at == 0) {
            fseek(log, sizeof(uint64_t), SEEK_CUR);
          }
          if(all == ROLLBACK_STATEMENT) {
            sz = fread(&rowid, sizeof(uint64_t), 1, log);
            if(feof(log)) {
              break;
            }
            if(sz != 1) {
              sql_print_error("could not read from transaction log");
              assert(false);
            }
            // delete this insert, which is equivalent to rolling it back
            if(warp_state->delete_bitmap->set_bit(rowid) != 0) {
              sql_print_error("could not set bit in deleted bitmap");
              assert(false);
            }
          }
          // do not have to roll back insertions as they will not be
          // in the commit bitmap
        break;

        case delete_marker:
          //row will be unlocked at trx delete
          //seek past the rowid
          fseek(log, sizeof(uint64_t), SEEK_CUR);
        break;
      }
    } 
    /* was dirty */
  }

  // need to commit the rolled back inserts to the delete bitmap
  if(all==ROLLBACK_STATEMENT) {
    if(warp_state->delete_bitmap->is_dirty()) {
      if(warp_state->delete_bitmap->commit() != 0) {
        sql_print_error("could not commit delete bitmap for rollback of statement");
        assert(false);
      }
    }
  
    // remove the savepoint data
    if(savepoint_at > 0) {
      fflush(log);
      ftruncate(fileno(log), savepoint_at);
      fsync(fileno(log));
    }
  } else {
    /* TRX rollback removes the trx from the commit list */

    commit_it->second = WARP_ROLLED_BACK_TRX;
    fclose(log);
    log = NULL;
    unlink(log_filename.c_str());
  } 

  commit_mtx.unlock();
}


warp_trx* warp_get_trx(handlerton* hton, THD* thd) {
  return (warp_trx*)thd->get_ha_data(hton->slot)->ha_ptr;
}

/* Commits a transaction to the WARP storage engine.  
   If the statement is an AUTOCOMMIT statement, then the 
   transaction is immediately committed.  If this is a
   multi-statement transaction, then the commit only
   happens when the commit_trx flag is true
   
   Only transactions that modified data need to be written
   to the commit log.  Read-only transaction don't need
   to do this work.
*/
int warp_commit(handlerton* hton, THD *thd, bool commit_trx) {
  auto current_trx = warp_get_trx(hton, thd);
  
  if(commit_trx || current_trx->autocommit) {
    if(current_trx->dirty) {
      current_trx->commit();
    }
  } else {
    /* this transaction is not ready to be committed to the
       storage engine because it is part of a multi-statement
       transaction
    */
    return 0;
  }
    
  /* if the transaction (autocommit or multi-statement) was 
     commited to disk, then the transaction information for
     the connection must be destroyed.
  */
  thd->get_ha_data(hton->slot)->ha_ptr = NULL;
  warp_state->free_locks(current_trx);
  warp_state->cleanup_history_locks();
  delete current_trx;
  return 0;
}

/*  rollback a transaction in the WARP storage engine
    -------------------------------------------------------------
    rollback_trx will be false if either the transaction is
    autocommit or if this is a single statement in a 
    multi-statement transaction.  If it is a single statement
    in multi-statement transaction, then only the changes in that 
    statement are rolled back.
*/
int warp_rollback(handlerton* hton, THD *thd, bool rollback_trx) {
  warp_trx* current_trx = warp_get_trx(hton, thd);
  //if a statement failed, we need to rollback the insertions
  //
  if(rollback_trx) {
    if(current_trx->dirty) {
     // undo the changes
     current_trx->rollback(true);
    }
  } else {
    //statement rollback
    if(current_trx->dirty) {  
        current_trx->rollback(ROLLBACK_STATEMENT);
    }
    if(current_trx->autocommit) {
      //warp_state->mark_transaction_closed(trx->trx_id);
      current_trx->dirty = false;
    } else {
      return 0;
    }
  }
  
  // destroy the transaction
  warp_state->free_locks(current_trx);
  warp_state->cleanup_history_locks();
  delete current_trx;
  thd->get_ha_data(hton->slot)->ha_ptr = NULL;
  return 0;
}

bool ha_warp::is_row_visible_to_read(uint64_t rowid) {

  uint64_t history_trx_id = warp_state->get_history_lock(rowid);
  
  auto current_trx = warp_get_trx(warp_hton, table->in_use);
  assert(current_trx != NULL);
  
  if(history_trx_id == 0 
    || history_trx_id < current_trx->trx_id 
    || (history_trx_id > current_trx->trx_id && (current_trx->isolation_level != ISO_REPEATABLE_READ && current_trx->isolation_level != ISO_SERIALIZABLE))) {
    // no history lock or may have been commited into delete map
    // in a visible trx so have to check to see if the row is deleted
    if(is_deleted(current_rowid)) {
      return false;
    }
  } else {
    /* another transaction has deleted or updated this row */
    if(history_trx_id != current_trx->trx_id) {
      return true;
    }
    return false;
  }
  return true;
}

// checks the transaction marker to see if if this
// row is visible
bool ha_warp::is_trx_visible_to_read(uint64_t row_trx_id) {
  if(last_trx_id == row_trx_id) {
    return is_trx_visible;
  }
  last_trx_id = row_trx_id;

  auto current_trx = warp_get_trx(warp_hton, table->in_use);
  assert(current_trx != NULL);
  auto commit_it = warp_state->commit_list.find(row_trx_id);
  
  //dbug("trx_id:" << current_trx->trx_id << " row_trx_id: " << row_trx_id);

  /* row belongs to current trx so it is visible */
  if(current_trx->trx_id == row_trx_id) {
    is_trx_visible = true;
    return is_trx_visible;
  }
  
  /* not on the commit list so it was rolled back or not recovered */
  if(commit_it == warp_state->commit_list.end()) {
    is_trx_visible = false;
    return is_trx_visible;
  }

  if(commit_it->second == WARP_ROLLED_BACK_TRX) {
    is_trx_visible = false;
    return is_trx_visible;
  }

  /* older trx are only visible if committed */
  if(row_trx_id < current_trx->trx_id) {
    if(commit_it->second == false) {
      is_trx_visible = false;
    } else {
      is_trx_visible = true;
    }
    return is_trx_visible;
  }

  // row_trx_id is newer and RR or SERIALIZABLE thus not visible due to isolation level
  if (current_trx->isolation_level == ISO_REPEATABLE_READ || current_trx->isolation_level == ISO_SERIALIZABLE) { 
    is_trx_visible = false;
    return is_trx_visible;
  }

  // if RC or RU if the trx is committed it is visible
  is_trx_visible = commit_it->second;
  return is_trx_visible;

}

/* Internal functions for maintaining and working with
   WARP tables
*/
int warp_upgrade_tables(uint16_t version) {
  if(version == 0) {
    ibis::partList parts;
    if(ibis::util::gatherParts(parts, ".") == 0) {
      // No tables so nothing to do!
      return 0;
    }

    for (auto it = parts.begin(); it < parts.end(); ++it) {
      auto part = *it;
      bool found_trx_column = false;
      auto colnames = part->columnNames();
      for(auto colit = colnames.begin(); colit < colnames.end(); ++colit) {
        if(std::string(*colit) == "t") {
          found_trx_column = true;
          break;
        }
      }
      if(!found_trx_column) {
        ibis::tablex* writer = ibis::tablex::create();
        const char* datadir = part->currentDataDir();
        const std::string metafile = std::string(datadir) + "/-part.txt";
        const std::string backup_metafile = std::string(datadir) + "/-part.txt.old";
        writer->readNamesAndTypes(metafile.c_str());
        writer->addColumn("t", ibis::ULONG, "transaction identifier");
        if(rename(metafile.c_str(), backup_metafile.c_str()) != 0) {
          sql_print_error("metadata rename failed %s -> %s", metafile.c_str(), backup_metafile.c_str());
          assert(false);
        }
        if(writer->writeMetaData(datadir) == (int)(part->columnNames().size() + 1)) {
          if(unlink(backup_metafile.c_str()) != 0) {
            sql_print_error("metadata write failed %s -> %s", metafile.c_str());
            assert(false);
          }
        } else {
          std::string logmsg = "Metadata write failed for metadata file %s";
          sql_print_error(logmsg.c_str(), metafile);
          assert(false);
        }
        std::string logmsg = "Upgraded WARP partition %s to include transaction identifiers";
        sql_print_error(logmsg.c_str(), datadir);
        writer->clearData();
        delete writer;
        std::string column_fname = std::string(datadir) + "/t";
        if(part->nRows() > 0) {
          FILE* cfp=fopen(column_fname.c_str(), "w");
          fseek(cfp, part->nRows()-1, SEEK_SET);
          uint64_t zero = 0;
          fwrite(&zero, 1, sizeof(zero), cfp);
          int ferrno = ferror(cfp);
          if(ferrno != 0) {
            sql_print_error("Failed to zerofill file ", column_fname.c_str());
            assert(false);
          }
          fclose(cfp);
        }
      }
    }
  } else {
    sql_print_error( 
      "On disk WARP version is greater than storage engine version. Engine version: %d but on disk version is %d", 
      WARP_VERSION,
      version
    );
    assert(false);
  }
  return 0;
}

warp_global_data::warp_global_data() {
  uint64_t on_disk_version = 0;
  bool shutdown_ok = false;
  assert(check_state() == true);

  fp = fopen(warp_state_file.c_str(), "rb+");
  //if fp == NULL then the database is being initialized for the first time
  if(fp == NULL) {
    sql_print_error("First time startup - initializing new WARP database.");
    next_rowid = 1;
    next_trx_id = 1;
    fp = fopen(warp_state_file.c_str(), "w+"); 
    if(fp == NULL) {
      sql_print_error("Could not open for writing: %s", warp_state_file.c_str());
      assert(false);
    }
    write();
    on_disk_version = WARP_VERSION;
    shutdown_ok = true;
  } else {
    on_disk_version = get_state_and_return_version();
    shutdown_ok = was_shutdown_clean();
  }  
  
  if(!shutdown_ok) {
    DIR *dir = opendir(".");
    struct dirent* ent;
    const char trxlog_file_extension[7] = ".txlog";
    if(dir == NULL) {
      sql_print_error("Could not open directory entry for data directory");
      assert(false);
    }
    // find any insertion logs and remove them - there is no need to roll
    // back the insertions, the transactions associated with them will
    // not be in the commit bitmap and any deletions associated with
    // those transactions will be rolled back automatically when the
    // bitmaps are opened
    while((ent = readdir(dir)) != NULL) {
      // skip the deleted and commit bitmaps
      if(ent->d_name[0] == 'c' || ent->d_name[0] == 'd') {
        char* ext_at = ent->d_name;
        int filename_len = strlen(ent->d_name);
        bool found = false;
        for(int i = 0; i< filename_len; ++i) {
          if(*(ext_at + i) == '.') {
            found = true;
            break;
          }
        }
        if(!found) {
            continue;
        }

        if(strncmp(trxlog_file_extension,ext_at, 6) == 0) {
          // found a txlog to remove
          if(unlink(ent->d_name) != 0) {
            sql_print_error("Could not remove transaction log %s", ent->d_name);
            assert(false);
          }
        }
      }
    }

    if(!repair_tables()) {
      assert("Table repair failed. Database could not be initialized");
    }
  } 
  
  // this file will be rewritten at clean shutdown
  unlink(shutdown_clean_file.c_str());
 
  commit_file = fopen(commit_filename.c_str(), "ab+");
   if(!commit_file) {  
     sql_print_error("Could not open commit file: %s", commit_filename.c_str());
    assert(false);
  }
 
  fseek(commit_file, 0, SEEK_SET);
  int sz = 0;
  uint64_t trx_id;

  /* load list of committed transactions to the commit list */
  while( (sz = fread(&trx_id, sizeof(trx_id), 1, commit_file)) == 1) {
     commit_list.emplace(std::pair<uint64_t, bool>(trx_id, true));
  }
  
  // this will create the commits.warp bitmap if it does not exist
  try {
     delete_bitmap = new sparsebitmap(delete_bitmap_file, LOCK_SH); 
  } catch(...) {
    sql_print_error("Could not open delete bitmap: %s", delete_bitmap_file.c_str());
    assert(false);
  }
  
  // if tables are an older version on disk, proceed with upgrade process
  if(on_disk_version != WARP_VERSION) {
     if(!warp_upgrade_tables(on_disk_version)) {
       sql_print_error("WARP upgrade tables failed");
      assert(false);
    }
    // will write new version information to disk
    // asserts if writing fails
    write();
  }
  // ALL OK - DATABASE IS OPEN AND INITIALIZED!
}

/* Check the state of the database. 
  1) the state file must exist
  2) the state file must be the correct size
  3) the commit bitmap must exist on disk
  4) 
  If all of these things are not correct then print an error message
  and crash the database, unless the state file does not exist AND
  the commit bitmap do not exist, which means this is the first time
  that WARP is being initialized UNLESS WARP tables already exist.
  If no WARP tables exist, this is the first WARP initialization
  (which means mysqld --initialize is running) and the files are 
  created.
*/
bool warp_global_data::check_state() {
  struct stat st;
  int state_exists = (stat(warp_state_file.c_str(), &st) == 0);
  int commit_file_exists = (stat(commit_filename.c_str(), &st) == 0);

  if((state_exists && !commit_file_exists)) {
    sql_print_error("warp_state found but commits.warp is missing! Database can not be initialized.");
    return false;
  } 
  
  if((!state_exists && commit_file_exists)) {
    sql_print_error("commits.warp is found but warp_state is missing! Database can not be initialized.");
    return false;
  } 
  
  auto parts = new ibis::partList;
  int has_warp_tables = ibis::util::gatherParts(*parts, std::string(".").c_str());
  if(!state_exists && !commit_file_exists && has_warp_tables > 0) {
    sql_print_error("WARP tables found but database state is missing! This may be a beta 1 database. WARP can not be initialized.");
    return false;
  }
  delete parts;
  return true;
}

uint64_t warp_global_data::get_next_trx_id() {
  mtx.lock();
  ++next_trx_id;
  write();
  mtx.unlock();
  return next_trx_id;
}

uint64_t warp_global_data::get_next_rowid_batch() {
  mtx.lock();
  next_rowid += WARP_ROWID_BATCH_SIZE;
  write();
  mtx.unlock();
  return next_rowid;
}

/* Only transactions that are for write are registered on the transaction list
   called in ::external_lock when a transaction first makes changes
*/
void warp_global_data::register_open_trx(uint64_t trx_id) {
  commit_mtx.lock();
  commit_list.emplace(std::pair<uint64_t, int>(trx_id, WARP_UNCOMMITTED_TRX));
  commit_mtx.unlock();
}

/* if trx not on commit list it is could be either rolled back or has not made any changes yet
   if trx is on commit list and is WARP_UNCOMMITTED_TRX then the transaction is open for writes
   if it is any other value then the transaction is not open anymore and has committed or
   rolled back - it will be removed when the history locks are cleaned up...
*/
bool warp_global_data::is_transaction_open(uint64_t trx_id) {
  bool retval = false;
  commit_mtx.lock();
  auto commit_it = commit_list.find(trx_id);
  if(commit_it == commit_list.end()) {
    retval = false;
  } else {
    retval = (commit_it->second == WARP_UNCOMMITTED_TRX);
  }
  commit_mtx.unlock();
  return retval;
}

int warp_global_data::create_lock(uint64_t rowid, warp_trx* trx, int lock_type) {
  uint spin_count = 0;
  struct timespec sleep_time;
  struct timespec remaining_time;
  sleep_time.tv_sec = (time_t)0;
  sleep_time.tv_nsec = 100000000L; // sleep a millisecond
  
  // each sleep beyond the spin locks increments the 
  // waiting time
  ulonglong max_wait_time = THDVAR(current_thd, lock_wait_timeout) * 100000000L;
  ulonglong wait_time = 0;
  // create a new lock for our lock
  // will be deleted and replaced if 
  // we discover we already have this 
  // lock!  
  warp_lock new_lock;
    
  // history locks are taken after EX_LOCKS are granted
  // for more information about history locks, see 
  // ha_warp::update_row comments
  if(lock_type == LOCK_HISTORY) {
    retry_hist_lock:
    history_lock_mtx.lock();
    if(history_lock_writing == 1) {
      history_lock_mtx.unlock();
      goto retry_hist_lock;
    }
    history_lock_writing=1;
    history_lock_mtx.unlock();
    history_locks.emplace(std::pair<uint64_t, uint64_t>(rowid, trx->trx_id));
    history_lock_mtx.lock();
    history_lock_writing=0;
    history_lock_mtx.unlock();
    return LOCK_HISTORY;
  }

  new_lock.holder = trx->trx_id;
  new_lock.waiting_on = 0;
  new_lock.lock_type = lock_type;
retry_lock:
  lock_mtx.lock();
  // this is used to iterate over all the locks held
  // for this row.  
  auto it = row_locks.find(rowid);

  // check to see if any row locks exist for this
  // row
  if(it == row_locks.end()) {
    // row is not locked so lock can proceed without
    // checking anything further!
    row_locks.emplace(std::pair<uint64_t, warp_lock>(rowid, new_lock));
    lock_mtx.unlock();
    return lock_type;
  } else {
    // row is already locked
    for(auto it2 = it;it2 != row_locks.end();++it2) {
      warp_lock test_lock = it2->second;

      // this lock will be released because of deadlock
      // so go to sleep and wait for it to be released
      // so that we don't possibly hit another deadlock
      // from the same trx before all the locks are 
      // released as the transaction closes
      if(test_lock.lock_type == LOCK_DEADLOCK) {
        lock_mtx.unlock();
        it2 = it;
        goto sleep;
      }

      // the current transaction already holds a lock on this row
      if(test_lock.holder != trx->trx_id) {
        
        if(test_lock.lock_type != LOCK_HISTORY && (test_lock.lock_type != LOCK_SH && lock_type != LOCK_SH)) {
          lock_mtx.unlock();
          goto sleep;
        }

      } else {
        if(test_lock.waiting_on != 0) {
          // does the waiting transaction still exist?
          if(is_transaction_open(test_lock.waiting_on)) {
            lock_mtx.unlock();
            goto sleep;
          }
          new_lock.waiting_on = 0;
          row_locks.erase(it);
          row_locks.emplace(std::pair<uint64_t, warp_lock>(rowid, new_lock));
          lock_mtx.unlock();
          return lock_type;
        }
       
        if(test_lock.lock_type == WRITE_INTENTION && lock_type == LOCK_EX && test_lock.holder == trx->trx_id) {
          // upgrade intention lock to EX_LOCK
          row_locks.erase(it2);
          row_locks.emplace(std::pair<uint64_t, warp_lock>(rowid, new_lock));
          lock_mtx.unlock();
          return lock_type;
        } else {
          // if LOCK_SH is requested and LOCK_EX has been granted return the EX_LOCK
          // this should generally never happen unless an update produced a unique
          // key violation and the row is being updated again.  If LOCK_SH is requested
          // and trx already had LOCK_SH then the existing lock is reused
          if(test_lock.lock_type >= lock_type && lock_type >= 0) {
            // this transaction already has a strong enough lock on this row
            // no need to insert the new lock and just return the lock 
            lock_mtx.unlock();
            return lock_type; 
          } 

          if(test_lock.lock_type == LOCK_SH && (lock_type == WRITE_INTENTION || lock_type == LOCK_EX)) {
            it2->second.lock_type = lock_type;
            lock_mtx.unlock();
            return lock_type;
          }
        }
        
        row_locks.erase(it);
        it2 = it;
        continue;
      }
      
      // this lock is a shared lock by somebody else
      // and this lock request is for a shared lock
      // so keep searching - we will grant the lock request
      // as long as no conflicting EX_LOCK is found
      // AND as long as this lock is not waiting on another
      // transaction
      if(test_lock.lock_type == LOCK_SH && new_lock.lock_type == LOCK_SH) {
        // if the existing  shared lock is not waiting on an EX lock
        // the shared lock can be granted, othewrise
        // we have to wait on this lock
        if(test_lock.waiting_on == 0) {
          // iterate because this trx might already hold a shared lock 
          // to reuse
          continue;
        }
        // the shared lock is waiting on an EX lock!
        // can not acquire the shared lock right now
        // will sleep a bit if spinlocks are exhausted and 
        // will error out if lock_wait_timeout is exhausted
        new_lock.waiting_on = test_lock.waiting_on;
        row_locks.emplace(std::pair<uint64_t, warp_lock>(rowid, new_lock));
        lock_mtx.unlock();
        goto sleep;
      }
      
      // If new_lock points to an existing lock and the 
      // other transaction is already waiting on this
      // lock, then a DEADLOCK is detected!
      // this transaction will be rolled back
      if((lock_type == LOCK_EX || lock_type == WRITE_INTENTION) && test_lock.waiting_on == new_lock.holder) {
        new_lock.lock_type = LOCK_DEADLOCK;
        row_locks.emplace(std::pair<uint64_t, warp_lock>(rowid, new_lock));
        lock_mtx.unlock();
        return LOCK_DEADLOCK;
      } else {
        // have to wait to upgrade the lock
        lock_mtx.unlock();
        goto sleep;
      }
    }
  }  

  new_lock.waiting_on = 0;
  // insert the new lock
  row_locks.emplace(std::pair<uint64_t, warp_lock>(rowid, new_lock));
  
  lock_mtx.unlock();
  return lock_type;

sleep:
  //lock_mtx.unlock();
  //fixme - make this configurable
  if(spin_count++ > 0) {
    
    int err = nanosleep(&sleep_time, &remaining_time);
    if(err < 0) {
      if(err == EINTR) {
        sleep_time=remaining_time;
        goto sleep;
      }
      return ER_LOCK_ABORTED;
     } 
  }
  wait_time += sleep_time.tv_nsec;
  if(wait_time >= max_wait_time) {
    return ER_LOCK_WAIT_TIMEOUT;
  }

  // lock sleep completed 
  goto retry_lock;

  // never reached
  return -1;
  
}

/* When the database shuts down clean it writes the
   warp_clean_shutdown file to disk
*/
bool warp_global_data::was_shutdown_clean() {
  struct stat st;
  if(stat(shutdown_clean_file.c_str(), &st) != 0) {
    return false;
  }
  return st.st_size == sizeof(uint8_t);
}

uint64_t warp_global_data::get_state_and_return_version() {
  struct on_disk_state state_record1;
  struct on_disk_state state_record2;
  struct on_disk_state* state_record;
  
  fread(&state_record1, sizeof(struct on_disk_state), 1, fp);
  if(ferror(fp) != 0) {
      sql_print_error("Failed to read state record one from warp_state");
    return 0;
  }
  fread(&state_record2, sizeof(struct on_disk_state), 1, fp);
  if(ferror(fp) != 0) {
    sql_print_error("Failed to read state record two from warp_state");
    return 0;
  }
  
  if(state_record2.state_counter == 0 && state_record1.state_counter == 0) {
    sql_print_error("Both state records are invalid.");
    return 0;
  }
  
  if(state_record2.state_counter == 0) {
    state_record = &state_record1;
  }
  
  if(state_record2.state_counter > state_record1.state_counter) {
    state_record = &state_record2;
  } else {
    state_record = &state_record1;
  }

  next_trx_id = state_record->next_trx_id;
  next_rowid = state_record->next_rowid;
  state_counter = state_record->state_counter;

  return state_record->version;
}

bool warp_global_data::repair_tables() {
  return true;
}

void warp_global_data::write_clean_shutdown() {
  FILE *sd = NULL;
  sd = fopen(shutdown_clean_file.c_str(), "w");
  if(!sd) {
    sql_print_error("could not open shutdown file");
    assert(false);
  }
  uint8_t one = 1;
  fwrite(&one, sizeof(uint8_t), 1, sd);
  if(ferror(sd) != 0) {
    sql_print_error("could not write shutdown file");
  }
  fflush(sd);
  fsync(fileno(sd));
  fclose(sd);
}

// the data is written to disk twice because if the database 
// or system crashes during the write, the state information
// would be corrupted!  
void warp_global_data::write() {
  struct on_disk_state record;
  // write the second record
  memset(&record, 0, sizeof(struct on_disk_state));
  // write the second record first.  If this fails, then the
  // old record will be used when the database restarts.
  if(fseek(fp, sizeof(struct on_disk_state), SEEK_SET) != 0) {
    sql_print_error("seek on warp_state failed!");
    assert(false);
  }
  fwrite(&record, sizeof(struct on_disk_state), 1, fp);
  if(ferror(fp) != 0) {
    sql_print_error("Write to database state failed");
    assert(false);
  }
  fflush(fp);
  if(fseek(fp, sizeof(struct on_disk_state), SEEK_SET) != 0) {
    sql_print_error("seek on warp_state failed!");
    assert(false);
  }
  record.next_rowid  = next_rowid;
  record.next_trx_id = next_trx_id;
  record.version     = WARP_VERSION;
  record.state_counter = ++state_counter;
  fwrite(&record, sizeof(struct on_disk_state), 1, fp);
  if(ferror(fp) != 0) {
    sql_print_error("Write to database state failed");
    assert(false);
  }
  fflush(fp);
  if(fsync(fileno(fp)) != 0) {
    sql_print_error("fsync to database state failed");
    assert(false);
  }

  // write the first record
  rewind(fp);   
  memset(&record, 0, sizeof(struct on_disk_state));
  fwrite(&record, sizeof(struct on_disk_state), 1, fp);
  if(ferror(fp) != 0) {
    sql_print_error("Write to database state failed");
    assert(false);
  }  
  fflush(fp);
  record.next_rowid  = next_rowid;
  record.next_trx_id = next_trx_id;
  record.version     = WARP_VERSION;
  record.state_counter = ++state_counter;
  fwrite(&record, sizeof(struct on_disk_state), 1, fp);
  if(ferror(fp) != 0) {
    sql_print_error("Write to database state failed");
    assert(false);
  }  
  fflush(fp);
  if(fsync(fileno(fp)) != 0) {
    sql_print_error("fsync to database state failed");
    assert(false);
  }
}

// not currently used - here for completeness
int warp_global_data::unlock(uint64_t rowid, warp_trx* trx) {
  lock_mtx.lock();
  // row is not locked!
  for(auto it = row_locks.find(rowid); it != row_locks.end();++it) {
    if(it->second.holder == trx->trx_id) {
      row_locks.erase(it);
      break;
    }
  }
  lock_mtx.unlock();
  return 0;

}
// an EX_LOCK can be downgraded to a history lock
// this function is here for completeness but it
// is not currently used as ::update_row and 
// ::delete_row automatically take history locks
int warp_global_data::downgrade_to_history_lock(uint64_t rowid, warp_trx* trx) {
  // row is not locked!
  lock_mtx.lock();
  for(auto it = row_locks.find(rowid); it != row_locks.end();++it) {
    if(it->second.holder == trx->trx_id) {
      row_locks.erase(it);
      break;
    }
  }
  lock_mtx.unlock();
  // any trx open at or before this transaction will see the 
  // history lock - no need to check the delete bitmap for
  // any row that has a history lock - it was deleted 
  // and is no longer visible to newer transactions
  // if a history lock doesn't exist the deleted bitmap
  // will be checked
  retry_lock:
  history_lock_mtx.lock();
  if(history_lock_writing == 1) {
    history_lock_mtx.unlock();
    goto retry_lock;
  }
  history_lock_writing=1;
  history_lock_mtx.unlock();
  history_locks.emplace(std::pair<uint64_t, uint64_t>(rowid, trx->trx_id));
  history_lock_mtx.lock();
  history_lock_writing=0;
  history_lock_mtx.unlock();

  

  
  return 0;
}

int warp_global_data::free_locks(warp_trx* trx) {
  lock_mtx.lock();
  restart:
  for(auto it = row_locks.begin(); it != row_locks.end();++it) {
    if(it->second.holder == trx->trx_id) {
      row_locks.erase(it);
      // deleting the lock invalidates the iterator...
      // so restart at the beginning.   There is probably
      // a better way to do this...
      goto restart;
    }
  }
  lock_mtx.unlock();
  return 0;
}

// returns 0 if no history lock or the trx_id that created
// the lock otherwise
uint64_t warp_global_data::get_history_lock(uint64_t rowid) {
  wait_for_writer:
  history_lock_mtx.lock();
  if(history_lock_writing == 1) {
    history_lock_mtx.unlock();
    goto wait_for_writer;
  }
  history_lock_mtx.unlock();
  auto it = history_locks.find(rowid);
  if(it == history_locks.end()) {
    return 0;
  }
  return it->second;
}

warp_global_data::~warp_global_data() {
  fclose(commit_file);
  /*
  if(commit_bitmap->close(1) != 0) {
    sql_print_error("Could not close bitmap %s", commit_bitmap->get_fname().c_str());
    assert(false);
  }*/
  if(delete_bitmap->close(1) != 0) {
    sql_print_error("Could not close bitmap %s", delete_bitmap->get_fname().c_str());
    assert(false);
  }
  delete delete_bitmap;
  delete_bitmap=NULL;
  write();
  fclose(fp);
  
  write_clean_shutdown();
}

std::string ha_warp::explain_extra() const { 
  if (pushed_cond != nullptr) {
    return ", with pushed condition: " + ItemToString(pushed_cond);
  }
  return "";
}

// get the number of rows in all the tables in the current schema
std::unordered_map<const char*, uint64_t> get_table_counts_in_schema(char* table_dir) {
  static std::unordered_map<const char*, uint64_t> table_counts;
  ibis::partList parts;
  if(!table_counts.empty()) return table_counts;

  char* schema_dir = strdup(table_dir);
  schema_dir = dirname(schema_dir);
  ibis::util::gatherParts(parts, schema_dir, true);
  for(auto part_it = parts.begin(); part_it < parts.end(); ++part_it) {
    ibis::part* part = *part_it;
    // the top-level partition ends in .data, the other partitions are ./data/pXXX
    if(strstr(part->currentDataDir(), ".data/") == NULL) {
      ibis::table* tbl = ibis::mensa::create(part->currentDataDir());
      if(!tbl) {
        table_counts.emplace(part->currentDataDir(), 0);
        continue;
      }
      table_counts.emplace(part->currentDataDir(), tbl->nRows());
      delete tbl;
    }
    
  }
  
  free(schema_dir);
  
  return table_counts;
}

// return the path to the table with the most rows in the database
const char* get_table_with_most_rows(std::unordered_map<const char*, uint64_t>* table_counts, std::unordered_map<std::string, bool> query_tables) {
  uint64_t max_cnt = 0;
  static const char* table_with_max_cnt = NULL;
  if(table_with_max_cnt != NULL) return table_with_max_cnt;

  for(auto it = table_counts->begin(); it != table_counts->end(); it++) {
    auto it2=query_tables.find(std::string(it->first));
    if(it2 == query_tables.end()) {
      continue;
    }
      
    if(it->second >= max_cnt) {
      max_cnt = it->second;
      table_with_max_cnt = it->first;
    }
  }
  return table_with_max_cnt;
}

// return the path to the table with the most rows in the database
uint64_t get_least_row_count(std::unordered_map<const char*, uint64_t>* table_counts) {
  static uint64_t min_cnt = -1ULL;
  if(min_cnt < -1ULL) {
    return min_cnt;
  }
  
  for(auto it = table_counts->begin(); it != table_counts->end(); it++) {
    if(it->second <= min_cnt) {
      min_cnt = it->second;
    }
  }
  return min_cnt;
}

uint64_t get_pushdown_info_count(THD* thd) {

  pushdown_mtx.lock();
  if(pd_info.empty()) { 
    pushdown_mtx.unlock();
    return 0;
  }

  auto it = pd_info.find(thd);
  if(it == pd_info.end()) {
    pushdown_mtx.unlock();
    return 0;
  }
  
  auto pushdown_info_map = it->second;
  if(pushdown_info_map == NULL) {
    pushdown_mtx.unlock();
    return 0;
  }
  
  uint64_t retval = pushdown_info_map->size();

  pushdown_mtx.unlock();

  return retval;

}

warp_pushdown_information* get_pushdown_info(THD* thd, const char* alias) {
  pushdown_mtx.lock();
  // The pushdown information will be missing if the referenced table
  // belongs to a different storage engine
  if(pd_info.empty()) { 
    pushdown_mtx.unlock();
    return NULL;
  }

  auto it = pd_info.find(thd);
  if(it == pd_info.end()) {
    pushdown_mtx.unlock();
    return NULL;
  }

  // If it was found above it is a WARP table and the map 
  // information should be set - if it is NULL then something is broken!
  auto pushdown_info_map = it->second;
  if(pushdown_info_map == NULL) {
    pushdown_mtx.unlock();
    return NULL;
  }
  
  if(pushdown_info_map->empty()) { 
    pushdown_mtx.unlock();
    return NULL; 
  }
  
  int is_empty = true;
  for(auto it2 = pushdown_info_map->begin();it2!=pushdown_info_map->end(); ++it2) {
    if(it2->first == std::string(alias)) {
      is_empty = false;
      break;
    }
  }
  if(is_empty) {
    pushdown_mtx.unlock();
    return NULL;
  }
  auto it2 = pushdown_info_map->find(alias);
  if(it2 == pushdown_info_map->end()) {
    pushdown_mtx.unlock();
    return NULL;
  }
  /*  
  if(it2->first == NULL) {
    pushdown_mtx.unlock();
    return NULL;
  }*/
  
  pushdown_mtx.unlock();
  // return the pushdown info
  return it2->second;
}

warp_pushdown_information* get_or_create_pushdown_info(THD* thd, const char* alias, const char* data_dir_name) {
  std::unordered_map<std::string, warp_pushdown_information*> *pushdown_info_map;
  warp_pushdown_information* pushdown_info;

  pushdown_mtx.lock();
   
  auto it = pd_info.find(thd);
  if(it == pd_info.end()) {
    pushdown_info_map = new std::unordered_map<std::string, warp_pushdown_information*>;
    pushdown_info = new warp_pushdown_information();
    //map the alias used by MySQL to the directory of the Fastbit table
    pushdown_info->datadir = data_dir_name;
    pushdown_info_map->emplace(std::pair<std::string, warp_pushdown_information*>(std::string(alias), pushdown_info));
    pd_info.emplace(std::pair<THD*, std::unordered_map<std::string, warp_pushdown_information*>*>(thd, pushdown_info_map));
  } else {
    pushdown_info_map = it->second;
    auto it2 = pushdown_info_map->find(alias);
    if(it2 == pushdown_info_map->end()) {
      pushdown_info = new warp_pushdown_information();
      pushdown_info->datadir = data_dir_name;
      pushdown_info_map->emplace(std::pair<std::string, warp_pushdown_information*>(std::string(alias), pushdown_info));
    } else {
      pushdown_info = it2->second;
    }
  }
  
  pushdown_mtx.unlock();
  
  return pushdown_info;
}
