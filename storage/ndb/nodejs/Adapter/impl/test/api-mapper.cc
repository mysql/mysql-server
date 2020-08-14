/*
   Copyright (c) 2013, 2020, Oracle and/or its affiliates.

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

#include "v8_binder.h"

#include "c-api.h"
#include "cxx-api.hpp"

#include "JsConverter.h"
#include "js_wrapper_macros.h"

using namespace v8;

Envelope PointEnvelope("Point");
Envelope CircleEnvelope("Circle");

/*  C function wrapper 
*/
Handle<Value> whatnumber_wrapper(const Arguments& args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);
  
  JsValueConverter<int>           arg0(args[0]);
  JsValueConverter<const char *>  arg1(args[1]);
    
  return scope.Close(toJS<int>(whatnumber(arg0.toC(), arg1.toC())));
}


/* Introduce C function in JavaScript namespace
*/
void whatnumber_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "whatnumber", whatnumber_wrapper);
}


//   c++ class wrapper...


/* Implementation of JS "new Point"
*/
Handle<Value> Point_new_wrapper(const Arguments &args) {
  HandleScope scope;

  REQUIRE_CONSTRUCTOR_CALL();
  REQUIRE_ARGS_LENGTH(2);

  JsValueConverter<double>  arg0(args[0]);
  JsValueConverter<double>  arg1(args[1]);

  Point * p = new Point(arg0.toC(), arg1.toC());

  wrapPointerInObject(p, PointEnvelope, args.This());
  return args.This();
}


/* Point::quadrant() 
*/
Handle<Value> Point_quadrant_wrapper(const Arguments &args) {
  HandleScope scope;

  REQUIRE_ARGS_LENGTH(0);

  Point *p = unwrapPointer<Point *>(args.Holder());
  
  return scope.Close(toJS<int>(p->quadrant()));
}


/* Introduce Point in JavaScript namespace
*/
void Point_initOnLoad(Handle<Object> target) {
  Local<FunctionTemplate> JSPoint;

  DEFINE_JS_CLASS(JSPoint, "Point", Point_new_wrapper);
  DEFINE_JS_METHOD(JSPoint, "quadrant", Point_quadrant_wrapper);
  DEFINE_JS_CONSTRUCTOR(target, "Point", JSPoint);
}



/* Circle */

Handle<Value> Circle_new_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(2);
  
  JsValueConverter<Point *> arg0(args[0]);
  JsValueConverter<double>  arg1(args[1]);

  Circle * c = new Circle(* arg0.toC(), arg1.toC());

  wrapPointerInObject(c, CircleEnvelope, args.This());
  return args.This();
 }


Handle<Value> Circle_area_wrapper(const Arguments &args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(0);
  
  Circle *c = unwrapPointer<Circle *>(args.Holder());
  
  return scope.Close(toJS<double>(c->area()));
}


void Circle_initOnLoad(Handle<Object> target) {
  Local<FunctionTemplate> JSCircle;
  
  DEFINE_JS_CLASS(JSCircle, "Circle", Circle_new_wrapper);
  DEFINE_JS_METHOD(JSCircle, "area", Circle_area_wrapper);
  DEFINE_JS_CONSTRUCTOR(target, "Circle", JSCircle);
}



/* Initializer for the whole module
*/
void initAllOnLoad(Handle<Object> target) {
  Point_initOnLoad(target);
  Circle_initOnLoad(target);
  whatnumber_initOnLoad(target);
}


/*  FINAL STEP.
    This macro associates the module name with its initializer function 
*/
V8BINDER_LOADABLE_MODULE(mapper, initAllOnLoad)
