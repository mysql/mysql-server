# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 


SET(DEB_NDB_CONTROL_EXTRAS
"
Package: mysql-${DEB_PRODUCTNAME}-management-server
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Management server
 This package contains the MySQL Cluster Management Server Daemon,
 which reads the cluster configuration file and distributes this
 information to all nodes in the cluster

Package: mysql-${DEB_PRODUCTNAME}-data-node
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends},
 libclass-methodmaker-perl
Description: Data node
 This package contains MySQL Cluster Data Node Daemon, it's the process
 that is used to handle all the data in tables using the NDB Cluster
 storage engine. It comes in two variants: ndbd and ndbmtd, the former
 is single threaded while the latter is multi-threaded.

Package: mysql-${DEB_PRODUCTNAME}-auto-installer
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends},
 python-paramiko
Description: Data node
 This package contains MySQL Cluster Data Node Daemon, it's the process
 that is used to handle all the data in tables using the NDB Cluster
 storage engine. It comes in two variants: ndbd and ndbmtd, the former
 is single threaded while the latter is multi-threaded.

Package: ndbclient
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Ndb client
 This package contains the shared libraries for MySQL MySQL NDB storage
 engine client applications.

Package: ndbclient-dev
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}, ndbclient
Description: ndbclient dev package
 This package contains the development header files and libraries
 necessary to develop client applications for MySQL NDB storage engine.

Package: mysql-${DEB_PRODUCTNAME}-java
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: Java connector
 This package contains MySQL Cluster Connector for Java, which includes
 ClusterJ and ClusterJPA, a plugin for use with OpenJPA.
 .
 ClusterJ is a high level database API that is similar in style and
 concept to object-relational mapping persistence frameworks such as
 Hibernate and JPA.
 .
 ClusterJPA is an OpenJPA implementation for MySQL Cluster that
 attempts to offer the best possible performance by leveraging the
 strengths of both ClusterJ and JDBC

Package: mysql-${DEB_PRODUCTNAME}-memcached
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}, mysql-${DEB_PRODUCTNAME}-server
Description: memcached
 This package contains the standard memcached server and a loadable
 storage engine for memcached using the Memcache API for MySQL Cluster
 to provide a persistent MySQL Cluster data store.
")

  SET (DEB_NDB_CLIENT_EXTRA
"
/usr/bin/ndb_blob_tool
/usr/bin/ndb_config
/usr/bin/ndb_delete_all
/usr/bin/ndb_desc
/usr/bin/ndb_drop_index
/usr/bin/ndb_drop_table
/usr/bin/ndb_error_reporter
/usr/bin/ndb_index_stat
/usr/bin/ndb_mgm
/usr/bin/ndb_move_data
/usr/bin/ndb_print_backup_file
/usr/bin/ndb_print_file
/usr/bin/ndb_print_frag_file
/usr/bin/ndb_print_schema_file
/usr/bin/ndb_print_sys_file
/usr/bin/ndb_redo_log_reader
/usr/bin/ndb_restore
/usr/bin/ndb_select_all
/usr/bin/ndb_select_count
/usr/bin/ndb_setup.py
/usr/bin/ndb_show_tables
/usr/bin/ndb_size.pl
/usr/bin/ndb_waiter
/usr/bin/ndbinfo_select_all

/usr/share/man/man1/ndb-common-options.1*
/usr/share/man/man1/ndb_blob_tool.1*
/usr/share/man/man1/ndb_config.1*
/usr/share/man/man1/ndb_cpcd.1*
/usr/share/man/man1/ndb_delete_all.1*
/usr/share/man/man1/ndb_desc.1*
/usr/share/man/man1/ndb_drop_index.1*
/usr/share/man/man1/ndb_drop_table.1*
/usr/share/man/man1/ndb_error_reporter.1*
/usr/share/man/man1/ndb_index_stat.1*
/usr/share/man/man1/ndb_mgm.1*
/usr/share/man/man1/ndb_print_backup_file.1*
/usr/share/man/man1/ndb_print_file.1*
/usr/share/man/man1/ndb_print_schema_file.1*
/usr/share/man/man1/ndb_print_sys_file.1*
/usr/share/man/man1/ndb_restore.1*
/usr/share/man/man1/ndb_select_all.1*
/usr/share/man/man1/ndb_select_count.1*
/usr/share/man/man1/ndb_setup.py.1*
/usr/share/man/man1/ndb_show_tables.1*
/usr/share/man/man1/ndb_size.pl.1*
/usr/share/man/man1/ndb_waiter.1*
/usr/share/man/man1/ndb_redo_log_reader.1*
/usr/share/man/man1/ndbinfo_select_all.1*
")


  SET (DEB_NDB_RULES_LICENSE
"
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/${DEB_INSTALL_LICENSEFILE} debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-auto-installer/${DEB_INSTALL_LICENSEFILE}
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/${DEB_INSTALL_LICENSEFILE} debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-data-node/${DEB_INSTALL_LICENSEFILE}
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/${DEB_INSTALL_LICENSEFILE} debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-java/${DEB_INSTALL_LICENSEFILE}
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/${DEB_INSTALL_LICENSEFILE} debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-management-server/${DEB_INSTALL_LICENSEFILE}
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/${DEB_INSTALL_LICENSEFILE} debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-memcached/${DEB_INSTALL_LICENSEFILE}
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/${DEB_INSTALL_LICENSEFILE} debian/tmp/usr/share/doc/ndbclient/${DEB_INSTALL_LICENSEFILE}
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/${DEB_INSTALL_LICENSEFILE} debian/tmp/usr/share/doc/ndbclient-dev/${DEB_INSTALL_LICENSEFILE}
")
  SET (DEB_NDB_RULES_README
"
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/README debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-auto-installer/README
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/README debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-data-node/README
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/README debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-java/README
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/README debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-management-server/README
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/README debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-memcached/README
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/README debian/tmp/usr/share/doc/ndbclient/README
	install -g root -o root -m 0644 debian/tmp/usr/share/mysql/README debian/tmp/usr/share/doc/ndbclient-dev/README
")
  SET (DEB_NDB_RULES_DOCDIRS
"
	install -g root -o root -m 0755 -d debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-auto-installer
	install -g root -o root -m 0755 -d debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-data-node
	install -g root -o root -m 0755 -d debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-java
	install -g root -o root -m 0755 -d debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-management-server
	install -g root -o root -m 0755 -d debian/tmp/usr/share/doc/mysql-${DEB_PRODUCTNAME}-memcached
	install -g root -o root -m 0755 -d debian/tmp/usr/share/doc/ndbclient
	install -g root -o root -m 0755 -d debian/tmp/usr/share/doc/ndbclient-dev
")
