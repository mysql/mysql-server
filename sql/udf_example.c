/* Copyright (C) 2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
** example file of UDF (user definable functions) that are dynamicly loaded
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
** Function 'metaphon' returns a metaphon string of the string argument.
** This is something like a soundex string, but it's more tuned for English.
**
** Function 'myfunc_double' returns summary of codes of all letters
** of arguments divided by summary length of all its arguments.
**
** Function 'myfunc_int' returns summary length of all its arguments.
**
** Function 'sequence' returns an sequence starting from a certain number.
**
** Function 'myfunc_argument_name' returns name of argument.
**
** On the end is a couple of functions that converts hostnames to ip and
** vice versa.
**
** A dynamicly loadable file should be compiled shared.
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
** CREATE FUNCTION metaphon RETURNS STRING SONAME "udf_example.so";
** CREATE FUNCTION myfunc_double RETURNS REAL SONAME "udf_example.so";
** CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "udf_example.so";
** CREATE FUNCTION sequence RETURNS INTEGER SONAME "udf_example.so";
** CREATE FUNCTION lookup RETURNS STRING SONAME "udf_example.so";
** CREATE FUNCTION reverse_lookup RETURNS STRING SONAME "udf_example.so";
** CREATE AGGREGATE FUNCTION avgcost RETURNS REAL SONAME "udf_example.so";
** CREATE FUNCTION myfunc_argument_name RETURNS STRING SONAME "udf_example.so";
**
** After this the functions will work exactly like native MySQL functions.
** Functions should be created only once.
**
** The functions can be deleted by:
**
** DROP FUNCTION metaphon;
** DROP FUNCTION myfunc_double;
** DROP FUNCTION myfunc_int;
** DROP FUNCTION lookup;
** DROP FUNCTION reverse_lookup;
** DROP FUNCTION avgcost;
** DROP FUNCTION myfunc_argument_name;
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
** 'type' in the mysql.func table.  If not, run 'mysql_fix_privilege_tables'.
**
*/

#ifdef STANDARD
/* STANDARD is defined, don't use any mysql functions */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __WIN__
typedef unsigned __int64 ulonglong;	/* Microsofts 64 bit types */
typedef __int64 longlong;
#else
typedef unsigned long long ulonglong;
typedef long long longlong;
#endif /*__WIN__*/
#else
#include <my_global.h>
#include <my_sys.h>
#if defined(MYSQL_SERVER)
#include <m_string.h>		/* To get strmov() */
#else
/* when compiled as standalone */
#include <string.h>
#define strmov(a,b) stpcpy(a,b)
#define bzero(a,b) memset(a,0,b)
#define memcpy_fixed(a,b,c) memcpy(a,b,c)
#endif
#endif
#include <mysql.h>
#include <ctype.h>

static pthread_mutex_t LOCK_hostname;

#ifdef HAVE_DLOPEN

/* These must be right or mysqld will not find the symbol! */

my_bool metaphon_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void metaphon_deinit(UDF_INIT *initid);
char *metaphon(UDF_INIT *initid, UDF_ARGS *args, char *result,
	       unsigned long *length, char *is_null, char *error);
my_bool myfunc_double_init(UDF_INIT *, UDF_ARGS *args, char *message);
double myfunc_double(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
		     char *error);
my_bool myfunc_int_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
longlong myfunc_int(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
		    char *error);
my_bool sequence_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
 void sequence_deinit(UDF_INIT *initid);
longlong sequence(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
		   char *error);
my_bool avgcost_init( UDF_INIT* initid, UDF_ARGS* args, char* message );
void avgcost_deinit( UDF_INIT* initid );
void avgcost_reset( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );
void avgcost_clear( UDF_INIT* initid, char* is_null, char *error );
void avgcost_add( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );
double avgcost( UDF_INIT* initid, UDF_ARGS* args, char* is_null, char *error );
my_bool is_const_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *is_const(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long
               *length, char *is_null, char *error);


/*************************************************************************
** Example of init function
** Arguments:
** initid	Points to a structure that the init function should fill.
**		This argument is given to all other functions.
**	my_bool maybe_null	1 if function can return NULL
**				Default value is 1 if any of the arguments
**				is declared maybe_null.
**	unsigned int decimals	Number of decimals.
**				Default value is max decimals in any of the
**				arguments.
**	unsigned int max_length  Length of string result.
**				The default value for integer functions is 21
**				The default value for real functions is 13+
**				default number of decimals.
**				The default value for string functions is
**				the longest string argument.
**	char *ptr;		A pointer that the function can use.
**
** args		Points to a structure which contains:
**	unsigned int arg_count		Number of arguments
**	enum Item_result *arg_type	Types for each argument.
**					Types are STRING_RESULT, REAL_RESULT
**					and INT_RESULT.
**	char **args			Pointer to constant arguments.
**					Contains 0 for not constant argument.
**	unsigned long *lengths;		max string length for each argument
**	char *maybe_null		Information of which arguments
**					may be NULL
**
** message	Error message that should be passed to the user on fail.
**		The message buffer is MYSQL_ERRMSG_SIZE big, but one should
**		try to keep the error message less than 80 bytes long!
**
** This function should return 1 if something goes wrong. In this case
** message should contain something usefull!
**************************************************************************/

#define MAXMETAPH 8

my_bool metaphon_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
  {
    strcpy(message,"Wrong arguments to metaphon;  Use the source");
    return 1;
  }
  initid->max_length=MAXMETAPH;
  return 0;
}

/****************************************************************************
** Deinit function. This should free all resources allocated by
** this function.
** Arguments:
** initid	Return value from xxxx_init
****************************************************************************/


void metaphon_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

/***************************************************************************
** UDF string function.
** Arguments:
** initid	Structure filled by xxx_init
** args		The same structure as to xxx_init. This structure
**		contains values for all parameters.
**		Note that the functions MUST check and convert all
**		to the type it wants!  Null values are represented by
**		a NULL pointer
** result	Possible buffer to save result. At least 255 byte long.
** length	Pointer to length of the above buffer.	In this the function
**		should save the result length
** is_null	If the result is null, one should store 1 here.
** error	If something goes fatally wrong one should store 1 here.
**
** This function should return a pointer to the result string.
** Normally this is 'result' but may also be an alloced string.
***************************************************************************/

/* Character coding array */
static char codes[26] =  {
    1,16,4,16,9,2,4,16,9,2,0,2,2,2,1,4,0,2,4,4,1,0,0,0,8,0
 /* A  B C  D E F G  H I J K L M N O P Q R S T U V W X Y Z*/
    };

/*--- Macros to access character coding array -------------*/

#define ISVOWEL(x)  (codes[(x) - 'A'] & 1)	/* AEIOU */

    /* Following letters are not changed */
#define NOCHANGE(x) (codes[(x) - 'A'] & 2)	/* FJLMNR */

    /* These form diphthongs when preceding H */
#define AFFECTH(x) (codes[(x) - 'A'] & 4)	/* CGPST */

    /* These make C and G soft */
#define MAKESOFT(x) (codes[(x) - 'A'] & 8)	/* EIY */

    /* These prevent GH from becoming F */
#define NOGHTOF(x)  (codes[(x) - 'A'] & 16)	/* BDH */


char *metaphon(UDF_INIT *initid __attribute__((unused)),
               UDF_ARGS *args, char *result, unsigned long *length,
               char *is_null, char *error __attribute__((unused)))
{
  const char *word=args->args[0];
  const char *w_end;
  char *org_result;
  char *n, *n_start, *n_end; /* pointers to string */
  char *metaph_end;	     /* pointers to end of metaph */
  char ntrans[32];	     /* word with uppercase letters */
  int  KSflag;		     /* state flag for X to KS */

  if (!word)					/* Null argument */
  {
    *is_null=1;
    return 0;
  }
  w_end=word+args->lengths[0];
  org_result=result;

  /*--------------------------------------------------------
   *  Copy word to internal buffer, dropping non-alphabetic
   *  characters and converting to uppercase.
   *-------------------------------------------------------*/

  for (n = ntrans + 1, n_end = ntrans + sizeof(ntrans)-2;
	word != w_end && n < n_end; word++ )
    if ( isalpha ( *word ))
      *n++ = toupper ( *word );

  if ( n == ntrans + 1 )	/* return empty string if 0 bytes */
  {
    *length=0;
    return result;
  }
  n_end = n;			/* set n_end to end of string */
  ntrans[0] = 'Z';		/* ntrans[0] should be a neutral char */
  n[0]=n[1]=0;			/* pad with nulls */
  n = ntrans + 1;		/* assign pointer to start */

  /*------------------------------------------------------------
   *  check for all prefixes:
   *		PN KN GN AE WR WH and X at start.
   *----------------------------------------------------------*/

  switch ( *n ) {
  case 'P':
  case 'K':
  case 'G':
    if ( n[1] == 'N')
      *n++ = 0;
    break;
  case 'A':
    if ( n[1] == 'E')
      *n++ = 0;
    break;
  case 'W':
    if ( n[1] == 'R' )
      *n++ = 0;
    else
      if ( *(n + 1) == 'H')
      {
	n[1] = *n;
	*n++ = 0;
      }
    break;
  case 'X':
    *n = 'S';
    break;
  }

  /*------------------------------------------------------------
   *  Now, loop step through string, stopping at end of string
   *  or when the computed metaph is MAXMETAPH characters long
   *----------------------------------------------------------*/

  KSflag = 0; /* state flag for KS translation */

  for (metaph_end = result + MAXMETAPH, n_start = n;
	n < n_end && result < metaph_end; n++ )
  {

    if ( KSflag )
    {
      KSflag = 0;
      *result++ = *n;
    }
    else
    {
      /* drop duplicates except for CC */
      if ( *( n - 1 ) == *n && *n != 'C' )
	continue;

      /* check for F J L M N R or first letter vowel */
      if ( NOCHANGE ( *n ) ||
	   ( n == n_start && ISVOWEL ( *n )))
	*result++ = *n;
      else
	switch ( *n ) {
	case 'B':	 /* check for -MB */
	  if ( n < n_end || *( n - 1 ) != 'M' )
	    *result++ = *n;
	  break;

	case 'C': /* C = X ("sh" sound) in CH and CIA */
	  /*   = S in CE CI and CY	      */
	  /*	 dropped in SCI SCE SCY       */
	  /* else K			      */
	  if ( *( n - 1 ) != 'S' ||
	       !MAKESOFT ( n[1]))
	  {
	    if ( n[1] == 'I' && n[2] == 'A' )
	      *result++ = 'X';
	    else
	      if ( MAKESOFT ( n[1]))
		*result++ = 'S';
	      else
		if ( n[1] == 'H' )
		  *result++ = (( n == n_start &&
				 !ISVOWEL ( n[2])) ||
			       *( n - 1 ) == 'S' ) ?
		    (char)'K' : (char)'X';
		else
		  *result++ = 'K';
	  }
	  break;

	case 'D':  /* J before DGE, DGI, DGY, else T */
	  *result++ =
	    ( n[1] == 'G' &&
	      MAKESOFT ( n[2])) ?
	    (char)'J' : (char)'T';
	  break;

	case 'G':   /* complicated, see table in text */
	  if (( n[1] != 'H' || ISVOWEL ( n[2]))
	      && (
		  n[1] != 'N' ||
		  (
		   (n + 1) < n_end  &&
		   (
		    n[2] != 'E' ||
		    *( n + 3 ) != 'D'
		    )
		   )
		  )
	      && (
		  *( n - 1 ) != 'D' ||
		  !MAKESOFT ( n[1])
		  )
	      )
	    *result++ =
	      ( MAKESOFT ( *( n  + 1 )) &&
		n[2] != 'G' ) ?
	      (char)'J' : (char)'K';
	  else
	    if ( n[1] == 'H'   &&
		!NOGHTOF( *( n - 3 )) &&
		*( n - 4 ) != 'H')
	      *result++ = 'F';
	  break;

	case 'H':   /* H if before a vowel and not after */
	  /* C, G, P, S, T */

	  if ( !AFFECTH ( *( n - 1 )) &&
	       ( !ISVOWEL ( *( n - 1 )) ||
		 ISVOWEL ( n[1])))
	    *result++ = 'H';
	  break;

	case 'K':    /* K = K, except dropped after C */
	  if ( *( n - 1 ) != 'C')
	    *result++ = 'K';
	  break;

	case 'P':    /* PH = F, else P = P */
	  *result++ = *( n +  1 ) == 'H'
	    ? (char)'F' : (char)'P';
	  break;
	case 'Q':   /* Q = K (U after Q is already gone */
	  *result++ = 'K';
	  break;

	case 'S':   /* SH, SIO, SIA = X ("sh" sound) */
	  *result++ = ( n[1] == 'H' ||
			( *(n  + 1) == 'I' &&
			  ( n[2] == 'O' ||
			    n[2] == 'A')))  ?
	    (char)'X' : (char)'S';
	  break;

	case 'T':  /* TIO, TIA = X ("sh" sound) */
	  /* TH = 0, ("th" sound ) */
	  if ( *( n  + 1 ) == 'I' && ( n[2] == 'O'
				      || n[2] == 'A') )
	    *result++ = 'X';
	  else
	    if ( n[1] == 'H' )
	      *result++ = '0';
	    else
	      if ( *( n + 1) != 'C' || n[2] != 'H')
		*result++ = 'T';
	  break;

	case 'V':     /* V = F */
	  *result++ = 'F';
	  break;

	case 'W':     /* only exist if a vowel follows */
	case 'Y':
	  if ( ISVOWEL ( n[1]))
	    *result++ = *n;
	  break;

	case 'X':     /* X = KS, except at start */
	  if ( n == n_start )
	    *result++ = 'S';
	  else
	  {
	    *result++ = 'K'; /* insert K, then S */
	    KSflag = 1; /* this flag will cause S to be
			   inserted on next pass thru loop */
	  }
	  break;

	case 'Z':
	  *result++ = 'S';
	  break;
	}
    }
  }
  *length= (unsigned long) (result - org_result);
  return org_result;
}


/***************************************************************************
** UDF double function.
** Arguments:
** initid	Structure filled by xxx_init
** args		The same structure as to xxx_init. This structure
**		contains values for all parameters.
**		Note that the functions MUST check and convert all
**		to the type it wants!  Null values are represented by
**		a NULL pointer
** is_null	If the result is null, one should store 1 here.
** error	If something goes fatally wrong one should store 1 here.
**
** This function should return the result.
***************************************************************************/

my_bool myfunc_double_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  uint i;

  if (!args->arg_count)
  {
    strcpy(message,"myfunc_double must have at least one argument");
    return 1;
  }
  /*
  ** As this function wants to have everything as strings, force all arguments
  ** to strings.
  */
  for (i=0 ; i < args->arg_count; i++)
    args->arg_type[i]=STRING_RESULT;
  initid->maybe_null=1;		/* The result may be null */
  initid->decimals=2;		/* We want 2 decimals in the result */
  initid->max_length=6;		/* 3 digits + . + 2 decimals */
  return 0;
}


double myfunc_double(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args,
                     char *is_null, char *error __attribute__((unused)))
{
  unsigned long val = 0;
  unsigned long v = 0;
  uint i, j;

  for (i = 0; i < args->arg_count; i++)
  {
    if (args->args[i] == NULL)
      continue;
    val += args->lengths[i];
    for (j=args->lengths[i] ; j-- > 0 ;)
      v += args->args[i][j];
  }
  if (val)
    return (double) v/ (double) val;
  *is_null=1;
  return 0.0;
}


/***************************************************************************
** UDF long long function.
** Arguments:
** initid	Return value from xxxx_init
** args		The same structure as to xxx_init. This structure
**		contains values for all parameters.
**		Note that the functions MUST check and convert all
**		to the type it wants!  Null values are represented by
**		a NULL pointer
** is_null	If the result is null, one should store 1 here.
** error	If something goes fatally wrong one should store 1 here.
**
** This function should return the result as a long long
***************************************************************************/

/* This function returns the sum of all arguments */

longlong myfunc_int(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args,
                    char *is_null __attribute__((unused)),
                    char *error __attribute__((unused)))
{
  longlong val = 0;
  uint i;

  for (i = 0; i < args->arg_count; i++)
  {
    if (args->args[i] == NULL)
      continue;
    switch (args->arg_type[i]) {
    case STRING_RESULT:			/* Add string lengths */
      val += args->lengths[i];
      break;
    case INT_RESULT:			/* Add numbers */
      val += *((longlong*) args->args[i]);
      break;
    case REAL_RESULT:			/* Add numers as longlong */
      val += (longlong) *((double*) args->args[i]);
      break;
    default:
      break;
    }
  }
  return val;
}

/*
  At least one of _init/_deinit is needed unless the server is started
  with --allow_suspicious_udfs.
*/
my_bool myfunc_int_init(UDF_INIT *initid __attribute__((unused)),
                        UDF_ARGS *args __attribute__((unused)),
                        char *message __attribute__((unused)))
{
  return 0;
}

/*
  Simple example of how to get a sequences starting from the first argument
  or 1 if no arguments have been given
*/

my_bool sequence_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count > 1)
  {
    strmov(message,"This function takes none or 1 argument");
    return 1;
  }
  if (args->arg_count)
    args->arg_type[0]= INT_RESULT;		/* Force argument to int */

  if (!(initid->ptr=(char*) malloc(sizeof(longlong))))
  {
    strmov(message,"Couldn't allocate memory");
    return 1;
  }
  bzero(initid->ptr,sizeof(longlong));
  /* 
    sequence() is a non-deterministic function : it has different value 
    even if called with the same arguments.
  */
  initid->const_item=0;
  return 0;
}

void sequence_deinit(UDF_INIT *initid)
{
  if (initid->ptr)
    free(initid->ptr);
}

longlong sequence(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args,
                  char *is_null __attribute__((unused)),
                  char *error __attribute__((unused)))
{
  ulonglong val=0;
  if (args->arg_count)
    val= *((longlong*) args->args[0]);
  return ++*((longlong*) initid->ptr) + val;
}


/****************************************************************************
** Some functions that handles IP and hostname conversions
** The orignal function was from Zeev Suraski.
**
** CREATE FUNCTION lookup RETURNS STRING SONAME "udf_example.so";
** CREATE FUNCTION reverse_lookup RETURNS STRING SONAME "udf_example.so";
**
****************************************************************************/

#ifdef __WIN__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

my_bool lookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void lookup_deinit(UDF_INIT *initid);
char *lookup(UDF_INIT *initid, UDF_ARGS *args, char *result,
	     unsigned long *length, char *null_value, char *error);
my_bool reverse_lookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void reverse_lookup_deinit(UDF_INIT *initid);
char *reverse_lookup(UDF_INIT *initid, UDF_ARGS *args, char *result,
		     unsigned long *length, char *null_value, char *error);


/****************************************************************************
** lookup IP for an hostname.
**
** This code assumes that gethostbyname_r exists and inet_ntoa() is thread
** safe (As it is in Solaris)
****************************************************************************/


my_bool lookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
  {
    strmov(message,"Wrong arguments to lookup;  Use the source");
    return 1;
  }
  initid->max_length=11;
  initid->maybe_null=1;
#if !defined(HAVE_GETHOSTBYADDR_R) || !defined(HAVE_SOLARIS_STYLE_GETHOST)
  (void) pthread_mutex_init(&LOCK_hostname,MY_MUTEX_INIT_SLOW);
#endif
  return 0;
}

void lookup_deinit(UDF_INIT *initid __attribute__((unused)))
{
#if !defined(HAVE_GETHOSTBYADDR_R) || !defined(HAVE_SOLARIS_STYLE_GETHOST)
  (void) pthread_mutex_destroy(&LOCK_hostname);
#endif
}

char *lookup(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args,
             char *result, unsigned long *res_length, char *null_value,
             char *error __attribute__((unused)))
{
  uint length;
  char name_buff[256];
  struct hostent *hostent;
#if defined(HAVE_GETHOSTBYADDR_R) && defined(HAVE_SOLARIS_STYLE_GETHOST)
  int tmp_errno;
  char hostname_buff[2048];
  struct hostent tmp_hostent;
#endif
  struct in_addr in;

  if (!args->args[0] || !(length=args->lengths[0]))
  {
    *null_value=1;
    return 0;
  }
  if (length >= sizeof(name_buff))
    length=sizeof(name_buff)-1;
  memcpy(name_buff,args->args[0],length);
  name_buff[length]=0;
#if defined(HAVE_GETHOSTBYADDR_R) && defined(HAVE_SOLARIS_STYLE_GETHOST)
  if (!(hostent=gethostbyname_r(name_buff,&tmp_hostent,hostname_buff,
				sizeof(hostname_buff), &tmp_errno)))
  {
    *null_value=1;
    return 0;
  }
#else
  VOID(pthread_mutex_lock(&LOCK_hostname));
  if (!(hostent= gethostbyname((char*) name_buff)))
  {
    VOID(pthread_mutex_unlock(&LOCK_hostname));
    *null_value= 1;
    return 0;
  }
  VOID(pthread_mutex_unlock(&LOCK_hostname));
#endif
  memcpy_fixed((char*) &in,(char*) *hostent->h_addr_list, sizeof(in.s_addr));
  *res_length= (ulong) (strmov(result, inet_ntoa(in)) - result);
  return result;
}


/****************************************************************************
** return hostname for an IP number.
** The functions can take as arguments a string "xxx.xxx.xxx.xxx" or
** four numbers.
****************************************************************************/

my_bool reverse_lookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count == 1)
    args->arg_type[0]= STRING_RESULT;
  else if (args->arg_count == 4)
    args->arg_type[0]=args->arg_type[1]=args->arg_type[2]=args->arg_type[3]=
      INT_RESULT;
  else
  {
    strmov(message,
	   "Wrong number of arguments to reverse_lookup;  Use the source");
    return 1;
  }
  initid->max_length=32;
  initid->maybe_null=1;
#if !defined(HAVE_GETHOSTBYADDR_R) || !defined(HAVE_SOLARIS_STYLE_GETHOST)
  (void) pthread_mutex_init(&LOCK_hostname,MY_MUTEX_INIT_SLOW);
#endif
  return 0;
}

void reverse_lookup_deinit(UDF_INIT *initid __attribute__((unused)))
{
#if !defined(HAVE_GETHOSTBYADDR_R) || !defined(HAVE_SOLARIS_STYLE_GETHOST)
  (void) pthread_mutex_destroy(&LOCK_hostname);
#endif
}

char *reverse_lookup(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args,
                     char *result, unsigned long *res_length,
                     char *null_value, char *error __attribute__((unused)))
{
#if defined(HAVE_GETHOSTBYADDR_R) && defined(HAVE_SOLARIS_STYLE_GETHOST)
  char name_buff[256];
  struct hostent tmp_hostent;
  int tmp_errno;
#endif
  struct hostent *hp;
  unsigned long taddr;
  uint length;

  if (args->arg_count == 4)
  {
    if (!args->args[0] || !args->args[1] ||!args->args[2] ||!args->args[3])
    {
      *null_value=1;
      return 0;
    }
    sprintf(result,"%d.%d.%d.%d",
	    (int) *((longlong*) args->args[0]),
	    (int) *((longlong*) args->args[1]),
	    (int) *((longlong*) args->args[2]),
	    (int) *((longlong*) args->args[3]));
  }
  else
  {					/* string argument */
    if (!args->args[0])			/* Return NULL for NULL values */
    {
      *null_value=1;
      return 0;
    }
    length=args->lengths[0];
    if (length >= (uint) *res_length-1)
      length=(uint) *res_length;
    memcpy(result,args->args[0],length);
    result[length]=0;
  }

  taddr = inet_addr(result);
  if (taddr == (unsigned long) -1L)
  {
    *null_value=1;
    return 0;
  }
#if defined(HAVE_GETHOSTBYADDR_R) && defined(HAVE_SOLARIS_STYLE_GETHOST)
  if (!(hp=gethostbyaddr_r((char*) &taddr,sizeof(taddr), AF_INET,
			   &tmp_hostent, name_buff,sizeof(name_buff),
			   &tmp_errno)))
  {
    *null_value=1;
    return 0;
  }
#else
  VOID(pthread_mutex_lock(&LOCK_hostname));
  if (!(hp= gethostbyaddr((char*) &taddr, sizeof(taddr), AF_INET)))
  {
    VOID(pthread_mutex_unlock(&LOCK_hostname));
    *null_value= 1;
    return 0;
  }
  VOID(pthread_mutex_unlock(&LOCK_hostname));
#endif
  *res_length=(ulong) (strmov(result,hp->h_name) - result);
  return result;
}

/*
** Syntax for the new aggregate commands are:
** create aggregate function <function_name> returns {string|real|integer}
**		  soname <name_of_shared_library>
**
** Syntax for avgcost: avgcost( t.quantity, t.price )
**	with t.quantity=integer, t.price=double
** (this example is provided by Andreas F. Bobak <bobak@relog.ch>)
*/


struct avgcost_data
{
  ulonglong	count;
  longlong	totalquantity;
  double	totalprice;
};


/*
** Average Cost Aggregate Function.
*/
my_bool
avgcost_init( UDF_INIT* initid, UDF_ARGS* args, char* message )
{
  struct avgcost_data*	data;

  if (args->arg_count != 2)
  {
    strcpy(
	   message,
	   "wrong number of arguments: AVGCOST() requires two arguments"
	   );
    return 1;
  }

  if ((args->arg_type[0] != INT_RESULT) || (args->arg_type[1] != REAL_RESULT) )
  {
    strcpy(
	   message,
	   "wrong argument type: AVGCOST() requires an INT and a REAL"
	   );
    return 1;
  }

  /*
  **	force arguments to double.
  */
  /*args->arg_type[0]	= REAL_RESULT;
    args->arg_type[1]	= REAL_RESULT;*/

  initid->maybe_null	= 0;		/* The result may be null */
  initid->decimals	= 4;		/* We want 4 decimals in the result */
  initid->max_length	= 20;		/* 6 digits + . + 10 decimals */

  if (!(data = (struct avgcost_data*) malloc(sizeof(struct avgcost_data))))
  {
    strmov(message,"Couldn't allocate memory");
    return 1;
  }
  data->totalquantity	= 0;
  data->totalprice	= 0.0;

  initid->ptr = (char*)data;

  return 0;
}

void
avgcost_deinit( UDF_INIT* initid )
{
  free(initid->ptr);
}


/* This is only for MySQL 4.0 compability */
void
avgcost_reset(UDF_INIT* initid, UDF_ARGS* args, char* is_null, char* message)
{
  avgcost_clear(initid, is_null, message);
  avgcost_add(initid, args, is_null, message);
}

/* This is needed to get things to work in MySQL 4.1.1 and above */

void
avgcost_clear(UDF_INIT* initid, char* is_null __attribute__((unused)),
              char* message __attribute__((unused)))
{
  struct avgcost_data* data = (struct avgcost_data*)initid->ptr;
  data->totalprice=	0.0;
  data->totalquantity=	0;
  data->count=		0;
}


void
avgcost_add(UDF_INIT* initid, UDF_ARGS* args,
            char* is_null __attribute__((unused)),
            char* message __attribute__((unused)))
{
  if (args->args[0] && args->args[1])
  {
    struct avgcost_data* data	= (struct avgcost_data*)initid->ptr;
    longlong quantity		= *((longlong*)args->args[0]);
    longlong newquantity	= data->totalquantity + quantity;
    double price		= *((double*)args->args[1]);

    data->count++;

    if (   ((data->totalquantity >= 0) && (quantity < 0))
	   || ((data->totalquantity <  0) && (quantity > 0)) )
    {
      /*
      **	passing from + to - or from - to +
      */
      if (   ((quantity < 0) && (newquantity < 0))
	     || ((quantity > 0) && (newquantity > 0)) )
      {
	data->totalprice	= price * (double)newquantity;
      }
      /*
      **	sub q if totalq > 0
      **	add q if totalq < 0
      */
      else
      {
	price		  = data->totalprice / (double)data->totalquantity;
	data->totalprice  = price * (double)newquantity;
      }
      data->totalquantity = newquantity;
    }
    else
    {
      data->totalquantity	+= quantity;
      data->totalprice		+= price * (double)quantity;
    }

    if (data->totalquantity == 0)
      data->totalprice = 0.0;
  }
}


double
avgcost( UDF_INIT* initid, UDF_ARGS* args __attribute__((unused)),
         char* is_null, char* error __attribute__((unused)))
{
  struct avgcost_data* data = (struct avgcost_data*)initid->ptr;
  if (!data->count || !data->totalquantity)
  {
    *is_null = 1;
    return 0.0;
  }

  *is_null = 0;
  return data->totalprice/(double)data->totalquantity;
}

my_bool myfunc_argument_name_init(UDF_INIT *initid, UDF_ARGS *args,
				  char *message);
char *myfunc_argument_name(UDF_INIT *initid, UDF_ARGS *args, char *result,
			   unsigned long *length, char *null_value,
			   char *error);

my_bool myfunc_argument_name_init(UDF_INIT *initid, UDF_ARGS *args,
				  char *message)
{
  if (args->arg_count != 1)
  {
    strmov(message,"myfunc_argument_name_init accepts only one argument");
    return 1;
  }
  initid->max_length= args->attribute_lengths[0];
  initid->maybe_null= 1;
  initid->const_item= 1;
  return 0;
}

char *myfunc_argument_name(UDF_INIT *initid __attribute__((unused)),
                           UDF_ARGS *args, char *result,
                           unsigned long *length, char *null_value,
                           char *error __attribute__((unused)))
{
  if (!args->attributes[0])
  {
    null_value= 0;
    return 0;
  }
  (*length)--; /* space for ending \0 (for debugging purposes) */
  if (*length > args->attribute_lengths[0])
    *length= args->attribute_lengths[0];
  memcpy(result, args->attributes[0], *length);
  result[*length]= 0;
  return result;
}



my_bool is_const_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count != 1)
  {
    strmov(message, "IS_CONST accepts only one argument");
    return 1;
  }
  initid->ptr= (char*)((args->args[0] != NULL) ? 1UL : 0);
  return 0;
}

char * is_const(UDF_INIT *initid, UDF_ARGS *args __attribute__((unused)),
                char *result, unsigned long *length,
                char *is_null, char *error __attribute__((unused)))
{
  if (initid->ptr != 0) {
    sprintf(result, "const");
  } else {
    sprintf(result, "not const");
  }
  *is_null= 0;
  *length= strlen(result);
  return result;
}



my_bool check_const_len_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count != 1)
  {
    strmov(message, "CHECK_CONST_LEN accepts only one argument");
    return 1;
  }
  if (args->args[0] == 0)
  {
    initid->ptr= (char*)"Not constant";
  }
  else if(strlen(args->args[0]) == args->lengths[0])
  {
    initid->ptr= (char*)"Correct length";
  }
  else
  {
    initid->ptr= (char*)"Wrong length";
  }
  initid->max_length = 100;
  return 0;
}

char * check_const_len(UDF_INIT *initid, UDF_ARGS *args __attribute__((unused)),
                char *result, unsigned long *length,
                char *is_null, char *error __attribute__((unused)))
{
  strmov(result, initid->ptr);
  *length= strlen(result);
  *is_null= 0;
  return result;
}


#endif /* HAVE_DLOPEN */
