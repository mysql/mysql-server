/* Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _SQL_PROFILE_H
#define _SQL_PROFILE_H

class Item;
struct TABLE_LIST;
class THD;
typedef struct st_field_info ST_FIELD_INFO;
typedef struct st_schema_table ST_SCHEMA_TABLE;

extern ST_FIELD_INFO query_profile_statistics_info[];
int fill_query_profile_statistics_info(THD *thd, TABLE_LIST *tables, Item *cond);
int make_profile_table_for_show(THD *thd, ST_SCHEMA_TABLE *schema_table);


#define PROFILE_NONE         (uint)0
#define PROFILE_CPU          (uint)(1<<0)
#define PROFILE_MEMORY       (uint)(1<<1)
#define PROFILE_BLOCK_IO     (uint)(1<<2)
#define PROFILE_CONTEXT      (uint)(1<<3)
#define PROFILE_PAGE_FAULTS  (uint)(1<<4)
#define PROFILE_IPC          (uint)(1<<5)
#define PROFILE_SWAPS        (uint)(1<<6)
#define PROFILE_SOURCE       (uint)(1<<16)
#define PROFILE_ALL          (uint)(~0)


#if defined(ENABLED_PROFILING)
#include "sql_priv.h"
#include "unireg.h"

#ifdef __WIN__
#include <psapi.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif


class PROF_MEASUREMENT;
class QUERY_PROFILE;
class PROFILING;


/**
  Implements a persistent FIFO using server List method names.  Not
  thread-safe.  Intended to be used on thread-local data only.  
*/
template <class T> class Queue
{
private:

  struct queue_item
  {
    T *payload;
    struct queue_item *next, *previous;
  };

  struct queue_item *first, *last;

public:
  Queue()
  {
    elements= 0;
    first= last= NULL;
  }

  void empty()
  {
    struct queue_item *i, *after_i;
    for (i= first; i != NULL; i= after_i)
    {
      after_i= i->next;
      my_free(i);
    }
    elements= 0;
  }

  ulong elements;                       /* The count of items in the Queue */

  void push_back(T *payload)
  {
    struct queue_item *new_item;

    new_item= (struct queue_item *) my_malloc(sizeof(struct queue_item), MYF(0));

    new_item->payload= payload;

    if (first == NULL)
      first= new_item;
    if (last != NULL)
    {
      DBUG_ASSERT(last->next == NULL);
      last->next= new_item;
    }
    new_item->previous= last;
    new_item->next= NULL;
    last= new_item;

    elements++;
  }

  T *pop()
  {
    struct queue_item *old_item= first;
    T *ret= NULL;

    if (first == NULL)
    {
      DBUG_PRINT("warning", ("tried to pop nonexistent item from Queue"));
      return NULL;
    }

    ret= old_item->payload;
    if (first->next != NULL)
      first->next->previous= NULL;
    else
      last= NULL;
    first= first->next;

    my_free(old_item);
    elements--;

    return ret;
  }

  bool is_empty()
  {
    DBUG_ASSERT(((elements > 0) && (first != NULL)) || ((elements == 0) || (first == NULL)));
    return (elements == 0);
  }

  void *new_iterator()
  {
    return first;
  }

  void *iterator_next(void *current)
  {
    return ((struct queue_item *) current)->next;
  }

  T *iterator_value(void *current)
  {
    return ((struct queue_item *) current)->payload;
  }

};


/**
  A single entry in a single profile.
*/
class PROF_MEASUREMENT
{
private:
  friend class QUERY_PROFILE;
  friend class PROFILING;

  QUERY_PROFILE *profile;
  char *status;
#ifdef HAVE_GETRUSAGE
  struct rusage rusage;
#elif defined(_WIN32)
  FILETIME ftKernel, ftUser;
  IO_COUNTERS io_count;
  PROCESS_MEMORY_COUNTERS mem_count;
#endif

  char *function;
  char *file;
  unsigned int line;

  ulong m_seq;
  double time_usecs;
  char *allocated_status_memory;

  void set_label(const char *status_arg, const char *function_arg, 
                  const char *file_arg, unsigned int line_arg);
  void clean_up();
  
  PROF_MEASUREMENT();
  PROF_MEASUREMENT(QUERY_PROFILE *profile_arg, const char *status_arg);
  PROF_MEASUREMENT(QUERY_PROFILE *profile_arg, const char *status_arg,
                const char *function_arg,
                const char *file_arg, unsigned int line_arg);
  ~PROF_MEASUREMENT();
  void collect();
};


/**
  The full profile for a single query, and includes multiple PROF_MEASUREMENT
  objects.
*/
class QUERY_PROFILE
{
private:
  friend class PROFILING;

  PROFILING *profiling;

  query_id_t profiling_query_id;        /* Session-specific id. */
  char *query_source;

  double m_start_time_usecs;
  double m_end_time_usecs;
  ulong m_seq_counter;
  Queue<PROF_MEASUREMENT> entries;


  QUERY_PROFILE(PROFILING *profiling_arg, const char *status_arg);
  ~QUERY_PROFILE();

  void set_query_source(char *query_source_arg, uint query_length_arg);

  /* Add a profile status change to the current profile. */
  void new_status(const char *status_arg,
              const char *function_arg,
              const char *file_arg, unsigned int line_arg);

  /* Reset the contents of this profile entry. */
  void reset();

  /* Show this profile.  This is called by PROFILING. */
  bool show(uint options);
};


/**
  Profiling state for a single THD; contains multiple QUERY_PROFILE objects.
*/
class PROFILING
{
private:
  friend class PROF_MEASUREMENT;
  friend class QUERY_PROFILE;

  /* 
    Not the system query_id, but a counter unique to profiling. 
  */
  query_id_t profile_id_counter;     
  THD *thd;
  bool keeping;
  bool enabled;

  QUERY_PROFILE *current;
  QUERY_PROFILE *last;
  Queue<QUERY_PROFILE> history;
 
  query_id_t next_profile_id() { return(profile_id_counter++); }

public:
  PROFILING();
  ~PROFILING();
  void set_query_source(char *query_source_arg, uint query_length_arg);

  void start_new_query(const char *initial_state= "starting");

  void discard_current_query();

  void finish_current_query();

  void status_change(const char *status_arg,
                     const char *function_arg,
                     const char *file_arg, unsigned int line_arg);

  inline void set_thd(THD *thd_arg) { thd= thd_arg; };

  /* SHOW PROFILES */
  bool show_profiles();

  /* ... from INFORMATION_SCHEMA.PROFILING ... */
  int fill_statistics_info(THD *thd, TABLE_LIST *tables, Item *cond);
};

#  endif /* ENABLED_PROFILING */
#endif /* _SQL_PROFILE_H */
