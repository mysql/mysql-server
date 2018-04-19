/* Copyright (c) 2017, 2018 Oracle and/or its affiliates. All rights reserved.

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

#define LOG_COMPONENT_TAG "pfs_example_plugin_employee"

#include <mysql/plugin.h>
#include <mysql_version.h>

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include "plugin/pfs_table_plugin/pfs_example_employee_name.h"
#include "plugin/pfs_table_plugin/pfs_example_employee_salary.h"
#include "plugin/pfs_table_plugin/pfs_example_machine.h"
#include "plugin/pfs_table_plugin/pfs_example_machines_by_emp_by_mtype.h"

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

/**
  @page EXAMPLE_PLUGIN An example plugin

  Plugin Name     : pfs_example_plugin_employee \n
  Source location : plugin/pfs_table_plugin

  This file contains a definition of the pfs_example_plugin_employee.
*/

/* clang-format off */
/* Records to be inserted into pfs_example_employee_name table from plugin code */
Ename_Record ename_array[] =
{
  {{1, false}, "foo1", 4, "bar1", 4, true},
  {{2, false}, "foo2", 4, "bar2", 4, true},
  {{3, false}, "foo3", 4, "bar3", 4, true}
};

/* Records to be inserted into pfs_example_employee_salary table from plugin code */
Esalary_Record esalary_array[] =
{
  {{1, false}, {1000, false}, "2013-11-12", 10, "12:02:34", 8, true},
  {{2, false}, {2000, false}, "2016-02-29", 10, "12:12:30", 8, true},
  {{3, false}, {3000, false}, "2017-03-24", 10, "11:12:50", 8, true}
};

/* Records to be inserted into pfs_example_machine table from plugin code */
Machine_Record machine_array[] =
{
  {{1, false}, {DESKTOP, false}, "Lenovo", 6, {1, false}, true},
  {{2, false}, {LAPTOP, false}, "Dell", 4, {2, false}, true},
  {{3, false}, {MOBILE, false}, "Apple", 5, {1, false}, true},
  {{4, false}, {MOBILE, false}, "Samsung", 7, {1, false}, true},
  {{5, false}, {LAPTOP, false}, "Lenovo", 6, {2, false}, true},
  {{6, false}, {MOBILE, false}, "Nokia", 5, {2, false}, true},
  {{7, false}, {LAPTOP, false}, "Apple", 5, {1, false}, true},
  {{8, false}, {LAPTOP, false}, "HP", 2, {3, false}, true},
  {{9, false}, {DESKTOP, false}, "Apple", 5, {3, false}, true},
};
/* clang-format off */

/* Global handles */
SERVICE_TYPE(registry) *r = NULL;
my_h_service h_ret_table_svc = NULL;
SERVICE_TYPE(pfs_plugin_table) *table_svc = NULL;

/* Collection of table shares to be added to performance schema */
PFS_engine_table_share_proxy* share_list[4]= {NULL, NULL, NULL, NULL};
unsigned int share_list_count= 4;

/**
* acquire_service_handles does following:
*   - Acquire the registry service for mysql_server.
*   - Acquire pfs_plugin_table service implementation.
*/
bool
acquire_service_handles(MYSQL_PLUGIN p MY_ATTRIBUTE((unused)))
{
  bool result = false;

  /* Acquire mysql_server's registry service */
  r = mysql_plugin_registry_acquire();
  if (!r)
  {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "mysql_plugin_registry_acquire() returns empty");
    result = true;
    goto error;
  }

  /* Acquire pfs_plugin_table service */
  if (r->acquire("pfs_plugin_table", &h_ret_table_svc))
  {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "can't find pfs_plugin_table service");
    result = true;
    goto error;
  }

  /* Type cast this handler to proper service handle */
  table_svc =
    reinterpret_cast<SERVICE_TYPE(pfs_plugin_table) *>(h_ret_table_svc);

error:
  return result;
}

/**
* release_service_handles does following:
*   - Release the handle to the pfs_plugin_table service.
*   - Release the handle to registry service.
*/
void
release_service_handles()
{
  if (r != NULL)
  {
    if (h_ret_table_svc != NULL)
    {
      /* Release pfs_plugin_table and pfs_plugin_table services */
      r->release(h_ret_table_svc);
      h_ret_table_svc = NULL;
      table_svc = NULL;
    }
    /* Release registry service */
    mysql_plugin_registry_release(r);
    r = NULL;
  }
}

/* Prepare and insert rows in pfs_example_employee_name table */
int
ename_prepare_insert_row()
{
  int result = 0;
  Ename_Table_Handle handle;
  int array_size= sizeof(ename_array) / sizeof(ename_array[0]);

  for (int i = 0; i < array_size; i++)
  {
    strncpy(handle.current_row.f_name, ename_array[i].f_name,
        ename_array[i].f_name_length);
    handle.current_row.f_name_length = ename_array[i].f_name_length;
    strncpy(handle.current_row.l_name, ename_array[i].l_name,
        ename_array[i].l_name_length);
    handle.current_row.l_name_length = ename_array[i].l_name_length;
    handle.current_row.e_number = ename_array[i].e_number;
    handle.current_row.m_exist = ename_array[i].m_exist;

    /* Insert a row in the table to be added */
    result = ename_write_row_values((PSI_table_handle *)&handle);

    if (result)
      break;
  }

  return result;
}

/* Prepare and insert rows in pfs_example_employee_salary table */
int
esalary_prepare_insert_row()
{
  int result = 0;
  Esalary_Table_Handle handle;
  int array_size= sizeof(esalary_array) / sizeof(esalary_array[0]);

  for (int i = 0; i < array_size; i++)
  {
    strncpy(handle.current_row.e_dob, esalary_array[i].e_dob,
        esalary_array[i].e_dob_length);
    handle.current_row.e_dob_length= esalary_array[i].e_dob_length;

    strncpy(handle.current_row.e_tob, esalary_array[i].e_tob,
        esalary_array[i].e_tob_length);
    handle.current_row.e_tob_length= esalary_array[i].e_tob_length;

    handle.current_row.e_number = esalary_array[i].e_number;
    handle.current_row.e_salary = esalary_array[i].e_salary;

    handle.current_row.m_exist = esalary_array[i].m_exist;

    /* Insert a row in the table to be added */
    result = esalary_write_row_values((PSI_table_handle *)&handle);
    if (result)
      break;
  }

  return result;
}

/* Prepare and insert rows in pfs_example_machine table */
int
machine_prepare_insert_row()
{
  int result = 0;
  Machine_Table_Handle handle;
  int array_size= sizeof(machine_array) / sizeof(machine_array[0]);

  for (int i = 0; i < array_size; i++)
  {
    handle.current_row.machine_number = machine_array[i].machine_number;
    strncpy(handle.current_row.machine_made, machine_array[i].machine_made,
      machine_array[i].machine_made_length);
    handle.current_row.machine_made_length = machine_array[i].machine_made_length;
    handle.current_row.machine_type = machine_array[i].machine_type;
    handle.current_row.employee_number = machine_array[i].employee_number;

    handle.current_row.m_exist = machine_array[i].m_exist;

    /* Insert a row in the table to be added */
    result = machine_write_row_values((PSI_table_handle *)&handle);
    if (result)
      break;
  }

  return result;
}

/**
*  pfs_example_func does following :
*    - Instantiate PFS_engine_table_share_proxy(s).
*    - Prepare and insert rows in tables from here.
*    - Acquire pfs_plugin_table service handle.
*    - Call add_table method of pfs_plugin_table service.

*  Error messages are written to the server's error log.
*  In case of success writes a single information message to the server's log.

*  @retval false  success
*  @retval true   failure
*/
static bool
pfs_example_func(MYSQL_PLUGIN p)
{
  bool result = false;

  /* Instantiate and initialize PFS_engine_table_share_proxy */
  init_ename_share(&ename_st_share);
  init_esalary_share(&esalary_st_share);
  init_machine_share(&machine_st_share);
  init_m_by_emp_by_mtype_share(&m_by_emp_by_mtype_st_share);

  /* From here, prepare rows for tables and insert */
  if (ename_prepare_insert_row() ||
      esalary_prepare_insert_row() ||
      machine_prepare_insert_row())
  {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Error returned during prepare and insert row.");
    result = true;
    goto error;
  }

  /* Get pfs_plugin_table service handle. */
  result = acquire_service_handles(p);
  if (result)
    goto error;

  /* Prepare the shares list to be passed to the service call */
  share_list[0]= &ename_st_share;
  share_list[1]= &esalary_st_share;
  share_list[2]= &machine_st_share;
  share_list[3]= &m_by_emp_by_mtype_st_share;

  /**
   * Call add_tables function of pfs_plugin_table service to
   * add plugin tables in performance schema.
   */
  if (table_svc->add_tables(&share_list[0], share_list_count))
  {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Error returned from add_tables()");
    result = true;
    goto error;
  }

  return result;

error:
  /* Release service handles. */
  release_service_handles();
  return result;
}

/**
*  Initialize the pfs_example_plugin_employee at server start or plugin
*  installation.
*
*   - Call pfs_example_func.
*/

static int
pfs_example_plugin_employee_init(void *p)
{
  DBUG_ENTER("pfs_example_plugin_employee_init");
  int result = 0;

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs))
    DBUG_RETURN(1);

  /* Initialize mutexes to be used for table records */
  mysql_mutex_init(0, &LOCK_ename_records_array, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(0, &LOCK_esalary_records_array, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(0, &LOCK_machine_records_array, MY_MUTEX_INIT_FAST);

  /* In case the plugin has been unloaded, and reloaded */
  ename_delete_all_rows();
  esalary_delete_all_rows();
  machine_delete_all_rows();

  result = pfs_example_func(reinterpret_cast<MYSQL_PLUGIN>(p)) ? 1 : 0;

  if (result)
  {
    /* Destroy mutexes for table records */
    mysql_mutex_destroy(&LOCK_ename_records_array);
    mysql_mutex_destroy(&LOCK_esalary_records_array);
    mysql_mutex_destroy(&LOCK_machine_records_array);
  }

  DBUG_RETURN(result);
}

static int
pfs_example_plugin_employee_check(void *)
{
  DBUG_ENTER("pfs_example_plugin_employee_check");

  if (table_svc != NULL)
  {
    if (table_svc->delete_tables(&share_list[0], share_list_count))
    {
      /* Block execution of UNINSTALL PLUGIN. */
      DBUG_RETURN(1);
    }
  }

  DBUG_RETURN(0);
}

/**
*  Terminate the pfs_example_plugin_employee at server shutdown or plugin
*  deinstallation.
*
*   - Delete/Drop plugin tables from Performance Schema.
*   - Release pfs_plugin_table service handle.
*/
static int
pfs_example_plugin_employee_deinit(void *p  MY_ATTRIBUTE((unused)))
{
  DBUG_ENTER("pfs_example_plugin_employee_deinit");

  /**
   * Call delete_tables function of pfs_plugin_table service to
   * delete plugin tables from performance schema
   */
  if (table_svc != NULL)
  {
    if (table_svc->delete_tables(&share_list[0], share_list_count))
    {
      LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                   "Error returned from delete_tables()");
      deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
      DBUG_RETURN(1);
    }
  }
  else /* Service not found or released */
  {
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    DBUG_RETURN(1);
  }

  /* Destroy mutexes for table records */
  mysql_mutex_destroy(&LOCK_ename_records_array);
  mysql_mutex_destroy(&LOCK_esalary_records_array);
  mysql_mutex_destroy(&LOCK_machine_records_array);

  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);

  /* Release service handles. */
  release_service_handles();

  DBUG_RETURN(0);
}

static struct st_mysql_daemon pfs_example_plugin_employee = {
  MYSQL_DAEMON_INTERFACE_VERSION};

/**
  pfs_example_plugin_employee plugin descriptor
*/

/* clang-format off */
mysql_declare_plugin(pfs_example_plugin_employee)
{
  MYSQL_DAEMON_PLUGIN,
  &pfs_example_plugin_employee,
  "pfs_example_plugin_employee",
  "Oracle Corporation",
  "pfs_example_plugin_employee",
  PLUGIN_LICENSE_GPL,
  pfs_example_plugin_employee_init,   /* Plugin Init      */
  pfs_example_plugin_employee_check,  /* Plugin Check uninstall */
  pfs_example_plugin_employee_deinit, /* Plugin Deinit    */
  0x0100 /* 1.0 */, NULL,             /* status variables */
  NULL,                               /* system variables */
  NULL,                               /* config options   */
  0,                                  /* flags            */
}
mysql_declare_plugin_end;
/* clang-format on */
