/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include <NdbApi.hpp>

#include "ndb_util/NdbWaitGroup.hpp"
#include "SharedList.h"

/* Limits the number of async operations that can be pending --
   i.e. that can have been sent with executeAsynch() but not yet polled.
*/
#define MAX_CONCURRENCY 1024

#ifdef FORCE_UV_LEGACY_COMPAT
#define PTHREAD_RETURN_TYPE void *
#define PTHREAD_RETURN_VAL NULL
#else
#define PTHREAD_RETURN_TYPE void
#define PTHREAD_RETURN_VAL
#endif

extern "C" {
  void ioCompleted(uv_async_t *, int);
  void ndbTxCompleted(int, NdbTransaction *, void *);
  PTHREAD_RETURN_TYPE run_ndb_listener_thread(void *);
}


class AsyncNdbContext {
public:
  /* Constructor */
  AsyncNdbContext(Ndb_cluster_connection *);

  /* Destructor */
  ~AsyncNdbContext();
  
  /* Methods */
  int executeAsynch(NdbTransaction *,
                    int execType, int abortOption, int forceSend,
                    v8::Persistent<v8::Function> execCompleteCallback);

  void shutdown();

  /* Friend functions have C linkage but call the protected methods */
  friend PTHREAD_RETURN_TYPE ::run_ndb_listener_thread(void *);
  friend void ::ioCompleted(uv_async_t *, int);
  
protected:
  void * runListenerThread();
  void completeCallbacks();

private:
  /* A uv_async_t is a UV object that can signal the main event loop upon
     completion of asynchronous tasks.
     In this design, the UV worker threads send transactions to NDB using 
     executeAsynch(), then adds the Ndb object to a queue of active Ndbs.  
     The NDB Wait Thread waits for all pending transactions to return.
  */
  uv_async_t async_handle;

  /* An AsyncNdbContext serves a single cluster connection
  */
  Ndb_cluster_connection * connection;

  /* The wait group manages the list of NDBs that have been sent.
  */
  NdbWaitGroup * waitgroup;

  /* The sent queue holds Ndbs which have just been sent (executeAsynch). 
  */
  SharedList<Ndb> sent_queue;
  
  /* The completed queue holds Ndbs which have returned from execution. 
  */
  SharedList<Ndb> completed_queue;

  /* Holds the thread ID of the Listener thread
  */
  uv_thread_t listener_thread_id;
};

