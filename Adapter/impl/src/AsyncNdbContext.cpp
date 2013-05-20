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
#include "AsyncNdbContext.h"
#include "AsyncMethodCall.h"
#include "NdbWrapperErrors.h"

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

/* ndbTxCompleted is the callback on tx->executeAsynch().
   It runs in the listener thread.
   Create a NativeMethodCall, and stash it in the Ndb object 
*/
void ndbTxCompleted(int status, NdbTransaction *tx, void *v) {
  typedef AsyncAsyncCall<int, NdbTransaction> MCALL;
  MCALL * mcallptr = (MCALL *) v;
  mcallptr->return_val = status;
  
  /* Now stuff the AsyncAsyncCall into the Ndb */
  tx->getNdb()->setCustomData(mcallptr);  
}

/* Launch the JavaScript callback for an individual execute call.
   Runs in the main thread.
*/
void main_thd_complete(AsyncCall *m) {
  v8::HandleScope scope;
  v8::TryCatch try_catch;
  try_catch.SetVerbose(true);

  m->handleErrors();
  DEBUG_PRINT("Dispatching Callback");
  m->doAsyncCallback(v8::Context::GetCurrent()->Global());

  /* exceptions */
  if(try_catch.HasCaught()) {
    try_catch.ReThrow();
  }
}

/* ====== Signals ===== */
static int SignalShutdown = 1;

/* ====== Class AsyncNdbContext ====== */

/* Constructor 
*/
AsyncNdbContext::AsyncNdbContext(Ndb_cluster_connection *conn) :
  connection(conn)
{
  DEBUG_MARKER(UDEB_DEBUG);

  /* Create the multi-wait group */
  waitgroup = connection->create_ndb_wait_group(MAX_CONCURRENCY);

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
      wait_timeout_millisec = 500;
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
        ndb->pollNdb(0, 1);  /* runs txNdbCompleted() */
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
  

/* This could run in a UV worker thread (JavaScript async execution)
   or possibly in the JavaScript thread (JavaScript sync execution)
*/
int AsyncNdbContext::executeAsynch(NdbTransaction *tx,
                                   int execType,
                                   int abortOption,
                                   int forceSend,
                                   v8::Persistent<v8::Function> jsCallback) {

  DEBUG_MARKER(UDEB_DEBUG);
  
  /* Create a container to help pass return values up the JS callback stack */
  typedef AsyncAsyncCall<int, NdbTransaction> MCALL;
  MCALL * mcallptr = new MCALL(tx, jsCallback,
                               getNdbErrorIfLessThanZero<int, NdbTransaction>);
  
  /* send the transaction to NDB */
  tx->executeAsynch((NdbTransaction::ExecType) execType,
                    ndbTxCompleted,
                    mcallptr,
                    (NdbOperation::AbortOption) abortOption,
                    forceSend);

  /* Attach it to the list of sent Ndbs */
  sent_queue.produce(new ListNode<Ndb>(tx->getNdb()));

  /* Notify the waitgroup that there is a new Ndb to wait on */
  waitgroup->wakeup();
  
  return 1;
}


/* This runs in the JavaScript main thread, at most once per uv_async_send().
   It dispatches JavaScript callbacks for completed operations.
*/
void AsyncNdbContext::completeCallbacks() {
  typedef AsyncAsyncCall<int, NdbTransaction> MCALL;
  ListNode<Ndb> * completedNdbs, * currentNode;
  
  completedNdbs = completed_queue.consumeAll();

  while(completedNdbs != 0) {
    currentNode = completedNdbs;
    Ndb * ndb = currentNode->item;
    MCALL * mcallptr = (MCALL *) ndb->getCustomData();
    ndb->setCustomData(0);

    main_thd_complete(mcallptr);
    completedNdbs = currentNode->next;

    delete mcallptr;     // Frees the AsyncAsyncCall from executeAsynch() 
    delete currentNode;  // Frees the ListNode from runListenerThread()
  }
}

