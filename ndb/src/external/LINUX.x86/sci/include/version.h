#ifndef _VERSION_H
#define _VERSION_H


/*
#define DEMO_VER_MAJOR   "1"
#define DEMO_VER_MINOR   "5"
#define DEMO_VER_MICRO   "0"
*/

#ifndef BUILD_DATE
#define BUILD_DATE      __DATE__
#endif

#ifndef BUILD_NAME   
#define BUILD_NAME     ""
#endif

/*
#define DEMO_VERSION "version " DEMO_VER_MAJOR "." DEMO_VER_MINOR "."DEMO_VER_MICRO " ("BUILD_NAME" "BUILD_DATE" )" 
*/
#define DEMO_VERSION " ("BUILD_NAME" "BUILD_DATE" )" 


#endif
