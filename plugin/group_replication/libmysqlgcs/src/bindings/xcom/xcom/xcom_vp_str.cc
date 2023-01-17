/* Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

#include "xdr_gen/xcom_vp.h"

/* purecov: begin deadcode */
const char *delivery_status_to_str(delivery_status x) {
  switch (x) {
    case delivery_ok:
      return "delivery_ok";
    case delivery_failure:
      return "delivery_failure";
    default:
      return "???";
  }
}

const char *cons_type_to_str(cons_type x) {
  switch (x) {
    case cons_majority:
      return "cons_majority";
    case cons_all:
      return "cons_all";
    default:
      return "???";
  }
}

const char *cargo_type_to_str(cargo_type x) {
  switch (x) {
    case unified_boot_type:
      return "unified_boot_type";
    case xcom_boot_type:
      return "xcom_boot_type";
    case xcom_set_group:
      return "xcom_set_group";
    case app_type:
      return "app_type";
    case exit_type:
      return "exit_type";
    case reset_type:
      return "reset_type";
    case begin_trans:
      return "begin_trans";
    case prepared_trans:
      return "prepared_trans";
    case abort_trans:
      return "abort_trans";
    case view_msg:
      return "view_msg";
    case remove_reset_type:
      return "remove_reset_type";
    case add_node_type:
      return "add_node_type";
    case remove_node_type:
      return "remove_node_type";
    case enable_arbitrator:
      return "enable_arbitrator";
    case disable_arbitrator:
      return "disable_arbitrator";
    case force_config_type:
      return "force_config_type";
    case x_terminate_and_exit:
      return "x_terminate_and_exit";
    case set_cache_limit:
      return "set_cache_limit";
    case get_event_horizon_type:
      return "get_event_horizon_type";
    case set_event_horizon_type:
      return "set_event_horizon_type";
    case get_synode_app_data_type:
      return "get_synode_app_data_type";
    case convert_into_local_server_type:
      return "convert_into_local_server_type";
    case set_max_leaders:
      return "set_max_leaders";
    case set_leaders_type:
      return "set_leaders_type";
    case get_leaders_type:
      return "get_leaders_type";
    default:
      return "???";
  }
}

const char *recover_action_to_str(recover_action x) {
  switch (x) {
    case rec_block:
      return "rec_block";
    case rec_delay:
      return "rec_delay";
    case rec_send:
      return "rec_send";
    default:
      return "???";
  }
}

const char *pax_op_to_str(pax_op x) {
  switch (x) {
    case client_msg:
      return "client_msg";
    case initial_op:
      return "initial_op";
    case prepare_op:
      return "prepare_op";
    case ack_prepare_op:
      return "ack_prepare_op";
    case ack_prepare_empty_op:
      return "ack_prepare_empty_op";
    case accept_op:
      return "accept_op";
    case ack_accept_op:
      return "ack_accept_op";
    case learn_op:
      return "learn_op";
    case recover_learn_op:
      return "recover_learn_op";
    case multi_prepare_op:
      return "multi_prepare_op";
    case multi_ack_prepare_empty_op:
      return "multi_ack_prepare_empty_op";
    case multi_accept_op:
      return "multi_accept_op";
    case multi_ack_accept_op:
      return "multi_ack_accept_op";
    case multi_learn_op:
      return "multi_learn_op";
    case skip_op:
      return "skip_op";
    case i_am_alive_op:
      return "i_am_alive_op";
    case are_you_alive_op:
      return "are_you_alive_op";
    case need_boot_op:
      return "need_boot_op";
    case snapshot_op:
      return "snapshot_op";
    case die_op:
      return "die_op";
    case read_op:
      return "read_op";
    case gcs_snapshot_op:
      return "gcs_snapshot_op";
    case xcom_client_reply:
      return "xcom_client_reply";
    case tiny_learn_op:
      return "tiny_learn_op";
    case synode_request:
      return "synode_request";
    case synode_allocated:
      return "synode_allocated";
    case LAST_OP:
      return "LAST_OP";
    default:
      return "???";
  }
}

const char *pax_msg_type_to_str(pax_msg_type x) {
  switch (x) {
    case normal:
      return "normal";
    case no_op:
      return "no_op";
    case multi_no_op:
      return "multi_no_op";
    default:
      return "???";
  }
}

const char *client_reply_code_to_str(client_reply_code x) {
  switch (x) {
    case REQUEST_OK:
      return "REQUEST_OK";
    case REQUEST_FAIL:
      return "REQUEST_FAIL";
    case REQUEST_RETRY:
      return "REQUEST_RETRY";
    case REQUEST_REDIRECT:
      return "REQUEST_REDIRECT";
    default:
      return "???";
  }
}

const char *xcom_proto_to_str(xcom_proto x) {
  switch (x) {
    case x_unknown_proto:
      return "x_unknown_proto";
    case x_1_0:
      return "x_1_0";
    case x_1_1:
      return "x_1_1";
    case x_1_2:
      return "x_1_2";
    case x_1_3:
      return "x_1_3";
    case x_1_4:
      return "x_1_4";
    case x_1_5:
      return "x_1_5";
    case x_1_6:
      return "x_1_6";
    case x_1_7:
      return "x_1_7";
    case x_1_8:
      return "x_1_8";
    case x_1_9:
      return "x_1_9";
    default:
      return "???";
  }
}
/* purecov: end */
