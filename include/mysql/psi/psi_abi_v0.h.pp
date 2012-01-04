#include "mysql/psi/psi.h"
C_MODE_START
struct TABLE_SHARE;
struct PSI_mutex;
struct PSI_rwlock;
struct PSI_cond;
struct PSI_table_share;
struct PSI_table;
struct PSI_thread;
struct PSI_file;
struct PSI_socket;
struct PSI_table_locker;
struct PSI_statement_locker;
struct PSI_idle_locker;
struct PSI_bootstrap
{
  void* (*get_interface)(int version);
};
struct PSI_none
{
  int opaque;
};
typedef struct PSI_none PSI;
struct PSI_stage_info_none
{
  unsigned int m_key;
  const char *m_name;
  int m_flags;
};
typedef struct PSI_stage_info_none PSI_stage_info;
extern MYSQL_PLUGIN_IMPORT PSI *PSI_server;
C_MODE_END
