/************************************************************************
                      Mysql Enterprise Backup
 Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 *************************************************************************/

#include "backup_page_tracker.h"
#include "backup_comp_constants.h"
#if defined _MSC_VER
#include <direct.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include "m_string.h"
#include "my_io.h"
#include "mysql/plugin.h"
#include "mysqld_error.h"

// defined in mysqlbackup component definition
extern char *mysqlbackup_backup_id;

// page track system variables
uchar *Backup_page_tracker::m_changed_pages_buf = nullptr;
static std::string changed_pages_file;
bool Backup_page_tracker::m_receive_changed_page_data = false;
std::list<udf_data_t *> Backup_page_tracker::m_udf_list;

bool Backup_page_tracker::backup_id_update() {
  // stop if any ongoing transfer
  if (Backup_page_tracker::m_receive_changed_page_data)
    Backup_page_tracker::m_receive_changed_page_data = false;

  // delete the existing page tracker file
  remove(changed_pages_file.c_str());
  return true;
}

/**
   Make a list of the UDFs exposed by mysqlbackup page_tracking.

   @return None
*/
void Backup_page_tracker::initialize_udf_list() {
  m_udf_list.push_back(new udf_data_t(
      Backup_comp_constants::udf_set_page_tracking, INT_RESULT,
      reinterpret_cast<Udf_func_any>(set_page_tracking),
      reinterpret_cast<Udf_func_init>(set_page_tracking_init),
      reinterpret_cast<Udf_func_deinit>(set_page_tracking_deinit)));

  m_udf_list.push_back(new udf_data_t(
      Backup_comp_constants::udf_get_start_lsn, INT_RESULT,
      reinterpret_cast<Udf_func_any>(page_track_get_start_lsn),
      reinterpret_cast<Udf_func_init>(page_track_get_start_lsn_init),
      reinterpret_cast<Udf_func_deinit>(page_track_get_start_lsn_deinit)));

  m_udf_list.push_back(new udf_data_t(
      Backup_comp_constants::udf_get_changed_page_count, INT_RESULT,
      reinterpret_cast<Udf_func_any>(page_track_get_changed_page_count),
      reinterpret_cast<Udf_func_init>(page_track_get_changed_page_count_init),
      reinterpret_cast<Udf_func_deinit>(
          page_track_get_changed_page_count_deinit)));

  m_udf_list.push_back(new udf_data_t(
      Backup_comp_constants::udf_get_changed_pages, INT_RESULT,
      reinterpret_cast<Udf_func_any>(page_track_get_changed_pages),
      reinterpret_cast<Udf_func_init>(page_track_get_changed_pages_init),
      reinterpret_cast<Udf_func_deinit>(page_track_get_changed_pages_deinit)));
}

/**
   Register backup page-track UDFs

   @return Status of UDF registration
   @retval 0 success
   @retval non-zero failure
*/
mysql_service_status_t Backup_page_tracker::register_udfs() {
  initialize_udf_list();
  std::list<udf_data_t *> success_list;
  for (auto udf : m_udf_list) {
    if (mysql_service_udf_registration->udf_register(
            udf->m_name.c_str(), udf->m_return_type, udf->m_function,
            udf->m_init_function, udf->m_deinit_function)) {
      LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG,
             std::string(udf->m_name + " registration failed.").c_str());
      // un-register already successful UDFs
      unregister_udfs(success_list);
      return (1);
    }
    // add the udf into success list
    success_list.push_back(udf);
  }
  return (0);
}

/**
   Un-Register a given list of UDFs

   @return Status of UDF un-registration
   @retval 0 success
   @retval non-zero failure
*/
mysql_service_status_t Backup_page_tracker::unregister_udfs(
    std::list<udf_data_t *> list) {
  int was_present;
  std::list<udf_data_t *> fail_list;

  for (auto udf : list) {
    if (mysql_service_udf_registration->udf_unregister(udf->m_name.c_str(),
                                                       &was_present) ||
        !was_present) {
      LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG,
             std::string(udf->m_name + " un-register failed").c_str());
      fail_list.push_back(udf);
    }
    delete (udf);
  }

  if (!fail_list.empty()) return (1);
  return (0);
}

/**
   Un-Register backup page-track UDFs

   @return Status of UDF un-registration
   @retval 0 success
   @retval non-zero failure
*/
mysql_service_status_t Backup_page_tracker::unregister_udfs() {
  return (unregister_udfs(m_udf_list));
}

/**
   Callback function for initialization of UDF "mysqlbackup_page_track_set".

   @return Status of initialization
   @retval false on success
   @retval true on failure
*/
bool Backup_page_tracker::set_page_tracking_init(UDF_INIT *, UDF_ARGS *,
                                                 char *) {
  return (false);
}

/**
   Callback method for initialization of UDF "mysqlbackup_page_track_set".

   @return None
*/
void Backup_page_tracker::set_page_tracking_deinit(
    UDF_INIT *initid MY_ATTRIBUTE((unused))) {}

/**
  UDF for "mysqlbackup_page_track_set"
  See include/mysql/udf_registration_types.h

  @return lsn at which page-tracking is started/stopped.
*/
long long Backup_page_tracker::set_page_tracking(UDF_INIT *, UDF_ARGS *args,
                                                 unsigned char *,
                                                 unsigned char *) {
  MYSQL_THD thd;
  if (mysql_service_mysql_current_thread_reader->get(&thd)) {
    return (-1);
  }

  if (args->arg_count != 1 || args->arg_type[0] != INT_RESULT) {
    return (-1);
  }

  int retval = 0;
  uint64_t lsn = 0;
  retval =
      mysql_service_mysql_page_track->start(thd, PAGE_TRACK_SE_INNODB, &lsn);
  if (retval) return (-1 * retval);

  // try stop only if page-track is ongoing
  if (!(*((long long *)args->args[0])) && (lsn != 0)) {
    retval =
        mysql_service_mysql_page_track->stop(thd, PAGE_TRACK_SE_INNODB, &lsn);
    if (retval) return (-1 * retval);
  }
  return lsn;
}

/**
   Callback function for initialization of UDF
   "mysqlbackup_page_track_get_start_lsn"

   @return Status of initialization
   @retval false on success
   @retval true on failure
*/
bool Backup_page_tracker::page_track_get_start_lsn_init(UDF_INIT *, UDF_ARGS *,
                                                        char *) {
  return (false);
}

/**
   Callback method for initialization of UDF
   "mysqlbackup_page_track_get_start_lsn"

   @return None
*/
void Backup_page_tracker::page_track_get_start_lsn_deinit(
    UDF_INIT *initid MY_ATTRIBUTE((unused))) {}

/**
  UDF for "mysqlbackup_page_track_get_start_lsn"
  See include/mysql/udf_registration_types.h

  @return lsn at which page-tracking is started/stopped.
*/
long long Backup_page_tracker::page_track_get_start_lsn(UDF_INIT *,
                                                        UDF_ARGS *args,
                                                        unsigned char *,
                                                        unsigned char *) {
  MYSQL_THD thd;
  if (mysql_service_mysql_current_thread_reader->get(&thd)) {
    return (-1);
  }
  if (args->arg_count != 0) {
    return (-1);
  }
  uint64_t first_start_lsn, last_start_lsn;  // ignore the return value
  mysql_service_mysql_page_track->get_status(thd, PAGE_TRACK_SE_INNODB,
                                             &first_start_lsn, &last_start_lsn);
  return first_start_lsn;
}

/**
   Callback function for initialization of UDF
   "mysqlbackup_page_track_get_changed_page_count".

   @return Status of initialization
   @retval false on success
   @retval true on failure
*/
bool Backup_page_tracker::page_track_get_changed_page_count_init(UDF_INIT *,
                                                                 UDF_ARGS *,
                                                                 char *) {
  return false;
}

/**
   Callback method for initialization of UDF
   "mysqlbackup_page_track_get_changed_page_count".

   @return None
*/
void Backup_page_tracker::page_track_get_changed_page_count_deinit(
    UDF_INIT *initid MY_ATTRIBUTE((unused))) {}

/**
  UDF for "mysqlbackup_page_track_get_changed_page_count"
  See include/mysql/udf_registration_types.h

  @return lsn at which page-tracking is started/stopped.
*/
long long Backup_page_tracker::page_track_get_changed_page_count(
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *) {
  MYSQL_THD thd;
  if (mysql_service_mysql_current_thread_reader->get(&thd)) {
    return (-1);
  }

  if (args->arg_count != 2 || args->arg_type[0] != INT_RESULT ||
      args->arg_type[1] != INT_RESULT) {
    return (-1);
  }
  uint64_t changed_page_count = 0;
  // get the values form the agrs passed to UDF
  uint64_t start_lsn = *((long long *)args->args[0]);
  uint64_t stop_lsn = *((long long *)args->args[1]);

  int status = mysql_service_mysql_page_track->get_num_page_ids(
      thd, PAGE_TRACK_SE_INNODB, &start_lsn, &stop_lsn, &changed_page_count);
  if (status) return (-1 * status);

  return (changed_page_count);
}

/**
   Callback function for initialization of UDF
   "mysqlbackup_page_track_get_changed_pages".

   @return Status of initialization
   @retval false on success
   @retval true on failure
*/
bool Backup_page_tracker::page_track_get_changed_pages_init(UDF_INIT *,
                                                            UDF_ARGS *,
                                                            char *) {
  m_changed_pages_buf = (uchar *)malloc(CHANGED_PAGES_BUFFER_SIZE);
  return false;
}

/**
   Callback method for initialization of UDF
   "mysqlbackup_page_track_get_changed_pages".

   @return None
*/
void Backup_page_tracker::page_track_get_changed_pages_deinit(
    UDF_INIT *initid MY_ATTRIBUTE((unused))) {
  free(m_changed_pages_buf);
}

/**
  UDF for "mysqlbackup_page_track_get_changed_pages"
  See include/mysql/udf_registration_types.h

  @returns an int status
*/
long long Backup_page_tracker::page_track_get_changed_pages(UDF_INIT *,
                                                            UDF_ARGS *args,
                                                            unsigned char *,
                                                            unsigned char *) {
  MYSQL_THD thd;
  if (mysql_service_mysql_current_thread_reader->get(&thd)) {
    return (-1);
  }

  if (args->arg_count != 2 || args->arg_type[0] != INT_RESULT ||
      args->arg_type[1] != INT_RESULT) {
    return (-1);
  }

  if (!mysqlbackup_backup_id) {
    return (-1);
  }
  // Not expecting anything other than digits in the backupid.
  // Make sure no elements of a relative path are there if the
  // above rule is relaxed
  std::string backupid = mysqlbackup_backup_id;
  if (!std::all_of(backupid.begin(), backupid.end(), ::isdigit)) return 1;

  char mysqlbackup_backupdir[1023];
  void *p = &mysqlbackup_backupdir;
  size_t var_len = 1023;

  mysql_service_component_sys_variable_register->get_variable(
      "mysql_server", "datadir", (void **)&p, &var_len);
  if (var_len == 0) return 2;

  std::string changed_pages_file_dir =
      mysqlbackup_backupdir + Backup_comp_constants::backup_scratch_dir;

#if defined _MSC_VER
  _mkdir(changed_pages_file_dir.c_str());
#else
  if (!mkdir(changed_pages_file_dir.c_str(), 0777)) {
    // no error if already exists
  }
#endif

  changed_pages_file = changed_pages_file_dir + FN_LIBCHAR + backupid +
                       Backup_comp_constants::change_file_extension;
  // if file already exists return error
  FILE *fd = fopen(changed_pages_file.c_str(), "r");
  if (fd) {
    fclose(fd);
    return (-1);
  }

  // get the values form the agrs passed to UDF
  uint64_t start_lsn = *((long long *)args->args[0]);
  uint64_t stop_lsn = *((long long *)args->args[1]);

  Backup_page_tracker::m_receive_changed_page_data = true;
  int status = mysql_service_mysql_page_track->get_page_ids(
      thd, PAGE_TRACK_SE_INNODB, &start_lsn, &stop_lsn,
      Backup_page_tracker::m_changed_pages_buf, CHANGED_PAGES_BUFFER_SIZE,
      page_track_callback, nullptr);
  Backup_page_tracker::m_receive_changed_page_data = false;

  return (status);
}

/**
   Callback method from InnoDB page-tracking to return the changed pages.

   @param[in]  opaque_thd     Current thread context.
   @param[in]  buffer         Buffer filled with 8 byte page ids.
   @param[in]  buffer_length  Total buffer length
   @param[in]  page_count     Number of pages in the buffer
   @param[in]  context        User data pointer passed to the InnoDB service

   @return Status if the call-back was successful in handling the data
   @retval 0 success
   @retval non-zero failure
*/
int page_track_callback(MYSQL_THD opaque_thd MY_ATTRIBUTE((unused)),
                        const uchar *buffer,
                        size_t buffer_length MY_ATTRIBUTE((unused)),
                        int page_count, void *context MY_ATTRIBUTE((unused))) {
  // append to the disk file in binary mode
  FILE *fd = fopen(changed_pages_file.c_str(), "ab");
  if (!fd) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, "[page-track] File open failed.");
    return (1);
  }

  size_t data_size = page_count * Backup_comp_constants::page_number_size;
  size_t write_count = fwrite(buffer, sizeof(char), data_size, fd);
  fclose(fd);

  // write failed
  if (write_count != data_size) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, "[page-track] Writing to file failed.");
    return (1);
  }

  // on-going backup interrupted, stop receiving the changed page data
  if (!Backup_page_tracker::m_receive_changed_page_data)
    return (2);  // interupt an on going transfer
  else
    return (0);
}
