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
  R (*function)();    // function pointer

  /* Constructor */
  NativeCFunctionCall_0_<R>(const Arguments &args) :
  function(0),
  AsyncCall_Returning<R>(args[0]) // callback
  { }

  /* Methods */
  void run() {
    assert(function);
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
  R (*function)(A0);    // function pointer

  /* Constructor */
  NativeCFunctionCall_1_<R, A0>(const Arguments &args) :
    function(0),
    AsyncCall_Returning<R>(args[1]), /* callback */
    Call_1_<A0>(args)                                           { }

  /* Methods */
  void run() {
    assert(function);
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
  R (*function)(A0,A1);    // function pointer

  /* Constructor */
  NativeCFunctionCall_2_<R, A0, A1>(const Arguments &args) :
    function(0),
    AsyncCall_Returning<R>(args[2]), // callback
    Call_2_<A0, A1>(args)                                         { }

  /* Methods */
  void run() {
    assert(function);
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
  R (*function)(A0,A1,A2);    // function pointer

  /* Constructor */
  NativeCFunctionCall_3_<R, A0, A1, A2>(const Arguments &args) :
    function(0),
    AsyncCall_Returning<R>(args[3]), /* callback */
    Call_3_<A0, A1, A2>(args)                                     { }

  /* Methods */
  void run() {
    assert(function);
    AsyncCall_Returning<R>::return_val = (function)(
      Call_3_<A0, A1, A2>::arg0,
      Call_3_<A0, A1, A2>::arg1,
      Call_3_<A0, A1, A2>::arg2
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
  R (*function)(A0,A1,A2,A3,A4,A5);    // function pointer

  /* Constructor */
  NativeCFunctionCall_6_<R, A0, A1, A2, A3, A4, A5>(const Arguments &args) :
    function(0),
    AsyncCall_Returning<R>(args[8]),  /* callback */
    Call_6_<A0, A1, A2, A3, A4, A5>(args)                { }

  /* Methods */
  void run() {
    assert(function);
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
  R (*function)(A0,A1,A2,A3,A4,A5,A6,A7);    // function pointer

  /* Constructor */
  NativeCFunctionCall_8_<R, A0, A1, A2, A3, A4, A5, A6, A7>(const Arguments &args) :
    function(0),
    AsyncCall_Returning<R>(args[8]),  /* callback */
    Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>(args)                { }

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
  void (*function)();

  /* Constructor */
  NativeCVoidFunctionCall_0_(const Arguments &args) :
    function(0),
    AsyncCall_Returning<int>(args[1]) /*callback*/                   { }

  /* Methods */
  void run() {
    assert(function);
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
  void (*function)(A0);   // function pointer

  /* Constructor */
  NativeCVoidFunctionCall_1_<A0>(const Arguments &args) :
    function(0),
    AsyncCall_Returning<int>(args[1]), // callback
    Call_1_<A0>(args)                                             { }

  /* Methods */
  void run() {
    assert(function);
    function(Call_1_<A0>::arg0);
  }
};
