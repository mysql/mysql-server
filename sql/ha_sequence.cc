/* Copyright (c) 2000, 2018, Alibaba and/or its affiliates. All rights reserved.

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

/**
  @file

  Sequence Engine handler interface and implementation.
*/

#include "my_systime.h"
#include "mysql/components/services/psi_mutex_bits.h"  //PSI_mutex_key
#include "mysql/components/services/psi_rwlock_bits.h"
#include "mysql/plugin.h"          //st_mysql_storage_engine
#include "mysql/psi/mysql_cond.h"  //mysql_mutex_init
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_mutex.h"  //mysql_mutex_init
#include "mysql/service_mysql_alloc.h"
#include "sql/handler.h"
#include "sql/mysqld.h"
#include "sql/psi_memory_key.h"
#include "sql/sequence_transaction.h"
#include "sql/sql_update.h"  //compare_record

#include "sql/ha_sequence.h"


/**
  @addtogroup Sequence Engine

  Implementation of Sequence Engine interface

  @{
*/

#define SEQUENCE_ENABLED_TABLE_FLAGS (HA_FILE_BASED)

#define SEQUENCE_DISABLED_TABLE_FLAGS \
  (HA_CAN_GEOMETRY | HA_CAN_FULLTEXT | HA_DUPLICATE_POS | HA_CAN_SQL_HANDLER)

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_sequence_share;
static PSI_mutex_key key_LOCK_sequence_open_shares_hash;
static PSI_cond_key key_COND_sequence_share;
static PSI_memory_key key_memory_sequence_share;

static PSI_mutex_info sequence_mutexes[] = {
    {&key_LOCK_sequence_share, "LOCK_sequence_share", 0, 0, PSI_DOCUMENT_ME},
    {&key_LOCK_sequence_open_shares_hash, "LOCK_sequence_hash", 0, 0,
     PSI_DOCUMENT_ME}};

static PSI_memory_info sequence_memory[] = {
    {&key_memory_sequence_share, "sequence_share", 0, 0, PSI_DOCUMENT_ME}};

static PSI_cond_info sequence_conds[] = {
    {&key_COND_sequence_share, "sequence_share", 0, 0, PSI_DOCUMENT_ME}};

static void init_sequence_psi_keys() {
  const char *category = "sql";
  int count;

  count = static_cast<int>(array_elements(sequence_mutexes));
  mysql_mutex_register(category, sequence_mutexes, count);

  count = static_cast<int>(array_elements(sequence_memory));
  mysql_memory_register(category, sequence_memory, count);

  count = static_cast<int>(array_elements(sequence_conds));
  mysql_cond_register(category, sequence_conds, count);
}
#endif /* HAVE_PSI_INTERFACE */

/* Global sequence engine handlerton */
handlerton *sequence_hton;


static const char sequence_plugin_author[] = "jianwei.zhao, Aliyun";
static const char sequence_plugin_name[] = "Sequence";

/* Protect sequence_open_shares map */
static mysql_mutex_t LOCK_sequence_open_shares_hash;
/* Sequence open shares map */
typedef collation_unordered_map<std::string, Sequence_share *>
    Sequence_shares_hash;
static Sequence_shares_hash *sequence_shares_hash;

/* Increment the sequence version */
static ulonglong sequence_global_version = 0;

static bool sequence_engine_inited = false;

static Sequence_share *get_share(const char *name)
{
  Sequence_share *share = NULL;
  Sequence_shares_hash::const_iterator it;
  DBUG_ENTER("get_share");

  /**
    We will hold the lock until the object creation, if the sequence_share
    didn't exist in the map, since the creation has only very low cost.

    Otherwise we should set CREATING flag to release the lock and
    load sequence value from table slowly.
  */
  mysql_mutex_lock(&LOCK_sequence_open_shares_hash);

  it = sequence_shares_hash->find(std::string(name));
  if (it != sequence_shares_hash->end()) {
    share = it->second;
  } else {
    share = new Sequence_share();
    share->init(name);
    share->m_version = sequence_global_version++;
    sequence_shares_hash->insert(
        std::pair<std::string, Sequence_share *>(std::string(name), share));
  }

  if (share) share->m_ref_count++;

  mysql_mutex_unlock(&LOCK_sequence_open_shares_hash);
  DBUG_RETURN(share);
}
/**
  Close the sequence share,
  make sure that sequence handler has been disassociated from it.

  @param[in]      share           Sequence share object

  @retval         void
*/
static void close_share(Sequence_share *share) {
  DBUG_ENTER("close_share");

  mysql_mutex_lock(&LOCK_sequence_open_shares_hash);
#ifndef DBUG_OFF
  Sequence_shares_hash::const_iterator it =
      sequence_shares_hash->find(std::string(share->m_name));
  DBUG_ASSERT(it != sequence_shares_hash->end() && it->second == share);
#endif
  DBUG_ASSERT(share->m_ref_count > 0);
  --share->m_ref_count;
  mysql_mutex_unlock(&LOCK_sequence_open_shares_hash);
  DBUG_VOID_RETURN;
}
/**
  Destroy the sequence_share object.

  @param[in]      name            Destroy the sequence_share object.

  @retval         void
*/
static void destroy_share(const char *name) {
  DBUG_ENTER("destroy_share");
  mysql_mutex_lock(&LOCK_sequence_open_shares_hash);

  Sequence_shares_hash::const_iterator it =
      sequence_shares_hash->find(std::string(name));

  if (it != sequence_shares_hash->end()) {
    delete it->second;
    sequence_shares_hash->erase(it);
  }
  mysql_mutex_unlock(&LOCK_sequence_open_shares_hash);
  DBUG_VOID_RETURN;
}

/**
  Init all the member variable.

  @param[in]      table_name      db_name + table_name

  @retval         void
*/
void Sequence_share::init(const char *table_name) {
  DBUG_ENTER("Sequence_share::init");
  mysql_mutex_init(key_LOCK_sequence_share, &m_mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_sequence_share, &m_cond);
  size_t length = strlen(table_name);
  m_name = my_strndup(key_memory_sequence_share, table_name, length,
                      MYF(MY_FAE | MY_ZEROFILL));

  bitmap_init(&m_read_set, NULL, SF_END, false);
  bitmap_init(&m_write_set, NULL, SF_END, false);
  bitmap_set_all(&m_read_set);
  bitmap_set_all(&m_write_set);

  m_cache_state = CACHE_STATE_INVALID;
  m_initialized = true;
  m_cache_end = 0;
  m_ref_count = 0;
  memset(m_caches, 0, sizeof(m_caches));
  DBUG_VOID_RETURN;
}

/**
  Get sequence share cache field value pointer

  @param[in]      field_num     The sequence field number

  @retval         field pointer
*/
ulonglong *Sequence_share::get_field_ptr(const Sequence_field field_num) {
  DBUG_ENTER("Sequence_share::get_field_ptr");
  DBUG_ASSERT(field_num < SF_END);
  DBUG_RETURN(&m_caches[field_num]);
}

/**
   Enter the wait condition until loading complete or error happened.
   @param[in]     thd           User connection

   @retval        0             Success
   @retval        ~0            Failure
*/
int Sequence_share::enter_cond(THD *thd) {
  int wait_result = 0;
  struct timespec abs_timeout;

  mysql_mutex_assert_owner(&m_mutex);
  set_timespec(&abs_timeout, thd->variables.lock_wait_timeout);

  while (m_cache_state == CACHE_STATE_LOADING && !thd->is_killed() &&
         !is_timeout(wait_result)) {
    wait_result = mysql_cond_timedwait(&m_cond, &m_mutex, &abs_timeout);
  }

  if (m_cache_state == CACHE_STATE_LOADING) {
    if (thd->is_killed()) {
      thd_set_kill_status(thd);  // set my_error
    } else if (is_timeout(wait_result)) {
      my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
    }
    return HA_ERR_SEQUENCE_ACCESS_FAILURE;
  }
  return 0;
}
/**
  Retrieve the nextval from cache directly.

  @param[out]     local_values    Used to store into thd->sequence_last_value

  @retval         request         Cache request result
*/
Sequence_cache_request Sequence_share::quick_read(ulonglong *local_values) {
  ulonglong *nextval_ptr;
  ulonglong *currval_ptr;
  ulonglong *increment_ptr;
  bool last_round;
  DBUG_ENTER("Sequence_share::quick_read");

  mysql_mutex_assert_owner(&m_mutex);
  DBUG_ASSERT(m_cache_state != CACHE_STATE_LOADING);

  nextval_ptr = &m_caches[SF_NEXTVAL];
  currval_ptr = &m_caches[SF_CURRVAL];
  increment_ptr = &m_caches[SF_INCREMENT];

  /* If cache is not valid, need load and flush cache. */
  if (m_cache_state == CACHE_STATE_INVALID)
    DBUG_RETURN(CACHE_REQUEST_NEED_LOAD);

  DBUG_ASSERT(m_cache_state == CACHE_STATE_VALID);

  /* If cache_end roll upon maxvalue, then it is last round */
  last_round = (m_caches[SF_MAXVALUE] == m_cache_end);

  if (!last_round && ulonglong(*nextval_ptr) >= m_cache_end) {
    DBUG_RETURN(CACHE_REQUEST_ROUND_OUT);
  } else if (last_round) {
    if (*nextval_ptr > m_cache_end) DBUG_RETURN(CACHE_REQUEST_ROUND_OUT);
  }

  /* Retrieve values from cache directly */
  {
    DBUG_ASSERT(*nextval_ptr <= m_cache_end);
    *currval_ptr = *nextval_ptr;
    memcpy(local_values, m_caches, sizeof(m_caches));
    if ((m_cache_end - *nextval_ptr) >= *increment_ptr)
      *nextval_ptr += *increment_ptr;
    else {
      *nextval_ptr = m_cache_end;
      invalidate();
    }
  }
  DBUG_RETURN(CACHE_REQUEST_HIT);
}

/**
  Reload the sequence value cache.

  @param[in]      table         TABLE object
  @param[out]     changed       Whether values are changed

  @retval         0             Success
  @retval         ~0            Failure
*/
int Sequence_share::reload_cache(TABLE *table, bool *changed) {
  st_sequence_field_info *field_info;
  Field **field;
  ulonglong durable[SF_END];
  Sequence_field field_num;
  DBUG_ENTER("Sequence_share::reload_cache");

  /* Read the durable values */
  for (field = table->field, field_info = seq_fields; *field;
       field++, field_info++) {
    field_num = field_info->field_num;
    durable[field_num] = (ulonglong)((*field)->val_int());
  }

  /* If someone update the table directly, need this check again. */
  if (check_sequence_values_valid(durable))
    DBUG_RETURN(HA_ERR_SEQUENCE_INVALID);

  /* Calculate the next round cache values */
  ulonglong begin;

  /* Step 1: overlap the cache using durable values */
  for (field_info = seq_fields; field_info->field_name; field_info++)
    m_caches[field_info->field_num] = durable[field_info->field_num];

  /* Step 2: decide the begin value */
  if (m_caches[SF_NEXTVAL] == 0) {
    if (m_caches[SF_ROUND] == 0)
      /* Take start value as the begining */
      begin = m_caches[SF_START];
    else
      /* Next round from minvalue */
      begin = m_caches[SF_MINVALUE];
  } else if (m_caches[SF_NEXTVAL] == m_caches[SF_MAXVALUE])
    /* Run out value when nocycle */
    DBUG_RETURN(HA_ERR_SEQUENCE_RUN_OUT);
  else
    begin = m_caches[SF_NEXTVAL];

  DBUG_ASSERT(begin <= m_caches[SF_MAXVALUE]);

  if (begin > m_caches[SF_MAXVALUE]) {
    DBUG_RETURN(HA_ERR_SEQUENCE_INVALID);
  }

  /* Step 3: calc the left counter to cache */
  longlong left = (m_caches[SF_MAXVALUE] - begin) / m_caches[SF_INCREMENT] - 1;

  /* The left counter is less than cache size */
  if (left < 0 || ((ulonglong)left) <= m_caches[SF_CACHE]) {
    /* If cycle, start again; else will report error! */
    m_cache_end = m_caches[SF_MAXVALUE];

    if (m_caches[SF_CYCLE] > 0) {
      durable[SF_NEXTVAL] = 0;
      durable[SF_ROUND]++;
    } else
      durable[SF_NEXTVAL] = m_caches[SF_MAXVALUE];
  } else {
    m_cache_end = begin + (m_caches[SF_CACHE] + 1) * m_caches[SF_INCREMENT];
    durable[SF_NEXTVAL] = m_cache_end;
    DBUG_ASSERT(m_cache_end < m_caches[SF_MAXVALUE]);
  }

  m_caches[SF_NEXTVAL] = begin;

  /* Step 4: Write back durable values */
  store_record(table, record[1]);
  for (field = table->field, field_info = seq_fields; *field;
       field++, field_info++) {
    (*field)->set_notnull();
    (*field)->store(durable[field_info->field_num], true);
  }
  *changed = compare_records(table);

#ifndef DBUG_OFF
  fprintf(stderr,
          "Sequence will write values: "
          "currval %llu "
          "nextval %llu "
          "minvalue %llu "
          "maxvalue %llu "
          "start %llu "
          "increment %llu "
          "cache %llu "
          "cycle %llu \n",
          durable[SF_CURRVAL],
          durable[SF_NEXTVAL],
          durable[SF_MINVALUE],
          durable[SF_MAXVALUE],
          durable[SF_START],
          durable[SF_INCREMENT],
          durable[SF_CACHE],
          durable[SF_CYCLE]);
#endif
  DBUG_RETURN(0);
}

/**
  Update the base table and flush the caches.

  @param[in]      table           Super TABLE object

  @retval         0               Success
  @retval         ~0              Failure
*/
int ha_sequence::ha_flush_cache(TABLE *) {
  int error = 0;
  bool changed;
  DBUG_ENTER("ha_sequence::ha_flush_cache");
  DBUG_ASSERT(m_file);

  Bitmap_helper helper(table, m_share);

  if ((error = m_file->ha_rnd_init(true))) goto err;

  if ((error = m_file->ha_rnd_next(table->record[0]))) goto err;

  if ((error = m_share->reload_cache(table, &changed))) goto err;

  if (!error && changed) {
    if ((error = m_file->ha_update_row(table->record[1], table->record[0])))
      goto err;
  }
err:
  m_file->ha_rnd_end();
  DBUG_RETURN(error);
}

/**
  Create sequence handler

  @param[in]      sequence_info         Sequence create info
  @param[in]      mem_root              thd->mem_root, handler is allocated from
                                        it.

  @retval         handler               Sequence engine handler object
*/
handler *get_ha_sequence(Sequence_info *sequence_info, MEM_ROOT *mem_root) {
  ha_sequence *ha;
  DBUG_ENTER("get_ha_sequence");
  if ((ha = new (mem_root) ha_sequence(sequence_hton, sequence_info))) {
    if (ha->initialize_sequence(mem_root)) {
      destroy(ha);
      ha = nullptr;
    } else
      ha->init();
  } else {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             static_cast<int>(sizeof(ha_sequence)));
  }
  DBUG_RETURN((handler *)ha);
}

/**
  Sequence base table engine setup.
*/
bool ha_sequence::setup_base_engine() {
  handlerton *hton;
  DBUG_ENTER("ha_sequence::setup_base_engine");
  DBUG_ASSERT((table_share && table_share->sequence_property->is_sequence()) ||
              !table_share);

  if (table_share) {
    hton = table_share->sequence_property->db_type();
    m_engine = ha_lock_engine(NULL, hton);
  } else {
    m_engine = ha_resolve_sequence_base(NULL);
  }
  if (!m_engine) goto err;

  DBUG_RETURN(FALSE);
err:
  clear_base_handler_file();
  DBUG_RETURN(TRUE);
}
/**
  Clear the locked sequence base table engine and destroy file handler
*/
void ha_sequence::clear_base_handler_file() {
  DBUG_ENTER("ha_sequence::clear_base_handler_file");
  if (m_engine) {
    plugin_unlock(NULL, m_engine);
    m_engine = NULL;
  }
  if (m_file) {
    destroy(m_file);
    m_file = NULL;
  }
  DBUG_VOID_RETURN;
}

/**
  Create the base table handler by m_engine.

  @param[in]      mem_root        Memory space

  @retval         false           Success
  @retval         true            Failure
*/
bool ha_sequence::setup_base_handler(MEM_ROOT *mem_root) {
  handlerton *hton;

  DBUG_ENTER("ha_sequence::setup_base_handler");
  DBUG_ASSERT(m_engine);

  hton = plugin_data<handlerton *>(m_engine);
  if (!(m_file = get_new_handler(table_share, false, mem_root, hton))) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             static_cast<int>(sizeof(handler)));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}
/**
  Setup the sequence base table engine and base file handler.

  @param[in]    name        Sequence table name
  @param[in]    mem_root    Memory space

  @retval       false       success
  @retval       true        failure
*/
bool ha_sequence::get_from_handler_file(const char *, MEM_ROOT *mem_root) {
  DBUG_ENTER("ha_sequence::get_from_handler_file");

  if (m_file) DBUG_RETURN(FALSE);

  if (setup_base_engine() || setup_base_handler(mem_root)) goto err;

  DBUG_RETURN(FALSE);
err:
  clear_base_handler_file();
  DBUG_RETURN(TRUE);
}

/**
  Init the sequence base table engine handler by sequence info

  @param[in]    mem_root    memory space

  @retval       false       success
  @retval       true        failure
*/
bool ha_sequence::new_handler_from_sequence_info(MEM_ROOT *mem_root) {
  DBUG_ENTER("ha_sequence::new_handler_from_sequence_info");
  DBUG_ASSERT(m_sequence_info);

  if (!(m_file = get_new_handler(table_share, false, mem_root,
                                 m_sequence_info->base_db_type))) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             static_cast<int>(sizeof(handler)));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}

/**
  Initialize sequence handler

  @param[in]    mem_root    memory space

  @retval       false       success
  @retval       true        failure
*/
bool ha_sequence::initialize_sequence(MEM_ROOT *mem_root) {
  DBUG_ENTER("ha_sequence::initialize_sequence");

  if (m_sequence_info) {
    if (new_handler_from_sequence_info(mem_root)) {
      DBUG_RETURN(TRUE);
    }
  } else if (get_from_handler_file(NULL, mem_root)) {
    DBUG_RETURN(TRUE);
  }

  DBUG_EXECUTE_IF("sequence_handler_error", {
    my_error(ER_SEQUENCE_ACCESS_FAILURE, MYF(0), NULL, NULL);
    DBUG_RETURN(TRUE);
  });

  DBUG_RETURN(FALSE);
}

/**
  Sequence handlerton create interface function.

  @param[in]    hton          sequence hton
  @param[in]    share         TABLE_SHARE object
  @param[in]    partitioned   whether base table is partition table
  @param[in]    mem_root      memory space

  @retval       handler       sequence handler
*/
static handler *sequence_create_handler(handlerton *hton, TABLE_SHARE *share,
                                        bool, MEM_ROOT *mem_root) {
  DBUG_ENTER("sequence_create_handler");
  ha_sequence *file = new (mem_root) ha_sequence(hton, share);
  if (file && file->initialize_sequence(mem_root)) {
    destroy(file);
    file = nullptr;
  }
  DBUG_RETURN(file);
}
/**
  Initialize the sequence handler member variable.
*/
void ha_sequence::init_variables() {
  DBUG_ENTER("ha_sequence::init_variables");
  m_file = NULL;
  m_engine = NULL;
  m_sequence_info = NULL;
  m_share = NULL;

  start_of_scan = 0;
  DBUG_VOID_RETURN;
}

ha_sequence::ha_sequence(handlerton *hton, TABLE_SHARE *share)
    : handler(hton, share) {
  init_variables();
}

/* Init handler when create sequence */
ha_sequence::ha_sequence(handlerton *hton, Sequence_info *info)
    : handler(hton, 0) {
  init_variables();
  m_sequence_info = info;
}

/**
  Unlock the base storage plugin and destroy the handler
*/
ha_sequence::~ha_sequence() {
  if (m_share) {
    close_share(m_share);
    m_share = NULL;
  }
  clear_base_handler_file();
}
/**
  Fill values into sequence table fields from iterated local_values

  @param[in]      thd             User connection
  @param[in]      table           TABLE object
  @param[in]      local_values    Temporoary iterated values

  @retval         false           Success
  @retval         true            Failure
*/
bool ha_sequence::fill_into_sequence_fields(THD *thd, TABLE *table,
                                            ulonglong *local_values) {
  Sequence_last_value *entry;
  st_sequence_field_info *field_info;
  Field **field;
  DBUG_ENTER("fill_sequence_fields");

  std::string key(table->s->table_cache_key.str,
                  table->s->table_cache_key.length);
  Sequence_last_value_hash::const_iterator it =
      thd->get_sequence_hash()->find(key);

  if (it != thd->get_sequence_hash()->end()) {
    entry = it->second;
  } else {
    entry = new Sequence_last_value();
    entry->set_version(m_share->m_version);
    thd->get_sequence_hash()->insert(
        std::pair<std::string, Sequence_last_value *>(key, entry));
  }

  Bitmap_helper bitmap_helper(table, m_share);

  for (field = table->field, field_info = seq_fields; *field;
       field++, field_info++) {
    DBUG_ASSERT(!memcmp(field_info->field_name, (*field)->field_name,
                        strlen(field_info->field_name)));

    ulonglong value = local_values[field_info->field_num];
    (*field)->set_notnull();
    (*field)->store(value, true);
    entry->m_values[field_info->field_num] = value;
  }
  DBUG_RETURN(false);
}

/**
  Fill values into sequence table fields from thd local Sequence_last_value.

  @param[in]      thd             User connection
  @param[in]      table           TABLE object

  @retval         false           Success
  @retval         true            Failure
*/
bool ha_sequence::fill_sequence_fields_from_thd(THD *thd, TABLE *table) {
  Sequence_last_value *entry;
  st_sequence_field_info *field_info;
  Field **field;
  DBUG_ENTER("fill_sequence_fields_from_thd");

  std::string key(table->s->table_cache_key.str,
                  table->s->table_cache_key.length);
  Sequence_last_value_hash::const_iterator it =
      thd->get_sequence_hash()->find(key);

  if (it != thd->get_sequence_hash()->end()) {
    entry = it->second;
    if (entry->get_version() != m_share->m_version) {
      thd->get_sequence_hash()->erase(it);
      DBUG_RETURN(true);
    }
  } else {
    DBUG_RETURN(true);
  }

  Bitmap_helper bitmap_helper(table, m_share);

  for (field = table->field, field_info = seq_fields; *field;
       field++, field_info++) {
    DBUG_ASSERT(!memcmp(field_info->field_name, (*field)->field_name,
                        strlen(field_info->field_name)));
    ulonglong value = entry->m_values[field_info->field_num];
    (*field)->set_notnull();
    (*field)->store(value, true);
  }

  DBUG_RETURN(false);
}

/**
  Sequence full table scan.

  @param[in]      scan
  @retval         ~0              error number
  @retval         0               success
*/
int ha_sequence::rnd_init(bool scan) {
  DBUG_ENTER("ha_sequence::rnd_init");
  DBUG_ASSERT(m_file);
  DBUG_ASSERT(m_share);
  DBUG_ASSERT(table_share && table);

  start_of_scan = 1;

  /* Inherit the sequence scan mode option. */
  m_scan_mode = table->sequence_scan.get();
  m_iter_mode = Sequence_iter_mode::IT_NON;

  if (m_scan_mode == Sequence_scan_mode::ITERATION_SCAN)
    m_iter_mode = sequence_iteration_type(table);

  DBUG_RETURN(m_file->ha_rnd_init(scan));
}

/**
  Sequence engine main logic.
  Embedded into the table scan process.

  Logics:
    1.Skip sequence cache to scan the based table record if
      a. update;
      b. select_from clause;

    2.Only scan the first row that controlled by
      variable 'start_of_scan'

    3.Lock strategy
      a. Only hold MDL_SHARED_READ if cache hit
      b. Hold MDL_SHARE_WRITE, GLOBAL READ LOCK when update, and COMMIT LOCK
          when autonomous transaction commit if cache miss

    4.Transaction
      a. begin a new autonomous transaction when updating base table.
*/
int ha_sequence::rnd_next(uchar *buf) {
  int error = 0;
  int retry_time = 2;
  Sequence_cache_request cache_request;
  ulonglong local_values[SF_END];
  DBUG_ENTER("ha_sequence::rnd_next");

  DBUG_ASSERT(m_file && m_share && ha_thd() && table_share && table);

  if (get_lock_type() == F_WRLCK ||
      m_scan_mode == Sequence_scan_mode::ORIGINAL_SCAN ||
      ha_thd()->variables.sequence_read_skip_cache) {
    DBUG_RETURN(m_file->ha_rnd_next(buf));
  }

  if (start_of_scan) {

    start_of_scan = 0;

    /**
      Get the currval from THD local sequence_last_value directly if only query
      currval.
    */
    if (m_iter_mode == Sequence_iter_mode::IT_NON_NEXTVAL) {
      if (fill_sequence_fields_from_thd(ha_thd(), table))
        DBUG_RETURN(HA_ERR_SEQUENCE_NOT_DEFINED);
      else
        DBUG_RETURN(0);
    }

    DBUG_ASSERT(m_iter_mode == Sequence_iter_mode::IT_NEXTVAL);

    Share_locker_helper share_locker(m_share);

  retry_once:
    retry_time--;
    /**
      Enter the condition:
       1. Wait if other thread is loading the cache.
       2. Report error if timeout.
       3. Return if thd->killed.
    */
    if ((error = m_share->enter_cond(ha_thd()))) {
      DBUG_RETURN(error);
    }
    cache_request = m_share->quick_read(local_values);
    switch (cache_request) {
      case Sequence_cache_request::CACHE_REQUEST_HIT:
        goto end;
      case Sequence_cache_request::CACHE_REQUEST_ERROR: {
        error = HA_ERR_SEQUENCE_ACCESS_FAILURE;
        break;
      }

      case Sequence_cache_request::CACHE_REQUEST_NEED_LOAD:
      case Sequence_cache_request::CACHE_REQUEST_ROUND_OUT: {
        if (retry_time > 0) {
          error = scroll_sequence(table, cache_request, &share_locker);
          share_locker.complete_load(error);
          if (error)
            break;
          else
            goto retry_once;
        } else {
          error = HA_ERR_SEQUENCE_RUN_OUT;
          break;
        }
      }
    } /* switch end */

    /* Here is the switch error result, if success, will goto end.  */
    m_share->invalidate();
    DBUG_RETURN(error);
  } else
    DBUG_RETURN(HA_ERR_END_OF_FILE); /* if (start_of_scan) end */

end:
  /* Fill the Sequence_last_value object.*/
  if (fill_into_sequence_fields(ha_thd(), table, local_values))
    DBUG_RETURN(HA_ERR_SEQUENCE_ACCESS_FAILURE);
  DBUG_RETURN(0);
}

int ha_sequence::rnd_end() {
  DBUG_ENTER("ha_sequence::rnd_end");
  DBUG_ASSERT(m_file && m_share);
  DBUG_ASSERT(table_share && table);
  DBUG_RETURN(m_file->ha_rnd_end());
}

int ha_sequence::rnd_pos(uchar *buf, uchar *pos) {
  DBUG_ENTER("ha_sequence::rnd_pos");
  DBUG_ASSERT(m_file);
  DBUG_RETURN(m_file->ha_rnd_pos(buf, pos));
}

void ha_sequence::position(const uchar *record) {
  DBUG_ENTER("ha_sequence::positioin");
  DBUG_ASSERT(m_file);
  m_file->position(record);
}

void ha_sequence::update_create_info(HA_CREATE_INFO *create_info) {
  if (m_file) m_file->update_create_info(create_info);
}

int ha_sequence::info(uint) {
  DBUG_ENTER("ha_sequence::info");
  DBUG_RETURN(false);
}

/**
  Add hidden columns and indexes to an InnoDB table definition.

  @param[in,out]	dd_table	      data dictionary cache object

  @retval         error number
  @retval         0               success
*/
int ha_sequence::get_extra_columns_and_keys(
    const HA_CREATE_INFO *create_info, const List<Create_field> *create_list,
    const KEY *key_info, uint key_count, dd::Table *dd_table) {
  DBUG_ENTER("ha_sequence::get_extra_columns_and_keys");
  DBUG_RETURN(m_file->get_extra_columns_and_keys(
      create_info, create_list, key_info, key_count, dd_table));
}

const char *ha_sequence::table_type() const {
  DBUG_ENTER("ha_sequence::table_type");
  DBUG_RETURN(sequence_plugin_name);
}

ulong ha_sequence::index_flags(uint inx, uint part, bool all_parts) const {
  DBUG_ENTER("ha_sequence::index_flags");
  DBUG_RETURN(m_file->index_flags(inx, part, all_parts));
}
/**
  Store lock
*/
THR_LOCK_DATA **ha_sequence::store_lock(THD *thd, THR_LOCK_DATA **to,
                                        enum thr_lock_type lock_type) {
  DBUG_ENTER("ha_sequence::store_lock");
  DBUG_RETURN(m_file->store_lock(thd, to, lock_type));
}
/**
  Open the sequence table, release the resource in ~ha_sequence if any error
  happened.

  @param[in]      name            Sequence table name.
  @param[in]      mode
  @param[in]      test_if_locked
  @param[in]      table_def       DD table definition


  @retval         0               Success
  @retval         ~0              Failure
*/
int ha_sequence::open(const char *name, int mode, uint test_if_locked,
                      const dd::Table *table_def) {
  int error;
  DBUG_ENTER("ha_sequence::open");
  DBUG_ASSERT(table->s == table_share);
  error = HA_ERR_INITIALIZATION;

  if (!(m_share = get_share(name))) DBUG_RETURN(error);

  if (get_from_handler_file(name, &table->mem_root)) DBUG_RETURN(error);

  DBUG_ASSERT(m_engine && m_file);

  DBUG_RETURN(
      (error = m_file->ha_open(table, name, mode, test_if_locked, table_def)));
}

/**
  Close sequence handler.
  We didn't destroy share although the ref_count == 0,
  the cached values will be lost if we do that.

  @retval         0               Success
  @retval         ~0              Failure
*/
int ha_sequence::close(void) {
  DBUG_ENTER("ha_sequence::close");
  close_share(m_share);
  m_share = NULL;
  DBUG_RETURN(m_file->ha_close());
}

ulonglong ha_sequence::table_flags() const {
  DBUG_ENTER("ha_sequence::table_flags");
  if (!m_file) {
    DBUG_RETURN(SEQUENCE_ENABLED_TABLE_FLAGS);
  }
  DBUG_RETURN(m_file->ha_table_flags() &
              ~(HA_STATS_RECORDS_IS_EXACT | HA_REQUIRE_PRIMARY_KEY));
}

/**
  Create sequence table.

  @param[in]      name            Sequence table name.
  @param[in]      form            TABLE object
  @param[in]      create_info     create options
  @param[in]      table_def       dd::Table object that has been created

  @retval         0               success
  @retval         ~0              failure
*/
int ha_sequence::create(const char *name, TABLE *form,
                        HA_CREATE_INFO *create_info, dd::Table *table_def) {
  int error;
  DBUG_ENTER("ha_sequence::create");

  if (get_from_handler_file(name, ha_thd()->mem_root)) DBUG_RETURN(TRUE);

  DBUG_ASSERT(m_engine && m_file);
  if ((error = m_file->ha_create(name, form, create_info, table_def))) goto err;

  DBUG_RETURN(false);

err:
  m_file->ha_delete_table(name, table_def);

  /* Delete the special file for sequence engine. */
  handler::delete_table(name, table_def);
  DBUG_RETURN(error);
}

static const char *ha_sequence_ext[] = {NullS};

/**
  Sequence engine special file extension

  @retval     String array      File extension array
*/
const char **ha_sequence::bas_ext() const {
  DBUG_ENTER("ha_sequence::bas_ext");
  DBUG_RETURN(ha_sequence_ext);
}

/**
  Drop sequence table object

  @param[in]    name        Sequence table name
  @param[in]    table_def   Table DD object

  @retval       0           Success
  @retval       ~0          Failure
*/
int ha_sequence::delete_table(const char *name, const dd::Table *table_def) {
  DBUG_ENTER("ha_sequence::delete_table");

  if (get_from_handler_file(name, ha_thd()->mem_root)) DBUG_RETURN(TRUE);

  destroy_share(name);
  DBUG_RETURN(m_file->ha_delete_table(name, table_def));
}

/**
  Write sequence row.

  @param[in]      buf       table->record

  @retval         0         Success
  @retval         ~0        Failure
*/
int ha_sequence::write_row(uchar *buf) {
  int error;
  DBUG_ENTER("ha_sequence::write_row");
  DBUG_ASSERT(m_file && m_share);

  Share_locker_helper share_locker(m_share);
  Disable_binlog_helper disable_binlog(ha_thd());
  if ((error = m_share->enter_cond(ha_thd()))) DBUG_RETURN(error);
  m_share->invalidate();
  error = m_file->ha_write_row(buf);

  DBUG_EXECUTE_IF("sequence_write_error",
                  { error = HA_ERR_SEQUENCE_ACCESS_FAILURE; });

  DBUG_RETURN(error);
}

int ha_sequence::update_row(const uchar *old_data, uchar *new_data) {
  int error;
  DBUG_ENTER("ha_sequence::update_row");
  DBUG_ASSERT(m_file && m_share);

  /* Binlog will decided by m_file engine. so disable here */
  Share_locker_helper share_locker(m_share);
  Disable_binlog_helper disable_binlog(ha_thd());
  if ((error = m_share->enter_cond(ha_thd()))) DBUG_RETURN(error);
  m_share->invalidate();
  DBUG_RETURN(m_file->ha_update_row(old_data, new_data));
}

int ha_sequence::delete_row(const uchar *buf) {
  int error;
  DBUG_ENTER("ha_sequence::update_row");
  DBUG_ASSERT(m_file && m_share);

  /* Binlog will decided by m_file engine. so disable here */
  Share_locker_helper share_locker(m_share);
  Disable_binlog_helper disable_binlog(ha_thd());
  if ((error = m_share->enter_cond(ha_thd()))) DBUG_RETURN(error);
  m_share->invalidate();
  DBUG_RETURN(m_file->ha_delete_row(buf));
}

/**
  External lock

  @param[in]      thd         User connection
  @param[in]      lock_typ    Lock type

  @retval         0         Success
  @retval         ~0        Failure
*/
int ha_sequence::external_lock(THD *thd, int lock_type) {
  DBUG_ENTER("ha_sequence::external_lock");
  DBUG_ASSERT(m_file);
  DBUG_RETURN(m_file->ha_external_lock(thd, lock_type));
}

/**
  Scrolling the sequence cache by update the base table through autonomous
  transaction.

  @param[in]      table       TABLE object
  @param[in]      state       Sequence cache state
  @param[in]      helper      Sequence share locker

  @retval         0         Success
  @retval         ~0        Failure
*/
int ha_sequence::scroll_sequence(TABLE *table,
                                 Sequence_cache_request cache_request,
                                 Share_locker_helper *helper) {
  DBUG_ENTER("ha_sequence::scroll_sequence");
  DBUG_ASSERT(cache_request ==
                  Sequence_cache_request::CACHE_REQUEST_NEED_LOAD ||
              cache_request == Sequence_cache_request::CACHE_REQUEST_ROUND_OUT);
  DBUG_ASSERT(m_share->m_cache_state !=
              Sequence_cache_state::CACHE_STATE_LOADING);
  helper->loading();

  /* Sequence transaction do the reload */
  Reload_sequence_cache_ctx ctx(ha_thd(), table_share);
  DBUG_RETURN(ctx.reload_sequence_cache(table));
}

/**
  Rename sequence table name.

  @param[in]      from            Old name of sequence table
  @param[in]      to              New name of sequence table
  @param[in]      from_table_def  Old dd::Table object
  @param[in/out]  to_table_def    New dd::Table object

  @retval         0               Success
  @retval         ~0              Failure
*/
int ha_sequence::rename_table(const char *from, const char *to,
                              const dd::Table *from_table_def,
                              dd::Table *to_table_def) {
  DBUG_ENTER("ha_sequence::rename_table");
  if (get_from_handler_file(from, ha_thd()->mem_root)) DBUG_RETURN(TRUE);

  destroy_share(from);
  DBUG_RETURN(m_file->ha_rename_table(from, to, from_table_def, to_table_def));
}

/**
  Construtor of Bitmap_helper, backup current read/write bitmap set.
*/
ha_sequence::Bitmap_helper::Bitmap_helper(TABLE *table, Sequence_share *share)
    : m_table(table) {
  save_read_set = table->read_set;
  save_write_set = table->write_set;
  table->read_set = &(share->m_read_set);
  table->write_set = &(share->m_write_set);
}

/**
  Destrutor of Bitmap_helper, restore the read/write bitmap set.
*/
ha_sequence::Bitmap_helper::~Bitmap_helper() {
  m_table->read_set = save_read_set;
  m_table->write_set = save_write_set;
}

/**
  Report sequence error.
*/
void ha_sequence::print_error(int error, myf errflag) {
  THD *thd = ha_thd();
  char *sequence_db = (char *)"???";
  char *sequence_name = (char *)"???";
  DBUG_ENTER("ha_sequence::print_error");

  if (table_share) {
    sequence_db = table_share->db.str;
    sequence_name = table_share->table_name.str;
  }
  switch (error) {
    case HA_ERR_SEQUENCE_INVALID: {
      my_error(ER_SEQUENCE_INVALID, MYF(0), sequence_db, sequence_name);
      DBUG_VOID_RETURN;
    }
    case HA_ERR_SEQUENCE_RUN_OUT: {
      my_error(ER_SEQUENCE_RUN_OUT, MYF(0), sequence_db, sequence_name);
      DBUG_VOID_RETURN;
    }
    case HA_ERR_SEQUENCE_NOT_DEFINED: {
      my_error(ER_SEQUENCE_NOT_DEFINED, MYF(0), sequence_db, sequence_name);
      DBUG_VOID_RETURN;
    }
    /*
      We has reported error using my_error, so this unkown error
      is used to prevent from repeating error definition
     */
    case HA_ERR_SEQUENCE_ACCESS_FAILURE: {
      if (thd->is_error()) DBUG_VOID_RETURN;

      my_error(ER_SEQUENCE_ACCESS_FAILURE, MYF(0), sequence_db, sequence_name);
      DBUG_VOID_RETURN;
    }
  }
  if (m_file)
    m_file->print_error(error, errflag);
  else
    handler::print_error(error, errflag);

  DBUG_VOID_RETURN;
}

void ha_sequence::unbind_psi() {
  DBUG_ENTER("ha_sequence::unbind_psi");
  handler::unbind_psi();

  DBUG_ASSERT(m_file != NULL);
  m_file->unbind_psi();
  DBUG_VOID_RETURN;
}

void ha_sequence::rebind_psi() {
  DBUG_ENTER("ha_sequence::rebind_psi");
  handler::rebind_psi();

  DBUG_ASSERT(m_file != NULL);
  m_file->rebind_psi();
  DBUG_VOID_RETURN;
}

/**
  Sequence engine end.

  @param[in]    p         engine handlerton
  @param[in]    type      panic type

  @retval       0         success
  @retval       >0        failure
*/
static int sequence_end(handlerton *,
                        ha_panic_function type __attribute__((unused))) {
  DBUG_ENTER("sequence_end");
  if (sequence_engine_inited) {
    destroy_hash<std::string, Sequence_share *>(sequence_shares_hash);
    sequence_shares_hash = NULL;
    mysql_mutex_destroy(&LOCK_sequence_open_shares_hash);
  }
  sequence_engine_inited = false;
  DBUG_RETURN(0);
}

/**
  Sequence support the atomic ddl by base engine.

  @param[in]    thd       User connection
*/
static void sequence_post_ddl(THD *thd) {
  handlerton *hton;
  plugin_ref plugin;
  DBUG_ENTER("sequence_post_ddl");
  if ((plugin = ha_resolve_sequence_base(NULL)) &&
      (hton = plugin_data<handlerton *>(plugin))) {
    hton->post_ddl(thd);
  }
  if (plugin) plugin_unlock(NULL, plugin);
  DBUG_VOID_RETURN;
}
/**
  Sequence engine init function.

  @param[in]    p         engine handlerton

  @retval       0         success
  @retval       >0        failure
*/
static int sequence_initialize(void *p) {
  handlerton *sequence_hton;
  DBUG_ENTER("sequence_initialize");

#ifdef HAVE_PSI_INTERFACE
  init_sequence_psi_keys();
#endif

  sequence_hton = (handlerton *)p;
  // TODO: functions

  sequence_hton->panic = sequence_end;
  sequence_hton->db_type = DB_TYPE_SEQUENCE_DB;
  sequence_hton->create = sequence_create_handler;
  sequence_hton->post_ddl = sequence_post_ddl;
  sequence_hton->flags = HTON_SUPPORTS_ATOMIC_DDL;
  mysql_mutex_init(key_LOCK_sequence_open_shares_hash,
                   &LOCK_sequence_open_shares_hash, MY_MUTEX_INIT_FAST);
  sequence_shares_hash =
      new Sequence_shares_hash(system_charset_info, key_memory_sequence_share);

  sequence_engine_inited = true;
  DBUG_RETURN(0);
}

/** Sequence storage engine declaration */
static struct st_mysql_storage_engine sequence_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

mysql_declare_plugin(sequence) {
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &sequence_storage_engine,
  sequence_plugin_name,
  sequence_plugin_author,
  "Sequence Storage Engine Helper",
  PLUGIN_LICENSE_GPL,
  sequence_initialize, /* Plugin Init */
  NULL,                                    /* Plugin Check uninstall */
  NULL,                                    /* Plugin Deinit */
  0x0100,                                  /* 1.0 */
  NULL,                                    /* status variables */
  NULL,                                    /* system variables */
  NULL,                                    /* config options */
  0,                                       /* flags */
}
mysql_declare_plugin_end;


/// @} (end of group Sequence Engine)
