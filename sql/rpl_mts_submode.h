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

class Mts_submode
{
private:
protected:
  enum_mts_parallel_type  type;
public:
  Mts_submode(){}
  inline enum_mts_parallel_type get_type(){return type;}
  /* pure virtual methods */
  virtual bool schedule_next_event(Relay_log_info* rli)= 0;
  virtual void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                  Query_log_event *ev)= 0;
  virtual void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                  Query_log_event *ev)= 0;
  virtual Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                                  DYNAMIC_ARRAY *ws,
                                                  Log_event *ev)= 0;
  virtual bool assign_group_parent_id(Relay_log_info* rli, Log_event* ev)= 0;
};

/* Extend the above class as per requirement  for each sub mode */
/*
  DB partitioned submode
 */
class Mts_submode_database: public Mts_submode
{
public:
  Mts_submode_database()
  {
    type= MTS_PARALLEL_TYPE_DB_NAME;
  }
  bool schedule_next_event(Relay_log_info* rli);
  void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                          DYNAMIC_ARRAY *ws, Log_event *ev);
  bool assign_group_parent_id(Relay_log_info* rli, Log_event* ev);
};

/*
  Parallelization using BGC
 */
class Mts_submode_master: public Mts_submode
{
private:
  uint worker_seq;
  bool first_event;
  int64 mts_last_known_commit_parent;
  int64 mts_last_known_parent_group_id;
protected:
  std::pair<uint, my_thread_id> get_server_and_thread_id(TABLE* table);
  Slave_worker* get_free_worker(Relay_log_info *rli);
public:
  Mts_submode_master()
  {
    type= MTS_PARALLEL_TYPE_BGC;
    first_event=true;
    mts_last_known_commit_parent= SEQ_UNINIT;
    mts_last_known_parent_group_id= -1;
  }
  bool schedule_next_event(Relay_log_info* rli);
  void attach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  void detach_temp_tables(THD *thd, const Relay_log_info* rli,
                                                      Query_log_event *ev);
  Slave_worker* get_least_occupied_worker(Relay_log_info* rli,
                                          DYNAMIC_ARRAY *ws, Log_event *ev);
  bool assign_group_parent_id(Relay_log_info* rli, Log_event* ev);
};

#endif /*MTS_SUBMODE_H*/
