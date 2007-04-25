/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _SQL_PROFILE_H
#define _SQL_PROFILE_H

#if __STDC_VERSION__ < 199901L
#  if __GNUC__ >= 2
#    define __func__ __FUNCTION__
#  else
#    define __func__ _unknown_func_
extern const char * const _unknown_func_;
#  endif
#elif defined(_MSC_VER)
#  if _MSC_VER < 1300
#     define __func__ _unknown_func_
extern const char * const _unknown_func_;
#  else
#    define __func__ __FUNCTION__
#  endif
#elif defined(__BORLANDC__)
#  define __func__ __FUNC__
#else
#  define __func__ _unknown_func_
extern const char * const _unknown_func_;
#endif

extern ST_FIELD_INFO query_profile_statistics_info[];
int fill_query_profile_statistics_info(THD *thd, struct st_table_list *tables, Item *cond);


#define PROFILE_NONE         0
#define PROFILE_CPU          (1<<0)
#define PROFILE_MEMORY       (1<<1)
#define PROFILE_BLOCK_IO     (1<<2)
#define PROFILE_CONTEXT      (1<<3)
#define PROFILE_PAGE_FAULTS  (1<<4)
#define PROFILE_IPC          (1<<5)
#define PROFILE_SWAPS        (1<<6)
#define PROFILE_SOURCE       (1<<16)
#define PROFILE_ALL          (~0)


#if !defined(ENABLED_PROFILING) || !defined(COMMUNITY_SERVER)

#  define thd_proc_info(thd, msg) do { (thd)->proc_info= (msg); } while (0)

#else

#  define thd_proc_info(thd, msg)                                             \
  do {                                                                        \
    if (unlikely(((thd)->options & OPTION_PROFILING) != 0))                   \
    {                                                                         \
      (thd)->profiling.status_change((msg), __func__, __FILE__, __LINE__);    \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      (thd)->proc_info= (msg);                                                \
    }                                                                         \
  } while (0)

#include "mysql_priv.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif


class PROFILE_ENTRY;
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
      my_free((char *) i, MYF(0));
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

    my_free((char *)old_item, MYF(0));
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
class PROFILE_ENTRY
{
private:
  friend class QUERY_PROFILE;
  friend class PROFILING;

  QUERY_PROFILE *profile;
  char *status;
#ifdef HAVE_GETRUSAGE
  struct rusage rusage;
#endif

  char *function;
  char *file;
  unsigned int line;

  double time_usecs;
  char *allocated_status_memory;

  void set_status(const char *status_arg, const char *function_arg, 
                  const char *file_arg, unsigned int line_arg);
  void clean_up();
  
  PROFILE_ENTRY();
  PROFILE_ENTRY(QUERY_PROFILE *profile_arg, const char *status_arg);
  PROFILE_ENTRY(QUERY_PROFILE *profile_arg, const char *status_arg,
                const char *function_arg,
                const char *file_arg, unsigned int line_arg);
  ~PROFILE_ENTRY();
  void collect();
};


/**
  The full profile for a single query, and includes multiple PROFILE_ENTRY
  objects.
*/
class QUERY_PROFILE
{
private:
  friend class PROFILING;

  PROFILING *profiling;

  query_id_t server_query_id;           /* Global id. */
  query_id_t profiling_query_id;        /* Session-specific id. */
  char *query_source;
  PROFILE_ENTRY profile_start;
  PROFILE_ENTRY *profile_end;
  Queue<PROFILE_ENTRY> entries;


  QUERY_PROFILE(PROFILING *profiling_arg, char *query_source_arg, uint query_length_arg);
  ~QUERY_PROFILE();

  void set_query_source(char *query_source_arg, uint query_length_arg);

  /* Add a profile status change to the current profile. */
  void status(const char *status_arg,
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
  friend class PROFILE_ENTRY;
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

  /** Reset the current profile and state of profiling for the next query. */
  void reset();

  /**
    Do we intend to keep the currently collected profile?
    
    We don't keep profiles for some commands, such as SHOW PROFILE, SHOW
    PROFILES, and some SQLCOM commands which aren't useful to profile.  The
    keep() and discard() functions can be called many times, only the final
    setting when the query finishes is used to decide whether to discard the
    profile.
    
    The default is to keep the profile for all queries.
  */
  inline void keep()    { keeping= true; };

  /**
    Do we intend to keep the currently collected profile?
    @see keep()
  */
  inline void discard() { keeping= false; };

  /** 
    Stash this profile in the profile history and remove the oldest
    profile if the history queue is full, as defined by the 
    profiling_history_size system variable.
  */
  void store();

  /**
    Called with every update of the status via thd_proc_info() , and is
    therefore the main hook into the profiling code.
  */
  void status_change(const char *status_arg,
                     const char *function_arg,
                     const char *file_arg, unsigned int line_arg);

  inline void set_thd(THD *thd_arg) { thd= thd_arg; };

  /* SHOW PROFILES */
  bool show_profiles();

  /* SHOW PROFILE FOR QUERY query_id */
  bool show(uint options, uint profiling_query_id);

  /* SHOW PROFILE */
  bool show_last(uint options);

  /* ... from INFORMATION_SCHEMA.PROFILING ... */
  int fill_statistics_info(THD *thd, struct st_table_list *tables, Item *cond);
};

#  endif /* HAVE_PROFILING */
#endif /* _SQL_PROFILE_H */
