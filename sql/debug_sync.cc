/* Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

/**
  == Debug Sync Facility ==

  The Debug Sync Facility allows placement of synchronization points in
  the server code by using the DEBUG_SYNC macro:

      open_tables(...)

      DEBUG_SYNC(thd, "after_open_tables");

      lock_tables(...)

  When activated, a sync point can

    - Emit a signal and/or
    - Wait for a signal

  Nomenclature:

    - signal:             A value of a global variable that persists
                          until overwritten by a new signal. The global
                          variable can also be seen as a "signal post"
                          or "flag mast". Then the signal is what is
                          attached to the "signal post" or "flag mast".

    - emit a signal:      Assign the value (the signal) to the global
                          variable ("set a flag") and broadcast a
                          global condition to wake those waiting for
                          a signal.

    - wait for a signal:  Loop over waiting for the global condition until
                          the global value matches the wait-for signal.

  By default, all sync points are inactive. They do nothing (except to
  burn a couple of CPU cycles for checking if they are active).

  A sync point becomes active when an action is requested for it.
  To do so, put a line like this in the test case file:

      SET DEBUG_SYNC= 'after_open_tables SIGNAL opened WAIT_FOR flushed';

  This activates the sync point 'after_open_tables'. It requests it to
  emit the signal 'opened' and wait for another thread to emit the signal
  'flushed' when the thread's execution runs through the sync point.

  For every sync point there can be one action per thread only. Every
  thread can request multiple actions, but only one per sync point. In
  other words, a thread can activate multiple sync points.

  Here is an example how to activate and use the sync points:

      --connection conn1
      SET DEBUG_SYNC= 'after_open_tables SIGNAL opened WAIT_FOR flushed';
      send INSERT INTO t1 VALUES(1);
          --connection conn2
          SET DEBUG_SYNC= 'now WAIT_FOR opened';
          SET DEBUG_SYNC= 'after_abort_locks SIGNAL flushed';
          FLUSH TABLE t1;

  When conn1 runs through the INSERT statement, it hits the sync point
  'after_open_tables'. It notices that it is active and executes its
  action. It emits the signal 'opened' and waits for another thread to
  emit the signal 'flushed'.

  conn2 waits immediately at the special sync point 'now' for another
  thread to emit the 'opened' signal.

  A signal remains in effect until it is overwritten. If conn1 signals
  'opened' before conn2 reaches 'now', conn2 will still find the 'opened'
  signal. It does not wait in this case.

  When conn2 reaches 'after_abort_locks', it signals 'flushed', which lets
  conn1 awake.

  Normally the activation of a sync point is cleared when it has been
  executed. Sometimes it is necessary to keep the sync point active for
  another execution. You can add an execute count to the action:

      SET DEBUG_SYNC= 'name SIGNAL sig EXECUTE 3';

  This sets the signal point's activation counter to 3. Each execution
  decrements the counter. After the third execution the sync point
  becomes inactive.

  One of the primary goals of this facility is to eliminate sleeps from
  the test suite. In most cases it should be possible to rewrite test
  cases so that they do not need to sleep. (But this facility cannot
  synchronize multiple processes.) However, to support test development,
  and as a last resort, sync point waiting times out. There is a default
  timeout, but it can be overridden:

      SET DEBUG_SYNC= 'name WAIT_FOR sig TIMEOUT 10 EXECUTE 2';

  TIMEOUT 0 is special: If the signal is not present, the wait times out
  immediately.

  When a wait timed out (even on TIMEOUT 0), a warning is generated so
  that it shows up in the test result.

  You can throw an error message and kill the query when a synchronization
  point is hit a certain number of times:

      SET DEBUG_SYNC= 'name HIT_LIMIT 3';

  Or combine it with signal and/or wait:

      SET DEBUG_SYNC= 'name SIGNAL sig EXECUTE 2 HIT_LIMIT 3';

  Here the first two hits emit the signal, the third hit returns the error
  message and kills the query.

  For cases where you are not sure that an action is taken and thus
  cleared in any case, you can force to clear (deactivate) a sync point:

      SET DEBUG_SYNC= 'name CLEAR';

  If you want to clear all actions and clear the global signal, use:

      SET DEBUG_SYNC= 'RESET';

  This is the only way to reset the global signal to an empty string.

  For testing of the facility itself you can execute a sync point just
  as if it had been hit:

      SET DEBUG_SYNC= 'name TEST';


  === Formal Syntax ===

  The string to "assign" to the DEBUG_SYNC variable can contain:

      {RESET |
       <sync point name> TEST |
       <sync point name> CLEAR |
       <sync point name> {{SIGNAL <signal name> |
                           WAIT_FOR <signal name> [TIMEOUT <seconds>]}
                          [EXECUTE <count>] &| HIT_LIMIT <count>}

  Here '&|' means 'and/or'. This means that one of the sections
  separated by '&|' must be present or both of them.


  === Activation/Deactivation ===

  The facility is an optional part of the MySQL server.
  It is enabled in a debug server by default.

      ./configure --enable-debug-sync

  The Debug Sync Facility, when compiled in, is disabled by default. It
  can be enabled by a mysqld command line option:

      --debug-sync-timeout[=default_wait_timeout_value_in_seconds]

  'default_wait_timeout_value_in_seconds' is the default timeout for the
  WAIT_FOR action. If set to zero, the facility stays disabled.

  The facility is enabled by default in the test suite, but can be
  disabled with:

      mysql-test-run.pl ... --debug-sync-timeout=0 ...

  Likewise the default wait timeout can be set:

      mysql-test-run.pl ... --debug-sync-timeout=10 ...

  The command line option influences the readable value of the system
  variable 'debug_sync'.

  * If the facility is not compiled in, the system variable does not exist.

  * If --debug-sync-timeout=0 the value of the variable reads as "OFF".

  * Otherwise the value reads as "ON - current signal: " followed by the
    current signal string, which can be empty.

  The readable variable value is the same, regardless if read as global
  or session value.

  Setting the 'debug-sync' system variable requires 'SUPER' privilege.
  You can never read back the string that you assigned to the variable,
  unless you assign the value that the variable does already have. But
  that would give a parse error. A syntactically correct string is
  parsed into a debug sync action and stored apart from the variable value.


  === Implementation ===

  Pseudo code for a sync point:

      #define DEBUG_SYNC(thd, sync_point_name)
                if (unlikely(opt_debug_sync_timeout))
                  debug_sync(thd, STRING_WITH_LEN(sync_point_name))

  The sync point performs a binary search in a sorted array of actions
  for this thread.

  The SET DEBUG_SYNC statement adds a requested action to the array or
  overwrites an existing action for the same sync point. When it adds a
  new action, the array is sorted again.


  === A typical synchronization pattern ===

  There are quite a few places in MySQL, where we use a synchronization
  pattern like this:

  mysql_mutex_lock(&mutex);
  thd->enter_cond(&condition_variable, &mutex, new_message);
  #if defined(ENABLE_DEBUG_SYNC)
  if (!thd->killed && !end_of_wait_condition)
     DEBUG_SYNC(thd, "sync_point_name");
  #endif
  while (!thd->killed && !end_of_wait_condition)
    mysql_cond_wait(&condition_variable, &mutex);
  thd->exit_cond(old_message);

  Here some explanations:

  thd->enter_cond() is used to register the condition variable and the
  mutex in thd->mysys_var. This is done to allow the thread to be
  interrupted (killed) from its sleep. Another thread can find the
  condition variable to signal and mutex to use for synchronization in
  this thread's THD::mysys_var.

  thd->enter_cond() requires the mutex to be acquired in advance.

  thd->exit_cond() unregisters the condition variable and mutex and
  releases the mutex.

  If you want to have a Debug Sync point with the wait, please place it
  behind enter_cond(). Only then you can safely decide, if the wait will
  be taken. Also you will have THD::proc_info correct when the sync
  point emits a signal. DEBUG_SYNC sets its own proc_info, but restores
  the previous one before releasing its internal mutex. As soon as
  another thread sees the signal, it does also see the proc_info from
  before entering the sync point. In this case it will be "new_message",
  which is associated with the wait that is to be synchronized.

  In the example above, the wait condition is repeated before the sync
  point. This is done to skip the sync point, if no wait takes place.
  The sync point is before the loop (not inside the loop) to have it hit
  once only. It is possible that the condition variable is signaled
  multiple times without the wait condition to be true.

  A bit off-topic: At some places, the loop is taken around the whole
  synchronization pattern:

  while (!thd->killed && !end_of_wait_condition)
  {
    mysql_mutex_lock(&mutex);
    thd->enter_cond(&condition_variable, &mutex, new_message);
    if (!thd->killed [&& !end_of_wait_condition])
    {
      [DEBUG_SYNC(thd, "sync_point_name");]
      mysql_cond_wait(&condition_variable, &mutex);
    }
    thd->exit_cond(old_message);
  }

  Note that it is important to repeat the test for thd->killed after
  enter_cond(). Otherwise the killing thread may kill this thread after
  it tested thd->killed in the loop condition and before it registered
  the condition variable and mutex in enter_cond(). In this case, the
  killing thread does not know that this thread is going to wait on a
  condition variable. It would just set THD::killed. But if we would not
  test it again, we would go asleep though we are killed. If the killing
  thread would kill us when we are after the second test, but still
  before sleeping, we hold the mutex, which is registered in mysys_var.
  The killing thread would try to acquire the mutex before signaling
  the condition variable. Since the mutex is only released implicitly in
  mysql_cond_wait(), the signaling happens at the right place. We
  have a safe synchronization.

  === Co-work with the DBUG facility ===

  When running the MySQL test suite with the --debug command line
  option, the Debug Sync Facility writes trace messages to the DBUG
  trace. The following shell commands proved very useful in extracting
  relevant information:

  egrep 'query:|debug_sync_exec:' mysql-test/var/log/mysqld.1.trace

  It shows all executed SQL statements and all actions executed by
  synchronization points.

  Sometimes it is also useful to see, which synchronization points have
  been run through (hit) with or without executing actions. Then add
  "|debug_sync_point:" to the egrep pattern.

  === Further reading ===

  For a discussion of other methods to synchronize threads see
  http://forge.mysql.com/wiki/MySQL_Internals_Test_Synchronization

  For complete syntax tests, functional tests, and examples see the test
  case debug_sync.test.

  See also worklog entry WL#4259 - Test Synchronization Facility
*/

#include "debug_sync.h"

#if defined(ENABLED_DEBUG_SYNC)

/*
  Due to weaknesses in our include files, we need to include
  sql_priv.h here. To have THD declared, we need to include
  sql_class.h. This includes log_event.h, which in turn requires
  declarations from sql_priv.h (e.g. OPTION_AUTO_IS_NULL).
  sql_priv.h includes almost everything, so is sufficient here.
*/
#include "sql_priv.h"
#include "sql_parse.h"

/*
  Action to perform at a synchronization point.
  NOTE: This structure is moved around in memory by realloc(), qsort(),
        and memmove(). Do not add objects with non-trivial constuctors
        or destructors, which might prevent moving of this structure
        with these functions.
*/
struct st_debug_sync_action
{
  ulong         activation_count;       /* max(hit_limit, execute) */
  ulong         hit_limit;              /* hits before kill query */
  ulong         execute;                /* executes before self-clear */
  ulong         timeout;                /* wait_for timeout */
  String        signal;                 /* signal to emit */
  String        wait_for;               /* signal to wait for */
  String        sync_point;             /* sync point name */
  bool          need_sort;              /* if new action, array needs sort */
};

/* Debug sync control. Referenced by THD. */
struct st_debug_sync_control
{
  st_debug_sync_action  *ds_action;             /* array of actions */
  uint                  ds_active;              /* # active actions */
  uint                  ds_allocated;           /* # allocated actions */
  ulonglong             dsp_hits;               /* statistics */
  ulonglong             dsp_executed;           /* statistics */
  ulonglong             dsp_max_active;         /* statistics */
  /*
    thd->proc_info points at unsynchronized memory.
    It must not go away as long as the thread exists.
  */
  char                  ds_proc_info[80];       /* proc_info string */
};


/**
  Definitions for the debug sync facility.
  1. Global string variable to hold a "signal" ("signal post", "flag mast").
  2. Global condition variable for signaling and waiting.
  3. Global mutex to synchronize access to the above.
*/
struct st_debug_sync_globals
{
  String                ds_signal;              /* signal variable */
  mysql_cond_t          ds_cond;                /* condition variable */
  mysql_mutex_t         ds_mutex;               /* mutex variable */
  ulonglong             dsp_hits;               /* statistics */
  ulonglong             dsp_executed;           /* statistics */
  ulonglong             dsp_max_active;         /* statistics */
};
static st_debug_sync_globals debug_sync_global; /* All globals in one object */

/**
  Callback pointer for C files.
*/
extern "C" void (*debug_sync_C_callback_ptr)(const char *, size_t);

/**
  Callbacks from C files.
*/
C_MODE_START
static void debug_sync_C_callback(const char *, size_t);
static int debug_sync_qsort_cmp(const void *, const void *);
C_MODE_END

/**
  Callback for debug sync, to be used by C files. See thr_lock.c for example.

  @description

    We cannot place a sync point directly in C files (like those in mysys or
    certain storage engines written mostly in C like MyISAM or Maria). Because
    they are C code and do not include sql_priv.h. So they do not know the
    macro DEBUG_SYNC(thd, sync_point_name). The macro needs a 'thd' argument.
    Hence it cannot be used in files outside of the sql/ directory.

    The workaround is to call back simple functions like this one from
    non-sql/ files.

    We want to allow modules like thr_lock to be used without sql/ and
    especially without Debug Sync. So we cannot just do a simple call
    of the callback function. Instead we provide a global pointer in
    the other file, which is to be set to the callback by Debug Sync.
    If the pointer is not set, no call back will be done. If Debug
    Sync sets the pointer to a callback function like this one, it will
    be called. That way thr_lock.c does not have an undefined reference
    to Debug Sync and can be used without it. Debug Sync, in contrast,
    has an undefined reference to that pointer and thus requires
    thr_lock to be linked too. But this is not a problem as it is part
    of the MySQL server anyway.

  @note
    The callback pointer in C files is set only if debug sync is
    initialized. And this is done only if opt_debug_sync_timeout is set.
*/

static void debug_sync_C_callback(const char *sync_point_name,
                                  size_t name_len)
{
  if (unlikely(opt_debug_sync_timeout))
    debug_sync(current_thd, sync_point_name, name_len);
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_debug_sync_globals_ds_mutex;

static PSI_mutex_info all_debug_sync_mutexes[]=
{
  { &key_debug_sync_globals_ds_mutex, "DEBUG_SYNC::mutex", PSI_FLAG_GLOBAL}
};

static PSI_cond_key key_debug_sync_globals_ds_cond;

static PSI_cond_info all_debug_sync_conds[]=
{
  { &key_debug_sync_globals_ds_cond, "DEBUG_SYNC::cond", PSI_FLAG_GLOBAL}
};

static void init_debug_sync_psi_keys(void)
{
  const char* category= "sql";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_debug_sync_mutexes);
  PSI_server->register_mutex(category, all_debug_sync_mutexes, count);

  count= array_elements(all_debug_sync_conds);
  PSI_server->register_cond(category, all_debug_sync_conds, count);
}
#endif /* HAVE_PSI_INTERFACE */


/**
  Initialize the debug sync facility at server start.

  @return status
    @retval     0       ok
    @retval     != 0    error
*/

int debug_sync_init(void)
{
  DBUG_ENTER("debug_sync_init");

#ifdef HAVE_PSI_INTERFACE
  init_debug_sync_psi_keys();
#endif

  if (opt_debug_sync_timeout)
  {
    int rc;

    /* Initialize the global variables. */
    debug_sync_global.ds_signal.length(0);
    if ((rc= mysql_cond_init(key_debug_sync_globals_ds_cond,
                             &debug_sync_global.ds_cond, NULL)) ||
        (rc= mysql_mutex_init(key_debug_sync_globals_ds_mutex,
                              &debug_sync_global.ds_mutex,
                              MY_MUTEX_INIT_FAST)))
      DBUG_RETURN(rc); /* purecov: inspected */

    /* Set the call back pointer in C files. */
    debug_sync_C_callback_ptr= debug_sync_C_callback;
  }

  DBUG_RETURN(0);
}


/**
  End the debug sync facility.

  @description
    This is called at server shutdown or after a thread initialization error.
*/

void debug_sync_end(void)
{
  DBUG_ENTER("debug_sync_end");

  /* End the facility only if it had been initialized. */
  if (debug_sync_C_callback_ptr)
  {
    /* Clear the call back pointer in C files. */
    debug_sync_C_callback_ptr= NULL;

    /* Destroy the global variables. */
    debug_sync_global.ds_signal.free();
    mysql_cond_destroy(&debug_sync_global.ds_cond);
    mysql_mutex_destroy(&debug_sync_global.ds_mutex);

    /* Print statistics. */
    {
      char llbuff[22];
      sql_print_information("Debug sync points hit:                   %22s",
                            llstr(debug_sync_global.dsp_hits, llbuff));
      sql_print_information("Debug sync points executed:              %22s",
                            llstr(debug_sync_global.dsp_executed, llbuff));
      sql_print_information("Debug sync points max active per thread: %22s",
                            llstr(debug_sync_global.dsp_max_active, llbuff));
    }
  }

  DBUG_VOID_RETURN;
}


/* purecov: begin tested */

/**
  Disable the facility after lack of memory if no error can be returned.

  @note
    Do not end the facility here because the global variables can
    be in use by other threads.
*/

static void debug_sync_emergency_disable(void)
{
  DBUG_ENTER("debug_sync_emergency_disable");

  opt_debug_sync_timeout= 0;

  DBUG_PRINT("debug_sync",
             ("Debug Sync Facility disabled due to lack of memory."));
  sql_print_error("Debug Sync Facility disabled due to lack of memory.");

  DBUG_VOID_RETURN;
}

/* purecov: end */


/**
  Initialize the debug sync facility at thread start.

  @param[in]    thd             thread handle
*/

void debug_sync_init_thread(THD *thd)
{
  DBUG_ENTER("debug_sync_init_thread");
  DBUG_ASSERT(thd);

  if (opt_debug_sync_timeout)
  {
    thd->debug_sync_control= (st_debug_sync_control*)
      my_malloc(sizeof(st_debug_sync_control), MYF(MY_WME | MY_ZEROFILL));
    if (!thd->debug_sync_control)
    {
      /*
        Error is reported by my_malloc().
        We must disable the facility. We have no way to return an error.
      */
      debug_sync_emergency_disable(); /* purecov: tested */
    }
  }

  DBUG_VOID_RETURN;
}


/**
  End the debug sync facility at thread end.

  @param[in]    thd             thread handle
*/

void debug_sync_end_thread(THD *thd)
{
  DBUG_ENTER("debug_sync_end_thread");
  DBUG_ASSERT(thd);

  if (thd->debug_sync_control)
  {
    st_debug_sync_control *ds_control= thd->debug_sync_control;

    /*
      This synchronization point can be used to synchronize on thread end.
      This is the latest point in a THD's life, where this can be done.
    */
    DEBUG_SYNC(thd, "thread_end");

    if (ds_control->ds_action)
    {
      st_debug_sync_action *action= ds_control->ds_action;
      st_debug_sync_action *action_end= action + ds_control->ds_allocated;
      for (; action < action_end; action++)
      {
        action->signal.free();
        action->wait_for.free();
        action->sync_point.free();
      }
      my_free(ds_control->ds_action);
    }

    /* Statistics. */
    mysql_mutex_lock(&debug_sync_global.ds_mutex);
    debug_sync_global.dsp_hits+=           ds_control->dsp_hits;
    debug_sync_global.dsp_executed+=       ds_control->dsp_executed;
    if (debug_sync_global.dsp_max_active < ds_control->dsp_max_active)
      debug_sync_global.dsp_max_active=    ds_control->dsp_max_active;
    mysql_mutex_unlock(&debug_sync_global.ds_mutex);

    my_free(ds_control);
    thd->debug_sync_control= NULL;
  }

  DBUG_VOID_RETURN;
}


/**
  Move a string by length.

  @param[out]   to              buffer for the resulting string
  @param[in]    to_end          end of buffer
  @param[in]    from            source string
  @param[in]    length          number of bytes to copy

  @return       pointer to end of copied string
*/

static char *debug_sync_bmove_len(char *to, char *to_end,
                                  const char *from, size_t length)
{
  DBUG_ASSERT(to);
  DBUG_ASSERT(to_end);
  DBUG_ASSERT(!length || from);
  set_if_smaller(length, (size_t) (to_end - to));
  memcpy(to, from, length);
  return (to + length);
}


#if !defined(DBUG_OFF)

/**
  Create a string that describes an action.

  @param[out]   result          buffer for the resulting string
  @param[in]    size            size of result buffer
  @param[in]    action          action to describe
*/

static void debug_sync_action_string(char *result, uint size,
                                     st_debug_sync_action *action)
{
  char  *wtxt= result;
  char  *wend= wtxt + size - 1; /* Allow emergency '\0'. */
  DBUG_ASSERT(result);
  DBUG_ASSERT(action);

  /* If an execute count is present, signal or wait_for are needed too. */
  DBUG_ASSERT(!action->execute ||
              action->signal.length() || action->wait_for.length());

  if (action->execute)
  {
    if (action->signal.length())
    {
      wtxt= debug_sync_bmove_len(wtxt, wend, STRING_WITH_LEN("SIGNAL "));
      wtxt= debug_sync_bmove_len(wtxt, wend, action->signal.ptr(),
                                 action->signal.length());
    }
    if (action->wait_for.length())
    {
      if ((wtxt == result) && (wtxt < wend))
        *(wtxt++)= ' ';
      wtxt= debug_sync_bmove_len(wtxt, wend, STRING_WITH_LEN(" WAIT_FOR "));
      wtxt= debug_sync_bmove_len(wtxt, wend, action->wait_for.ptr(),
                                 action->wait_for.length());

      if (action->timeout != opt_debug_sync_timeout)
      {
        wtxt+= my_snprintf(wtxt, wend - wtxt, " TIMEOUT %lu", action->timeout);
      }
    }
    if (action->execute != 1)
    {
      wtxt+= my_snprintf(wtxt, wend - wtxt, " EXECUTE %lu", action->execute);
    }
  }
  if (action->hit_limit)
  {
    wtxt+= my_snprintf(wtxt, wend - wtxt, "%sHIT_LIMIT %lu",
                       (wtxt == result) ? "" : " ", action->hit_limit);
  }

  /*
    If (wtxt == wend) string may not be terminated.
    There is one byte left for an emergency termination.
  */
  *wtxt= '\0';
}


/**
  Print actions.

  @param[in]    thd             thread handle
*/

static void debug_sync_print_actions(THD *thd)
{
  st_debug_sync_control *ds_control= thd->debug_sync_control;
  uint                  idx;
  DBUG_ENTER("debug_sync_print_actions");
  DBUG_ASSERT(thd);

  if (!ds_control)
    DBUG_VOID_RETURN;

  for (idx= 0; idx < ds_control->ds_active; idx++)
  {
    const char *dsp_name= ds_control->ds_action[idx].sync_point.c_ptr();
    char action_string[256];

    debug_sync_action_string(action_string, sizeof(action_string),
                             ds_control->ds_action + idx);
    DBUG_PRINT("debug_sync_list", ("%s %s", dsp_name, action_string));
  }

  DBUG_VOID_RETURN;
}

#endif /* !defined(DBUG_OFF) */


/**
  Compare two actions by sync point name length, string.

  @param[in]    arg1            reference to action1
  @param[in]    arg2            reference to action2

  @return       difference
    @retval     == 0            length1/string1 is same as length2/string2
    @retval     < 0             length1/string1 is smaller
    @retval     > 0             length1/string1 is bigger
*/

static int debug_sync_qsort_cmp(const void* arg1, const void* arg2)
{
  st_debug_sync_action *action1= (st_debug_sync_action*) arg1;
  st_debug_sync_action *action2= (st_debug_sync_action*) arg2;
  int diff;
  DBUG_ASSERT(action1);
  DBUG_ASSERT(action2);

  if (!(diff= action1->sync_point.length() - action2->sync_point.length()))
    diff= memcmp(action1->sync_point.ptr(), action2->sync_point.ptr(),
                 action1->sync_point.length());

  return diff;
}


/**
  Find a debug sync action.

  @param[in]    actionarr       array of debug sync actions
  @param[in]    quantity        number of actions in array
  @param[in]    dsp_name        name of debug sync point to find
  @param[in]    name_len        length of name of debug sync point

  @return       action
    @retval     != NULL         found sync point in array
    @retval     NULL            not found

  @description
    Binary search. Array needs to be sorted by length, sync point name.
*/

static st_debug_sync_action *debug_sync_find(st_debug_sync_action *actionarr,
                                             int quantity,
                                             const char *dsp_name,
                                             uint name_len)
{
  st_debug_sync_action  *action;
  int                   low ;
  int                   high ;
  int                   mid ;
  int                   diff ;
  DBUG_ASSERT(actionarr);
  DBUG_ASSERT(dsp_name);
  DBUG_ASSERT(name_len);

  low= 0;
  high= quantity;

  while (low < high)
  {
    mid= (low + high) / 2;
    action= actionarr + mid;
    if (!(diff= name_len - action->sync_point.length()) &&
        !(diff= memcmp(dsp_name, action->sync_point.ptr(), name_len)))
      return action;
    if (diff > 0)
      low= mid + 1;
    else
      high= mid - 1;
  }

  if (low < quantity)
  {
    action= actionarr + low;
    if ((name_len == action->sync_point.length()) &&
        !memcmp(dsp_name, action->sync_point.ptr(), name_len))
      return action;
  }

  return NULL;
}


/**
  Reset the debug sync facility.

  @param[in]    thd             thread handle

  @description
    Remove all actions of this thread.
    Clear the global signal.
*/

static void debug_sync_reset(THD *thd)
{
  st_debug_sync_control *ds_control= thd->debug_sync_control;
  DBUG_ENTER("debug_sync_reset");
  DBUG_ASSERT(thd);
  DBUG_ASSERT(ds_control);

  /* Remove all actions of this thread. */
  ds_control->ds_active= 0;

  /* Clear the global signal. */
  mysql_mutex_lock(&debug_sync_global.ds_mutex);
  debug_sync_global.ds_signal.length(0);
  mysql_mutex_unlock(&debug_sync_global.ds_mutex);

  DBUG_VOID_RETURN;
}


/**
  Remove a debug sync action.

  @param[in]    ds_control      control object
  @param[in]    action          action to be removed

  @description
    Removing an action mainly means to decrement the ds_active counter.
    But if the action is between other active action in the array, then
    the array needs to be shrinked. The active actions above the one to
    be removed have to be moved down by one slot.
*/

static void debug_sync_remove_action(st_debug_sync_control *ds_control,
                                     st_debug_sync_action *action)
{
  uint dsp_idx= action - ds_control->ds_action;
  DBUG_ENTER("debug_sync_remove_action");
  DBUG_ASSERT(ds_control);
  DBUG_ASSERT(ds_control == current_thd->debug_sync_control);
  DBUG_ASSERT(action);
  DBUG_ASSERT(dsp_idx < ds_control->ds_active);

  /* Decrement the number of currently active actions. */
  ds_control->ds_active--;

  /*
    If this was not the last active action in the array, we need to
    shift remaining active actions down to keep the array gap-free.
    Otherwise binary search might fail or take longer than necessary at
    least. Also new actions are always put to the end of the array.
  */
  if (ds_control->ds_active > dsp_idx)
  {
    /*
      Do not make save_action an object of class st_debug_sync_action.
      Its destructor would tamper with the String pointers.
    */
    uchar save_action[sizeof(st_debug_sync_action)];

    /*
      Copy the to-be-removed action object to temporary storage before
      the shift copies the string pointers over. Do not use assignment
      because it would use assignment operator methods for the Strings.
      This would copy the strings. The shift below overwrite the string
      pointers without freeing them first. By using memmove() we save
      the pointers, which are overwritten by the shift.
    */
    memmove(save_action, action, sizeof(st_debug_sync_action));

    /* Move actions down. */
    memmove(ds_control->ds_action + dsp_idx,
            ds_control->ds_action + dsp_idx + 1,
            (ds_control->ds_active - dsp_idx) *
            sizeof(st_debug_sync_action));

    /*
      Copy back the saved action object to the now free array slot. This
      replaces the double references of String pointers that have been
      produced by the shift. Again do not use an assignment operator to
      avoid string allocation/copy.
    */
    memmove(ds_control->ds_action + ds_control->ds_active, save_action,
            sizeof(st_debug_sync_action));
  }

  DBUG_VOID_RETURN;
}


/**
  Get a debug sync action.

  @param[in]    thd             thread handle
  @param[in]    dsp_name        debug sync point name
  @param[in]    name_len        length of sync point name

  @return       action
    @retval     != NULL         ok
    @retval     NULL            error

  @description
    Find the debug sync action for a debug sync point or make a new one.
*/

static st_debug_sync_action *debug_sync_get_action(THD *thd,
                                                   const char *dsp_name,
                                                   uint name_len)
{
  st_debug_sync_control *ds_control= thd->debug_sync_control;
  st_debug_sync_action  *action;
  DBUG_ENTER("debug_sync_get_action");
  DBUG_ASSERT(thd);
  DBUG_ASSERT(dsp_name);
  DBUG_ASSERT(name_len);
  DBUG_ASSERT(ds_control);
  DBUG_PRINT("debug_sync", ("sync_point: '%.*s'", (int) name_len, dsp_name));
  DBUG_PRINT("debug_sync", ("active: %u  allocated: %u",
                            ds_control->ds_active, ds_control->ds_allocated));

  /* There cannot be more active actions than allocated. */
  DBUG_ASSERT(ds_control->ds_active <= ds_control->ds_allocated);
  /* If there are active actions, the action array must be present. */
  DBUG_ASSERT(!ds_control->ds_active || ds_control->ds_action);

  /* Try to reuse existing action if there is one for this sync point. */
  if (ds_control->ds_active &&
      (action= debug_sync_find(ds_control->ds_action, ds_control->ds_active,
                               dsp_name, name_len)))
  {
    /* Reuse an already active sync point action. */
    DBUG_ASSERT((uint)(action - ds_control->ds_action) < ds_control->ds_active);
    DBUG_PRINT("debug_sync", ("reuse action idx: %ld",
                              (long) (action - ds_control->ds_action)));
  }
  else
  {
    /* Create a new action. */
    int dsp_idx= ds_control->ds_active++;
    set_if_bigger(ds_control->dsp_max_active, ds_control->ds_active);
    if (ds_control->ds_active > ds_control->ds_allocated)
    {
      uint new_alloc= ds_control->ds_active + 3;
      void *new_action= my_realloc(ds_control->ds_action,
                                   new_alloc * sizeof(st_debug_sync_action),
                                   MYF(MY_WME | MY_ALLOW_ZERO_PTR));
      if (!new_action)
      {
        /* Error is reported by my_malloc(). */
        goto err; /* purecov: tested */
      }
      ds_control->ds_action= (st_debug_sync_action*) new_action;
      ds_control->ds_allocated= new_alloc;
      /* Clear memory as we do not run string constructors here. */
      bzero((uchar*) (ds_control->ds_action + dsp_idx),
            (new_alloc - dsp_idx) * sizeof(st_debug_sync_action));
    }
    DBUG_PRINT("debug_sync", ("added action idx: %u", dsp_idx));
    action= ds_control->ds_action + dsp_idx;
    if (action->sync_point.copy(dsp_name, name_len, system_charset_info))
    {
      /* Error is reported by my_malloc(). */
      goto err; /* purecov: tested */
    }
    action->need_sort= TRUE;
  }
  DBUG_ASSERT(action >= ds_control->ds_action);
  DBUG_ASSERT(action < ds_control->ds_action + ds_control->ds_active);
  DBUG_PRINT("debug_sync", ("action: 0x%lx  array: 0x%lx  count: %u",
                            (long) action, (long) ds_control->ds_action,
                            ds_control->ds_active));

  DBUG_RETURN(action);

  /* purecov: begin tested */
 err:
  DBUG_RETURN(NULL);
  /* purecov: end */
}


/**
  Set a debug sync action.

  @param[in]    thd             thread handle
  @param[in]    action          synchronization action

  @return       status
    @retval     FALSE           ok
    @retval     TRUE            error

  @description
    This is called from the debug sync parser. It arms the action for
    the requested sync point. If the action parsed into an empty action,
    it is removed instead.

    Setting an action for a sync point means to make the sync point
    active. When it is hit it will execute this action.

    Before parsing, we "get" an action object. This is placed at the
    end of the thread's action array unless the requested sync point
    has an action already.

    Then the parser fills the action object from the request string.

    Finally the action is "set" for the sync point. If it was parsed
    to be empty, it is removed from the array. If it did belong to a
    sync point before, the sync point becomes inactive. If the action
    became non-empty and it did not belong to a sync point before (it
    was added at the end of the action array), the action array needs
    to be sorted by sync point.

    If the sync point name is "now", it is executed immediately.
*/

static bool debug_sync_set_action(THD *thd, st_debug_sync_action *action)
{
  st_debug_sync_control *ds_control= thd->debug_sync_control;
  bool is_dsp_now= FALSE;
  DBUG_ENTER("debug_sync_set_action");
  DBUG_ASSERT(thd);
  DBUG_ASSERT(action);
  DBUG_ASSERT(ds_control);

  action->activation_count= max(action->hit_limit, action->execute);
  if (!action->activation_count)
  {
    debug_sync_remove_action(ds_control, action);
    DBUG_PRINT("debug_sync", ("action cleared"));
  }
  else
  {
    const char *dsp_name= action->sync_point.c_ptr();
    DBUG_EXECUTE("debug_sync", {
        /* Functions as DBUG_PRINT args can change keyword and line nr. */
        const char *sig_emit= action->signal.c_ptr();
        const char *sig_wait= action->wait_for.c_ptr();
        DBUG_PRINT("debug_sync",
                   ("sync_point: '%s'  activation_count: %lu  hit_limit: %lu  "
                    "execute: %lu  timeout: %lu  signal: '%s'  wait_for: '%s'",
                    dsp_name, action->activation_count,
                    action->hit_limit, action->execute, action->timeout,
                    sig_emit, sig_wait));});

    /* Check this before sorting the array. action may move. */
    is_dsp_now= !my_strcasecmp(system_charset_info, dsp_name, "now");

    if (action->need_sort)
    {
      action->need_sort= FALSE;
      /* Sort actions by (name_len, name). */
      my_qsort(ds_control->ds_action, ds_control->ds_active,
               sizeof(st_debug_sync_action), debug_sync_qsort_cmp);
    }
  }
  DBUG_EXECUTE("debug_sync_list", debug_sync_print_actions(thd););

  /* Execute the special sync point 'now' if activated above. */
  if (is_dsp_now)
  {
    DEBUG_SYNC(thd, "now");
    /*
      If HIT_LIMIT for sync point "now" was 1, the execution of the sync
      point decremented it to 0. In this case the following happened:

      - an error message was reported with my_error() and
      - the statement was killed with thd->killed= THD::KILL_QUERY.

      If a statement reports an error, it must not call send_ok().
      The calling functions will not call send_ok(), if we return TRUE
      from this function.

      thd->killed is also set if the wait is interrupted from a
      KILL or KILL QUERY statement. In this case, no error is reported
      and shall not be reported as a result of SET DEBUG_SYNC.
      Hence, we check for the first condition above.
    */
    if (thd->is_error())
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/**
  Extract a token from a string.

  @param[out]     token_p         returns start of token
  @param[out]     token_length_p  returns length of token
  @param[in,out]  ptr             current string pointer, adds '\0' terminators

  @return       string pointer or NULL
    @retval     != NULL         ptr behind token terminator or at string end
    @retval     NULL            no token found in remainder of string

  @note
    This function assumes that the string is in system_charset_info,
    that this charset is single byte for ASCII NUL ('\0'), that no
    character except of ASCII NUL ('\0') contains a byte with value 0,
    and that ASCII NUL ('\0') is used as the string terminator.

    This function needs to return tokens that are terminated with ASCII
    NUL ('\0'). The tokens are used in my_strcasecmp(). Unfortunately
    there is no my_strncasecmp().

    To return the last token without copying it, we require the input
    string to be nul terminated.

  @description
    This function skips space characters at string begin.

    It returns a pointer to the first non-space character in *token_p.

    If no non-space character is found before the string terminator
    ASCII NUL ('\0'), the function returns NULL. *token_p and
    *token_length_p remain unchanged in this case (they are not set).

    The function takes a space character or an ASCII NUL ('\0') as a
    terminator of the token. The space character could be multi-byte.

    It returns the length of the token in bytes, excluding the
    terminator, in *token_length_p.

    If the terminator of the token is ASCII NUL ('\0'), it returns a
    pointer to the terminator (string end).

    If the terminator is a space character, it replaces the the first
    byte of the terminator character by ASCII NUL ('\0'), skips the (now
    corrupted) terminator character, and skips all following space
    characters. It returns a pointer to the next non-space character or
    to the string terminator ASCII NUL ('\0').
*/

static char *debug_sync_token(char **token_p, uint *token_length_p, char *ptr)
{
  DBUG_ASSERT(token_p);
  DBUG_ASSERT(token_length_p);
  DBUG_ASSERT(ptr);

  /* Skip leading space */
  while (my_isspace(system_charset_info, *ptr))
    ptr+= my_mbcharlen(system_charset_info, (uchar) *ptr);

  if (!*ptr)
  {
    ptr= NULL;
    goto end;
  }

  /* Get token start. */
  *token_p= ptr;

  /* Find token end. */
  while (*ptr && !my_isspace(system_charset_info, *ptr))
    ptr+= my_mbcharlen(system_charset_info, (uchar) *ptr);

  /* Get token length. */
  *token_length_p= ptr - *token_p;

  /* If necessary, terminate token. */
  if (*ptr)
  {
    /* Get terminator character length. */
    uint mbspacelen= my_mbcharlen(system_charset_info, (uchar) *ptr);

    /* Terminate token. */
    *ptr= '\0';

    /* Skip the terminator. */
    ptr+= mbspacelen;

    /* Skip trailing space */
    while (my_isspace(system_charset_info, *ptr))
      ptr+= my_mbcharlen(system_charset_info, (uchar) *ptr);
  }

 end:
  return ptr;
}


/**
  Extract a number from a string.

  @param[out]   number_p        returns number
  @param[in]    actstrptr       current pointer in action string

  @return       string pointer or NULL
    @retval     != NULL         ptr behind token terminator or at string end
    @retval     NULL            no token found or token is not valid number

  @note
    The same assumptions about charset apply as for debug_sync_token().

  @description
    This function fetches a token from the string and converts it
    into a number.

    If there is no token left in the string, or the token is not a valid
    decimal number, NULL is returned. The result in *number_p is
    undefined in this case.
*/

static char *debug_sync_number(ulong *number_p, char *actstrptr)
{
  char                  *ptr;
  char                  *ept;
  char                  *token;
  uint                  token_length;
  DBUG_ASSERT(number_p);
  DBUG_ASSERT(actstrptr);

  /* Get token from string. */
  if (!(ptr= debug_sync_token(&token, &token_length, actstrptr)))
    goto end;

  *number_p= strtoul(token, &ept, 10);
  if (*ept)
    ptr= NULL;

 end:
  return ptr;
}


/**
  Evaluate a debug sync action string.

  @param[in]        thd             thread handle
  @param[in,out]    action_str      action string to receive '\0' terminators

  @return           status
    @retval         FALSE           ok
    @retval         TRUE            error

  @description
    This is called when the DEBUG_SYNC system variable is set.
    Parse action string, build a debug sync action, activate it.

    Before parsing, we "get" an action object. This is placed at the
    end of the thread's action array unless the requested sync point
    has an action already.

    Then the parser fills the action object from the request string.

    Finally the action is "set" for the sync point. This means that the
    sync point becomes active or inactive, depending on the action
    values.

  @note
    The input string needs to be ASCII NUL ('\0') terminated. We split
    nul-terminated tokens in it without copy.

  @see the function comment of debug_sync_token() for more constraints
    for the string.
*/

static bool debug_sync_eval_action(THD *thd, char *action_str)
{
  st_debug_sync_action  *action= NULL;
  const char            *errmsg;
  char                  *ptr;
  char                  *token;
  uint                  token_length= 0;
  DBUG_ENTER("debug_sync_eval_action");
  DBUG_ASSERT(thd);
  DBUG_ASSERT(action_str);

  /*
    Get debug sync point name. Or a special command.
  */
  if (!(ptr= debug_sync_token(&token, &token_length, action_str)))
  {
    errmsg= "Missing synchronization point name";
    goto err;
  }

  /*
    If there is a second token, the first one is the sync point name.
  */
  if (*ptr)
  {
    /* Get an action object to collect the requested action parameters. */
    action= debug_sync_get_action(thd, token, token_length);
    if (!action)
    {
      /* Error message is sent. */
      DBUG_RETURN(TRUE); /* purecov: tested */
    }
  }

  /*
    Get kind of action to be taken at sync point.
  */
  if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
  {
    /* No action present. Try special commands. Token unchanged. */

    /*
      Try RESET.
    */
    if (!my_strcasecmp(system_charset_info, token, "RESET"))
    {
      /* It is RESET. Reset all actions and global signal. */
      debug_sync_reset(thd);
      goto end;
    }

    /* Token unchanged. It still contains sync point name. */
    errmsg= "Missing action after synchronization point name '%.*s'";
    goto err;
  }

  /*
    Check for pseudo actions first. Start with actions that work on
    an existing action.
  */
  DBUG_ASSERT(action);

  /*
    Try TEST.
  */
  if (!my_strcasecmp(system_charset_info, token, "TEST"))
  {
    /* It is TEST. Nothing must follow it. */
    if (*ptr)
    {
      errmsg= "Nothing must follow action TEST";
      goto err;
    }

    /* Execute sync point. */
    debug_sync(thd, action->sync_point.ptr(), action->sync_point.length());
    /* Fix statistics. This was not a real hit of the sync point. */
    thd->debug_sync_control->dsp_hits--;
    goto end;
  }

  /*
    Now check for actions that define a new action.
    Initialize action. Do not use bzero(). Strings may have malloced.
  */
  action->activation_count= 0;
  action->hit_limit= 0;
  action->execute= 0;
  action->timeout= 0;
  action->signal.length(0);
  action->wait_for.length(0);

  /*
    Try CLEAR.
  */
  if (!my_strcasecmp(system_charset_info, token, "CLEAR"))
  {
    /* It is CLEAR. Nothing must follow it. */
    if (*ptr)
    {
      errmsg= "Nothing must follow action CLEAR";
      goto err;
    }

    /* Set (clear/remove) action. */
    goto set_action;
  }

  /*
    Now check for real sync point actions.
  */

  /*
    Try SIGNAL.
  */
  if (!my_strcasecmp(system_charset_info, token, "SIGNAL"))
  {
    /* It is SIGNAL. Signal name must follow. */
    if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
    {
      errmsg= "Missing signal name after action SIGNAL";
      goto err;
    }
    if (action->signal.copy(token, token_length, system_charset_info))
    {
      /* Error is reported by my_malloc(). */
      /* purecov: begin tested */
      errmsg= NULL;
      goto err;
      /* purecov: end */
    }

    /* Set default for EXECUTE option. */
    action->execute= 1;

    /* Get next token. If none follows, set action. */
    if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
      goto set_action;
  }

  /*
    Try WAIT_FOR.
  */
  if (!my_strcasecmp(system_charset_info, token, "WAIT_FOR"))
  {
    /* It is WAIT_FOR. Wait_for signal name must follow. */
    if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
    {
      errmsg= "Missing signal name after action WAIT_FOR";
      goto err;
    }
    if (action->wait_for.copy(token, token_length, system_charset_info))
    {
      /* Error is reported by my_malloc(). */
      /* purecov: begin tested */
      errmsg= NULL;
      goto err;
      /* purecov: end */
    }

    /* Set default for EXECUTE and TIMEOUT options. */
    action->execute= 1;
    action->timeout= opt_debug_sync_timeout;

    /* Get next token. If none follows, set action. */
    if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
      goto set_action;

    /*
      Try TIMEOUT.
    */
    if (!my_strcasecmp(system_charset_info, token, "TIMEOUT"))
    {
      /* It is TIMEOUT. Number must follow. */
      if (!(ptr= debug_sync_number(&action->timeout, ptr)))
      {
        errmsg= "Missing valid number after TIMEOUT";
        goto err;
      }

      /* Get next token. If none follows, set action. */
      if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
        goto set_action;
    }
  }

  /*
    Try EXECUTE.
  */
  if (!my_strcasecmp(system_charset_info, token, "EXECUTE"))
  {
    /*
      EXECUTE requires either SIGNAL and/or WAIT_FOR to be present.
      In this case action->execute has been preset to 1.
    */
    if (!action->execute)
    {
      errmsg= "Missing action before EXECUTE";
      goto err;
    }

    /* Number must follow. */
    if (!(ptr= debug_sync_number(&action->execute, ptr)))
    {
      errmsg= "Missing valid number after EXECUTE";
      goto err;
    }

    /* Get next token. If none follows, set action. */
    if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
      goto set_action;
  }

  /*
    Try HIT_LIMIT.
  */
  if (!my_strcasecmp(system_charset_info, token, "HIT_LIMIT"))
  {
    /* Number must follow. */
    if (!(ptr= debug_sync_number(&action->hit_limit, ptr)))
    {
      errmsg= "Missing valid number after HIT_LIMIT";
      goto err;
    }

    /* Get next token. If none follows, set action. */
    if (!(ptr= debug_sync_token(&token, &token_length, ptr)))
      goto set_action;
  }

  errmsg= "Illegal or out of order stuff: '%.*s'";

 err:
  if (errmsg)
  {
    /*
      NOTE: errmsg must either have %.*s or none % at all.
      It can be NULL if an error message is already reported
      (e.g. by my_malloc()).
    */
    set_if_smaller(token_length, 64); /* Limit error message length. */
    my_printf_error(ER_PARSE_ERROR, errmsg, MYF(0), token_length, token);
  }
  if (action)
    debug_sync_remove_action(thd->debug_sync_control, action);
  DBUG_RETURN(TRUE);

 set_action:
  DBUG_RETURN(debug_sync_set_action(thd, action));

 end:
  DBUG_RETURN(FALSE);
}

/**
  Set the system variable 'debug_sync'.

  @param[in]    thd             thread handle
  @param[in]    var             set variable request

  @return       status
    @retval     FALSE           ok, variable is set
    @retval     TRUE            error, variable could not be set

  @note
    "Setting" of the system variable 'debug_sync' does not mean to
    assign a value to it as usual. Instead a debug sync action is parsed
    from the input string and stored apart from the variable value.

  @note
    For efficiency reasons, the action string parser places '\0'
    terminators in the string. So we need to take a copy here.
*/

bool debug_sync_update(THD *thd, char *val_str)
{
  DBUG_ENTER("debug_sync_update");
  DBUG_PRINT("debug_sync", ("set action: '%s'", val_str));

  /*
    debug_sync_eval_action() places '\0' in the string, which itself
    must be '\0' terminated.
  */
  DBUG_RETURN(opt_debug_sync_timeout ?
              debug_sync_eval_action(thd, val_str) :
              FALSE);
}


/**
  Retrieve the value of the system variable 'debug_sync'.

  @param[in]    thd             thread handle

  @return       string
    @retval     != NULL         ok, string pointer
    @retval     NULL            memory allocation error

  @note
    The value of the system variable 'debug_sync' reflects if
    the facility is enabled ("ON") or disabled (default, "OFF").

    When "ON", the current signal is added.
*/

uchar *debug_sync_value_ptr(THD *thd)
{
  char *value;
  DBUG_ENTER("debug_sync_value_ptr");

  if (opt_debug_sync_timeout)
  {
    static char on[]= "ON - current signal: '"; 

    // Ensure exclusive access to debug_sync_global.ds_signal
    mysql_mutex_lock(&debug_sync_global.ds_mutex);

    size_t lgt= (sizeof(on) /* includes '\0' */ +
                 debug_sync_global.ds_signal.length() + 1 /* for '\'' */);
    char *vend;
    char *vptr;

    if ((value= (char*) alloc_root(thd->mem_root, lgt)))
    {
      vend= value + lgt - 1; /* reserve space for '\0'. */
      vptr= debug_sync_bmove_len(value, vend, STRING_WITH_LEN(on));
      vptr= debug_sync_bmove_len(vptr, vend, debug_sync_global.ds_signal.ptr(),
                                 debug_sync_global.ds_signal.length());
      if (vptr < vend)
        *(vptr++)= '\'';
      *vptr= '\0'; /* We have one byte reserved for the worst case. */
    }
    mysql_mutex_unlock(&debug_sync_global.ds_mutex);
  }
  else
  {
    /* purecov: begin tested */
    value= const_cast<char*>("OFF");
    /* purecov: end */
  }

  DBUG_RETURN((uchar*) value);
}


/**
  Execute requested action at a synchronization point.

  @param[in]    thd                 thread handle
  @param[in]    action              action to be executed

  @note
    This is to be called only if activation count > 0.
*/

static void debug_sync_execute(THD *thd, st_debug_sync_action *action)
{
#ifndef DBUG_OFF
  const char *dsp_name= action->sync_point.c_ptr();
  const char *sig_emit= action->signal.c_ptr();
  const char *sig_wait= action->wait_for.c_ptr();
#endif
  DBUG_ENTER("debug_sync_execute");
  DBUG_ASSERT(thd);
  DBUG_ASSERT(action);
  DBUG_PRINT("debug_sync",
             ("sync_point: '%s'  activation_count: %lu  hit_limit: %lu  "
              "execute: %lu  timeout: %lu  signal: '%s'  wait_for: '%s'",
              dsp_name, action->activation_count, action->hit_limit,
              action->execute, action->timeout, sig_emit, sig_wait));

  DBUG_ASSERT(action->activation_count);
  action->activation_count--;

  if (action->execute)
  {
    const char *UNINIT_VAR(old_proc_info);

    action->execute--;

    /*
      If we will be going to wait, set proc_info for the PROCESSLIST table.
      Do this before emitting the signal, so other threads can see it
      if they awake before we enter_cond() below.
    */
    if (action->wait_for.length())
    {
      st_debug_sync_control *ds_control= thd->debug_sync_control;
      strxnmov(ds_control->ds_proc_info, sizeof(ds_control->ds_proc_info)-1,
               "debug sync point: ", action->sync_point.c_ptr(), NullS);
      old_proc_info= thd->proc_info;
      thd_proc_info(thd, ds_control->ds_proc_info);
    }

    /*
      Take mutex to ensure that only one thread access
      debug_sync_global.ds_signal at a time.  Need to take mutex for
      read access too, to create a memory barrier in order to avoid that
      threads just reads an old cached version of the signal.
    */
    mysql_mutex_lock(&debug_sync_global.ds_mutex);

    if (action->signal.length())
    {
      /* Copy the signal to the global variable. */
      if (debug_sync_global.ds_signal.copy(action->signal))
      {
        /*
          Error is reported by my_malloc().
          We must disable the facility. We have no way to return an error.
        */
        debug_sync_emergency_disable(); /* purecov: tested */
      }
      /* Wake threads waiting in a sync point. */
      mysql_cond_broadcast(&debug_sync_global.ds_cond);
      DBUG_PRINT("debug_sync_exec", ("signal '%s'  at: '%s'",
                                     sig_emit, dsp_name));
    } /* end if (action->signal.length()) */

    if (action->wait_for.length())
    {
      mysql_mutex_t *old_mutex;
      mysql_cond_t  *old_cond;
      int             error= 0;
      struct timespec abstime;

      /*
        We don't use enter_cond()/exit_cond(). They do not save old
        mutex and cond. This would prohibit the use of DEBUG_SYNC
        between other places of enter_cond() and exit_cond().

        We need to check for existence of thd->mysys_var to also make
        it possible to use DEBUG_SYNC framework in scheduler when this
        variable has been set to NULL.
      */
      if (thd->mysys_var)
      {
        old_mutex= thd->mysys_var->current_mutex;
        old_cond= thd->mysys_var->current_cond;
        thd->mysys_var->current_mutex= &debug_sync_global.ds_mutex;
        thd->mysys_var->current_cond= &debug_sync_global.ds_cond;
      }
      else
        old_mutex= NULL;

      set_timespec(abstime, action->timeout);
      DBUG_EXECUTE("debug_sync_exec", {
          /* Functions as DBUG_PRINT args can change keyword and line nr. */
          const char *sig_glob= debug_sync_global.ds_signal.c_ptr();
          DBUG_PRINT("debug_sync_exec",
                     ("wait for '%s'  at: '%s'  curr: '%s'",
                      sig_wait, dsp_name, sig_glob));});

      /*
        Wait until global signal string matches the wait_for string.
        Interrupt when thread or query is killed or facility disabled.
        The facility can become disabled when some thread cannot get
        the required dynamic memory allocated.
      */
      while (stringcmp(&debug_sync_global.ds_signal, &action->wait_for) &&
             !thd->killed && opt_debug_sync_timeout)
      {
        error= mysql_cond_timedwait(&debug_sync_global.ds_cond,
                                    &debug_sync_global.ds_mutex,
                                    &abstime);
        DBUG_EXECUTE("debug_sync", {
            /* Functions as DBUG_PRINT args can change keyword and line nr. */
            const char *sig_glob= debug_sync_global.ds_signal.c_ptr();
            DBUG_PRINT("debug_sync",
                       ("awoke from %s  global: %s  error: %d",
                        sig_wait, sig_glob, error));});
        if (error == ETIMEDOUT || error == ETIME)
        {
          push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                       ER_DEBUG_SYNC_TIMEOUT, ER(ER_DEBUG_SYNC_TIMEOUT));
          break;
        }
        error= 0;
      }
      DBUG_EXECUTE("debug_sync_exec",
                   if (thd->killed)
                     DBUG_PRINT("debug_sync_exec",
                                ("killed %d from '%s'  at: '%s'",
                                 thd->killed, sig_wait, dsp_name));
                   else
                     DBUG_PRINT("debug_sync_exec",
                                ("%s from '%s'  at: '%s'",
                                 error ? "timeout" : "resume",
                                 sig_wait, dsp_name)););

      /*
        We don't use enter_cond()/exit_cond(). They do not save old
        mutex and cond. This would prohibit the use of DEBUG_SYNC
        between other places of enter_cond() and exit_cond(). The
        protected mutex must always unlocked _before_ mysys_var->mutex
        is locked. (See comment in THD::exit_cond().)
      */
      mysql_mutex_unlock(&debug_sync_global.ds_mutex);
      if (old_mutex)
      {
        mysql_mutex_lock(&thd->mysys_var->mutex);
        thd->mysys_var->current_mutex= old_mutex;
        thd->mysys_var->current_cond= old_cond;
        thd_proc_info(thd, old_proc_info);
        mysql_mutex_unlock(&thd->mysys_var->mutex);
      }
      else
        thd_proc_info(thd, old_proc_info);
    }
    else
    {
      /* In case we don't wait, we just release the mutex. */
      mysql_mutex_unlock(&debug_sync_global.ds_mutex);
    } /* end if (action->wait_for.length()) */

  } /* end if (action->execute) */

  /* hit_limit is zero for infinite. Don't decrement unconditionally. */
  if (action->hit_limit)
  {
    if (!--action->hit_limit)
    {
      thd->killed= THD::KILL_QUERY;
      my_error(ER_DEBUG_SYNC_HIT_LIMIT, MYF(0));
    }
    DBUG_PRINT("debug_sync_exec", ("hit_limit: %lu  at: '%s'",
                                   action->hit_limit, dsp_name));
  }

  DBUG_VOID_RETURN;
}


/**
  Execute requested action at a synchronization point.

  @param[in]     thd                thread handle
  @param[in]     sync_point_name    name of synchronization point
  @param[in]     name_len           length of sync point name
*/

void debug_sync(THD *thd, const char *sync_point_name, size_t name_len)
{
  st_debug_sync_control *ds_control= thd->debug_sync_control;
  st_debug_sync_action  *action;
  DBUG_ENTER("debug_sync");
  DBUG_ASSERT(thd);
  DBUG_ASSERT(sync_point_name);
  DBUG_ASSERT(name_len);
  DBUG_ASSERT(ds_control);
  DBUG_PRINT("debug_sync_point", ("hit: '%s'", sync_point_name));

  /* Statistics. */
  ds_control->dsp_hits++;

  if (ds_control->ds_active &&
      (action= debug_sync_find(ds_control->ds_action, ds_control->ds_active,
                               sync_point_name, name_len)) &&
      action->activation_count)
  {
    /* Sync point is active (action exists). */
    debug_sync_execute(thd, action);

    /* Statistics. */
    ds_control->dsp_executed++;

    /* If action became inactive, remove it to shrink the search array. */
    if (!action->activation_count)
      debug_sync_remove_action(ds_control, action);
  }

  DBUG_VOID_RETURN;
}

/**
  Define debug sync action.

  @param[in]        thd             thread handle
  @param[in]        action_str      action string

  @return           status
    @retval         FALSE           ok
    @retval         TRUE            error

  @description
    The function is similar to @c debug_sync_eval_action but is
    to be called immediately from the server code rather than 
    to be triggered by setting a value to DEBUG_SYNC system variable.

  @note
    The input string is copied prior to be fed to
    @c debug_sync_eval_action to let the latter modify it.

    Caution.
    The function allocates in THD::mem_root and therefore
    is not recommended to be deployed inside big loops.    
*/

bool debug_sync_set_action(THD *thd, const char *action_str, size_t len)
{
  bool                  rc;
  char *value;
  DBUG_ENTER("debug_sync_set_action");
  DBUG_ASSERT(thd);
  DBUG_ASSERT(action_str);
  
  value= strmake_root(thd->mem_root, action_str, len);
  rc= debug_sync_eval_action(thd, value);
  DBUG_RETURN(rc);
}


#endif /* defined(ENABLED_DEBUG_SYNC) */
