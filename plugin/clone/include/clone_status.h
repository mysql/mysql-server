/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
@file clone/include/clone_status.h
Clone Plugin: Client Status Interface

*/

#ifndef CLONE_STATUS_H
#define CLONE_STATUS_H

#include <mysql/components/services/pfs_plugin_table_service.h>
#include <array>
#include "my_systime.h"
#include "plugin/clone/include/clone.h"

THD *thd_get_current_thd();

/* Namespace for all clone data types */
namespace myclone {

/** Log the current error message.
@param[in,out]	thd		current session THD
@param[in]	is_client	true, if called by client
@param[in]	error		error code
@param[in]	message_start	start error message string */
void log_error(THD *thd, bool is_client, int32_t error,
               const char *message_start);

/** Abstract base class for clone PFS tables. */
class Table_pfs {
 public:
  /** Constructor.
  @param[in]	num_rows	total number of rows in table. */
  Table_pfs(uint32_t num_rows);

  /** Destructor. */
  virtual ~Table_pfs() = default;

  /** Read column at index of current row. Implementation
  is specific to table.
   @param[out]	field	column value
   @param[in]	index	column position within row
   @return error code. */
  virtual int read_column_value(PSI_field *field, uint32_t index) = 0;

  /** Initialize the table.
  @return plugin table error code. */
  virtual int rnd_init() = 0;

  /** Initialize position for table.
  @param[in]	id	clone ID. */
  void init_position(uint32_t id) {
    m_position = 0;
    m_empty = (id == 0);
  }

  /** Set cursor to next record.
  @return plugin table error code. */
  int rnd_next() {
    /* Table is empty. */
    if (is_empty()) {
      return (PFS_HA_ERR_END_OF_FILE);
    }
    ++m_position;
    if (m_position <= m_rows) {
      return (0);
    }
    /* All rows are read. */
    assert(m_position == m_rows + 1);
    return (PFS_HA_ERR_END_OF_FILE);
  }

  /** Set cursor to current position: currently no op.
  @return plugin table error code. */
  int rnd_pos() {
    if (m_position > 0 && m_position <= m_rows) {
      return (0);
    }
    return (PFS_HA_ERR_END_OF_FILE);
  }

  /** Reset cursor position to beginning. */
  void reset_pos() { m_position = 0; }

  /** Close the table. */
  void close() { m_position = 0; }

  /* @return address of current position. PFS needs it to set
  the position for proxy table. */
  uint32_t *get_position_address() { return (&m_position); }

  /** Acquire service handles and create proxy tables
  @return false if successful. */
  static bool acquire_services();

  /** Release service handles and delete proxy tables. */
  static void release_services();

  /** Initialize all stage and state names. */
  static void init_state_names();

  /** Clone States. */
  enum Clone_state : uint32_t {
    STATE_NONE = 0,
    STATE_STARTED,
    STATE_SUCCESS,
    STATE_FAILED,
    NUM_STATES
  };
  /** All clone states */
  static std::array<const char *, NUM_STATES> s_state_names;

  /** Clone Stages. Keep in consecutive order as we use it as index. */
  enum Clone_stage : uint32_t {
    STAGE_NONE = 0,
    STAGE_CLEANUP = 1,
    STAGE_FILE_COPY = 2,
    STAGE_PAGE_COPY = 3,
    STAGE_REDO_COPY = 4,
    STAGE_FILE_SYNC = 5,
    STAGE_RESTART = 6,
    STAGE_RECOVERY = 7,
    NUM_STAGES = 8
  };

  /** All clone Stages. */
  static std::array<const char *, NUM_STAGES> s_stage_names;

 protected:
  /** @return Current cursor position. */
  uint32_t get_position() const { return (m_position); }

  /** @return Proxy table share reference. */
  PFS_engine_table_share_proxy *get_proxy_share() { return (&m_pfs_table); }

  /** @return true, if no data in table. */
  bool is_empty() const { return (m_empty); }

 private:
  /** Create PFS proxy tables.
  @return error code. */
  static int create_proxy_tables();

  /** Drop PFS proxy tables. */
  static void drop_proxy_tables();

 private:
  /** Number of rows in table. */
  uint32_t m_rows;

  /** Current position of the cursor. */
  uint32_t m_position;

  /** If the table is empty. */
  bool m_empty;

  /** Proxy table defined in plugin to register callbacks with PFS. */
  PFS_engine_table_share_proxy m_pfs_table;
};

const char g_local_string[] = "LOCAL INSTANCE";

class Status_pfs : public Table_pfs {
 public:
  /* Constructor. */
  Status_pfs();

  /** Read column at specific index of current row.
   @param[out]	field	column value
   @param[in]	index	column position within row
   @return error code. */
  int read_column_value(PSI_field *field, uint32_t index) override;

  /** Initialize the table.
  @return plugin table error code. */
  int rnd_init() override;

  /** Number of rows in status table. Currently we keep last clone status. */
  static const uint32_t S_NUM_ROWS = 1;

  /** POD for the progress data. */
  struct Data {
    /** Read data from status file. */
    void read();

    /** Extract and write recovery information. */
    void recover();

    /** Write data to status file.
    @param[in]	write_error	write error information. */
    void write(bool write_error);

    /* @return true, if destination is current database. */
    bool is_local() const {
      return (0 == strncmp(&m_destination[0], &g_local_string[0],
                           sizeof(m_destination)));
    }

    /** Set PFS table data while starting Clone operation.
    @param[in]	id		clone ID
    @param[in]	thd		session THD
    @param[in]	host		clone source host
    @param[in]	port		clone source port
    @param[in]	destination	clone destination directory or host */
    void begin(uint32_t id, THD *thd, const char *host, uint32_t port,
               const char *destination) {
      m_id = id;
      m_pid = static_cast<uint32_t>(thd_get_thread_id(thd));
      /* Clone from local instance. */
      if (host == nullptr) {
        strncpy(m_source, &g_local_string[0], sizeof(m_source) - 1);
      } else {
        snprintf(m_source, sizeof(m_source) - 1, "%s:%u", host, port);
      }
      /* Clone into local instance. */
      if (destination == nullptr) {
        destination = &g_local_string[0];
      }
      strncpy(m_destination, destination, sizeof(m_destination) - 1);
      m_error_number = 0;
      memset(m_error_mesg, 0, sizeof(m_error_mesg));
      m_binlog_pos = 0;
      memset(m_binlog_file, 0, sizeof(m_binlog_file));
      m_gtid_string.clear();
      m_start_time = my_micro_time();
      m_end_time = 0;
      m_state = STATE_STARTED;
      write(false);
    }

    /** Update PFS table data while ending clone operation.
    @param[in]	err_num		error number
    @param[in]	err_mesg	error message
    @param[in]	provisioning	if we are provisioning current directory. */
    void end(uint32_t err_num, const char *err_mesg, bool provisioning) {
      m_end_time = my_micro_time();
      if (err_num == 0) {
        /* For provisioning, recovery stage is left. */
        if (!provisioning) {
          m_state = Table_pfs::STATE_SUCCESS;
        }
        write(true);
        return;
      }
      m_state = Table_pfs::STATE_FAILED;
      m_error_number = err_num;
      strncpy(m_error_mesg, err_mesg, sizeof(m_error_mesg) - 1);
      write(true);
    }

    /** Update source binlog position consistent with cloned data.
    @param[in]	binlog_file	binary log file name
    @param[in]	position	binary log offset within file */
    void update_binlog_position(const char *binlog_file, uint64_t position) {
      m_binlog_pos = position;
      strncpy(m_binlog_file, binlog_file, sizeof(m_binlog_file) - 1);
    }

    /** Length of variable length character columns. */
    static const size_t S_VAR_COL_LENGTH = 512;

    /** Current State. */
    Clone_state m_state{STATE_NONE};

    /** Clone error number. */
    uint32_t m_error_number{};

    /** Unique identifier in current instance. */
    uint32_t m_id{};

    /** Process List ID. */
    uint32_t m_pid{};

    /** Clone start time. */
    uint64_t m_start_time{};

    /** Clone end time. */
    uint64_t m_end_time{};

    /* Source binary log position. */
    uint64_t m_binlog_pos{};

    /** Clone source. */
    char m_source[S_VAR_COL_LENGTH]{};

    /** Clone destination. */
    char m_destination[S_VAR_COL_LENGTH]{};

    /** Clone error message. */
    char m_error_mesg[S_VAR_COL_LENGTH]{};

    /** Source binary log file name. */
    char m_binlog_file[S_VAR_COL_LENGTH]{};

    /** Clone GTID set */
    std::string m_gtid_string;
  };

 private:
  /** Current status data. */
  Data m_data;
};

class Progress_pfs : public Table_pfs {
 public:
  /* Constructor. */
  Progress_pfs();

  /** Read column at specific index of current row.
   @param[out]	field	column value
   @param[in]	index	column position within row
   @return error code. */
  int read_column_value(PSI_field *field, uint32_t index) override;

  /** Initialize the table.
  @return plugin table error code. */
  int rnd_init() override;

  /** Number of rows in progress table. Therea is one row for each stage. */
  static const uint32_t S_NUM_ROWS = NUM_STAGES - 1;

  /** POD for the progress data. */
  struct Data {
    /** Read data from progress file. */
    void read();

    /** Write data to progress file.
    @@param[in]	data_dir	data directory for write. */
    void write(const char *data_dir);

    /** Get next stage from current.
    @param[in,out]	stage	current/next stage. */
    void next_stage(Clone_stage &stage) {
      auto next_num = static_cast<uint32_t>(stage) + 1;
      auto max_num = static_cast<uint32_t>(NUM_STAGES);
      if (next_num >= max_num) {
        stage = STAGE_NONE;
        return;
      }
      stage = static_cast<Clone_stage>(next_num);
    }

    /** Initialize PFS stage.
    @@param[in]	data_dir	data directory for write. */
    void init_stage(const char *data_dir) {
      m_id = 0;
      m_current_stage = STAGE_NONE;

      /* Clean current stage information. */
      m_data_speed = 0;
      m_network_speed = 0;

      /* Clean all stage specific information. */
      next_stage(m_current_stage);
      while (m_current_stage != STAGE_NONE) {
        /* State */
        m_states[m_current_stage] = STATE_NONE;
        m_threads[m_current_stage] = 0;
        /* Time */
        m_start_time[m_current_stage] = 0;
        m_end_time[m_current_stage] = 0;
        /* Estimates */
        m_estimate[m_current_stage] = 0;
        m_complete[m_current_stage] = 0;
        m_network[m_current_stage] = 0;

        next_stage(m_current_stage);
      }
      write(data_dir);
    }

    /** Set PFS table data while starting Clone a stage.
    @param[in]	id		clone ID
    @@param[in]	data_dir	data directory for write.
    @param[in]	threads		current number of concurrent threads
    @param[in]	estimate	estimated data bytes for stage */
    void begin_stage(uint32_t id, const char *data_dir, uint64_t threads,
                     uint64_t estimate) {
      next_stage(m_current_stage);
      if (m_current_stage == STAGE_NONE) {
        assert(false); /* purecov: inspected */
        return;
      }
      m_states[m_current_stage] = STATE_STARTED;
      m_id = id;
      m_threads[m_current_stage] = threads;

      /* Set time at beginning. */
      m_start_time[m_current_stage] = my_micro_time();
      m_end_time[m_current_stage] = 0;

      /* Reset progress data at the beginning of stage. */
      m_estimate[m_current_stage] = estimate;
      m_complete[m_current_stage] = 0;
      m_network[m_current_stage] = 0;
      m_data_speed = 0;
      m_network_speed = 0;
      write(data_dir);
    }

    /** Set PFS table data while ending a Clone stage.
    @@param[in]	data_dir	data directory for write. */
    void end_stage(bool failed, const char *data_dir) {
      m_end_time[m_current_stage] = my_micro_time();
      m_states[m_current_stage] = failed ? STATE_FAILED : STATE_SUCCESS;
      write(data_dir);
    }

    /** Update data and network consumed.
    @param[in]	data		data bytes transferred
    @param[in]	network		network bytes transferred
    @param[in]	data_speed	data transfer speed in bytes/sec
    @param[in]	net_speed	network transfer speed in bytes/sec
    @param[in]	num_workers	number of worker threads */
    void update_data(uint64_t data, uint64_t network, uint32_t data_speed,
                     uint32_t net_speed, uint32_t num_workers) {
      m_complete[m_current_stage] += data;
      m_network[m_current_stage] += network;
      m_data_speed = data_speed;
      m_network_speed = net_speed;
      m_threads[m_current_stage] = num_workers + 1;
    }

    /** Current progress stage. */
    Clone_stage m_current_stage{STAGE_NONE};

    /** State information for all stages. */
    Clone_state m_states[NUM_STAGES];

    /** Unique identifier in current instance. */
    uint32_t m_id{};

    /** Current data transfer rate. */
    uint32_t m_data_speed{};

    /** Current network transfer rate. */
    uint32_t m_network_speed{};

    /** Number of active threads. */
    uint32_t m_threads[NUM_STAGES]{};

    /** Stage start time. */
    uint64_t m_start_time[NUM_STAGES]{};

    /** Stage end time. */
    uint64_t m_end_time[NUM_STAGES]{};

    /** Estimated bytes for all stages. */
    uint64_t m_estimate[NUM_STAGES]{};

    /** Completed bytes for all stages. */
    uint64_t m_complete[NUM_STAGES]{};

    /** Completed network bytes for all stages. */
    uint64_t m_network[NUM_STAGES]{};
  };

 private:
  /** Current progress data. */
  Data m_data;
};
}  // namespace myclone

#endif /* CLONE_STATUS_H */
