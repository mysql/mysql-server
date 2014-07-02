#!/bin/bash

# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#set -x

user="$(whoami)"
echo
echo "grant privileges to users: '', $user"
# MySQL doesn't support wildcards in user names but anonymous users as '';
# any user who connects from the local host with the correct password for
# the anonymous user will be allowed access then.
./mysql.sh -v -u root -e "GRANT ALL ON *.* TO ''@localhost, $user@localhost;"
s=$?
echo "mysql exit status: $s"

echo
echo done.
exit $s

#set +x
