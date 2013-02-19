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

#include <node.h>
#include <uv.h>

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "AsyncNdbContext.h"
#include "AsyncMethodCall.h"
#include "NdbWrapperErrors.h"


/* Thread starter, for pthread_create()
*/
RUN_THREAD_RETURN run_ndb_listener_thread(void *v) {
  AsyncNdbContext * ctx = (AsyncNdbContext *) v;
  ctx->runListenerThread();
#ifdef FORCE_UV_LEGACY_COMPAT 
  return NULL;
#endif
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
  mcallptr->handleErrors();
  
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

  m->doAsyncCallback(v8::Context::GetCurrent()->Global());

  /* exceptions */
  if(try_catch.HasCaught()) {
    try_catch.ReThrow();
  }
}


/* ====== Class AsyncNdbContext ====== */

/* Constructor 
*/
AsyncNdbContext::AsyncNdbContext(Ndb_cluster_connection *conn)
{
  /* Create the multi-wait group */
  waitgroup = conn->create_ndb_wait_group(MAX_CONCURRENCY);

  /* Register the completion function */
  uv_async_init(uv_default_loop(), & async_handle, ioCompleted);
  
  /* Store some context in the uv_async_t */
  async_handle.data = (void *) this;
}


/* Start the listener thread.
   This is separated from the constructor so that the constructor can be 
   wrapped by a synchronous JavaScript call, but then startListenerThread() is
   wrapped by an async call.
*/
void AsyncNdbContext::startListenerThread() {
  uv_thread_create(& listener_thread_id, run_ndb_listener_thread, (void *) this);
}


void * AsyncNdbContext::runListenerThread() {
  ListNode<Ndb> * sentNdbs, * completedNdbs, * currentNode;
  Ndb * ndb;
  Ndb ** ready_list;
  int wait_timeout_millisec = 5000;
  int n_added, min_ready, nwaiting;

  while(1) {  // Listener thread main loop
  
    /* Add new Ndbs to the wait group */
    n_added = 0;
    sentNdbs = sent_queue.consumeAll();
    while(sentNdbs != 0) {
      currentNode = sentNdbs;
      sentNdbs = sentNdbs->next;
      ndb = currentNode->item;
      waitgroup->addNdb(ndb);
      n_added++;
      delete currentNode;   // Frees the ListNode from executeAsynch() 
    }

    /* What's the minimum number of ready Ndb's to wake up for? */
    int n = n_added / 4;
    min_ready = n > 0 ? n : 1;
    
    /* Wait until something is ready to poll */
    nwaiting = waitgroup->wait(ready_list, wait_timeout_millisec, min_ready);

    completedNdbs = 0;
    if(nwaiting > 0) {
      /* Poll the ones that are ready */
      for(int i = 0 ; i < nwaiting ; i++) {
        ndb = ready_list[i];
        ndb->pollNdb(0, 1);
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
}


/* This could run in a UV worker thread (JavaScript async execution)
   or possibly in the JavaScript thread (JavaScript sync execution)
*/

int AsyncNdbContext::executeAsynch(NdbTransaction *tx,
                                   int execType,
                                   int abortOption,
                                   int forceSend,
                                   v8::Local<v8::Value> jsCallback) {
  
  /* Create a container to help pass return values up the JS callback stack */
  typedef AsyncAsyncCall<int, NdbTransaction> MCALL;
  MCALL * mcallptr = new MCALL(tx, jsCallback);
  mcallptr->errorHandler = getNdbErrorIfNonZero<int, NdbTransaction>;
  
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
    delete currentNode;  // Frees the ListNode runListenerThread()
  }
}
