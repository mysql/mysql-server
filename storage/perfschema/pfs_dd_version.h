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

#ifndef PFS_DD_VERSION_H
#define PFS_DD_VERSION_H

/**
  @file storage/perfschema/pfs_dd_version.h
  Performance schema data dictionary version.
*/

/**
  The version of the current performance_schema database schema.
  Version numbering forms a name space,
  which needs to be unique across all MySQL versions,
  even including forks.

  This version number is stored on disk in the data dictionary.
  Every time the performance_schema structure changes,
  this version number must change.

  Do not use a naive 1, 2, 3, ... N numbering scheme,
  as it requires an authoritative registry to assign numbers.
  This can not work in a distributed development environment,
  even less with forks, patches and back ports done by third parties.

  The numbering to use is the MySQL version number
  of the first MySQL version that published a given database schema.
  The format is Mmmdd with M=Major, m=minor, d=dot,
  so that MySQL 8.0.4 is encoded as 80004.

  In case of -dash version numbers, encode MySQL 8.12.34-56 as 8123456.

  Historical version number published in the data dictionary:

  1:

  Introduced in MySQL 8.0.3 by WL#7900.
  Never published in a GA version, abandoned.

  80004:

  performance_schema tables changed in MySQL 8.0.4 are
  - setup_threads (created)
  - setup_instruments (modified)
  - variables_info (modified)
  - setup_timers (removed)
  - metadata_locks (modified, added column COLUMN_NAME)
  - replication_connection_configuration (modified)
  - instance_log_resource (created)

  80005:

  performance_schema tables changed in MySQL 8.0.5 are
  - all, changed UTF8 (aka UTF8MB3) to UTF8MB4.

  80006:

  performance_schema tables changed in MySQL 8.0.6 are
  - variables_info.set_time precision changed from 0 to 6.

  80011:

  Version bump from 8.0.6 to 8.0.11,
  versions [8.0.5 - 8.0.10] inclusive are abandoned.
  performance_schema tables changed in MySQL 8.0.11 are
  - instance_log_resource was renamed to log_resource.

  80014:

  performance_schema tables changed in MySQL 8.0.14 are
  - events_statements_current, added column QUERY_ID
  - events_statements_history, added column QUERY_ID
  - events_statements_history_long, added column QUERY_ID

  80015:

  performance_schema.keyring_keys

  80017: -- WARNING, EXPOSED BY MySQL 8.0.16 --

  Unfortunately, MySQL 8.0.16 claim the 80017 tag, due to a bad merge.
  performance_schema tables changed in MySQL 8.0.16
  - replication_connection_configuration, added column NETWORK_NAMESPACE

  800171: -- WARNING, EXPOSED BY MySQL 8.0.17 --
    ---------------------------------------------------------------------
    IMPORTANT NOTE:
    The release 8.0.16 made use of version 80017 incorrectly, which makes
    the upgrade from 8.0.16 to 8.0.17 release immutable. This was
    introduced by WL#12720.

    In order to allow upgrade from 8.0.16 to 8.0.17, we choose the new
    version number as 800171 for 8.0.17 release. Going forward the
    release 8.0.18 would use 80018.

    Note that, any comparison between two PFS version numbers should be
    carefully done.
    ---------------------------------------------------------------------

  performance_schema tables changed in MySQL 8.0.17
  - WL#12571 increases the HOST name length from 60 to 255.

  80018:

  performance_schema tables changed in MySQL 8.0.18
  - replication_connection_configuration, added column
  MASTER_COMPRESSION_ALGORITHMS, MASTER_COMPRESSION_LEVEL
  - replication_applier_configuration, added column
  PRIVILEGE_CHECKS_USER

  80019:

  performance_schema tables changed in MySQL 8.0.19
  - replication_connection_configuration, added column
  TLS_CIPHERSUITES
  - replication_applier_configuration, added column
  REQUIRE_ROW_FORMAT

  80020:

  performance_schema tables changed in MySQL 8.0.20
  - WL#3549 created binary_log_transaction_compression_stats
  - replication_applier_configuration, added column
  REQUIRE_TABLE_PRIMARY_KEY_CHECK

  80021:

  performance_schema tables changed in MySQL 8.0.21
  - tls_channel_status (created)
  - replication_connection_configuration, added column
  SOURCE_CONNECTION_AUTO_FAILOVER

  80022:

  performance_schema tables changed in MySQL 8.0.22
  - WL#9090 created processlist
  - WL#13681 created error_log

  80023:

  performance_schema tables changed in MySQL 8.0.23
  - WL#12819 replication_applier_configuration, added column
  ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_TYPE
  ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS_VALUE
  - performance_schema.replication_asynchronous_connection_failover,
  added column MANAGED_NAME
  - added table
  performance_schema.replication_asynchronous_connection_failover_managed

  80024:

  performance_schema tables changed in MySQL 8.0.24
  - WL#13446 added performance_schema.keyring_component_status

  80027:

  performance_schema tables changed in MySQL 8.0.27
  - WL#9852 added replication_group_members column
    MEMBER_COMMUNICATION_PROTOCOL_STACK
  - Bug#32701593:'SELECT FROM
    PS.REPLICATION_ASYNCHRONOUS_CONNECTION_FAILOVER' DIDN'T RETURN A RE
  - WL#7491 added the column to replication_connection_configuration
    the column GTID_ONLY
  - BUG#104643 Defaults in performance schema tables incompatible with sql_mode
    fixed TIMESTAMP columns (removed default 0)
    fixed DOUBLE columns (removed sign)

  80028:

  performance_schema tables changed in MySQL 8.0.28
  - WL#14779 PERFORMANCE_SCHEMA, ADD CPU TIME TO STATEMENT METRICS
    added CPU_TIME, SUM_CPU_TIME columns.
  - Fixed performance_schema.processlist host column to size 261.

  80029:

  performance_schema tables changed in MySQL 8.0.29
  - WL#14346 PERFORMANCE_SCHEMA, SECONDARY_ENGINE STATS
    Added columns EXECUTION_ENGINE, COUNT_SECONDARY
  - Bug #30624990 NO UTF8MB3 IN INFORMATION_SCHEMA.CHARACTER_SETS
    Use 'utf8mb3' rather than 'utf8' alias for for character set names.

  80030:

  performance_schema tables changed in MySQL 8.0.30
  - WL#12527 added innodb_redo_log_files table (table is created dynamically
    and based on PFS_engine_table_share_proxy mechanism)
  - Bug #33787300 Rename utf8_xxx collations to utf8mb3_xxx
    This patch renames utf8_bin to utf8mb3_bin
    This patch renames utf8_general_ci

  80031:

  - WL#14432 Session memory limits in performance schema
    Modified column PROPERTIES in table setup_instruments
    Added column FLAGS in table setup_instruments

  80032:

  - WL#15419: Make the replica_generate_invisible_primary_key option settable
    per channel
    Modified column REQUIRE_TABLE_PRIMARY_KEY_CHECK in table
    replication_applier_configuration

  80033:

   - WL#15059: PERFORMANCE_SCHEMA, OTEL TRACE INTERFACE
     New column TELEMETRY_ACTIVE added to performance_schema.threads

  80040:

   - Bug#31763497 PERFORMANCE DEGRADATION CAUSED BY MONITORING
     SYS.INNODB_LOCK_WAITS IN MYSQL 8.0
     - Table performance_schema.data_lock_waits, add PRIMARY KEY.

  80200:

   - WL#15199: PERFORMANCE_SCHEMA, OTEL METRICS INTERFACE
      New tables setup_meters, setup_metrics added to performance_schema

  80300:

   - WL#15294: Extending GTID with tags to identify group of transactions
      Modified performance schema tables:
      - replication_connection_status:
        - LAST_QUEUED_TRANSACTION 57->90 bytes
        - QUEUEING_TRANSACTION 57->90 bytes
      - replication_applier_status_by_coordinator:
        - LAST_PROCESSED_TRANSACTION 57->90 bytes
        - PROCESSING_TRANSACTION  57->90 bytes
      - replication_applier_status_by_worker
        - APPLYING_TRANSACTION 57->90 bytes
        - LAST_APPLIED_TRANSACTION 57->90 bytes
      - events_transactions_current:
        - GTID 64->90 bytes
      - events_transactions_history:
        - GTID 64->90 bytes
      - events_transactions_history_long:
        - GTID 64->90 bytes

  80403:

   - Bug#31763497 PERFORMANCE DEGRADATION CAUSED BY MONITORING
     SYS.INNODB_LOCK_WAITS IN MYSQL 8.0
     - Table performance_schema.data_lock_waits, add PRIMARY KEY.

  90000:

   - WL#15855: System variable metadata
      New tables variable_metadata, global_variable_attributes added to
  performance_schema

  90100:

   - Bug#31763497 PERFORMANCE DEGRADATION CAUSED BY MONITORING
     SYS.INNODB_LOCK_WAITS IN MYSQL 8.0
     - Table performance_schema.data_lock_waits, add PRIMARY KEY.
*/

static const uint PFS_DD_VERSION = 90100;

#endif /* PFS_DD_VERSION_H */
