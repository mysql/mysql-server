#include "mysql/psi/psi_memory.h"
#include "my_psi_config.h"
typedef unsigned int PSI_memory_key;
struct PSI_thread;
struct PSI_memory_bootstrap
{
  void* (*get_interface)(int version);
};
typedef struct PSI_memory_bootstrap PSI_memory_bootstrap;
typedef struct PSI_placeholder PSI_memory_service_t;
typedef struct PSI_placeholder PSI_memory_info;
extern MYSQL_PLUGIN_IMPORT PSI_memory_service_t *psi_memory_service;
