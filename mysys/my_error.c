/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_string.h>
#include <stdarg.h>
#include <m_ctype.h>
#include "my_base.h"
#include "my_handler_errors.h"

/* Max length of a error message. Should be kept in sync with MYSQL_ERRMSG_SIZE. */
#define ERRMSGSIZE      (512)

/* Define some external variables for error handling */

/*
  WARNING!
  my_error family functions have to be used according following rules:
  - if message have not parameters use my_message(ER_CODE, ER(ER_CODE), MYF(N))
  - if message registered use my_error(ER_CODE, MYF(N), ...).
  - With some special text of errror message use:
  my_printf_error(ER_CODE, format, MYF(N), ...)
*/

/*
  Message texts are registered into a linked list of 'my_err_head' structs.
  Each struct contains (1.) an array of pointers to C character strings with
  '\0' termination, (2.) the error number for the first message in the array
  (array index 0) and (3.) the error number for the last message in the array
  (array index (last - first)).
  The array may contain gaps with NULL pointers and pointers to empty strings.
  Both kinds of gaps will be translated to "Unknown error %d.", if my_error()
  is called with a respective error number.
  The list of header structs is sorted in increasing order of error numbers.
  Negative error numbers are allowed. Overlap of error numbers is not allowed.
  Not registered error numbers will be translated to "Unknown error %d.".
*/
static struct my_err_head
{
  struct my_err_head    *meh_next;         /* chain link */
  const char**          (*get_errmsgs) (); /* returns error message format */
  int                   meh_first;       /* error number matching array slot 0 */
  int                   meh_last;          /* error number matching last slot */
} my_errmsgs_globerrs = {NULL, get_global_errmsgs, EE_ERROR_FIRST, EE_ERROR_LAST};

static struct my_err_head *my_errmsgs_list= &my_errmsgs_globerrs;


/**
  Get a string describing a system or handler error. thread-safe.

  @param  buf  a buffer in which to return the error message
  @param  len  the size of the aforementioned buffer
  @param  nr   the error number

  @retval buf  always buf. for signature compatibility with strerror(3).
*/

char *my_strerror(char *buf, size_t len, int nr)
{
  char *msg= NULL;

  buf[0]= '\0';                                  /* failsafe */

  /*
    These (handler-) error messages are shared by perror, as required
    by the principle of least surprise.
  */
  if ((nr >= HA_ERR_FIRST) && (nr <= HA_ERR_LAST))
    msg= (char *) handler_error_messages[nr - HA_ERR_FIRST];

  if (msg != NULL)
    strmake(buf, msg, len - 1);
  else
  {
    /*
      On Windows, do things the Windows way. On a system that supports both
      the GNU and the XSI variant, use whichever was configured (GNU); if
      this choice is not advertised, use the default (POSIX/XSI).  Testing
      for __GNUC__ is not sufficient to determine whether this choice exists.
    */
#if defined(__WIN__)
    strerror_s(buf, len, nr);
#elif ((defined _POSIX_C_SOURCE && (_POSIX_C_SOURCE >= 200112L)) ||    \
       (defined _XOPEN_SOURCE   && (_XOPEN_SOURCE >= 600)))      &&    \
      ! defined _GNU_SOURCE
    strerror_r(nr, buf, len);             /* I can build with or without GNU */
#elif defined _GNU_SOURCE
    char *r= strerror_r(nr, buf, len);
    if (r != buf)                         /* Want to help, GNU? */
      strmake(buf, r, len - 1);           /* Then don't. */
#else
    strerror_r(nr, buf, len);
#endif
  }

  /*
    strerror() return values are implementation-dependent, so let's
    be pragmatic.
  */
  if (!buf[0])
    strmake(buf, "unknown error", len - 1);

  return buf;
}


/**
  @brief Get an error format string from one of the my_error_register()ed sets

  @note
    NULL values are possible even within a registered range.

  @param nr Errno

  @retval NULL  if no message is registered for this error number
  @retval str   C-string
*/

const char *my_get_err_msg(int nr)
{
  const char *format;
  struct my_err_head *meh_p;

  /* Search for the range this error is in. */
  for (meh_p= my_errmsgs_list; meh_p; meh_p= meh_p->meh_next)
    if (nr <= meh_p->meh_last)
      break;

  /*
    If we found the range this error number is in, get the format string.
    If the string is empty, or a NULL pointer, or if we're out of return,
    we return NULL.
  */
  if (!(format= (meh_p && (nr >= meh_p->meh_first)) ?
                meh_p->get_errmsgs()[nr - meh_p->meh_first] : NULL) ||
      !*format)
    return NULL;

  return format;
}


/**
  Fill in and print a previously registered error message.

  @note
    Goes through the (sole) function registered in error_handler_hook

  @param nr        error number
  @param MyFlags   Flags
  @param ...       variable list matching that error format string
*/

void my_error(int nr, myf MyFlags, ...)
{
  const char *format;
  va_list args;
  char ebuff[ERRMSGSIZE];
  DBUG_ENTER("my_error");
  DBUG_PRINT("my", ("nr: %d  MyFlags: %d  errno: %d", nr, MyFlags, errno));

  if (!(format = my_get_err_msg(nr)))
    (void) my_snprintf(ebuff, sizeof(ebuff), "Unknown error %d", nr);
  else
  {
    va_start(args,MyFlags);
    (void) my_vsnprintf_ex(&my_charset_utf8_general_ci, ebuff,
                           sizeof(ebuff), format, args);
    va_end(args);
  }
  (*error_handler_hook)(nr, ebuff, MyFlags);
  DBUG_VOID_RETURN;
}


/**
  Print an error message.

  @note
    Goes through the (sole) function registered in error_handler_hook

  @param error     error number
  @param format    format string
  @param MyFlags   Flags
  @param ...       variable list matching that error format string
*/

void my_printf_error(uint error, const char *format, myf MyFlags, ...)
{
  va_list args;
  char ebuff[ERRMSGSIZE];
  DBUG_ENTER("my_printf_error");
  DBUG_PRINT("my", ("nr: %d  MyFlags: %d  errno: %d  Format: %s",
		    error, MyFlags, errno, format));

  va_start(args,MyFlags);
  (void) my_vsnprintf_ex(&my_charset_utf8_general_ci, ebuff,
                         sizeof(ebuff), format, args);
  va_end(args);
  (*error_handler_hook)(error, ebuff, MyFlags);
  DBUG_VOID_RETURN;
}

/**
  Print an error message.

  @note
    Goes through the (sole) function registered in error_handler_hook

  @param error     error number
  @param format    format string
  @param MyFlags   Flags
  @param ap        variable list matching that error format string
*/

void my_printv_error(uint error, const char *format, myf MyFlags, va_list ap)
{
  char ebuff[ERRMSGSIZE];
  DBUG_ENTER("my_printv_error");
  DBUG_PRINT("my", ("nr: %d  MyFlags: %d  errno: %d  format: %s",
		    error, MyFlags, errno, format));

  (void) my_vsnprintf(ebuff, sizeof(ebuff), format, ap);
  (*error_handler_hook)(error, ebuff, MyFlags);
  DBUG_VOID_RETURN;
}

/*
  Warning as printf

  SYNOPSIS
    my_printf_warning()
      format>   Format string
      ...>      variable list
*/
void(*sql_print_warning_hook)(const char *format,...);
void my_printf_warning(const char *format, ...)
{
  va_list args;
  char wbuff[ERRMSGSIZE];
  DBUG_ENTER("my_printf_warning");
  DBUG_PRINT("my", ("Format: %s", format));
  va_start(args,format);
  (void) my_vsnprintf (wbuff, sizeof(wbuff), format, args);
  va_end(args);
  (*sql_print_warning_hook)(wbuff);
  DBUG_VOID_RETURN;
}

/**
  Print an error message.

  @note
    Goes through the (sole) function registered in error_handler_hook

  @param error     error number
  @param str       error message
  @param MyFlags   Flags
*/

void my_message(uint error, const char *str, register myf MyFlags)
{
  (*error_handler_hook)(error, str, MyFlags);
}


/**
  Register error messages for use with my_error().

  @description

    The pointer array is expected to contain addresses to NUL-terminated
    C character strings. The array contains (last - first + 1) pointers.
    NULL pointers and empty strings ("") are allowed. These will be mapped to
    "Unknown error" when my_error() is called with a matching error number.
    This function registers the error numbers 'first' to 'last'.
    No overlapping with previously registered error numbers is allowed.

  @param   errmsgs  array of pointers to error messages
  @param   first    error number of first message in the array
  @param   last     error number of last message in the array

  @retval  0        OK
  @retval  != 0     Error
*/

int my_error_register(const char** (*get_errmsgs) (), int first, int last)
{
  struct my_err_head *meh_p;
  struct my_err_head **search_meh_pp;

  /* Allocate a new header structure. */
  if (! (meh_p= (struct my_err_head*) my_malloc(sizeof(struct my_err_head),
                                                MYF(MY_WME))))
    return 1;
  meh_p->get_errmsgs= get_errmsgs;
  meh_p->meh_first= first;
  meh_p->meh_last= last;

  /* Search for the right position in the list. */
  for (search_meh_pp= &my_errmsgs_list;
       *search_meh_pp;
       search_meh_pp= &(*search_meh_pp)->meh_next)
  {
    if ((*search_meh_pp)->meh_last > first)
      break;
  }

  /* Error numbers must be unique. No overlapping is allowed. */
  if (*search_meh_pp && ((*search_meh_pp)->meh_first <= last))
  {
    my_free(meh_p);
    return 1;
  }

  /* Insert header into the chain. */
  meh_p->meh_next= *search_meh_pp;
  *search_meh_pp= meh_p;
  return 0;
}


/**
  Unregister formerly registered error messages.

  @description

    This function unregisters the error numbers 'first' to 'last'.
    These must have been previously registered by my_error_register().
    'first' and 'last' must exactly match the registration.
    If a matching registration is present, the header is removed from the
    list and the pointer to the error messages pointers array is returned.
    (The messages themselves are not released here as they may be static.)
    Otherwise, NULL is returned.

  @param   first     error number of first message
  @param   last      error number of last message

  @retval  NULL      Error, no such number range registered.
  @retval  non-NULL  OK, returns address of error messages pointers array.
*/

const char **my_error_unregister(int first, int last)
{
  struct my_err_head    *meh_p;
  struct my_err_head    **search_meh_pp;
  const char            **errmsgs;

  /* Search for the registration in the list. */
  for (search_meh_pp= &my_errmsgs_list;
       *search_meh_pp;
       search_meh_pp= &(*search_meh_pp)->meh_next)
  {
    if (((*search_meh_pp)->meh_first == first) &&
        ((*search_meh_pp)->meh_last == last))
      break;
  }
  if (! *search_meh_pp)
    return NULL;

  /* Remove header from the chain. */
  meh_p= *search_meh_pp;
  *search_meh_pp= meh_p->meh_next;

  /* Save the return value and free the header. */
  errmsgs= meh_p->get_errmsgs();
  my_free(meh_p);
  
  return errmsgs;
}


/**
  Unregister all formerly registered error messages.

  @description

    This function unregisters all error numbers that previously have
    been previously registered by my_error_register().
    All headers are removed from the list; the messages themselves are
    not released here as they may be static.
*/

void my_error_unregister_all(void)
{
  struct my_err_head *cursor, *saved_next;

  for (cursor= my_errmsgs_globerrs.meh_next; cursor != NULL; cursor= saved_next)
  {
    /* We need this ptr, but we're about to free its container, so save it. */
    saved_next= cursor->meh_next;

    my_free(cursor);
  }
  my_errmsgs_globerrs.meh_next= NULL;  /* Freed in first iteration above. */

  my_errmsgs_list= &my_errmsgs_globerrs;
}
