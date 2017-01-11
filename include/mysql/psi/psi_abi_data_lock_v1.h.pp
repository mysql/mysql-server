#include "mysql/psi/psi_data_lock.h"
#include "my_global.h"
#include "psi_base.h"
#include "my_psi_config.h"
#include "my_config.h"
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
struct PSI_data_lock_bootstrap
{
  void *(*get_interface)(int version);
};
typedef struct PSI_data_lock_bootstrap PSI_data_lock_bootstrap;
class PSI_server_data_lock_container
{
public:
  PSI_server_data_lock_container()
  {
  }
  virtual ~PSI_server_data_lock_container()
  {
  }
  virtual const char *cache_string(const char *string) = 0;
  virtual const char *cache_data(const char *ptr, size_t length) = 0;
  virtual bool accept_engine(const char *engine, size_t engine_length) = 0;
  virtual bool accept_lock_id(const char *engine_lock_id,
                              size_t engine_lock_id_length) = 0;
  virtual bool accept_transaction_id(ulonglong transaction_id) = 0;
  virtual bool accept_thread_id_event_id(ulonglong thread_id,
                                         ulonglong event_id) = 0;
  virtual bool accept_object(const char *table_schema,
                             size_t table_schema_length,
                             const char *table_name,
                             size_t table_name_length,
                             const char *partition_name,
                             size_t partition_name_length,
                             const char *sub_partition_name,
                             size_t sub_partition_name_length) = 0;
  virtual void add_lock_row(const char *engine,
                            size_t engine_length,
                            const char *engine_lock_id,
                            size_t engine_lock_id_length,
                            ulonglong transaction_id,
                            ulonglong thread_id,
                            ulonglong event_id,
                            const char *table_schema,
                            size_t table_schema_length,
                            const char *table_name,
                            size_t table_name_length,
                            const char *partition_name,
                            size_t partition_name_length,
                            const char *sub_partition_name,
                            size_t sub_partition_name_length,
                            const char *index_name,
                            size_t index_name_length,
                            const void *identity,
                            const char *lock_mode,
                            const char *lock_type,
                            const char *lock_status,
                            const char *lock_data) = 0;
};
class PSI_server_data_lock_wait_container
{
public:
  PSI_server_data_lock_wait_container()
  {
  }
  virtual ~PSI_server_data_lock_wait_container()
  {
  }
  virtual const char *cache_string(const char *string) = 0;
  virtual const char *cache_data(const char *ptr, size_t length) = 0;
  virtual bool accept_engine(const char *engine, size_t engine_length) = 0;
  virtual bool accept_requesting_lock_id(const char *engine_lock_id,
                                         size_t engine_lock_id_length) = 0;
  virtual bool accept_blocking_lock_id(const char *engine_lock_id,
                                       size_t engine_lock_id_length) = 0;
  virtual bool accept_requesting_transaction_id(ulonglong transaction_id) = 0;
  virtual bool accept_blocking_transaction_id(ulonglong transaction_id) = 0;
  virtual bool accept_requesting_thread_id_event_id(ulonglong thread_id,
                                                    ulonglong event_id) = 0;
  virtual bool accept_blocking_thread_id_event_id(ulonglong thread_id,
                                                  ulonglong event_id) = 0;
  virtual void add_lock_wait_row(const char *engine,
                                 size_t engine_length,
                                 const char *requesting_engine_lock_id,
                                 size_t requesting_engine_lock_id_length,
                                 ulonglong requesting_transaction_id,
                                 ulonglong requesting_thread_id,
                                 ulonglong requesting_event_id,
                                 const void *requesting_identity,
                                 const char *blocking_engine_lock_id,
                                 size_t blocking_engine_lock_id_length,
                                 ulonglong blocking_transaction_id,
                                 ulonglong blocking_thread_id,
                                 ulonglong blocking_event_id,
                                 const void *blocking_identity) = 0;
};
class PSI_engine_data_lock_iterator
{
public:
  PSI_engine_data_lock_iterator()
  {
  }
  virtual ~PSI_engine_data_lock_iterator()
  {
  }
  virtual bool scan(PSI_server_data_lock_container *container,
                    bool with_lock_data) = 0;
  virtual bool fetch(PSI_server_data_lock_container *container,
                     const char *engine_lock_id,
                     size_t engine_lock_id_length,
                     bool with_lock_data) = 0;
};
class PSI_engine_data_lock_wait_iterator
{
public:
  PSI_engine_data_lock_wait_iterator()
  {
  }
  virtual ~PSI_engine_data_lock_wait_iterator()
  {
  }
  virtual bool scan(PSI_server_data_lock_wait_container *container) = 0;
  virtual bool fetch(PSI_server_data_lock_wait_container *container,
                     const char *requesting_engine_lock_id,
                     size_t requesting_engine_lock_id_length,
                     const char *blocking_engine_lock_id,
                     size_t blocking_engine_lock_id_length) = 0;
};
class PSI_engine_data_lock_inspector
{
public:
  PSI_engine_data_lock_inspector()
  {
  }
  virtual ~PSI_engine_data_lock_inspector()
  {
  }
  virtual PSI_engine_data_lock_iterator *create_data_lock_iterator() = 0;
  virtual PSI_engine_data_lock_wait_iterator *
  create_data_lock_wait_iterator() = 0;
  virtual void destroy_data_lock_iterator(
    PSI_engine_data_lock_iterator *it) = 0;
  virtual void destroy_data_lock_wait_iterator(
    PSI_engine_data_lock_wait_iterator *it) = 0;
};
typedef void (*register_data_lock_v1_t)(
  PSI_engine_data_lock_inspector *inspector);
typedef void (*unregister_data_lock_v1_t)(
  PSI_engine_data_lock_inspector *inspector);
struct PSI_data_lock_service_v1
{
  register_data_lock_v1_t register_data_lock;
  unregister_data_lock_v1_t unregister_data_lock;
};
typedef struct PSI_data_lock_service_v1 PSI_data_lock_service_t;
extern MYSQL_PLUGIN_IMPORT PSI_data_lock_service_t *psi_data_lock_service;
