

/* This file simply redefines create_instance()
   and then includes the default engine in its entirety.
*/

#define create_instance create_my_default_instance
#include "default_engine.c"
