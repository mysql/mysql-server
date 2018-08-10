# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

SET(DEB_CONTROL_ROUTER
"
Package: mysql-router-${DEB_PRODUCTNAME}
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Breaks: mysql-router-${DEB_PRODUCTNAME}
Replaces: mysql-router-${DEB_PRODUCTNAME}
Conflicts: mysql-router-${DEB_NOTPRODUCTNAME}, mysql-router (<< 8.0.3)
Description: MySQL Router
 The MySQL(TM) Router software delivers a fast, multi-threaded way of
 routing connections from MySQL Clients to MySQL Servers. MySQL is a
 trademark of Oracle.


Package: mysql-router
Architecture: any
Depends: mysql-router-${DEB_PRODUCTNAME}
Description: MySQL Router Metapackage
 The MySQL(TM) Router software delivers a fast, multi-threaded way of
 routing connections from MySQL Clients to MySQL Servers. MySQL is a
 trademark of Oracle. This is the shared router metapackage, used for
 dependency handling.

Package: mysql-router-${DEB_PRODUCTNAME}-dev
Architecture: any
Depends: \${misc:Depends}, \${shlibs:Depends}
Conflicts: mysql-router-${DEB_NOTPRODUCTNAME}-dev
Description: MySQL Router development files
 The MySQL(TM) Router software delivers a fast, multi-threaded way of
 routing connections from MySQL Clients to MySQL Servers. MySQL is a
 trademark of Oracle.

")
