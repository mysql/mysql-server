# Copyright (c) 2017, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET (DEB_CMAKE_EXTRAS "${DEB_CMAKE_EXTRAS} -DMYSQL_SERVER_SUFFIX=\"-commercial\"")
SET (DEB_CONTROL_BDEPS_COMMERCIAL ", libcurl4-openssl-dev, libkrb5-dev, libsasl2-modules-gssapi-mit")

SET (DEB_RULES_PROFILE_GENERATE
"
	if [ -n \"$(PROFILE)\" ]; then \\
          mkdir release && cd release && \\
          cmake .. \\
                -DFPROFILE_GENERATE=1 \\
                -DBUILD_CONFIG=mysql_release \\
                -DCMAKE_INSTALL_PREFIX=/usr \\
                -DINSTALL_LIBDIR=lib/$(DEB_HOST_MULTIARCH) \\
                -DSYSCONFDIR=/etc/mysql \\
                -DMYSQL_UNIX_ADDR=/var/run/mysqld/mysqld.sock \\
                -DWITH_MECAB=system \\
                -DWITH_ZLIB=${DEB_ZLIB_OPTION} \\
                -DWITH_NUMA=ON \\
                -DCOMPILATION_COMMENT=\"MySQL ${DEB_PRODUCTNAMEC} - ${DEB_LICENSENAME}\" \\
                -DCOMPILATION_COMMENT_SERVER=\"MySQL ${DEB_PRODUCTNAMEC} Server - ${DEB_LICENSENAME}\" \\
                -DINSTALL_LAYOUT=DEB \\
                -DREPRODUCIBLE_BUILD=OFF \\
                -DDEB_PRODUCT=${DEB_PRODUCT} \\
                ${DEB_CMAKE_EXTRAS} && \\
          make $(JOBS) VERBOSE=1 && \\
          make run-profile-suite && \\
          cd .. && rm -rf release ; \\
        fi
")
