/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef REPLICATION_H
#define REPLICATION_H

#include <mysql.h>

typedef struct st_mysql MYSQL;

#ifdef __cplusplus
extern "C" {
#endif

/**
   Transaction observer flags.
*/
enum Trans_flags {
  /** Transaction is a real transaction */
  TRANS_IS_REAL_TRANS = 1
};

/**
   Transaction observer parameter
*/
typedef struct Trans_param {
  uint32 server_id;
  uint32 flags;

  /*
    The latest binary log file name and position written by current
    transaction, if binary log is disabled or no log event has been
    written into binary log file by current transaction (events
    written into transaction log cache are not counted), these two
    member will be zero.
  */
  const char *log_file;
  my_off_t log_pos;
} Trans_param;

/**
   Observes and extends transaction execution
*/
typedef struct Trans_observer {
  uint32 len;

  /**
     This callback is called after transaction commit
     
     This callback is called right after commit to storage engines for
     transactional tables.

     For non-transactional tables, this is called at the end of the
     statement, before sending statement status, if the statement
     succeeded.

     @note The return value is currently ignored by the server.

     @param param The parameter for transaction observers

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*after_commit)(Trans_param *param);

  /**
     This callback is called after transaction rollback

     This callback is called right after rollback to storage engines
     for transactional tables.

     For non-transactional tables, this is called at the end of the
     statement, before sending statement status, if the statement
     failed.

     @note The return value is currently ignored by the server.

     @param param The parameter for transaction observers

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*after_rollback)(Trans_param *param);
} Trans_observer;

/**
   Binlog storage flags
*/
enum Binlog_storage_flags {
  /** Binary log was sync:ed */
  BINLOG_STORAGE_IS_SYNCED = 1
};

/**
   Binlog storage observer parameters
 */
typedef struct Binlog_storage_param {
  uint32 server_id;
} Binlog_storage_param;

/**
   Observe binlog logging storage
*/
typedef struct Binlog_storage_observer {
  uint32 len;

  /**
     This callback is called after binlog has been flushed

     This callback is called after cached events have been flushed to
     binary log file. Whether the binary log file is synchronized to
     disk is indicated by the bit BINLOG_STORAGE_IS_SYNCED in @a flags.

     @param param Observer common parameter
     @param log_file Binlog file name been updated
     @param log_pos Binlog position after update
     @param flags flags for binlog storage

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*after_flush)(Binlog_storage_param *param,
                     const char *log_file, my_off_t log_pos,
                     uint32 flags);
} Binlog_storage_observer;

/**
   Replication binlog transmitter (binlog dump) observer parameter.
*/
typedef struct Binlog_transmit_param {
  uint32 server_id;
  uint32 flags;
} Binlog_transmit_param;

/**
   Observe and extends the binlog dumping thread.
*/
typedef struct Binlog_transmit_observer {
  uint32 len;
  
  /**
     This callback is called when binlog dumping starts


     @param param Observer common parameter
     @param log_file Binlog file name to transmit from
     @param log_pos Binlog position to transmit from

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*transmit_start)(Binlog_transmit_param *param,
                        const char *log_file, my_off_t log_pos);

  /**
     This callback is called when binlog dumping stops

     @param param Observer common parameter
     
     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*transmit_stop)(Binlog_transmit_param *param);

  /**
     This callback is called to reserve bytes in packet header for event transmission

     This callback is called when resetting transmit packet header to
     reserve bytes for this observer in packet header.

     The @a header buffer is allocated by the server code, and @a size
     is the size of the header buffer. Each observer can only reserve
     a maximum size of @a size in the header.

     @param param Observer common parameter
     @param header Pointer of the header buffer
     @param size Size of the header buffer
     @param len Header length reserved by this observer

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*reserve_header)(Binlog_transmit_param *param,
                        unsigned char *header,
                        unsigned long size,
                        unsigned long *len);

  /**
     This callback is called before sending an event packet to slave

     @param param Observer common parameter
     @param packet Binlog event packet to send
     @param len Length of the event packet
     @param log_file Binlog file name of the event packet to send
     @param log_pos Binlog position of the event packet to send

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*before_send_event)(Binlog_transmit_param *param,
                           unsigned char *packet, unsigned long len,
                           const char *log_file, my_off_t log_pos );

  /**
     This callback is called after sending an event packet to slave

     @param param Observer common parameter
     @param event_buf Binlog event packet buffer sent
     @param len length of the event packet buffer

     @retval 0 Sucess
     @retval 1 Failure
   */
  int (*after_send_event)(Binlog_transmit_param *param,
                          const char *event_buf, unsigned long len);

  /**
     This callback is called after resetting master status

     This is called when executing the command RESET MASTER, and is
     used to reset status variables added by observers.

     @param param Observer common parameter

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*after_reset_master)(Binlog_transmit_param *param);
} Binlog_transmit_observer;

/**
   Binlog relay IO flags
*/
enum Binlog_relay_IO_flags {
  /** Binary relay log was sync:ed */
  BINLOG_RELAY_IS_SYNCED = 1
};


/**
  Replication binlog relay IO observer parameter
*/
typedef struct Binlog_relay_IO_param {
  uint32 server_id;

  /* Master host, user and port */
  char *host;
  char *user;
  unsigned int port;

  char *master_log_name;
  my_off_t master_log_pos;

  MYSQL *mysql;                        /* the connection to master */
} Binlog_relay_IO_param;

/**
   Observes and extends the service of slave IO thread.
*/
typedef struct Binlog_relay_IO_observer {
  uint32 len;

  /**
     This callback is called when slave IO thread starts

     @param param Observer common parameter

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*thread_start)(Binlog_relay_IO_param *param);

  /**
     This callback is called when slave IO thread stops

     @param param Observer common parameter

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*thread_stop)(Binlog_relay_IO_param *param);

  /**
     This callback is called before slave requesting binlog transmission from master

     This is called before slave issuing BINLOG_DUMP command to master
     to request binlog.

     @param param Observer common parameter
     @param flags binlog dump flags

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*before_request_transmit)(Binlog_relay_IO_param *param, uint32 flags);

  /**
     This callback is called after read an event packet from master

     @param param Observer common parameter
     @param packet The event packet read from master
     @param len Length of the event packet read from master
     @param event_buf The event packet return after process
     @param event_len The length of event packet return after process

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*after_read_event)(Binlog_relay_IO_param *param,
                          const char *packet, unsigned long len,
                          const char **event_buf, unsigned long *event_len);

  /**
     This callback is called after written an event packet to relay log

     @param param Observer common parameter
     @param event_buf Event packet written to relay log
     @param event_len Length of the event packet written to relay log
     @param flags flags for relay log

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*after_queue_event)(Binlog_relay_IO_param *param,
                           const char *event_buf, unsigned long event_len,
                           uint32 flags);

  /**
     This callback is called after reset slave relay log IO status
     
     @param param Observer common parameter

     @retval 0 Sucess
     @retval 1 Failure
  */
  int (*after_reset_slave)(Binlog_relay_IO_param *param);
} Binlog_relay_IO_observer;


/**
   Register a transaction observer

   @param observer The transaction observer to register
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer already exists
*/
int register_trans_observer(Trans_observer *observer, void *p);

/**
   Unregister a transaction observer

   @param observer The transaction observer to unregister
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer not exists
*/
int unregister_trans_observer(Trans_observer *observer, void *p);

/**
   Register a binlog storage observer

   @param observer The binlog storage observer to register
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer already exists
*/
int register_binlog_storage_observer(Binlog_storage_observer *observer, void *p);

/**
   Unregister a binlog storage observer

   @param observer The binlog storage observer to unregister
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer not exists
*/
int unregister_binlog_storage_observer(Binlog_storage_observer *observer, void *p);

/**
   Register a binlog transmit observer

   @param observer The binlog transmit observer to register
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer already exists
*/
int register_binlog_transmit_observer(Binlog_transmit_observer *observer, void *p);

/**
   Unregister a binlog transmit observer

   @param observer The binlog transmit observer to unregister
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer not exists
*/
int unregister_binlog_transmit_observer(Binlog_transmit_observer *observer, void *p);

/**
   Register a binlog relay IO (slave IO thread) observer

   @param observer The binlog relay IO observer to register
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer already exists
*/
int register_binlog_relay_io_observer(Binlog_relay_IO_observer *observer, void *p);

/**
   Unregister a binlog relay IO (slave IO thread) observer

   @param observer The binlog relay IO observer to unregister
   @param p pointer to the internal plugin structure

   @retval 0 Sucess
   @retval 1 Observer not exists
*/
int unregister_binlog_relay_io_observer(Binlog_relay_IO_observer *observer, void *p);

/**
   Connect to master

   This function can only used in the slave I/O thread context, and
   will use the same master information to do the connection.

   @code
   MYSQL *mysql = mysql_init(NULL);
   if (rpl_connect_master(mysql))
   {
     // do stuff with the connection
   }
   mysql_close(mysql); // close the connection
   @endcode
   
   @param mysql address of MYSQL structure to use, pass NULL will
   create a new one

   @return address of MYSQL structure on success, NULL on failure
*/
MYSQL *rpl_connect_master(MYSQL *mysql);

/**
   Set thread entering a condition

   This function should be called before putting a thread to wait for
   a condition. @a mutex should be held before calling this
   function. After being waken up, @f thd_exit_cond should be called.

   @param thd      The thread entering the condition, NULL means current thread
   @param cond     The condition the thread is going to wait for
   @param mutex    The mutex associated with the condition, this must be
                   held before call this function
   @param msg      The new process message for the thread
*/
const char* thd_enter_cond(MYSQL_THD thd, mysql_cond_t *cond,
                           mysql_mutex_t *mutex, const char *msg);

/**
   Set thread leaving a condition

   This function should be called after a thread being waken up for a
   condition.

   @param thd      The thread entering the condition, NULL means current thread
   @param old_msg  The process message, ususally this should be the old process
                   message before calling @f thd_enter_cond
*/
void thd_exit_cond(MYSQL_THD thd, const char *old_msg);

/**
   Get the value of user variable as an integer.

   This function will return the value of variable @a name as an
   integer. If the original value of the variable is not an integer,
   the value will be converted into an integer.

   @param name     user variable name
   @param value    pointer to return the value
   @param null_value if not NULL, the function will set it to true if
   the value of variable is null, set to false if not

   @retval 0 Success
   @retval 1 Variable not found
*/
int get_user_var_int(const char *name,
                     long long int *value, int *null_value);

/**
   Get the value of user variable as a double precision float number.

   This function will return the value of variable @a name as real
   number. If the original value of the variable is not a real number,
   the value will be converted into a real number.

   @param name     user variable name
   @param value    pointer to return the value
   @param null_value if not NULL, the function will set it to true if
   the value of variable is null, set to false if not

   @retval 0 Success
   @retval 1 Variable not found
*/
int get_user_var_real(const char *name,
                      double *value, int *null_value);

/**
   Get the value of user variable as a string.

   This function will return the value of variable @a name as
   string. If the original value of the variable is not a string,
   the value will be converted into a string.

   @param name     user variable name
   @param value    pointer to the value buffer
   @param len      length of the value buffer
   @param precision precision of the value if it is a float number
   @param null_value if not NULL, the function will set it to true if
   the value of variable is null, set to false if not

   @retval 0 Success
   @retval 1 Variable not found
*/
int get_user_var_str(const char *name,
                     char *value, unsigned long len,
                     unsigned int precision, int *null_value);

  

#ifdef __cplusplus
}
#endif
#endif /* REPLICATION_H */
