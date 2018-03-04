/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "AsyncMethodCall.h"

using namespace v8;

/** These templated classes represent normal C or C++ function calls.
  *
  * They inherit from AsyncCall_Returning<R>, which encapsualtes
  * wrapper functions for async execution, and some return type,
  * and from a Call_N_<> template class templated over argument types.
  *
  */


/** Template class with:
  * no arguments
  * return value of type R
**/

template <typename R>
class NativeCFunctionCall_0_ : public AsyncCall_Returning<R> {
public:
  /* Member variables */
  typedef R (*Function_T)();
  Function_T function;

  /* Constructor */
  NativeCFunctionCall_0_<R>(Function_T f, const Arguments &args) :
  AsyncCall_Returning<R>(args.GetIsolate(), args[0]) /*callback*/ ,
  function(f)
  { }

  /* Methods */
  void run() {
    AsyncCall_Returning<R>::return_val = (*function)();
  }
};


/** Template class with:
  * one argument of type A0
  * return value of type R
**/

template <typename R, typename A0>
class NativeCFunctionCall_1_ : public AsyncCall_Returning<R>,
                               public Call_1_<A0>
{
public:
  /* Member variables */
  typedef R (*Function_T)(A0);
  Function_T function;

  /* Constructor */
  NativeCFunctionCall_1_<R, A0>(Function_T f, const Arguments &args) :
    AsyncCall_Returning<R>(args.GetIsolate(), args[1]), /* callback */
    Call_1_<A0>(args),
    function(f)
  { }

  /* Methods */
  void run() {
    AsyncCall_Returning<R>::return_val = (function)(Call_1_<A0>::arg0);
  }
};


/** Template class with:
  * two arguments of type A0, A1
  * return value of type R
**/

template <typename R, typename A0, typename A1>
class NativeCFunctionCall_2_ : public AsyncCall_Returning<R>,
                               public Call_2_<A0, A1>
{
public:
  /* Member variables */
  typedef R (*Function_T)(A0,A1);
  Function_T function;

  /* Constructor */
  NativeCFunctionCall_2_<R, A0, A1>(Function_T f, const Arguments &args) :
    AsyncCall_Returning<R>(args.GetIsolate(), args[2]), // callback
    Call_2_<A0, A1>(args),
    function(f)
  { }

  /* Methods */
  void run() {
    AsyncCall_Returning<R>::return_val = (function)(
      Call_2_<A0, A1>::arg0,
      Call_2_<A0, A1>::arg1
    );
  }
};


/** Template class with:
  * three arguments of type A0, A1, A2
  * return value of type R
**/

template <typename R, typename A0, typename A1, typename A2>
class NativeCFunctionCall_3_ : public AsyncCall_Returning<R>,
                               public Call_3_<A0, A1, A2>
{
public:
  /* Member variables */
  typedef R (*Function_T)(A0,A1,A2);
  Function_T function;

  /* Constructor */
  NativeCFunctionCall_3_<R, A0, A1, A2>(Function_T f, const Arguments &args) :
    AsyncCall_Returning<R>(args.GetIsolate(), args[3]), /* callback */
    Call_3_<A0, A1, A2>(args),
    function(f)
  { }

  /* Methods */
  void run() {
    AsyncCall_Returning<R>::return_val = (function)(
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

template <typename R, typename A0, typename A1, typename A2,
                      typename A3>
class NativeCFunctionCall_4_ : public AsyncCall_Returning<R>,
                               public Call_4_<A0, A1, A2, A3>
{
public:
  /* Member variables */
  typedef R (*Function_T)(A0,A1,A2,A3);
  Function_T function;
  
  /* Constructor */
  NativeCFunctionCall_4_<R, A0, A1, A2, A3>(Function_T f, const Arguments &args) :
    AsyncCall_Returning<R>(args.GetIsolate(), args[4]),  /* callback */
    Call_4_<A0, A1, A2, A3>(args),
    function(f)
  { }

  /* Methods */
  void run() {
    AsyncCall_Returning<R>::return_val = (function)(
      Call_4_<A0, A1, A2, A3>::arg0,
      Call_4_<A0, A1, A2, A3>::arg1,
      Call_4_<A0, A1, A2, A3>::arg2,
      Call_4_<A0, A1, A2, A3>::arg3
    );
  }
};


/** Template class with:
  * 6 arguments
  * return value of type R
**/

template <typename R, typename A0, typename A1, typename A2,
                      typename A3, typename A4, typename A5>
class NativeCFunctionCall_6_ : public AsyncCall_Returning<R>,
                               public Call_6_<A0, A1, A2, A3, A4, A5>
{
public:
  /* Member variables */
  typedef R (*Function_T)(A0,A1,A2,A3,A4,A5);
  Function_T function;
  
  /* Constructor */
  NativeCFunctionCall_6_<R, A0, A1, A2, A3, A4, A5>(Function_T f, const Arguments &args) :
    AsyncCall_Returning<R>(args.GetIsolate(), args[6]),  /* callback */
    Call_6_<A0, A1, A2, A3, A4, A5>(args),
    function(f)
  { }

  /* Methods */
  void run() {
    AsyncCall_Returning<R>::return_val = (function)(
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
  * 8 arguments
  * return value of type R
**/

template <typename R, typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6, typename A7>
class NativeCFunctionCall_8_ : public AsyncCall_Returning<R>,
                               public Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>
{
public:
  /* Member variables */
  typedef R (*Function_T)(A0,A1,A2,A3,A4,A5,A6,A7);
  Function_T function;
  
  /* Constructor */
  NativeCFunctionCall_8_<R, A0, A1, A2, A3, A4, A5, A6, A7>(Function_T f, const Arguments &args) :
    AsyncCall_Returning<R>(args.GetIsolate(), args[8]),  /* callback */
    Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>(args),
    function(f)
  { }

  /* Methods */
  void run() {
    assert(function);
    AsyncCall_Returning<R>::return_val = (function)(
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

/*********************************************************************/
/*  Functions returning void */


/** Template class with no arguments
 * Wrapped native funtion call returning void
 *
**/
class NativeCVoidFunctionCall_0_ : public AsyncCall_Returning<int> {
public:
  /* Member variables */
  typedef void (*Function_T)();
  Function_T function;

  /* Constructor */
  NativeCVoidFunctionCall_0_(Function_T f, const Arguments &args) :
    AsyncCall_Returning<int>(args.GetIsolate(), args[1], 1) /*callback*/,
    function(f)
  { }

  /* Methods */
  void run() {
    function();
  }
};


/** Template class with:
 * one argument of type A0
 * Wrapped native funtion call returning void
 * The javascript return value is integer 0.
 *
**/

template <typename A0>
class NativeCVoidFunctionCall_1_ : public AsyncCall_Returning<int>,
                                   public Call_1_<A0>
{
public:
  /* Member variables */
  typedef void (*Function_T)(A0);
  Function_T function;

  /* Constructor */
  NativeCVoidFunctionCall_1_<A0>(Function_T f, const Arguments &args) :
    AsyncCall_Returning<int>(args.GetIsolate(), args[1], 1), // callback
    Call_1_<A0>(args),
    function(f)
  { }

  /* Methods */
  void run() {
    function(Call_1_<A0>::arg0);
  }
};
