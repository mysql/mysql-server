/* Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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
#ifndef MTS_SUBMODE_H
#define MTS_SUBMODE_H

#include <stddef.h>
#include <sys/types.h>
#include <atomic>
#include <utility>

#include "binlog_event.h"      // SEQ_UNINIT
#include "my_inttypes.h"
#include "my_thread_local.h"   // my_thread_id
#include "mysql/udf_registration_types.h"
#include "prealloced_array.h"  // Prealloced_array

class Log_event;
class Query_log_event;
class Relay_log_info;
class Slave_worker;
class THD;
struct TABLE;

typedef Prealloced_array<Slave_worker*, 4> Slave_worker_array;

enum enum_mts_parallel_type {
  /* Parallel slave based on Database name */
  MTS_PARALLEL_TYPE_DB_NAME= 0,
  /* Parallel slave based on group information from Binlog group commit */
  MTS_PARALLEL_TYPE_LOGICAL_CLOCK= 1
};

// Extend the following class as per requirement for each sub mode
class Mts_submode
{
private:
protected:

  /* Parallel type */
  enum_mts_parallel_type  type;
public:
  Mts_submode(){}
  inline enum_mts_parallel_type get_type(){return type;}
  // pure virtual methods. Should be extended in the derieved class

  /* Logic to schedule the next event. called at the B event for each
     transaction */
  virtual int schedule_next_event(Relay_log_info* rli,
                                   Log_event *ev)= 0;

  /* logic to attach temp tables Should be extended in the derieved class */
  virtual void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                  Query_log_event *ev)= 0;

  /* logic to detach temp tables. Should be extended in the derieved class  */
  virtual void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                  Query_log_event *ev)= 0;

  /* returns the least occupied worker. Should be extended in the derieved class  */
  virtual Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                                  Slave_worker_array *ws,
                                                  Log_event *ev)= 0;
  /* wait for slave workers to finish */
  virtual int wait_for_workers_to_finish(Relay_log_info *rli,
                                         Slave_worker *ignore= NULL)=0;

  virtual ~Mts_submode(){}
};

/**
  DB partitioned submode
  For significance of each method check definition of Mts_submode
*/
class Mts_submode_database: public Mts_submode
{
public:
  Mts_submode_database()
  {
    type= MTS_PARALLEL_TYPE_DB_NAME;
  }
  int schedule_next_event(Relay_log_info* rli, Log_event *ev);
  void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                          Query_log_event *ev);
  Slave_worker* get_least_occupied_worker(Relay_log_info*,
                                          Slave_worker_array *ws,
                                          Log_event*);
  ~Mts_submode_database(){};
  int wait_for_workers_to_finish(Relay_log_info  *rli,
                                 Slave_worker *ignore= NULL);
};

/**
  Parallelization using Master parallelization information
  For significance of each method check definition of Mts_submode
 */
class Mts_submode_logical_clock: public Mts_submode
{
private:
  bool first_event, force_new_group;
  bool is_new_group;
  uint delegated_jobs;
  /* "instant" value of committed transactions low-water-mark */
  std::atomic<longlong> last_lwm_timestamp;
  /* GAQ index corresponding to the min commit point */
  ulong last_lwm_index;
  longlong last_committed;
  longlong sequence_number;

public:
  uint jobs_done;
  bool is_error;
  /*
    the logical timestamp of the olderst transaction that is being waited by
    before to resume scheduling.
  */
  std::atomic<longlong> min_waited_timestamp;
  /*
    Committed transactions and those that are waiting for their commit parents
    comprise sequences whose items are identified as GAQ index.
    An empty sequence is described by the following magic value which can't
    be in the GAQ legitimate range.
    todo: an alternative could be to pass a magic value to the constructor.
    E.g GAQ.size as a good candidate being outside of the valid range.
    That requires further wl6314 refactoring in activation/deactivation
    of the scheduler.
  */
  static const ulong INDEX_UNDEF= (ulong) -1;

protected:
  std::pair<uint, my_thread_id> get_server_and_thread_id(TABLE* table);
  Slave_worker* get_free_worker(Relay_log_info *rli);
public:
  Mts_submode_logical_clock();
  int schedule_next_event(Relay_log_info* rli, Log_event *ev);
  void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                          Query_log_event*);
  Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                          Slave_worker_array *ws,
                                          Log_event *ev);
  /* Sets the force new group variable */
  inline void start_new_group()
  {
    force_new_group= true;
    first_event= true;
  }
  /**
    Withdraw the delegated_job increased by the group.
  */
  void withdraw_delegated_job()
  {
    delegated_jobs--;
  }
  int wait_for_workers_to_finish(Relay_log_info  *rli,
                                 Slave_worker *ignore= NULL);
  bool wait_for_last_committed_trx(Relay_log_info* rli,
                                   longlong last_committed_arg);
  /*
    LEQ comparison of two logical timestamps follows regular rules for
    integers. SEQ_UNINIT is regarded as the least value in the clock domain.

    @param a  the lhs logical timestamp value
    @param b  the rhs logical timestamp value

    @return   true  when a "<=" b,
              false otherwise
  */
  static bool clock_leq(longlong a, longlong b)
  {
    if (a == SEQ_UNINIT)
      return true;
    else if (b == SEQ_UNINIT)
      return false;
    else
      return a <= b;
  }

  longlong get_lwm_timestamp(Relay_log_info *rli, bool need_lock);
  longlong estimate_lwm_timestamp()
  {
    return last_lwm_timestamp.load();
  };
  ~Mts_submode_logical_clock() {}
};

#endif /*MTS_SUBMODE_H*/
