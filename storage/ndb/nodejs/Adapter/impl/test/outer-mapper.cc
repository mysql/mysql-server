#include "v8_binder.h"

#include "c-api.h"

#include "JsConverter.h"
#include "js_wrapper_macros.h"

using namespace v8;


/* This tests a module  (outermapper)
   which has a link-time dependency on another module (mapper).
   The inner module must be loaded first.
*/


/*  C function wrapper 
*/
Handle<Value> doubleminus_wrapper(const Arguments& args) {
  HandleScope scope;
  
  REQUIRE_ARGS_LENGTH(1);
  
  JsValueConverter<unsigned int> arg0(args[0]);
    
  return scope.Close(toJS<int>(doubleminus(arg0.toC())));
}


/* Introduce C function in JavaScript namespace
*/
void outermapper_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "doubleminus", doubleminus_wrapper);
}




/*  FINAL STEP.
    This macro associates the module name with its initializer function 
*/
V8BINDER_LOADABLE_MODULE(outermapper, outermapper_initOnLoad)
