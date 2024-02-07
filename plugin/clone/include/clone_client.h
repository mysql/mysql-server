/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

/**
@file clone/include/clone_client.h
Clone Plugin: Client Interface

*/

#ifndef CLONE_CLIENT_H
#define CLONE_CLIENT_H

#include "plugin/clone/include/clone.h"
#include "plugin/clone/include/clone_hton.h"
#include "plugin/clone/include/clone_status.h"

#include "mysql/psi/mysql_thread.h"

#include <array>
#include <atomic>
#include <thread>
#include <vector>

/* Namespace for all clone data types */
namespace myclone {

using Clock = std::chrono::steady_clock;
using Time_Point = std::chrono::time_point<Clock>;

using Time_Msec = std::chrono::milliseconds;
using Time_Sec = std::chrono::seconds;
using Time_Min = std::chrono::minutes;

struct Thread_Info {
  /** Default constructor */
  Thread_Info() = default;

  /** Copy constructor needed for std::vector. */
  Thread_Info(const Thread_Info &) { reset(); } /* purecov: inspected */

  /** Reset transferred data bytes. */
  void reset() {
    m_last_update = Clock::now();
    m_last_data_bytes = 0;
    m_last_network_bytes = 0;

    m_data_bytes.store(0);
    m_network_bytes.store(0);
  }

  /** Update transferred data bytes.
  @param[in]	data_bytes	data bytes transferred
  @param[in]	net_bytes	network bytes transferred */
  void update(uint64_t data_bytes, uint64_t net_bytes) {
    m_data_bytes.fetch_add(data_bytes);
    m_network_bytes.fetch_add(net_bytes);
  }

  /** Calculate the expected time for transfer based on target.
  @param[in]	current	current number of transferred data bytes
  @param[in]	prev	previous number of transferred data bytes
  @param[in]	target	target data transfer rate in bytes per second
  @return expected time in milliseconds. */
  uint64_t get_target_time(uint64_t current, uint64_t prev, uint64_t target);

  /** Check target transfer speed and throttle if needed. The thread sleeps
  for appropriate time if the current transfer rate is more than target.
  @param[in]	data_target	target data bytes transfer per second
  @param[in]	net_target	target network bytes transfer per second */
  void throttle(uint64_t data_target, uint64_t net_target);

  /** Data transfer throttle interval */
  Time_Msec m_interval{100};

  /** Current thread */
  std::thread m_thread;

  /** Last time information was updated. */
  Time_Point m_last_update;

  /** Data bytes at last update. */
  uint64_t m_last_data_bytes{};

  /** Network bytes at last update. */
  uint64_t m_last_network_bytes{};

  /** Total amount of data transferred. */
  std::atomic<uint64_t> m_data_bytes;

  /** Total amount of network bytes transferred. The value differs
  from data as we use compression in network layer. */
  std::atomic<uint64_t> m_network_bytes;
};

/** Thread information vector. */
using Thread_Vector = std::vector<Thread_Info>;

/** Maximum size of history data */
const size_t STAT_HISTORY_SIZE = 16;

/** Auto tuning information for threads. */
struct Thread_Tune_Auto {
  /** Auto tuning state */
  enum class State { INIT, ACTIVE, DONE };

  /** Reset to initial state. */
  void reset() {
    m_prev_number = 0;
    m_next_number = 0;
    m_cur_number = 0;
    m_prev_speed = 0;
    m_last_step_speed = 0;
    m_prev_history_index = 0;
    m_state = State::INIT;
  }

  /** Statistics history interval for tuning. */
  const uint64_t m_history_interval{5};

  /** Number of threads to increase in each step. */
  const uint64_t m_step{4};

  /* Previous number of threads. */
  uint32_t m_prev_number{};

  /** Next target number of threads. */
  uint32_t m_next_number{};

  /** Current number of threads. */
  uint32_t m_cur_number{};

  /** Average data transfer MB/sec */
  uint64_t m_prev_speed{};

  /** Average data transfer in last step MB/sec */
  uint64_t m_last_step_speed{};

  /* Saved history index on last tuning. */
  uint64_t m_prev_history_index{};

  /** Current tuning state. */
  State m_state{State::INIT};
};

/** Client data transfer statistics. */
class Client_Stat {
 public:
  /** Update statistics data.
  @param[in]	reset		reset all previous history
  @param[in]	threads		all concurrent thread information
  @param[in]	num_workers	current number of worker threads */
  void update(bool reset, const Thread_Vector &threads, uint32_t num_workers);

  /** Tune total number of threads based on stat
  @param[in]	num_threads	current number of active threads
  @param[in]	max_threads	maximum number of threads
  @return suggested number of threads. */
  uint32_t get_tuned_thread_number(uint32_t num_threads, uint32_t max_threads);

  /** Get target speed, in case user has specified limits.
  @param[out]	data_speed	target data transfer in bytes/sec
  @param[out]	net_speed	target network transfer in bytes/sec */
  void get_target(uint64_t &data_speed, uint64_t &net_speed) const {
    data_speed = m_target_data_speed.load();
    net_speed = m_target_network_speed.load();
  }

  /** Initialize target speed read by all threads. Adjusted later based on
  maximum bandwidth threads. Zero implies unlimited bandwidth. */
  void init_target() {
    m_target_data_speed.store(0);
    m_target_network_speed.store(0);
  }

  /** Save finished byte stat when thread info is released. It is
  used during clone restart after network failure.
  @param[in]	data_bytes	data bytes to save
  @param[in]	net_bytes	network bytes to save */
  void save_at_exit(uint64_t data_bytes, uint64_t net_bytes) {
    m_finished_data_bytes += data_bytes;
    m_finished_network_bytes += net_bytes;
  }

  /** Finish automatic tuning for spawning threads. */
  void finish_tuning() { m_tune.m_state = Thread_Tune_Auto::State::DONE; }

  /** Reset history elements.
  @param[in]	init	true, if called during initialization */
  void reset_history(bool init);

 private:
  /** Calculate target for each task based on current performance.
  @param[in]	target_speed	overall target speed in bytes per second
  @param[in]	current_speed	overall current speed in bytes per second
  @param[in]	current_target	current target for a task in bytes per second
  @param[in]	num_tasks	number of clone tasks
  @return target for a task in bytes per second. */
  uint64_t task_target(uint64_t target_speed, uint64_t current_speed,
                       uint64_t current_target, uint32_t num_tasks);

  /** Set target bandwidth for data and network per thread.
  @param[in]	num_workers	current number of worker threads
  @param[in]	is_reset	if called during stage reset
  @param[in]	data_speed	current data speed in bytes per second
  @param[in]	net_speed	current network speed in bytes per second */
  void set_target_bandwidth(uint32_t num_workers, bool is_reset,
                            uint64_t data_speed, uint64_t net_speed);

  /** @return true if bandwidth limit is already reached. */
  bool is_bandwidth_saturated();

  /** @return true if tuning has improved performance.
  @param[in]	num_threads	current number of threads */
  bool tune_has_improved(uint32_t num_threads);

  /* Set next target number of threads
  @param[in]	num_threads	current number of threads
  @param[in]	max_threads	maximum number of threads */
  void tune_set_target(uint32_t num_threads, uint32_t max_threads);

 private:
  /** Statistics update interval - 1 sec*/
  const Time_Msec m_interval{1000};

  /** Minimum data transfer rate per task - 1M */
  const uint64_t m_minimum_speed = 1048576;

  /* If stat elements are initialized. */
  bool m_initialized{false};

  /** Starting point for clone data transfer. */
  Time_Point m_start_time;

  /** Last evaluation time */
  Time_Point m_eval_time;

  /** Data transferred at last evaluation time. */
  uint64_t m_eval_data_bytes{};

  /** All data bytes transferred by threads already finished. */
  uint64_t m_finished_data_bytes{};

  /** Network bytes transferred at last evaluation time. */
  uint64_t m_eval_network_bytes{};

  /** All data bytes transferred by threads already finished. */
  uint64_t m_finished_network_bytes{};

  /** Network speed history. */
  std::array<uint64_t, STAT_HISTORY_SIZE> m_network_speed_history{};

  /** Data speed history. */
  std::array<uint64_t, STAT_HISTORY_SIZE> m_data_speed_history{};

  /** Current index for history data. */
  size_t m_current_history_index{};

  /** Target Network bytes to be transferred per thread per second. */
  std::atomic<uint64_t> m_target_network_speed;

  /** Target data bytes to be transferred per thread per second. */
  std::atomic<uint64_t> m_target_data_speed;

  /** Thread auto tuning state and information. */
  Thread_Tune_Auto m_tune;
};

/* Shared client information for multi threaded clone */
struct Client_Share {
  /** Construct clone client share. Initialize storage handle.
  @param[in]	host	remote host IP address
  @param[in]	port	remote server port
  @param[in]	user	remote user name
  @param[in]	passwd	remote user's password
  @param[in]	dir	target data directory for clone
  @param[in]	mode	client SSL mode */
  Client_Share(const char *host, const uint port, const char *user,
               const char *passwd, const char *dir, int mode)
      : m_host(host),
        m_port(port),
        m_user(user),
        m_passwd(passwd),
        m_data_dir(dir),
        m_ssl_mode(mode),
        m_max_concurrency(clone_max_concurrency),
        m_protocol_version(CLONE_PROTOCOL_VERSION) {
    m_storage_vec.reserve(MAX_CLONE_STORAGE_ENGINE);
    m_threads.resize(m_max_concurrency);
    assert(m_max_concurrency > 0);
    m_stat.init_target();
  }

  /** Remote host name */
  const char *m_host;

  /** Remote port */
  const uint32_t m_port;

  /** Remote user name */
  const char *m_user;

  /** Remote user password */
  const char *m_passwd;

  /** Cloned database directory */
  const char *m_data_dir;

  /** Client SSL mode */
  const int m_ssl_mode;

  /** Maximum number of concurrent threads for current operation. */
  const uint32_t m_max_concurrency;

  /** Negotiated protocol version */
  uint32_t m_protocol_version;

  /** Clone storage vector */
  Storage_Vector m_storage_vec;

  /** Thread vector for multi threaded clone. */
  Thread_Vector m_threads;

  /** Data transfer statistics. */
  Client_Stat m_stat;
};

/** Auxiliary connection to send ACK */
struct Client_Aux {
  /** Initialize members */
  void reset() {
    m_buffer = nullptr;
    m_buf_len = 0;
    m_cur_index = 0;
    m_error = 0;
  }

  /** Clone remote client connection */
  MYSQL *m_conn;

  /** ACK descriptor buffer */
  const uchar *m_buffer;

  /** ACK descriptor length */
  size_t m_buf_len;

  /** Current SE index */
  uint m_cur_index;

  /** Saved error */
  int m_error;
};

struct Remote_Parameters {
  /** Remote plugins */
  String_Keys m_plugins;

  /** Remote character sets with collation */
  String_Keys m_charsets;

  /** Remote configurations to validate */
  Key_Values m_configs;

  /** Remote configurations to use */
  Key_Values m_other_configs;

  /** Remote plugins with shared object name */
  Key_Values m_plugins_with_so;
};

/** For Remote Clone, "Clone Client" is created at recipient. It receives data
over network from remote "Clone Server" and applies to Storage Engines. */
class Client {
 public:
  /** Construct clone client. Initialize external handle.
  @param[in,out]	thd		server thread handle
  @param[in]		share		shared client information
  @param[in]		index		current thread index
  @param[in]		is_master	if it is master thread */
  Client(THD *thd, Client_Share *share, uint32_t index, bool is_master);

  /** Destructor: Free the transfer buffer, if created. */
  ~Client();

  /** Check if it is the master client object.
  @return true if this is the master object */
  bool is_master() const { return (m_is_master); }

  /** @return maximum concurrency for current clone operation. */
  uint32_t get_max_concurrency() const {
    assert(m_share->m_max_concurrency > 0);
    return (m_share->m_max_concurrency);
  }

  /** @return current thread information. */
  Thread_Info &get_thread_info() {
    return (m_share->m_threads[m_thread_index]);
  }

  /** Check if network error
  @param[in]	err		error code
  @param[in]	protocol_error	include protocol error
  @return true if network error */
  static bool is_network_error(int err, bool protocol_error);

  /** Update statistics and tune threads
  @param[in]	is_reset	reset statistics
  @return tuned number of worker threads. */
  uint32_t update_stat(bool is_reset);

  /** Check transfer speed and throttle. */
  void check_and_throttle();

  /** Get auxiliary connection information
  @return auxiliary connection data */
  Client_Aux *get_aux() { return (&m_conn_aux); }

  /** Get Shared area for client tasks
  @return shared client data */
  Client_Share *get_share() { return (m_share); }

  /** Get storage handle vector for data transfer.
  @return storage handle vector */
  Storage_Vector &get_storage_vector() { return (m_share->m_storage_vec); }

  /** Get tasks for different SE
  @return task vector */
  Task_Vector &get_task_vector() { return (m_tasks); }

  /** Get external handle for data transfer. This is file
  or buffer for local clone and network socket to remote server
  for remote clone.
  @param[out]	conn	connection handle to remote server
  @return external handle */
  Data_Link *get_data_link(MYSQL *&conn) {
    conn = m_conn;
    return (&m_ext_link);
  }

  /** Get server thread handle
  @return server thread */
  THD *get_thd() { return (m_server_thd); }

  /** Get target clone data directory
  @return data directory */
  const char *get_data_dir() const { return (m_share->m_data_dir); }

  /** Get clone locator for a storage engine at specified index.
  @param[in]	index	locator index
  @param[out]	loc_len	locator length in bytes
  @return storage locator */
  const uchar *get_locator(uint index, uint &loc_len) const {
    assert(index < m_share->m_storage_vec.size());

    loc_len = m_share->m_storage_vec[index].m_loc_len;
    return (m_share->m_storage_vec[index].m_loc);
  }

  /** Get aligned intermediate buffer for transferring data. Allocate,
  when called for first time.
  @param[in]	len	length of allocated buffer
  @return allocated buffer pointer */
  uchar *get_aligned_buffer(uint32_t len);

  /** Limit total memory used for clone transfer buffer.
  @param[in]	buffer_size	configured buffer size
  @return actual buffer size to allocate. */
  uint32_t limit_buffer(uint32_t buffer_size);

  /** Limit spawning initial number of workers if data or network
  bandwidth is small.
  @param[in]	num_workers	planned number of workers to spawn
  @return actual number of workers to be spawned. */
  uint32_t limit_workers(uint32_t num_workers);

  /* Spawn worker threads.
  @param[in]	num_workers	number of worker threads
  @param[in]	func		worker function */
  template <typename F>
  void spawn_workers(uint32_t num_workers, F func) {
    /* Currently we don't reduce the number of threads. */
    if (!is_master() || num_workers <= m_num_active_workers) {
      return;
    }
    auto &thread_vector = m_share->m_threads;

    /* Maximum number of workers are fixed. */
    if (num_workers + 1 > get_max_concurrency()) {
      assert(false); /* purecov: inspected */
      return;
    }

    while (m_num_active_workers < num_workers) {
      ++m_num_active_workers;
      auto &info = thread_vector[m_num_active_workers];
      info.reset();
      try {
        info.m_thread = std::thread(func, m_share, m_num_active_workers);
      } catch (...) {
        /* purecov: begin deadcode */
        auto &stat = m_share->m_stat;
        stat.finish_tuning();

        char info_mesg[64];
        snprintf(info_mesg, sizeof(info_mesg), "Failed to spawn worker: %d",
                 m_num_active_workers);
        LogPluginErr(INFORMATION_LEVEL, ER_CLONE_CLIENT_TRACE, info_mesg);

        --m_num_active_workers;
        break;
        /* purecov: end */
      }
    }
  }

  /** Wait for worker threads to finish. */
  void wait_for_workers();

  /** Get data from remote server and create cloned database by
  applying to storage engines.
  @return error code */
  int clone();

  /** Execute RPC clone command on remote server
  @param[in]	com	RPC command ID
  @param[in]	use_aux	use auxiliary connection
  @return error if not successful */
  int remote_command(Command_RPC com, bool use_aux);

  /** Begin state in PFS table.
  @return error code. */
  int pfs_begin_state();

  /** Change stage in PFS progress table. */
  void pfs_change_stage(uint64_t estimate);

  /** End state in PFS table.
  @param[in]	err_num		error number
  @param[in]	err_mesg	error message */
  void pfs_end_state(uint32_t err_num, const char *err_mesg);

  /** Copy PFS status data safely.
  @param[out]	pfs_data	status data. */
  static void copy_pfs_data(Status_pfs::Data &pfs_data);

  /** Copy PFS progress data safely.
  @param[out]	pfs_data	progress data. */
  static void copy_pfs_data(Progress_pfs::Data &pfs_data);

  /** Update data and network consumed.
  @param[in]	data		data bytes transferred
  @param[in]	network		network bytes transferred
  @param[in]	data_speed	data transfer speed in bytes/sec
  @param[in]	net_speed	network transfer speed in bytes/sec
  @param[in]	num_workers	number of worker threads */
  static void update_pfs_data(uint64_t data, uint64_t network,
                              uint32_t data_speed, uint32_t net_speed,
                              uint32_t num_workers);

  /** Init PFS mutex for table. */
  static void init_pfs();

  /** Destroy PFS mutex for table. */
  static void uninit_pfs();

 private:
  /** Connect to remote server
  @param[in]	is_restart	restarting clone after network failure
  @param[in]	use_aux		establish auxiliary connection
  @return	error code */
  int connect_remote(bool is_restart, bool use_aux);

  /** Initialize storage engine and command buffer.
  @param[in]	mode	initialization mode
  @param[out]	cmd_len	serialized command length
  @return error if initialization fails. */
  int init_storage(enum Ha_clone_mode mode, size_t &cmd_len);

  /** Prepare command buffer for remote RPC
  @param[in]	com	RPC command ID
  @param[out]	buf_len	command buffer length
  @return error if allocation fails */
  int prepare_command_buffer(Command_RPC com, size_t &buf_len);

  /** Serialize the buffer for COM_INIT
  @param[out]	buf_len	length of serialized buffer */
  int serialize_init_cmd(size_t &buf_len);

  /** Serialize the buffer for COM_ACK
  @param[out]	buf_len	length of serialized buffer */
  int serialize_ack_cmd(size_t &buf_len);

  /** Receive and handle response from remote server
  @param[in]	com		RPC command ID
  @param[in]	use_aux		use auxiliary connection
  @return error code */
  int receive_response(Command_RPC com, bool use_aux);

  /** Handle response packet from remote server
  @param[in]	packet		data packet
  @param[in]	length		length of the packet
  @param[in]	in_err		skip if error has occurred
  @param[in]	skip_loc	skip applying locator
  @param[out]	is_last		true if last packet
  @return error code */
  int handle_response(const uchar *packet, size_t length, int in_err,
                      bool skip_loc, bool &is_last);

  /** Handle error and check if needs to exit
  @param[in]	current_err			error number
  @param[in,out]	first_error		first error that has occurred
  @param[in,out]	first_error_time	time for first error in
  milliseconds
  @return true if the caller needs to exit */
  bool handle_error(int current_err, int &first_error,
                    ulonglong &first_error_time);

  /** Validate all remote parameters.
  @return error code */
  int validate_remote_params();

  /** Check if plugin is installed.
  @param[in]	plugin_name	plugin name
  @return true iff installed. */
  bool plugin_is_installed(std::string &plugin_name);

  /**  Check if plugin shared object can be loaded.
  @param[in]	so_name	shared object name
  @return true iff able to load. */
  bool plugin_is_loadable(std::string &so_name);

  /** Extract string from network buffer.
  @param[in,out]	packet	network packet
  @param[in,out]	length	packet length
  @param[out]		str	extracted string
  @return error code */
  int extract_string(const uchar *&packet, size_t &length, String_Key &str);

  /** Extract string from network buffer.
  @param[in,out]	packet	network packet
  @param[in,out]	length	packet length
  @param[out]		keyval	extracted key value pair
  @return error code */
  int extract_key_value(const uchar *&packet, size_t &length,
                        Key_Value &keyval);

  /** Extract and add plugin name from network packet.
  @param[in]	packet	network packet
  @param[in]	length	packet length
  @return error code */
  int add_plugin(const uchar *packet, size_t length);

  /** Extract and add plugin and shared object name from network packet.
  @param[in]	packet	network packet
  @param[in]	length	packet length
  @return error code */
  int add_plugin_with_so(const uchar *packet, size_t length);

  /** Extract and add charset name from network packet.
  @param[in]	packet	network packet
  @param[in]	length	packet length
  @return error code */
  int add_charset(const uchar *packet, size_t length);

  /** Extract and add remote configuration from network packet.
  @param[in]	packet	network packet
  @param[in]	length	packet length
  @param[in]	other	true if additional configuration
  @return error code */
  int add_config(const uchar *packet, size_t length, bool other);

  /** Use additional configurations if sent by donor. */
  void use_other_configs();

  /** Set locators returned by remote server
  @param[in]	buffer	serialized locator information
  @param[in]	length	length of serialized data
  @return error code */
  int set_locators(const uchar *buffer, size_t length);

  /** Apply descriptor returned by remote server
  @param[in]	buffer	serialized data descriptor
  @param[in]	length	length of serialized data
  @return error code */
  int set_descriptor(const uchar *buffer, size_t length);

  /** Extract and set error mesg from remote server
  @param[in]	buffer	Remote error buffer
  @param[in]	length	length of error buffer
  @return error code */
  int set_error(const uchar *buffer, size_t length);

  /** Suspends client thread for the specified time
  @param[in]	wait_time Time in seconds
  @return error code */
  int wait(Time_Sec wait_time);

  /** Check if delay is requested from the user
  @return error code */
  int delay_if_needed();

  /** If PFS table and mutex is initialized. */
  static bool s_pfs_initialized;

 private:
  /** Clone status table data. */
  static Status_pfs::Data s_status_data;

  /** Clone progress table data. */
  static Progress_pfs::Data s_progress_data;

  /** Clone table mutex to protect PFS table data. */
  static mysql_mutex_t s_table_mutex;

  /** Number of concurrent clone clients. */
  static uint32_t s_num_clones;

  /** Time out for connecting back to donor server after network failure. */
  static Time_Sec s_reconnect_timeout;

  /** Interval for attempting re-connect after failure. */
  static Time_Sec s_reconnect_interval;

 private:
  /** Server thread object */
  THD *m_server_thd;

  /** Auxiliary client connection */
  Client_Aux m_conn_aux;

  /** Clone remote client connection */
  MYSQL *m_conn;
  NET_SERVER m_conn_server_extn;

  /** Intermediate buffer for data copy when zero copy is not used. */
  Buffer m_copy_buff;

  /** Buffer holding data for RPC command */
  Buffer m_cmd_buff;

  /** Clone external handle. Data is transferred from
  external handle(network) to storage handle. */
  Data_Link m_ext_link;

  /** If it is the master thread */
  bool m_is_master;

  /** Thread index for multi-threaded clone */
  uint32_t m_thread_index;

  /** Number of active worker tasks. */
  uint32_t m_num_active_workers;

  /** Task IDs for different SE */
  Task_Vector m_tasks;

  /** Storage is initialized */
  bool m_storage_initialized;

  /** Storage is active with locators set */
  bool m_storage_active;

  /** If backup lock is acquired */
  bool m_acquired_backup_lock;

  /** Remote parameters for validation. */
  Remote_Parameters m_parameters;

  /** Shared client information */
  Client_Share *m_share;
};

/** Clone client interface to handle callback from Storage Engine */
class Client_Cbk : public Ha_clone_cbk {
 public:
  /** Construct Callback. Set clone client object.
  @param[in]	clone	clone client object */
  Client_Cbk(Client *clone) : m_clone_client(clone) {}

  /** Get clone object
  @return clone client object */
  Client *get_clone_client() const { return (m_clone_client); }

  /** Clone client file callback: Not used for client.
  @param[in]	from_file	source file descriptor
  @param[in]	len		data length
  @return error code */
  int file_cbk(Ha_clone_file from_file, uint len) override;

  /** Clone client buffer callback: Not used for client.
  @param[in]	from_buffer	source buffer
  @param[in]	buf_len		data length
  @return error code */
  int buffer_cbk(uchar *from_buffer, uint buf_len) override;

  /** Clone client apply callback: Copy data to storage
  engine file from network.
  @param[in]	to_file destination file descriptor
  @return error code */
  int apply_file_cbk(Ha_clone_file to_file) override;

  /** Clone client apply callback: Get data in buffer
  @param[out]  to_buffer  data buffer
  @param[out]  len        data length
  @return error code */
  int apply_buffer_cbk(uchar *&to_buffer, uint &len) override;

 private:
  /** Apply data to local file or buffer.
  @param[in,out]        to_file         destination file
  @param[in]            apply_file      copy data to file
  @param[out]           to_buffer       data buffer
  @param[out]           to_len          data length
  @return error code */
  int apply_cbk(Ha_clone_file to_file, bool apply_file, uchar *&to_buffer,
                uint &to_len);

 private:
  /** Clone client object */
  Client *m_clone_client;
};

}  // namespace myclone

#endif /* CLONE_CLIENT_H */
