/************************************************************************
                      Mysql Enterprise Backup
 Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "backup_page_tracker.h"

#include <algorithm>
#if defined _MSC_VER
#include <direct.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>

#include "backup_comp_constants.h"
#include "mysql/plugin.h"
#include "mysqld_error.h"

// defined in mysqlbackup component definition
extern char *mysqlbackup_backup_id;

// Page track system variables
bool Backup_page_tracker::m_receive_changed_page_data = false;
char *Backup_page_tracker::m_changed_pages_file = nullptr;
uchar *Backup_page_tracker::m_changed_pages_buf = nullptr;
std::list<udf_data_t *> Backup_page_tracker::m_udf_list;

bool Backup_page_tracker::backup_id_update() {
  // stop if any ongoing transfer
  if (Backup_page_tracker::m_receive_changed_page_data)
    Backup_page_tracker::m_receive_changed_page_data = false;

  // Delete the existing page tracker file
  if (m_changed_pages_file != nullptr) {
    remove(m_changed_pages_file);
    free(m_changed_pages_file);
    m_changed_pages_file = nullptr;
  }

  return true;
}

void Backup_page_tracker::deinit() {
  if (m_changed_pages_file != nullptr) {
    free(m_changed_pages_file);
    m_changed_pages_file = nullptr;
  }
}

/**
   Make a list of the UDFs exposed by mysqlbackup page_tracking.
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

  m_udf_list.push_back(new udf_data_t(
      Backup_comp_constants::udf_page_track_purge_up_to, INT_RESULT,
      reinterpret_cast<Udf_func_any>(page_track_purge_up_to),
      reinterpret_cast<Udf_func_init>(page_track_purge_up_to_init),
      reinterpret_cast<Udf_func_deinit>(page_track_purge_up_to_deinit)));
}

/**
   Register backup page-track UDFs

   @return Status of UDF registration
   @retval 0 success
   @retval non-zero failure
*/
mysql_service_status_t Backup_page_tracker::register_udfs() {
  if (!m_udf_list.empty()) {
    std::string msg{"UDF list for mysqlbackup_component is not empty."};
    LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG, msg.c_str());
    return (1);
  }

  // Initialize the UDF list
  initialize_udf_list();

  for (auto udf : m_udf_list) {
    if (udf->m_is_registered) {
      std::string msg{udf->m_name + " is already registered."};
      LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG, msg.c_str());
      // un-register the already registered UDFs
      unregister_udfs();
      return (1);
    }

    if (mysql_service_udf_registration->udf_register(
            udf->m_name.c_str(), udf->m_return_type, udf->m_function,
            udf->m_init_function, udf->m_deinit_function)) {
      std::string msg{udf->m_name + " register failed."};
      LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG, msg.c_str());
      // un-register the already registered UDFs
      unregister_udfs();
      return (1);
    }

    // UDF is registered successfully
    udf->m_is_registered = true;
  }

  return (0);
}

/**
   Un-Register backup page-track UDFs

   @return Status of UDF un-registration
   @retval 0 success
   @retval non-zero failure
*/
mysql_service_status_t Backup_page_tracker::unregister_udfs() {
  mysql_service_status_t fail_status{0};

  for (auto udf : m_udf_list) {
    int was_present;
    if (mysql_service_udf_registration->udf_unregister(udf->m_name.c_str(),
                                                       &was_present) &&
        was_present) {
      if (udf->m_is_registered) {
        std::string msg{udf->m_name + " unregister failed."};
        LogErr(ERROR_LEVEL, ER_MYSQLBACKUP_MSG, msg.c_str());
        fail_status = 1;
      }
    } else {
      // UDF is un-registered successfully
      udf->m_is_registered = false;
    }
  }

  if (fail_status == 0) {
    // UDFs are un-registered successfully, clear the UDF list.
    while (!m_udf_list.empty()) {
      delete (m_udf_list.back());
      m_udf_list.pop_back();
    }
  }

  return fail_status;
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
*/
void Backup_page_tracker::set_page_tracking_deinit(UDF_INIT *initid
                                                   [[maybe_unused]]) {}

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
*/
void Backup_page_tracker::page_track_get_start_lsn_deinit(UDF_INIT *initid
                                                          [[maybe_unused]]) {}

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
*/
void Backup_page_tracker::page_track_get_changed_page_count_deinit(
    UDF_INIT *initid [[maybe_unused]]) {}

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
*/
void Backup_page_tracker::page_track_get_changed_pages_deinit(UDF_INIT *initid [
    [maybe_unused]]) {
  free(m_changed_pages_buf);
  m_changed_pages_buf = nullptr;
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
      mysqlbackup_backupdir +
      std::string(Backup_comp_constants::backup_scratch_dir);

#if defined _MSC_VER
  _mkdir(changed_pages_file_dir.c_str());
#else
  if (!mkdir(changed_pages_file_dir.c_str(), 0777)) {
    // no error if already exists
  }
#endif

  free(m_changed_pages_file);
  m_changed_pages_file =
      strdup((changed_pages_file_dir + FN_LIBCHAR + backupid +
              Backup_comp_constants::change_file_extension)
                 .c_str());

  // If file already exists return error
  FILE *fd = fopen(m_changed_pages_file, "r");
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
  Callback function for initialization of UDF
  "mysqlbackup_page_track_purge_up_to".

  @return Status of initialization
  @retval false on success
  @retval true on failure
*/
bool Backup_page_tracker::page_track_purge_up_to_init(UDF_INIT *,
                                                      UDF_ARGS *args,
                                                      char *message) {
  if (args->arg_count != 1) {
    snprintf(message, MYSQL_ERRMSG_SIZE, "Invalid number of arguments.");
    return true;
  }
  if (args->arg_type[0] != INT_RESULT) {
    snprintf(message, MYSQL_ERRMSG_SIZE, "Invalid argument type.");
    return true;
  }
  return false;
}

/**
  Callback method for deinitialization of UDF
  "mysqlbackup_page_track_purge_up_to".
*/
void Backup_page_tracker::page_track_purge_up_to_deinit(UDF_INIT *) {}

/**
  UDF for "mysqlbackup_page_track_purge_up_to"
  See include/mysql/udf_registration_types.h

  The function takes the following parameter:
  lsn           the lsn up to which the page-track data shall be purged.

  The lsn is fixed down to the last start-lsn.

  @return Status
  @retval lsn up to which page-track data was purged on success
  @retval -1 on failure
*/
long long Backup_page_tracker::page_track_purge_up_to(UDF_INIT *,
                                                      UDF_ARGS *args,
                                                      unsigned char *,
                                                      unsigned char *) {
  MYSQL_THD thd;
  if (mysql_service_mysql_current_thread_reader->get(&thd)) {
    mysql_error_service_printf(ER_MYSQLBACKUP_CLIENT_MSG, MYF(0),
                               "Cannot get current thread handle");
    return -1;
  }

  uint64_t lsn = *((long long *)args->args[0]);
  int retval =
      mysql_service_mysql_page_track->purge(thd, PAGE_TRACK_SE_INNODB, &lsn);
  if (retval != 0) {
    return -1;
  }
  return lsn;
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
int page_track_callback(MYSQL_THD opaque_thd [[maybe_unused]],
                        const uchar *buffer,
                        size_t buffer_length [[maybe_unused]], int page_count,
                        void *context [[maybe_unused]]) {
  // Append to the disk file in binary mode
  FILE *fd = fopen(Backup_page_tracker::m_changed_pages_file, "ab");
  if (!fd) {
    std::string msg{std::string("[page-track] Cannot open '") +
                    Backup_page_tracker::m_changed_pages_file +
                    "': " + strerror(errno) + "\n"};
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, msg.c_str());
    return (1);
  }

  size_t data_size = page_count * Backup_comp_constants::page_number_size;
  size_t write_count = fwrite(buffer, sizeof(char), data_size, fd);
  fclose(fd);

  // write failed
  if (write_count != data_size) {
    std::string msg{std::string("[page-track] Cannot write '") +
                    Backup_page_tracker::m_changed_pages_file +
                    "': " + strerror(errno) + "\n"};
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_MYSQLBACKUP_MSG, msg.c_str());
    return (1);
  }

  // on-going backup interrupted, stop receiving the changed page data
  if (!Backup_page_tracker::m_receive_changed_page_data)
    return (2);  // interrupt an ongoing transfer
  else
    return (0);
}
