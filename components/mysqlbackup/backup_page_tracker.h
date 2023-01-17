/************************************************************************
                      Mysql Enterprise Backup
 Copyright (c) 2019, 2023, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
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
  bool m_is_registered;

  udf_data_t(const std::string name, Item_result return_type,
             Udf_func_any function, Udf_func_init init_function,
             Udf_func_deinit deinit_function)
      : m_name(name),
        m_return_type(return_type),
        m_function(function),
        m_init_function(init_function),
        m_deinit_function(deinit_function),
        m_is_registered(false) {}
};

class Backup_page_tracker {
 private:
  static uchar *m_changed_pages_buf;
  static std::list<udf_data_t *> m_udf_list;
  static void initialize_udf_list();

 public:
  static bool m_receive_changed_page_data;
  static char *m_changed_pages_file;

  // Page track UDF functions
  static mysql_service_status_t register_udfs();
  static mysql_service_status_t unregister_udfs();

  static bool set_page_tracking_init(UDF_INIT *initid, UDF_ARGS *, char *);
  static void set_page_tracking_deinit(UDF_INIT *initid [[maybe_unused]]);
  static long long set_page_tracking(UDF_INIT *initid, UDF_ARGS *,
                                     unsigned char *is_null,
                                     unsigned char *error);

  static bool page_track_get_changed_pages_init(UDF_INIT *initid, UDF_ARGS *,
                                                char *);
  static void page_track_get_changed_pages_deinit(UDF_INIT *initid
                                                  [[maybe_unused]]);
  static long long page_track_get_changed_pages(UDF_INIT *initid, UDF_ARGS *,
                                                unsigned char *is_null,
                                                unsigned char *error);

  static bool page_track_get_start_lsn_init(UDF_INIT *initid, UDF_ARGS *,
                                            char *);
  static void page_track_get_start_lsn_deinit(UDF_INIT *initid
                                              [[maybe_unused]]);
  static long long page_track_get_start_lsn(UDF_INIT *initid, UDF_ARGS *,
                                            unsigned char *is_null,
                                            unsigned char *error);

  static bool page_track_get_changed_page_count_init(UDF_INIT *initid,
                                                     UDF_ARGS *, char *);
  static void page_track_get_changed_page_count_deinit(UDF_INIT *initid
                                                       [[maybe_unused]]);
  static long long page_track_get_changed_page_count(UDF_INIT *initid,
                                                     UDF_ARGS *,
                                                     unsigned char *is_null,
                                                     unsigned char *error);

  static bool page_track_purge_up_to_init(UDF_INIT *initid, UDF_ARGS *, char *);
  static void page_track_purge_up_to_deinit(UDF_INIT *initid [[maybe_unused]]);
  static long long page_track_purge_up_to(UDF_INIT *initid, UDF_ARGS *,
                                          unsigned char *is_null,
                                          unsigned char *error);

  // method to act on a changed backup-id
  static bool backup_id_update();

  /// Method to de-allocate the memory allocated for the buffer holding
  /// path value for the page-track file, if not done before.
  static void deinit();
};

#endif /* INC_PAGE_TRACKER_SERVICE_H */
