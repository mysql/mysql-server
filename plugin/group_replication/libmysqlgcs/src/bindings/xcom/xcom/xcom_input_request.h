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

#ifndef XCOM_INPUT_REQUEST_H
#define XCOM_INPUT_REQUEST_H

#include "xcom/pax_msg.h"

/**
 * A request directed to XCom through the input channel.
 */
struct xcom_input_request;
typedef struct xcom_input_request *xcom_input_request_ptr;

/**
 * The function type that XCom will use to reply to a request.
 */
typedef void (*xcom_input_reply_function_ptr)(void *reply_arg,
                                              pax_msg *payload);

/**
 * Creates a new XCom request.
 *
 * Takes ownership of @c a.
 *
 * @param a the request's app_data payload
 * @param reply_function the function used to reply to the request
 * @param reply_arg opaque argument to the reply_function
 * @retval xcom_input_request_ptr if successful
 * @retval NULL if unsuccessful
 */
xcom_input_request_ptr xcom_input_request_new(
    app_data_ptr a, xcom_input_reply_function_ptr reply_function,
    void *reply_arg);

/**
 * Frees the given request and its payload.
 *
 * @param request the request to free
 */
void xcom_input_request_free(xcom_input_request_ptr request);

/**
 * Links @c request to the list of requests @c next.
 *
 * @param request the request to link
 * @param next the list to be linked to
 */
void xcom_input_request_set_next(xcom_input_request_ptr request,
                                 xcom_input_request_ptr next);

/**
 * Unlinks @c request from its list.
 *
 * @param request the request to unlink
 * @returns the tail of the list the request was unlinked from
 */
xcom_input_request_ptr xcom_input_request_extract_next(
    xcom_input_request_ptr request);

/**
 * Extract the given request's payload.
 *
 * Transfers ownership of the result to the caller.
 *
 * @param request the request from which to extract the payload
 * @returns the request's app_data payload
 */
app_data_ptr xcom_input_request_extract_app_data(
    xcom_input_request_ptr request);

/**
 * Replies to the request using the strategy chosen by the request's origin.
 *
 * @param request the request to reply to
 * @param payload the payload of the reply
 */
void xcom_input_request_reply(xcom_input_request_ptr request, pax_msg *payload);

#endif /* XCOM_INPUT_REQUEST_H */
