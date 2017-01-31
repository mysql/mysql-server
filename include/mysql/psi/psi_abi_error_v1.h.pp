#include "mysql/psi/psi_error.h"
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
struct PSI_error_bootstrap
{
  void* (*get_interface)(int version);
};
typedef struct PSI_error_bootstrap PSI_error_bootstrap;
enum PSI_error_operation
{
  PSI_ERROR_OPERATION_RAISED = 0,
  PSI_ERROR_OPERATION_HANDLED
};
typedef enum PSI_error_operation PSI_error_operation;
typedef void (*log_error_v1_t)(unsigned int error_num,
                               PSI_error_operation error_operation);
struct PSI_error_service_v1
{
  log_error_v1_t log_error;
};
typedef struct PSI_error_service_v1 PSI_error_service_t;
extern MYSQL_PLUGIN_IMPORT PSI_error_service_t* psi_error_service;
