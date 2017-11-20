#include "mysql/psi/psi_memory.h"
#include "my_psi_config.h"
#include "my_sharedlib.h"
#include "mysql/components/services/psi_memory_bits.h"
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
typedef unsigned int PSI_memory_key;
struct PSI_thread;
struct PSI_memory_info_v1
{
  PSI_memory_key *m_key;
  const char *m_name;
  uint m_flags;
  int m_volatility;
  const char *m_documentation;
};
typedef struct PSI_memory_info_v1 PSI_memory_info_v1;
typedef void (*register_memory_v1_t)(const char *category,
                                     struct PSI_memory_info_v1 *info,
                                     int count);
typedef PSI_memory_key (*memory_alloc_v1_t)(PSI_memory_key key,
                                            size_t size,
                                            struct PSI_thread **owner);
typedef PSI_memory_key (*memory_realloc_v1_t)(PSI_memory_key key,
                                              size_t old_size,
                                              size_t new_size,
                                              struct PSI_thread **owner);
typedef PSI_memory_key (*memory_claim_v1_t)(PSI_memory_key key,
                                            size_t size,
                                            struct PSI_thread **owner);
typedef void (*memory_free_v1_t)(PSI_memory_key key,
                                 size_t size,
                                 struct PSI_thread *owner);
typedef struct PSI_memory_info_v1 PSI_memory_info;
typedef unsigned int PSI_memory_key;
struct PSI_thread;
struct PSI_memory_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_memory_bootstrap PSI_memory_bootstrap;
struct PSI_memory_service_v1
{
  register_memory_v1_t register_memory;
  memory_alloc_v1_t memory_alloc;
  memory_realloc_v1_t memory_realloc;
  memory_claim_v1_t memory_claim;
  memory_free_v1_t memory_free;
};
typedef struct PSI_memory_service_v1 PSI_memory_service_t;
extern PSI_memory_service_t *psi_memory_service;
