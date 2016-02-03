#include "mysql/psi/psi_statement.h"
#include "my_global.h"
#include "psi_base.h"
typedef unsigned int PSI_mutex_key;
typedef unsigned int PSI_rwlock_key;
typedef unsigned int PSI_cond_key;
typedef unsigned int PSI_thread_key;
typedef unsigned int PSI_file_key;
typedef unsigned int PSI_stage_key;
typedef unsigned int PSI_statement_key;
typedef unsigned int PSI_socket_key;
typedef unsigned int PSI_memory_key;
struct PSI_placeholder
{
  int m_placeholder;
};
C_MODE_START
struct PSI_statement_bootstrap
{
  void* (*get_interface)(int version);
};
typedef struct PSI_statement_bootstrap PSI_statement_bootstrap;
typedef struct PSI_placeholder PSI_statement_service_t;
typedef struct PSI_placeholder PSI_statement_info;
typedef struct PSI_placeholder PSI_statement_locker_state;
typedef struct PSI_placeholder PSI_sp_locker_state;
extern MYSQL_PLUGIN_IMPORT PSI_statement_service_t *psi_statement_service;
C_MODE_END
