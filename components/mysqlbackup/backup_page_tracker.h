/************************************************************************
                      Mysql Enterprise Backup
 Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 *************************************************************************/

#ifndef BACKUP_PAGE_TRACKER_SERVICE_H
#define BACKUP_PAGE_TRACKER_SERVICE_H

#include <mysql/components/services/page_track_service.h>
#include <list>
#include "mysqlbackup.h"

#ifdef __cplusplus
class THD;
#define MYSQL_THD THD *
#else
#define MYSQL_THD void *
#endif

#define CHANGED_PAGES_BUFFER_SIZE (16 * 1024 * 1024)

// InnoDB page tracking service
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_page_track);

int page_track_callback(MYSQL_THD opaque_thd, const uchar *buffer,
                        size_t buffer_length, int page_count, void *context);

struct udf_data_t {
  std::string m_name;
  Item_result m_return_type;
  Udf_func_any m_function;
  Udf_func_init m_init_function;
  Udf_func_deinit m_deinit_function;

  udf_data_t(const std::string name, Item_result return_type,
             Udf_func_any function, Udf_func_init init_function,
             Udf_func_deinit deinit_function)
      : m_name(name),
        m_return_type(return_type),
        m_function(function),
        m_init_function(init_function),
        m_deinit_function(deinit_function) {}
};

class Backup_page_tracker {
 private:
  static uchar *m_changed_pages_buf;
  static std::list<udf_data_t *> m_udf_list;
  static mysql_service_status_t unregister_udfs(std::list<udf_data_t *> list);
  static void initialize_udf_list();

 public:
  static bool m_receive_changed_page_data;

  // Page track UDF functions
  static mysql_service_status_t register_udfs();
  static mysql_service_status_t unregister_udfs();

  static bool set_page_tracking_init(UDF_INIT *initid, UDF_ARGS *, char *);
  static void set_page_tracking_deinit(UDF_INIT *initid MY_ATTRIBUTE((unused)));
  static long long set_page_tracking(UDF_INIT *initid, UDF_ARGS *,
                                     unsigned char *is_null,
                                     unsigned char *error);

  static bool page_track_get_changed_pages_init(UDF_INIT *initid, UDF_ARGS *,
                                                char *);
  static void page_track_get_changed_pages_deinit(
      UDF_INIT *initid MY_ATTRIBUTE((unused)));
  static long long page_track_get_changed_pages(UDF_INIT *initid, UDF_ARGS *,
                                                unsigned char *is_null,
                                                unsigned char *error);

  static bool page_track_get_start_lsn_init(UDF_INIT *initid, UDF_ARGS *,
                                            char *);
  static void page_track_get_start_lsn_deinit(
      UDF_INIT *initid MY_ATTRIBUTE((unused)));
  static long long page_track_get_start_lsn(UDF_INIT *initid, UDF_ARGS *,
                                            unsigned char *is_null,
                                            unsigned char *error);

  static bool page_track_get_changed_page_count_init(UDF_INIT *initid,
                                                     UDF_ARGS *, char *);
  static void page_track_get_changed_page_count_deinit(
      UDF_INIT *initid MY_ATTRIBUTE((unused)));
  static long long page_track_get_changed_page_count(UDF_INIT *initid,
                                                     UDF_ARGS *,
                                                     unsigned char *is_null,
                                                     unsigned char *error);

  // method to act on a changed backup-id
  static bool backup_id_update();
};

#endif /* INC_PAGE_TRACKER_SERVICE_H */
