/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef APP_DATA_H
#define APP_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#define copy_app_data(target, source) {MAY_DBG(FN; STRLIT(" copy_app_data "); PTREXP(target); PTREXP(*target); PTREXP(source)); _replace_app_data_list(target, source);}

#define steal_app_data(target, source) {(target) = (source); (source) = NULL; }

app_data_ptr clone_app_data(app_data_ptr a);
app_data_ptr clone_app_data_single(app_data_ptr a);
app_data_ptr new_app_data();
app_data_ptr init_app_data(app_data_ptr a);

app_data_ptr new_data(u_int n, char *val, cons_type consensus);
app_data_ptr new_exit();
app_data_ptr new_nodes(u_int n, node_address *names, cargo_type cargo);
app_data_ptr new_reset(cargo_type type);

void _replace_app_data_list(app_data_list target, app_data_ptr source);
char *dbg_app_data(app_data_ptr a);
void follow(app_data_list l, app_data_ptr p);
void sort_app_data(app_data_ptr x[], int n);
size_t app_data_size(app_data const *a);
size_t app_data_list_size(app_data const *a);


#ifdef __cplusplus
}
#endif

#endif

