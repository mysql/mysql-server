#include "mysql/psi/psi_thread.h"
#include "my_global.h"
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
C_MODE_START
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
typedef struct PSI_placeholder PSI_thread_service_t;
typedef struct PSI_placeholder PSI_thread_info;
extern MYSQL_PLUGIN_IMPORT PSI_thread_service_t *psi_thread_service;
C_MODE_END
