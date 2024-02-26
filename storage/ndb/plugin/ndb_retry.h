/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_RETRY_H
#define NDB_RETRY_H

#include <functional>  // std::function

#include "ndbapi/NdbApi.hpp"               // NdbError
#include "sql/sql_class.h"                 // THD
#include "storage/ndb/plugin/ndb_sleep.h"  // ndb_retry_sleep

/**
  A wrapper to execute the given std::function instance that uses NdbTransaction
  to perform transactions on the data nodes. The transaction has a possibility
  of failing due to a temporary error. In such cases, this wrapper will execute
  the function again after sleeping for some time.

  @note
    A generic std::function instance is used here rather than a generic
    function pointer. This is because the std::function handles the member
    functions better than the function pointers.

  @note
    Execution will be retried only if the THD has not been killed. To disable
    this check, pass nullptr to thd param instead of passing a valid pointer.

  @param ndb                The Ndb object.
  @param thd                THD object pointer.
  @param retry_sleep        The amount of time(in ms) to sleep before retrying
                            in case of a temporary error.
  @param[out] last_ndb_err  The last NdbError in case of failure
  @param ndb_func           The std::function instance of the function that
                            needs to be executed by this wrapper. The function
                            should take a NdbTransaction* parameter followed by
                            any number of other parameters. It should return
                            a const NdbError* on failure and nullptr on success.
  @param args               The arguments that need to be passed to the
                            function during execution.

  @retval true              On success
  @retval false             On failure
*/
template <typename... FunctionArgTypes, typename... FunctionArgs>
bool ndb_execute_and_retry(
    Ndb *ndb, const THD *thd, unsigned int retry_sleep, NdbError &last_ndb_err,
    const std::function<const NdbError *(NdbTransaction *, FunctionArgTypes...)>
        &ndb_func,
    FunctionArgs... args) {
  int retries = 100;
  const NdbError *ndbError;
  assert(ndb != nullptr);

  do {
    /* Start transaction */
    NdbTransaction *ndb_transaction = ndb->startTransaction();
    if (ndb_transaction != nullptr) {
      /* Starting the transaction succeeded.
         Execute the function and store the error */
      ndbError = ndb_func(ndb_transaction, args...);
    } else {
      /* Failed to create transaction */
      ndbError = &(ndb->getNdbError());
    }

    /* Handle the error */
    if (ndbError == nullptr) {
      /* No error, the function execution is a success */
      ndb_transaction->close();
      return true;
    }

    // Copy the last NdbError
    last_ndb_err = *ndbError;

    if (ndbError->status == NdbError::TemporaryError &&
        (thd == nullptr || !thd->killed)) {
      /* Temporary error. Close transaction, sleep and retry. */
      if (ndb_transaction != nullptr) {
        ndb_transaction->close();
      }
      ndb_retry_sleep(retry_sleep);
    } else {
      /* Permanent error or the thread has been killed.
         Skip retrying and return. */
      if (ndb_transaction != nullptr) {
        ndb_transaction->close();
      }
      return false;
    }
  } while (retries--); /* Retry on temporary error */

  /* All retries failed.*/
  return false;
}

/**
  @brief Wrapper of function ndb_execute_and_retry with a fixed sleep of 30ms
         in between retries. To be used to execute functions that build
         and execute NDB Transactions.
*/
template <typename... FunctionArgTypes, typename... FunctionArgs>
bool ndb_trans_retry(
    Ndb *ndb, const THD *thd, NdbError &last_ndb_err,
    const std::function<const NdbError *(NdbTransaction *, FunctionArgTypes...)>
        &ndb_func,
    FunctionArgs... args) {
  return ndb_execute_and_retry<FunctionArgTypes...>(
      ndb, thd, 30, last_ndb_err, ndb_func,
      std::forward<FunctionArgs>(args)...);
}

#endif /* NDB_RETRY_H */
