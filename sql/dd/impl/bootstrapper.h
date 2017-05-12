/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__BOOTSTRAPPER_INCLUDED
#define DD__BOOTSTRAPPER_INCLUDED


#include "dd/string_type.h"                    // dd::String_type

class THD;

/**
  Create a lex string for the query from the string supplied
  and execute the query.

  @param thd     Thread handle.
  @param q_buf   String containing the query text.

  @retval false  Success.
  @retval true   Error.
*/
bool execute_query(THD *thd, const dd::String_type &q_buf);


/**
  Data dictionary initialization.

  The data dictionary is initialized whenever the mysqld process starts.
  We distinguish between the first time start and the subsequent normal
  restarts, as explained below. However, there are three main design
  principles that should be elaborated first.

  1. Two-step process: The dictionary initialization is implemented as
     a two step process. First, scaffolding is built to prepare the
     synchronization with persistent storage, then, the actual synchronization
     is done. The way this is done depends on the context, and is different
     for first time start and the subsequent restarts.

  2. Use SQL: The initialization uses SQL to build the scaffolding. This
     means that we execute SQL statements to create the dictionary tables.
     Since this is done at a stage where the physical tables either do not
     exist yet, or are not known, we must instrument the DDL execution to
     create the physical counterpart of the tables only on first time start.
     The goal is to keep the instrumentation at a minimum.

  3. Fake caching: As a consequence of keeping instrumentation at a minimum,
     we provide uniform behavior of the caching layer in the data dictionary
     also in the scaffolding phase. This means that as seen from the outside,
     dictionary objects can be retrieved from the cache. Internally, below the
     caching layer, the objects are only kept in a separate buffer until all
     the required scaffolding is built. At that point, we can start using the
     underlying physical tables, depending on the circumstances:

     - For first time start (initialization), we can flush the meta data
       generated in the scaffolding phase, to the DD tables.
     - For ordinary restart, we can use the scaffolding to open the physical
       tables, and then sync up the real meta data that is stored persistently.

     After the scaffolding has been flushed or synced, what should be left is
     a collection of the core DD meta data objects. This collection is located
     in the storage adapter, and allows the DD cache to evict core DD objects
     in the same way as other DD objects.

  Please note that dictionary initialization is only a small part of server
  initialization. There is a lot going on before and after dictionary
  initialization while starting the server.

  Please see more elaborated descriptions for the initialize() and restart()
  methods below.
*/

namespace dd {
namespace bootstrap {

// Enumeration of bootstrapping stages.
enum enum_bootstrap_stage
{
  BOOTSTRAP_NOT_STARTED,  // Not started.
  BOOTSTRAP_STARTED,      // Started, nothing prepared yet.
  BOOTSTRAP_PREPARED,     // Ready to start creating tables.
  BOOTSTRAP_CREATED,      // Tables created, able to store persistently.
  BOOTSTRAP_SYNCED,       // Cached meta data synced with persistent storage.
  BOOTSTRAP_POPULATED,    // (Re)populated tables with meta data.
  BOOTSTRAP_FINISHED      // Completed.
};


/**
  Get the current stage of bootstrapping.

  @return Enumerated value indicating the current bootstrapping stage.
*/

enum_bootstrap_stage stage();


/**
  Initialize the dictionary while starting the server for the first time.

  At this point, the DDSE has been initialized as a normal plugin. The
  dictionary initialization proceeds as follows:

  1. Preparation phase

  1.1 Call dict_init() to initialize the DDSE. This will make the predefined
      tablespaces be created physically, and their meta data be returned to
      the SQL layer along with the meta data for the DD tables required by
      the DDSE. The tables are not yet created physically.
  1.2 Prepare the dd::Tablespace objects reflecting the predefined tablespace
      objects and add them to the core registry in the storage adapter.

  2. Scaffolding phase

  2.1 Create and use the dictionary schema by executing SQL statements.
      The schema is created physically since this is the first time start,
      and the meta data is generated and stored in the core registry of
      the storage adapter without being written to disk.
  2.2 Create tables by executing SQL statements. Like for the schema, the
      tables are created physically, and the meta data is generated
      and stored in the core registry without being written to disk.
      This is done to prepare enough meta data to actually be able to
      open the DD tables.

  3. Synchronization phase

  3.1 Store meta data for the DD schema, tablespace and tables, i.e., the DD
      objects that were generated in the scaffolding phase, and make sure the
      IDs are maintained when the objects are stored.
  3.2 Populate the DD tables which have some predefined static contents to
      be inserted. This is, e.g., relevant for the 'catalogs' table, which
      only has a single default entry in it. Dynamic contents is added in
      other ways, e.g. by storing generated DD objects (see above) or by
      inserting data from other sources (see re-population of character sets
      in the context of server restart below).
  3.3 Add cyclic foreign keys that cannot be defined while creating the tables
      in the scaffolding phase. For e.g. the tables representing character
      sets and collations, there is a cyclic foreign key relationship.
      Non-cyclic foreign keys are defined as part of the create table
      statements, but the cyclic keys must be added by ALTER TABLE statements
      afterwards.
  3.4 Verify that the dictionary objects representing the core DD table meta
      data are present in the core registry of the storage adapter. If an
      object representing the meta data of a core DD table is not available,
      then we loose access to the DD tables, and we will not be able to handle
      cache misses or updates to the meta data.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/

bool initialize(THD *thd);


/**
  Initialize the dictionary while restarting the server.

  At this point, the DDSE has been initialized as a normal plugin. The
  dictionary initialization proceeds as follows:

  1. Preparation phase

  1.1 Call dict_init() to initialize the DDSE. This will retrieve the meta data
      of the predefined tablespaces and the DD tables required by the DDSE.
      Both the tables and the tablespaces are already created physically, the
      point here is just to get hold of enough meta data to start using the DD.
  1.2 Prepare the dd::Tablespace objects reflecting the predefined tablespace
      objects and add them to the core registry in the storage adapter.

  2. Scaffolding phase

  2.1 Create and use the dictionary schema by executing SQL statements.
      The schema is not created physically, but the meta data is generated
      and stored in the core registry without being written to disk.
  2.2 Create tables by executing SQL statements. Like for the schema, the
      tables are not created physically, but the meta data is generated
      and stored in the core registry without being written to disk.
      This is done to prepare enough meta data to actually be able to
      open the DD tables.

  3. Synchronization phase

  3.1 Read meta data for the DD tables from the DD tables. Here, we use the
      meta data from the scaffolding phase for the schema, tablespace and the
      DD tables to open the physical DD tables. We read the stored objects,
      and update the in-memory copies in the core registry with the real meta
      data from the objects that are retrieved form persistent storage. Finally,
      we flush the tables to empty the table definition cache to make sure the
      table share structures for the DD tables are re-created based on the
      actual meta data that was read from disk rather than the temporary meta
      data from the scaffolding phase.
  3.2 Re-populate character sets and collations: The character set and
      collation information is read from files and added to a server
      internal data structure when the server starts. This data structure is,
      in turn, used to populate the corresponding DD tables. The tables must
      be re-populated on each server start if new character sets or collations
      have been added. However, we can not do this if in read only mode.
  3.3 Verify that the dictionary objects representing the core DD table meta
      data are present in the core registry of the storage adapter. If an
      object representing the meta data of a core DD table is not available,
      then we loose access to the DD tables, and we will not be able to handle
      cache misses or updates to the meta data.

  @param thd            Thread context.

  @return       Upon failure, return true, otherwise false.
*/

bool restart(THD *thd);


/**
  Iterate through all the plugins, and store IS table meta data
  into dictionary, once during MySQL server bootstrap.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool store_plugin_IS_table_metadata(THD *thd);


/**
  Check if the server is starting on a data directory without dictionary
  tables or not.

  If the dictionary tables are present, continue with restart of the server.

  If the dicionary tables are not present, create the dictionary tables in
  existing data directory.  This function marks dd_upgrade_flag as true to
  indicate to the server that Data dictionary is being upgraded.

  metadata mysql.plugin table is migrated to the DD tables in case of upgrade.
  mysql.plugin table is used to initialize all other Storage Engines.
  This is necessary before migrating other user tables.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool upgrade_do_pre_checks_and_initialize_dd(THD *thd);

/**
  Finalize upgrade to the new data-dictionary by populating
  Data Dictionary tables with metadata.

  Populate metadata in Data dictionary tables.
  This will be done for following database objects:
  - Databases
  - Tables
  - Views
  - Stored Procedures and Stored Functions
  - Events
  - Triggers

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool upgrade_fill_dd_and_finalize(THD *thd);

/**
  Drop all DD tables in case there is an error while upgrading server.

  @param[in] thd               Thread context.

  @return       Upon failure, return true, otherwise false.
*/
bool delete_dictionary_and_cleanup(THD *thd);
}
}

#endif // DD__BOOTSTRAPPER_INCLUDED
