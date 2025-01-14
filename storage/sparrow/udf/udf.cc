/*
**  file of UDF (user definable functions) that are dynamicly loaded
** into the standard mysqld core.
**
** The functions name, type and shared library is saved in the new system
** table 'func'.  To be able to create new functions one must have write
** privilege for the database 'mysql'.	If one starts MySQL with
** --skip-grant, then UDF initialization will also be skipped.
**
** Syntax for the new commands are:
** create function <function_name> returns {string|real|integer}
**		  soname <name_of_shared_library>
** drop function <function_name>
**
** Each defined function may have a xxxx_init function and a xxxx_deinit
** function.  The init function should alloc memory for the function
** and tell the main function about the max length of the result
** (for string functions), number of decimals (for double functions) and
** if the result may be a null value.
**
** If a function sets the 'error' argument to 1 the function will not be
** called anymore and mysqld will return NULL for all calls to this copy
** of the function.
**
** All strings arguments to functions are given as string pointer + length
** to allow handling of binary data.
** Remember that all functions must be thread safe. This means that one is not
** allowed to alloc any global or static variables that changes!
** If one needs memory one should alloc this in the init function and free
** this on the __deinit function.
**
** Note that the init and __deinit functions are only called once per
** SQL statement while the value function may be called many times
**
** Function 'value_at' returns an indicator value corresponding to the maximum value of the given determiner.

** Function 'maxt' returns a timestamp corresponding to the maximum value of the given determiner.
**
** A dynamically loadable file should be compiled shared.
** (something like: gcc -shared -o my_func.so myfunc.cc).
** You can easily get all switches right by doing:
** cd sql ; make udf_example.o
** Take the compile line that make writes, remove the '-c' near the end of
** the line and add -shared -o udf_example.so to the end of the compile line.
** The resulting library (udf_example.so) should be copied to some dir
** searched by ld. (/usr/lib ?)
** If you are using gcc, then you should be able to create the udf_example.so
** by simply doing 'make udf_example.so'.
**
** After the library is made one must notify mysqld about the new
** functions with the commands:
**
** CREATE AGGREGATE FUNCTION value_at RETURNS REAL SONAME "udf_example.so";
** CREATE AGGREGATE FUNCTION maxt RETURNS STRING SONAME "udf_example.so";
**
** After this the functions will work exactly like native MySQL functions.
** Functions should be created only once.
**
** The functions can be deleted by:
**
** DROP FUNCTION value_at;
** DROP FUNCTION maxt;
**
** The CREATE FUNCTION and DROP FUNCTION update the func@mysql table. All
** Active function will be reloaded on every restart of server
** (if --skip-grant-tables is not given)
**
** If you ge problems with undefined symbols when loading the shared
** library, you should verify that mysqld is compiled with the -rdynamic
** option.
**
** If you can't get AGGREGATES to work, check that you have the column
** 'type' in the mysql.func table.  If not, run 'mysql_upgrade'.
**
*/

#include "udf.h"

#include <new>
#include <algorithm>
#include <vector>

#include <m_string.h>

/*#if defined(MYSQL_SERVER)
#include <m_string.h>
#else
// when compiled as standalone
#include <string.h>
#define strmov(a,b) stpcpy(a,b)
#endif*/

#include "operator.h"

#ifdef HAVE_DLOPEN

// Method to convert a sql timestamp YYYY-MM-DD HH:MM:SS to YYYYMMDDHHMMSS
longlong timestampToLongLong(const char* timestamp);
// Methods to parse a sql timestamp YYYYMMDDHHMMSS
longlong getYear(long long sqltimestamp);
longlong getMonth(long long sqltimestamp);
longlong getDay(long long sqltimestamp);
longlong getHour(long long sqltimestamp);
longlong getMinute(long long sqltimestamp);
longlong getSecond(long long sqltimestamp);
longlong getDate(long long sqltimestamp);
longlong getTime(long long sqltimestamp);

typedef OperatorData<UdfArgumentReal,UdfArgumentReal> ValueAtData;
typedef OperatorData<UdfArgumentReal,UdfArgumentInt> MaxtData;

/*
** Syntax for the new aggregate commands are:
** create aggregate function <function_name> returns {string|real|integer}
**		  soname <name_of_shared_library>
**
** Syntax for percentile: percentile( t.indicator, t.determiner )
**	with t.indicator=double, t.determiner=double
*/

class PercentileData {
public:
	PercentileData() : empty_(true), percent_(0.0) {;}

	void clear() { data_.clear(); empty_=true; percent_=0.0; }
	void add(UDF_ARGS* args, char* error);
	double compute(bool& isNull, char* error);

public:
	bool		empty_;
	double		percent_;
	std::vector<double>		data_;
};

void PercentileData::add(UDF_ARGS* args, char* error) {
	// Test arguments
	if (args == NULL) { 
		*error = 1; return; 
	}

	if (args->args[1] == NULL) {
		*error = 1; return; 
	}

	UdfArgumentReal		arg(args->args[1],args->lengths[1]);
	if (arg.isNull()) {
		*error = 1; return; 
	}

	double	value = arg.getValue();
	if (value < 0.0 || value > 100.0) {
		*error = 1; return; 
	}

	if ( empty_ ) {
		percent_ = value;
		empty_ = false;
	} else if (percent_ != value) {
		*error = 1; return; 
	}

	if (args->args[0] != NULL) {
		UdfArgumentReal		arg(args->args[0],args->lengths[0]);
		if (!arg.isNull()) {
			double	value = arg.getValue();
			data_.push_back(value);
		}
	}
}

double PercentileData::compute(bool& isNull, char* error) {
	if (empty_ || data_.empty()) {
		*error = 1; 
		isNull = true;
		return 0;
	}
	isNull = false;
	int		position = static_cast<int>((data_.size()-1)*percent_/100);
	std::nth_element(data_.begin(), data_.begin()+position, data_.end());
	double percentile=data_[position];
	return percentile;
}


// percentile Aggregate Function.

// Allocate memory and initialize parameters
bool
percentile_init( UDF_INIT* initid, UDF_ARGS* args, char* message )
{
  PercentileData*	data = NULL;

  // Test argument count
  if (args->arg_count != 2)
  {
    strcpy( message, "wrong number of arguments: percentile() requires two arguments" );
    return 1;
  }

  // Allocate working structure
  data = new (std::nothrow)PercentileData;
  if (data == NULL)
  {
    strcpy(message,"Couldn't allocate memory");
    return 1;
  }

  // Force type of arguments
  args->arg_type[0]	= REAL_RESULT;
  args->arg_type[1]	= REAL_RESULT;

  initid->maybe_null	= 1;	// The result may be null
  initid->decimals		= 4;		// We want 4 decimals in the result
  initid->max_length	= 20;	// 6 digits + . + 10 decimals

  // Initialize flag
  data->clear();

  // Store working structure
  initid->ptr = (char*)data;

  return 0;
}

// Deallocate memory
void
percentile_deinit( UDF_INIT* initid )
{
  // Deallocate working structure
  void *void_ptr= initid->ptr;
  PercentileData *data = static_cast<PercentileData*>(void_ptr);
  delete data;
}

// This is needed to get things to work in MySQL 4.1.1 and above
void
percentile_clear(UDF_INIT* initid, [[maybe_unused]] char* is_null,
				 [[maybe_unused]] char* error)
{
	PercentileData *data = (PercentileData*)(initid->ptr);

	// Initialize flag
	data->clear();
}

// Treats a new row
void
percentile_add(UDF_INIT* initid, UDF_ARGS* args,
				 [[maybe_unused]] char* is_null,
				 [[maybe_unused]] char* error)
{
	PercentileData *data = (PercentileData*)(initid->ptr);

	data->add(args, error);
}

// This is only for MySQL 4.0 compatibility
void
percentile_reset(UDF_INIT* initid, UDF_ARGS* args, char* is_null, char* error)
{
	percentile_clear(initid, is_null, error);
	percentile_add(initid, args, is_null, error);
}

// Return the indicator value, if any
double
percentile(UDF_INIT* initid,  [[maybe_unused]] UDF_ARGS* args,
			char* is_null,  [[maybe_unused]] char* error)
{
	PercentileData *data = (PercentileData*)(initid->ptr);
	bool	isNull = false;
	double	result = data->compute(isNull, error);
	// If nothing happened, return null
	if (isNull)	{
		*is_null = 1;
		return 0.0;
	}
	*is_null = 0;
	return result;
}



/*
** Syntax for the new aggregate commands are:
** create aggregate function <function_name> returns {string|real|integer}
**		  soname <name_of_shared_library>
**
** Syntax for value_at: value_at( t.indicator, t.determiner )
**	with t.indicator=double, t.determiner=double
*/

// Value at Aggregate Function.

// Allocate memory and initialize parameters
bool
value_at_init( UDF_INIT* initid, UDF_ARGS* args, char* message )
{
  ValueAtData*	data = NULL;

  // Test argument count
  if (args->arg_count != 2)
  {
    strcpy(message,"wrong number of arguments: value_at() requires two arguments");
    return 1;
  }

  // Don't test input type
  /*
  if ((args->arg_type[0] != REAL_RESULT) || (args->arg_type[1] != REAL_RESULT) )
  {
    strcpy(
	   message,
	   "wrong argument type: value_at() requires an REAL and a REAL"
	   );
    return 1;
  }*/

  // Allocate working structure
  data = new (std::nothrow) ValueAtData;
  if (data == NULL)
  {
    strcpy(message,"Couldn't allocate memory");
    return 1;
  }

  // Force type of arguments
  args->arg_type[0]	= data->getIndicator().getType();
  args->arg_type[1]	= data->getDeterminer().getType();

  initid->maybe_null	= 1;	// The result may be null
  initid->decimals	= 4;		// We want 4 decimals in the result
  initid->max_length	= 20;	// 6 digits + . + 10 decimals

  // Initialize flag
  data->empty();

  // Store working structure
  initid->ptr = (char*)data;

  return 0;
}

// Deallocate memory
void
value_at_deinit( UDF_INIT* initid )
{
  // Deallocate working structure
  void *void_ptr= initid->ptr;
  ValueAtData *data = static_cast<ValueAtData*>(void_ptr);
  delete data;
}

// This is needed to get things to work in MySQL 4.1.1 and above
void
value_at_clear(UDF_INIT* initid, [[maybe_unused]] char* is_null,
               [[maybe_unused]] char* message)
{
  ValueAtData *data = (ValueAtData*) initid->ptr;

  // Initialize flag
  data->empty();
}

// Treats a new row
void
value_at_add(UDF_INIT* initid, UDF_ARGS* args,
            [[maybe_unused]] char* is_null,
            [[maybe_unused]] char* message)
{
  ValueAtData *data = (ValueAtData*) initid->ptr;
  data->updateIfGreater(args);
}

// This is only for MySQL 4.0 compability
void
value_at_reset(UDF_INIT* initid, UDF_ARGS* args, char* is_null, char* message)
{
  value_at_clear(initid, is_null, message);
  value_at_add(initid, args, is_null, message);
}

// Return the indicator value, if any
double
value_at( UDF_INIT* initid, [[maybe_unused]] UDF_ARGS* args,
         char* is_null, [[maybe_unused]] char* error)
{
  ValueAtData *data = (ValueAtData*) initid->ptr;

  // If nothing happened, return null
  if (data->isEmpty())
  {
    *is_null = 1;
    return 0.0;
  }

#ifdef LOG_OPERATOR
  data->logContent();
#endif //#ifdef LOG_OPERATOR

  // Return the indicator value
  if (data->getIndicator().isNull())
  {
    *is_null = 1;
    return 0.0;
  }
  else
  {
	  *is_null = 0;
	  return data->getIndicator().getValue();
  }
}


/*
** Syntax for the new aggregate commands are:
** create aggregate function <function_name> returns {string|real|integer}
**		  soname <name_of_shared_library>
**
** Syntax for maxt: value_at( t.indicator, t.determiner )
**	with t.indicator=longlong, t.determiner=double
*/


// maxt Aggregate Function.

// Allocate memory and initialize parameters
bool
maxt_init( UDF_INIT* initid, UDF_ARGS* args, char* message )
{
  MaxtData*	data = NULL;

  // Test argument count
  if (args->arg_count != 2)
  {
    strcpy(message,"wrong number of arguments: maxt() requires two arguments");
    return 1;
  }

  // Don't test input type
  /*
  if ((args->arg_type[0] != REAL_RESULT) || (args->arg_type[1] != REAL_RESULT) )
  {
    strcpy(
	   message,
	   "wrong argument type: maxt() requires an REAL and a REAL"
	   );
    return 1;
  }*/

  // Allocate working structure
  data = new (std::nothrow) MaxtData;
  if (data == NULL)
  {
    strcpy(message,"Couldn't allocate memory");
    return 1;
  }

  // Force type of arguments
  args->arg_type[0]	= data->getIndicator().getType();
  args->arg_type[1]	= data->getDeterminer().getType();

  initid->maybe_null	= 1;	// The result may be null */
  initid->decimals	= 4;		// We want 4 decimals in the result */
  initid->max_length	= 20;	// 6 digits + . + 10 decimals

  // Initialize flag
  data->empty();

  // Store working structure
  initid->ptr = (char*)data;

  return 0;
}

// Deallocate memory
void
maxt_deinit( UDF_INIT* initid )
{
  // Deallocate working structure
  void *void_ptr = initid->ptr;
  MaxtData *data = static_cast<MaxtData*>(void_ptr);
  delete data;
}

// This is needed to get things to work in MySQL 4.1.1 and above
void
maxt_clear(UDF_INIT* initid, [[maybe_unused]] char* is_null,
              [[maybe_unused]] char* message)
{
  MaxtData *data = (MaxtData*) initid->ptr;

  // Initialize flag
  data->empty();
}

// Treats a new row
void
maxt_add(UDF_INIT* initid, UDF_ARGS* args,
            [[maybe_unused]] char* is_null,
            [[maybe_unused]] char* message)
{
  MaxtData *data = (MaxtData*) initid->ptr;
  data->updateIfGreater(args);
}

// This is only for MySQL 4.0 compability
void
maxt_reset(UDF_INIT* initid, UDF_ARGS* args, char* is_null, char* message)
{
  maxt_clear(initid, is_null, message);
  maxt_add(initid, args, is_null, message);
}

// Return the indicator value, if any
longlong
maxt( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error )
{
  MaxtData *data = (MaxtData*) initid->ptr;

  // If nothing happened, return null
  if (data->isEmpty())
  {
    *is_null = 1;
    return 0;
  }

#ifdef LOG_OPERATOR
  data->logContent();
#endif //#ifdef LOG_OPERATOR

  // Return the indicator value
  if (data->getIndicator().isNull())
  {
    *is_null = 1;
    return 0;
  }
  else
  {
	  *is_null = 0;
	  return data->getIndicator().getValue();
  }
}

// Method to convert a sql timestamp YYYY-MM-DD HH:MM:SS to YYYYMMDDHHMMSS
longlong timestampToLongLong(const char* timestamp)
{
	long year,month,day,hour,minute,second;
	sscanf(timestamp,"%ld-%ld-%ld %ld:%ld:%ld",&year,&month,&day,&hour,&minute,&second);
	return
		year * 10000000000L + \
		month * 100000000L + \
		day * 1000000L + \
		hour * 10000L + \
		minute * 100L + \
		second \
		;
}

// Methods to parse a sql timestamp YYYYMMDDHHMMSS
longlong getYear(long long sqltimestamp)
{
  return (sqltimestamp / 10000000000L);
}
longlong getMonth(long long sqltimestamp)
{
  return ((sqltimestamp % 10000000000L) / 100000000L);
}
longlong getDay(long long sqltimestamp)
{
  return ((sqltimestamp % 100000000L) / 1000000L);
}
longlong getHour(long long sqltimestamp)
{
  return ((sqltimestamp % 1000000L) / 10000L);
}
longlong getMinute(long long sqltimestamp)
{
  return ((sqltimestamp % 10000L) / 100L);
}
longlong getSecond(long long sqltimestamp)
{
  return (sqltimestamp % 100L);
}
longlong getDate(long long sqltimestamp)
{
	return (sqltimestamp / 1000000L);
}
longlong getTime(long long sqltimestamp)
{
  return (sqltimestamp % 1000000L);
}

#endif /* HAVE_DLOPEN */
