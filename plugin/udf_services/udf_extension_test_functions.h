/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef UDF_EXTENSION_TEST_FUNCTIONS_INCLUDED
#define UDF_EXTENSION_TEST_FUNCTIONS_INCLUDED

#include <mysql/plugin.h>

#ifdef WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

/**
  Initialize the UDF function that tests character set of return value.
  It sets the charset of the return value in the UDF_INIT structure.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_result_charset_init(UDF_INIT *initid, UDF_ARGS *args,
                                            char *message);

/**
  Executes the UDF function that returns the first argument but after
  converting that in to the character set as specified during UDF
  initialization time.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_result_charset(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
                                        unsigned char *is_null,
                                        unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_result_charset_deinit(UDF_INIT *initid);

/**
  Initialize the UDF function that tests character set of arguments.
  It changes the character set of first argument as of a character set
  of second argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_args_charset_init(UDF_INIT *initid, UDF_ARGS *args,
                                          char *message);

/**
  Executes the UDF function that returns the first argument of UDF as it is.
  Server provides the converted first argument as that had been changed
  during UDF inititialization.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_args_charset(UDF_INIT *initid, UDF_ARGS *args,
                                      char *result, unsigned long *length,
                                      unsigned char *is_null,
                                      unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_args_charset_deinit(UDF_INIT *initid);

/**
  Initialize the UDF function that tests collation of return value.
  It fetches the charset from the collation of second argument and sets
  that as character set of first argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_result_collation_init(UDF_INIT *initid, UDF_ARGS *args,
                                              char *message);
/**
  Executes the UDF function that picks the first argument, convert the value
  into character set as specified during UDF inititlization abd return
  the same.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_result_collation(UDF_INIT *initid, UDF_ARGS *args,
                                          char *result, unsigned long *length,
                                          unsigned char *is_null,
                                          unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_result_collation_deinit(UDF_INIT *initid);

/**
  Initialize the UDF function that tests collation of arguments.
  It changes the charset of the first UDF argument. It fetches the
  charset from the collation of second argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_args_collation_init(UDF_INIT *initid, UDF_ARGS *args,
                                            char *message);

/**
  Executes the UDF function to check the charset conversion of UDF argument
  performed by the server. It returns the first UDF argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_args_collation(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
                                        unsigned char *is_null,
                                        unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_args_collation_deinit(UDF_INIT *initid);

/**
  Initialize the UDF function that tests character set of return value.
  It sets the charset of first UDF argument as specified by the user in
  the second UDF argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_result_charset_with_value_init(UDF_INIT *initid,
                                                       UDF_ARGS *args,
                                                       char *message);

/**
  Executes the UDF function that converts the first UDF argument into the
  charset as specified by the user. It reads the charset of return value
  set by the user during UDF preparation time, converts the first UDF
  argument into that charset before returning the same.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_result_charset_with_value(UDF_INIT *initid,
                                                   UDF_ARGS *args, char *result,
                                                   unsigned long *length,
                                                   unsigned char *is_null,
                                                   unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_result_charset_with_value_deinit(UDF_INIT *initid);

/**
  Initialize the UDF function that tests character set of arguments.
  It sets the charset of the first UDF argument as specified by the user
  in second argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_args_charset_with_value_init(UDF_INIT *initid,
                                                     UDF_ARGS *args,
                                                     char *message);

/**
  Executes the UDF function to check if server converted the value of
  first UDF argument as it was specified by the user during UDF
  preparation time. It returns the first UDF argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_args_charset_with_value(UDF_INIT *initid,
                                                 UDF_ARGS *args, char *result,
                                                 unsigned long *length,
                                                 unsigned char *is_null,
                                                 unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_args_charset_with_value_deinit(UDF_INIT *initid);

/**
  Initializes the UDF function that tests character set of return value.
  It detmines the charset of the return value from the collation of the
  second argument and set the same in the extension argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_result_collation_with_value_init(UDF_INIT *initid,
                                                         UDF_ARGS *args,
                                                         char *message);

/**
  Executes the UDF function that converts the return value in to the
  charset as determined during UDF preparation time. It returns the
  converted first UDF argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_result_collation_with_value(
    UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *is_null, unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_result_collation_with_value_deinit(UDF_INIT *initid);

/**
  Initialize the UDF function that checks if server performs the charset
  conversion of the UDF argument. It fetches the charset from collation name
  as specified by the user in the second argument and sets the same for the
  first UDF argument.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    message Error message set by the user in case something went
                          wrong.
  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT bool test_args_collation_with_value_init(UDF_INIT *initid,
                                                       UDF_ARGS *args,
                                                       char *message);

/**
  Executes the UDF function that reads the converted the value of the
  firstUDF argument. Server must have converted the first argument as
  it was specified during UDF preparation time. It returns the first
  UDF argument as received.

  @param [in,out] initid  Return value from xxxx_init
  @param [in,out] args    Array of arguments
  @param [out]    result  The result to be returned
  @param [out]    length  Length of the result
  @param [out]    is_null Flag indicating if the result vale could be null.
  @param [out]    error   Flag indicating if UDF execution returned error.

  @retval true    If UDF initialization failed
  @retval false   Otherwise
*/
PLUGIN_EXPORT char *test_args_collation_with_value(UDF_INIT *initid,
                                                   UDF_ARGS *args, char *result,
                                                   unsigned long *length,
                                                   unsigned char *is_null,
                                                   unsigned char *error);

/**
  Cleanups the resources acquired during UDF initialization.

  @param [in,out] initid  Return value from xxxx_init
*/
PLUGIN_EXPORT void test_args_collation_with_value_deinit(UDF_INIT *initid);

#endif  // UDF_EXTENSION_TEST_FUNCTIONS_INCLUDED
