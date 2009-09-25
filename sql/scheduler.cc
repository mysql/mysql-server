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

/*
  Implementation for the thread scheduler
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma implementation
#endif

#include <mysql_priv.h>
#if MYSQL_VERSION_ID >= 60000
#include "sql_audit.h"
#endif

/*
  'Dummy' functions to be used when we don't need any handling for a scheduler
  event
 */

static bool init_dummy(void) {return 0;}
static void post_kill_dummy(THD *thd) {}
static void end_dummy(void) {}
static bool end_thread_dummy(THD *thd, bool cache_thread) { return 0; }

/*
  Initialize default scheduler with dummy functions so that setup functions
  only need to declare those that are relvant for their usage
*/

scheduler_functions::scheduler_functions()
  :init(init_dummy),
   init_new_connection_thread(init_new_connection_handler_thread),
   add_connection(0),                           // Must be defined
   post_kill_notification(post_kill_dummy),
   end_thread(end_thread_dummy), end(end_dummy)
{}


/*
  End connection, in case when we are using 'no-threads'
*/

static bool no_threads_end(THD *thd, bool put_in_cache)
{
  unlink_thd(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  return 1;                                     // Abort handle_one_connection
}


/*
  Initailize scheduler for --thread-handling=no-threads
*/

void one_thread_scheduler(scheduler_functions *func)
{
  func->max_threads= 1;
  max_connections= 1;
  func->max_connections= &max_connections;
  func->connection_count= &connection_count;
#ifndef EMBEDDED_LIBRARY
  func->add_connection= handle_connection_in_main_thread;
#endif
  func->init_new_connection_thread= init_dummy;
  func->end_thread= no_threads_end;
}


/*
  Initialize scheduler for --thread-handling=one-thread-per-connection
*/

#ifndef EMBEDDED_LIBRARY
void one_thread_per_connection_scheduler(scheduler_functions *func,
                                         ulong *arg_max_connections,
                                         uint *arg_connection_count)
{
  func->max_threads= *arg_max_connections + 1;
  func->max_connections= arg_max_connections;
  func->connection_count= arg_connection_count;
  func->add_connection= create_thread_to_handle_connection;
  func->end_thread= one_thread_per_connection_end;
}
#endif /* EMBEDDED_LIBRARY */


#if defined(HAVE_LIBEVENT) && HAVE_POOL_OF_THREADS == 1

#include "event.h"

static struct event_base *base;

static uint created_threads, killed_threads;
static bool kill_pool_threads;

static struct event thd_add_event;
static struct event thd_kill_event;

static pthread_mutex_t LOCK_thd_add;    /* protects thds_need_adding */
static LIST *thds_need_adding;    /* list of thds to add to libevent queue */

static int thd_add_pair[2]; /* pipe to signal add a connection to libevent*/
static int thd_kill_pair[2]; /* pipe to signal kill a connection in libevent */

/*
  LOCK_event_loop protects the non-thread safe libevent calls (event_add and 
  event_del) and thds_need_processing and thds_waiting_for_io.
*/
static pthread_mutex_t LOCK_event_loop;
static LIST *thds_need_processing; /* list of thds that needs some processing */
static LIST *thds_waiting_for_io; /* list of thds with added events */

pthread_handler_t libevent_thread_proc(void *arg);
static void libevent_end();
static bool libevent_needs_immediate_processing(THD *thd);
static void libevent_connection_close(THD *thd);
static bool libevent_should_close_connection(THD* thd);
static void libevent_thd_add(THD* thd);
void libevent_io_callback(int Fd, short Operation, void *ctx);
void libevent_add_thd_callback(int Fd, short Operation, void *ctx);
void libevent_kill_thd_callback(int Fd, short Operation, void *ctx);


/*
  Create a pipe and set to non-blocking.
  Returns TRUE if there is an error.
*/

static bool init_socketpair(int sock_pair[])
{
  sock_pair[0]= sock_pair[1]= -1;
  return (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, sock_pair) < 0 ||
          evutil_make_socket_nonblocking(sock_pair[0]) == -1 ||
          evutil_make_socket_nonblocking(sock_pair[1]) == -1);
}

static void close_socketpair(int sock_pair[])
{
  if (sock_pair[0] != -1)
    EVUTIL_CLOSESOCKET(sock_pair[0]);
  if (sock_pair[1] != -1)
    EVUTIL_CLOSESOCKET(sock_pair[1]);
}

/*
  thd_scheduler keeps the link between THD and events.
  It's embedded in the THD class.
*/

thd_scheduler::thd_scheduler()
  : logged_in(FALSE), io_event(NULL), thread_attached(FALSE)
{
#ifndef DBUG_OFF
  dbug_explain[0]= '\0';
  set_explain= FALSE;
#endif
}


thd_scheduler::~thd_scheduler()
{
  my_free(io_event, MYF(MY_ALLOW_ZERO_PTR));
}


bool thd_scheduler::init(THD *parent_thd)
{
  io_event=
    (struct event*)my_malloc(sizeof(*io_event),MYF(MY_ZEROFILL|MY_WME));

  if (!io_event)
  {
    sql_print_error("Memory allocation error in thd_scheduler::init\n");
    return TRUE;
  }

  event_set(io_event, (int)parent_thd->net.vio->sd, EV_READ,
            libevent_io_callback, (void*)parent_thd);

  list.data= parent_thd;

  return FALSE;
}


/*
  Attach/associate the connection with the OS thread, for command processing.
*/

bool thd_scheduler::thread_attach()
{
  DBUG_ASSERT(!thread_attached);
  THD* thd = (THD*)list.data;
  if (libevent_should_close_connection(thd) ||
      setup_connection_thread_globals(thd))
  {
    return TRUE;
  }
  my_errno= 0;
  thd->mysys_var->abort= 0;
  thread_attached= TRUE;
#ifndef DBUG_OFF
  /*
    When we attach the thread for a connection for the first time,
    we know that there is no session value set yet. Thus
    the initial setting of set_explain to FALSE is OK.
  */
  if (set_explain)
    DBUG_SET(dbug_explain);
#endif
  return FALSE;
}


/*
  Detach/disassociate the connection with the OS thread.
*/

void thd_scheduler::thread_detach()
{
  if (thread_attached)
  {
    THD* thd = (THD*)list.data;
    pthread_mutex_lock(&thd->LOCK_thd_data);
    thd->mysys_var= NULL;
    pthread_mutex_unlock(&thd->LOCK_thd_data);
    thread_attached= FALSE;
#ifndef DBUG_OFF
    /*
      If during the session @@session.dbug was assigned, the
      dbug options/state has been pushed. Check if this is the
      case, to be able to restore the state when we attach this
      logical connection to a physical thread.
    */
    if (_db_is_pushed_())
    {
      set_explain= TRUE;
      if (DBUG_EXPLAIN(dbug_explain, sizeof(dbug_explain)))
        sql_print_error("thd_scheduler: DBUG_EXPLAIN buffer is too small");
    }
    /* DBUG_POP() is a no-op in case there is no session state */
    DBUG_POP();
#endif
  }
}

/**
  Create all threads for the thread pool

  NOTES
    After threads are created we wait until all threads has signaled that
    they have started before we return

  RETURN
    0  ok
    1  We got an error creating the thread pool
       In this case we will abort all created threads
*/

static bool libevent_init(void)
{
  uint i;
  DBUG_ENTER("libevent_init");

  base= (struct event_base *) event_init();

  created_threads= 0;
  killed_threads= 0;
  kill_pool_threads= FALSE;

  pthread_mutex_init(&LOCK_event_loop, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_thd_add, MY_MUTEX_INIT_FAST);

  /* set up sockets used to add new thds to the event pool */
  if (init_socketpair(thd_add_pair))
  {
    sql_print_error("init_socketpair(thd_add_spair) error in libevent_init");
    DBUG_RETURN(1);
  }
  /* set up sockets used to kill thds in the event queue */
  if (init_socketpair(thd_kill_pair))
  {
    sql_print_error("init_socketpair(thd_kill_pair) error in libevent_init");
    close_socketpair(thd_add_pair);
    DBUG_RETURN(1);
  }
  event_set(&thd_add_event, thd_add_pair[0], EV_READ|EV_PERSIST,
            libevent_add_thd_callback, NULL);
  event_set(&thd_kill_event, thd_kill_pair[0], EV_READ|EV_PERSIST,
            libevent_kill_thd_callback, NULL);

  if (event_add(&thd_add_event, NULL) || event_add(&thd_kill_event, NULL))
  {
    sql_print_error("thd_add_event event_add error in libevent_init");
    libevent_end();
    DBUG_RETURN(1);
  }
  /* Set up the thread pool */
  created_threads= killed_threads= 0;
  pthread_mutex_lock(&LOCK_thread_count);

  for (i= 0; i < thread_pool_size; i++)
  {
    pthread_t thread;
    int error;
    if ((error= pthread_create(&thread, &connection_attrib,
                               libevent_thread_proc, 0)))
    {
      sql_print_error("Can't create completion port thread (error %d)",
                      error);
      pthread_mutex_unlock(&LOCK_thread_count);
      libevent_end();                      // Cleanup
      DBUG_RETURN(TRUE);
    }
  }

  /* Wait until all threads are created */
  while (created_threads != thread_pool_size)
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);

  DBUG_PRINT("info", ("%u threads created", (uint) thread_pool_size));
  DBUG_RETURN(FALSE);
}


/*
  This is called when data is ready on the socket.

  NOTES
    This is only called by the thread that owns LOCK_event_loop.

    We add the thd that got the data to thds_need_processing, and
    cause the libevent event_loop() to terminate. Then this same thread will
    return from event_loop and pick the thd value back up for processing.
*/

void libevent_io_callback(int, short, void *ctx)
{
  safe_mutex_assert_owner(&LOCK_event_loop);
  THD *thd= (THD*)ctx;
  thds_waiting_for_io= list_delete(thds_waiting_for_io, &thd->event_scheduler.list);
  thds_need_processing= list_add(thds_need_processing, &thd->event_scheduler.list);
}

/*
  This is called when we have a thread we want to be killed.

  NOTES
    This is only called by the thread that owns LOCK_event_loop.
*/

void libevent_kill_thd_callback(int Fd, short, void*)
{
  safe_mutex_assert_owner(&LOCK_event_loop);

  /* clear the pending events */
  char c;
  while (recv(Fd, &c, sizeof(c), 0) == sizeof(c))
  {}

  LIST* list= thds_waiting_for_io;
  while (list)
  {
    THD *thd= (THD*)list->data;
    list= list_rest(list);
    if (thd->killed == THD::KILL_CONNECTION)
    {
      /*
        Delete from libevent and add to the processing queue.
      */
      event_del(thd->event_scheduler.io_event);
      thds_waiting_for_io= list_delete(thds_waiting_for_io,
                                       &thd->event_scheduler.list);
      thds_need_processing= list_add(thds_need_processing,
                                     &thd->event_scheduler.list);
    }
  }
}


/*
  This is used to add connections to the pool. This callback is invoked from
  the libevent event_loop() call whenever the thd_add_pair[1]  has a byte
  written to it.

  NOTES
    This is only called by the thread that owns LOCK_event_loop.
*/

void libevent_add_thd_callback(int Fd, short, void *)
{
  safe_mutex_assert_owner(&LOCK_event_loop);

  /* clear the pending events */
  char c;
  while (recv(Fd, &c, sizeof(c), 0) == sizeof(c))
  {}

  pthread_mutex_lock(&LOCK_thd_add);
  while (thds_need_adding)
  {
    /* pop the first thd off the list */
    THD* thd= (THD*)thds_need_adding->data;
    thds_need_adding= list_delete(thds_need_adding, thds_need_adding);

    pthread_mutex_unlock(&LOCK_thd_add);

    if (!thd->event_scheduler.logged_in || libevent_should_close_connection(thd))
    {
      /*
        Add thd to thds_need_processing list. If it needs closing we'll close
        it outside of event_loop().
      */
      thds_need_processing= list_add(thds_need_processing,
                                     &thd->event_scheduler.list);
    }
    else
    {
      /* Add to libevent */
      if (event_add(thd->event_scheduler.io_event, NULL))
      {
        sql_print_error("event_add error in libevent_add_thd_callback");
        libevent_connection_close(thd);
      }
      else
      {
        thds_waiting_for_io= list_add(thds_waiting_for_io,
                                      &thd->event_scheduler.list);
      }
    }
    pthread_mutex_lock(&LOCK_thd_add);
  }
  pthread_mutex_unlock(&LOCK_thd_add);
}


/**
  Notify the thread pool about a new connection

  NOTES
    LOCK_thread_count is locked on entry. This function MUST unlock it!
*/

static void libevent_add_connection(THD *thd)
{
  DBUG_ENTER("libevent_add_connection");
  DBUG_PRINT("enter", ("thd: %p  thread_id: %lu",
                       thd, thd->thread_id));

  if (thd->event_scheduler.init(thd))
  {
    sql_print_error("Scheduler init error in libevent_add_new_connection");
    pthread_mutex_unlock(&LOCK_thread_count);
    libevent_connection_close(thd);
    DBUG_VOID_RETURN;
  }
  threads.append(thd);
  libevent_thd_add(thd);

  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}


/**
  @brief Signal a waiting connection it's time to die.

  @details This function will signal libevent the THD should be killed.
    Either the global LOCK_thd_count or the THD's LOCK_thd_data must be locked
    upon entry.

  @param[in]  thd The connection to kill
*/

static void libevent_post_kill_notification(THD *)
{
  /*
    Note, we just wake up libevent with an event that a THD should be killed,
    It will search its list of thds for thd->killed ==  KILL_CONNECTION to
    find the THDs it should kill.

    So we don't actually tell it which one and we don't actually use the
    THD being passed to us, but that's just a design detail that could change
    later.
  */
  char c= 0;
  send(thd_kill_pair[1], &c, sizeof(c), 0);
}


/*
  Close and delete a connection.
*/

static void libevent_connection_close(THD *thd)
{
  DBUG_ENTER("libevent_connection_close");
  DBUG_PRINT("enter", ("thd: %p", thd));

  thd->killed= THD::KILL_CONNECTION;          // Avoid error messages

  if (thd->net.vio->sd >= 0)                  // not already closed
  {
    end_connection(thd);
    close_connection(thd, 0, 1);
  }
  thd->event_scheduler.thread_detach();
  unlink_thd(thd);   /* locks LOCK_thread_count and deletes thd */
  pthread_mutex_unlock(&LOCK_thread_count);

  DBUG_VOID_RETURN;
}


/*
  Returns true if we should close and delete a THD connection.
*/

static bool libevent_should_close_connection(THD* thd)
{
  return thd->net.error ||
         thd->net.vio == 0 ||
         thd->killed == THD::KILL_CONNECTION;
}


/*
  libevent_thread_proc is the outer loop of each thread in the thread pool.
  These procs only return/terminate on shutdown (kill_pool_threads == true).
*/

pthread_handler_t libevent_thread_proc(void *arg)
{
  if (init_new_connection_handler_thread())
  {
    my_thread_global_end();
    sql_print_error("libevent_thread_proc: my_thread_init() failed");
    exit(1);
  }
  DBUG_ENTER("libevent_thread_proc");

  /*
    Signal libevent_init() when all threads has been created and are ready to
    receive events.
  */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  created_threads++;
  thread_created++;
  if (created_threads == thread_pool_size)
    (void) pthread_cond_signal(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  for (;;)
  {
    THD *thd= NULL;
    (void) pthread_mutex_lock(&LOCK_event_loop);

    /* get thd(s) to process */
    while (!thds_need_processing)
    {
      if (kill_pool_threads)
      {
        /* the flag that we should die has been set */
        (void) pthread_mutex_unlock(&LOCK_event_loop);
        goto thread_exit;
      }
      event_loop(EVLOOP_ONCE);
    }

    /* pop the first thd off the list */
    thd= (THD*)thds_need_processing->data;
    thds_need_processing= list_delete(thds_need_processing,
                                      thds_need_processing);

    (void) pthread_mutex_unlock(&LOCK_event_loop);

    /* now we process the connection (thd) */

    /* set up the thd<->thread links. */
    thd->thread_stack= (char*) &thd;

    if (thd->event_scheduler.thread_attach())
    {
      libevent_connection_close(thd);
      continue;
    }

    /* is the connection logged in yet? */
    if (!thd->event_scheduler.logged_in)
    {
      DBUG_PRINT("info", ("init new connection.  sd: %d",
                          thd->net.vio->sd));
      lex_start(thd);
      if (login_connection(thd))
      {
        /* Failed to log in */
        libevent_connection_close(thd);
        continue;
      }
      else
      {
        /* login successful */
#if MYSQL_VERSION_ID >= 60000
        MYSQL_CONNECTION_START(thd->thread_id, thd->security_ctx->priv_user,
                               (char *) thd->security_ctx->host_or_ip);
#endif
        thd->event_scheduler.logged_in= TRUE;
        prepare_new_connection_state(thd);
        if (!libevent_needs_immediate_processing(thd))
          continue; /* New connection is now waiting for data in libevent*/
      }
    }

    do
    {
      /* Process a query */
      if (do_command(thd))
      {
        libevent_connection_close(thd);
        break;
      }
    } while (libevent_needs_immediate_processing(thd));
  }

thread_exit:
  DBUG_PRINT("exit", ("ending thread"));
  (void) pthread_mutex_lock(&LOCK_thread_count);
  killed_threads++;
  pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);                               /* purify: deadcode */
}


/*
  Returns TRUE if the connection needs immediate processing and FALSE if
  instead it's queued for libevent processing or closed,
*/

static bool libevent_needs_immediate_processing(THD *thd)
{
  if (libevent_should_close_connection(thd))
  {
    libevent_connection_close(thd);
    return FALSE;
  }
  /*
    If more data in the socket buffer, return TRUE to process another command.

    Note: we cannot add for event processing because the whole request might
    already be buffered and we wouldn't receive an event.
  */
  if (vio_pending(thd->net.vio) > 0)
    return TRUE;

  thd->event_scheduler.thread_detach();
  libevent_thd_add(thd);
  return FALSE;
}


/*
  Adds a THD to queued for libevent processing.

  This call does not actually register the event with libevent.
  Instead, it places the THD onto a queue and signals libevent by writing
  a byte into thd_add_pair, which will cause our libevent_add_thd_callback to
  be invoked which will find the THD on the queue and add it to libevent.
*/

static void libevent_thd_add(THD* thd)
{
  char c= 0;
  /* release any audit resources, this thd is going to sleep */
#if MYSQL_VERSION_ID >= 60000
  mysql_audit_release(thd);
#endif
  pthread_mutex_lock(&LOCK_thd_add);
  /* queue for libevent */
  thds_need_adding= list_add(thds_need_adding, &thd->event_scheduler.list);
  /* notify libevent */
  send(thd_add_pair[1], &c, sizeof(c), 0);
  pthread_mutex_unlock(&LOCK_thd_add);
}


/**
  Wait until all pool threads have been deleted for clean shutdown
*/

static void libevent_end()
{
  DBUG_ENTER("libevent_end");
  DBUG_PRINT("enter", ("created_threads: %d  killed_threads: %u",
                       created_threads, killed_threads));

  /*
    check if initialized. This may not be the case if get an error at
    startup
  */
  if (!base)
    DBUG_VOID_RETURN;

  (void) pthread_mutex_lock(&LOCK_thread_count);


  kill_pool_threads= TRUE;
  while (killed_threads != created_threads)
  {
    /* wake up the event loop */
    char c= 0;
    send(thd_add_pair[1], &c, sizeof(c), 0);
    pthread_cond_wait(&COND_thread_count, &LOCK_thread_count);
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  event_del(&thd_add_event);
  close_socketpair(thd_add_pair);
  event_del(&thd_kill_event);
  close_socketpair(thd_kill_pair);
  event_base_free(base);
  base= 0;

  (void) pthread_mutex_destroy(&LOCK_event_loop);
  (void) pthread_mutex_destroy(&LOCK_thd_add);
  DBUG_VOID_RETURN;
}


void pool_of_threads_scheduler(scheduler_functions* func)
{
  func->max_threads= thread_pool_size;
  func->max_connections=  &max_connections;
  func->connection_count= &connection_count;
  func->init= libevent_init;
  func->end=  libevent_end;
  func->post_kill_notification= libevent_post_kill_notification;
  func->add_connection= libevent_add_connection;
}

#endif
