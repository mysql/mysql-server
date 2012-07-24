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

#include"NativeMethodCall.h"

using namespace v8;

/** These templated classes inherit from NativeMethodCall<R>, which encapsualtes
  * wrapper functions for async execution, and some return type.
  * 
  * The classes in this file wrap C function calls
  *
  */


/** Template class with:
  * no arguments
  * return value of type R
**/

template <typename R> 
class NativeCFunctionCall_0_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  R (*function)();    // function pointer
  
  /* Constructor */
  NativeCFunctionCall_0_<R>(const Arguments &args) : function(0) { }

  /* Methods */
  void run() {
    assert(function);
    NativeMethodCall<R>::return_val = (*function)();
  }
};


/** Template class with:
  * one argument of type A0
  * return value of type R
**/

template <typename R, typename A0> 
class NativeCFunctionCall_1_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  A0  arg0;
  R (*function)(A0);    // function pointer
  
  /* Constructor */
  NativeCFunctionCall_1_<R, A0>(const Arguments &args) : function(0)
  {    
    JsValueConverter<A0> arg0converter(args[0]);
    arg0 = arg0converter.toC();
  }

  /* Methods */
  void run() {
    assert(function);
    NativeMethodCall<R>::return_val = (function)(arg0);
  }
};


/** Template class with:
  * two arguments of type A0, A1
  * return value of type R
**/

template <typename R, typename A0, typename A1> 
class NativeCFunctionCall_2_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  A0  arg0;
  A1  arg1;
  R (*function)(A0,A1);    // function pointer
  
  /* Constructor */
  NativeCFunctionCall_2_<R, A0, A1>(const Arguments &args) : function(0)
  {    
    JsValueConverter<A0> arg0converter(args[0]);
    arg0 = arg0converter.toC();

    JsValueConverter<A1> arg1converter(args[1]);
    arg1 = arg1converter.toC();
  }

  /* Methods */
  void run() {
    assert(function);
    NativeMethodCall<R>::return_val = (function)(arg0, arg1);
  }
};


/** Template class with:
  * 8 arguments
  * return value of type R
**/

template <typename R, typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6, typename A7> 
class NativeCFunctionCall_8_ : public NativeMethodCall<R> {
public:
  /* Member variables */
  A0  arg0;
  A1  arg1;
  A2  arg2;
  A3  arg3;
  A4  arg4;
  A5  arg5;
  A6  arg6;
  A7  arg7;

  R (*function)(A0,A1,A2,A3,A4,A5,A6,A7);    // function pointer
  
  /* Constructor */
  NativeCFunctionCall_8_<R, A0, A1, A2, A3, A4, A5, A6, A7>
  (const Arguments &args) : function(0)
  {    
    JsValueConverter<A0> arg0converter(args[0]);
    arg0 = arg0converter.toC();

    JsValueConverter<A1> arg1converter(args[1]);
    arg1 = arg1converter.toC();

    JsValueConverter<A2> arg2converter(args[2]);
    arg2 = arg2converter.toC();

    JsValueConverter<A3> arg3converter(args[3]);
    arg3 = arg3converter.toC();

    JsValueConverter<A4> arg4converter(args[4]);
    arg4 = arg4converter.toC();

    JsValueConverter<A5> arg5converter(args[5]);
    arg5 = arg5converter.toC();

    JsValueConverter<A6> arg6converter(args[6]);
    arg6 = arg6converter.toC();

    JsValueConverter<A7> arg7converter(args[7]);
    arg7 = arg7converter.toC();
  }

  /* Methods */
  void run() {
    assert(function);
    NativeMethodCall<R>::return_val = 
      (function)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7);
  }
};


/*********************************************************************/
/*  Functions returning void */

/** Template class with:
 * one argument of type A0
 * Wrapped native funtion call returning void
 * The javascript return value is integer 0. 
 * 
**/

template <typename A0> 
class NativeCVoidFunctionCall_1_ : public NativeMethodCall<int> {
public:
  /* Member variables */
  A0  arg0;
  void (*function)(A0);   // function pointer
  
  /* Constructor */
  NativeCVoidFunctionCall_1_<A0>(const Arguments &args) : function(0) 
  {    
    JsValueConverter<A0> arg0converter(args[0]);
    arg0 = arg0converter.toC();
  }
  
  /* Methods */
  void run() {
    assert(function);
    function(arg0);
  }
};
