#include "mysql/psi/psi_system.h"
#include "my_inttypes.h"
#include "my_config.h"
typedef unsigned char uchar;
typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef unsigned long long int ulonglong;
typedef long long int longlong;
typedef longlong int64;
typedef ulonglong uint64;
typedef unsigned long long my_ulonglong;
typedef intptr_t intptr;
typedef ulonglong my_off_t;
typedef ptrdiff_t my_ptrdiff_t;
typedef int myf;
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sharedlib.h"
#include "mysql/components/services/psi_system_bits.h"
typedef void (*unload_plugin_v1_t)(const char *plugin_name);
struct PSI_system_bootstrap {
  void *(*get_interface)(int version);
};
typedef struct PSI_system_bootstrap PSI_system_bootstrap;
struct PSI_system_service_v1 {
  unload_plugin_v1_t unload_plugin;
};
typedef struct PSI_system_service_v1 PSI_system_service_t;
extern PSI_system_service_t *psi_system_service;
