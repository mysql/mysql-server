/*****************************************************************************

Copyright (c) 2018, 2019, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

#include "log0meb.h"

#include <ctype.h>
#include <limits.h>
#include <cstring>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "mysql/components/services/dynamic_privilege.h"
#include "mysql/components/services/udf_registration.h"
#include "mysql/plugin.h"
#include "mysql/service_plugin_registry.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"

#include "db0err.h"
#include "dict0dd.h"
#include "ha_innodb.h"
#include "log0log.h"
#include "log0types.h"
#include "os0event.h"
#include "os0file.h"
#include "os0thread-create.h"
#include "sess0sess.h"
#include "srv0srv.h"
#include "sync0sync.h"
#include "ut0mutex.h"
#include "ut0new.h"

namespace meb {
const std::string logmsgpfx("innodb_redo_log_archive: ");
constexpr size_t QUEUE_BLOCK_SIZE = 4096;
constexpr size_t QUEUE_SIZE_MAX = 16384;
mysql_pfs_key_t redo_log_archive_consumer_thread_key;
mysql_pfs_key_t redo_log_archive_file_key;

/** Encapsulates a log block of size QUEUE_BLOCK_SIZE, enqueued by the
    producer, dequeued by the consumer and written into the redo log
    archive file. */
class Block {
 public:
  /** Constructor initializes the byte array to all 0's and sets that the log
      block is not the last log block enqueued (is_final_block = false). */
  Block() { reset(); }

  /** Destructor initializes the byte array to all 0's and sets that the log
      block is not the last log block enqueued (is_final_block = false). */
  ~Block() { reset(); }

  Block &operator=(const Block &) = default;

  /** Resets the data in the log block, initializing the byte array to all 0's
      and sets that the block is not the last log block enqueued
      (is_final_block = false) */
  void reset() {
    memset(m_block, 0, QUEUE_BLOCK_SIZE);
    m_is_final_block = false;
    m_offset = 0;
  }

  /** Get the byte array of size  QUEUE_BLOCK_SIZE associated with this
      object.

      @retval byte[] The byte array of size  QUEUE_BLOCK_SIZE in this
      object. */
  const byte *get_queue_block() const MY_ATTRIBUTE((warn_unused_result)) {
    return m_block;
  }

  /** Copy a log block from the given position inside the input byte array. Note
      that a complete log block is of size OS_FILE_LOG_BLOCK_SIZE. A log block
      could also be of size less than OS_FILE_LOG_BLOCK_SIZE, in which case it
      is overwritten in the next iteration of log writing by InnoDB.

      @param[in] block The byte array containing the log block to be stored in
                       this log block object.
      @param[in] pos The position inside the byte array from which a log block
                     should be copied.

      @retval true if a complete redo log block (multiple of
                   OS_FILE_LOG_BLOCK_SIZE) was copied.
      @retval false otherwise. */
  bool put_log_block(const byte block[], const size_t pos)
      MY_ATTRIBUTE((warn_unused_result)) {
    ut_ad(!full());

    size_t size = log_block_get_data_len(block + pos);

    /* if the incoming log block is empty */
    if (size == 0) {
      return false; /* purecov: inspected */
    }

    memcpy(m_block + m_offset, block + pos, OS_FILE_LOG_BLOCK_SIZE);

    /* If the incoming log block is complete. */
    if (size == OS_FILE_LOG_BLOCK_SIZE) {
      m_offset += size;
      return true;
    }
    return false;
  }

  /** Return the is_final_block flag.

      @retval true if the is_final_block flag is true.
              false if the is_final_block flag is false. */
  bool get_is_final_block() const MY_ATTRIBUTE((warn_unused_result)) {
    return m_is_final_block;
  }

  /** Set the is_final_block flag.

      @param[in] is_final_block the state of the is_final_block flag. */
  void set_is_final_block(const bool is_final_block) {
    m_is_final_block = is_final_block;
  }

  /** Return if the log block is full.

      Condition is (m_offset == QUEUE_BLOCK_SIZE). Since we increment
      m_offset by OS_FILE_LOG_BLOCK_SIZE only, the equivalent condition
      is (m_offset > QUEUE_BLOCK_SIZE - OS_FILE_LOG_BLOCK_SIZE). The
      latter one convinces the fortify tool, that we will never overrun
      the buffer, while the first one is insufficient for the tool.

      @retval true if the log block has QUEUE_BLOCK_SIZE bytes.
      @retval false otherwise. */
  bool full() const MY_ATTRIBUTE((warn_unused_result)) {
    return (m_offset > QUEUE_BLOCK_SIZE - OS_FILE_LOG_BLOCK_SIZE);
  }

 private:
  /** The bytes in the log block object. */
  byte m_block[QUEUE_BLOCK_SIZE];
  /** Offset inside the byte array of the log block object at which the next
      redo log block should be written. */
  size_t m_offset{0};
  /** Flag indicating if this is the last block enqueued by the producer. */
  bool m_is_final_block{false};
};

/** This template class implements a queue that,

    1. Implements a Ring Buffer.
       1.1 The ring buffer can store QUEUE_SIZE_MAX elements.
       1.2 Each element of the ring buffer stores log blocks of size
           QUEUE_BLOCK_SIZE.
    2. Blocks for more data to be enqueued if the queue is empty.
    3. Blocks for data to be dequeued if the queue is full.
    4. Is thread safe. */
template <typename T>
class Queue {
 public:
  /** Create the queue with essential objects. */
  void create() {
    ut_ad(m_enqueue_event == nullptr);
    ut_ad(m_dequeue_event == nullptr);
    ut_ad(m_ring_buffer == nullptr);
    m_front = -1;
    m_rear = -1;
    m_size = 0;
    m_enqueue_event = os_event_create("redo_log_archive_enqueue");
    m_dequeue_event = os_event_create("redo_log_archive_dequeue");
    mutex_create(LATCH_ID_REDO_LOG_ARCHIVE_QUEUE_MUTEX, &m_mutex);
  }

  /** Initialize the ring buffer by allocating memory and initialize the
      indexes of the queue. The initialization is done in a separate
      method so that the ring buffer is allocated memory only when redo
      log archiving is started.
      @param[in] size The size of the ring buffer. */
  void init(const int size) {
    mutex_enter(&m_mutex);
    ut_ad(m_enqueue_event != nullptr);
    ut_ad(m_dequeue_event != nullptr);
    ut_ad(m_ring_buffer == nullptr);

    m_front = -1;
    m_rear = -1;
    m_size = size;

    m_ring_buffer.reset(new T[m_size]);
    mutex_exit(&m_mutex);
  }

  /** Deinitialize the ring buffer by deallocating memory and reset the
      indexes of the queue. */
  void deinit() {
    mutex_enter(&m_mutex);
    m_ring_buffer.reset();
    m_front = -1;
    m_rear = -1;
    m_size = 0;

    while (m_waiting_for_dequeue || m_waiting_for_enqueue) {
      /* purecov: begin inspected */
      if (m_waiting_for_dequeue) os_event_set(m_dequeue_event);
      if (m_waiting_for_enqueue) os_event_set(m_enqueue_event);
      mutex_exit(&m_mutex);
      os_thread_yield();
      mutex_enter(&m_mutex);
      /* purecov: end */
    }
    mutex_exit(&m_mutex);
  }

  /** Delete the queue and its essential objects. */
  void drop() {
    deinit();
    mutex_enter(&m_mutex);
    os_event_destroy(m_enqueue_event);
    os_event_destroy(m_dequeue_event);
    m_enqueue_event = nullptr;
    m_dequeue_event = nullptr;
    mutex_exit(&m_mutex);
    mutex_free(&m_mutex);
  }

  /* Enqueue the log block into the queue and update the indexes in the ring
     buffer.

     @param[in] lb The log block that needs to be enqueued. */
  void enqueue(const T &lb) {
    /* Enter the critical section before enqueuing log blocks to ensure thread
       safe writes. */
    mutex_enter(&m_mutex);

    /* If the queue is full, wait for a dequeue. */
    while ((m_ring_buffer != nullptr) && (m_front == ((m_rear + 1) % m_size))) {
      /* purecov: begin inspected */
      m_waiting_for_dequeue = true;
      mutex_exit(&m_mutex);
      os_event_wait(m_dequeue_event);
      os_event_reset(m_dequeue_event);
      mutex_enter(&m_mutex);
      /* purecov: end */
    }
    m_waiting_for_dequeue = false;

    if (m_ring_buffer != nullptr) {
      /* Perform the insert into the ring buffer and update the indexes. */
      if (m_front == -1) {
        m_front = 0;
      }
      m_rear = (m_rear + 1) % m_size;
      m_ring_buffer[m_rear] = lb;
      os_event_set(m_enqueue_event);
    }

    mutex_exit(&m_mutex);
  }

  /** Dequeue the log block from the queue and update the indexes in the ring
      buffer.

      @param[out] lb The log that was dequeued from the queue. */
  void dequeue(T &lb) {
    /* Enter the critical section before dequeuing log blocks to ensure thread
       safe reads. */
    mutex_enter(&m_mutex);

    /* If the queue is empty wait for an enqueue. */
    while ((m_ring_buffer != nullptr) && (m_front == -1)) {
      m_waiting_for_enqueue = true;
      mutex_exit(&m_mutex);
      os_event_wait(m_enqueue_event);
      os_event_reset(m_enqueue_event);
      mutex_enter(&m_mutex);
    }
    m_waiting_for_enqueue = false;

    if (m_ring_buffer != nullptr) {
      /* Perform the reads from the ring buffer and update the indexes. */
      lb = m_ring_buffer[m_front];
      if (m_front == m_rear) {
        m_front = -1;
        m_rear = -1;
      } else {
        m_front = (m_front + 1) % m_size;
      }
      os_event_set(m_dequeue_event);
    }

    mutex_exit(&m_mutex);
  }

  bool empty() { return m_front == -1; }

 private:
  /** Whether the producer waits for a dequeue event. */
  bool m_waiting_for_dequeue{false};
  /** Whether the consumer waits for an enqueue event. */
  bool m_waiting_for_enqueue{false};
  /** Index representing the front of the ring buffer. */
  int m_front{-1};
  /** Index representing the rear of the ring buffer. */
  int m_rear{-1};
  /** The total number of elements in the ring buffer. */
  int m_size{0};

  /** The buffer containing the contents of the queue. */
  std::unique_ptr<T[]> m_ring_buffer{};

  /** The queue mutex, used to lock the queue during the enqueue and dequeue
      operations, to ensure thread safety. */
  ib_mutex_t m_mutex{};

  /** When the queue is full, enqueue operations wait on this event. When it is
      set, it indicates that a dequeue has happened and there is space in the
      queue.*/
  os_event_t m_dequeue_event{};

  /** When the queue is empty, dequeue operatios wait on this event. When it is
      set, it indicates that a enqueue operation has happened and there is an
      element in the queue, that can be dequeued. */
  os_event_t m_enqueue_event{};
};

/*
  The Guardian cares for a safe report of control stay in a block. If
  constructed as a local stack object, it sets the state as true until
  control leaves the enclosing block. When control leaves the block,
  regardless how, the Guardian is destructed and it resets the state to
  false.

  NOTE: The constructor must be called under the mutex!
  NOTE: The destructor must *NOT* be called under the mutex!
*/
class Guardian {
  /* Reference to a state flag */
  bool *m_state;
  /* Reference to an event. */
  os_event_t *m_event;
  /* Reference to a mutex. */
  ib_mutex_t *m_mutex;

 public:
  /*
    Constructor
    @param[in,out]  state         state flag to set/reset
    @param[in,out]  event         optional event to set when flag is set/reset
    @param[in,out]  mutex         mutex to use for reset
  */
  Guardian(bool *state, os_event_t *event, ib_mutex_t *mutex)
      : m_state(state), m_event(event), m_mutex(mutex) {
    *m_state = true;
    if ((m_event != nullptr) && (*m_event != nullptr)) {
      os_event_set(*m_event);
    }
  }
  ~Guardian() {
    mutex_enter(m_mutex);
    *m_state = false;
    if ((m_event != nullptr) && (*m_event != nullptr)) {
      os_event_set(*m_event);
    }
    mutex_exit(m_mutex);
  }
};

/* The innodb_redo_log_archive_dirs plugin variable value. */
char *redo_log_archive_dirs{};

/*
  Whether redo_log_archive has already been initialized.
  This could be read by redo_log_archive_session_end() even before
  the InnoDB subsystem has started. Hence the atomic qualifier.
*/
static std::atomic<bool> redo_log_archive_initialized{};

/** Mutex to synchronize start and stop of the redo log archiving. */
static ib_mutex_t redo_log_archive_admin_mutex;

/*
  CAUTION: Global variables!

  WARNING: To avoid races, all these variables must be read/written
           under the redo_log_archive_admin_mutex only.
*/

/** Boolean indicating whether the redo log archiving is active. */
static bool redo_log_archive_active{false};

/** Session */
static innodb_session_t *redo_log_archive_session{};
static THD *redo_log_archive_thd{};
static bool redo_log_archive_session_ending{false};

/** Error message recorded during redo log archiving. */
static std::string redo_log_archive_recorded_error{};

/** String containing the redo log archive filename. */
static std::string redo_log_archive_file_pathname{};

/** The file handle to the redo log archive file. */
static pfs_os_file_t redo_log_archive_file_handle{};

/** Whether the consumer thread is running. */
static bool redo_log_archive_consume_running{false};

/** Whether the consumer has completed. */
static bool redo_log_archive_consume_complete{true};

/** Event to inform that the consumer has exited after purging all the queue
    elements */
static os_event_t redo_log_archive_consume_event{};

/**
  Boolean indicating whether to produce queue blocks.

  WARNING: To avoid races, this variable must be read/written
           under the 'log_sys.writer_mutex' only.
           Initialization to 'false' is an exception.
*/
static bool redo_log_archive_produce_blocks{false};

/** Temporary buffer used to build complete redo log blocks of size
    QUEUE_BLOCK_SIZE by the producer. */
static Block redo_log_archive_tmp_block{};

/** Queue into which the producer enqueues redo log blocks of size
    QUEUE_BLOCK_SIZE, and from which the consumer reads redo log
    blocks of size QUEUE_BLOCK_SIZE. */
static Queue<Block> redo_log_archive_queue{};

/* Forward declarations */
static void redo_log_archive_consumer();
static bool terminate_consumer(bool rapid);
static void unregister_udfs();
static bool register_udfs();

/**
  Register a privilege.
  @param[in]      priv_name     privilege name
  @return         status
    @retval       false         success
    @retval       true          failure
*/
bool register_privilege(const char *priv_name) {
  ut_ad(priv_name != nullptr);
  SERVICE_TYPE(registry) *reg = mysql_plugin_registry_acquire();
  if (reg == nullptr) {
    /* purecov: begin inspected */
    LogErr(
        ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
        (logmsgpfx + "mysql_plugin_registry_acquire() returns NULL").c_str());
    return true;
    /* purecov: end */
  }

  bool failed = false;
  // Multiple other implementations use
  // "dynamic_privilege_register.mysql_server"
  my_service<SERVICE_TYPE(dynamic_privilege_register)> reg_priv(
      "dynamic_privilege_register", reg);
  if (reg_priv.is_valid()) {
    if (reg_priv->register_privilege(priv_name, strlen(priv_name))) {
      /* purecov: begin inspected */
      LogErr(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
             (logmsgpfx + "cannot register privilege '" + priv_name + "'")
                 .c_str());
      failed = true;
      /* purecov: end */
    }
  }
  mysql_plugin_registry_release(reg);
  return failed;
}

/**
  Initialize redo log archiving.
  To be called when the InnoDB handlerton is initialized.
*/
void redo_log_archive_init() {
  DBUG_TRACE;
  /* Do not acquire the logwriter mutex at this early stage. */
  redo_log_archive_produce_blocks = false;
  if (redo_log_archive_initialized) {
    redo_log_archive_deinit();
  }
  mutex_create(LATCH_ID_REDO_LOG_ARCHIVE_ADMIN_MUTEX,
               &redo_log_archive_admin_mutex);
  mutex_enter(&redo_log_archive_admin_mutex);
  redo_log_archive_active = false;
  redo_log_archive_session = nullptr;
  redo_log_archive_thd = nullptr;
  redo_log_archive_session_ending = false;
  /* Keep recorded_error */
  redo_log_archive_file_pathname.clear();
  redo_log_archive_consume_complete = true;
  redo_log_archive_file_handle.m_file = OS_FILE_CLOSED;
  redo_log_archive_queue.create();
  bool failed = false;
  if (register_privilege("INNODB_REDO_LOG_ARCHIVE")) {
    failed = true;
  } else if (register_udfs()) {
    failed = true;
  }
  mutex_exit(&redo_log_archive_admin_mutex);
  redo_log_archive_initialized = true;
  if (failed) {
    redo_log_archive_deinit();
  }
}

/**
  Drop potential left-over resources to avoid leaks.

  NOTE: This function must be called under the redo_log_archive_admin_mutex!

  @param[in]      force         whether to drop resorces even if
                                consumer cannot be stopped
  @return         status
    @retval       false         success
    @retval       true          failure
*/
static bool drop_remnants(bool force) {
  DBUG_TRACE;
  /* Do not start if a comsumer is still lurking around. */
  if (redo_log_archive_consume_running) {
    /* purecov: begin inspected */
    if (!redo_log_archive_recorded_error.empty()) {
      redo_log_archive_recorded_error.append("; ");
    }
    redo_log_archive_recorded_error.append(
        "Consumer thread did not terminate properly");
    LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
           (logmsgpfx + redo_log_archive_recorded_error).c_str());
    if (terminate_consumer(/*rapid*/ true) && !force) {
      return true;
    }
    /* purecov: end */
  }
  if (redo_log_archive_consume_event != nullptr) {
    /* purecov: begin inspected */
    os_event_destroy(redo_log_archive_consume_event);
    redo_log_archive_consume_event = nullptr;
    /* purecov: end */
  }
  if (redo_log_archive_file_handle.m_file != OS_FILE_CLOSED) {
    /* purecov: begin inspected */
    os_file_close(redo_log_archive_file_handle);
    redo_log_archive_file_handle.m_file = OS_FILE_CLOSED;
    /* purecov: end */
  }
  if (!redo_log_archive_file_pathname.empty()) {
    /* purecov: begin inspected */
    os_file_delete_if_exists(redo_log_archive_file_key,
                             redo_log_archive_file_pathname.c_str(), NULL);
    /* purecov: end */
  }
  return false;
}

/**
  De-initialize redo log archiving.
  To be called when the InnoDB handlerton is de-initialized.
*/
void redo_log_archive_deinit() {
  DBUG_TRACE;
  if (redo_log_archive_initialized) {
    redo_log_archive_initialized = false;
    /* Do not acquire the logwriter mutex at this late stage. */
    redo_log_archive_produce_blocks = false;
    /* Unregister the UDFs. */
    unregister_udfs();
    mutex_enter(&redo_log_archive_admin_mutex);
    if (redo_log_archive_active) {
      /* purecov: begin inspected */ /* Only needed at shutdown. */
      terminate_consumer(/*rapid*/ true);
      /* purecov: end */
    }
    drop_remnants(/*force*/ true);
    redo_log_archive_file_pathname.clear();
    redo_log_archive_recorded_error.clear();
    redo_log_archive_session_ending = false;
    redo_log_archive_thd = nullptr;
    redo_log_archive_session = nullptr;
    redo_log_archive_active = false;
    redo_log_archive_queue.drop();
    mutex_exit(&redo_log_archive_admin_mutex);
    mutex_free(&redo_log_archive_admin_mutex);
  }
}

/**
  Check whether a valid value is given to innodb_redo_log_archive_dirs.
  This function is registered as a callback with MySQL.
  @param[in]	thd       thread handle
  @param[in]	var       pointer to system variable
  @param[out]	save      immediate result for update function
  @param[in]	value     incoming string
  @return 0 for valid contents
*/
int validate_redo_log_archive_dirs(THD *thd MY_ATTRIBUTE((unused)),
                                   SYS_VAR *var MY_ATTRIBUTE((unused)),
                                   void *save, struct st_mysql_value *value) {
  ut_a(save != NULL);
  ut_a(value != NULL);
  char buff[STRING_BUFFER_USUAL_SIZE];
  int len = sizeof(buff);
  int ret = 0;
  const char *irla_dirs = value->val_str(value, buff, &len);
  /* Parse the variable contents. */
  const char *ptr = irla_dirs;
  while ((ptr != nullptr) && (*ptr != '\0')) {
    /* Search colon. */
    const char *terminator = strchr(ptr, ':');
    if (terminator == nullptr) {
      /* No colon contained. */
      ret = 1;
      break;
    }
    /* Search semi-colon. */
    ptr = strchr(terminator + 1, ';');
    if (ptr != nullptr) {
      if (ptr == terminator + 1) {
        /* path name is empty. */
        ret = 1;
        break;
      }
      ptr++;
    } else {
      /* No semicolon found. */
      if (terminator[1] == '\0') {
        /* Path name is empty. */
        ret = 1;
      }
    }
  }
  if (ret == 0) {
    *static_cast<const char **>(save) = irla_dirs;
  }
  return (ret);
}

/**
  Verify that thd has the INNODB_REDO_LOG_ARCHIVE privilege.
  @param[in,out]  thd           current THD instance, current session
  @return         status
    @retval       false         success
    @retval       true          failure
*/
static bool verify_redo_log_archive_privilege(THD *thd) {
  DBUG_TRACE;
  if (thd == nullptr) {
    /* service interface does not allow this. */
    /* purecov: begin inspected */
    my_error(ER_INVALID_USE_OF_NULL, MYF(0));
    return true;
    /* purecov: end */
  }
  auto sctx = thd->security_context();
  const char privilege[]{"INNODB_REDO_LOG_ARCHIVE"};
  if (!(sctx->has_global_grant(STRING_WITH_LEN(privilege)).first)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), privilege);
    return true;
  }
  return false;
}

/**
  Get the directory behind the label in a semi-colon separated list of
  labeled directories.
  @param[in]      label         label for the selected directory; can be empty
  @param[out]     dir           directory path name
  @return         status
    @retval       false         success
    @retval       true          failure
*/
static bool get_labeled_directory(const char *label, std::string *dir) {
  DBUG_TRACE;
  DBUG_PRINT("redo_log_archive",
             ("label: '%s'  dirs: '%s'", label, redo_log_archive_dirs));
  size_t label_len = strlen(label);
  const char *ptr = redo_log_archive_dirs;
  while ((ptr != nullptr) && (*ptr != '\0')) {
    /* Search colon. */
    const char *terminator = strchr(ptr, ':');
    if (terminator == nullptr) {
      /* No colon found - no label. */
      /* purecov: begin inspected */
      /* validate_redo_log_archive_dirs prevents this. */
      ptr = nullptr;
      break;
      /* purecov: end */
    }
    if (((terminator - ptr) == std::ptrdiff_t(label_len)) &&
        (0 == strncmp(ptr, label, label_len))) {
      /* Found the matching label. Set ptr behind the colon. */
      ptr = terminator + 1;
      break;
    }
    /* Search semi-colon. */
    ptr = strchr(terminator + 1, ';');
    if (ptr != nullptr) {
      ptr++;
    }
  }
  if (ptr == nullptr) {
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_LABEL_NOT_FOUND, MYF(0), label);
    return true;
  }
  /* Search semi-colon. */
  const char *terminator = strchr(ptr, ';');
  if ((terminator == ptr) || (ptr[0] == '\0')) {
    /* validate_redo_log_archive_dirs() does not allow this. */
    /* purecov: begin inspected */
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_DIR_EMPTY, MYF(0), label);
    return true;
    /* purecov: end */
  }
  dir->assign(ptr, (terminator != nullptr) ? (terminator - ptr) : strlen(ptr));
  DBUG_PRINT("redo_log_archive", ("dir: '%s'", dir->c_str()));
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG
  LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
         (logmsgpfx + "selected dir '" + dir + "'").c_str());
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG */
  return false;
}

#ifndef _WIN32
/**
  Verify that a file system object does not grant permissions to everyone.
  @param[in]      path          path name of the file system object
  @return         status
    @retval       false         success
    @retval       true          failure
*/
static bool verify_no_world_permissions(const Fil_path &path) {
  DBUG_TRACE;
  struct stat statbuf;
  int ret = stat(path.abs_path().c_str(), &statbuf);
  if ((ret != 0) || ((statbuf.st_mode & S_IRWXO) != 0)) {
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_DIR_PERMISSIONS, MYF(0), path());
    return true;
  }
  return false;
}
#endif /* _WIN32 */

/**
  Trim trailing directory delimiters from a path name.
  @param[in]      path_name     path name
  @return         trimmed path name
*/
static std::string trim_path_name(const std::string &path_name) {
  std::string trimmed_name(path_name);
  while ((trimmed_name.length() > 0) &&
         (trimmed_name[trimmed_name.length() - 1] == OS_PATH_SEPARATOR)) {
    trimmed_name.pop_back();
  }
  return trimmed_name;
}

/**
  Append a trailing directory delimiter to a path name.
  This is done to support regression tests, which may want to replace
  path names based on server variable values, that could contain a
  trailing directory delimiter.
  @param[in]      path_name     path name
  @return         trimmed path name
*/
static std::string delimit_dir_name(const std::string &path_name) {
  std::string delimited_name(trim_path_name(path_name));
  delimited_name.push_back(OS_PATH_SEPARATOR);
  return delimited_name;
}

/**
  Append a path to a vector of directory paths.
  Append a variable name to a vector of variable names.
  The variable names belong to the server variables, from which the
  directory paths have been taken. The matching pair shares the same
  vector index.
  Only non-NULL, non-empty path names and their corresponding
  variable names are appended.
  The appended paths are normalized absolute real path names.
  @param[in]      variable_name variable name from which the path comes
  @param[in]      path_name     path name, may be NULL or empty
  @param[out]     variables     vector of variable names
  @param[out]     directories   vector of directory paths
*/
static void append_path(const char *variable_name, const char *path_name,
                        std::vector<std::string> *variables,
                        std::vector<Fil_path> *directories) {
  DBUG_TRACE;
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA
  DBUG_PRINT("redo_log_archive",
             ("append_path '%s' '%s'", variable_name, path_name));
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA */
  if ((path_name != nullptr) && (path_name[0] != '\0')) {
    Fil_path path(delimit_dir_name(path_name), /*normalize*/ true);
    /*
      Do not add datadir multiple times. Most variables default to datadir.
      Datadir is added first and so occupies vector slot zero.
    */
    if ((directories->size() == 0) ||
        (path.abs_path() != directories->at(0).abs_path())) {
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA
      DBUG_PRINT("redo_log_archive", ("add server directory '%s' '%s'",
                                      variable_name, path.abs_path().c_str()));
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA */
      variables->push_back(variable_name);
      directories->push_back(path);
    }
  }
}

/**
  Verify that a path name is not in, under, or above a server directory.
  @param[in]      path          normalized absolute real path name
  @return         status
    @retval       false         success
    @retval       true          failure
*/
static bool verify_no_server_directory(const Fil_path &path) {
  DBUG_TRACE;

  /* Collect server directories as normalized absolute real path names. */
  std::vector<std::string> variables;
  std::vector<Fil_path> directories;
  append_path("datadir", mysql_real_data_home_ptr, &variables, &directories);
  append_path("innodb_data_home_dir", srv_data_home, &variables, &directories);
  append_path("innodb_directories", innobase_directories, &variables,
              &directories);
  append_path("innodb_log_group_home_dir", srv_log_group_home_dir, &variables,
              &directories);
  append_path("innodb_temp_tablespaces_dir", ibt::srv_temp_dir, &variables,
              &directories);
  append_path("innodb_tmpdir", thd_innodb_tmpdir(nullptr), &variables,
              &directories);
  append_path("innodb_undo_directory", srv_undo_dir, &variables, &directories);
  append_path("secure_file_priv", opt_secure_file_priv, &variables,
              &directories);

  /* Test the target path against the collected directories. */
  std::string target(trim_path_name(path.abs_path()));
  size_t target_len = target.length();
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA
  DBUG_PRINT("redo_log_archive", ("target  directory '%s'", target.c_str()));
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA */
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG
  LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
         (logmsgpfx + "compare '" + target + "'").c_str());
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG */

  for (int idx = 0; idx < int(std::min(variables.size(), directories.size()));
       idx++) {
    Fil_path compare_path = directories[idx];
    std::string compare = trim_path_name(compare_path.abs_path());
    size_t compare_len = compare.length();
    size_t min_len = std::min(target_len, compare_len);
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA
    DBUG_PRINT("redo_log_archive", ("compare directory '%s'", compare.c_str()));
    DBUG_PRINT("redo_log_archive",
               ("target len: %lu  compare len: %lu  min len: %lu",
                static_cast<unsigned long int>(target_len),
                static_cast<unsigned long int>(compare_len),
                static_cast<unsigned long int>(min_len)));
    DBUG_EXECUTE(
        "redo_log_archive", if (target_len > compare_len) {
          DBUG_PRINT("redo_log_archive",
                     ("target at: %lu  is: '%c'",
                      static_cast<unsigned long int>(compare_len),
                      target[compare_len]));
        });
    DBUG_EXECUTE(
        "redo_log_archive", if (compare_len > target_len) {
          DBUG_PRINT("redo_log_archive",
                     ("compare at: %lu  is: '%c'",
                      static_cast<unsigned long int>(target_len),
                      compare[target_len]));
        });
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA */
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG
    LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
           (logmsgpfx + "with    '" + compare + "'").c_str());
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG */
    if (((compare_len == target_len) ||
         ((compare_len < target_len) &&
          (target[compare_len] == OS_PATH_SEPARATOR)) ||
         ((target_len < compare_len) &&
          (compare[target_len] == OS_PATH_SEPARATOR))) &&
#ifdef _WIN32
        (0 == native_strncasecmp(target.c_str(), compare.c_str(), min_len))
#else  /* _WIN32 */
        (0 == strncmp(target.c_str(), compare.c_str(), min_len))
#endif /* _WIN32 */
    ) {
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG
      LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
             (logmsgpfx + "match").c_str());
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG */
      my_error(ER_INNODB_REDO_LOG_ARCHIVE_DIR_CLASH, MYF(0), path(),
               variables[idx].c_str(), compare_path());
      return true;
    }
  }
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG
  LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
         (logmsgpfx + "no match").c_str());
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG */
  return false;
}

/**
  Construct the file path name as directory/archive.serverUUID.000001.log.
  @param[in]      path          normalized absolute real path name
  @param[out]     file_pathname file path name
*/
static void construct_file_pathname(const Fil_path &path,
                                    std::string *file_pathname) {
  DBUG_TRACE;
  file_pathname->assign(path.path());
  if (file_pathname->empty() || (file_pathname->back() != OS_PATH_SEPARATOR)) {
    file_pathname->push_back(OS_PATH_SEPARATOR); /* purecov: inspected */
  }
  file_pathname->append("archive.");
  file_pathname->append(server_uuid_ptr);
  file_pathname->append(".000001.log");
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA
  DBUG_PRINT("redo_log_archive",
             ("redo log archive file '%s'", file_pathname->c_str()));
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA */
}

/**
  Execute security checks and construct a file path name.
  @param[in,out]  thd           current THD instance, current session
  @param[in]      label         a label from innodb_redo_log_archive_dirs
  @param[in]      subdir        a plain directory name, on Unix/Linux/Mac
                                no slash ('/') is allowed, on Windows no
                                slash ('/'), backslash ('\'), nor colon
                                (':') is allowed in the argument.
                                Can be NULL or empty
  @param[out]     file_pathname the secure file path name
  @return         status
    @retval       false         success
    @retval       true          failure
*/
static bool construct_secure_file_path_name(THD *thd, const char *label,
                                            const char *subdir,
                                            std::string *file_pathname) {
  DBUG_TRACE;

  /* 'label' must not be NULL, but can be empty. */
  if (label == nullptr) {
    /* mysqlbackup component does not allow this. */
    /* purecov: begin inspected */
    my_error(ER_INVALID_USE_OF_NULL, MYF(0));
    return true;
    /* purecov: end */
  }

  /* 'subdir' is allowed to be NULL or empty. */

  /*
    Security measure: Require the innodb_redo_log_archive_dirs plugin
    variable to be non-NULL and non-empty.
  */
  if ((redo_log_archive_dirs == nullptr) ||
      (redo_log_archive_dirs[0] == '\0')) {
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_DIRS_INVALID, MYF(0));
    return true;
  }

  /*
    Get the directory behind the label in the redo log archive dirs
    plugin variable. 'label' can be empty.
  */
  std::string directory;
  if (get_labeled_directory(label, &directory)) {
    return true;
  }

  /*
    Security measure: If 'subdir' is given, it must be a plain directory
    name. Append it to the directory name.
  */
  if ((subdir != nullptr) && (subdir[0] != '\0')) {
    if (Fil_path::type_of_path(subdir) != Fil_path::file_name_only) {
      my_error(ER_INNODB_REDO_LOG_ARCHIVE_START_SUBDIR_PATH, MYF(0));
      return true;
    }
    if (directory.back() != OS_PATH_SEPARATOR) {
      directory.push_back(OS_PATH_SEPARATOR);
    }
    directory.append(subdir);
#ifdef DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG
    LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
           (logmsgpfx + "subdir path '" + directory + "'").c_str());
#endif /* DEBUG_REDO_LOG_ARCHIVE_EXTRA_LOG */
  }

  /*
    Security measure: The directory path name must lead to an existing
    directory. The server does not create it.
  */
  Fil_path subdir_path(directory, /*normalize*/ false);
  if (!subdir_path.is_directory_and_exists()) {
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_NO_SUCH_DIR, MYF(0), subdir_path());
    return true;
  }

  /*
    Security measure: The directory must not be in, under, or above a
    server directory.
  */
  if (verify_no_server_directory(subdir_path)) {
    return true;
  }

#ifndef _WIN32
  /*
    Security measure: The directory must not grant permissions to everyone.
  */
  if (verify_no_world_permissions(subdir_path)) {
    return true;
  }
#endif /* _WIN32 */

  /*
    Security measure: Do not allow arbitrary names, but construct the
    file name as archive.serverUUID.000001.log.
  */
  construct_file_pathname(subdir_path, file_pathname);

  return false;
}

/*
  Terminate the consumer thread.

  In the normal case the redo_log_archive_tmp_block is marked as the final
  block and enqueued, so that the consumer writes it to the file and
  ends. In the rapid case redo_log_archive_consume_complete is set and
  the queue is cleared.

  NOTE: This function must be called under the redo_log_archive_admin_mutex!

  @param[in]      rapid         whether a rapid termination is requested
  @return         status
    @retval       false         success
    @retval       true          failure
*/
static bool terminate_consumer(bool rapid) {
  DBUG_TRACE;
  if (rapid) {
    redo_log_archive_consume_complete = true;
    redo_log_archive_queue.deinit();
  } else if (redo_log_archive_consume_running) {
    /*
      Mark the last block as the final block and enqueue it for writing
      into the redo log archive file. This is required in any case if the
      consumer is still active. It may be waiting on the queue.

      If this call is from session end, then an error message is recorded
      and the comsumer sees it after dequeueing a block. It will skip all
      blocks, but still terminate on the final block only.
    */
    redo_log_archive_tmp_block.set_is_final_block(true);
    mutex_exit(&redo_log_archive_admin_mutex);
    redo_log_archive_queue.enqueue(redo_log_archive_tmp_block);
    mutex_enter(&redo_log_archive_admin_mutex);
  }

  /*
    Wait for the consumer to terminate. The
    redo_log_archive_consume_event is set after the final block
    is written into the redo log archive file.
  */
  float seconds_to_wait = 600.0;
  while (redo_log_archive_consume_running && (seconds_to_wait > 0.0) &&
         (redo_log_archive_consume_event != nullptr)) {
    os_event_t consume_event = redo_log_archive_consume_event;
    mutex_exit(&redo_log_archive_admin_mutex);
    os_event_wait_time(consume_event, 100000);  // 0.1 second
    seconds_to_wait -= 0.1;
    os_event_reset(consume_event);
    mutex_enter(&redo_log_archive_admin_mutex);
  }
  if (seconds_to_wait < 0.0) {
    /* This would require yet another tricky error injection. */
    /* purecov: begin inspected */
    if (!redo_log_archive_recorded_error.empty()) {
      redo_log_archive_recorded_error.append("; ");
    }
    redo_log_archive_recorded_error.append(
        "Termination of the consumer thread timed out");
    LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
           (logmsgpfx + redo_log_archive_recorded_error).c_str());
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_FAILED, MYF(0),
             redo_log_archive_recorded_error.c_str());
    return true;
    /* purecov: end */
  }

  srv_threads.m_backup_log_archiver.join();
  return false;
}

/*
  Start the redo log archiving.
*/
static bool redo_log_archive_start(THD *thd, const char *label,
                                   const char *subdir) {
  DBUG_TRACE;
  DBUG_PRINT("redo_log_archive", ("label: '%s'  subdir: '%s'",
                                  (label == nullptr) ? "[NULL]" : label,
                                  (subdir == nullptr) ? "[NULL]" : subdir));
  /* Security measure: Require the redo log archive privilege. */
  if (verify_redo_log_archive_privilege(thd)) {
    return true;
  }

  /* Synchronize with with other threads while using global objects. */
  mutex_enter(&redo_log_archive_admin_mutex);

  /*
    Redo log archiving must not already be active. Do this check early,
    because other error reports in checking the parameters might be
    confusing, if archiving is active already.
  */
  if (redo_log_archive_active) {
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_ACTIVE, MYF(0),
             redo_log_archive_file_pathname.c_str());
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
  }

  /* Drop potential left-over resources to avoid leaks. */
  if (drop_remnants(/*force*/ false)) {
    /* purecov: begin inspected */
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
    /* purecov: end */
  }

  /*
    Construct a file path name.
  */
  std::string file_pathname;
  if (construct_secure_file_path_name(thd, label, subdir, &file_pathname)) {
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
  }

  /* Get current session. */
  innodb_session_t *session = thd_to_innodb_session(thd);
  ut_ad(session != nullptr);

  /*
    Create the redo log archive file.
  */
  ulint os_innodb_umask_saved = os_file_get_umask();
#ifdef _WIN32
  os_file_set_umask(_S_IREAD);
#else  /* _WIN32 */
  os_file_set_umask(S_IRUSR | S_IRGRP);
#endif /* _WIN32 */
  bool success;
  pfs_os_file_t file_handle = os_file_create_simple_no_error_handling(
      redo_log_archive_file_key, file_pathname.c_str(), OS_FILE_CREATE,
      OS_FILE_READ_WRITE, /*read_only*/ false, &success);
  os_file_set_umask(os_innodb_umask_saved);
  if (!success) {
    int os_errno = errno;
    char errbuf[MYSYS_STRERROR_SIZE];
    my_strerror(errbuf, sizeof(errbuf), os_errno);

    /* On Windows it fails with 0 if the file exists. */
    if ((os_errno != 0) && (os_errno != EEXIST)) {
      /* Found cases, where the file had been created in spite of !success. */
      /* purecov: begin inspected */
      os_file_delete_if_exists(redo_log_archive_file_key, file_pathname.c_str(),
                               NULL);
      /* purecov: end */
    }
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_FILE_CREATE, MYF(0),
             file_pathname.c_str(), os_errno, errbuf);
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
  }
  DBUG_PRINT("redo_log_archive", ("Created redo_log_archive_file_pathname '%s'",
                                  file_pathname.c_str()));

  /*
    Create the consume_event.
  */
  os_event_t consume_event = os_event_create("redo_log_archive_consume_event");
  DBUG_EXECUTE_IF("redo_log_archive_bad_alloc", os_event_destroy(consume_event);
                  consume_event = nullptr;);
  if (consume_event == nullptr) {
    os_file_close(file_handle);
    os_file_delete_if_exists(redo_log_archive_file_key, file_pathname.c_str(),
                             NULL);
    my_error(ER_STD_BAD_ALLOC_ERROR, MYF(0), "redo_log_archive_consume_event",
             "redo_log_archive_start");
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
  }
  os_event_reset(consume_event);
  DBUG_PRINT("redo_log_archive", ("Created consume_event"));

  /* Initialize the temporary block. */
  redo_log_archive_tmp_block.reset();
  redo_log_archive_tmp_block.set_is_final_block(false);

  /* Initialize the queue. */
  redo_log_archive_queue.init(QUEUE_SIZE_MAX);

  /* Set the redo log archiving to active. */
  redo_log_archive_consume_event = consume_event;
  redo_log_archive_consume_complete = false;
  redo_log_archive_file_handle = file_handle;
  redo_log_archive_file_pathname = file_pathname;
  redo_log_archive_recorded_error.clear();
  redo_log_archive_session_ending = false;
  redo_log_archive_thd = thd;
  redo_log_archive_session = session;
  redo_log_archive_active = true;

  srv_threads.m_backup_log_archiver = os_thread_create(
      redo_log_archive_consumer_thread_key, redo_log_archive_consumer);

  mutex_exit(&redo_log_archive_admin_mutex);

  /* Create the consumer thread. */
  DBUG_PRINT("redo_log_archive", ("Creating consumer thread"));

  srv_threads.m_backup_log_archiver.start();

  /*
    Wait for the consumer to start. We do not want to report success
    before the consumer thread has started to work.
  */
  float seconds_to_wait = 600.0;
  DBUG_EXECUTE_IF("innodb_redo_log_archive_start_timeout",
                  seconds_to_wait = -1.0;);
  mutex_enter(&redo_log_archive_admin_mutex);
  while (!redo_log_archive_consume_running && (seconds_to_wait > 0.0) &&
         (redo_log_archive_consume_event != nullptr)) {
    os_event_t consume_event = redo_log_archive_consume_event;
    mutex_exit(&redo_log_archive_admin_mutex);
    os_event_wait_time(consume_event, 100000);  // 0.1 second
    seconds_to_wait -= 0.1;
    os_event_reset(consume_event);
    mutex_enter(&redo_log_archive_admin_mutex);
  }
  if (seconds_to_wait < 0.0) {
    os_event_destroy(redo_log_archive_consume_event);
    redo_log_archive_consume_event = nullptr;
    redo_log_archive_consume_complete = true;
    if (redo_log_archive_file_handle.m_file != OS_FILE_CLOSED) {
      os_file_close(redo_log_archive_file_handle);
      redo_log_archive_file_handle.m_file = OS_FILE_CLOSED;
    }
    os_file_delete_if_exists(redo_log_archive_file_key,
                             redo_log_archive_file_pathname.c_str(), NULL);
    redo_log_archive_file_pathname.clear();
    /* Keep recorded_error */
    redo_log_archive_session_ending = false;
    redo_log_archive_thd = nullptr;
    redo_log_archive_session = nullptr;
    redo_log_archive_active = false;
    redo_log_archive_queue.deinit();
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_START_TIMEOUT, MYF(0));
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
  }
  mutex_exit(&redo_log_archive_admin_mutex);
  DBUG_PRINT("redo_log_archive", ("Redo log archiving started"));
  return false;
}

/*
  Stop the redo log archiving.

  This can either be called
  - through the service interface
    - when redo log archiving is active and in good state
      => Stop in normal, clean way.
    - when redo log archiving is active and in error state
      => Stop quickly and return the error.

    - when redo log archiving is inactive and an error is recorded
      => Return the recorded error.
    - when redo log archiving is inactive
      => Return an error.

  - at session end when redo log archiving is active
      => Stop quickly and record an error for the next stop operation.
*/
static bool redo_log_archive_stop(THD *thd) {
  DBUG_TRACE;

  /*
    Security measure: Require the redo log archive privilege.
  */
  if (verify_redo_log_archive_privilege(thd)) {
    return true;
  }

  /* Synchronize with with other threads while using global objects. */
  mutex_enter(&redo_log_archive_admin_mutex);

  /*
    If redo log archiving is inactive, the stop request fails.
    If there was an error recorded, return it.
  */
  if (!redo_log_archive_active) {
    DBUG_PRINT("redo_log_archive", ("Not active"));
    if (!redo_log_archive_recorded_error.empty()) {
      DBUG_PRINT("redo_log_archive", ("Recorded error '%s'",
                                      redo_log_archive_recorded_error.c_str()));
      my_error(ER_INNODB_REDO_LOG_ARCHIVE_FAILED, MYF(0),
               redo_log_archive_recorded_error.c_str());
      /* Do not clear the error, it may be wanted by another session again. */
      mutex_exit(&redo_log_archive_admin_mutex);
      return true;
    }
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_INACTIVE, MYF(0));
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
  }

  /*
    Redo log archiving is still active.
    We must not stop it if another session has started it.
  */
  if ((redo_log_archive_session != thd_to_innodb_session(thd)) ||
      (redo_log_archive_thd != thd)) {
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_SESSION, MYF(0));
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
  }

  DBUG_PRINT("redo_log_archive", ("Stopping redo log archiving on '%s'",
                                  redo_log_archive_file_pathname.c_str()));

  /*
    This session has started the redo log archiving.
    The call can be from the service interface or from session end.
    Disable the producer as soon as possible.
  */
  mutex_exit(&redo_log_archive_admin_mutex);
  ut_ad(log_sys != nullptr);
  log_writer_mutex_enter(*log_sys);
  redo_log_archive_produce_blocks = false;
  log_writer_mutex_exit(*log_sys);
  mutex_enter(&redo_log_archive_admin_mutex);

  if (terminate_consumer(/*rapid*/ false)) {
    return true; /* purecov: inspected */
  }
  redo_log_archive_queue.deinit();
  /*
    Publish the stop state.
  */
  os_event_destroy(redo_log_archive_consume_event);
  redo_log_archive_consume_event = nullptr;
  redo_log_archive_consume_complete = true;
  if (redo_log_archive_file_handle.m_file != OS_FILE_CLOSED) {
    /* purecov: begin inspected */
    os_file_close(redo_log_archive_file_handle);
    redo_log_archive_file_handle.m_file = OS_FILE_CLOSED;
    /* purecov: end */
  }
  /*
    If redo log archiving was in error state, remove the redo log
    archive file, if the consumer has not already done it.
  */
  if (!redo_log_archive_recorded_error.empty() &&
      !redo_log_archive_file_pathname.empty()) {
    DBUG_PRINT("redo_log_archive", ("Recorded error '%s'",
                                    redo_log_archive_recorded_error.c_str()));
    DBUG_PRINT("redo_log_archive", ("Delete redo log archive file '%s'",
                                    redo_log_archive_file_pathname.c_str()));
    os_file_delete_if_exists(redo_log_archive_file_key,
                             redo_log_archive_file_pathname.c_str(), NULL);
  }
  redo_log_archive_file_pathname.clear();
  /* Keep recorded_error */
  redo_log_archive_thd = nullptr;
  redo_log_archive_session = nullptr;
  redo_log_archive_active = false;

  DBUG_PRINT("redo_log_archive", ("Redo log archiving stopped"));

  /*
    If the stop was called after the occurrence of an error condition,
    - Session that started redo log archiving terminated,
    - Error while trying to write into the redo log archive,
    report the error message back to the caller.

    But do not report an error if the session is ending. The session
    might be in error already.
  */
  if (!redo_log_archive_recorded_error.empty() &&
      !redo_log_archive_session_ending) {
    /* purecov: begin inspected */
    my_error(ER_INNODB_REDO_LOG_ARCHIVE_FAILED, MYF(0),
             redo_log_archive_recorded_error.c_str());
    /* Do not clear the error, it may be wanted by another session again. */
    mutex_exit(&redo_log_archive_admin_mutex);
    return true;
    /* purecov: end */
  }
  mutex_exit(&redo_log_archive_admin_mutex);
  /* Success */
  return false;
}

/*
  Security function to be called when the current session ends.
*/
void redo_log_archive_session_end(innodb_session_t *session) {
  /*
    This function can be called after the InnoDB handlerton has been
    initialized and before InnoDB is started. In such case the
    redo_log_archive_admin_mutex has not yet been created. To prevent
    the access of a non-existing mutex, the global atomic variable
    redo_log_archive_initialized can be used as it is true only when the
    mutex exists. Due to the std::atomic qualifier it should be thread
    safe in protecting access to the mutex.
  */
  if (redo_log_archive_initialized) {
    bool stop_required = false;
    THD *thd{};

    /* Synchronize with with other threads while using global objects. */
    mutex_enter(&redo_log_archive_admin_mutex);
    if (redo_log_archive_active) {
      if (redo_log_archive_session == session) {
        DBUG_PRINT("redo_log_archive",
                   ("Redo log archiving is active by this session. Stopping."));
        stop_required = true;
        redo_log_archive_session_ending = true;
        thd = redo_log_archive_thd;
        if (!redo_log_archive_recorded_error.empty()) {
          redo_log_archive_recorded_error.append("; "); /* purecov: inspected */
        }
        redo_log_archive_recorded_error.append(
            "Session terminated with active redo log archiving");
      }
    }
    mutex_exit(&redo_log_archive_admin_mutex);

    if (stop_required && (thd != nullptr)) {
      LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
             (logmsgpfx + "Unexpected termination of the session that started"
                          " redo log archiving. Stopping redo log archiving.")
                 .c_str());
      if (redo_log_archive_stop(thd)) {
        /* return value must be used. */
      }
    }
  }
}

/**
  Produce redo log blocks for the queue.

  This function is called for every log write. So it must be as
  efficient as possible.

  NOTE: This function must be called under the 'log_sys.writer_mutex'!
*/
void redo_log_archive_produce(const byte *write_buf, const size_t write_size) {
  /* Execute the function body only if redo log archiving is active. */
  if (redo_log_archive_produce_blocks) {
    ut_ad(log_sys != nullptr);
    ut_ad(log_writer_mutex_own(*log_sys));
    ut_ad(write_buf != nullptr);
    ut_ad(write_size > 0);

    /*
      Scan the redo log block in chunks of  OS_FILE_LOG_BLOCK_SIZE (512) bytes.
      - If a chunk is empty or incomplete, parsing is stopped at this point.
      - If the temporary buffer becomes full, it is enqueued and the temporary
      buffer is cleared for storing further log records.
    */
    for (size_t i = 0; i < write_size; i += OS_FILE_LOG_BLOCK_SIZE) {
      if (redo_log_archive_tmp_block.full()) {
        redo_log_archive_queue.enqueue(redo_log_archive_tmp_block);
        redo_log_archive_tmp_block.reset();
        redo_log_archive_tmp_block.set_is_final_block(false);
      }
      if (!redo_log_archive_tmp_block.put_log_block(write_buf, i)) {
        break;
      }
    }
  }
}

/**
  Dequeue blocks of size QUEUE_BLOCK_SIZE, enqueued by the producer.
  Write the blocks to the redo log archive file sequentially.
*/
static void redo_log_archive_consumer() {
  DBUG_TRACE;
  /* Synchronize with with other threads while using global objects. */
  mutex_enter(&redo_log_archive_admin_mutex);

  if (redo_log_archive_consume_running) {
    /* Another consumer thread is still running. */
    /* purecov: begin inspected */
    if (!redo_log_archive_recorded_error.empty()) {
      redo_log_archive_recorded_error.append("; ");
    }
    redo_log_archive_recorded_error.append(
        "Consumer thread refuses to start - another one is running");
    if (redo_log_archive_consume_event != nullptr) {
      os_event_set(redo_log_archive_consume_event);
    }
    LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
           (logmsgpfx + "Redo log archiving consumer thread refuses to start"
                        " - another one is running")
               .c_str());
    mutex_exit(&redo_log_archive_admin_mutex);
    DBUG_PRINT("redo_log_archive", ("Other consumer is running"));
    return;
    /* purecov: end */
  }
  DBUG_EXECUTE_IF("innodb_redo_log_archive_start_timeout",
                  my_sleep(9000000););  // 9s

  /*
    A Guardian sets the 'running' status to true. When leaving the
    function (ending the thread), the Guardian's destructor sets it back
    to false again. The Guardian sets the event (if not NULL at that time)
    in both cases.
  */
  Guardian consumer_guardian(&redo_log_archive_consume_running,
                             &redo_log_archive_consume_event,
                             &redo_log_archive_admin_mutex);

  /* Start might have timed out meanwhile. */
  if (redo_log_archive_consume_complete ||
      (redo_log_archive_consume_event == nullptr)) {
    if (!redo_log_archive_recorded_error.empty()) {
      redo_log_archive_recorded_error.append("; "); /* purecov: inspected */
    }
    redo_log_archive_recorded_error.append(
        "Consumer appears completed at start - terminating");
    if (redo_log_archive_consume_event != nullptr) {
      os_event_set(redo_log_archive_consume_event); /* purecov: inspected */
    }
    LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
           (logmsgpfx +
            "Redo log archiving consumer thread sees completion at start"
            " - terminating")
               .c_str());
    mutex_exit(&redo_log_archive_admin_mutex);
    DBUG_PRINT("redo_log_archive", ("Consumer is already marked complete"));
    return;
  }
  mutex_exit(&redo_log_archive_admin_mutex);

  /*
    Activate the producer outside of the redo_log_archive_admin_mutex
    and open a block, which defines the scope for the producer guardian.
  */
  {
    ut_ad(log_sys != nullptr);
    Guardian producer_guardian(&redo_log_archive_produce_blocks, nullptr,
                               &log_sys->writer_mutex);

    /*
      Offset inside the redo log archive file. The offset is incremented
      each time the consumer writes to the redo log archive file.
    */
    uint64_t file_offset{0};
    IORequest request(IORequest::WRITE);
    Block temp_block;

    mutex_enter(&redo_log_archive_admin_mutex);
    while (!redo_log_archive_consume_complete) {
      /* Dequeue a log block from the queue outside of the mutex. */
      mutex_exit(&redo_log_archive_admin_mutex);
      redo_log_archive_queue.dequeue(temp_block);
      mutex_enter(&redo_log_archive_admin_mutex);
      /* Check the redo log archiving state. It could have changed meanwhile. */
      if (redo_log_archive_consume_complete) {
        /* purecov: begin inspected */
        DBUG_PRINT("redo_log_archive",
                   ("Consume complete - Stopping consumer."));
        break;
        /* purecov: end */
      }

      /*
        Write the dequeued block only if redo log archiving is in a good state.
      */
      if (redo_log_archive_active && redo_log_archive_recorded_error.empty() &&
          !redo_log_archive_file_pathname.empty() &&
          (redo_log_archive_file_handle.m_file != OS_FILE_CLOSED)) {
        dberr_t err = os_file_write(
            request, redo_log_archive_file_pathname.c_str(),
            redo_log_archive_file_handle, temp_block.get_queue_block(),
            file_offset, QUEUE_BLOCK_SIZE);
        /*
          An error during the write to the redo log archive file causes
          the consumer to terminate and record the error for the next
          redo_log_archive_stop() call.
        */
        if (err != DB_SUCCESS) {
          /* This requires disk full testing */
          /* purecov: begin inspected */
          int os_errno = errno;
          char errbuf[MYSYS_STRERROR_SIZE];
          my_strerror(errbuf, sizeof(errbuf), os_errno);
          std::stringstream recorded_error_ss;
          recorded_error_ss << "Cannot write to file '"
                            << redo_log_archive_file_pathname << "' at offset "
                            << file_offset << " (OS errno: " << os_errno
                            << " - " << errbuf << ")";
          if (!redo_log_archive_recorded_error.empty()) {
            redo_log_archive_recorded_error.append("; ");
          }
          redo_log_archive_recorded_error.append(recorded_error_ss.str());
          redo_log_archive_consume_complete = true;
          break;
          /* purecov: end */
        }
        file_offset += QUEUE_BLOCK_SIZE;
      }

      if (temp_block.get_is_final_block()) {
        DBUG_PRINT("redo_log_archive", ("Final Block - Stopping consumer."));
        redo_log_archive_consume_complete = true;
      }
    } /* end while loop */
    mutex_exit(&redo_log_archive_admin_mutex);
  } /* end producer_guardian block -> disable producer */

  mutex_enter(&redo_log_archive_admin_mutex);
  if (redo_log_archive_consume_event != nullptr) {
    os_event_set(redo_log_archive_consume_event);
  }
  if (redo_log_archive_file_handle.m_file != OS_FILE_CLOSED) {
    os_file_close(redo_log_archive_file_handle);
    redo_log_archive_file_handle.m_file = OS_FILE_CLOSED;
  }
  if (!redo_log_archive_recorded_error.empty()) {
    /* We have to remove the file on error. */
    /* This requires disk full testing */
    /* purecov: begin inspected */
    if (!redo_log_archive_file_pathname.empty()) {
      os_file_delete_if_exists(redo_log_archive_file_key,
                               redo_log_archive_file_pathname.c_str(), NULL);
      /*
        Do not clear the filename here. Redo log archiving is not yet inactive.
      */
    }
    redo_log_archive_recorded_error.append(
        " - stopped redo log archiving and deleted the file.");
    LogErr(INFORMATION_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
           (logmsgpfx + redo_log_archive_recorded_error).c_str());
    /* purecov: end */
  }
  mutex_exit(&redo_log_archive_admin_mutex);
  DBUG_PRINT("redo_log_archive", ("Redo log archive log consumer stopped"));
}

/**
  Initialize UDF innodb_redo_log_archive_start

  See include/mysql/udf_registration_types.h
*/
bool innodb_redo_log_archive_start_init(UDF_INIT *initid MY_ATTRIBUTE((unused)),
                                        UDF_ARGS *args, char *message) {
  if ((args->arg_count < 1) || (args->arg_count > 2)) {
    strncpy(message, "Invalid number of arguments.", MYSQL_ERRMSG_SIZE);
    return true;
  }
  if (args->args[0] == nullptr) {
    strncpy(message, "First argument must not be null.", MYSQL_ERRMSG_SIZE);
    return true;
  }
  if (args->arg_type[0] != STRING_RESULT) {
    strncpy(message, "Invalid first argument type.", MYSQL_ERRMSG_SIZE);
    return true;
  }
  if ((args->arg_count == 2) && (args->arg_type[1] != STRING_RESULT)) {
    strncpy(message, "Invalid second argument type.", MYSQL_ERRMSG_SIZE);
    return true;
  }
  return false;
}

/**
  Deinitialize UDF innodb_redo_log_archive_start

  See include/mysql/udf_registration_types.h
*/
void innodb_redo_log_archive_start_deinit(
    UDF_INIT *initid MY_ATTRIBUTE((unused))) {
  return;
}

/**
  UDF innodb_redo_log_archive_start

  The UDF is of type Udf_func_longlong returning INT_RESULT

  See include/mysql/udf_registration_types.h

  The UDF expects two or three arguments:
  - A thread context pointer, maybe NULL
  - A label from the server system variable innodb_redo_log_archive_dirs
  - An optional subdirectory inside the corresponding directory path from
    innodb_redo_log_archive_dirs. This must be a plain directory
    name. On Unix/Linux/Mac no slash ('/') is allowed in the
    argument. On Windows, no slash ('/'), backslash ('\'), nor
    colon (':') is allowed in the argument. Can be NULL or empty.

  Returns zero on success, one otherwise.
*/
long long innodb_redo_log_archive_start(
    UDF_INIT *initid MY_ATTRIBUTE((unused)), UDF_ARGS *args,
    unsigned char *null_value MY_ATTRIBUTE((unused)),
    unsigned char *error MY_ATTRIBUTE((unused))) {
  return static_cast<long long>(meb::redo_log_archive_start(
      current_thd, args->args[0],
      (args->arg_count == 2) ? args->args[1] : nullptr));
}

/**
  Initialize UDF innodb_redo_log_archive_stop

  See include/mysql/udf_registration_types.h
*/
bool innodb_redo_log_archive_stop_init(UDF_INIT *initid MY_ATTRIBUTE((unused)),
                                       UDF_ARGS *args, char *message) {
  if (args->arg_count != 0) {
    strncpy(message, "Invalid number of arguments.", MYSQL_ERRMSG_SIZE);
    return true;
  }
  return false;
}

/**
  Deinitialize UDF innodb_redo_log_archive_stop

  See include/mysql/udf_registration_types.h
*/
void innodb_redo_log_archive_stop_deinit(
    UDF_INIT *initid MY_ATTRIBUTE((unused))) {
  return;
}

/**
  UDF innodb_redo_log_archive_stop

  The UDF is of type Udf_func_longlong returning INT_RESULT

  See include/mysql/udf_registration_types.h

  The UDF expects one argument:
  - A thread context pointer, maybe NULL

  Returns zero on success, one otherwise.
*/
long long innodb_redo_log_archive_stop(
    UDF_INIT *initid MY_ATTRIBUTE((unused)),
    UDF_ARGS *args MY_ATTRIBUTE((unused)),
    unsigned char *null_value MY_ATTRIBUTE((unused)),
    unsigned char *error MY_ATTRIBUTE((unused))) {
  return static_cast<long long>(meb::redo_log_archive_stop(current_thd));
}

/**
  Type and data for tracking registered UDFs.
*/
struct udf_data_t {
  const std::string m_name;
  const Item_result m_return_type;
  const Udf_func_any m_func;
  const Udf_func_init m_init_func;
  const Udf_func_deinit m_deinit_func;
  udf_data_t(const std::string &name, const Item_result return_type,
             const Udf_func_any func, const Udf_func_init init_func,
             const Udf_func_deinit deinit_func)
      : m_name(name),
        m_return_type(return_type),
        m_func(func),
        m_init_func(init_func),
        m_deinit_func(deinit_func) {}
};

/**
  This component's UDFs.
*/
static udf_data_t component_udfs[] = {
    {"innodb_redo_log_archive_start", INT_RESULT,
     reinterpret_cast<Udf_func_any>(innodb_redo_log_archive_start),
     reinterpret_cast<Udf_func_init>(innodb_redo_log_archive_start_init),
     reinterpret_cast<Udf_func_deinit>(innodb_redo_log_archive_start_deinit)},
    {"innodb_redo_log_archive_stop", INT_RESULT,
     reinterpret_cast<Udf_func_any>(innodb_redo_log_archive_stop),
     reinterpret_cast<Udf_func_init>(innodb_redo_log_archive_stop_init),
     reinterpret_cast<Udf_func_deinit>(innodb_redo_log_archive_stop_deinit)}};

/**
  Unregister UDF(s)
*/
static void unregister_udfs() {
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  if (plugin_registry == nullptr) {
    /* purecov: begin inspected */
    LogErr(
        WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
        (logmsgpfx + "mysql_plugin_registry_acquire() returns NULL").c_str());
    return;
    /* purecov: end */
  }

  /*
    Open a new block so that udf_registrar is automatically destroyed
    before we release the plugin_registry.
  */
  {
    my_service<SERVICE_TYPE(udf_registration)> udf_registrar("udf_registration",
                                                             plugin_registry);
    if (udf_registrar.is_valid()) {
      for (udf_data_t udf : component_udfs) {
        const char *name = udf.m_name.c_str();
        int was_present = 0;
        if (udf_registrar->udf_unregister(name, &was_present) && was_present) {
          /* purecov: begin inspected */ /* Only needed if unregister fails. */
          LogErr(WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
                 (logmsgpfx + "Cannot unregister UDF '" + name + "'").c_str());
          /* purecov: end */
        }
      }
    } else {
      LogErr(WARNING_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
             (logmsgpfx + "Cannot get valid udf_registration service").c_str());
    }
  } /* end of udf_registrar block */
  mysql_plugin_registry_release(plugin_registry);
}

/**
  Register UDF(s).

  This does first try to unregister any functions, that might be left over
  from an earlier use of the component.

  @return       status
    @retval     false           success
    @retval     true            failure
*/
static bool register_udfs() {
  /* Try to unregister potentially left over functions from last run. */
  unregister_udfs();

  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  if (plugin_registry == nullptr) {
    /* purecov: begin inspected */
    LogErr(
        ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
        (logmsgpfx + "mysql_plugin_registry_acquire() returns NULL").c_str());
    return true;
    /* purecov: end */
  }

  bool failed = false;
  /*
    Open a new block so that udf_registrar is automatically destroyed
    before we release the plugin_registry.
  */
  {
    my_service<SERVICE_TYPE(udf_registration)> udf_registrar("udf_registration",
                                                             plugin_registry);
    if (udf_registrar.is_valid()) {
      for (udf_data_t udf : component_udfs) {
        const char *name = udf.m_name.c_str();
        if (udf_registrar->udf_register(name, udf.m_return_type, udf.m_func,
                                        udf.m_init_func, udf.m_deinit_func)) {
          /* purecov: begin inspected */ /* Only needed if register fails. */
          LogErr(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
                 (logmsgpfx + "Cannot register UDF '" + name + "'").c_str());
          failed = true;
          break;
          /* purecov: end */
        }
      }
    } else {
      LogErr(ERROR_LEVEL, ER_INNODB_ERROR_LOGGER_MSG,
             (logmsgpfx + "Cannot get valid udf_registration service").c_str());
      failed = true;
    }
  } /* end of udf_registrar block */
  mysql_plugin_registry_release(plugin_registry);
  if (failed) {
    unregister_udfs();
  }
  return failed;
}

} /* namespace meb */
