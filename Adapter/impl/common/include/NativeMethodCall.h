/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

#include <assert.h>

#include <v8.h>

#include "v8_binder.h"
#include "JsConverter.h"
#include "async_common.h"

using namespace v8;


/** These classes wrap various sorts of C and C++ functions for use as either
  * synchronous or asynchronous JavaScript methods.
  *
  * The base class AsyncMethodCall wraps the worker-thread run() routine
  * and the main thread (post-run) doAsyncCallback() needed for async execution.
  *
  * The run() method, declared void run(void), will be scheduled to run in a 
  * uv worker thread.
  * 
  * The doAsyncCallback() method will take a JavaScript context.  It is expected
  * to prepare the result and call the user's callback function.
  * 
  * The templated class NativeMethodCall<R> inherits from AsyncMethodCall and
  * adds a return type, which is initialized at 0.
  *
  * Finally several sets of templated classes inherit from NativeMethodCall<> 
  * adding arguments.  In this file, the set NativeMethodCall_0_ etc. wrap 
  * method calls on C++ objects.  
  */


/** Base class
**/
class AsyncMethodCall {
  public:
    /* Member variables */
    bool has_cb;
    Persistent<Function> callback;
    Envelope * envelope;
    
    /* Constructors */
    AsyncMethodCall() : callback(), envelope(0), has_cb(false) {}
    
    AsyncMethodCall(Local<Value> f) {
      // FIXME: raise an error if you use the wrong type as a callback
      callback = Persistent<Function>::New(Local<Function>::Cast(f));
      has_cb = true;      
    }

    /* Destructor */
    ~AsyncMethodCall() {
      if(has_cb) callback.Dispose();
    }

    /* Methods (Pure virtual) */
    virtual void run(void) = 0;
    virtual void doAsyncCallback(Local<Object>) = 0;
    
    /* Base Class Methods */
    void runAsync() {
      uv_work_t * req = new uv_work_t;
      req->data = (void *) this;
      uv_queue_work(uv_default_loop(), req, work_thd_run, main_thd_complete);
    }    
};  


/** First-level template class;
    templated over return types
**/
template <typename RETURN_TYPE> 
class NativeMethodCall : public AsyncMethodCall {
public:
  /* Member variables */
  RETURN_TYPE return_val;

  /* Constructor */
  NativeMethodCall<RETURN_TYPE>(Local<Value> callback) : 
    AsyncMethodCall(callback), return_val(0) {};

  /* Methods */
  Local<Value> jsReturnVal() {
    HandleScope scope;

    if(isPointer(return_val)) {
      DEBUG_ASSERT(envelope);
      Local<Object> obj = envelope->newWrapper();
      wrapPointerInObject(return_val, *envelope, obj);
      return scope.Close(obj);
    }
    else {
      return scope.Close(toJS(return_val));
    }
  }
  
  /* doAsyncCallback() is an async callback, run by main_thread_complete().
     It currently returns *one* JavaScript object (wrapped return_val), 
     and no error indicator
  */
  void doAsyncCallback(Local<Object> context) {
    DEBUG_PRINT("doAsyncCallback() for %s", 
                (envelope && envelope->classname) ? 
                envelope->classname : "{?}");
    Handle<Value> cb_args[1];    
    cb_args[0] = jsReturnVal();
    callback->Call(context, 1, cb_args);
  }

};


/** Template class with:
 * wrapped class C
 * one argument of type A0
 * Wrapped native method call returning void
 * The javascript return value is integer 0. 
 * 
**/

template <typename C, typename A0> 
class NativeVoidMethodCall_1_ : public NativeMethodCall<int> {
public:
  /* Member variables */
  C * native_obj;

  JsValueConverter<A0> arg0converter;
  A0  arg0;

  void (C::*method)(A0);  // "method" is pointer to member function
  
  /* Constructor */
  NativeVoidMethodCall_1_<C, A0>(const Arguments &args) : 
    method(0),
    arg0converter(args[0]),
    NativeMethodCall<int>(args[1]) // callback
  {
    native_obj = unwrapPointer<C *>(args.Holder());
    arg0 = arg0converter.toC();
  }
  
  /* Methods */
  void run() {
    assert(method);
    ((native_obj)->*(method))(arg0);
  }
};



/** Template class with:
 * wrapped class C
 * no arguments
 * return value of type R
 **/

template <typename R, typename C> 
class NativeMethodCall_0_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(void);  // "method" is pointer to member function
  
  /* Constructor */
  NativeMethodCall_0_<R, C>(const Arguments &args) : 
  method(0),
  NativeMethodCall<R>(args[0]) // callback
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }
  
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))();
  }
};


/** Template class with:
  * wrapped class C
  * one argument of type A0
  * return value of type R
**/

template <typename R, typename C, typename A0> 
class NativeMethodCall_1_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  C * native_obj;
  JsValueConverter<A0> arg0converter;
  A0  arg0;
  R (C::*method)(A0);  // "method" is pointer to member function
  
  /* Constructor */
  NativeMethodCall_1_<R, C, A0>(const Arguments &args) : 
  method(0),
  arg0converter(args[0]),
  NativeMethodCall<R>(args[1]) // callback
  {
    native_obj = unwrapPointer<C *>(args.Holder());    
    arg0 = arg0converter.toC();
  }

  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(arg0);
  }
};


/** Template class with:
 * wrapped class C
 * two arguments of type A0 and A1
 * return type R
 **/

template <typename R, typename C, typename A0, typename A1> 
class NativeMethodCall_2_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  C * native_obj;
  JsValueConverter<A0> arg0converter;
  JsValueConverter<A1> arg1converter;
  A0  arg0;
  A1  arg1;
  R (C::*method)(A0, A1);  // "method" is pointer to member function
  
  /* Constructor */
  NativeMethodCall_2_<R, C, A0, A1>(const Arguments &args) :
  method(0),
  arg0converter(args[0]),
  arg1converter(args[1]),
  NativeMethodCall<R>(args[2]) // callback
  {
    native_obj = unwrapPointer<C *>(args.Holder());
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
  }
  
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(arg0, arg1);
  }
};



/** Template class with:
 * wrapped class C
 * three arguments of type A0, A1, and A2
 * return type R
 **/

template <typename R, typename C, typename A0, typename A1, typename A2> 
class NativeMethodCall_3_ : public NativeMethodCall <R> {
public:
  /* Member variables */
  C * native_obj;
  JsValueConverter<A0> arg0converter;
  JsValueConverter<A1> arg1converter;
  JsValueConverter<A2> arg2converter;
  A0  arg0;
  A1  arg1;
  A2  arg2;
  R (C::*method)(A0, A1, A2);  // "method" is pointer to member function
  
  /* Constructor */
  NativeMethodCall_3_<R, C, A0, A1, A2>(const Arguments &args) : 
  method(0),
  arg0converter(args[0]),
  arg1converter(args[1]),
  arg2converter(args[2]),
  NativeMethodCall<R>(args[3]) // callback
  {
    native_obj = unwrapPointer<C *>(args.Holder());
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg1converter.toC();
  }
  
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(arg0, arg1, arg2);
  }
};

