Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

----
Instructions on how to compile with wolfssl

* Get wolfssl-3.14.0.zip from wolfssl.com
* Extract it to extra/wolfssl-3.14.0
* Download wolfssl-3.14.0-mysql.diff to extra/wolfssl-3.14.0 folder from:
  https://github.com/gkodinov/wolfssl-diff-for-mysql-8.0
* Apply wolfssl-3.14.0-mysql.diff:
  cd extra/wolfssl-3.14.0
  patch -p1 < wolfssl-3.14.0-mysql.diff
* cd extra/wolfssl-3.14.0/IDE/MYSQL
* Execute do.sh
* use -DWITH_SSL=wolfssl for CMake
