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
class Mts_submode_master;

// Extend the following class as per requirement for each sub mode
class Mts_submode
{
private:
protected:

  /* Parallel type */
  enum_mts_parallel_type  type;
public:
  /* default constructor */
  Mts_submode(){}
  inline enum_mts_parallel_type get_type(){return type;}
  // pure virtual methods. Should be extended in the derieved class

  /* Logic to schedule the next event. called at the B event for each
     transaction */
  virtual bool schedule_next_event(Relay_log_info* rli,
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

  /* assigns the parent id to the group. Should be extended in the derieved class  */
  virtual bool assign_group_parent_id(Relay_log_info* rli, Log_event* ev)= 0;
};

/*
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
  bool schedule_next_event(Relay_log_info* rli, Log_event *ev);
  void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                          DYNAMIC_ARRAY *ws, Log_event *ev);
  bool assign_group_parent_id(Relay_log_info* rli, Log_event* ev);
};

/*
  Parallelization using Master parallelization information
  For significance of each method check definition of Mts_submode
 */
class Mts_submode_master: public Mts_submode
{
private:
  uint worker_seq;
  bool first_event, force_new_group, defer_new_group;
  int64 mts_last_known_commit_parent;
  int64 mts_last_known_parent_group_id;
protected:
  std::pair<uint, my_thread_id> get_server_and_thread_id(TABLE* table);
  Slave_worker* get_free_worker(Relay_log_info *rli);
public:
  Mts_submode_master();
  bool schedule_next_event(Relay_log_info* rli, Log_event *ev);
  void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                          DYNAMIC_ARRAY *ws, Log_event *ev);
  bool assign_group_parent_id(Relay_log_info* rli, Log_event* ev);
};

#endif /*MTS_SUBMODE_H*/
