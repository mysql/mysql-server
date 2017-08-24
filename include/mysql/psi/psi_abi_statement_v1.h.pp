#include "mysql/psi/psi_statement.h"
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
#include "psi_base.h"
#include "my_psi_config.h"
struct PSI_placeholder
{
  int m_placeholder;
};
#include "mysql/components/services/psi_statement_bits.h"
#include "my_inttypes.h"
#include "my_macros.h"
typedef unsigned int PSI_statement_key;
struct PSI_statement_locker;
typedef struct PSI_statement_locker PSI_statement_locker;
struct PSI_prepared_stmt;
typedef struct PSI_prepared_stmt PSI_prepared_stmt;
struct PSI_digest_locker;
typedef struct PSI_digest_locker PSI_digest_locker;
struct PSI_sp_share;
typedef struct PSI_sp_share PSI_sp_share;
struct PSI_sp_locker;
typedef struct PSI_sp_locker PSI_sp_locker;
struct PSI_statement_info_v1
{
  PSI_statement_key m_key;
  const char *m_name;
  uint m_flags;
  const char *m_documentation;
};
typedef struct PSI_statement_info_v1 PSI_statement_info_v1;
struct PSI_statement_locker_state_v1
{
  bool m_discarded;
  bool m_in_prepare;
  uchar m_no_index_used;
  uchar m_no_good_index_used;
  uint m_flags;
  void *m_class;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  void *m_statement;
  ulonglong m_lock_time;
  ulonglong m_rows_sent;
  ulonglong m_rows_examined;
  ulong m_created_tmp_disk_tables;
  ulong m_created_tmp_tables;
  ulong m_select_full_join;
  ulong m_select_full_range_join;
  ulong m_select_range;
  ulong m_select_range_check;
  ulong m_select_scan;
  ulong m_sort_merge_passes;
  ulong m_sort_range;
  ulong m_sort_rows;
  ulong m_sort_scan;
  const struct sql_digest_storage *m_digest;
  char m_schema_name[(64 * 3)];
  uint m_schema_name_length;
  uint m_cs_number;
  const char *m_query_sample;
  uint m_query_sample_length;
  bool m_query_sample_truncated;
  PSI_sp_share *m_parent_sp_share;
  PSI_prepared_stmt *m_parent_prepared_stmt;
};
typedef struct PSI_statement_locker_state_v1 PSI_statement_locker_state_v1;
struct PSI_sp_locker_state_v1
{
  uint m_flags;
  struct PSI_thread *m_thread;
  ulonglong m_timer_start;
  ulonglong (*m_timer)(void);
  PSI_sp_share *m_sp_share;
};
typedef struct PSI_sp_locker_state_v1 PSI_sp_locker_state_v1;
typedef void (*register_statement_v1_t)(const char *category,
                                        struct PSI_statement_info_v1 *info,
                                        int count);
typedef struct PSI_statement_locker *(*get_thread_statement_locker_v1_t)(
  struct PSI_statement_locker_state_v1 *state,
  PSI_statement_key key,
  const void *charset,
  PSI_sp_share *sp_share);
typedef struct PSI_statement_locker *(*refine_statement_v1_t)(
  struct PSI_statement_locker *locker, PSI_statement_key key);
typedef void (*start_statement_v1_t)(struct PSI_statement_locker *locker,
                                     const char *db,
                                     uint db_length,
                                     const char *src_file,
                                     uint src_line);
typedef void (*set_statement_text_v1_t)(struct PSI_statement_locker *locker,
                                        const char *text,
                                        uint text_len);
typedef void (*set_statement_lock_time_t)(struct PSI_statement_locker *locker,
                                          ulonglong lock_time);
typedef void (*set_statement_rows_sent_t)(struct PSI_statement_locker *locker,
                                          ulonglong count);
typedef void (*set_statement_rows_examined_t)(
  struct PSI_statement_locker *locker, ulonglong count);
typedef void (*inc_statement_created_tmp_disk_tables_t)(
  struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_created_tmp_tables_t)(
  struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_full_join_t)(
  struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_full_range_join_t)(
  struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_range_t)(
  struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_range_check_t)(
  struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_select_scan_t)(struct PSI_statement_locker *locker,
                                            ulong count);
typedef void (*inc_statement_sort_merge_passes_t)(
  struct PSI_statement_locker *locker, ulong count);
typedef void (*inc_statement_sort_range_t)(struct PSI_statement_locker *locker,
                                           ulong count);
typedef void (*inc_statement_sort_rows_t)(struct PSI_statement_locker *locker,
                                          ulong count);
typedef void (*inc_statement_sort_scan_t)(struct PSI_statement_locker *locker,
                                          ulong count);
typedef void (*set_statement_no_index_used_t)(
  struct PSI_statement_locker *locker);
typedef void (*set_statement_no_good_index_used_t)(
  struct PSI_statement_locker *locker);
typedef void (*end_statement_v1_t)(struct PSI_statement_locker *locker,
                                   void *stmt_da);
typedef PSI_prepared_stmt *(*create_prepared_stmt_v1_t)(
  void *identity,
  uint stmt_id,
  PSI_statement_locker *locker,
  const char *stmt_name,
  size_t stmt_name_length,
  const char *name,
  size_t length);
typedef void (*destroy_prepared_stmt_v1_t)(PSI_prepared_stmt *prepared_stmt);
typedef void (*reprepare_prepared_stmt_v1_t)(PSI_prepared_stmt *prepared_stmt);
typedef void (*execute_prepared_stmt_v1_t)(PSI_statement_locker *locker,
                                           PSI_prepared_stmt *prepared_stmt);
typedef struct PSI_digest_locker *(*digest_start_v1_t)(
  struct PSI_statement_locker *locker);
typedef void (*digest_end_v1_t)(struct PSI_digest_locker *locker,
                                const struct sql_digest_storage *digest);
typedef struct PSI_sp_share *(*get_sp_share_v1_t)(uint object_type,
                                                  const char *schema_name,
                                                  uint schema_name_length,
                                                  const char *object_name,
                                                  uint object_name_length);
typedef void (*release_sp_share_v1_t)(struct PSI_sp_share *share);
typedef PSI_sp_locker *(*start_sp_v1_t)(struct PSI_sp_locker_state_v1 *state,
                                        struct PSI_sp_share *sp_share);
typedef void (*end_sp_v1_t)(struct PSI_sp_locker *locker);
typedef void (*drop_sp_v1_t)(uint object_type,
                             const char *schema_name,
                             uint schema_name_length,
                             const char *object_name,
                             uint object_name_length);
typedef struct PSI_statement_info_v1 PSI_statement_info;
typedef struct PSI_statement_locker_state_v1 PSI_statement_locker_state;
typedef struct PSI_sp_locker_state_v1 PSI_sp_locker_state;
struct PSI_statement_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_statement_bootstrap PSI_statement_bootstrap;
struct PSI_statement_service_v1
{
  register_statement_v1_t register_statement;
  get_thread_statement_locker_v1_t get_thread_statement_locker;
  refine_statement_v1_t refine_statement;
  start_statement_v1_t start_statement;
  set_statement_text_v1_t set_statement_text;
  set_statement_lock_time_t set_statement_lock_time;
  set_statement_rows_sent_t set_statement_rows_sent;
  set_statement_rows_examined_t set_statement_rows_examined;
  inc_statement_created_tmp_disk_tables_t inc_statement_created_tmp_disk_tables;
  inc_statement_created_tmp_tables_t inc_statement_created_tmp_tables;
  inc_statement_select_full_join_t inc_statement_select_full_join;
  inc_statement_select_full_range_join_t inc_statement_select_full_range_join;
  inc_statement_select_range_t inc_statement_select_range;
  inc_statement_select_range_check_t inc_statement_select_range_check;
  inc_statement_select_scan_t inc_statement_select_scan;
  inc_statement_sort_merge_passes_t inc_statement_sort_merge_passes;
  inc_statement_sort_range_t inc_statement_sort_range;
  inc_statement_sort_rows_t inc_statement_sort_rows;
  inc_statement_sort_scan_t inc_statement_sort_scan;
  set_statement_no_index_used_t set_statement_no_index_used;
  set_statement_no_good_index_used_t set_statement_no_good_index_used;
  end_statement_v1_t end_statement;
  create_prepared_stmt_v1_t create_prepared_stmt;
  destroy_prepared_stmt_v1_t destroy_prepared_stmt;
  reprepare_prepared_stmt_v1_t reprepare_prepared_stmt;
  execute_prepared_stmt_v1_t execute_prepared_stmt;
  digest_start_v1_t digest_start;
  digest_end_v1_t digest_end;
  get_sp_share_v1_t get_sp_share;
  release_sp_share_v1_t release_sp_share;
  start_sp_v1_t start_sp;
  end_sp_v1_t end_sp;
  drop_sp_v1_t drop_sp;
};
typedef struct PSI_statement_service_v1 PSI_statement_service_t;
extern PSI_statement_service_t *psi_statement_service;
