/* Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_server.cc
  Private interface for the server (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "mysys_err.h"
#include "pfs_server.h"
#include "pfs.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_builtin_memory.h"
#include "pfs_instr.h"
#include "pfs_events_waits.h"
#include "pfs_events_stages.h"
#include "pfs_events_statements.h"
#include "pfs_events_transactions.h"
#include "pfs_timer.h"
#include "pfs_setup_actor.h"
#include "pfs_setup_object.h"
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_account.h"
#include "pfs_defaults.h"
#include "pfs_digest.h"
#include "pfs_program.h"
#include "template_utils.h"
#include "pfs_prepared_stmt.h"

PFS_global_param pfs_param;

PFS_table_stat PFS_table_stat::g_reset_template;

C_MODE_START
static void destroy_pfs_thread(void *key);
C_MODE_END

static void cleanup_performance_schema(void);
void cleanup_instrument_config(void);

void pre_initialize_performance_schema()
{
  pfs_initialized= false;

  init_all_builtin_memory_class();

  PFS_table_stat::g_reset_template.reset();
  global_idle_stat.reset();
  global_table_io_stat.reset();
  global_table_lock_stat.reset();

  if (my_create_thread_local_key(&THR_PFS, destroy_pfs_thread))
    return;
  if (my_create_thread_local_key(&THR_PFS_VG, NULL))  // global_variables
    return;
  if (my_create_thread_local_key(&THR_PFS_SV, NULL))  // session_variables
    return;
  if (my_create_thread_local_key(&THR_PFS_VBT, NULL)) // variables_by_thread
    return;
  if (my_create_thread_local_key(&THR_PFS_SG, NULL))  // global_status
    return;
  if (my_create_thread_local_key(&THR_PFS_SS, NULL))  // session_status
    return;
  if (my_create_thread_local_key(&THR_PFS_SBT, NULL)) // status_by_thread
    return;
  if (my_create_thread_local_key(&THR_PFS_SBU, NULL)) // status_by_user
    return;
  if (my_create_thread_local_key(&THR_PFS_SBH, NULL)) // status_by_host
    return;
  if (my_create_thread_local_key(&THR_PFS_SBA, NULL)) // status_by_account
    return;

  THR_PFS_initialized= true;
}

struct PSI_bootstrap*
initialize_performance_schema(PFS_global_param *param)
{
  if (!THR_PFS_initialized)
  {
    /* Pre-initialization failed. */
    return NULL;
  }

  pfs_enabled= param->m_enabled;

  pfs_automated_sizing(param);
  init_timers();
  init_event_name_sizing(param);
  register_global_classes();

  if (init_sync_class(param->m_mutex_class_sizing,
                      param->m_rwlock_class_sizing,
                      param->m_cond_class_sizing) ||
      init_thread_class(param->m_thread_class_sizing) ||
      init_table_share(param->m_table_share_sizing) ||
      init_table_share_lock_stat(param->m_table_lock_stat_sizing) ||
      init_table_share_index_stat(param->m_index_stat_sizing) ||
      init_file_class(param->m_file_class_sizing) ||
      init_stage_class(param->m_stage_class_sizing) ||
      init_statement_class(param->m_statement_class_sizing) ||
      init_socket_class(param->m_socket_class_sizing) ||
      init_memory_class(param->m_memory_class_sizing) ||
      init_instruments(param) ||
      init_events_waits_history_long(
        param->m_events_waits_history_long_sizing) ||
      init_events_stages_history_long(
        param->m_events_stages_history_long_sizing) ||
      init_events_statements_history_long(
        param->m_events_statements_history_long_sizing) ||
      init_events_transactions_history_long(
        param->m_events_transactions_history_long_sizing) ||
      init_file_hash(param) ||
      init_table_share_hash(param) ||
      init_setup_actor(param) ||
      init_setup_actor_hash(param) ||
      init_setup_object(param) ||
      init_setup_object_hash(param) ||
      init_host(param) ||
      init_host_hash(param) ||
      init_user(param) ||
      init_user_hash(param) ||
      init_account(param) ||
      init_account_hash(param) ||
      init_digest(param) ||
      init_digest_hash(param) ||
      init_program(param) ||
      init_program_hash(param) ||
      init_prepared_stmt(param))
  {
    /*
      The performance schema initialization failed.
      Free the memory used, and disable the instrumentation.
    */
    cleanup_performance_schema();
    return NULL;
  }

  if (param->m_enabled)
  {
    /** Default values for SETUP_CONSUMERS */
    flag_events_stages_current=            param->m_consumer_events_stages_current_enabled;
    flag_events_stages_history=            param->m_consumer_events_stages_history_enabled;
    flag_events_stages_history_long=       param->m_consumer_events_stages_history_long_enabled;
    flag_events_statements_current=        param->m_consumer_events_statements_current_enabled;
    flag_events_statements_history=        param->m_consumer_events_statements_history_enabled;
    flag_events_statements_history_long=   param->m_consumer_events_statements_history_long_enabled;
    flag_events_transactions_current=      param->m_consumer_events_transactions_current_enabled;
    flag_events_transactions_history=      param->m_consumer_events_transactions_history_enabled;
    flag_events_transactions_history_long= param->m_consumer_events_transactions_history_long_enabled;
    flag_events_waits_current=             param->m_consumer_events_waits_current_enabled;
    flag_events_waits_history=             param->m_consumer_events_waits_history_enabled;
    flag_events_waits_history_long=        param->m_consumer_events_waits_history_long_enabled;
    flag_global_instrumentation=           param->m_consumer_global_instrumentation_enabled;
    flag_thread_instrumentation=           param->m_consumer_thread_instrumentation_enabled;
    flag_statements_digest=                param->m_consumer_statement_digest_enabled;
  }
  else
  {
    flag_events_stages_current= false;
    flag_events_stages_history= false;
    flag_events_stages_history_long= false;
    flag_events_statements_current= false;
    flag_events_statements_history= false;
    flag_events_statements_history_long= false;
    flag_events_transactions_current= false;
    flag_events_transactions_history= false;
    flag_events_transactions_history_long= false;
    flag_events_waits_current= false;
    flag_events_waits_history= false;
    flag_events_waits_history_long= false;
    flag_global_instrumentation= false;
    flag_thread_instrumentation= false;
    flag_statements_digest= false;
  }

  pfs_initialized= true;

  if (param->m_enabled)
  {
    install_default_setup(&PFS_bootstrap);
    return &PFS_bootstrap;
  }

  return NULL;
}

static void destroy_pfs_thread(void *key)
{
  PFS_thread* pfs= reinterpret_cast<PFS_thread*> (key);
  DBUG_ASSERT(pfs);
  /*
    This automatic cleanup is a last resort and best effort to avoid leaks,
    and may not work on windows due to the implementation of pthread_key_create().
    Please either use:
    - my_thread_end()
    - or PSI_server->delete_current_thread()
    in the instrumented code, to explicitly cleanup the instrumentation.

    Avoid invalid writes when the main() thread completes after shutdown:
    the memory pointed by pfs is already released.
  */
  if (pfs_initialized)
    destroy_thread(pfs);
}

static void cleanup_performance_schema(void)
{
  /*
    my.cnf options
  */

  cleanup_instrument_config();

  /*
    All the LF_HASH
  */

  cleanup_setup_actor_hash();
  cleanup_setup_object_hash();
  cleanup_account_hash();
  cleanup_host_hash();
  cleanup_user_hash();
  cleanup_program_hash();
  cleanup_table_share_hash();
  cleanup_file_hash();
  cleanup_digest_hash();

  /*
    Then the lookup tables
  */

  cleanup_setup_actor();
  cleanup_setup_object();

  /*
    Then the history tables
  */

  cleanup_events_waits_history_long();
  cleanup_events_stages_history_long();
  cleanup_events_statements_history_long();
  cleanup_events_transactions_history_long();

  /*
    Then the various aggregations
  */

  cleanup_digest();
  cleanup_account();
  cleanup_host();
  cleanup_user();

  /*
    Then the instrument classes.
    Once a class is cleaned up,
    find_XXX_class(key)
    will return PSI_NOT_INSTRUMENTED
  */
  cleanup_program();
  cleanup_prepared_stmt();
  cleanup_sync_class();
  cleanup_thread_class();
  cleanup_table_share();
  cleanup_table_share_lock_stat();
  cleanup_table_share_index_stat();
  cleanup_file_class();
  cleanup_stage_class();
  cleanup_statement_class();
  cleanup_socket_class();
  cleanup_memory_class();

  cleanup_instruments();
}

void shutdown_performance_schema(void)
{
  pfs_initialized= false;

  /* disable everything, especially for this thread. */
  flag_events_stages_current= false;
  flag_events_stages_history= false;
  flag_events_stages_history_long= false;
  flag_events_statements_current= false;
  flag_events_statements_history= false;
  flag_events_statements_history_long= false;
  flag_events_transactions_current= false;
  flag_events_transactions_history= false;
  flag_events_transactions_history_long= false;
  flag_events_waits_current= false;
  flag_events_waits_history= false;
  flag_events_waits_history_long= false;
  flag_global_instrumentation= false;
  flag_thread_instrumentation= false;
  flag_statements_digest= false;

  global_table_io_class.m_enabled= false;
  global_table_lock_class.m_enabled= false;
  global_idle_class.m_enabled= false;
  global_metadata_class.m_enabled= false;
  global_transaction_class.m_enabled= false;

  cleanup_performance_schema();
  /*
    Be careful to not delete un-initialized keys,
    this would affect key 0, which is THR_KEY_mysys,
  */
  if (THR_PFS_initialized)
  {
    my_set_thread_local(THR_PFS, NULL);
    my_set_thread_local(THR_PFS_VG, NULL);  // global_variables
    my_set_thread_local(THR_PFS_SV, NULL);  // session_variables
    my_set_thread_local(THR_PFS_VBT, NULL); // variables_by_thread
    my_set_thread_local(THR_PFS_SG, NULL);  // global_status
    my_set_thread_local(THR_PFS_SS, NULL);  // session_status
    my_set_thread_local(THR_PFS_SBT, NULL); // status_by_thread
    my_set_thread_local(THR_PFS_SBU, NULL); // status_by_user
    my_set_thread_local(THR_PFS_SBH, NULL); // status_by_host
    my_set_thread_local(THR_PFS_SBA, NULL); // status_by_account

    my_delete_thread_local_key(THR_PFS);
    my_delete_thread_local_key(THR_PFS_VG);
    my_delete_thread_local_key(THR_PFS_SV);
    my_delete_thread_local_key(THR_PFS_VBT);
    my_delete_thread_local_key(THR_PFS_SG);
    my_delete_thread_local_key(THR_PFS_SS);
    my_delete_thread_local_key(THR_PFS_SBT);
    my_delete_thread_local_key(THR_PFS_SBU);
    my_delete_thread_local_key(THR_PFS_SBH);
    my_delete_thread_local_key(THR_PFS_SBA);

    THR_PFS_initialized= false;
  }
}

/**
  Initialize the dynamic array used to hold PFS_INSTRUMENT configuration
  options.
*/
void init_pfs_instrument_array()
{
  pfs_instr_config_array= new Pfs_instr_config_array(PSI_NOT_INSTRUMENTED);
}

/**
  Deallocate the PFS_INSTRUMENT array.
*/
void cleanup_instrument_config()
{
  if (pfs_instr_config_array != NULL)
    my_free_container_pointers(*pfs_instr_config_array);
  delete pfs_instr_config_array;
  pfs_instr_config_array= NULL;
}

/**
  Process one performance_schema_instrument configuration string. Isolate the
  instrument name, evaluate the option value, and store them in a dynamic array.
  Return 'false' for success, 'true' for error.

  @param name    Instrument name
  @param value   Configuration option: 'on', 'off', etc.
  @return 0 for success, non zero for errors
*/

int add_pfs_instr_to_array(const char* name, const char* value)
{
  size_t name_length= strlen(name);
  size_t value_length= strlen(value);

  /* Allocate structure plus string buffers plus null terminators */
  PFS_instr_config* e = (PFS_instr_config*)my_malloc(PSI_NOT_INSTRUMENTED,
                                                     sizeof(PFS_instr_config)
                       + name_length + 1 + value_length + 1, MYF(MY_WME));
  if (!e) return 1;

  /* Copy the instrument name */
  e->m_name= (char*)e + sizeof(PFS_instr_config);
  memcpy(e->m_name, name, name_length);
  e->m_name_length= (uint)name_length;
  e->m_name[name_length]= '\0';

  /* Set flags accordingly */
  if (!my_strcasecmp(&my_charset_latin1, value, "counted"))
  {
    e->m_enabled= true;
    e->m_timed= false;
  }
  else
  if (!my_strcasecmp(&my_charset_latin1, value, "true") ||
      !my_strcasecmp(&my_charset_latin1, value, "on") ||
      !my_strcasecmp(&my_charset_latin1, value, "1") ||
      !my_strcasecmp(&my_charset_latin1, value, "yes"))
  {
    e->m_enabled= true;
    e->m_timed= true;
  }
  else
  if (!my_strcasecmp(&my_charset_latin1, value, "false") ||
      !my_strcasecmp(&my_charset_latin1, value, "off") ||
      !my_strcasecmp(&my_charset_latin1, value, "0") ||
      !my_strcasecmp(&my_charset_latin1, value, "no"))
  {
    e->m_enabled= false;
    e->m_timed= false;
  }
  else
  {
    my_free(e);
    return 1;
  }

  /* Add to the array of default startup options */
  if (pfs_instr_config_array->push_back(e))
  {
    my_free(e);
    return 1;
  }

  return 0;
}
