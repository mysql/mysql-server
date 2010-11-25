
#include <wqueue.h>

#define STRUCT_PTR(TYPE, MEMBER, a)                                           \
          (TYPE *) ((char *) (a) - offsetof(TYPE, MEMBER))
/*
  Link a thread into double-linked queue of waiting threads.

  SYNOPSIS
    wqueue_link_into_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be added to the queue

  RETURN VALUE
    none

  NOTES.
    Queue is represented by a circular list of the thread structures
    The list is double-linked of the type (**prev,*next), accessed by
    a pointer to the last element.
*/

void wqueue_link_into_queue(WQUEUE *wqueue, struct st_my_thread_var *thread)
{
  struct st_my_thread_var *last;
  if (!(last= wqueue->last_thread))
  {
    /* Queue is empty */
    thread->next= thread;
    thread->prev= &thread->next;
  }
  else
  {
    thread->prev= last->next->prev;
    last->next->prev= &thread->next;
    thread->next= last->next;
    last->next= thread;
  }
  wqueue->last_thread= thread;
}


/*
  Add a thread to single-linked queue of waiting threads

  SYNOPSIS
    wqueue_add_to_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be added to the queue

  RETURN VALUE
    none

  NOTES.
    Queue is represented by a circular list of the thread structures
    The list is single-linked of the type (*next), accessed by a pointer
    to the last element.
*/

void wqueue_add_to_queue(WQUEUE *wqueue, struct st_my_thread_var *thread)
{
  struct st_my_thread_var *last;
  if (!(last= wqueue->last_thread))
    thread->next= thread;
  else
  {
    thread->next= last->next;
    last->next= thread;
  }
#ifndef DBUG_OFF
  thread->prev= NULL; /* force segfault if used */
#endif
  wqueue->last_thread= thread;
}

/*
  Unlink a thread from double-linked queue of waiting threads

  SYNOPSIS
    wqueue_unlink_from_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be removed from the queue

  RETURN VALUE
    none

  NOTES.
    See NOTES for link_into_queue
*/

void wqueue_unlink_from_queue(WQUEUE *wqueue, struct st_my_thread_var *thread)
{
  if (thread->next == thread)
    /* The queue contains only one member */
    wqueue->last_thread= NULL;
  else
  {
    thread->next->prev= thread->prev;
    *thread->prev= thread->next;
    if (wqueue->last_thread == thread)
      wqueue->last_thread= STRUCT_PTR(struct st_my_thread_var, next,
                                      thread->prev);
  }
  thread->next= NULL;
}


/*
  Remove all threads from queue signaling them to proceed

  SYNOPSIS
    wqueue_realease_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be added to the queue

  RETURN VALUE
    none

  NOTES.
    See notes for add_to_queue
    When removed from the queue each thread is signaled via condition
    variable thread->suspend.
*/

void wqueue_release_queue(WQUEUE *wqueue)
{
  struct st_my_thread_var *last= wqueue->last_thread;
  struct st_my_thread_var *next= last->next;
  struct st_my_thread_var *thread;
  do
  {
    thread= next;
    pthread_cond_signal(&thread->suspend);
    next= thread->next;
    thread->next= NULL;
  }
  while (thread != last);
  wqueue->last_thread= NULL;
}


/**
  @brief Removes all threads waiting for read or first one waiting for write.

  @param wqueue          pointer to the queue structure
  @param thread          pointer to the thread to be added to the queue

  @note This function is applicable only to single linked lists.
*/

void wqueue_release_one_locktype_from_queue(WQUEUE *wqueue)
{
  struct st_my_thread_var *last= wqueue->last_thread;
  struct st_my_thread_var *next= last->next;
  struct st_my_thread_var *thread;
  struct st_my_thread_var *new_list= NULL;
  uint first_type= next->lock_type;
  if (first_type == MY_PTHREAD_LOCK_WRITE)
  {
    /* release first waiting for write lock */
    pthread_cond_signal(&next->suspend);
    if (next == last)
      wqueue->last_thread= NULL;
    else
      last->next= next->next;
    next->next= NULL;
    return;
  }
  do
  {
    thread= next;
    next= thread->next;
    if (thread->lock_type == MY_PTHREAD_LOCK_WRITE)
    {
      /* skip waiting for write lock */
      if (new_list)
      {
        thread->next= new_list->next;
        new_list= new_list->next= thread;
      }
      else
        new_list= thread->next= thread;
    }
    else
    {
      /* release waiting for read lock */
      pthread_cond_signal(&thread->suspend);
      thread->next= NULL;
    }
  } while (thread != last);
  wqueue->last_thread= new_list;
}


/*
  Add thread and wait

  SYNOPSYS
    wqueue_add_and_wait()
    wqueue               queue to add to
    thread               thread which is waiting
    lock                 mutex need for the operation
*/

void wqueue_add_and_wait(WQUEUE *wqueue,
                         struct st_my_thread_var *thread,
                         pthread_mutex_t *lock)
{
  DBUG_ENTER("wqueue_add_and_wait");
  DBUG_PRINT("enter",
             ("thread: 0x%lx  cond: 0x%lx  mutex: 0x%lx",
              (ulong) thread, (ulong) &thread->suspend, (ulong) lock));
  wqueue_add_to_queue(wqueue, thread);
  do
  {
    DBUG_PRINT("info", ("wait... cond:  0x%lx  mutex:  0x%lx",
                        (ulong) &thread->suspend, (ulong) lock));
    pthread_cond_wait(&thread->suspend, lock);
    DBUG_PRINT("info", ("wait done cond: 0x%lx  mutex: 0x%lx   next: 0x%lx",
                        (ulong) &thread->suspend, (ulong) lock,
                        (ulong) thread->next));
  }
  while (thread->next);
  DBUG_VOID_RETURN;
}
