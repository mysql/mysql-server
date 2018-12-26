/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_NOTIFICATION_INCLUDED
#define GCS_XCOM_NOTIFICATION_INCLUDED

#include <queue>
#include "xplatform/my_xp_thread.h"
#include "xplatform/my_xp_mutex.h"
#include "xplatform/my_xp_cond.h"
#include "gcs_control_interface.h"
#include "gcs_xcom_utils.h"
#include "xcom_vp.h"

/**
  Abstract class that defines a notification that will be sent from XCOM
  to MySQL GCS or from an user thread to MySQL GCS.

  This is a very simple implementation that chooses simplicity over flexibilty.
  For example, it does not support notifications on member functions (i.e.
  methods) and a new notification generates some duplicated code. Note that
  these limitations could be eliminated with the use of generalized functors.
  However, this solution would have increased code complexity as we cannot
  use C++ 11.

  Note also that we have used the term functor here to designed the pointer
  to the callback function.

  We will revisit this decision in the future if any of the choices become a
  problem and when we start using C++ 11.
*/
class Gcs_xcom_notification
{
public:
  /**
    Constructor for Gcs_xcom_notification which an abstract class
    that represents notifications sent from XCOM to MySQL GCS.

    Such notifications are read from a queue by a MySQL GCS' thread,
    specifically by the Gcs_xcom_engine which is responsible for
    executing them.

    The main loop in the GCS_xcom_engine is stopped when the
    execution returns true.
  */

  explicit Gcs_xcom_notification() { }


  /**
    Task implemented by this notification which calls do_execute.
  */

  virtual bool operator()()= 0;


  /**
    Destructor for the Gcs_xcom_notification.
  */

  virtual ~Gcs_xcom_notification() { }

private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_notification(Gcs_xcom_notification const&);
  Gcs_xcom_notification& operator=(Gcs_xcom_notification const&);
};

typedef void (xcom_initialize_functor)();
typedef void (xcom_finalize_functor)();
class Gcs_xcom_engine
{
public:
  /**
    Constructor for Gcs_xcom_engine.
  */

  explicit Gcs_xcom_engine();


  /**
    Destructor for Gcs_xcom_engine.
  */

  ~Gcs_xcom_engine();


  /**
    Start the notification processing by spwaning a thread that will be
    responsible for reading all incoming notifications.
  */

  void initialize(xcom_initialize_functor *functor);


  /**
    Finalize the notification processing by stopping the thread that is
    responsible for reading all incoming notifications. Optionally, a
    callback function can be scheduled in order to do some clean up.

    When the finalize has been executed the engine will not accept any
    new incoming notification, the processing thread will be stopped
    and the optional callback will be the last one called if there is
    any.

    @param functor: Last callback function to be executed.
  */

  void finalize(xcom_finalize_functor *functor);


  /**
    Process notifications from the incoming queue until an empty
    notifications comes in.
  */

  void process();


  /**
    Clean up the notification queue and also forbid any incoming
    notitification to be added to the queue.
  */

  void cleanup();


  /**
    Push a notification to the queue.


    @param notification: Pointer to notification to be queued.

    @return If the request was successfully enqueued true is
            returned, otherwise, false is returned.
  */

  bool push(Gcs_xcom_notification *notification);

private:
  /*
    Condition variable used to inform when there are new
    notifications in the queue.
  */
  My_xp_cond_impl m_wait_for_notification_cond;

  /*
    Mutex used to prevent concurrent access to the queue.
  */
  My_xp_mutex_impl m_wait_for_notification_mutex;

  /*
    Queue that holds notifications from XCOM.
  */
  std::queue<Gcs_xcom_notification *> m_notification_queue;

  /*
    Thread responsible for reading the queue and process
    the notifications enqueued by XCOM.
  */
  My_xp_thread_impl m_engine_thread;

  /*
    Whether the engine is accepting new notifications or not.
  */
  bool m_schedule;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_engine(Gcs_xcom_engine const&);
  Gcs_xcom_engine& operator=(Gcs_xcom_engine const&);
};


/**
  Template that defines whether a notification shall make the
  engine stop or not.

  class X_notification : public Paremetrized_notification<true>
  {
  public:
    void do_execute()
    {
      // Do something.
    }
  }
*/
template <bool stop>
class Parameterized_notification : public Gcs_xcom_notification
{
public:
  /**
    Constructor for Parameterized_notification.
  */

  explicit Parameterized_notification() { }


  /**
    Destructor for Parameterized_notification.
  */

  virtual ~Parameterized_notification() { }


  /**
    Task implemented by this notification which calls do_execute.

    Return whether the notification should stop the engine or not.
  */

  bool operator()()
  {
    do_execute();

    return stop;
  }

private:
  /**
    Method that must be implemented buy the different types of
    notifications.
  */

  virtual void do_execute()=0;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Parameterized_notification(Parameterized_notification const&);
  Parameterized_notification& operator=(Parameterized_notification const&);
};


/**
   Notification used to stop the Gcs_xcom_engine.
*/
class Finalize_notification : public Parameterized_notification<true>
{
public:
  /**
    Constructor for Finalize_notification.

    @param gcs_engine: Reference to the engine.
    @param functor: Pointer to a function that contains that actual
                    core of the execution.
  */

  explicit Finalize_notification(
    Gcs_xcom_engine *gcs_engine, xcom_finalize_functor *functor
  );


  /**
    Destructor for Finalize_notification.
  */

  ~Finalize_notification();

private:
  /**
    Task implemented by this notification.
  */

  void do_execute();

  /**
    Pointer to the MySQL GCS Engine.
  */
  Gcs_xcom_engine *m_gcs_engine;

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  xcom_finalize_functor *m_functor;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Finalize_notification(Finalize_notification const&);
  Finalize_notification& operator=(Finalize_notification const&);
};


class Initialize_notification : public Parameterized_notification<false>
{
public:
  /**
    Constructor for Initialize_notification.

    @param gcs_engine: Reference to the engine.
    @param functor: Pointer to a function that contains that actual
                    core of the execution.
  */

  explicit Initialize_notification(
    xcom_initialize_functor *functor
  );


  /**
    Destructor for Initialize_notification.
  */

  ~Initialize_notification();

private:
  /**
    Task implemented by this notification.
  */

  void do_execute();

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  xcom_initialize_functor *m_functor;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Initialize_notification(Initialize_notification const&);
  Initialize_notification& operator=(Initialize_notification const&);
};



typedef void (xcom_receive_data_functor)(synode_no, Gcs_xcom_nodes *, u_int, char *);
/**
  Notification used to inform that data has been totally ordered.
*/
class Data_notification : public Parameterized_notification<false>
{
public:
  /**
    Constructor for Data_notification.

    @param functor: Pointer to a function that contains that actual
                    core of the execution.
    @param message_id: Messaged Id.
    @param nodes: Set of nodes that participated in the consensus
                  to deliver the message.
    @param size: Size of the message's content.
    @param data: This is the message's content.
  */

  explicit Data_notification(xcom_receive_data_functor *functor,
                             synode_no message_id,
                             Gcs_xcom_nodes *xcom_nodes,
                             u_int size, char *data);


   /**
     Destructor for Data_notification
   */

  ~Data_notification();

private:
  /**
    Task implemented by this notification which calls the functor
    with the parameters provided in the contructor.
  */

  void do_execute();

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  xcom_receive_data_functor *m_functor;

  /*
    Messaged Id.
  */
  synode_no m_message_id;

  /*
    Set of nodes that participated in the consensus to deliver the
    message.
  */
  Gcs_xcom_nodes *m_xcom_nodes;

  /*
    Size of the message's content.
  */
  u_int m_size;

  /*
    This is the message's content.
  */
  char *m_data;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Data_notification(Data_notification const&);
  Data_notification& operator=(Data_notification const&);
};


typedef void (xcom_status_functor)(int);
/**
  Notification used to inform that has been a change in XCOM's state
  machine such as it has started up or shut down.
*/
class Status_notification : public Parameterized_notification<false>
{
public:
  /**
    Constructor for Status_notification.

    @param functor: Pointer to a function that contains that actual
                    core of the execution.
    @param status: XCOM's status.
  */

  explicit Status_notification(xcom_status_functor *functor, int status);


  /**
    Destructor for Status_notification.
  */

  ~Status_notification();

private:
  /**
    Task implemented by this notification.
  */

  void do_execute();

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  xcom_status_functor* m_functor;

  /*
    XCOM's status.
  */
  int m_status;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Status_notification(Status_notification const&);
  Status_notification& operator=(Status_notification const&);
};


typedef void (xcom_global_view_functor)(synode_no, synode_no, Gcs_xcom_nodes *);
/**
  Notification used to inform there have been change to the configuration,
  i.e. nodes have been added, removed or considered dead/faulty.
*/
class Global_view_notification : public Parameterized_notification<false>
{
public:
  /**
    Constructor for Global_view_notification.

    @param functor: Pointer to a function that contains that actual
                    core of the execution.
    @param config_id: Message Id when the configuration, i.e. nodes,
                      was installed.
    @param message_id: Messaged Id.
    @param xcom_nodes: Set of nodes that participated in the consensus
                       to deliver the message.
  */

  explicit Global_view_notification(xcom_global_view_functor* functor,
                               synode_no config_id,
                               synode_no message_id,
                               Gcs_xcom_nodes *xcom_nodes);


  /**
    Destructor for Global_view_notification.
  */

  ~Global_view_notification();

private:
  /**
    Task implemented by this notification.
  */

  void do_execute();

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  xcom_global_view_functor *m_functor;

  /*
    Message Id when the configuration, i.e. nodes, was installed.
  */
  synode_no m_config_id;

  /*
    Messaged Id.
  */

  synode_no m_message_id;

  /*
    Set of nodes that participated in the consensus to deliver
    the message.
  */
  Gcs_xcom_nodes *m_xcom_nodes;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Global_view_notification(Global_view_notification const&);
  Global_view_notification& operator=(Global_view_notification const&);
};


typedef void (xcom_local_view_functor)(synode_no, Gcs_xcom_nodes *);
/**
  Notification used to provide hints on nodes' availability.
*/
class Local_view_notification : public Parameterized_notification<false>
{
public:
  /**
    Constructor for Local_view_notification.

    @param functor: Pointer to a function that contains that actual
                    core of the execution.
    @param message_id: Messaged Id.
    @param xcom_nodes: Set of nodes that were defined when the notification
                       happened.
  */

  explicit Local_view_notification(xcom_local_view_functor* functor,
                                   synode_no message_id,
                                   Gcs_xcom_nodes *xcom_nodes);


  /**
    Destructor for Local_view_notification.
  */
  ~Local_view_notification();

private:
  /**
    Task implemented by this notification.
  */

  void do_execute();

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  xcom_local_view_functor *m_functor;

  /*
    Message Id.
  */
  synode_no m_message_id;

  /*
    Set of nodes that were defined when the notification happened.
  */
  Gcs_xcom_nodes *m_xcom_nodes;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Local_view_notification(Local_view_notification const&);
  Local_view_notification& operator=(Local_view_notification const&);
};


typedef void (xcom_control_functor)(Gcs_control_interface *);
/**
  Notification used to make a node join or leave the cluster, provided
  the system was already initialized.
*/
class Control_notification : public Parameterized_notification<false>
{
public:
  /**
    Constructor for Control_notification.

    @param functor: Pointer to a function that contains that actual
                    core of the execution.
    @param control_if: Reference to Control Interface.
  */

  explicit Control_notification(xcom_control_functor *functor,
                             Gcs_control_interface *control_if);


  /**
    Destructor for Control_notification.
  */
  ~Control_notification();

private:
  /**
    Task implemented by this notification.
  */

  void do_execute();

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  xcom_control_functor *m_functor;

  /*
    Pointer to a function that contains that actual core of the
    execution.
  */
  Gcs_control_interface *m_control_if;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Control_notification(Control_notification const&);
  Control_notification& operator=(Control_notification const&);
};

typedef void(xcom_expel_functor)(void);

/**
  Notification used to inform that the node has been expelled or is about
  to be.
*/
class Expel_notification : public Parameterized_notification<false> {
 public:
  /**
    Constructor for Expel_notification.
    @param functor Pointer to a function that contains that actual core of
    the execution.
  */
  explicit Expel_notification(xcom_expel_functor *functor);

  /**
    Destructor for Expel_notification.
  */
  ~Expel_notification();

 private:
  /**
    Task implemented by this notification.
  */
  void do_execute();

  /*
    Pointer to a function that contains that actual core of the execution.
  */
  xcom_expel_functor *m_functor;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Expel_notification(Expel_notification const &);
  Expel_notification &operator=(Expel_notification const &);

};

#endif // GCS_XCOM_NOTIFICATION_INCLUDED
