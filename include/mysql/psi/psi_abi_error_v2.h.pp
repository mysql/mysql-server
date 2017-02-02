#include "mysql/psi/psi_error.h"
#include "my_global.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sharedlib.h"
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
typedef struct PSI_placeholder PSI_error_service_t;
extern PSI_error_service_t* psi_error_service;
