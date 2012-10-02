/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef RPL_MI_H
#define RPL_MI_H

#ifdef HAVE_REPLICATION

#include <my_global.h>
#include <sql_priv.h>

#define DEFAULT_CONNECT_RETRY 60

#include "rpl_rli.h"
#include "my_sys.h"

typedef struct st_mysql MYSQL;
class Rpl_info_factory;

/*****************************************************************************
  Replication IO Thread

  Master_info contains:
    - information about how to connect to a master
    - current master log name
    - current master log offset
    - misc control variables

  Master_info is initialized once from the master.info repository if such
  exists. Otherwise, data members corresponding to master.info fields
  are initialized with defaults specified by master-* options. The
  initialization is done through mi_init_info() call.

  Logically, the format of master.info repository is presented as follows:

  log_name
  log_pos
  master_host
  master_user
  master_pass
  master_port
  master_connect_retry

  To write out the contents of master.info to disk a call to flush_info()
  is required. Currently, it is needed every time we read and queue data
  from the master.

  To clean up, call end_info()

*****************************************************************************/

class Master_info : public Rpl_info
{
friend class Rpl_info_factory;

public:
  /**
    Host name or ip address stored in the master.info.
  */
  char host[HOSTNAME_LENGTH + 1];

private:
  /**
    If true, USER/PASSWORD was specified when running START SLAVE.
  */
  bool start_user_configured;
  /**
    User's name stored in the master.info.
  */
  char user[USERNAME_LENGTH + 1];
  /**
    User's password stored in the master.info.
  */
  char password[MAX_PASSWORD_LENGTH + 1]; 
  /**
    User specified when running START SLAVE.
  */
  char start_user[USERNAME_LENGTH + 1];
  /**
    Password specified when running START SLAVE.
  */
  char start_password[MAX_PASSWORD_LENGTH + 1]; 
  /**
    Stores the autentication plugin specified when running START SLAVE.
  */
  char start_plugin_auth[FN_REFLEN + 1];
  /**
    Stores the autentication plugin directory specified when running
    START SLAVE.
  */
  char start_plugin_dir[FN_REFLEN + 1];

public:
  /**
    Returns if USER/PASSWORD was specified when running
    START SLAVE.

    @return true or false.
  */
  bool is_start_user_configured() const
  {
    return start_user_configured;
  }
  /**
    Returns if DEFAULT_AUTH was specified when running START SLAVE.

    @return true or false.
  */
  bool is_start_plugin_auth_configured() const
  {
    return (start_plugin_auth[0] != 0);
  }
  /**
    Returns if PLUGIN_DIR was specified when running START SLAVE.

    @return true or false.
  */
  bool is_start_plugin_dir_configured() const
  {
    return (start_plugin_dir[0] != 0);
  }
  /**
    Defines that USER/PASSWORD was specified or not when running
    START SLAVE.

    @param config is true or false.
  */
  void set_start_user_configured(bool config)
  {
    start_user_configured= config;
  }
  /**
    Sets either user's name in the master.info repository when CHANGE
    MASTER is executed or user's name used in START SLAVE if USER is
    specified.

    @param user_arg is user's name.
  */
  void set_user(const char* user_arg)
  {
    if (user_arg && start_user_configured)
    {
      strmake(start_user, user_arg, sizeof(start_user) - 1);
    }
    else if (user_arg)
    {
      strmake(user, user_arg, sizeof(user) - 1);
    }
  }
  /*
    Returns user's size name. See @code get_user().

    @return user's size name.
  */
  size_t get_user_size() const
  {
    return (start_user_configured ? sizeof(start_user) : sizeof(user));
  }
  /**
    If an user was specified when running START SLAVE, this function returns
    such user. Otherwise, it returns the user stored in master.info.

    @return user's name.
  */
  const char *get_user() const
  {
    return start_user_configured ? start_user : user;
  } 
  /**
    Stores either user's password in the master.info repository when CHANGE
    MASTER is executed or user's password used in START SLAVE if PASSWORD
    is specified.

    @param password_arg is user's password.
    @param password_arg_size is password's size.

    @return false if there is no error, otherwise true is returned.
  */
  bool set_password(const char* password_arg, int password_arg_size);
  /**
    Returns either user's password in the master.info repository or
    user's password used in START SLAVE.

    @param password_arg is user's password.

    @return false if there is no error, otherwise true is returned.
  */
  bool get_password(char *password_arg, int *password_arg_size);
  /**
    Cleans in-memory password defined by START SLAVE.
  */
  void reset_start_info();
  /**
    Returns the DEFAULT_AUTH defined by START SLAVE.

    @return DEFAULT_AUTH.
  */
  const char *get_start_plugin_auth()
  {
    return start_plugin_auth;
  }
  /**
    Returns the PLUGIN_DIR defined by START SLAVE.

    @return PLUGIN_DIR.
  */
  const char *get_start_plugin_dir()
  {
    return start_plugin_dir;
  }
  /**
    Stores the DEFAULT_AUTH defined by START SLAVE.

    @param DEFAULT_AUTH.
  */
  void set_plugin_auth(const char* src)
  {
    if (src)
      strmake(start_plugin_auth, src, sizeof(start_plugin_auth) - 1);
  }
  /**
    Stores the DEFAULT_AUTH defined by START SLAVE.

    @param DEFAULT_AUTH.
  */
  void set_plugin_dir(const char* src)
  {
    if (src)
      strmake(start_plugin_dir, src, sizeof(start_plugin_dir) - 1);
  }

  my_bool ssl; // enables use of SSL connection if true
  char ssl_ca[FN_REFLEN], ssl_capath[FN_REFLEN], ssl_cert[FN_REFLEN];
  char ssl_cipher[FN_REFLEN], ssl_key[FN_REFLEN];
  char ssl_crl[FN_REFLEN], ssl_crlpath[FN_REFLEN];
  my_bool ssl_verify_server_cert;

  MYSQL* mysql;
  uint32 file_id;				/* for 3.23 load data infile */
  Relay_log_info *rli;
  uint port;
  uint connect_retry;
  /*
     The difference in seconds between the clock of the master and the clock of
     the slave (second - first). It must be signed as it may be <0 or >0.
     clock_diff_with_master is computed when the I/O thread starts; for this the
     I/O thread does a SELECT UNIX_TIMESTAMP() on the master.
     "how late the slave is compared to the master" is computed like this:
     clock_of_slave - last_timestamp_executed_by_SQL_thread - clock_diff_with_master

  */
  long clock_diff_with_master;
  float heartbeat_period;         // interface with CHANGE MASTER or master.info
  ulonglong received_heartbeats;  // counter of received heartbeat events

  time_t last_heartbeat;

  Dynamic_ids *ignore_server_ids;

  ulong master_id;
  /*
    to hold checksum alg in use until IO thread has received FD.
    Initialized to novalue, then set to the queried from master
    @@global.binlog_checksum and deactivated once FD has been received.
  */
  uint8 checksum_alg_before_fd;
  ulong retry_count;
  char master_uuid[UUID_LENGTH+1];
  char bind_addr[HOSTNAME_LENGTH+1];

  ulong master_gtid_mode;

  int mi_init_info();
  void end_info();
  int flush_info(bool force= FALSE);
  void set_relay_log_info(Relay_log_info *info);

  bool shall_ignore_server_id(ulong s_id);

  virtual ~Master_info();

protected:
  char master_log_name[FN_REFLEN];
  my_off_t master_log_pos;

public:
  void clear_in_memory_info(bool all);

  inline const char* get_master_log_name() { return master_log_name; }
  inline ulonglong get_master_log_pos() { return master_log_pos; }
  inline void set_master_log_name(const char *log_file_name)
  {
     strmake(master_log_name, log_file_name, sizeof(master_log_name) - 1);
  }
  inline void set_master_log_pos(ulonglong log_pos)
  {
    master_log_pos= log_pos;
  }
  inline const char* get_io_rpl_log_name()
  {
    return (master_log_name[0] ? master_log_name : "FIRST");
  }
  static size_t get_number_info_mi_fields();

  bool is_auto_position()
  {
    return auto_position;
  }

  void set_auto_position(bool auto_position_param)
  {
    auto_position= auto_position_param;
  }

private:
  /**
    Format_description_log_event for events received from the master
    by the IO thread and written to the tail of the relay log.

    Use patterns:
     - Created when the IO thread starts and destroyed when the IO
       thread stops.
     - Updated when the IO thread receives a
       Format_description_log_event.
     - Accessed by the IO thread when it de-serializes events (e.g.,
       rotate events, Gtid events).
     - Written by the IO thread to the new relay log on every rotation.
     - Written by a client that executes FLUSH LOGS to the new relay
       log on every rotation.

    Locks:
    All access is protected by Master_info::data_lock.
  */
  Format_description_log_event *mi_description_event;
public:
  Format_description_log_event *get_mi_description_event()
  {
    mysql_mutex_assert_owner(&data_lock);
    return mi_description_event;
  }
  void set_mi_description_event(Format_description_log_event *fdle)
  {
    mysql_mutex_assert_owner(&data_lock);
    delete mi_description_event;
    mi_description_event= fdle;
  }

private:
  void init_master_log_pos();

  bool read_info(Rpl_info_handler *from);
  bool write_info(Rpl_info_handler *to);

  bool auto_position;

  Master_info(
#ifdef HAVE_PSI_INTERFACE
              PSI_mutex_key *param_key_info_run_lock,
              PSI_mutex_key *param_key_info_data_lock,
              PSI_mutex_key *param_key_info_sleep_lock,
              PSI_mutex_key *param_key_info_data_cond,
              PSI_mutex_key *param_key_info_start_cond,
              PSI_mutex_key *param_key_info_stop_cond,
              PSI_mutex_key *param_key_info_sleep_cond,
#endif
              uint param_id
             );

  Master_info(const Master_info& info);
  Master_info& operator=(const Master_info& info);
};
int change_master_server_id_cmp(ulong *id1, ulong *id2);

#endif /* HAVE_REPLICATION */
#endif /* RPL_MI_H */
