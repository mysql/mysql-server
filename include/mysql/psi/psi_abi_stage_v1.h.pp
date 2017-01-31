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
struct PSI_stage_progress_v1
{
  ulonglong m_work_completed;
  ulonglong m_work_estimated;
};
typedef struct PSI_stage_progress_v1 PSI_stage_progress_v1;
struct PSI_stage_info_v1
{
  PSI_stage_key m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_stage_info_v1 PSI_stage_info_v1;
typedef void (*register_stage_v1_t)(const char *category,
                                    struct PSI_stage_info_v1 **info,
                                    int count);
typedef PSI_stage_progress_v1 *(*start_stage_v1_t)(PSI_stage_key key,
                                                   const char *src_file,
                                                   int src_line);
typedef PSI_stage_progress_v1 *(*get_current_stage_progress_v1_t)(void);
typedef void (*end_stage_v1_t)(void);
struct PSI_stage_service_v1
{
  register_stage_v1_t register_stage;
  start_stage_v1_t start_stage;
  get_current_stage_progress_v1_t get_current_stage_progress;
  end_stage_v1_t end_stage;
};
typedef struct PSI_stage_service_v1 PSI_stage_service_t;
typedef struct PSI_stage_info_v1 PSI_stage_info;
typedef struct PSI_stage_progress_v1 PSI_stage_progress;
extern MYSQL_PLUGIN_IMPORT PSI_stage_service_t *psi_stage_service;
