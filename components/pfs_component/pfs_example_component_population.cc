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

#include <cstring>
#include "pfs_example_component_population.h"
#include "pfs_example_continent.h"
#include "pfs_example_country.h"
#include "my_sys.h"

/**
  @page EXAMPLE_COMPONENT An example component

  Component Name  : pfs_example_component_population \n 
  Source location : components/pfs_component

  This file contains a definition of the pfs_example_component_population.
*/

/* clang-format off */
/* Records to be inserted into pfs_example_continent table from component code */
Continent_record continent_array[] =
{
    {"bar1", 4, true},
    {"bar2", 4, true}
};

/* Records to be inserted into pfs_example_country table from component code */
Country_record country_array[] =
{
    {"foo1", 4, "bar1", 4, {2016, false}, {10000, false}, {1.11, false}, true},
    {"foo2", 4, "bar2", 4, {2016, false}, {1000, false}, {2.22, false}, true}
};
/* clang-format on */

#define MAX_BUFFER_LENGTH 80

#define WRITE_LOG(lit_log_text)                                         \
  if (outfile)                                                          \
  {                                                                     \
    strcpy (log_text, lit_log_text);                                    \
    fwrite((uchar*)log_text, sizeof(char), strlen(log_text), outfile);  \
  }

/* Log file */
FILE *outfile = NULL;
const char *filename= "pfs_example_component_population.log";
char log_text[MAX_BUFFER_LENGTH] = {'\0'};

/* Collection of table shares to be added to performance schema */
PFS_engine_table_share_proxy *share_list[2] = {NULL, NULL};
unsigned int share_list_count = 2;

/* Prepare and insert rows in pfs_example_continent table */
int
continent_prepare_insert_row()
{
  int result = 0;
  Continent_Table_Handle handle;
  int array_size = sizeof(continent_array) / sizeof(continent_array[0]);

  for (int i = 0; i < array_size; i++)
  {
    /* Prepare a sample row to be inserted from here */
    strncpy(handle.current_row.name,
            continent_array[i].name,
            continent_array[i].name_length);
    handle.current_row.name_length = continent_array[i].name_length;
    handle.current_row.m_exist = continent_array[i].m_exist;

    /* Insert a row in the table to be added */
    result = write_rows_from_component(&handle);
    if (result)
      break;
  }

  return result;
}

/* Prepare and insert rows in pfs_example_country table */
int
country_prepare_insert_row()
{
  int result = 0;
  Country_Table_Handle handle;
  int array_size = sizeof(country_array) / sizeof(country_array[0]);

  for (int i = 0; i < array_size; i++)
  {
    /* Prepare a sample row to be inserted from here */
    strncpy(handle.current_row.name,
            country_array[i].name,
            country_array[i].name_length);
    handle.current_row.name_length = country_array[i].name_length;
    strncpy(handle.current_row.continent_name,
            country_array[i].continent_name,
            country_array[i].continent_name_length);
    handle.current_row.continent_name_length =
      country_array[i].continent_name_length;
    handle.current_row.year = country_array[i].year;
    handle.current_row.population = country_array[i].population;
    handle.current_row.growth_factor = country_array[i].growth_factor;
    handle.current_row.m_exist = country_array[i].m_exist;

    /* Insert a row in the table to be added */
    result = country_write_row_values((PSI_table_handle *)&handle);
    if (result)
      break;
  }

  return result;
}

/**
*  Initialize the pfs_example_component_population at server start or
*  component installation.
*
*    - Instantiate and initialize PFS_engine_table_share_proxy.
*    - Prepare and insert rows in tables from here.
*    - Call add_table method of pfs_plugin_table service.
*
*  @retval 0  success
*  @retval non-zero   failure
*/

mysql_service_status_t
pfs_example_component_population_init()
{
  mysql_service_status_t result = 0;
  /* If fopen fails, outfile will be NULL so there will be no write
     in WRITE_LOG
  */
  outfile= fopen(filename, "w+");

  WRITE_LOG("pfs_example_component_population init:\n");

  /* Initialize mutex */
  native_mutex_init(&LOCK_continent_records_array, NULL);
  native_mutex_init(&LOCK_country_records_array, NULL);

  /* Instantiate and initialize PFS_engine_table_share_proxy */
  init_continent_share(&continent_st_share);
  init_country_share(&country_st_share);

  /* In case the plugin has been unloaded, and reloaded */
  continent_delete_all_rows();
  country_delete_all_rows();

  /* From here, prepare rows for tables and insert */
  if (continent_prepare_insert_row() || country_prepare_insert_row())
  {
    WRITE_LOG("Error returned from prepare_insert_row()\n");
    result = true;
    goto error;
  }

  /* Prepare the shares list to be passed to the service call */
  share_list[0] = &continent_st_share;
  share_list[1] = &country_st_share;

  /**
   * Call add_table function of pfs_plugin_table service to
   * add component tables in performance schema.
   */
  if (mysql_service_pfs_plugin_table->add_tables(&share_list[0],
                                                 share_list_count))
  {
    WRITE_LOG("Error returned from add_tables()\n");
    result = true;
    goto error;
  }
  else
  {
    WRITE_LOG ("Passed add_tables()\n");
  }

error:
  WRITE_LOG ("End of init\n\n");
  fclose(outfile);
  if (result)
  {
    /* Destroy mutexes */
    native_mutex_destroy(&LOCK_continent_records_array);
    native_mutex_destroy(&LOCK_country_records_array);
  }
  return result;
}

/**
*  Terminate the pfs_example_component_population at server shutdown or
*  component deinstallation.
*
*   - Delete/Drop component tables from Performance Schema.
*
*  @retval 0  success
*  @retval non-zero   failure
*/
mysql_service_status_t
pfs_example_component_population_deinit()
{
  mysql_service_status_t result = 0;
  /* If fopen fails, outfile will be NULL so there will be no write
     in WRITE_LOG
  */
  outfile= fopen(filename, "a+");

  WRITE_LOG("pfs_example_component_population_deinit:\n");

  /**
   * Call delete_tables function of pfs_plugin_table service to
   * delete component tables from performance schema
   */
  if (mysql_service_pfs_plugin_table->delete_tables(&share_list[0],
                                                    share_list_count))
  {
    WRITE_LOG("Error returned from delete_table()\n");
    result = 1;
    goto error;
  }
  else
  {
    WRITE_LOG ("Passed delete_tables()\n");
  }

error:
  if (!result)
  {
    /* Destroy mutexes */
    native_mutex_destroy(&LOCK_continent_records_array);
    native_mutex_destroy(&LOCK_country_records_array);
  }

  WRITE_LOG ("End of deinit\n\n");
  fclose(outfile);
  return result;
}

/* pfs_example_component_population doesn't provide any service */
BEGIN_COMPONENT_PROVIDES(pfs_example_component_population)
END_COMPONENT_PROVIDES()

/* pfs_example_component requires/uses pfs_plugin_table service */
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_table);

BEGIN_COMPONENT_REQUIRES(pfs_example_component_population)
  REQUIRES_SERVICE(pfs_plugin_table)
END_COMPONENT_REQUIRES()

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(pfs_example_component_population)
  METADATA("mysql.author", "Oracle Corporation")
  METADATA("mysql.license", "GPL")
  METADATA("test_property", "1")
END_COMPONENT_METADATA()

/* Declaration of the Component. */
DECLARE_COMPONENT(pfs_example_component_population, "mysql:pfs_example_component_population")
  pfs_example_component_population_init,
  pfs_example_component_population_deinit
END_DECLARE_COMPONENT()

/* Defines list of Components contained in this library. Note that for now we
  assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(pfs_example_component_population)
END_DECLARE_LIBRARY_COMPONENTS
