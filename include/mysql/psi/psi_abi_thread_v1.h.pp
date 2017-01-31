#include "mysql/psi/psi_thread.h"
#include "my_global.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_thread.h"
#include "psi_base.h"
#include "my_psi_config.h"
typedef unsigned int PSI_mutex_key;
typedef unsigned int PSI_rwlock_key;
typedef unsigned int PSI_cond_key;
typedef unsigned int PSI_thread_key;
typedef unsigned int PSI_file_key;
typedef unsigned int PSI_stage_key;
typedef unsigned int PSI_statement_key;
typedef unsigned int PSI_socket_key;
struct PSI_placeholder
{
  int m_placeholder;
};
struct opaque_THD
{
  int dummy;
};
typedef struct opaque_THD THD;
struct PSI_thread_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_thread_bootstrap PSI_thread_bootstrap;
typedef int opaque_vio_type;
struct PSI_thread;
typedef struct PSI_thread PSI_thread;
struct PSI_thread_info_v1
{
  PSI_thread_key *m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_thread_info_v1 PSI_thread_info_v1;
typedef void (*register_thread_v1_t)(const char *category,
                                     struct PSI_thread_info_v1 *info,
                                     int count);
typedef int (*spawn_thread_v1_t)(PSI_thread_key key,
                                 my_thread_handle *thread,
                                 const my_thread_attr_t *attr,
                                 void *(*start_routine)(void *),
                                 void *arg);
typedef struct PSI_thread *(*new_thread_v1_t)(PSI_thread_key key,
                                              const void *identity,
                                              ulonglong thread_id);
typedef void (*set_thread_THD_v1_t)(struct PSI_thread *thread, THD *thd);
typedef void (*set_thread_id_v1_t)(struct PSI_thread *thread, ulonglong id);
typedef void (*set_thread_os_id_v1_t)(struct PSI_thread *thread);
typedef struct PSI_thread *(*get_thread_v1_t)(void);
typedef void (*set_thread_user_v1_t)(const char *user, int user_len);
typedef void (*set_thread_account_v1_t)(const char *user,
                                        int user_len,
                                        const char *host,
                                        int host_len);
typedef void (*set_thread_db_v1_t)(const char *db, int db_len);
typedef void (*set_thread_command_v1_t)(int command);
typedef void (*set_connection_type_v1_t)(opaque_vio_type conn_type);
typedef void (*set_thread_start_time_v1_t)(time_t start_time);
typedef void (*set_thread_state_v1_t)(const char *state);
typedef void (*set_thread_info_v1_t)(const char *info, uint info_len);
typedef void (*set_thread_v1_t)(struct PSI_thread *thread);
typedef void (*delete_current_thread_v1_t)(void);
typedef void (*delete_thread_v1_t)(struct PSI_thread *thread);
typedef int (*set_thread_connect_attrs_v1_t)(const char *buffer,
                                             uint length,
                                             const void *from_cs);
typedef void (*get_thread_event_id_v1_t)(ulonglong *thread_internal_id,
                                         ulonglong *event_id);
struct PSI_thread_service_v1
{
  register_thread_v1_t register_thread;
  spawn_thread_v1_t spawn_thread;
  new_thread_v1_t new_thread;
  set_thread_id_v1_t set_thread_id;
  set_thread_THD_v1_t set_thread_THD;
  set_thread_os_id_v1_t set_thread_os_id;
  get_thread_v1_t get_thread;
  set_thread_user_v1_t set_thread_user;
  set_thread_account_v1_t set_thread_account;
  set_thread_db_v1_t set_thread_db;
  set_thread_command_v1_t set_thread_command;
  set_connection_type_v1_t set_connection_type;
  set_thread_start_time_v1_t set_thread_start_time;
  set_thread_state_v1_t set_thread_state;
  set_thread_info_v1_t set_thread_info;
  set_thread_v1_t set_thread;
  delete_current_thread_v1_t delete_current_thread;
  delete_thread_v1_t delete_thread;
  set_thread_connect_attrs_v1_t set_thread_connect_attrs;
  get_thread_event_id_v1_t get_thread_event_id;
};
typedef struct PSI_thread_service_v1 PSI_thread_service_t;
typedef struct PSI_thread_info_v1 PSI_thread_info;
extern MYSQL_PLUGIN_IMPORT PSI_thread_service_t *psi_thread_service;
