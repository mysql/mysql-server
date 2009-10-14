/* Copyright (C) 2000-2008 MySQL AB, 2008 Sun Microsystems, Inc

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

/* This makes a wrapper for mutex handling to make it easier to debug mutex */

#include <my_global.h>
#if defined(TARGET_OS_LINUX) && !defined (__USE_UNIX98)
#define __USE_UNIX98			/* To get rw locks under Linux */
#endif
#ifdef SAFE_MUTEX
#define SAFE_MUTEX_DEFINED
#endif

#if defined(THREAD)
#undef SAFE_MUTEX			/* Avoid safe_mutex redefinitions */
#include "mysys_priv.h"
#include "my_static.h"
#include <m_string.h>
#include <hash.h>

#ifndef DO_NOT_REMOVE_THREAD_WRAPPERS
/* Remove wrappers */
#undef pthread_mutex_t
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#undef safe_mutex_free_deadlock_data
#ifdef HAVE_NONPOSIX_PTHREAD_MUTEX_INIT
#define pthread_mutex_init(a,b) my_pthread_noposix_mutex_init((a),(b))
#endif
#endif /* DO_NOT_REMOVE_THREAD_WRAPPERS */

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
pthread_mutexattr_t my_fast_mutexattr;
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
pthread_mutexattr_t my_errorcheck_mutexattr;
#endif
#ifdef SAFE_MUTEX_DEFINED
static pthread_mutex_t THR_LOCK_mutex;
static ulong safe_mutex_count= 0;		/* Number of mutexes created */
static ulong safe_mutex_id= 0;
my_bool safe_mutex_deadlock_detector= 1;        /* On by default */

#ifdef SAFE_MUTEX_DETECT_DESTROY
static struct st_safe_mutex_create_info_t *safe_mutex_create_root= NULL;
#endif

static my_bool add_used_to_locked_mutex(safe_mutex_t *used_mutex,
                                        safe_mutex_deadlock_t *locked_mutex);
static my_bool add_to_locked_mutex(safe_mutex_deadlock_t *locked_mutex,
                                   safe_mutex_t *current_mutex);
static my_bool remove_from_locked_mutex(safe_mutex_t *mp,
                                        safe_mutex_t *delete_mutex);
static my_bool remove_from_used_mutex(safe_mutex_deadlock_t *locked_mutex,
                                      safe_mutex_t *mutex);
static void print_deadlock_warning(safe_mutex_t *new_mutex,
                                   safe_mutex_t *conflicting_mutex);
#endif


/* Initialize all mutex handling */

void my_mutex_init()
{
  /* Initialize mutex attributes */
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  /*
    Set mutex type to "fast" a.k.a "adaptive"

    In this case the thread may steal the mutex from some other thread
    that is waiting for the same mutex.  This will save us some
    context switches but may cause a thread to 'starve forever' while
    waiting for the mutex (not likely if the code within the mutex is
    short).
  */
  pthread_mutexattr_init(&my_fast_mutexattr);
  pthread_mutexattr_settype(&my_fast_mutexattr,
                            PTHREAD_MUTEX_ADAPTIVE_NP);
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  /*
    Set mutex type to "errorcheck"
  */
  pthread_mutexattr_init(&my_errorcheck_mutexattr);
  pthread_mutexattr_settype(&my_errorcheck_mutexattr,
                            PTHREAD_MUTEX_ERRORCHECK);
#endif

#if defined(SAFE_MUTEX_DEFINED)
  safe_mutex_global_init();
#elif defined(MY_PTHREAD_FASTMUTEX)
  fastmutex_global_init();
#endif
}

void my_mutex_end()
{
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_fast_mutexattr);
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_errorcheck_mutexattr);
#endif
}


/* Initialize safe_mutex handling */

#ifdef SAFE_MUTEX_DEFINED
void safe_mutex_global_init(void)
{
  pthread_mutex_init(&THR_LOCK_mutex,MY_MUTEX_INIT_FAST);
  safe_mutex_id= safe_mutex_count= 0;
  safe_mutex_deadlock_detector= 1;

#ifdef SAFE_MUTEX_DETECT_DESTROY
  safe_mutex_create_root= 0;
#endif
}

static inline void remove_from_active_list(safe_mutex_t *mp)
{
  if (!(mp->active_flags & (MYF_NO_DEADLOCK_DETECTION | MYF_TRY_LOCK)))
  {
    /* Remove mutex from active mutex linked list */
    if (mp->next)
      mp->next->prev= mp->prev;
    if (mp->prev)
      mp->prev->next= mp->next;
    else
      *my_thread_var_mutex_in_use()= mp->next;
  }
  mp->prev= mp->next= 0;
}

/*
  We initialise the hashes for deadlock detection lazily.
  This greatly helps with performance when lots of mutexes are initiased but
  only a few of them are actually used (eg. XtraDB).
*/
static int safe_mutex_lazy_init_deadlock_detection(safe_mutex_t *mp)
{
  if (!my_multi_malloc(MY_FAE | MY_WME,
                       &mp->locked_mutex, sizeof(*mp->locked_mutex),
                       &mp->used_mutex, sizeof(*mp->used_mutex), NullS))
  {
    /* Disable deadlock handling for this mutex */
    mp->create_flags|= MYF_NO_DEADLOCK_DETECTION;
    mp->active_flags|= MYF_NO_DEADLOCK_DETECTION;
    return 1;                                   /* Error */
  }

  pthread_mutex_lock(&THR_LOCK_mutex);
  mp->id= ++safe_mutex_id;
  pthread_mutex_unlock(&THR_LOCK_mutex);
  hash_init(mp->locked_mutex, &my_charset_bin,
            1000,
            offsetof(safe_mutex_deadlock_t, id),
            sizeof(mp->id),
            0, 0, HASH_UNIQUE);
  hash_init(mp->used_mutex, &my_charset_bin,
            1000,
            offsetof(safe_mutex_t, id),
            sizeof(mp->id),
            0, 0, HASH_UNIQUE);
  return 0;
}

int safe_mutex_init(safe_mutex_t *mp,
		    const pthread_mutexattr_t *attr __attribute__((unused)),
                    const char *name,
                    myf my_flags,
		    const char *file,
		    uint line)
{
  DBUG_ENTER("safe_mutex_init");
  DBUG_PRINT("enter",("mutex: 0x%lx  name: %s", (ulong) mp, name));
  bzero((char*) mp,sizeof(*mp));
  pthread_mutex_init(&mp->global,MY_MUTEX_INIT_ERRCHK);
  pthread_mutex_init(&mp->mutex,attr);
  /* Mark that mutex is initialized */
  mp->file= file;
  mp->line= line;
  /* Skip the very common '&' prefix from the autogenerated name */
  mp->name= name[0] == '&' ? name + 1 : name;

  if (!safe_mutex_deadlock_detector)
    my_flags|= MYF_NO_DEADLOCK_DETECTION;
  /* Deadlock detection is initialised only lazily, on first use. */

  mp->create_flags= my_flags;

#ifdef SAFE_MUTEX_DETECT_DESTROY
  /*
    Monitor the freeing of mutexes.  This code depends on single thread init
    and destroy
  */
  if ((mp->info= (safe_mutex_info_t *) malloc(sizeof(safe_mutex_info_t))))
  {
    struct st_safe_mutex_info_t *info= mp->info;

    info->init_file= file;
    info->init_line= line;
    info->prev= NULL;
    info->next= NULL;

    pthread_mutex_lock(&THR_LOCK_mutex);
    if ((info->next= safe_mutex_create_root))
      safe_mutex_create_root->prev= info;
    safe_mutex_create_root= info;
    safe_mutex_count++;
    pthread_mutex_unlock(&THR_LOCK_mutex);
  }
#else
  thread_safe_increment(safe_mutex_count, &THR_LOCK_mutex);
#endif /* SAFE_MUTEX_DETECT_DESTROY */
  DBUG_RETURN(0);
}


int safe_mutex_lock(safe_mutex_t *mp, myf my_flags, const char *file,
                    uint line)
{
  int error;
  DBUG_PRINT("mutex", ("%s (0x%lx) locking", mp->name ? mp->name : "Null",
                       (ulong) mp));
  if (!mp->file)
  {
    fprintf(stderr,
	    "safe_mutex: Trying to lock unitialized mutex at %s, line %d\n",
	    file, line);
    fflush(stderr);
    abort();
  }

  pthread_mutex_lock(&mp->global);
  if (mp->count > 0)
  {
    /*
      Check that we are not trying to lock mutex twice. This is an error
      even if we are using 'try_lock' as it's not portably what happens
      if you lock the mutex many times and this is in any case bad
      behaviour that should not be encouraged
    */
    if (pthread_equal(pthread_self(),mp->thread))
    {
      fprintf(stderr,
              "safe_mutex: Trying to lock mutex at %s, line %d, when the"
              " mutex was already locked at %s, line %d in thread %s\n",
              file,line,mp->file, mp->line, my_thread_name());
      fflush(stderr);
      abort();
    }
  }
  pthread_mutex_unlock(&mp->global);

  /*
    If we are imitating trylock(), we need to take special
    precautions.

    - We cannot use pthread_mutex_lock() only since another thread can
      overtake this thread and take the lock before this thread
      causing pthread_mutex_trylock() to hang. In this case, we should
      just return EBUSY. Hence, we use pthread_mutex_trylock() to be
      able to return immediately.

    - We cannot just use trylock() and continue execution below, since
      this would generate an error and abort execution if the thread
      was overtaken and trylock() returned EBUSY . In this case, we
      instead just return EBUSY, since this is the expected behaviour
      of trylock().
   */
  if (my_flags & MYF_TRY_LOCK)
  {
    error= pthread_mutex_trylock(&mp->mutex);
    if (error == EBUSY)
      return error;
  }
  else
    error= pthread_mutex_lock(&mp->mutex);

  if (error || (error=pthread_mutex_lock(&mp->global)))
  {
    fprintf(stderr,"Got error %d when trying to lock mutex %s at %s, line %d\n",
	    error, mp->name, file, line);
    fflush(stderr);
    abort();
  }
  mp->thread= pthread_self();
  if (mp->count++)
  {
    fprintf(stderr,"safe_mutex: Error in thread libray: Got mutex %s at %s, "
            "line %d more than 1 time\n", mp->name, file,line);
    fflush(stderr);
    abort();
  }
  mp->file= file;
  mp->line= line;
  mp->active_flags= mp->create_flags | my_flags;
  pthread_mutex_unlock(&mp->global);

  /* Deadlock detection */

  mp->prev= mp->next= 0;
  if (!(mp->active_flags & (MYF_TRY_LOCK | MYF_NO_DEADLOCK_DETECTION)) &&
      (mp->used_mutex != NULL || !safe_mutex_lazy_init_deadlock_detection(mp)))
  {
    safe_mutex_t **mutex_in_use= my_thread_var_mutex_in_use();

    if (!mutex_in_use)
    {
      /* thread has not called my_thread_init() */
      mp->active_flags|= MYF_NO_DEADLOCK_DETECTION;
    }
    else
    {
      safe_mutex_t *mutex_root;
      if ((mutex_root= *mutex_in_use))   /* If not first locked */
      {
        /*
          Protect locked_mutex against changes if a mutex is deleted
        */
        pthread_mutex_lock(&THR_LOCK_mutex);

        if (!hash_search(mutex_root->locked_mutex, (uchar*) &mp->id, 0))
        {
          safe_mutex_deadlock_t *deadlock;
          safe_mutex_t *mutex;

          /* Create object to store mutex info */
          if (!(deadlock= my_malloc(sizeof(*deadlock),
                                    MYF(MY_ZEROFILL | MY_WME | MY_FAE))))
            goto abort_loop;
          deadlock->name= mp->name;
          deadlock->id= mp->id;
          deadlock->mutex= mp;
          /* The following is useful for debugging wrong mutex usage */
          deadlock->file= file;
          deadlock->line= line;

          /* Check if potential deadlock */
          mutex= mutex_root;
          do
          {
            if (hash_search(mp->locked_mutex, (uchar*) &mutex->id, 0))
            {
              print_deadlock_warning(mp, mutex);
              /* Mark wrong usage to avoid future warnings for same error */
              deadlock->warning_only= 1;
              add_to_locked_mutex(deadlock, mutex_root);
              DBUG_ASSERT(deadlock->count > 0);
              goto abort_loop;
            }
          }
          while ((mutex= mutex->next));

          /*
            Copy current mutex and all mutex that has been locked
            after current mutex (mp->locked_mutex) to all mutex that
            was locked before previous mutex (mutex_root->used_mutex)

            For example if A->B would have been done before and we
            are now locking (C) in B->C, then we would add C into
            B->locked_mutex and A->locked_mutex
          */
          my_hash_iterate(mutex_root->used_mutex,
                          (my_hash_walk_action) add_used_to_locked_mutex,
                          deadlock);

          /*
            Copy all current mutex and all mutex locked after current one
            into the prev mutex
          */
          add_used_to_locked_mutex(mutex_root, deadlock);
          DBUG_ASSERT(deadlock->count > 0);
        }
  abort_loop:
        pthread_mutex_unlock(&THR_LOCK_mutex);
      }
      /* Link mutex into mutex_in_use list */
      if ((mp->next= *mutex_in_use))
        (*mutex_in_use)->prev= mp;
      *mutex_in_use= mp;
    }
  }

  DBUG_PRINT("mutex", ("%s (0x%lx) locked", mp->name, (ulong) mp));
  return error;
}


int safe_mutex_unlock(safe_mutex_t *mp,const char *file, uint line)
{
  int error;
  DBUG_PRINT("mutex", ("%s (0x%lx) unlocking", mp->name, (ulong) mp));
  pthread_mutex_lock(&mp->global);
  if (mp->count == 0)
  {
    fprintf(stderr,
            "safe_mutex: Trying to unlock mutex %s that wasn't locked at "
            "%s, line %d\n"
            "Last used at %s, line: %d\n",
	    mp->name ? mp->name : "Null", file, line,
            mp->file ? mp->file : "Null", mp->line);
    fflush(stderr);
    abort();
  }
  if (!pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,
            "safe_mutex: Trying to unlock mutex %s at %s, line %d that was "
            "locked by "
            "another thread at: %s, line: %d\n",
	    mp->name, file, line, mp->file, mp->line);
    fflush(stderr);
    abort();
  }
  mp->thread= 0;
  mp->count--;

  remove_from_active_list(mp);

#ifdef __WIN__
  pthread_mutex_unlock(&mp->mutex);
  error=0;
#else
  error=pthread_mutex_unlock(&mp->mutex);
  if (error)
  {
    fprintf(stderr,
            "safe_mutex: Got error: %d (%d) when trying to unlock mutex "
            "%s at %s, line %d\n", error, errno, mp->name, file, line);
    fflush(stderr);
    abort();
  }
#endif /* __WIN__ */
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_cond_wait(pthread_cond_t *cond, safe_mutex_t *mp, const char *file,
		   uint line)
{
  int error;
  safe_mutex_t save_state;

  pthread_mutex_lock(&mp->global);
  if (mp->count == 0)
  {
    fprintf(stderr,
            "safe_mutex: Trying to cond_wait on a unlocked mutex %s at %s, "
            "line %d\n",
            mp->name ? mp->name : "Null", file, line);
    fflush(stderr);
    abort();
  }
  if (!pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,
            "safe_mutex: Trying to cond_wait on a mutex %s at %s, line %d "
            "that was locked by another thread at: %s, line: %d\n",
	    mp->name, file, line, mp->file, mp->line);
    fflush(stderr);
    abort();
  }

  if (mp->count-- != 1)
  {
    fprintf(stderr,
            "safe_mutex:  Count was %d on locked mutex %s at %s, line %d\n",
	    mp->count+1, mp->name, file, line);
    fflush(stderr);
    abort();
  }
  save_state= *mp;
  remove_from_active_list(mp);
  pthread_mutex_unlock(&mp->global);
  error=pthread_cond_wait(cond,&mp->mutex);
  pthread_mutex_lock(&mp->global);

  if (error)
  {
    fprintf(stderr,
            "safe_mutex: Got error: %d (%d) when doing a safe_mutex_wait on "
            "%s at %s, line %d\n", error, errno, mp->name, file, line);
    fflush(stderr);
    abort();
  }
  /* Restore state as it was before */
  mp->thread=       save_state.thread;
  mp->active_flags= save_state.active_flags;
  mp->next=         save_state.next;
  mp->prev=         save_state.prev;

  if (mp->count++)
  {
    fprintf(stderr,
	    "safe_mutex:  Count was %d in thread 0x%lx when locking mutex %s "
            "at %s, line %d\n",
	    mp->count-1, my_thread_dbug_id(), mp->name, file, line);
    fflush(stderr);
    abort();
  }
  mp->file= file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_cond_timedwait(pthread_cond_t *cond, safe_mutex_t *mp,
			struct timespec *abstime,
			const char *file, uint line)
{
  int error;
  safe_mutex_t save_state;

  pthread_mutex_lock(&mp->global);
  if (mp->count != 1 || !pthread_equal(pthread_self(),mp->thread))
  {
    fprintf(stderr,
            "safe_mutex: Trying to cond_wait at %s, line %d on a not hold "
            "mutex %s\n",
            file, line, mp->name ? mp->name : "Null");
    fflush(stderr);
    abort();
  }
  mp->count--;					/* Mutex will be released */
  save_state= *mp;
  remove_from_active_list(mp);
  pthread_mutex_unlock(&mp->global);
  error=pthread_cond_timedwait(cond,&mp->mutex,abstime);
#ifdef EXTRA_DEBUG
  if (error && (error != EINTR && error != ETIMEDOUT && error != ETIME))
  {
    fprintf(stderr,
            "safe_mutex: Got error: %d (%d) when doing a safe_mutex_timedwait "
            "on %s at %s, line %d\n",
            error, errno, mp->name, file, line);
  }
#endif
  pthread_mutex_lock(&mp->global);
  /* Restore state as it was before */
  mp->thread=       save_state.thread;
  mp->active_flags= save_state.active_flags;
  mp->next=         save_state.next;
  mp->prev=         save_state.prev;

  if (mp->count++)
  {
    fprintf(stderr,
	    "safe_mutex:  Count was %d in thread 0x%lx when locking mutex "
            "%s at %s, line %d (error: %d (%d))\n",
	    mp->count-1, my_thread_dbug_id(), mp->name, file, line,
            error, error);
    fflush(stderr);
    abort();
  }
  mp->file= file;
  mp->line=line;
  pthread_mutex_unlock(&mp->global);
  return error;
}


int safe_mutex_destroy(safe_mutex_t *mp, const char *file, uint line)
{
  int error=0;
  DBUG_ENTER("safe_mutex_destroy");
  DBUG_PRINT("enter", ("mutex: 0x%lx  name: %s", (ulong) mp, mp->name));
  if (!mp->file)
  {
    fprintf(stderr,
	    "safe_mutex: Trying to destroy unitialized mutex at %s, line %d\n",
	    file, line);
    fflush(stderr);
    abort();
  }
  if (mp->count != 0)
  {
    fprintf(stderr,
            "safe_mutex: Trying to destroy a mutex %s that was locked at %s, "
            "line %d at %s, line %d\n",
	    mp->name, mp->file, mp->line, file, line);
    fflush(stderr);
    abort();
  }

  /* Free all entries that points to this one */
  safe_mutex_free_deadlock_data(mp);

#ifdef __WIN__ 
  pthread_mutex_destroy(&mp->global);
  pthread_mutex_destroy(&mp->mutex);
#else
  if (pthread_mutex_destroy(&mp->global))
    error=1;
  if (pthread_mutex_destroy(&mp->mutex))
    error=1;
#endif
  mp->file= 0;					/* Mark destroyed */

#ifdef SAFE_MUTEX_DETECT_DESTROY
  if (mp->info)
  {
    struct st_safe_mutex_info_t *info= mp->info;
    pthread_mutex_lock(&THR_LOCK_mutex);

    if (info->prev)
      info->prev->next = info->next;
    else
      safe_mutex_create_root = info->next;
    if (info->next)
      info->next->prev = info->prev;
    safe_mutex_count--;

    pthread_mutex_unlock(&THR_LOCK_mutex);
    free(info);
    mp->info= NULL;				/* Get crash if double free */
  }
#else
  thread_safe_sub(safe_mutex_count, 1, &THR_LOCK_mutex);
#endif /* SAFE_MUTEX_DETECT_DESTROY */
  DBUG_RETURN(error);
}


/**
  Free all data related to deadlock detection

  This is also useful together with safemalloc when you don't want to
  have reports of not freed memory for mysys mutexes.
*/

void safe_mutex_free_deadlock_data(safe_mutex_t *mp)
{
  /* Free all entries that points to this one */
  if (!(mp->create_flags & MYF_NO_DEADLOCK_DETECTION) && mp->used_mutex != NULL)
  {
    pthread_mutex_lock(&THR_LOCK_mutex);
    my_hash_iterate(mp->used_mutex,
                    (my_hash_walk_action) remove_from_locked_mutex,
                    mp);
    my_hash_iterate(mp->locked_mutex,
                    (my_hash_walk_action) remove_from_used_mutex,
                    mp);
    pthread_mutex_unlock(&THR_LOCK_mutex);

    hash_free(mp->used_mutex);
    hash_free(mp->locked_mutex);
    my_free(mp->locked_mutex, 0);
    mp->create_flags|= MYF_NO_DEADLOCK_DETECTION;
  }
}

/*
  Free global resources and check that all mutex has been destroyed

  SYNOPSIS
    safe_mutex_end()
    file		Print errors on this file

  NOTES
    We can't use DBUG_PRINT() here as we have in my_end() disabled
    DBUG handling before calling this function.

   In MySQL one may get one warning for a mutex created in my_thr_init.c
   This is ok, as this thread may not yet have been exited.
*/

void safe_mutex_end(FILE *file __attribute__((unused)))
{
  if (!safe_mutex_count)			/* safetly */
    pthread_mutex_destroy(&THR_LOCK_mutex);
#ifdef SAFE_MUTEX_DETECT_DESTROY
  if (!file)
    return;

  if (safe_mutex_count)
  {
    fprintf(file, "Warning: Not destroyed mutex: %lu\n", safe_mutex_count);
    (void) fflush(file);
  }
  {
    struct st_safe_mutex_info_t *ptr;
    for (ptr= safe_mutex_create_root ; ptr ; ptr= ptr->next)
    {
      fprintf(file, "\tMutex %s initiated at line %4u in '%s'\n",
	      ptr->name, ptr->init_line, ptr->init_file);
      (void) fflush(file);
    }
  }
#endif /* SAFE_MUTEX_DETECT_DESTROY */
}


static my_bool add_used_to_locked_mutex(safe_mutex_t *used_mutex,
                                        safe_mutex_deadlock_t *locked_mutex)
{
  /* Add mutex to all parent of the current mutex */
  if (!locked_mutex->warning_only)
  {
    (void) my_hash_iterate(locked_mutex->mutex->locked_mutex,
                           (my_hash_walk_action) add_to_locked_mutex,
                           used_mutex);
    /* mark that locked_mutex is locked after used_mutex */
    (void) add_to_locked_mutex(locked_mutex, used_mutex);
  }
  return 0;
}


/**
   register that locked_mutex was locked after current_mutex
*/

static my_bool add_to_locked_mutex(safe_mutex_deadlock_t *locked_mutex,
                                   safe_mutex_t *current_mutex)
{
  DBUG_ENTER("add_to_locked_mutex");
  DBUG_PRINT("info", ("inserting 0x%lx  into  0x%lx  (id: %lu -> %lu)",
                      (ulong) locked_mutex, (long) current_mutex,
                      locked_mutex->id, current_mutex->id));
  if (my_hash_insert(current_mutex->locked_mutex, (uchar*) locked_mutex))
  {
    /* Got mutex through two paths; ignore */
    DBUG_RETURN(0);
  }
  locked_mutex->count++;
  if (my_hash_insert(locked_mutex->mutex->used_mutex,
                     (uchar*) current_mutex))
  {
    DBUG_ASSERT(0);
  }
  DBUG_RETURN(0);
}


/**
  Remove mutex from the locked mutex hash
  @fn    remove_from_used_mutex()
  @param mp            Mutex that has delete_mutex in it's locked_mutex hash
  @param delete_mutex  Mutex should be removed from the hash

  @notes
    safe_mutex_deadlock_t entries in the locked hash are shared.
    When counter goes to 0, we delete the safe_mutex_deadlock_t entry.
*/

static my_bool remove_from_locked_mutex(safe_mutex_t *mp,
                                        safe_mutex_t *delete_mutex)
{
  safe_mutex_deadlock_t *found;
  DBUG_ENTER("remove_from_locked_mutex");
  DBUG_PRINT("enter", ("delete_mutex: 0x%lx  mutex: 0x%lx  (id: %lu <- %lu)",
                       (ulong) delete_mutex, (ulong) mp, 
                       delete_mutex->id, mp->id));

  found= (safe_mutex_deadlock_t *) hash_search(mp->locked_mutex,
                                               (uchar*) &delete_mutex->id, 0);
  DBUG_ASSERT(found);
  if (found)
  {
    if (hash_delete(mp->locked_mutex, (uchar*) found))
    {
      DBUG_ASSERT(0);
    }
    if (!--found->count)
      my_free(found, MYF(0));
  }
  DBUG_RETURN(0);
}

static my_bool remove_from_used_mutex(safe_mutex_deadlock_t *locked_mutex,
                                      safe_mutex_t *mutex)
{
  DBUG_ENTER("remove_from_used_mutex");
  DBUG_PRINT("enter", ("delete_mutex: 0x%lx  mutex: 0x%lx  (id: %lu <- %lu)",
                       (ulong) mutex, (ulong) locked_mutex, 
                       mutex->id, locked_mutex->id));
  if (hash_delete(locked_mutex->mutex->used_mutex, (uchar*) mutex))
  {
    DBUG_ASSERT(0);
  }
  if (!--locked_mutex->count)
    my_free(locked_mutex, MYF(0));
  DBUG_RETURN(0);
}


static void print_deadlock_warning(safe_mutex_t *new_mutex,
                                   safe_mutex_t *parent_mutex)
{
  safe_mutex_t *mutex_root;
  DBUG_ENTER("print_deadlock_warning");
  DBUG_PRINT("enter", ("mutex: %s  parent: %s",
                       new_mutex->name, parent_mutex->name));

  fprintf(stderr, "safe_mutex: Found wrong usage of mutex "
          "'%s' and '%s'\n",
          parent_mutex->name, new_mutex->name);
  DBUG_PRINT("info", ("safe_mutex: Found wrong usage of mutex "
                      "'%s' and '%s'",
                      parent_mutex->name, new_mutex->name));
  fprintf(stderr, "Mutex currently locked (in reverse order):\n");
  DBUG_PRINT("info", ("Mutex currently locked (in reverse order):"));
  fprintf(stderr, "%-32.32s  %s  line %u\n", new_mutex->name, new_mutex->file,
          new_mutex->line);
  DBUG_PRINT("info", ("%-32.32s  %s  line %u\n", new_mutex->name,
                      new_mutex->file, new_mutex->line));
  for (mutex_root= *my_thread_var_mutex_in_use() ;
       mutex_root;
       mutex_root= mutex_root->next)
  {
    fprintf(stderr, "%-32.32s  %s  line %u\n", mutex_root->name,
            mutex_root->file, mutex_root->line);
    DBUG_PRINT("info", ("%-32.32s  %s  line %u", mutex_root->name,
                        mutex_root->file, mutex_root->line));
  }
  fflush(stderr);
  DBUG_VOID_RETURN;
}


#endif /* THREAD && SAFE_MUTEX */

#if defined(THREAD) && defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX)

#include "mysys_priv.h"
#include "my_static.h"
#include <m_string.h>

#include <m_ctype.h>
#include <hash.h>
#include <myisampack.h>
#include <mysys_err.h>
#include <my_sys.h>

#undef pthread_mutex_t
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_trylock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_cond_wait
#undef pthread_cond_timedwait

ulong mutex_delay(ulong delayloops)
{
  ulong	i;
  volatile ulong j;

  j = 0;

  for (i = 0; i < delayloops * 50; i++)
    j += i;

  return(j); 
}	

#define MY_PTHREAD_FASTMUTEX_SPINS 8
#define MY_PTHREAD_FASTMUTEX_DELAY 4

static int cpu_count= 0;

int my_pthread_fastmutex_init(my_pthread_fastmutex_t *mp,
                              const pthread_mutexattr_t *attr)
{
  if ((cpu_count > 1) && (attr == MY_MUTEX_INIT_FAST))
    mp->spins= MY_PTHREAD_FASTMUTEX_SPINS; 
  else
    mp->spins= 0;
  mp->rng_state= 1;
  return pthread_mutex_init(&mp->mutex, attr); 
}

/**
  Park-Miller random number generator. A simple linear congruential
  generator that operates in multiplicative group of integers modulo n.

  x_{k+1} = (x_k g) mod n

  Popular pair of parameters: n = 2^32 − 5 = 4294967291 and g = 279470273.
  The period of the generator is about 2^31.
  Largest value that can be returned: 2147483646 (RAND_MAX)

  Reference:

  S. K. Park and K. W. Miller
  "Random number generators: good ones are hard to find"
  Commun. ACM, October 1988, Volume 31, No 10, pages 1192-1201.
*/

static double park_rng(my_pthread_fastmutex_t *mp)
{
  mp->rng_state= ((my_ulonglong)mp->rng_state * 279470273U) % 4294967291U;
  return (mp->rng_state / 2147483647.0);
}

int my_pthread_fastmutex_lock(my_pthread_fastmutex_t *mp)
{
  int   res;
  uint  i;
  uint  maxdelay= MY_PTHREAD_FASTMUTEX_DELAY;

  for (i= 0; i < mp->spins; i++)
  {
    res= pthread_mutex_trylock(&mp->mutex);

    if (res == 0)
      return 0;

    if (res != EBUSY)
      return res;

    mutex_delay(maxdelay);
    maxdelay += park_rng(mp) * MY_PTHREAD_FASTMUTEX_DELAY + 1;
  }
  return pthread_mutex_lock(&mp->mutex);
}


void fastmutex_global_init(void)
{
#ifdef _SC_NPROCESSORS_CONF
  cpu_count= sysconf(_SC_NPROCESSORS_CONF);
#endif
}
  
#endif /* SAFE_MUTEX_DEFINED */
#endif /* THREAD */
