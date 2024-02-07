/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "AsyncNdbContext.h"
#include "AsyncMethodCall.h"
#include "NdbWrapperErrors.h"
#include "TransactionImpl.h"
#include "adapter_global.h"

/* Thread starter, for pthread_create()
 */
void run_ndb_listener_thread(void *v) {
  AsyncNdbContext *ctx = (AsyncNdbContext *)v;
  ctx->runListenerThread();
}

/* ioCompleted will run in the JavaScript main thread.
   If the waiter thread called uv_async_send() more than once within an
   interval, libuv might coalesce those into a single call here.
*/
void ioCompleted(uv_async_t *ndbWaitLoop) {
  AsyncNdbContext *ctx = (AsyncNdbContext *)ndbWaitLoop->data;
  ctx->completeCallbacks();
}

/* Class AsyncExecCall
 */
class AsyncExecCall : public AsyncAsyncCall<int, NdbTransaction> {
 public:
  AsyncExecCall(NdbTransaction *tx, v8::Local<v8::Function> jsCallback)
      : AsyncAsyncCall<int, NdbTransaction>(
            tx, jsCallback, getNdbErrorIfLessThanZero<int, NdbTransaction>) {}
  TransactionImpl *closeContext;

  void closeTransaction() {
    if (closeContext) {
      DEBUG_PRINT("Closing");
      closeContext->closeTransaction();
      closeContext->registerClose();
    }
  }
};

/* ndbTxCompleted is the callback on tx->executeAsynch().
   Cast the void pointer back to AsyncExecCall and set its return value.
   TODO: Is a HandleScope needed and if so who owns it??
*/
void ndbTxCompleted(int status, NdbTransaction *tx, void *v) {
  DEBUG_PRINT("ndbTxCompleted: %d %p %p", status, tx, v);
  AsyncExecCall *mcallptr = (AsyncExecCall *)v;
  mcallptr->return_val = status;
  mcallptr->handleErrors();
  mcallptr->closeTransaction();
  tx->getNdb()->setCustomData(mcallptr);
}

/* ====== Class AsyncNdbContext ====== */

/* Constructor
 */
AsyncNdbContext::AsyncNdbContext(Ndb_cluster_connection *conn)
    : connection(conn), shutdown_flag() {
  DEBUG_MARKER(UDEB_DEBUG);

  /* Create the multi-wait group */
  waitgroup = connection->create_ndb_wait_group(WAIT_GROUP_SIZE);

  /* Register the completion function */
  uv_async_init(uv_default_loop(), &async_handle, ioCompleted);

  /* Store some context in the uv_async_t */
  async_handle.data = (void *)this;

  /* Start the listener thread. */
  uv_thread_create(&listener_thread_id, run_ndb_listener_thread, (void *)this);
}

/* Destructor
 */
AsyncNdbContext::~AsyncNdbContext() {
  uv_thread_join(&listener_thread_id);
  connection->release_ndb_wait_group(waitgroup);
}

/* Methods
 */

/* This could run in a UV worker thread (JavaScript async execution)
   or possibly in the JavaScript thread (JavaScript sync execution)
*/
int AsyncNdbContext::executeAsynch(TransactionImpl *txc, NdbTransaction *tx,
                                   int execType, int abortOption, int forceSend,
                                   v8::Local<v8::Function> jsCallback) {
  /* Create a container to help pass return values up the JS callback stack */
  AsyncExecCall *mcallptr = new AsyncExecCall(tx, jsCallback);

  Ndb *ndb = tx->getNdb();
  DEBUG_PRINT("NdbTransaction:%p:executeAsynch(%d,%d) -- Push: %p",
              mcallptr->native_obj, execType, abortOption, ndb);

  /* The NdbTransaction should be closed unless execType is NoCommit */
  mcallptr->closeContext = (execType == NdbTransaction::NoCommit) ? 0 : txc;

  /* send the transaction to NDB */
  tx->executeAsynch((NdbTransaction::ExecType)execType, ndbTxCompleted,
                    mcallptr, (NdbOperation::AbortOption)abortOption,
                    forceSend);

  waitgroup->push(ndb);

  /* Notify the waitgroup that there is a new Ndb to wait on */
  /* TODO: This could depend on forceSend? */
  waitgroup->wakeup();

  return 1;
}

void *AsyncNdbContext::runListenerThread() {
  DEBUG_MARKER(UDEB_DEBUG);
  int wait_timeout_millisec = 100;
  int pct_ready = 50;
  bool running = true;

  while (running) {
    if (shutdown_flag.test()) {
      DEBUG_PRINT("MULTIWAIT LISTENER GOT SHUTDOWN.");
      pct_ready = 100; /* One final read of all outstanding items */
      wait_timeout_millisec = 200;
      running = false;
    }

    /* Wait for ready Ndbs */
    if (waitgroup->wait(wait_timeout_millisec, pct_ready) > 0) {
      uv_async_send(&async_handle);  // => ioCompleted() => completeCallbacks()
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
  AsyncExecCall *mcallptr;
  Ndb *ndb = waitgroup->pop();

  while (ndb) {
    DEBUG_PRINT("                                           -- Pop:  %p", ndb);
    ndb->pollNdb(0, 1); /* runs ndbTxCompleted() */
    mcallptr = (AsyncExecCall *)ndb->getCustomData();
    ndb->setCustomData(0);
    main_thd_complete_async_call(mcallptr);
    ndb = waitgroup->pop();
  }
}
