#ifndef _udf_h_
#define _udf_h_

// Client library users on Windows need this macro defined here.
//#include <my_global.h>
#include "mysql_com.h"

// Linkage and calling conventions.
#if defined (_WIN32) || defined (__WIN32__) || defined (WIN32)

#define DLLIMPORT        __declspec(dllimport)
#define DLLEXPORT        __declspec(dllexport)

#define DLLCALL          __stdcall

#else // !( _WIN32 || __WIN32__ || WIN32)

#define DLLIMPORT
#if defined(__GNUC__) && __GNUC__ > 3
#define DLLEXPORT __attribute__ ((visibility("default")))
#else
#define DLLEXPORT
#endif

#define DLLCALL

#endif // !( _WIN32 || __WIN32__ || WIN32)


extern "C" {

	bool value_at_init( UDF_INIT* initid, UDF_ARGS* args, char* message );
	void value_at_deinit( UDF_INIT* initid );
	void value_at_reset( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );
	void value_at_clear( UDF_INIT* initid, char* is_null, char *error );
	void value_at_add( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );
	double value_at( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );

	bool percentile_init( UDF_INIT* initid, UDF_ARGS* args, char* message );
	void percentile_deinit( UDF_INIT* initid );
	void percentile_reset( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );
	void percentile_clear( UDF_INIT* initid, char* is_null, char *error );
	void percentile_add( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );
	double percentile( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );

};

#endif //#define _udf_h_
