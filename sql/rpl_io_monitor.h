/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_IO_MONITOR_H
#define RPL_IO_MONITOR_H

#include <atomic>
#include <vector>

#include "sql/rpl_async_conn_failover_table_operations.h"
#include "sql/rpl_mysql_connect.h"

class Master_info;
class THD;
struct TABLE;

/* Mysql_connection class object */
using MYSQL_CONN_PTR = std::unique_ptr<Mysql_connection>;

/* mysql connection map key <channel, host, port> */
using MYSQL_CONN_KEY = std::tuple<std::string, std::string, uint>;

/**
  Connection managed tuple <channel, host, port, network_namespace, weight,
                            managed_name, primary_weight, secondary_weight>
*/
using SENDER_CONN_MERGE_TUPLE =
    std::tuple<std::string, std::string, uint, std::string, uint, std::string,
               uint, uint>;

/* Sql queries tag list */
enum class enum_sql_query_tag : uint {
  CONFIG_MODE_QUORUM_MONITOR = 0,
  CONFIG_MODE_QUORUM_IO,
  GR_MEMBER_ALL_DETAILS,
  GR_MEMBER_ALL_DETAILS_FETCH_FOR_57,
  QUERY_SERVER_SELECT_ONE
};

/* Configuration mode quorum status */
enum class enum_conf_mode_quorum_status : uint {
  MANAGED_GR_HAS_QUORUM = 1,
  MANAGED_GR_HAS_ERROR,
  NOT_MANAGED,
};

struct thread_state {
  /**
   * @enum  thread_state_enum
   * @brief Maintains thread status
   */
  enum thread_state_enum {
    THREAD_NONE = 0, /**< THREAD_NOT_CREATED */
    THREAD_CREATED,  /**< THREAD_CREATED */
    THREAD_INIT,     /**< THREAD_INIT */

    THREAD_RUNNING, /**< THREAD_RUNNING */

    THREAD_TERMINATED, /**< THREAD_EXIT */
    THREAD_END         /**< END OF ENUM */
  };

 private:
  thread_state_enum thread_state_var;

 public:
  thread_state() : thread_state_var(thread_state_enum::THREAD_NONE) {}

  void set_running() { thread_state_var = thread_state_enum::THREAD_RUNNING; }

  void set_terminated() {
    thread_state_var = thread_state_enum::THREAD_TERMINATED;
  }

  void set_initialized() { thread_state_var = thread_state_enum::THREAD_INIT; }

  void set_created() { thread_state_var = thread_state_enum::THREAD_CREATED; }

  bool is_initialized() const {
    return ((thread_state_var >= thread_state_enum::THREAD_INIT) &&
            (thread_state_var < thread_state_enum::THREAD_TERMINATED));
  }

  bool is_running() const {
    return thread_state_var == thread_state_enum::THREAD_RUNNING;
  }

  bool is_alive_not_running() const {
    return thread_state_var < thread_state_enum::THREAD_RUNNING;
  }

  bool is_thread_alive() const {
    return ((thread_state_var >= thread_state_enum::THREAD_CREATED) &&
            (thread_state_var < thread_state_enum::THREAD_TERMINATED));
  }

  bool is_thread_dead() const { return !is_thread_alive(); }
};

/**
  @class Source_IO_monitor
  Class that contains functionality to monitor group member's state, role and
  quorum changes on all the potential senders in the Sender List, and if it
  finds any changes or lost quorum it does automatic update of the sender list.
*/
class Source_IO_monitor {
 public:
  /* Source_IO_monitor class constructor */
  Source_IO_monitor();

  /* Source_IO_monitor class destructor */
  virtual ~Source_IO_monitor();

  /* Source_IO_monitor class copy constructor (restricted) */
  Source_IO_monitor(const Source_IO_monitor &) = delete;

  /* Source_IO_monitor class assignment operator (restricted) */
  Source_IO_monitor &operator=(const Source_IO_monitor &) = delete;

  /**
    Fetch Source_IO_monitor class instance.

    @return Pointer to the Source_IO_monitor class instance.
  */
  static Source_IO_monitor *get_instance();

  /**
    Creates and launches new Monitor IO thread.

    @param[in] thread_key  instrumentation key

    @returns false in case of success, or true otherwise.
  */
  bool launch_monitoring_process(PSI_thread_key thread_key);

  /**
    Terminate the Monitor IO thread.

    @returns 0 in case of success, or 1 otherwise.
  */
  int terminate_monitoring_process();

  /**
    Check if Monitor IO thread is killed.

    @param[in] thd  The thread.
    @param[in] mi   the pointer to the Master_info object.
    @return true if yes, false otherwise
  */
  bool is_monitor_killed(THD *thd, Master_info *mi);

  /**
    Gets the delay time between each iteration where it fetches group details.

    @return the delay time in seconds.
  */
  uint get_monitoring_wait();

  /**
    Gets the status of monitor IO thread whether its running.

    @return true if monitor IO thread running, false otherwise.
  */
  bool is_monitoring_process_running();

  /**
    It gets stored senders details for channel from
    replication_asynchronous_connection_failover table.

    @param[in] channel_name  the channel from which get the senders

    @returns std::tuple<bool, List_of_Tuple> where each element has
             following meaning:

             first element of tuple is function return value and determines:
             false  Successful
             true   Error

             second element of the tuple contains following details in tuple
              <channel, host, port, network_namespace, weight,
               managed_name, primary_weight, secondary_weight>
  */
  std::tuple<bool, std::vector<SENDER_CONN_MERGE_TUPLE>> get_senders_details(
      const std::string &channel_name);

  /**
    The function started by Monitor IO thread which does monitor group member's
    state, role and quorum changes on all the potential senders in the Sender
    List, and if it finds any changes or lost quorum it does automatic update
    of the sender list. The thread runs in infinite loop till its not killed.
  */
  void source_monitor_handler();

  /**
    Sets the delay between each iteration where it fetches group details.

    @param[in] wait_time  the delay time in seconds to set.
  */
  void set_monitoring_wait(uint wait_time);

  /**
    Gets the sql query string.

    @param[in] qtag  the query to fetch.

    @return the sql query string.
  */
  std::string get_query(enum_sql_query_tag qtag);

 private:
  /* The Monitor IO thread THD object. */
  THD *m_monitor_thd{nullptr};

  /* The flag to determine if Monitor IO thread aborted */
  bool m_abort_monitor{false};

  /* The delay time in seconds */
  uint m_retry_monitor_wait{5};

  /* monitor IO thread lock for thread synchronization */
  mysql_mutex_t m_run_lock;

  /* monitor IO thread condition variable for thread wait. */
  mysql_cond_t m_run_cond;

  /* monitor IO thread variable used for THD creation. */
  my_thread_handle m_th;

  /* Monitor IO thread state */
  thread_state m_monitor_thd_state;

  bool m_primary_lost_contact_with_majority_warning_logged{false};

  /* Sql queries result column number */
  enum enum_res_col {
    COL_GROUP_NAME = 0,
    COL_HOST,
    COL_PORT,
    COL_STATE,
    COL_ROLE,
  };

  /**
    It gets stored senders details for channel from
    replication_asynchronous_connection_failover table, and then connects
    to it. It also stores client connection object to all the connected
    stores.
    Then it gets group membership list from each sender.

    @param[in] thd  The thread.

    @return 0 if success, error otherwise.
  */
  int sync_senders_details(THD *thd);

  /**
    It gets stored senders details for channel from
    replication_asynchronous_connection_failover table, and then connects
    to it. It also stores client connection object to all the connected
    stores.

    @param[in] thd  The thread.
    @param[in] channel_name The channel name.

    @return false if success, true otherwise.
  */
  int connect_senders(THD *thd, const std::string &channel_name);

  /**
    It connects to server and runs a simple query.

    @param[in] thd   The thread.
    @param[in] mi    The pointer to the Master_info object.
    @param[in] conn_detail  std::tuple containing <channel, host, port,
                             network_namespace, weight, group_name>

    @return true on success
            false on failure like unable to connect or query fails
  */
  bool check_connection_and_run_query(THD *thd, Master_info *mi,
                                      RPL_FAILOVER_SOURCE_TUPLE &conn_detail);

  /**
    It connects to each stored sender in connect_senders() and check for quorum
    and group replication plugin enabled. It gets group membership list if
    group replication plugin is enabled and its also has quorum.

    @param[in] thd   The thread.
    @param[in] mi    The pointer to the Master_info object.
    @param[in] conn  The Mysql_connection class object to query remote source.
    @param[in] source_conn_detail  std::tuple containing <channel, host, port,
                                   network_namespace, weight, group_name,
                                   primary_weight, secondary_weight>.
    @param[out] group_membership_detail  std::tuple containing <channel, host,
                                         port, network_namespace, weight,
                                         group_name>
    @param[out] curr_highest_group_weight the highest weight of the source for
                                          the group
    @param[out] curr_conn_weight          weight for current connected sender

    @returns std::tuple<int, uint, bool, bool,
                        std::tuple<std::string, std::string, uint>> where each
             element has following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple determine if the current connected member
                            through asynchronous channel has changed the group.

              third element of tuple determine if the current connected member
                            through asynchronous channel has lost quorum.

              fourth element of tuple is also a tuple containing <channel, host,
                             port> of member who lost quorum. It is only useful
                             when fourth element of returned tuple is true.
  */
  std::tuple<int, bool, bool, std::tuple<std::string, std::string, uint>>
  get_online_members(
      THD *thd, Master_info *mi, const Mysql_connection *conn,
      SENDER_CONN_MERGE_TUPLE source_conn_detail,
      std::vector<RPL_FAILOVER_SOURCE_TUPLE> &group_membership_detail,
      uint &curr_highest_group_weight, uint &curr_conn_weight);

  /**
    Store gathered membership details to
    replication_asynchronous_connection_failover table.

    @param[in] channel_name        The managed channel for which failover
                                   is enabled.
    @param[in] managed_name        The group name UID value of the group.
    @param[in] source_conn_list    The list of std::tuple containing <channel,
                                   host, port, network_namespace, weight,
                                   managed_name>.

    @return false if success, true otherwise.
  */
  int save_group_members(
      std::string channel_name, std::string managed_name,
      std::vector<RPL_FAILOVER_SOURCE_TUPLE> &source_conn_list);

  /**
    Delete provided row to the table with commit.

    @param[in]  table_op     The Rpl_sys_table_access class object.
    @param[in]  table        The table object.
    @param[in]  field_name   The name of column/field of the table.
    @param[in]  conn_detail  std::tuple containing <channel, host, port>

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> delete_rows(
      Rpl_sys_table_access &table_op, TABLE *table,
      std::vector<std::string> field_name,
      std::tuple<std::string, std::string, uint> conn_detail);

  /**
    Insert provided row to the table with commit.

    @param[in]  table_op     The Rpl_sys_table_access class object.
    @param[in]  table        The table object.
    @param[in]  field_name   The name of column/field of the table.
    @param[in]  conn_detail  std::tuple containing <channel, host, port,
                             network_namespace, weight, group_name>

    @returns std::tuple<bool, std::string> where each element has
             following meaning:

              first element of tuple is function return value and determines:
                false  Successful
                true   Error

              second element of tuple is error message.
  */
  std::tuple<bool, std::string> write_rows(
      Rpl_sys_table_access &table_op, TABLE *table,
      std::vector<std::string> field_name,
      RPL_FAILOVER_SOURCE_TUPLE conn_detail);

  /**
    Checks if primary member has lost contact with majority

    @return status
      @retval true  primary member has lost contact with majority
      @retval false otherwise
  */
  bool has_primary_lost_contact_with_majority();

  /**
    Gets the Json key for primary weight for the Configuration column of
    replication_asynchronous_connection_failover_managed table.

    @return the Json key for primary weight for the Configuration column of
            replication_asynchronous_connection_failover_managed table.
  */
  const char *primary_weight_str() { return "Primary_weight"; }

  /**
    Gets the Json key for secondary weight for the Configuration column of
    replication_asynchronous_connection_failover_managed table.

    @return the Json key for secondary weight for the Configuration column of
            replication_asynchronous_connection_failover_managed table.
  */
  const char *secondary_weight_str() { return "Secondary_weight"; }
};
#endif /* RPL_IO_MONITOR_H */
