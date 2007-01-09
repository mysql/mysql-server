/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef SQL_PROFILE_H
#define SQL_PROFILE_H

#include <sys/time.h>
#include <sys/resource.h>

#if 1
#define THD_PROC_INFO(thd, msg) do { if(unlikely((thd)->profiling.enabled)) { (thd)->profiling.status((msg), __FUNCTION__, __FILE__, __LINE__); } else { (thd)->proc_info= (msg); } } while (0)
#else
#define THD_PROC_INFO(thd, msg) do { (thd)->proc_info= (msg); } while (0)
#endif

#if 0

     struct rusage {
             struct timeval ru_utime; /* user time used */
             struct timeval ru_stime; /* system time used */
             long ru_maxrss;          /* integral max resident set size */
             long ru_ixrss;           /* integral shared text memory size */
             long ru_idrss;           /* integral unshared data size */
             long ru_isrss;           /* integral unshared stack size */
             long ru_minflt;          /* page reclaims */
             long ru_majflt;          /* page faults */
             long ru_nswap;           /* swaps */
             long ru_inblock;         /* block input operations */
             long ru_oublock;         /* block output operations */
             long ru_msgsnd;          /* messages sent */
             long ru_msgrcv;          /* messages received */
             long ru_nsignals;        /* signals received */
             long ru_nvcsw;           /* voluntary context switches */
             long ru_nivcsw;          /* involuntary context switches */
     };

#endif

#define PROFILE_NONE         0
#define PROFILE_CPU          1
#define PROFILE_MEMORY       2
#define PROFILE_BLOCK_IO     4
#define PROFILE_CONTEXT      8
#define PROFILE_PAGE_FAULTS  16
#define PROFILE_IPC          32
#define PROFILE_SWAPS        64
#define PROFILE_SOURCE       16384
#define PROFILE_ALL          32767

class PROFILE_ENTRY;
class PROFILE;
class PROFILING;

/*
  A single entry in a single profile.
*/

class PROFILE_ENTRY: public Sql_alloc
{
public:
  PROFILE   *profile;
  char      *status;
  ulonglong time;
  struct rusage rusage;

  char *function;
  char *file;
  unsigned int line;
  
  PROFILE_ENTRY();
  PROFILE_ENTRY(PROFILE *profile_arg, const char *status_arg);
  PROFILE_ENTRY(PROFILE *profile_arg, const char *status_arg,
                const char *function_arg,
                const char *file_arg, unsigned int line_arg);
  ~PROFILE_ENTRY();

  void set_status(const char *status_arg);
  void collect();
};

/*
  The full profile for a single query.  Includes multiple PROFILE_ENTRY
  objects.
*/

class PROFILE: public Sql_alloc
{
public:
  PROFILING *profiling;
  query_id_t query_id;
  PROFILE_ENTRY profile_start;
  PROFILE_ENTRY *profile_end;
  List<PROFILE_ENTRY> entries;

  PROFILE(PROFILING *profiling_arg);
  ~PROFILE();

  /* Add a profile status change to the current profile. */
  void status(const char *status_arg,
              const char *function_arg,
              const char *file_arg, unsigned int line_arg);

  /* Reset the contents of this profile entry. */
  void reset();

  /* Show this profile.  This is called by PROFILING. */
  bool show(uint options);
};

/*
  Profiling state for a single THD.  Contains multiple PROFILE objects.
*/

class PROFILING: public Sql_alloc
{
public:
  MEM_ROOT root;
  THD *thd;
  bool enabled;
  bool keeping;

  PROFILE       *current;
  PROFILE       *last;
  List<PROFILE> history;

  PROFILING();
  ~PROFILING();

  inline void set_thd(THD *thd_arg) { thd= thd_arg; };

  /*
    Should we try to collect profiling information at all?
    
    If we disable profiling, we cannot later decide to turn it back
    on for the same query.
  */
  inline void enable()  { enabled= 1; };
  inline void disable() { enabled= 0; };

  /*
    Do we intend to keep the currently collected profile?
    
    We don't keep profiles for some commands, such as SHOW PROFILE,
    SHOW PROFILES, and some SQLCOM commands which aren't useful to
    profile.  The keep() and discard() functions can be called many
    times, only the final setting when the query finishes is used
    to decide whether to discard the profile.
    
    The default is to keep the profile for all queries.
  */
  inline void keep()    { keeping= 1; };
  inline void discard() { keeping= 0; };

  void status(const char *status_arg,
              const char *function_arg,
              const char *file_arg, unsigned int line_arg);

  /* Stash this profile in the profile history. */
  void store();
  
  /* Reset the current profile and state of profiling for the next query. */
  void reset();

  /* SHOW PROFILES */
  bool show_profiles();

  /* SHOW PROFILE FOR QUERY query_id */
  bool show(uint options, uint query_id);

  /* SHOW PROFILE */
  bool show_last(uint options);
};

#endif /* SQL_PROFILE_H */
