#include "mysql/psi/psi_stage.h"
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
struct PSI_stage_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_stage_bootstrap PSI_stage_bootstrap;
typedef struct PSI_placeholder PSI_stage_service_t;
typedef struct PSI_placeholder PSI_stage_info;
typedef struct PSI_placeholder PSI_stage_progress;
extern MYSQL_PLUGIN_IMPORT PSI_stage_service_t *psi_stage_service;
