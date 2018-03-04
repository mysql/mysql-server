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


/* These classes build on the basic templates declared in AsyncMethodCall.h
 *
 * They wrap C++ Method calls
 *
 * We require one set of wrappers for "const" methods, and a separate set
 * for non-const methods.
 */


template <typename C> 
class NativeDestructorCall : public NativeVoidMethodCall<C> {
public:
  /* Constructor */
  NativeDestructorCall<C>(const Arguments &args) :
    NativeVoidMethodCall<C>(args, 0)
  { }

  /* Method */
  void run() {
    DEBUG_PRINT_DETAIL("NativeDestructorCall: Async destructor %p", NativeVoidMethodCall<C>::native_obj);
    delete NativeVoidMethodCall<C>::native_obj;
  }
};


/** Template class with:
 * wrapped class C
 * no arguments & void return
 */
template <typename C> 
class NativeVoidMethodCall_0_ : public NativeVoidMethodCall<C> {
public:
  /* Member variables */
  typedef void (C::*Method_T)(void);
  Method_T method;
  
  /* Constructors */
   NativeVoidMethodCall_0_<C>(Method_T m, const Arguments &args) :
    NativeVoidMethodCall<C>(args, 0),
    method(m)
  {  } 
  
  /* Methods */
  void run() {
    ((NativeVoidMethodCall<C>::native_obj)->*(method))();
  }
};


/** Template class with:
 * wrapped class C
 * no arguments
 * return value of type R
 */
template <typename R, typename C>
class NativeMethodCall_0_ : public NativeMethodCall<R,C> {
public:
  /* Member variables */
  typedef R (C::*Method_T)(void);
  Method_T method;

  /* Constructors */
  NativeMethodCall_0_<R, C>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 0),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
      ((NativeMethodCall<R,C>::native_obj)->*(method))();
  }
};


/** Template class with:
 * wrapped class C
 * one argument of type A0
 * return value of type R
 */
template <typename R, typename C, typename A0>
class NativeMethodCall_1_ : public NativeMethodCall<R,C>,
                            public Call_1_<A0>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0);
  Method_T method;

  /* Constructors */
  NativeMethodCall_1_<R, C, A0>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 1),
    Call_1_<A0>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
      ((NativeMethodCall<R,C>::native_obj)->*(method))(Call_1_<A0>::arg0);
  }
};


/** Template class with:
 * wrapped class C
 * one argument of type A0
 * Wrapped native method call returning void
 * The javascript return value is integer 0.
 */
template <typename C, typename A0>
class NativeVoidMethodCall_1_ : public NativeVoidMethodCall<C> ,
                                public Call_1_<A0>
{
public:
  /* Member variables */
  typedef void (C::*Method_T)(A0);
  Method_T method;

  /* Constructor */
  NativeVoidMethodCall_1_<C, A0>(Method_T m, const Arguments &args) :
    NativeVoidMethodCall<C>(args, 1),
    Call_1_<A0>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    ((NativeVoidMethodCall<C>::native_obj)->*(method))(Call_1_<A0>::arg0);
  }
};


/** Template class with:
 * wrapped class C
 * two arguments of type A0 and A1
 * return type R
 */
template <typename R, typename C, typename A0, typename A1>
class NativeMethodCall_2_ : public NativeMethodCall<R,C>,
                            public Call_2_<A0, A1>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0, A1);  // "method" is pointer to member function
  Method_T method;

  /* Constructor */
  NativeMethodCall_2_<R, C, A0, A1>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 2),
    Call_2_<A0, A1>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
      ((NativeMethodCall<R,C>::native_obj)->*(method))(
        Call_2_<A0, A1>::arg0,
        Call_2_<A0, A1>::arg1);
  }
};


/** Template class with
 *  Method returning void; 2 arguments
 */
template <typename C, typename A0, typename A1>
class NativeVoidMethodCall_2_ : public NativeVoidMethodCall<C> ,
                                public Call_2_<A0, A1>
{
public:
  /* Member variables */
  typedef void (C::*Method_T)(A0, A1);
  Method_T method;

  /* Constructor */
  NativeVoidMethodCall_2_<C, A0, A1>(Method_T m, const Arguments &args) :
    NativeVoidMethodCall<C>(args, 2),
    Call_2_<A0, A1>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    ((NativeVoidMethodCall<C>::native_obj)->*(method))
      (Call_2_<A0,A1>::arg0, Call_2_<A0,A1>::arg1);
  }
};


/** Template class with:
 * wrapped class C
 * three arguments of type A0, A1, and A2
 * return type R
 */
template <typename R, typename C, typename A0, typename A1, typename A2>
class NativeMethodCall_3_ : public NativeMethodCall <R,C>,
                            public Call_3_<A0, A1, A2>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0, A1, A2); 
  Method_T method;

  /* Constructor */
  NativeMethodCall_3_<R, C, A0, A1, A2>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 3),
    Call_3_<A0, A1, A2>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
    ((NativeMethodCall<R,C>::native_obj)->*(method))(
      Call_3_<A0, A1, A2>::arg0,
      Call_3_<A0, A1, A2>::arg1,
      Call_3_<A0, A1, A2>::arg2
    );
  }
};


/** Template class with:
 * wrapped class C
 * three arguments of type A0, A1, and A2
 * void return
 */
template <typename C, typename A0, typename A1, typename A2>
class NativeVoidMethodCall_3_ : public NativeVoidMethodCall<C>,
                                public Call_3_<A0, A1, A2>
{
public:
  /* Member variables */
  typedef void (C::*Method_T)(A0, A1, A2);
  Method_T method;
  
  /* Constructor */
  NativeVoidMethodCall_3_<C, A0, A1, A2>(Method_T m, const Arguments &args) :
    NativeVoidMethodCall<C>(args, 3),
    Call_3_<A0, A1, A2>(args),
    method(m)
  { }
  
  /* Methods */
  void run() {
    ((NativeVoidMethodCall<C>::native_obj)->*(method))(
      Call_3_<A0, A1, A2>::arg0,
      Call_3_<A0, A1, A2>::arg1,
      Call_3_<A0, A1, A2>::arg2
    );
  }
};


/** Template class with:
 * 4 arguments
 * return value of type R
 */
template <typename R, typename C,
          typename A0, typename A1, typename A2, typename A3>
class NativeMethodCall_4_ : public NativeMethodCall<R,C> ,
                            public Call_4_<A0, A1, A2, A3>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0,A1,A2,A3);
  Method_T method;

  /* Constructor */
  NativeMethodCall_4_<R, C, A0, A1, A2, A3>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 4),
    Call_4_<A0, A1, A2, A3>(args),
    method(m)
 {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
     ((NativeMethodCall<R,C>::native_obj)->*(method))(
      Call_4_<A0, A1, A2, A3>::arg0,
      Call_4_<A0, A1, A2, A3>::arg1,
      Call_4_<A0, A1, A2, A3>::arg2,
      Call_4_<A0, A1, A2, A3>::arg3
    );
  }
};


/** Template class with:
 * wrapped class C
 * 4 arguments of type A0, A1, A2, A3
 * void return
 */
template <typename C, typename A0, typename A1, typename A2, typename A3>
class NativeVoidMethodCall_4_ : public NativeVoidMethodCall<C>,
public Call_4_<A0, A1, A2, A3>
{
public:
  /* Member variables */
  typedef void (C::*Method_T)(A0, A1, A2, A3);
  Method_T method;
  
  /* Constructor */
  NativeVoidMethodCall_4_<C, A0, A1, A2, A3>(Method_T m, const Arguments &args) :
    NativeVoidMethodCall<C>(args, 4),
    Call_4_<A0, A1, A2, A3>(args),
    method(m)
  { }
  
  /* Methods */
  void run() {
    ((NativeVoidMethodCall<C>::native_obj)->*(method))(
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
 */
template <typename R, typename C,
          typename A0, typename A1, typename A2, typename A3, typename A4>
class NativeMethodCall_5_ : public NativeMethodCall<R,C>,
                            public Call_5_<A0, A1, A2, A3, A4>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0,A1,A2,A3,A4);
  Method_T method;
  
  /* Constructor */
  NativeMethodCall_5_<R, C, A0, A1, A2, A3, A4>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 5),
    Call_5_<A0, A1, A2, A3, A4>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
     ((NativeMethodCall<R,C>::native_obj)->*(method))(
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
 */
template <typename R, typename C,
          typename A0, typename A1, typename A2,
          typename A3, typename A4, typename A5>
class NativeMethodCall_6_ : public NativeMethodCall<R,C> ,
                            public Call_6_<A0, A1, A2, A3, A4, A5>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0,A1,A2,A3,A4,A5);
  Method_T method;
  
  /* Constructor */
  NativeMethodCall_6_<R, C, A0, A1, A2, A3, A4, A5>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 6),
    Call_6_<A0, A1, A2, A3, A4, A5>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
     ((NativeMethodCall<R,C>::native_obj)->*(method))(
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
 */
template <typename R, typename C,
          typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6>
class NativeMethodCall_7_ : public NativeMethodCall<R,C>,
                            public Call_7_<A0, A1, A2, A3, A4, A5, A6>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0,A1,A2,A3,A4,A5,A6);
  Method_T method;
  
  /* Constructor */
  NativeMethodCall_7_<R, C, A0, A1, A2, A3, A4, A5, A6>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 7),
    Call_7_<A0, A1, A2, A3, A4, A5, A6>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
     ((NativeMethodCall<R,C>::native_obj)->*(method))(
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
 */
template <typename R, typename C,
          typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6, typename A7>
class NativeMethodCall_8_ : public NativeMethodCall<R,C>,
                            public Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0,A1,A2,A3,A4,A5,A6,A7);
  Method_T method;

  /* Constructor */
  NativeMethodCall_8_<R, C, A0, A1, A2, A3, A4, A5, A6, A7>(Method_T m, const Arguments &args) :
    NativeMethodCall<C, R>(args, 8),
    Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
     ((NativeMethodCall<R,C>::native_obj)->*(method))(
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


/* Const versions */

/** Template class with
 *  Return type R; no arguments
 */
template <typename R, typename C>
class NativeConstMethodCall_0_ : public NativeMethodCall<R,C> {
public:
  /* Member variables */
  typedef R (C::*Method_T)(void) const;
  Method_T method;
  
  /* Constructors */
  NativeConstMethodCall_0_<R, C>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 0),
    method(m)
  {  }
  
  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
      ((NativeMethodCall<R,C>::native_obj)->*(method))();
  }
};


/** Template class with:
 * return value of type R
 * one argument of type A0
 */
template <typename R, typename C, typename A0>
class NativeConstMethodCall_1_ : public NativeMethodCall<R,C>,
                                 public Call_1_<A0>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0) const;
  Method_T method;

  /* Constructors */
  NativeConstMethodCall_1_<R, C, A0>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 1),
    Call_1_<A0>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
      ((NativeMethodCall<R,C>::native_obj)->*(method))(Call_1_<A0>::arg0);
  }
};


/** Template class with
 *  Method returning void; 2 arguments
 */
template <typename C, typename A0, typename A1>
class NativeVoidConstMethodCall_2_ : public NativeVoidMethodCall<C> ,
                                     public Call_2_<A0, A1>
{
public:
  /* Member variables */
  typedef void (C::*Method_T)(A0, A1) const;
  Method_T method;

  /* Constructor */
  NativeVoidConstMethodCall_2_<C, A0, A1>(Method_T m, const Arguments &args) :
    NativeVoidMethodCall<C>(args, 2),
    Call_2_<A0, A1>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    ((NativeVoidMethodCall<C>::native_obj)->*(method))
      (Call_2_<A0,A1>::arg0, Call_2_<A0,A1>::arg1);
  }
};

/** Template class with
 *  Method returning R; 2 arguments 
 */
template <typename R, typename C, typename A0, typename A1>
class NativeConstMethodCall_2_ : public NativeMethodCall<R,C>,
                                 public Call_2_<A0, A1>
{
public:
  /* Member variables */
  typedef R (C::*Method_T)(A0, A1) const;
  Method_T method;

  /* Constructor */
  NativeConstMethodCall_2_<R, C, A0, A1>(Method_T m, const Arguments &args) :
    NativeMethodCall<R, C>(args, 2),
    Call_2_<A0, A1>(args),
    method(m)
  {  }

  /* Methods */
  void run() {
    NativeMethodCall<R,C>::return_val =
      ((NativeMethodCall<R,C>::native_obj)->*(method))(
        Call_2_<A0, A1>::arg0,
        Call_2_<A0, A1>::arg1);
  }
};
