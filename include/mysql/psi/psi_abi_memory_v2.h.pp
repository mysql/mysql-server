#include "mysql/psi/psi_memory.h"
typedef unsigned int PSI_memory_key;
C_MODE_START
struct PSI_thread;
struct PSI_memory_bootstrap
{
  void* (*get_interface)(int version);
};
typedef struct PSI_memory_bootstrap PSI_memory_bootstrap;
typedef struct PSI_placeholder PSI_memory_service_t;
typedef struct PSI_placeholder PSI_memory_info;
extern MYSQL_PLUGIN_IMPORT PSI_memory_service_t *psi_memory_service;
C_MODE_END
