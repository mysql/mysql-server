/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef DD__DD_VERSION_INCLUDED
#define DD__DD_VERSION_INCLUDED

#include "mysql_version.h"  // MYSQL_VERSION_ID

/**
  @file sql/dd/dd_version.h
  Data dictionary version.
*/

/**
  The version of the current data dictionary table definitions.

  This version number is stored on disk in the data dictionary. Every time
  the data dictionary schema structure changes, this version number must
  change. The table that stores the data dictionary version number is never
  allowed to change.

  The data dictionary version number is the MySQL server version number
  of the first MySQL server version that published a given database schema.
  The format is Mmmdd with M=Major, m=minor, d=dot, so that MySQL 8.0.4 is
  encoded as 80004. This is the same version numbering scheme as the
  information schema and performance schema are using.

  When a data dictionary version is made public, the next change to a
  dictionary table will be associated with the next available MySQL server
  version number. So if DD version 80004 is made available in MySQL 8.0.4,
  and 8.0.5 is an MRU with no changes to the DD tables, then the DD version
  will stay 80004 also in MySQL 8.0.5. If MySQL 9.0.4 is the first GA of
  9.0, and if there are no changes to the DD tables compared to 8.0.4, then
  the DD version number will stay 80004 also in MySQL 9.0.4. Then, if there
  are changes to the DD tables after MySQL 9.0.4, then the new DD version will
  be 90005. In day to day builds internally, changes to the DD tables may be
  done incrementally, so there may be different builds having the same DD
  version number, yet with different DD table definitions.

  Historical version number published in the data dictionary:


  1: Published in 8.0.3-RC.
  ----------------------------------------------------------------------------
  Introduced in MySQL 8.0.0 by WL#6378. Never published in a GA version.
  Last changes were:

  - WL#6049: Removed foreign_keys.unique_constraint_id and the corresponding
    FK constraint, added foreign_keys.unique_constraint_name.

  - Bug#2620373: Added index_stats.cached_time and table_stats.cached_time.


  80004: Published in 8.0.4-RC.
  ----------------------------------------------------------------------------
  Changes from version 1:

  - WL#9059: Added collation clause for spatial_reference_systems.organization

  - WL#9553: Added new 'options' column to the following DD tables:
    catalogs, character_sets, collations, column_statistics, events,
    foreign_keys, resource_groups, routines, schemata,
    st_spatial_reference_systems, triggers.

    (Other relevant DD tables have this column already: columns,
     indexes, parameters, tables, tablespaces).

    Also added explicit indexes for foreign keys instead of relying
    on these to be created implicitly for the following tables/columns:
    character_sets.default_collation_id,  collations.character_set_id,
    columns.collation_id, columns.srs_id, events.client_collation_id,
    events.connection_collation_id, events.schema_collation_id,
    foreign_key_column_usage.column_id, index_column_usage.column_id,
    index_partitions.index_id, index_partitions.tablespace_id,
    indexes.tablespace_id, parameters.collation_id,
    routines.result_collation_id, routines.client_collation_id,
    routines.connection_collation_id, routines.schema_collation_id,
    schemata.default.collation_id, table_partitions.tablespace_id,
    tables.collation_id, tables.tablespace_id, triggers.client_collation_id,
    triggers.connection_collation_id, triggers.schema_collation_id,


  80011: Published in 8.0 GA.
  ----------------------------------------------------------------------------
  Changes from version 80004:

  - WL#8383 and WL#9465: Removed obsolete SQL modes from enums in 'events',
    'routines' and 'triggers'.

  - WL#10774 removed NO_AUTO_CREATE_USER as a valid sql mode value.
    As a result events, routines and triggers table are updated.

  - Bug#27499518 changed the data type for the column 'hidden' in table
    'columns' from BOOL to ENUM('Visible', 'SE', 'SQL').

  - Bug#11754608 "MYSQL DOESN'T SHOW WHAT COLLATION WAS USED IF THAT
    COLLATION IS THE DEFAU"
    Added a new column 'is_explicit_collation' to the 'columns' DD table.

  - BUG#27309116: Add a new column `external_language` to `mysql`.`routines`
    and update `information_schema`.`routines` to reflect this column.

  - Bug#27690593: CHANGE TYPE OF MYSQL.DD_PROPERTIES.PROPERTIES.
    Changed type of 'dd_properties.properties' from MEDIUMTEXT to
    MEDIUMBLOB.


  80012: Published in 8.0.12
  ----------------------------------------------------------------------------
  Changes from version 80011:

  - Bug#27745526: Various adjustments to make the DD table definitions
    in sync with WL#6379.


  80013: Published in 8.0.13
  ----------------------------------------------------------------------------
  Changes from version 80012:

  - Bug#24741307: add last_checked_for_upgrade column to msyql.tables table


  80014: Published in 8.0.14
  ----------------------------------------------------------------------------
  Changes from version 80013:

  - Bug#28492272: Synchronize sql_mode in server with that in DD.


  80015: Not published. DD version still at 80014 in server 8.0.15.
  ----------------------------------------------------------------------------
  No changes from version 80014.


  80016: Published in 8.0.16
  ----------------------------------------------------------------------------
  Changes from version 80014:

  - WL#929 - CHECK CONSTRAINTS
      New DD table check_constraints is introduced for the check
      constraints metadata.

  - WL#12261 adds new mysql.schemata.default_encryption DD column.

  - Bug#29053560 Increases DD column mysql.tablespaces.name length to 268.

  80017: Published in 8.0.17
  ----------------------------------------------------------------------------
  Changes from version 80016:

  - WL#12731 adds new mysql.schemata.se_private_data DD column.
  - WL#12571 Support fully qualified hostnames longer than 60 characters
    Server metadata table columns size is increased to 255.

  80021: Published in 8.0.21
  ----------------------------------------------------------------------------
  Changes from version 80017:

  - WL#13341 adds new columns
      mysql.tables.engine_attribute
      mysql.tables.secondary_engine_attribute
      mysql.columns.engine_attribute
      mysql.columns.secondary_engine_attribute
      mysql.indexes.engine_attribute
      mysql.indexes.secondary_engine_attribute
      mysql.tablespaces.engine_attribute

  80022: Published in 8.0.22
  ----------------------------------------------------------------------------
  Changes from version 80021:

  - Bug#31587625: PERFORMANCE DEGRADATION AFTER WL14073: Adds definer index for
    mysql.{events, routines, tables, triggers}.

  80023: Current.
  ----------------------------------------------------------------------------
  Changes from version 80022:

  - WL#10905 adds new hidden type 'USER' to mysql.columns.hidden column.
  - Bug#31867653 changes the type of mysql.table_partition_values.list_num from
    TINYINT to SMALLINT.

  80024: Next DD version number after the previous is public.
  ----------------------------------------------------------------------------
  Changes from version 80023:
  - No changes, this version number is not active yet.

  80200:
  ----------------------------------------------------------------------------
  Changes:
  - WL#14190: Replace old terms in replication SQL commands on the SOURCE
    > Changes the enum of mysql.events.status to use the correct terminology

  80300:
  ----------------------------------------------------------------------------
  Changes:
  - WL#15786: Automatically updated histograms
    > Adds a boolean "auto-update" property to the histogram JSON object in
      the mysql.column_statistics table.

  90000:
  ----------------------------------------------------------------------------
  Changes:
  - WL#16081: Native Vector Embeddings Support In MySQL And HeatWave
 */
namespace dd {

static const uint DD_VERSION = 90000;
static_assert(DD_VERSION <= MYSQL_VERSION_ID,
              "This release can not use a version number from the future");

/**
  If a new DD version is published in a MRU, that version may or may not
  be possible to downgrade to previous MRUs within the same GA. From a
  technical perspective, we may support downgrade for some types of
  changes to the DD tables, such as:

  i)   Addition of new attributes to a predefined general purpose option-like
       field.
  ii)  Addition of a column at the end of the table definition.
  iii) Addition of elements at the end of an enumeration column type.
  iv)  Extension of a VARCHAR field.
  v)   Addition of an index on a column.

  This means we can support downgrade in terms of being able to open the DD
  tables and read from them. However, additional considerations are relevant
  in order to determine whether downgrade should be supported or not, e.g.:

  - For changes like i) and iii): In the older version, will invalid entries
    just be ignored, or will they lead to a failure?
  - For changes like iv): In the older version, are there buffer sizes that
    may be insufficient?

  If downgrade is supported, the constant DD_VERSION_MINOR_DOWNGRADE_THRESHOLD
  should be set to the lowest DD_VERSION that we may downgrade to. If downgrade
  is not supported at all, then DD_VERSION_MINOR_DOWNGRADE_THRESHOLD should be
  set to DD_VERSION.

  It has been decided that the default policy for MySQL 8.0 is not to allow
  downgrade for any minor release. One of the major reasons for this is that
  this would lead to a huge amount of possible upgrade/downgrade paths with
  correspondingly complicated and effort demanding QA. Thus, we set this
  constant to be equal to DD_VERSION to prohibit downgrade attempts. This
  decision may be relaxed for future releases.
*/
static const uint DD_VERSION_MINOR_DOWNGRADE_THRESHOLD = DD_VERSION;
static_assert(DD_VERSION_MINOR_DOWNGRADE_THRESHOLD <= MYSQL_VERSION_ID,
              "This release can not use a version number from the future");

/**
  A new release model supporting Long Term Stability (LTS) and innovation
  releases is being introduced. The LTS releases will be kept as stable as
  possible, patch updates will be provided mostly for critical issues.
  Innovation releases will be released regularly with new features.

  When we start a new major version, e.g. 9.0.0, this is an innovation release.
  Each consecutive release is a new innovation release where the minor version
  number is incremented. The last minor version for a given major version is an
  LTS release, this will normally have minor version 7, e.g. 9.7.0. The LTS
  release will have regular CPU releases (critical patch update) where the major
  and minor versions stay unchanged, and only the patch number is incremented
  (e.g. 9.7.1, 9.7.2 etc.). In parallel with the CPU releases, a new major
  version will also be released, starting with an incremented major version
  number (e.g. 10.0.0). In some cases, the innovation releases may also have
  CPU releases, e.g. 9.1.0, 9.1.1 etc.

  With the release model supporting LTS releases, we will have to support
  downgrade between patch releases. Normally, there will be no changes in
  features in a patch release, and the disk image should have a similar format,
  both in terms of record layout, data dictionary structure, system table
  definitions, information schema, performance schema, etc. However, there might
  be situations where changes that are not backwards compatible need to be made
  within a patch release. For some server artifacts, we already have mechanisms
  in place to allow older versions to reject a downgrade attempt (e.g. if the
  data dictionary is changed, the older version will reject the downgrade
  attempt). For other artifacts, there is no such mechanism. Thus, we introduce
  the SERVER_DOWNGRADE_THRESHOLD which makes it possible to define how far back
  the current version should be able to downgrade. On a downgrade attempt,
  the target version will look at the threshold which has been stored
  persistently by the actual server that we downgrade from. If the target server
  version is lower than the threshold, it will reject the downgrade attempt.

  The threshold defaults to 0. This means that downgrade back to the first patch
  for the given version is possible. E.g. if LTS version 9.7.2 has the threshold
  defined to 0, downgrades to 9.7.1 and 9.7.0 is possible. Then, if LTS version
  9.7.3 is released with the threshold set to 9.7.2, then only downgrade to
  9.7.2 is possible. Downgrades to or from innovation releases are never
  supported, regardless of the downgrade threshold.
*/
constexpr uint SERVER_DOWNGRADE_THRESHOLD = 0;
static_assert(SERVER_DOWNGRADE_THRESHOLD <= MYSQL_VERSION_ID,
              "This release can not use a version number from the future");

/**
  The new release model explained above will also open the possibility of
  upgrading to a version that has been released in the past. I.e., we will
  upgrade from an actual version to a target version with a higher version
  number, but an earlier (older) GA release date.

  Like for the patch downgrades mentioned above, we already have mechanisms in
  place to allow older versions to reject an upgrade attempt (e.g. if the data
  dictionary is changed, the older version will reject the upgrade attempt).
  For other artifacts, there is no such mechanism. Thus, we introduce the
  SERVER_UPGRADE_THRESHOLD which makes it possible to define how far back
  the current version should be able to upgrade. On an upgrade attempt, the
  target version will look at the threshold which has been stored persistently
  by the actual server that we upgrade from. If the target server version is
  lower than the threshold, it will reject the upgrade attempt.

  The threshold defaults to 0. This means that for upgrades to any higher
  version is possible (unless prohibited by other rules). E.g. if LTS version
  9.7.2 has the threshold defined to 0, upgrades to 10.0.0, 10.1.0 etc. is
  possible. Then, if LTS version 9.7.3 is released with the upgrade threshold
  set to 10.2.0, then upograde from 9.7.3 is possible only to 10.2.0 or higher.
*/
constexpr uint SERVER_UPGRADE_THRESHOLD = 0;

}  // namespace dd

#endif /* DD__DD_VERSION_INCLUDED */
