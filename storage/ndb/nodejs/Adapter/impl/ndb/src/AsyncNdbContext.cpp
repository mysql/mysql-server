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

#include "adapter_global.h"
#include "async_common.h"
#include "NdbWrapperErrors.h"
#include "AsyncNdbContext.h"
#include "AsyncMethodCall.h"
#include "DBTransactionContext.h"

/* Thread starter, for pthread_create()
*/
PTHREAD_RETURN_TYPE run_ndb_listener_thread(void *v) {
  AsyncNdbContext * ctx = (AsyncNdbContext *) v;
  ctx->runListenerThread();
  return PTHREAD_RETURN_VAL;
}

/* ioCompleted will run in the JavaScript main thread.
   If the waiter thread called uv_async_send() more than once within an 
   interval, libuv might coalesce those into a single call here.
*/
void ioCompleted(uv_async_t *ndbWaitLoop, int) {
  AsyncNdbContext * ctx = (AsyncNdbContext *) ndbWaitLoop->data;
  ctx->completeCallbacks();
}

/* Class AsyncExecCall
*/
class AsyncExecCall : public AsyncAsyncCall<int, NdbTransaction> {
public: 
  AsyncExecCall(NdbTransaction *tx, v8::Persistent<v8::Function> jsCallback) :
    AsyncAsyncCall<int, NdbTransaction>(tx, jsCallback, 
      getNdbErrorIfLessThanZero<int, NdbTransaction>)                        {};
  DBTransactionContext * closeContext;
  
  void closeTransaction() {
    if(closeContext) {
      DEBUG_PRINT("Closing");
      closeContext->closeTransaction();
      closeContext->registerClose();
    }
  }
};

/* ndbTxCompleted is the callback on tx->executeAsynch().
   Cast the void pointer back to AsyncExecCall and set its return value.
*/
void ndbTxCompleted(int status, NdbTransaction *tx, void *v) {
  DEBUG_PRINT("ndbTxCompleted: %d %p %p", status, tx, v);
  AsyncExecCall * mcallptr = (AsyncExecCall *) v;
  mcallptr->return_val = status;
  mcallptr->handleErrors();
  mcallptr->closeTransaction();
  tx->getNdb()->setCustomData(mcallptr);
}


/* ====== Class AsyncNdbContext ====== */

/* Constructor 
*/
AsyncNdbContext::AsyncNdbContext(Ndb_cluster_connection *conn) :
  connection(conn),
  shutdown_flag()
{
  DEBUG_MARKER(UDEB_DEBUG);

  /* Create the multi-wait group */
  waitgroup = connection->create_ndb_wait_group(WAIT_GROUP_SIZE);

  /* Register the completion function */
  uv_async_init(uv_default_loop(), & async_handle, ioCompleted);
  
  /* Store some context in the uv_async_t */
  async_handle.data = (void *) this;
  
  /* Start the listener thread. */
  uv_thread_create(& listener_thread_id, run_ndb_listener_thread, (void *) this);
}


/* Destructor 
*/
AsyncNdbContext::~AsyncNdbContext()
{
  uv_thread_join(& listener_thread_id);
  connection->release_ndb_wait_group(waitgroup);  
}


/* Methods 
*/

/* This could run in a UV worker thread (JavaScript async execution)
   or possibly in the JavaScript thread (JavaScript sync execution)
*/
int AsyncNdbContext::executeAsynch(DBTransactionContext *txc,
                                   NdbTransaction *tx,
                                   int execType,
                                   int abortOption,
                                   int forceSend,
                                   v8::Persistent<v8::Function> jsCallback) {
  
  /* Create a container to help pass return values up the JS callback stack */
  AsyncExecCall * mcallptr = new AsyncExecCall(tx, jsCallback);
  
  Ndb * ndb = tx->getNdb();
  DEBUG_PRINT("NdbTransaction:%p:executeAsynch(%d,%d) -- Push: %p", 
              mcallptr->native_obj, execType, abortOption, ndb);

  /* The NdbTransaction should be closed unless execType is NoCommit */
  mcallptr->closeContext = (execType == NdbTransaction::NoCommit) ? 0 : txc;

  /* send the transaction to NDB */
  tx->executeAsynch((NdbTransaction::ExecType) execType,
                    ndbTxCompleted,
                    mcallptr,
                    (NdbOperation::AbortOption) abortOption,
                    forceSend);

#ifdef USE_OLD_MULTIWAIT_API
  sent_queue.produce(new ListNode<Ndb>(ndb));
#else
  waitgroup->push(ndb);
#endif

  /* Notify the waitgroup that there is a new Ndb to wait on */
  /* TODO: This could depend on forceSend? */
  waitgroup->wakeup();
  
  return 1;
}


#ifndef USE_OLD_MULTIWAIT_API

void * AsyncNdbContext::runListenerThread() {
  DEBUG_MARKER(UDEB_DEBUG);
  int wait_timeout_millisec = 100;
  int pct_ready = 50;
  bool running = true;

  while(running) {
    if(shutdown_flag.test()) {
      DEBUG_PRINT("MULTIWAIT LISTENER GOT SHUTDOWN.");
      pct_ready = 100;    /* One final read of all outstanding items */
      wait_timeout_millisec = 200;
      running = false;
    }

    /* Wait for ready Ndbs */
    if(waitgroup->wait(wait_timeout_millisec, pct_ready) > 0) {
      uv_async_send(& async_handle);  // => ioCompleted() => completeCallbacks()
    }
  }

  return 0;
}

void AsyncNdbContext::shutdown() {
  DEBUG_MARKER(UDEB_DEBUG);
  shutdown_flag.set();
  waitgroup->wakeup();
}


void AsyncNdbContext::completeCallbacks() {
  AsyncExecCall * mcallptr;
  Ndb * ndb = waitgroup->pop();
  
  while(ndb) {
    DEBUG_PRINT("                                           -- Pop:  %p", ndb);
    ndb->pollNdb(0, 1);  /* runs ndbTxCompleted() */
    mcallptr = (AsyncExecCall *) ndb->getCustomData();
    ndb->setCustomData(0);
    main_thd_complete_async_call(mcallptr);
    ndb = waitgroup->pop();
  }
}

#else     /* Old Multiwait */

/* ====== Signals ===== */
static int SignalShutdown = 1;

void * AsyncNdbContext::runListenerThread() {
  DEBUG_MARKER(UDEB_DEBUG);
  ListNode<Ndb> * sentNdbs, * completedNdbs, * currentNode;
  Ndb * ndb;
  Ndb ** ready_list;
  int wait_timeout_millisec = 5000;
  int min_ready, nwaiting, npending = 0;
  bool running = true;

  while(running) {  // Listener thread main loop
  
    /* Add new Ndbs to the wait group */
    sentNdbs = sent_queue.consumeAll();
    while(sentNdbs != 0) {
      currentNode = sentNdbs;
      sentNdbs = sentNdbs->next;

      if(currentNode->signalinfo == SignalShutdown) {
        running = false;
      }
      else {
        waitgroup->addNdb(currentNode->item);
        npending++;
        DEBUG_PRINT("Listener: %d pending", npending);
      }
      delete currentNode;   // Frees the ListNode from executeAsynch() 
    }

    /* What's the minimum number of ready Ndb's to wake up for? */
    if(! running) {
      min_ready = npending;  // Wait one final time for all outstanding Ndbs
      wait_timeout_millisec = 200;
    }
    else {
      int n = npending / 4;
      min_ready = n > 0 ? n : 1;
    }
    
    /* Wait until something is ready to poll */
    nwaiting = waitgroup->wait(ready_list, wait_timeout_millisec, min_ready);

    completedNdbs = 0;
    if(nwaiting > 0) {
      /* Poll the ones that are ready */
      DEBUG_PRINT("Listener: %d ready", nwaiting);
      for(int i = 0 ; i < nwaiting ; i++) {
        npending--;
        assert(npending >= 0);
        ndb = ready_list[i];
        ndb->pollNdb(0, 1);  /* runs ndbTxCompleted() */
        currentNode = new ListNode<Ndb>(ndb);
        currentNode->next = completedNdbs;
        completedNdbs = currentNode;
      }

      /* Publish the completed ones */
      completed_queue.produce(completedNdbs);

      /* Notify the main thread */
      uv_async_send(& async_handle);
    }
  } // Listener thread main loop

  return 0;
}


/* Shut down the context 
*/
void AsyncNdbContext::shutdown() {
  DEBUG_MARKER(UDEB_DEBUG);
  ListNode<Ndb> * finalNode = new ListNode<Ndb>((Ndb *) 0);
  finalNode->signalinfo = SignalShutdown;

  /* Queue the shutdown node, and wake up the listener thread for it */
  sent_queue.produce(finalNode);
  waitgroup->wakeup();
}
  

/* This runs in the JavaScript main thread, at most once per uv_async_send().
   It dispatches JavaScript callbacks for completed operations.
*/
void AsyncNdbContext::completeCallbacks() {
  ListNode<Ndb> * completedNdbs, * currentNode;
  
  completedNdbs = completed_queue.consumeAll();

  while(completedNdbs != 0) {
    currentNode = completedNdbs;
    Ndb * ndb = currentNode->item;
    AsyncExecCall * mcallptr = static_cast<AsyncExecCall *>(ndb->getCustomData());
    ndb->setCustomData(0);

    main_thd_complete_async_call(mcallptr);
    completedNdbs = currentNode->next;

    delete currentNode;  // Frees the ListNode from runListenerThread()
  }
}

#endif

