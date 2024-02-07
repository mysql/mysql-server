/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef E_T_S
#define E_T_S
extern const char *delivery_status_to_str(delivery_status x);
extern const char *cons_type_to_str(cons_type x);
extern const char *cargo_type_to_str(cargo_type x);
extern const char *recover_action_to_str(recover_action x);
extern const char *pax_op_to_str(pax_op x);
extern const char *pax_msg_type_to_str(pax_msg_type x);
extern const char *client_reply_code_to_str(client_reply_code x);
extern const char *xcom_proto_to_str(xcom_proto x);
#endif
