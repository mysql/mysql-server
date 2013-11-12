/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
#ifndef MTS_SUBMODE_H
#define MTS_SUBMODE_H

#include "log.h"
#include "log_event.h"
#include "rpl_rli.h"

class Mts_submode_database;
class Mts_submode_logical_clock;

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
                                                  DYNAMIC_ARRAY *ws,
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
  Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                          DYNAMIC_ARRAY *ws, Log_event *ev);
  int wait_for_workers_to_finish(Relay_log_info  *rli,
                                 Slave_worker *ignore= NULL);
  ~Mts_submode_database(){}
};

/**
  Parallelization using Master parallelization information
  For significance of each method check definition of Mts_submode
 */
class Mts_submode_logical_clock: public Mts_submode
{
private:
  uint worker_seq;
  bool first_event, force_new_group;
  int64 mts_last_known_commit_parent;
  /*
     The following are used to check if the last group has been applied
     completely, Here is how this works.

     if (!is_new_group)
     {
       delegated_jobs++;
       // schedule this group.
     }
     else
     {
       while (delegated_jobs > jobs_done)
         mts check_point_routine()...
      delegated_jobs = 1;
      jobs_done= 0;
      //schedule next event...
     }

     in mts_checkpoint routine
     {
       for every job completed by a worker,
       job_done++;
     }
     Also since both these are being done by the coordinator, we
     don't need any locks.
   */
  bool is_new_group;
  uint delegated_jobs;
  int64 commit_seq_no;
public:
  bool defer_new_group;
  uint jobs_done;

protected:
  std::pair<uint, my_thread_id> get_server_and_thread_id(TABLE* table);
  Slave_worker* get_free_worker(Relay_log_info *rli);
  bool assign_group(Relay_log_info* rli, Log_event* ev);
public:
  Mts_submode_logical_clock();
  int schedule_next_event(Relay_log_info* rli, Log_event *ev);
  void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                          DYNAMIC_ARRAY *ws, Log_event *ev);
  /* Sets the force new group variable */
  inline void start_new_group(){force_new_group= true;}
  int wait_for_workers_to_finish(Relay_log_info  *rli,
                                 Slave_worker *ignore= NULL);
  ~Mts_submode_logical_clock(){}
};

#endif /*MTS_SUBMODE_H*/
