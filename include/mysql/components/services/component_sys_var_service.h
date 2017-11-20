/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef COMPONENT_SYS_VAR_SERVICE_H
#define COMPONENT_SYS_VAR_SERVICE_H

#include <stddef.h>

#include <mysql/components/service.h>

/**
  Component system variables as a service to mysql_server component
*/

class THD;
#define MYSQL_THD THD*

typedef int (*mysql_sys_var_check_func) (MYSQL_THD thd,
                                         struct st_mysql_sys_var *var,
                                         void *save,
                                         struct st_mysql_value *value);

typedef void (*mysql_sys_var_update_func) (MYSQL_THD thd,
                                           struct st_mysql_sys_var *var,
                                           void *var_ptr,
                                           const void *save);

#define COPY_MYSQL_PLUGIN_VAR_HEADER(sys_var_type, type,            \
                                     sys_var_check, sys_var_update) \
  sys_var_type->flags= flags;                                       \
  sys_var_type->name= var_name;                                     \
  sys_var_type->comment= comment;                                   \
  sys_var_type->check= check_func ? check_func : sys_var_check;     \
  sys_var_type->update= update_func ? update_func : sys_var_update; \
  sys_var_type->value= (type *) variable_value;

#define COPY_MYSQL_PLUGIN_VAR_REMAINING(sys_var_type, check_arg_type) \
  sys_var_type->def_val= check_arg_type->def_val;                     \
  sys_var_type->min_val= check_arg_type->min_val;                     \
  sys_var_type->max_val= check_arg_type->max_val;                     \
  sys_var_type->blk_sz= check_arg_type->blk_sz;


#define SYSVAR_INTEGRAL_TYPE(type) struct sysvar_ ## type ## _type {  \
  MYSQL_PLUGIN_VAR_HEADER;        \
  type *value;                    \
  type def_val;                   \
  type min_val;                   \
  type max_val;                   \
  type blk_sz;                    \
}

#define SYSVAR_ENUM_TYPE(type) struct sysvar_ ## type ## _type {  \
  MYSQL_PLUGIN_VAR_HEADER;        \
  unsigned long *value;           \
  unsigned long def_val;          \
  TYPELIB *typelib;               \
}

#define SYSVAR_BOOL_TYPE(type) struct sysvar_ ## type ## _type {  \
  MYSQL_PLUGIN_VAR_HEADER;        \
  bool *value;                    \
  bool def_val;                   \
}

#define SYSVAR_STR_TYPE(type) struct sysvar_ ## type ## _type {  \
  MYSQL_PLUGIN_VAR_HEADER;        \
  char **value;                   \
  char *def_val;                  \
}

#define INTEGRAL_CHECK_ARG(type) struct type ## _check_arg_s {  \
  type def_val;                   \
  type min_val;                   \
  type max_val;                   \
  type blk_sz;                    \
}

#define ENUM_CHECK_ARG(type) struct type ## _check_arg_s {  \
  unsigned long def_val;          \
  TYPELIB *typelib;               \
}

#define BOOL_CHECK_ARG(type) struct type ## _check_arg_s {  \
  bool def_val;             \
}

#define STR_CHECK_ARG(type) struct type ## _check_arg_s {  \
  char *def_val;            \
}

/**
  Service to register variable and get variable value
*/
BEGIN_SERVICE_DEFINITION(component_sys_variable_register)

  DECLARE_BOOL_METHOD(register_variable,
  (const char *component_name,
   const char *name,
   int flags,
   const char *comment,
   mysql_sys_var_check_func check,
   mysql_sys_var_update_func update,
   void *check_arg,
   void *variable_value));

  DECLARE_BOOL_METHOD(get_variable,
  (const char *component_name,
   const char *name, void **val,
   size_t *out_length_of_val));

END_SERVICE_DEFINITION(component_sys_variable_register)

/**
  Service to unregister variable
*/
BEGIN_SERVICE_DEFINITION(component_sys_variable_unregister)

  DECLARE_BOOL_METHOD(unregister_variable,
  (const char *component_name,
   const char *name));

END_SERVICE_DEFINITION(component_sys_variable_unregister)

#endif /* COMPONENT_SYS_VAR_SERVICE_H */
