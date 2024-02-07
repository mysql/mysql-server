/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_BITS_SYSTEM_VARIABLES_BITS_H
#define COMPONENTS_SERVICES_BITS_SYSTEM_VARIABLES_BITS_H

/**
  @addtogroup group_components_services_sys_var_service_types Variable types

  Possible system variable types. Use at most one of these.

  @sa mysql_service_component_sys_variable_register service.

  @{
*/

/** bool variable. Use @ref BOOL_CHECK_ARG */
#define PLUGIN_VAR_BOOL 0x0001
/** int variable. Use @ref INTEGRAL_CHECK_ARG */
#define PLUGIN_VAR_INT 0x0002
/** long variable Use @ref INTEGRAL_CHECK_ARG */
#define PLUGIN_VAR_LONG 0x0003
/** longlong variable. Use @ref INTEGRAL_CHECK_ARG */
#define PLUGIN_VAR_LONGLONG 0x0004
/** char * variable. Use @ref STR_CHECK_ARG */
#define PLUGIN_VAR_STR 0x0005
/** Enum variable. Use @ref ENUM_CHECK_ARG */
#define PLUGIN_VAR_ENUM 0x0006
/** A set variable. Use @ref ENUM_CHECK_ARG */
#define PLUGIN_VAR_SET 0x0007
/** double variable. Use @ref INTEGRAL_CHECK_ARG */
#define PLUGIN_VAR_DOUBLE 0x0008
/** @} */
/**
  @addtogroup group_components_services_sys_var_service_flags Variable flags

  Flags to specify the behavior of system variables. Use multiple as needed.

  @sa mysql_service_component_sys_variable_register service.

  @{
*/
#define PLUGIN_VAR_UNSIGNED 0x0080  /**< The variable is unsigned */
#define PLUGIN_VAR_THDLOCAL 0x0100  /**< Variable is per-connection */
#define PLUGIN_VAR_READONLY 0x0200  /**< Server variable is read only */
#define PLUGIN_VAR_NOSYSVAR 0x0400  /**< Not a server variable */
#define PLUGIN_VAR_NOCMDOPT 0x0800  /**< Not a command line option */
#define PLUGIN_VAR_NOCMDARG 0x1000  /**< No argument for cmd line */
#define PLUGIN_VAR_RQCMDARG 0x0000  /**< Argument required for cmd line */
#define PLUGIN_VAR_OPCMDARG 0x2000  /**< Argument optional for cmd line */
#define PLUGIN_VAR_NODEFAULT 0x4000 /**< SET DEFAULT is prohibited */
#define PLUGIN_VAR_MEMALLOC 0x8000  /**< String needs memory allocated */
#define PLUGIN_VAR_NOPERSIST \
  0x10000 /**< SET PERSIST_ONLY is prohibited for read only variables */
#define PLUGIN_VAR_PERSIST_AS_READ_ONLY 0x20000
#define PLUGIN_VAR_INVISIBLE 0x40000 /**< Variable should not be shown */
#define PLUGIN_VAR_SENSITIVE 0x80000 /**< Sensitive variable */
/** @} */

/**
  st_mysql_value struct for reading values from mysqld.
  Used by server variables framework to parse user-provided values.
  Will be used for arguments when implementing UDFs.

  Note that val_str() returns a string in temporary memory
  that will be freed at the end of statement. Copy the string
  if you need it to persist.
*/

#define MYSQL_VALUE_TYPE_STRING 0
#define MYSQL_VALUE_TYPE_REAL 1
#define MYSQL_VALUE_TYPE_INT 2

struct st_mysql_value {
  int (*value_type)(struct st_mysql_value *);
  const char *(*val_str)(struct st_mysql_value *, char *buffer, int *length);
  int (*val_real)(struct st_mysql_value *, double *realbuf);
  int (*val_int)(struct st_mysql_value *, long long *intbuf);
  int (*is_unsigned)(struct st_mysql_value *);
};

#endif /* COMPONENTS_SERVICES_BITS_SYSTEM_VARIABLES_BITS_H */
