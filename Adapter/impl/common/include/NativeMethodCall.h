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


/** Base class templated over arguments
*/
template <typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6, typename A7> 
class Call_8_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;
  JsValueConverter<A5> arg5converter;       A5 arg5;
  JsValueConverter<A6> arg6converter;       A6 arg6;
  JsValueConverter<A7> arg7converter;       A7 arg7;

  /* Constructor */
  Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4]),
    arg5converter(args[5]),
    arg6converter(args[6]),
    arg7converter(args[7])   
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
    arg5 = arg5converter.toC();
    arg6 = arg6converter.toC();
    arg7 = arg7converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6> 
class Call_7_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;
  JsValueConverter<A5> arg5converter;       A5 arg5;
  JsValueConverter<A6> arg6converter;       A6 arg6;

  /* Constructor */
  Call_7_<A0, A1, A2, A3, A4, A5, A6>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4]),
    arg5converter(args[5]),
    arg6converter(args[6])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
    arg5 = arg5converter.toC();
    arg6 = arg6converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5> 
class Call_6_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;
  JsValueConverter<A5> arg5converter;       A5 arg5;

  /* Constructor */
  Call_6_<A0, A1, A2, A3, A4, A5>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4]),
    arg5converter(args[5])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
    arg5 = arg5converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3, typename A4> 
class Call_5_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;

  /* Constructor */
  Call_5_<A0, A1, A2, A3, A4>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4]) 
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3> 
class Call_4_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;

  /* Constructor */
  Call_4_<A0, A1, A2, A3>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
  }
};


template <typename A0, typename A1, typename A2> 
class Call_3_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;

  /* Constructor */
  Call_3_<A0, A1, A2>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
  }
};


template <typename A0, typename A1> 
class Call_2_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;

  /* Constructor */
  Call_2_<A0, A1>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1])  
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
  }
};


template <typename A0> 
class Call_1_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;

  /* Constructor */
  Call_1_<A0>(const Arguments &args) :
    arg0converter(args[0])
  {
    arg0 = arg0converter.toC();
  }
};

/***********************************************************/

/** Template class with 
 *  Wrapped class C
 *  2 Arguments
 *  Const method returning void
 *
**/
template <typename C, typename A0, typename A1> 
class NativeConstVoidMethodCall_2_ : public NativeMethodCall<int> ,
                                     public Call_2_<A0, A1>
{
public:
  /* Member variables */
  C * native_obj;
  void (C::*method)(A0, A1) const; 

  /* Constructor */
  NativeConstVoidMethodCall_2_<C, A0, A1>(const Arguments &args) : 
    method(0),
    Call_2_<A0, A1>(args), 
    NativeMethodCall<int>(args[2]) /* callback */
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }

  /* Methods */
  void run() {
    assert(method);
    ((native_obj)->*(method))(Call_2_<A0,A1>::arg0, Call_2_<A0,A1>::arg1);
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
class NativeVoidMethodCall_1_ : public NativeMethodCall<int> ,
                                public Call_1_<A0>
{
public:
  /* Member variables */
  C * native_obj;
  void (C::*method)(A0);  // "method" is pointer to member function
  
  /* Constructor */
  NativeVoidMethodCall_1_<C, A0>(const Arguments &args) : 
    method(0),
    Call_1_<A0>(args), 
    NativeMethodCall<int>(args[1]) /* callback */
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }

  /* Methods */
  void run() {
    assert(method);
    ((native_obj)->*(method))(Call_1_<A0>::arg0);
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

/* Const version */

template <typename R, typename C> 
class NativeConstMethodCall_0_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(void) const;  // "method" is pointer to member function
  
  /* Constructor */
  NativeConstMethodCall_0_<R, C>(const Arguments &args) : 
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
class NativeMethodCall_1_ : public NativeMethodCall<R>,
                            public Call_1_<A0>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0);  // "method" is pointer to member function
  
  /* Constructor */
  NativeMethodCall_1_<R, C, A0>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[1]),  /* callback */
    Call_1_<A0>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_1_<A0>::arg0);
  }
};


/* Const version */
template <typename R, typename C, typename A0> 
class NativeConstMethodCall_1_ : public NativeMethodCall<R>,
                                 public Call_1_<A0>
{
public:
  /* Member variables */
  C * native_obj;
  R ((C::*method)(A0) const);  // "method" is pointer to member function
  
  /* Constructor */
  NativeConstMethodCall_1_<R, C, A0>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[1]),  /* callback */
    Call_1_<A0>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }

  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_1_<A0>::arg0);
  }
};



/** Template class with:
 * wrapped class C
 * two arguments of type A0 and A1
 * return type R
 **/

template <typename R, typename C, typename A0, typename A1> 
class NativeMethodCall_2_ : public NativeMethodCall<R>,
                            public Call_2_<A0, A1>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0, A1);  // "method" is pointer to member function
  
  /* Constructor */
  NativeMethodCall_2_<R, C, A0, A1>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[2]),  /* callback */
    Call_2_<A0, A1>(args)  
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }

  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_2_<A0, A1>::arg0, 
      Call_2_<A0, A1>::arg1);
  }
};


/* Const version */
template <typename R, typename C, typename A0, typename A1> 
class NativeConstMethodCall_2_ : public NativeMethodCall<R>,
                                 public Call_2_<A0, A1>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0, A1) const;  // "method" is pointer to member function
  
  /* Constructor */
  NativeConstMethodCall_2_<R, C, A0, A1>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[2]),  /* callback */
    Call_2_<A0, A1>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }

  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_2_<A0, A1>::arg0, 
      Call_2_<A0, A1>::arg1);
  }
};


/** Template class with:
 * wrapped class C
 * three arguments of type A0, A1, and A2
 * return type R
 **/

template <typename R, typename C, typename A0, typename A1, typename A2> 
class NativeMethodCall_3_ : public NativeMethodCall <R>,
                            public Call_3_<A0, A1, A2>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0, A1, A2);  // "method" is pointer to member function
  
  /* Constructor */
  NativeMethodCall_3_<R, C, A0, A1, A2>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[3]),  /* callback */
    Call_3_<A0, A1, A2>(args) 
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }
      
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_3_<A0, A1, A2>::arg0,
      Call_3_<A0, A1, A2>::arg1,
      Call_3_<A0, A1, A2>::arg2
    );
  }
};


/** Template class with:
  * 4 arguments
  * return value of type R
**/

template <typename R, typename C, 
          typename A0, typename A1, typename A2, typename A3> 
class NativeMethodCall_4_ : public NativeMethodCall<R> ,
                            public Call_4_<A0, A1, A2, A3>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0,A1,A2,A3);
  
  /* Constructor */
  NativeMethodCall_4_<R, C, A0, A1, A2, A3>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[4]),  /* callback */
    Call_4_<A0, A1, A2, A3>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }
  
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_4_<A0, A1, A2, A3>::arg0,
      Call_4_<A0, A1, A2, A3>::arg1,
      Call_4_<A0, A1, A2, A3>::arg2,
      Call_4_<A0, A1, A2, A3>::arg3
    );
  }
};


/** Template class with:
  * 5 arguments
  * return value of type R
**/

template <typename R, typename C, 
          typename A0, typename A1, typename A2, typename A3, typename A4> 
class NativeMethodCall_5_ : public NativeMethodCall<R>,
                            public Call_5_<A0, A1, A2, A3, A4>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0,A1,A2,A3,A4);
  
  /* Constructor */
  NativeMethodCall_5_<R, C, A0, A1, A2, A3, A4>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[5]),  /* callback */
    Call_5_<A0, A1, A2, A3, A4>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }

  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_5_<A0, A1, A2, A3, A4>::arg0,
      Call_5_<A0, A1, A2, A3, A4>::arg1,
      Call_5_<A0, A1, A2, A3, A4>::arg2,
      Call_5_<A0, A1, A2, A3, A4>::arg3,
      Call_5_<A0, A1, A2, A3, A4>::arg4
    );
  }
};


/** Template class with:
  * 6 arguments
  * return value of type R
**/

template <typename R, typename C, 
          typename A0, typename A1, typename A2, 
          typename A3, typename A4, typename A5> 
class NativeMethodCall_6_ : public NativeMethodCall<R> ,
                            public Call_6_<A0, A1, A2, A3, A4, A5>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0,A1,A2,A3,A4,A5);
  
  /* Constructor */
  NativeMethodCall_6_<R, C, A0, A1, A2, A3, A4, A5>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[6]),  /* callback */
    Call_6_<A0, A1, A2, A3, A4, A5>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }
    
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_6_<A0, A1, A2, A3, A4, A5>::arg0,
      Call_6_<A0, A1, A2, A3, A4, A5>::arg1,
      Call_6_<A0, A1, A2, A3, A4, A5>::arg2,
      Call_6_<A0, A1, A2, A3, A4, A5>::arg3,
      Call_6_<A0, A1, A2, A3, A4, A5>::arg4,
      Call_6_<A0, A1, A2, A3, A4, A5>::arg5
    );
  }
};


/** Template class with:
  * 7 arguments
  * return value of type R
**/

template <typename R, typename C, 
          typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6> 
class NativeMethodCall_7_ : public NativeMethodCall<R>, 
                            public Call_7_<A0, A1, A2, A3, A4, A5, A6>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0,A1,A2,A3,A4,A5,A6);
  
  /* Constructor */
  NativeMethodCall_7_<R, C, A0, A1, A2, A3, A4, A5, A6>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[7]),  /* callback */
    Call_7_<A0, A1, A2, A3, A4, A5, A6>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }
    
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_7_<A0, A1, A2, A3, A4, A5, A6>::arg0,
      Call_7_<A0, A1, A2, A3, A4, A5, A6>::arg1,
      Call_7_<A0, A1, A2, A3, A4, A5, A6>::arg2,
      Call_7_<A0, A1, A2, A3, A4, A5, A6>::arg3,
      Call_7_<A0, A1, A2, A3, A4, A5, A6>::arg4,
      Call_7_<A0, A1, A2, A3, A4, A5, A6>::arg5,
      Call_7_<A0, A1, A2, A3, A4, A5, A6>::arg6
    );
  }
};


/** Template class with:
  * 8 arguments
  * return value of type R
**/

template <typename R, typename C, 
          typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6, typename A7> 
class NativeMethodCall_8_ : public NativeMethodCall<R>,
                            public Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>
{
public:
  /* Member variables */
  C * native_obj;
  R (C::*method)(A0,A1,A2,A3,A4,A5,A6,A7);
  
  /* Constructor */
  NativeMethodCall_8_<R, C, A0, A1, A2, A3, A4, A5, A6, A7>(const Arguments &args) : 
    method(0),
    NativeMethodCall<R>(args[8]),  /* callback */
    Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>(args)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }
  
  /* Methods */
  void run() {
    assert(method);
    NativeMethodCall<R>::return_val = ((native_obj)->*(method))(
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg0,
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg1,
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg2,
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg3,
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg4,
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg5,
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg6,
      Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>::arg7
    );    
  }
};
