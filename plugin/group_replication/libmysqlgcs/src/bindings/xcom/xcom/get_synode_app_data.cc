/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include <stdlib.h> /* calloc */
#ifdef _MSC_VER
#include <stdint.h>
#endif

#include "xcom/checked_data.h"
#include "xcom/get_synode_app_data.h"
#include "xcom/synode_no.h"  /* synode_eq */
#include "xcom/xcom_base.h"  /* pm_finished */
#include "xcom/xcom_cache.h" /* pax_machine, hash_get */
#include "xcom/xcom_memory.h"

static xcom_get_synode_app_data_result can_satisfy_request(
    synode_no_array const *const synodes);
static xcom_get_synode_app_data_result have_decided_synode_app_data(
    synode_no const *const synode);

static xcom_get_synode_app_data_result prepare_reply(
    synode_no_array const *const synodes, synode_app_data_array *const reply);
static xcom_get_synode_app_data_result copy_all_synode_app_data_to_reply(
    synode_no_array const *const synodes, synode_app_data_array *const reply);
static xcom_get_synode_app_data_result copy_synode_app_data_to_reply(
    synode_no const *const synode, synode_app_data *const reply);

xcom_get_synode_app_data_result xcom_get_synode_app_data(
    synode_no_array const *const synodes, synode_app_data_array *const reply) {
  xcom_get_synode_app_data_result error_code = XCOM_GET_SYNODE_APP_DATA_ERROR;

  /*
   These should always be FALSE, but rather than asserting, treat as failure if
   they are not.
   */
  if (reply->synode_app_data_array_len != 0) goto end;
  if (reply->synode_app_data_array_val != nullptr) goto end;

  error_code = can_satisfy_request(synodes);
  if (error_code != XCOM_GET_SYNODE_APP_DATA_OK) goto end;

  error_code = prepare_reply(synodes, reply);

end:
  return error_code;
}

/*
 Check whether we can satisfy a get_synode_app_data request for the given
 synodes:

 - They must be cached
 - They must be decided
 */
static xcom_get_synode_app_data_result can_satisfy_request(
    synode_no_array const *const synodes) {
  xcom_get_synode_app_data_result error_code = XCOM_GET_SYNODE_APP_DATA_ERROR;

  {
    u_int const nr_synodes = synodes->synode_no_array_len;
    u_int index;
    for (index = 0; index < nr_synodes; index++) {
      synode_no const *const synode = &synodes->synode_no_array_val[index];

      error_code = have_decided_synode_app_data(synode);
      if (error_code != XCOM_GET_SYNODE_APP_DATA_OK) goto end;
    }
  }

  error_code = XCOM_GET_SYNODE_APP_DATA_OK;

end:
  return error_code;
}

/*
 Check if the given synode is cached and decided.
 */
static xcom_get_synode_app_data_result have_decided_synode_app_data(
    synode_no const *const synode) {
  xcom_get_synode_app_data_result error_code = XCOM_GET_SYNODE_APP_DATA_ERROR;
  bool_t is_decided = FALSE;

  pax_machine *paxos = hash_get(*synode);
  bool_t const is_cached = (paxos != nullptr);
  if (!is_cached) {
    error_code = XCOM_GET_SYNODE_APP_DATA_NOT_CACHED;
    goto end;
  }

  is_decided = (pm_finished(paxos) == 1);
  if (!is_decided) {
    error_code = XCOM_GET_SYNODE_APP_DATA_NOT_DECIDED;
    goto end;
  }

  /*
   These should always be FALSE, but rather than asserting, treat as failure if
   they are not.
   */
  if (synode_eq(paxos->learner.msg->synode, *synode) != 1) goto end;
  if (paxos->learner.msg->a->body.c_t != app_type) goto end;

  error_code = XCOM_GET_SYNODE_APP_DATA_OK;

end:
  return error_code;
}

/*
 Allocate the reply of the get_synode_app_data request and copy the app_datas
 of the given synodes to reply.
 */
static xcom_get_synode_app_data_result prepare_reply(
    synode_no_array const *const synodes, synode_app_data_array *const reply) {
  xcom_get_synode_app_data_result error_code = XCOM_GET_SYNODE_APP_DATA_ERROR;
  u_int const nr_synodes = synodes->synode_no_array_len;

  reply->synode_app_data_array_val =
      (synode_app_data *)xcom_calloc(nr_synodes, sizeof(synode_app_data));
  if (reply->synode_app_data_array_val == nullptr) {
    /* purecov: begin inspected */
    error_code = XCOM_GET_SYNODE_APP_DATA_NO_MEMORY;
    goto end;
    /* purecov: end */
  }
  reply->synode_app_data_array_len = nr_synodes;

  error_code = copy_all_synode_app_data_to_reply(synodes, reply);

end:
  return error_code;
}

/*
 Copy the app_datas of the given synodes to reply.
 */
static xcom_get_synode_app_data_result copy_all_synode_app_data_to_reply(
    synode_no_array const *const synodes, synode_app_data_array *const reply) {
  xcom_get_synode_app_data_result error_code = XCOM_GET_SYNODE_APP_DATA_ERROR;
  u_int const nr_synodes = synodes->synode_no_array_len;
  u_int index;
  for (index = 0; index < nr_synodes; index++) {
    synode_no const *const synode = &synodes->synode_no_array_val[index];
    synode_app_data *const reply_entry =
        &reply->synode_app_data_array_val[index];

    error_code = copy_synode_app_data_to_reply(synode, reply_entry);
    if (error_code != XCOM_GET_SYNODE_APP_DATA_OK) goto end;
  }

  error_code = XCOM_GET_SYNODE_APP_DATA_OK;

end:
  return error_code;
}

/*
 Copy the app_data of the given synode to reply.
 */
static xcom_get_synode_app_data_result copy_synode_app_data_to_reply(
    synode_no const *const synode, synode_app_data *const reply) {
  xcom_get_synode_app_data_result error_code = XCOM_GET_SYNODE_APP_DATA_ERROR;

  pax_machine const *paxos = hash_get(*synode);
  pax_msg const *p = paxos->learner.msg;
  checked_data const *cached_data = &p->a->body.app_u_u.data;

  reply->synode = *synode;
  reply->origin = p->a->unique_id;

  /*
   We need to copy because by the time the reply is sent, the cache may have
   been modified.
  */
  {
    bool_t const copied = copy_checked_data(&reply->data, cached_data);
    if (copied) {
      error_code = XCOM_GET_SYNODE_APP_DATA_OK;
    } else {
      /* purecov: begin inspected */
      error_code = XCOM_GET_SYNODE_APP_DATA_NO_MEMORY;
      /* purecov: end */
    }
  }

  return error_code;
}
