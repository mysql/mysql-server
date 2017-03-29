/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_RLI_PDB_H

#define RPL_RLI_PDB_H

#ifdef HAVE_REPLICATION

#include "sql_string.h"
#include "rpl_rli.h"
#include <my_sys.h>
#include <my_bitmap.h>
#include "rpl_slave.h"

/**
  Legends running throughout the module:

  C  - Coordinator
  CP - checkpoint
  W  - Worker

  B-event event that Begins a group (a transaction)
  T-event event that Terminates a group (a transaction)
*/

/* Assigned Partition Hash (APH) entry */
typedef struct st_db_worker_hash_entry
{
  uint  db_len;
  const char *db;
  Slave_worker *worker;
  /*
    The number of transaction pending on this database.
    This should only be modified under the lock slave_worker_hash_lock.
   */
  long usage;
  /*
    The list of temp tables belonging to @ db database is
    attached to an assigned @c worker to become its thd->temporary_tables.
    The list is updated with every ddl incl CREATE, DROP.
    It is removed from the entry and merged to the coordinator's
    thd->temporary_tables in case of events: slave stops, APH oversize.
  */
  TABLE* volatile temporary_tables;

  /* todo: relax concurrency to mimic record-level locking.
     That is to augmenting the entry with mutex/cond pair
     pthread_mutex_t
     pthread_cond_t
     timestamp updated_at; */

} db_worker_hash_entry;

bool init_hash_workers(ulong slave_parallel_workers);
void destroy_hash_workers(Relay_log_info*);
Slave_worker *map_db_to_worker(const char *dbname, Relay_log_info *rli,
                               db_worker_hash_entry **ptr_entry,
                               bool need_temp_tables, Slave_worker *w);
Slave_worker *get_least_occupied_worker(DYNAMIC_ARRAY *workers);
int wait_for_workers_to_finish(Relay_log_info const *rli,
                               Slave_worker *ignore= NULL);

#define SLAVE_INIT_DBS_IN_GROUP 4     // initial allocation for CGEP dynarray

#define NUMBER_OF_FIELDS_TO_IDENTIFY_WORKER 2

typedef struct slave_job_item
{
  void *data;
} Slave_job_item;

/**
   The class defines a type of queue with a predefined max size that is
   implemented using the circular memory buffer.
   That is items of the queue are accessed as indexed elements of
   the array buffer in a way that when the index value reaches
   a max value it wraps around to point to the first buffer element.
*/
class circular_buffer_queue
{
public:

  DYNAMIC_ARRAY Q;
  ulong size;           // the Size of the queue in terms of element
  ulong avail;          // first Available index to append at (next to tail)
  ulong entry;          // the head index or the entry point to the queue.
  volatile ulong len;   // actual length
  bool inited_queue;

  circular_buffer_queue(uint el_size, ulong max, uint alloc_inc= 0) :
    size(max), avail(0), entry(max), len(0), inited_queue(FALSE)
  {
    DBUG_ASSERT(size < (ulong) -1);
    if (!my_init_dynamic_array(&Q, el_size, size, alloc_inc))
      inited_queue= TRUE;
  }
  circular_buffer_queue () : inited_queue(FALSE) {}
  ~circular_buffer_queue ()
  {
    if (inited_queue)
      delete_dynamic(&Q);
  }

   /**
      Content of the being dequeued item is copied to the arg-pointer
      location.
      
      @return the queue's array index that the de-queued item
      located at, or
      an error encoded in beyond the index legacy range.
   */
  ulong de_queue(uchar *);
  /**
     Similar to de_queue but extracting happens from the tail side.
  */
  ulong de_tail(uchar *val);

  /**
    return the index where the arg item locates
           or an error encoded as a value in beyond of the legacy range
           [0, size) (value `size' is excluded).
  */
  ulong en_queue(void *item);
  /**
     return the value of @c data member of the head of the queue.
  */
  void* head_queue();
  bool   gt(ulong i, ulong k); // comparision of ordering of two entities
  /* index is within the valid range */
  bool in(ulong k) { return !empty() && 
      (entry > avail ? (k >= entry || k < avail) : (k >= entry && k < avail)); }
  bool empty() { return entry == size; }
  bool full() { return avail == size; }
};

typedef struct st_slave_job_group
{
  char *group_master_log_name;   // (actually redundant)
  /*
    T-event lop_pos filled by Worker for CheckPoint (CP)
  */
  my_off_t group_master_log_pos;

  /* 
     When relay-log name changes  allocates and fill in a new name of relay-log,
     otherwise it fills in NULL.
     Coordinator keeps track of each Worker has been notified on the updating
     to make sure the routine runs once per change.

     W checks the value at commit and memoriezes a not-NULL.
     Freeing unless NULL is left to Coordinator at CP.
  */
  char     *group_relay_log_name; // The value is last seen relay-log 
  my_off_t group_relay_log_pos;  // filled by W
  ulong worker_id;
  Slave_worker *worker;
  ulonglong total_seqno;

  my_off_t master_log_pos;       // B-event log_pos
  /* checkpoint coord are reset by periodical and special (Rotate event) CP:s */
  uint  checkpoint_seqno;
  my_off_t checkpoint_log_pos; // T-event lop_pos filled by W for CheckPoint
  char*    checkpoint_log_name;
  my_off_t checkpoint_relay_log_pos; // T-event lop_pos filled by W for CheckPoint
  char*    checkpoint_relay_log_name;
  volatile uchar done;  // Flag raised by W,  read and reset by Coordinator
  ulong    shifted;     // shift the last CP bitmap at receiving a new CP
  time_t   ts;          // Group's timestampt to update Seconds_behind_master
#ifndef DBUG_OFF
  bool     notified;    // to debug group_master_log_name change notification
#endif
  /*
    Coordinator fills the struct with defaults and options at starting of 
    a group distribution.
  */
  void reset(my_off_t master_pos, ulonglong seqno)
  {
    master_log_pos= master_pos;
    group_master_log_pos= group_relay_log_pos= 0;
    group_master_log_name= NULL; // todo: remove
    group_relay_log_name= NULL;
    worker_id= MTS_WORKER_UNDEF;
    total_seqno= seqno;
    checkpoint_log_name= NULL;
    checkpoint_log_pos= 0;
    checkpoint_relay_log_name= NULL;
    checkpoint_relay_log_pos= 0;
    checkpoint_seqno= (uint) -1;
    done= 0;
    ts= 0;
#ifndef DBUG_OFF
    notified= false;
#endif
  }
} Slave_job_group;

/**
  Group Assigned Queue whose first element identifies first gap
  in committed sequence. The head of the queue is therefore next to 
  the low-water-mark.
*/
class Slave_committed_queue : public circular_buffer_queue
{
public:
  
  bool inited;

  /* master's Rot-ev exec */
  void update_current_binlog(const char *post_rotate);

  /*
     The last checkpoint time Low-Water-Mark
  */
  Slave_job_group lwm;
  
  /* last time processed indexes for each worker */
  DYNAMIC_ARRAY last_done;

  /* the being assigned group index in GAQ */
  ulong assigned_group_index;

  Slave_committed_queue (const char *log, uint el_size, ulong max, uint n,
                         uint inc= 0)
    : circular_buffer_queue(el_size, max, inc), inited(FALSE)
  {
    uint k;
    ulonglong l= 0;
    
    if (max >= (ulong) -1 || !circular_buffer_queue::inited_queue)
      return;
    else
      inited= TRUE;
    my_init_dynamic_array(&last_done, sizeof(lwm.total_seqno), n, 0);
    for (k= 0; k < n; k++)
      insert_dynamic(&last_done, (uchar*) &l);  // empty for each Worker
    lwm.group_relay_log_name= (char *) my_malloc(FN_REFLEN + 1, MYF(0));
    lwm.group_relay_log_name[0]= 0;
  }

  ~Slave_committed_queue ()
  { 
    if (inited)
    {
      delete_dynamic(&last_done);
      my_free(lwm.group_relay_log_name);
      free_dynamic_items();  // free possibly left allocated strings in GAQ list
    }
  }

#ifndef DBUG_OFF
  bool count_done(Relay_log_info* rli);
#endif

  /* Checkpoint routine refreshes the queue */
  ulong move_queue_head(DYNAMIC_ARRAY *ws);
  /* Method is for slave shutdown time cleanup */
  void free_dynamic_items();
  /* 
     returns a pointer to Slave_job_group struct instance as indexed by arg
     in the circular buffer dyn-array 
  */
  Slave_job_group* get_job_group(ulong ind)
  {
    return (Slave_job_group*) dynamic_array_ptr(&Q, ind);
  }

  /**
     Assignes @c assigned_group_index to an index of enqueued item
     and returns it.
  */
  ulong en_queue(void *item)
  {
    return assigned_group_index= circular_buffer_queue::en_queue(item);
  }

};

class Slave_jobs_queue : public circular_buffer_queue
{
public:

  /* 
     Coordinator marks with true, Worker signals back at queue back to
     available
  */
  bool overfill;
  ulonglong waited_overfill;
};

class Slave_worker : public Relay_log_info
{
public:
  Slave_worker(Relay_log_info *rli
#ifdef HAVE_PSI_INTERFACE
               ,PSI_mutex_key *param_key_info_run_lock,
               PSI_mutex_key *param_key_info_data_lock,
               PSI_mutex_key *param_key_info_sleep_lock,
               PSI_mutex_key *param_key_info_data_cond,
               PSI_mutex_key *param_key_info_start_cond,
               PSI_mutex_key *param_key_info_stop_cond,
               PSI_mutex_key *param_key_info_sleep_cond
#endif
               , uint param_id
              );
  virtual ~Slave_worker();

  Slave_jobs_queue jobs;   // assignment queue containing events to execute
  mysql_mutex_t jobs_lock; // mutex for the jobs queue
  mysql_cond_t  jobs_cond; // condition variable for the jobs queue
  Relay_log_info *c_rli;   // pointer to Coordinator's rli
  DYNAMIC_ARRAY curr_group_exec_parts; // Current Group Executed Partitions
  bool curr_group_seen_begin; // is set to TRUE with explicit B-event
  ulong id;                 // numberic identifier of the Worker

  /*
    Worker runtime statictics
  */
  // the index in GAQ of the last processed group by this Worker
  volatile ulong last_group_done_index;
  ulonglong last_groups_assigned_index; // index of previous group assigned to worker
  ulong wq_empty_waits;  // how many times got idle
  ulong events_done;     // how many events (statements) processed
  ulong groups_done;     // how many groups (transactions) processed
  volatile int curr_jobs; // number of active  assignments
  // number of partitions allocated to the worker at point in time
  long usage_partition;
  // symmetric to rli->mts_end_group_sets_max_dbs
  bool end_group_sets_max_dbs;

  volatile bool relay_log_change_notified; // Coord sets and resets, W can read
  volatile bool checkpoint_notified; // Coord sets and resets, W can read
  volatile bool master_log_change_notified; // Coord sets and resets, W can read
  ulong bitmap_shifted;  // shift the last bitmap at receiving new CP
  // WQ current excess above the overrun level
  long wq_overrun_cnt;
  /*
    number of events starting from which Worker queue is regarded as
    close to full. The number of the excessive events yields a weight factor
    to compute Coordinator's nap.
  */
  ulong overrun_level;
  /*
     reverse to overrun: the number of events below which Worker is
     considered underruning
  */
  ulong underrun_level;
  /*
    Total of increments done to rli->mts_wq_excess_cnt on behalf of this worker.
    When WQ length is dropped below overrun the counter is reset.
  */
  ulong excess_cnt;
  /*
    Coordinates of the last CheckPoint (CP) this Worker has
    acknowledged; part of is persisent data
  */
  char checkpoint_relay_log_name[FN_REFLEN];
  ulonglong checkpoint_relay_log_pos;
  char checkpoint_master_log_name[FN_REFLEN];
  ulonglong checkpoint_master_log_pos;
  MY_BITMAP group_executed; // bitmap describes groups executed after last CP
  MY_BITMAP group_shifted;  // temporary bitmap to compute group_executed
  ulong checkpoint_seqno;   // the most significant ON bit in group_executed
  enum en_running_state
  {
    NOT_RUNNING= 0,
    RUNNING= 1,
    ERROR_LEAVING= 2,         // is set by Worker
    STOP= 3,                  // is set by Coordinator upon receiving STOP
    STOP_ACCEPTED= 4          // is set by worker upon completing job when STOP SLAVE is issued
  };
  /*
    The running status is guarded by jobs_lock mutex that a writer
    Coordinator or Worker itself needs to hold when write a new value.
  */
  en_running_state volatile running_status;
  /*
    exit_incremented indicates whether worker has contributed to max updated index.
    By default it is set to false. When the worker contibutes for the first time this
    variable is set to true.
  */
  bool exit_incremented;

  int init_worker(Relay_log_info*, ulong);
  int rli_init_info(bool);
  int flush_info(bool force= FALSE);
  static size_t get_number_worker_fields();
  void slave_worker_ends_group(Log_event*, int);
  const char *get_master_log_name();
  ulonglong get_master_log_pos() { return master_log_pos; };
  ulonglong set_master_log_pos(ulong val) { return master_log_pos= val; };
  bool commit_positions(Log_event *evt, Slave_job_group *ptr_g, bool force);
  /*
    When commit fails clear bitmap for executed worker group. Revert back the
    positions to the old positions that existed before commit using the checkpoint.

    @param Slave_job_group a pointer to Slave_job_group struct instance which
    holds group master log pos, group relay log pos and checkpoint positions.
  */
  void rollback_positions(Slave_job_group *ptr_g);
  bool reset_recovery_info();
  /**
     Different from the parent method in that this does not delete
     rli_description_event.
     The method runs by Coordinator when Worker are synched or being
     destroyed.
  */
  void set_rli_description_event(Format_description_log_event *fdle)
  {
    DBUG_ASSERT(!fdle || (running_status == Slave_worker::RUNNING && info_thd));
#ifndef DBUG_OFF
    if (fdle)
      mysql_mutex_assert_owner(&jobs_lock);
#endif

    if (fdle)
      adapt_to_master_version(fdle);
    rli_description_event= fdle;
  }

  inline void reset_gaq_index() { gaq_index= c_rli->gaq->size; };
  inline void set_gaq_index(ulong val)
  { 
    if (gaq_index == c_rli->gaq->size)
      gaq_index= val;
  };

protected:

  virtual void do_report(loglevel level, int err_code,
                         const char *msg, va_list v_args) const;

private:
  ulong gaq_index;          // GAQ index of the current assignment 
  ulonglong master_log_pos; // event's cached log_pos for possibile error report
  void end_info();
  bool read_info(Rpl_info_handler *from);
  bool write_info(Rpl_info_handler *to);
  Slave_worker& operator=(const Slave_worker& info);
  Slave_worker(const Slave_worker& info);
};

void * head_queue(Slave_jobs_queue *jobs, Slave_job_item *ret);
bool handle_slave_worker_stop(Slave_worker *worker, Slave_job_item *job_item);
bool set_max_updated_index_on_stop(Slave_worker *worker,
                                   Slave_job_item *job_item);

TABLE* mts_move_temp_table_to_entry(TABLE*, THD*, db_worker_hash_entry*);
TABLE* mts_move_temp_tables_to_thd(THD*, TABLE*);
#endif // HAVE_REPLICATION
#endif
