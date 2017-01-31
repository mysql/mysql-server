#include "mysql/psi/psi_mdl.h"
#include "my_global.h"
#include "my_macros.h"
#include "my_psi_config.h"
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
struct PSI_mdl_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_mdl_bootstrap PSI_mdl_bootstrap;
typedef struct PSI_placeholder PSI_mdl_service_t;
typedef struct PSI_placeholder PSI_metadata_locker_state;
extern MYSQL_PLUGIN_IMPORT PSI_mdl_service_t *psi_mdl_service;
