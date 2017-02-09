#include "mysql/psi/psi_stage.h"
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
typedef char my_bool;
typedef int myf;
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
struct PSI_stage_info_none
{
  unsigned int m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_stage_info_none PSI_stage_info;
typedef struct PSI_placeholder PSI_stage_progress;
