#include "mysql/psi/psi_file.h"
#include "my_global.h"
#include "my_io.h"
#include "my_config.h"
static inline int is_directory_separator(char c)
{
  return c == '/';
}
typedef int File;
typedef mode_t MY_MODE;
typedef socklen_t socket_len_t;
typedef int my_socket;
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
struct PSI_file;
typedef struct PSI_file PSI_file;
struct PSI_file_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_file_bootstrap PSI_file_bootstrap;
typedef struct PSI_placeholder PSI_file_service_t;
typedef struct PSI_placeholder PSI_file_info;
typedef struct PSI_placeholder PSI_file_locker_state;
extern MYSQL_PLUGIN_IMPORT PSI_file_service_t *psi_file_service;
