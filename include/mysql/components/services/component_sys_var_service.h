/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COMPONENT_SYS_VAR_SERVICE_H
#define COMPONENT_SYS_VAR_SERVICE_H

#include <stddef.h>

#include <mysql/components/service.h>

/**
  Component system variables as a service to mysql_server component
*/

struct TYPE_LIB
{     /* Different types saved here */
  /*
    These constructors are no longer needed when we go to C++14, where
    aggregate initialization is allowed on classes that have default
    member initializers.
  */
  TYPE_LIB() {}

  TYPE_LIB(size_t count_arg, const char *name_arg,
          const char **type_names_arg, unsigned int *type_lengths_arg)
    : count(count_arg), name(name_arg),
      type_names(type_names_arg), type_lengths(type_lengths_arg)
  {
  }

  size_t count{0};                 /* How many types */
  const char *name{nullptr};             /* Name of typelib */
  const char **type_names{nullptr};
  unsigned int *type_lengths{nullptr};
};

/*
  declarations for server variables and command line options
*/
#define PLUGIN_VAR_BOOL         0x0001
#define PLUGIN_VAR_INT          0x0002
#define PLUGIN_VAR_LONG         0x0003
#define PLUGIN_VAR_LONGLONG     0x0004
#define PLUGIN_VAR_STR          0x0005
#define PLUGIN_VAR_ENUM         0x0006
#define PLUGIN_VAR_SET          0x0007
#define PLUGIN_VAR_DOUBLE       0x0008
#define PLUGIN_VAR_UNSIGNED     0x0080
#define PLUGIN_VAR_THDLOCAL     0x0100 /* Variable is per-connection */
#define PLUGIN_VAR_READONLY     0x0200 /* Server variable is read only */
#define PLUGIN_VAR_NOSYSVAR     0x0400 /* Not a server variable */
#define PLUGIN_VAR_NOCMDOPT     0x0800 /* Not a command line option */
#define PLUGIN_VAR_NOCMDARG     0x1000 /* No argument for cmd line */
#define PLUGIN_VAR_RQCMDARG     0x0000 /* Argument required for cmd line */
#define PLUGIN_VAR_OPCMDARG     0x2000 /* Argument optional for cmd line */
#define PLUGIN_VAR_NODEFAULT    0x4000 /* SET DEFAULT is prohibited */
#define PLUGIN_VAR_MEMALLOC     0x8000 /* String needs memory allocated */
#define PLUGIN_VAR_NOPERSIST    0x10000 /* SET PERSIST_ONLY is prohibited
                                           for read only variables */

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
  TYPE_LIB *typelib;              \
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
  TYPE_LIB *typelib;              \
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
