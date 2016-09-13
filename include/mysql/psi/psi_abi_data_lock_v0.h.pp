#include "mysql/psi/psi_data_lock.h"
#include "my_global.h"
#include "psi_base.h"
#include "my_psi_config.h"
#include "my_config.h"
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
struct PSI_data_lock_bootstrap
{
  void* (*get_interface)(int version);
};
typedef struct PSI_data_lock_bootstrap PSI_data_lock_bootstrap;
typedef struct PSI_placeholder PSI_data_lock_service_t;
extern MYSQL_PLUGIN_IMPORT PSI_data_lock_service_t *psi_data_lock_service;
