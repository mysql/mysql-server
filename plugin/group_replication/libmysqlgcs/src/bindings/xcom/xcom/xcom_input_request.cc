/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifdef _MSC_VER
#include <stdint.h>
#endif
#include <stdlib.h>
#include "xcom/xcom_input_request.h"
#include "xcom/xcom_memory.h"

typedef struct xcom_input_request {
  app_data_ptr a;
  xcom_input_reply_function_ptr reply_function;
  void *reply_arg;
  struct xcom_input_request *next;
} xcom_input_request;

xcom_input_request_ptr xcom_input_request_new(
    app_data_ptr a, xcom_input_reply_function_ptr reply_function,
    void *reply_arg) {
  xcom_input_request_ptr request = (xcom_input_request_ptr)xcom_calloc(
      (size_t)1, sizeof(xcom_input_request));
  if (request != nullptr) {
    request->a = a;
    request->reply_function = reply_function;
    request->reply_arg = reply_arg;
  }
  return request;
}

void xcom_input_request_free(xcom_input_request_ptr request) {
  /* We own the app_data we point to, so it's our job to free it. */
  if (request->a != nullptr) {
    /* The app_data is supposed to be unlinked. */
    assert(request->a->next == nullptr);
    /* Because the app_data_ptr is allocated on the heap and not on the stack.
     */
    xdr_free((xdrproc_t)xdr_app_data_ptr, (char *)&request->a);
  }
  free(request);
}

void xcom_input_request_set_next(xcom_input_request_ptr request,
                                 xcom_input_request_ptr next) {
  request->next = next;
}

xcom_input_request_ptr xcom_input_request_extract_next(
    xcom_input_request_ptr request) {
  xcom_input_request_ptr next = request->next;
  request->next = nullptr;
  return next;
}

app_data_ptr xcom_input_request_extract_app_data(
    xcom_input_request_ptr request) {
  app_data_ptr a = request->a;
  request->a = nullptr;
  return a;
}

void xcom_input_request_reply(xcom_input_request_ptr request,
                              pax_msg *payload) {
  (*request->reply_function)(request->reply_arg, payload);
}
