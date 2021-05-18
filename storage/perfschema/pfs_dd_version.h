/* Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.

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


  Version published is now 80020. The next number to use is 80021.

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
*/

static const uint PFS_DD_VERSION = 80022;

#endif /* PFS_DD_VERSION_H */
