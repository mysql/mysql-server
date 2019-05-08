# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.

# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Handle upgrade of distributed privilege tables, which are supported
# in MySQL Cluster 7.x but not in MySQL Cluster 8.0.

# Four tables have a Timestamp column, which we update where its value
# is zero in order to avoid an error when copying the data.
UPDATE mysql.tables_priv set Timestamp = now() where Timestamp = 0;
UPDATE mysql.columns_priv set Timestamp = now() where Timestamp = 0;
UPDATE mysql.procs_priv set Timestamp = now() where Timestamp = 0;
UPDATE mysql.proxies_priv set Timestamp = now() where Timestamp = 0;


# These six table names are hard-coded inside the mysql server to trigger
# special behavior so that the tables are not deleted globally from NDB, but
# remain available to other mysql servers which may have not yet upgraded.

ALTER TABLE mysql.user engine=innodb;
ALTER TABLE mysql.db engine=innodb;
ALTER TABLE mysql.tables_priv engine=innodb;
ALTER TABLE mysql.columns_priv engine=innodb;
ALTER TABLE mysql.procs_priv engine=innodb;
ALTER TABLE mysql.proxies_priv engine=innodb;
